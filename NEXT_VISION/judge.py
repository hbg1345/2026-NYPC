"""
Local judge/simulator for the NYPC 2026 finals problem NEXT VISION.

It supports both live local matches and authoritative transcript playback.
Transcript playback feeds the recorded final-protocol results (including
fog-of-war WARRIOR/BUILDING snapshots) back to one live agent, which is the
mode to use when reproducing a submitted replay and producing debug_A.txt or
debug_B.txt.

Typical usage:
    python judge.py agentA.cpp agentB.cpp [--seed N]
    python judge.py agent.cpp --transcript replay.txt --side A --debug

Each argument may be a .cpp/.cc/.c++ source (compiled automatically, and
recompiled only if the source is newer than its cached .exe) or an already
built executable. Every run writes a CSV log, a JSON replay, and a
self-contained HTML replay viewer into sim/out/ -- no extra flags needed.

This is not the official contest judge binary. The live simulator implements
the published finals rules, while random map generation is only a compatible
local approximation because the finals map-generation details were not
published in GameRules.md. Authoritative transcript playback does not
re-simulate the recorded match and therefore reproduces its observations
exactly.
"""
import argparse
import queue
import random
import subprocess
import sys
import threading
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field

try:
    from tqdm import tqdm
except ModuleNotFoundError:
    class tqdm:
        """Minimal no-op fallback for environments without the progress package."""
        def __init__(self, *args, **kwargs):
            pass

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def update(self, n=1):
            pass

        def set_postfix(self, **kwargs):
            pass

# ---------------------------------------------------------------------------
# Constants (mirrors main.c++)
# ---------------------------------------------------------------------------
MAX_TURN = 400
START_GOLD = 750
START_WARRIORS = 3
MOVE_COST = 10
TRAIN_COST = 120
WORK_INCOME = 15
UPKEEP_PER_WARRIOR = 2
HQ_MAX_LEVEL = 5
BASE_MAX_LEVEL = 3
HQ_HEAL_COST = 1000
BASE_HEAL_COST = 500

# index 0 unused
HQ_LEVELS = {
    1: dict(cost=0, whp=4, hp=10, turret=1, train_cap=1, work_cap=1),
    2: dict(cost=600, whp=5, hp=15, turret=2, train_cap=1, work_cap=2),
    3: dict(cost=1000, whp=6, hp=20, turret=2, train_cap=2, work_cap=3),
    4: dict(cost=2000, whp=7, hp=25, turret=3, train_cap=2, work_cap=4),
    5: dict(cost=3000, whp=8, hp=30, turret=3, train_cap=3, work_cap=5),
}
BASE_LEVELS = {
    1: dict(cost=500, hp=6, turret=1, work_cap=1),
    2: dict(cost=550, hp=12, turret=1, work_cap=2),
    3: dict(cost=600, hp=18, turret=2, work_cap=3),
}

OTHER = {"A": "B", "B": "A"}


# ---------------------------------------------------------------------------
# Map generation -- faithful implementation of GameRules.md section 5:
#   (2)-(3) Poisson-disk-ish symmetric point sampling in a disk of radius L
#   (4)     an auxiliary ring Q of A=24 equally-spaced points at radius 1.5L
#   (5)     Voronoi(P u Q) -> move each P point to its cell's centroid (P'),
#           then Voronoi(P' u Q) again for the final cell shapes/adjacency
#   (6)     region t = Voronoi cell of P'_t; adjacency = Delaunay/ridge edges
#           between two P'-indices (edges touching Q are discarded)
#   (7)     stronghold selection by rejection sampling
# ---------------------------------------------------------------------------
def _polygon_centroid(verts):
    import numpy as np
    x = verts[:, 0]
    y = verts[:, 1]
    x1 = np.roll(x, -1)
    y1 = np.roll(y, -1)
    cross = x * y1 - x1 * y
    A2 = cross.sum()
    if abs(A2) < 1e-9:
        return float(x.mean()), float(y.mean())
    cx = ((x + x1) * cross).sum() / (3 * A2)
    cy = ((y + y1) * cross).sum() / (3 * A2)
    return float(cx), float(cy)


def _voronoi_centroids(real_pts, aux_pts):
    """Return, for each point in real_pts, the centroid of its Voronoi cell
    within Voronoi(real_pts u aux_pts). Falls back to the original point if
    its cell is unbounded (should not happen once aux_pts properly encloses
    real_pts, but guarded defensively)."""
    from scipy.spatial import Voronoi
    import numpy as np
    n = len(real_pts)
    allpts = np.vstack([real_pts, aux_pts])
    vor = Voronoi(allpts)
    out = []
    unbounded = 0
    for i in range(n):
        region_idx = vor.point_region[i]
        region = vor.regions[region_idx]
        if not region or -1 in region:
            unbounded += 1
            out.append(tuple(real_pts[i]))
            continue
        verts = vor.vertices[region]
        out.append(_polygon_centroid(verts))
    return out, unbounded, vor


def _ridge_adjacency(n, vor):
    """Adjacency among the first n points (the real regions) from a Voronoi
    diagram's ridge_points (equivalent to the dual Delaunay edges)."""
    adjset = [set() for _ in range(n)]
    for a, b in vor.ridge_points:
        if a < n and b < n:
            adjset[a].add(int(b))
            adjset[b].add(int(a))
    return adjset


def gen_map(rng, force_np=None, force_kp=None):
    import math
    import numpy as np

    L = 10000
    D = 100
    A_ARCS = 24

    # Finals constraints: odd N with 180 < N < 250, hence
    # N = 2*Np+1 and 90 <= Np <= 124.
    Np = force_np if force_np is not None else rng.randint(90, 124)
    N = 2 * Np + 1
    k_lo = math.ceil(math.sqrt(N) - 1)
    k_hi = math.floor(math.sqrt(N) + 4)
    odd_k = [k for k in range(k_lo, k_hi + 1) if k % 2 == 1]
    if not odd_k:
        raise RuntimeError(f"no odd K satisfies finals constraints for N={N}")
    Kp = force_kp if force_kp is not None else (rng.choice(odd_k) - 1) // 2

    # ---- (3) sample P: mirror-symmetric, D-separated, distinct-x lattice points
    pts = [(0, 0)]
    attempts = 0
    while len(pts) < 2 * Np + 1:
        attempts += 1
        if attempts > 400000:
            raise RuntimeError("map generation failed to converge")
        ux = rng.randint(-L, L)
        uy = rng.randint(-L, L)
        if ux * ux + uy * uy > L * L:
            continue
        ok = True
        for (vx, vy) in pts:
            if ux == vx:
                ok = False
                break
            if (ux - vx) ** 2 + (uy - vy) ** 2 < D * D:
                ok = False
                break
        if not ok:
            continue
        pts.append((ux, uy))
        pts.append((-ux, -uy))

    # ---- assign region indices 0..N-1 by x-ascending order (fixed from here on)
    order = sorted(range(len(pts)), key=lambda i: pts[i][0])
    P = np.array([pts[i] for i in order], dtype=float)
    N = len(P)

    # ---- (4) auxiliary ring Q: A equally spaced points at radius 1.5L
    # (dividing the upper/lower semicircle between the x-axis crossings into
    # A/2 equal arcs each is the same as A equally spaced points around C)
    ring_r = 1.5 * L
    Q = np.array([
        (ring_r * math.cos(2 * math.pi * k / A_ARCS), ring_r * math.sin(2 * math.pi * k / A_ARCS))
        for k in range(A_ARCS)
    ], dtype=float)

    # ---- (5) first Voronoi(P u Q) pass -> relax P to its cells' centroids (P')
    centroids, unbounded1, _ = _voronoi_centroids(P, Q)
    if unbounded1:
        print(f"[judge] warning: {unbounded1} region(s) had an unbounded Voronoi "
              f"cell in the first relaxation pass; kept their original position", file=sys.stderr)
    Pp = np.array(centroids, dtype=float)

    # ---- second Voronoi(P' u Q) pass -> final cell shapes / adjacency
    _, unbounded2, vor2 = _voronoi_centroids(Pp, Q)
    if unbounded2:
        print(f"[judge] warning: {unbounded2} region(s) had an unbounded Voronoi "
              f"cell in the final pass", file=sys.stderr)
    adjset = _ridge_adjacency(N, vor2)
    adj = [sorted(s) for s in adjset]

    # final integer coordinates (the protocol/main.c++ parse x,y as integers)
    xs = [int(round(v)) for v in Pp[:, 0]]
    ys = [int(round(v)) for v in Pp[:, 1]]

    # actual cell polygons (for rendering the map as real Voronoi territory,
    # not just dots+edges) -- one closed vertex ring per region, drawn from
    # the same final Voronoi pass that produced the adjacency graph.
    polys = []
    for i in range(N):
        region_idx = vor2.point_region[i]
        region = vor2.regions[region_idx]
        if not region or -1 in region:
            cx, cy = Pp[i]
            s = 60.0
            polys.append([[cx - s, cy - s], [cx + s, cy - s], [cx + s, cy + s], [cx - s, cy + s]])
            continue
        verts = vor2.vertices[region]
        polys.append([[round(float(vx), 1), round(float(vy), 1)] for vx, vy in verts])

    # stronghold selection (section 5, step 7) -- rejection sampling can paint
    # itself into a corner (no valid candidate left for the current partial R),
    # so retry the whole selection with a fresh R a few times before giving up.
    mid = Np
    K_target = 2 * Kp + 1
    all_regions = list(range(N))

    def try_select_strongholds():
        R = {mid}
        tries = 0
        while len(R) < K_target:
            tries += 1
            if tries > 500000:
                # fallback: brute-force scan remaining valid candidates
                candidates = []
                for t in all_regions:
                    if t in R:
                        continue
                    mirror = N - 1 - t
                    if t == mirror or mirror in adjset[t]:
                        continue
                    bad = False
                    for r in R:
                        if r in adjset[t] or r in adjset[mirror]:
                            bad = True
                            break
                    if not bad:
                        candidates.append(t)
                if not candidates:
                    return None
                t = rng.choice(candidates)
            else:
                t = rng.randint(0, N - 1)
                if t in R:
                    continue
                mirror = N - 1 - t
                if t == mirror or mirror in adjset[t]:
                    continue
                bad = False
                for r in R:
                    if r in adjset[t] or r in adjset[mirror]:
                        bad = True
                        break
                if bad:
                    continue
            R.add(t)
            R.add(N - 1 - t)
        return R

    R = None
    for _ in range(20):
        R = try_select_strongholds()
        if R is not None:
            break
    if R is None:
        raise RuntimeError("stronghold generation stuck after 20 restart attempts")

    strongholds = sorted(R)
    K = len(strongholds)
    return dict(N=N, K=K, x=xs, y=ys, strongholds=strongholds, adj=adj, polys=polys)


def load_map_from_file(path):
    """디버그 재현용: .txt 로그(또는 순수 맵 파일)의 MAP 블록을 읽어 gen_map과
    같은 형식의 맵 dict으로 만든다. 시드로 생성하는 대신 로그에 박힌 그 맵을
    그대로 쓰므로, 시드를 몰라도 그 판의 맵을 100% 재현할 수 있다.

    기대 형식(로그의 MAP 블록):
        MAP
        <N> <K>
        <x_0 ... x_{N-1}>
        <y_0 ... y_{N-1}>
        STRONGHOLDS <s...>      (STRONGHOLDS 접두어는 있어도/없어도 됨)
        <deg n1 n2 ...>  * N줄
        END MAP
    "MAP" 마커가 없으면 파일 첫 줄이 "N K"인 순수 맵 파일로 간주한다.
    polys(렌더링용)는 좌표 둘레 사각형으로 근사한다 — 게임 시뮬은 polys를 안 씀."""
    with open(path, "r", encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]

    start = 0
    for i, ln in enumerate(lines):
        if ln == "MAP":
            start = i + 1
            break

    def ints(s):
        return [int(t) for t in s.split()]

    nk = ints(lines[start])
    N, K = nk[0], nk[1]
    xs = ints(lines[start + 1])
    ys = ints(lines[start + 2])
    st_tokens = lines[start + 3].split()
    if st_tokens and st_tokens[0].upper() == "STRONGHOLDS":
        st_tokens = st_tokens[1:]
    strongholds = sorted(int(t) for t in st_tokens)
    adj = []
    for i in range(N):
        tk = ints(lines[start + 4 + i])
        deg = tk[0]
        adj.append(sorted(tk[1:1 + deg]))

    s = 60.0
    polys = [[[xs[i] - s, ys[i] - s], [xs[i] + s, ys[i] - s],
              [xs[i] + s, ys[i] + s], [xs[i] - s, ys[i] + s]] for i in range(N)]

    if len(strongholds) != K:
        print(f"[judge] warning: map-file K={K} 인데 스트롱홀드 {len(strongholds)}개 파싱됨",
              file=sys.stderr)
    return dict(N=N, K=K, x=xs, y=ys, strongholds=strongholds, adj=adj, polys=polys)


def euclid_ceil(x, y, u, v):
    import math
    dx = x[u] - x[v]
    dy = y[u] - y[v]
    return math.ceil(math.sqrt(dx * dx + dy * dy))


def floyd_warshall(N, x, y, adj):
    INF = float("inf")
    dist = [[INF] * N for _ in range(N)]
    nxt = [[-1] * N for _ in range(N)]
    for i in range(N):
        dist[i][i] = 0.0
        nxt[i][i] = i
    for u in range(N):
        for v in adj[u]:
            w = euclid_ceil(x, y, u, v)
            if w < dist[u][v]:
                dist[u][v] = w
    for k in range(N):
        dk = dist[k]
        for u in range(N):
            duk = dist[u][k]
            if duk == INF:
                continue
            du = dist[u]
            for v in range(N):
                cand = duk + dk[v]
                if cand < du[v]:
                    du[v] = cand
    for u in range(N):
        for v in range(N):
            if u == v or dist[u][v] == INF:
                continue
            best = INF
            bestn = -1
            for nb in adj[u]:  # ascending order -> ties keep smallest neighbor
                if dist[nb][v] == INF:
                    continue
                score = euclid_ceil(x, y, u, nb) + dist[nb][v]
                if score < best:
                    best = score
                    bestn = nb
            nxt[u][v] = bestn
    return dist, nxt


# ---------------------------------------------------------------------------
# Game state
# ---------------------------------------------------------------------------
@dataclass
class Warrior:
    side: str
    num: int
    region: int
    hp: int
    state: str = "STATIONARY"  # or MOVING
    target: int = 0
    prev_region: int = -1

    @property
    def id(self):
        return f"{self.side}{self.num}"


@dataclass
class Building:
    region: int
    side: str
    btype: str  # HQ or BASE
    level: int = 1
    hp: int = 0

    def table(self):
        return HQ_LEVELS if self.btype == "HQ" else BASE_LEVELS

    def max_hp(self):
        return self.table()[self.level]["hp"]

    def turret(self):
        return self.table()[self.level]["turret"]

    def work_cap(self):
        return self.table()[self.level]["work_cap"]

    def max_level(self):
        return HQ_MAX_LEVEL if self.btype == "HQ" else BASE_MAX_LEVEL


class Engine:
    def __init__(self, M):
        self.M = M
        self.N = M["N"]
        self.x, self.y, self.adj = M["x"], M["y"], M["adj"]
        self.strongholds = set(M["strongholds"])
        self.dist, self.nxt = floyd_warshall(self.N, self.x, self.y, self.adj)

        self.gold = {"A": START_GOLD, "B": START_GOLD}
        self.next_num = {"A": START_WARRIORS + 1, "B": START_WARRIORS + 1}
        self.warriors = []
        for side, hq in (("A", 0), ("B", self.N - 1)):
            for i in range(1, START_WARRIORS + 1):
                self.warriors.append(Warrior(side, i, hq, HQ_LEVELS[1]["whp"]))
        self.buildings = {
            0: Building(0, "A", "HQ", 1, HQ_LEVELS[1]["hp"]),
            self.N - 1: Building(self.N - 1, "B", "HQ", 1, HQ_LEVELS[1]["hp"]),
        }

    def hq_region(self, side):
        return 0 if side == "A" else self.N - 1

    def warriors_at(self, region, side=None):
        return [w for w in self.warriors if w.region == region and (side is None or w.side == side)]

    def next_hop(self, u, v):
        return self.nxt[u][v]

    def visible_regions(self, side):
        """Finals vision: graph distance at most two from every own unit/building."""
        visible = set()
        frontier = [w.region for w in self.warriors if w.side == side]
        frontier += [b.region for b in self.buildings.values() if b.side == side]
        visible.update(frontier)
        for _ in range(2):
            nxt = []
            for region in frontier:
                for neighbor in self.adj[region]:
                    if neighbor in visible:
                        continue
                    visible.add(neighbor)
                    nxt.append(neighbor)
            frontier = nxt
        return visible

    # -------------------- BUILD --------------------
    def apply_build(self, side, regions):
        done = set()
        applied = []
        for r in regions:
            if r in done:
                continue
            if not (0 <= r < self.N):
                continue
            mine = self.warriors_at(r, side)
            enemy = self.warriors_at(r, OTHER[side])
            if not mine or enemy:
                continue
            b = self.buildings.get(r)
            if b is None:
                if r not in self.strongholds:
                    continue
                cost = BASE_LEVELS[1]["cost"]
                if self.gold[side] < cost:
                    continue
                self.gold[side] -= cost
                self.buildings[r] = Building(r, side, "BASE", 1, BASE_LEVELS[1]["hp"])
            else:
                if b.side != side:
                    continue
                if b.level >= b.max_level():
                    cost = HQ_HEAL_COST if b.btype == "HQ" else BASE_HEAL_COST
                    if self.gold[side] < cost:
                        continue
                    self.gold[side] -= cost
                    b.hp = b.max_hp()
                else:
                    tab = b.table()
                    cost = tab[b.level + 1]["cost"]
                    if self.gold[side] < cost:
                        continue
                    self.gold[side] -= cost
                    b.level += 1
                    b.hp = b.max_hp()
            done.add(r)
            applied.append(r)
        return applied

    # -------------------- MOVE --------------------
    def apply_move_orders(self, side, orders, free_targets=None):
        if free_targets is None:
            free_targets = {
                b.region for b in self.buildings.values() if b.side == side
            }
        for wid, target in orders:
            if not (0 <= target < self.N):
                continue
            w = self.find_warrior(side, wid)
            if w is None or w.state == "MOVING":
                continue
            cost = 0 if target in free_targets else MOVE_COST
            if self.gold[side] < cost:
                continue
            self.gold[side] -= cost
            w.state = "MOVING"
            w.target = target

    def step_moves(self):
        # Blocking must be judged against the board as it stood at the start of
        # today's move phase, not against positions already updated earlier in
        # this same loop -- otherwise the outcome would depend on iteration
        # order, which the rules do not intend (movement is simultaneous).
        occ = {}
        for w in self.warriors:
            occ.setdefault(w.region, set()).add(w.side)

        moved = []
        for w in self.warriors:
            if w.state != "MOVING":
                continue
            enemy_here = bool(occ.get(w.region, set()) - {w.side})
            if enemy_here:
                continue
            if w.region == w.target:
                w.state = "STATIONARY"
                continue
            nx = self.next_hop(w.region, w.target)
            if nx is None or nx < 0:
                continue
            w.prev_region = w.region
            w.region = nx
            moved.append(w)
            if w.region == w.target:
                w.state = "STATIONARY"
        return moved

    # -------------------- TRAIN --------------------
    def apply_train(self, side, n):
        hqr = self.hq_region(side)
        b = self.buildings.get(hqr)
        cap = HQ_LEVELS[b.level]["train_cap"] if b else HQ_LEVELS[1]["train_cap"]
        n = max(0, min(n, cap))
        n = min(n, self.gold[side] // TRAIN_COST)
        created = []
        whp = HQ_LEVELS[b.level]["whp"] if b else HQ_LEVELS[1]["whp"]
        for _ in range(n):
            self.gold[side] -= TRAIN_COST
            num = self.next_num[side]
            self.next_num[side] += 1
            w = Warrior(side, num, hqr, whp)
            self.warriors.append(w)
            created.append(w)
        return created

    def find_warrior(self, side, wid):
        for w in self.warriors:
            if w.side == side and w.num == wid:
                return w
        return None

    # -------------------- COMBAT --------------------
    @staticmethod
    def deliver(count, defenders, dmg, bld_hp):
        for _ in range(count):
            idx = -1
            for i, dw in enumerate(defenders):
                if dmg[i] >= dw.hp:
                    continue
                if idx == -1 or dw.hp - dmg[i] < defenders[idx].hp - dmg[idx] or (
                    dw.hp - dmg[i] == defenders[idx].hp - dmg[idx] and dw.num < defenders[idx].num
                ):
                    idx = i
            if idx != -1:
                dmg[idx] += 1
            elif bld_hp[0] > 0:
                bld_hp[0] -= 1

    def resolve_combat(self):
        damage_events = []  # {cause, side, num, dmg}
        siege_log = {}      # region -> (building owner, total dmg this day)

        for r in range(self.N):
            aW = self.warriors_at(r, "A")
            bW = self.warriors_at(r, "B")
            if not aW and not bW:
                continue
            b = self.buildings.get(r)
            a_turret = b.turret() if (b and b.side == "A") else 0
            b_turret = b.turret() if (b and b.side == "B") else 0
            a_bld_hp = [b.hp] if (b and b.side == "A") else [-1]
            b_bld_hp = [b.hp] if (b and b.side == "B") else [-1]

            a_dmg = [0] * len(aW)
            b_dmg = [0] * len(bW)

            self.deliver(a_turret, bW, b_dmg, b_bld_hp)
            self.deliver(b_turret, aW, a_dmg, a_bld_hp)
            a_turret_dmg = list(a_dmg)
            b_turret_dmg = list(b_dmg)
            self.deliver(len(aW), bW, b_dmg, b_bld_hp)
            self.deliver(len(bW), aW, a_dmg, a_bld_hp)

            for warriors, total, turret in (
                (aW, a_dmg, a_turret_dmg),
                (bW, b_dmg, b_turret_dmg),
            ):
                for w, total_dmg, turret_dmg in zip(warriors, total, turret):
                    combat_dmg = total_dmg - turret_dmg
                    if turret_dmg > 0:
                        damage_events.append(dict(
                            cause="TURRET", side=w.side, num=w.num,
                            dmg=turret_dmg))
                    if combat_dmg > 0:
                        damage_events.append(dict(
                            cause="COMBAT", side=w.side, num=w.num,
                            dmg=combat_dmg))
                    w.hp -= total_dmg

            if b is not None:
                lost = b.hp - (a_bld_hp[0] if b.side == "A" else b_bld_hp[0])
                if lost > 0:
                    owner, old_damage = siege_log.get(r, (b.side, 0))
                    siege_log[r] = (owner, old_damage + lost)
                    b.hp -= lost

        self.warriors = [w for w in self.warriors if w.hp > 0]
        dead_regions = [r for r, b in self.buildings.items() if b.hp <= 0]
        for r in dead_regions:
            del self.buildings[r]

        return damage_events, siege_log, dead_regions

    # -------------------- LABOR / SUPPLY --------------------
    def resolve_income(self):
        for b in self.buildings.values():
            cnt = len(self.warriors_at(b.region, b.side))
            self.gold[b.side] += WORK_INCOME * min(cnt, b.work_cap())

    def resolve_upkeep(self):
        hunger_events = []
        for side in ("A", "B"):
            ws = sorted([w for w in self.warriors if w.side == side], key=lambda w: w.num)
            for w in ws:
                if self.gold[side] >= UPKEEP_PER_WARRIOR:
                    self.gold[side] -= UPKEEP_PER_WARRIOR
                else:
                    w.hp -= 1
                    hunger_events.append(dict(
                        cause="HUNGER", side=w.side, num=w.num, dmg=1))
        self.warriors = [w for w in self.warriors if w.hp > 0]
        return hunger_events

    def hq_hp(self, side):
        b = self.buildings.get(self.hq_region(side))
        return b.hp if b else 0

    def hq_alive(self, side):
        return self.hq_region(side) in self.buildings


# ---------------------------------------------------------------------------
# Agent process wrapper
# ---------------------------------------------------------------------------
class Agent:
    RESPONSE_TIMEOUT_SECONDS = 10.0
    _live = set()

    def __init__(self, side, exe):
        self.side = side
        self._closed = False
        self._stdout_queue = queue.Queue()
        self._stderr_tail = ""
        self.proc = subprocess.Popen(
            [exe],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._live.add(self)

        def pump_stdout():
            try:
                for line in self.proc.stdout:
                    self._stdout_queue.put(line)
            finally:
                self._stdout_queue.put(None)

        def pump_stderr():
            for line in self.proc.stderr:
                self._stderr_tail = (self._stderr_tail + line)[-4000:]

        threading.Thread(target=pump_stdout, daemon=True).start()
        threading.Thread(target=pump_stderr, daemon=True).start()

    def send(self, line):
        self.proc.stdin.write(line if line.endswith("\n") else line + "\n")
        self.proc.stdin.flush()

    def readline(self):
        try:
            line = self._stdout_queue.get(timeout=self.RESPONSE_TIMEOUT_SECONDS)
        except queue.Empty:
            self.close()
            raise TimeoutError(
                f"agent {self.side} did not answer within "
                f"{self.RESPONSE_TIMEOUT_SECONDS:.0f}s"
            )
        if line is None:
            raise RuntimeError(
                f"agent {self.side} closed stdout unexpectedly. "
                f"stderr:\n{self._stderr_tail}"
            )
        return line.rstrip("\n")

    def close(self):
        if self._closed:
            return
        self._closed = True
        self._live.discard(self)
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.terminate()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=1)
        except Exception:
            try:
                self.proc.kill()
                self.proc.wait(timeout=1)
            except Exception:
                pass

    @classmethod
    def close_all(cls):
        for agent in list(cls._live):
            agent.close()


class ScriptedAgent:
    """실제 프로세스 대신, 로그에서 뽑은 그 쪽의 COMMAND를 매 턴 그대로 돌려주는
    가짜 에이전트. Agent와 인터페이스(send/readline/close)가 같아서 run_one_game이
    구분 없이 쓴다. send()로 들어오는 init/결과 라인은 무시하고, "START..."가 올
    때마다 그 다음 턴 블록(COMMAND/명령들/END)을 출력 큐에 쌓는다. 턴 번호 정합은
    로그의 턴 키를 오름차순으로 정렬해 n번째 START에 n번째 키를 매기므로, 로그가
    0-based든 1-based든(또는 시작 턴이 뭐든) 알아서 맞는다."""

    def __init__(self, side, cmds_by_turn):
        self.side = side
        self.turns = sorted(cmds_by_turn.keys())
        self.cmds = cmds_by_turn
        self.step = 0
        self.out = []
        self.oi = 0
        self.okd = False

    def send(self, line):
        if line.startswith("START"):
            self.out.append("COMMAND")
            if self.step < len(self.turns):
                for c in self.cmds[self.turns[self.step]]:
                    self.out.append(c)
            self.out.append("END")
            self.step += 1
        # 그 외(READY/좌표/인접/턴결과 등)는 대본 봇이므로 무시한다.

    def readline(self):
        if self.oi < len(self.out):
            v = self.out[self.oi]
            self.oi += 1
            return v
        if not self.okd:  # send_init 이 기대하는 OK
            self.okd = True
            return "OK"
        return "END"  # 안전장치(정상 흐름에선 도달 안 함)

    def close(self):
        pass


def parse_log_commands(path):
    """.txt 로그(트랜스크립트)에서 턴별 COMMAND 블록을 뽑아
    {"A": {turn: [line,...]}, "B": {turn: [line,...]}} 로 돌려준다. COMMAND LEFT->A,
    RIGHT->B. 빈 블록도 그 턴 키를 만들어(빈 리스트) 턴 정합이 어긋나지 않게 한다."""
    with open(path, "r", encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    cmds = {"A": {}, "B": {}}
    cur = None
    mode = None  # "A" / "B" / None
    for ln in lines:
        tk = ln.split()
        if not tk:
            continue
        if tk[0] == "TURN" and len(tk) == 2:  # "TURN k" (RESULT는 3토큰이라 제외)
            cur = int(tk[1])
            mode = None
            continue
        if tk[0] == "COMMAND" and len(tk) >= 3:
            side = "A" if tk[1] == "LEFT" else "B" if tk[1] == "RIGHT" else None
            if side is not None:
                if tk[2] == "START":
                    mode = side
                    if cur is not None:
                        cmds[side].setdefault(cur, [])  # 빈 블록도 키 생성
                else:
                    mode = None
            continue
        if mode is not None and cur is not None:
            cmds[mode].setdefault(cur, []).append(ln)
    return cmds


def load_official_transcript(path):
    """Parse a finals transcript and reconstruct each authoritative end-of-turn state."""
    from pathlib import Path
    try:
        from NEXT_VISION.build_sample_replays import parse_log
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "NEXT_VISION/build_sample_replays.py is required for --transcript"
        ) from exc
    return parse_log(Path(path))


def _frame_visible_regions(game_map, frame, observer):
    visible = set()
    frontier = [u["r"] for u in frame["warrior_units"]
                if u["side"] == observer]
    frontier += [b["r"] for b in frame["buildings"]
                 if b["side"] == observer]
    visible.update(frontier)
    for _ in range(2):
        nxt = []
        for region in frontier:
            for neighbor in game_map["adj"][region]:
                if neighbor in visible:
                    continue
                visible.add(neighbor)
                nxt.append(neighbor)
        frontier = nxt
    return visible


def send_recorded_turn_result(agent, game_map, frame, observer):
    """Feed one authoritative transcript frame using the finals wire protocol."""
    agent.send(f"TURN {frame['t']}")
    # The text transcript records labelled wall times, but strategies only
    # receive the five numeric protocol fields. Countdown values do not affect
    # replay state, so keep both sides at their normal initial allowance.
    agent.send("TIME 0 5 0 5")

    upgraded = frame["upgraded"]
    agent.send(f"UPGRADE {len(upgraded)}")
    for item in upgraded:
        agent.send(f"{item['side']} {item['r']}")

    trained = frame["trained"]
    agent.send(f"TRAIN {len(trained)}")
    if trained:
        agent.send(" ".join(item["id"] for item in trained))

    moved = frame["moved"]
    agent.send(f"MOVE {len(moved)}")
    for item in moved:
        agent.send(f"{item['id']} {item['to']}")

    damaged = frame["damaged"]
    agent.send(f"DAMAGE {len(damaged)}")
    for item in damaged:
        agent.send(f"{item['cause']} {item['id']} {item['dmg']}")

    sieged = frame["sieged"]
    agent.send(f"SIEGE {len(sieged)}")
    for item in sieged:
        agent.send(f"{item['side']} {item['r']} {item['dmg']}")

    visible = _frame_visible_regions(game_map, frame, observer)
    warriors = sorted(
        (u for u in frame["warrior_units"] if u["r"] in visible),
        key=lambda u: (u["side"], int(u["id"][1:])),
    )
    agent.send(f"WARRIOR {len(warriors)}")
    for unit in warriors:
        agent.send(f"{unit['id']} {unit['r']} {unit['hp']}")

    buildings = sorted(
        (b for b in frame["buildings"] if b["r"] in visible),
        key=lambda b: b["r"],
    )
    agent.send(f"BUILDING {len(buildings)}")
    for building in buildings:
        agent.send(
            f"{building['side']} {building['r']} {building['type']} "
            f"{building['level']} {building['hp']}"
        )
    agent.send("END")


def run_transcript_replay(exe, transcript_path, side):
    """Run one live agent against recorded observations and verify every command."""
    replay = load_official_transcript(transcript_path)
    game_map = replay["map"]
    frames = replay["frames"]
    side_name = "LEFT" if side == "A" else "RIGHT"
    agent = Agent(side, exe)
    matched = 0
    try:
        send_init(agent, game_map, side_name)
        for frame in frames:
            turn = frame["t"]
            agent.send(f"START TURN {turn}")
            actual_moves, actual_upgrades, actual_train = read_command_block(agent)

            recorded = frame["orders"][side]
            expected_moves = [
                (int(move["id"][1:]), move["to"])
                for move in recorded["move"]
            ]
            expected = (expected_moves, recorded["upgrade"], recorded["train"])
            actual = (actual_moves, actual_upgrades, actual_train)
            if actual != expected:
                raise RuntimeError(
                    f"transcript command mismatch at turn {turn} for {side}\n"
                    f"  expected moves={expected[0]} upgrades={expected[1]} "
                    f"train={expected[2]}\n"
                    f"  actual   moves={actual[0]} upgrades={actual[1]} "
                    f"train={actual[2]}"
                )

            matched += 1
            send_recorded_turn_result(agent, game_map, frame, side)
        agent.send("FINISH")
    finally:
        agent.close()

    result = replay["result"]
    print(f"[judge] authoritative replay matched {matched}/{len(frames)} turns")
    print(f"[judge] recorded result: winner={result['winner']} reason={result['reason']}")
    return matched


def send_init(agent, M, side_name):
    a = agent
    a.send(f"READY {side_name}")
    a.send(f"{M['N']} {M['K']}")
    a.send(" ".join(str(v) for v in M["x"]))
    a.send(" ".join(str(v) for v in M["y"]))
    a.send(" ".join(str(v) for v in M["strongholds"]))
    for i in range(M["N"]):
        nb = M["adj"][i]
        a.send(f"{len(nb)} " + " ".join(str(v) for v in nb))
    ok = a.readline()
    if ok != "OK":
        raise RuntimeError(f"agent {a.side} did not answer OK to init, got: {ok!r}")


def read_command_block(agent):
    line = agent.readline()
    assert line == "COMMAND", f"expected COMMAND, got {line!r}"
    moves = []
    upgrades = []
    train_n = 0
    while True:
        line = agent.readline()
        if line == "END":
            break
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "MOVE":
            wid = int(parts[1][1:])
            target = int(parts[2])
            moves.append((wid, target))
        elif parts[0] == "UPGRADE":
            upgrades.append(int(parts[1]))
        elif parts[0] == "TRAIN":
            train_n = int(parts[1])
    return moves, upgrades, train_n


def send_turn_result(agent, engine, turn, self_side, my_cd, opp_cd,
                     upgrades_applied, trained, moved, damage_events,
                     siege_log):
    """Send one complete finals-protocol result, filtered by the receiver's vision."""
    a = agent
    a.send(f"TURN {turn}")
    # TIME <my elapsed ms> <my countdown> <opp elapsed ms> <opp countdown>
    a.send(f"TIME 0 {my_cd} 0 {opp_cd}")
    a.send(f"UPGRADE {len(upgrades_applied)}")
    for side, region in upgrades_applied:
        a.send(f"{side} {region}")
    a.send(f"TRAIN {len(trained)}")
    if trained:
        a.send(" ".join(w.id for w in trained))
    a.send(f"MOVE {len(moved)}")
    for w in moved:
        a.send(f"{w.id} {w.region}")
    a.send(f"DAMAGE {len(damage_events)}")
    for event in damage_events:
        a.send(f"{event['cause']} {event['side']}{event['num']} {event['dmg']}")
    a.send(f"SIEGE {len(siege_log)}")
    for region, (owner, dmg) in siege_log.items():
        a.send(f"{owner} {region} {dmg}")

    visible = engine.visible_regions(self_side)
    warriors = sorted(
        (w for w in engine.warriors if w.region in visible),
        key=lambda w: (w.side, w.num),
    )
    a.send(f"WARRIOR {len(warriors)}")
    for w in warriors:
        a.send(f"{w.id} {w.region} {w.hp}")

    buildings = sorted(
        (b for b in engine.buildings.values() if b.region in visible),
        key=lambda b: b.region,
    )
    a.send(f"BUILDING {len(buildings)}")
    for b in buildings:
        a.send(f"{b.side} {b.region} {b.btype} {b.level} {b.hp}")
    a.send("END")


# ---------------------------------------------------------------------------
# Compilation helper: accept either a .cpp/.cc/.c++ source or a ready exe
# ---------------------------------------------------------------------------
SRC_EXTS = (".cpp", ".cc", ".c++", ".cxx")


def resolve_runnable(path, debug=False):
    import os
    path = os.path.abspath(path)
    root, ext = os.path.splitext(path)
    if ext.lower() not in SRC_EXTS:
        return path  # already an executable
    exe_path = root + (".debug.exe" if debug else ".exe")
    need_build = (not os.path.exists(exe_path)) or (os.path.getmtime(path) > os.path.getmtime(exe_path))
    if need_build:
        print(f"[judge] compiling {path} -> {exe_path}")
        cmd = ["g++", "-O2", "-std=c++20"]
        if debug:
            cmd.append("-DENABLE_DEBUG_LOG=1")
        cmd += ["-o", exe_path, path]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"[judge] compile failed for {path}:\n{r.stdout}\n{r.stderr}")
    else:
        print(f"[judge] using cached build {exe_path}")
    return exe_path


# ---------------------------------------------------------------------------
# Main simulation
# ---------------------------------------------------------------------------
def center_stronghold(M):
    """두 사령부(region 0, N-1) 좌표의 중점에 가장 가까운 스트롱홀드 region.
    스트롱홀드가 없으면 None."""
    strongholds = M["strongholds"]
    if not strongholds:
        return None
    N, x, y = M["N"], M["x"], M["y"]
    cx = (x[0] + x[N - 1]) / 2.0
    cy = (y[0] + y[N - 1]) / 2.0
    return min(strongholds, key=lambda r: (x[r] - cx) ** 2 + (y[r] - cy) ** 2)


def run_one_game(exe_a, exe_b, seed, record=True, map_override=None,
                 replay_side=None, replay_cmds=None):
    """Run a single simulated game. Returns (winner, reason, M, log_rows, frames).

    When record is False, the per-turn CSV rows / replay frames are not built
    (they're only needed for the one game whose replay gets written), which
    saves a fair amount of time/memory when running many games in parallel.

    map_override가 주어지면 시드로 맵을 생성하지 않고 그 맵을 그대로 쓴다
    (디버그 재현용 --map-file 모드).

    replay_side("A"/"B") + replay_cmds가 주어지면 그 쪽은 실제 프로세스 대신
    로그 대본(ScriptedAgent)으로 돌린다. 반대쪽만 라이브로 실행된다.
    """
    rng = random.Random(seed)

    M = map_override if map_override is not None else gen_map(rng)

    eng = Engine(M)

    center = center_stronghold(M)
    center_first = None  # 가운데 거점을 먼저 먹은(먼저 기지를 지은) 쪽: "A"/"B", 아직이면 None

    def make_agent(side, exe):
        if replay_side == side and replay_cmds is not None:
            return ScriptedAgent(side, replay_cmds.get(side, {}))
        return Agent(side, exe)

    agentA = make_agent("A", exe_a)
    agentB = make_agent("B", exe_b)
    send_init(agentA, M, "LEFT")
    send_init(agentB, M, "RIGHT")

    log_rows = []
    frames = []
    winner = None
    reason = None

    t0 = time.time()
    for turn in range(1, MAX_TURN + 1):
        decision_truth = None
        if record:
            # This snapshot is taken before START TURN, so it lines up exactly
            # with the strategy's `Tn VIEW ...` line written before decide().
            decision_truth = {}
            for observer in ("A", "B"):
                enemy = OTHER[observer]
                enemy_hq = eng.hq_region(enemy)
                visible = eng.visible_regions(observer)
                enemy_units = [w for w in eng.warriors if w.side == enemy]
                visible_enemy = [w for w in enemy_units if w.region in visible]
                decision_truth[observer] = dict(
                    actual_enemy_gold=eng.gold[enemy],
                    enemy_hq_visible=int(enemy_hq in visible),
                    visible_enemy=len(visible_enemy),
                    visible_enemy_at_hq=sum(
                        1 for w in visible_enemy if w.region == enemy_hq),
                    actual_enemy_alive=len(enemy_units),
                    actual_enemy_at_hq=sum(
                        1 for w in enemy_units if w.region == enemy_hq),
                    enemy_units=[
                        dict(
                            id=w.id,
                            region=w.region,
                            hp=w.hp,
                            visible=int(w.region in visible),
                        )
                        for w in sorted(enemy_units,
                                        key=lambda unit: unit.num)
                    ],
                )

        agentA.send(f"START TURN {turn}")
        agentB.send(f"START TURN {turn}")

        movesA, upgA, trainA = read_command_block(agentA)
        movesB, upgB, trainB = read_command_block(agentB)

        # MOVE is free only when its destination was an own building at command
        # submission time. A base built earlier in this same morning does not
        # retroactively make the order free.
        free_targets = {
            side: {b.region for b in eng.buildings.values() if b.side == side}
            for side in ("A", "B")
        }

        # ---- BUILD ----
        appliedA = eng.apply_build("A", upgA)
        appliedB = eng.apply_build("B", upgB)
        upgrades_applied = [("A", r) for r in appliedA] + [("B", r) for r in appliedB]

        # ---- MOVE ----
        eng.apply_move_orders("A", movesA, free_targets["A"])
        eng.apply_move_orders("B", movesB, free_targets["B"])
        moved = eng.step_moves()

        # ---- TRAIN ----
        trainedA = eng.apply_train("A", trainA)
        trainedB = eng.apply_train("B", trainB)
        trained = trainedA + trainedB

        # ---- COMBAT ----
        damage_events, siege_log, dead_regions = eng.resolve_combat()

        # 가운데 거점에 처음 기지가 생긴 시점의 소유 진영을 "먼저 먹은 쪽"으로
        # 한 번만 기록한다(이후 뺏겨도 불변).
        if center is not None and center_first is None:
            bc = eng.buildings.get(center)
            if bc is not None:
                center_first = bc.side

        hq_a_dead = not eng.hq_alive("A")
        hq_b_dead = not eng.hq_alive("B")

        # ---- LABOR / SUPPLY ----
        eng.resolve_income()
        damage_events += eng.resolve_upkeep()

        if record:
            log_rows.append(dict(
                turn=turn,
                goldA=eng.gold["A"], goldB=eng.gold["B"],
                warA=sum(1 for w in eng.warriors if w.side == "A"),
                warB=sum(1 for w in eng.warriors if w.side == "B"),
                hqA=eng.hq_hp("A"), hqB=eng.hq_hp("B"),
                basesA=sum(1 for b in eng.buildings.values() if b.side == "A" and b.btype == "BASE"),
                basesB=sum(1 for b in eng.buildings.values() if b.side == "B" and b.btype == "BASE"),
            ))
            warriors_by_region = {}
            for w in eng.warriors:
                d = warriors_by_region.setdefault(w.region, {"A": 0, "B": 0})
                d[w.side] += 1
            frames.append(dict(
                t=turn,
                decision_truth=decision_truth,
                gold={"A": eng.gold["A"], "B": eng.gold["B"]},
                hq={"A": eng.hq_hp("A"), "B": eng.hq_hp("B")},
                orders={
                    "A": {
                        "move": [dict(id=f"A{wid}", to=target) for wid, target in movesA],
                        "upgrade": list(upgA),
                        "train": trainA,
                    },
                    "B": {
                        "move": [dict(id=f"B{wid}", to=target) for wid, target in movesB],
                        "upgrade": list(upgB),
                        "train": trainB,
                    },
                },
                buildings=[
                    dict(r=b.region, side=b.side, type=b.btype, level=b.level, hp=b.hp)
                    for b in eng.buildings.values()
                ],
                warriors=warriors_by_region,
                warrior_units=[
                    dict(id=w.id, side=w.side, r=w.region, hp=w.hp,
                         state=w.state, target=w.target)
                    for w in eng.warriors
                ],
                upgraded=[dict(side=side, r=region) for side, region in upgrades_applied],
                moved=[dict(id=w.id, side=w.side, src=w.prev_region, to=w.region)
                       for w in moved],
                trained=[dict(id=w.id, side=w.side) for w in trained],
                damaged=[dict(id=f"{event['side']}{event['num']}", **event)
                         for event in damage_events],
                sieged=[dict(side=owner, r=r, dmg=dmg)
                        for r, (owner, dmg) in siege_log.items()],
            ))

        send_turn_result(agentA, eng, turn, "A", 5, 5, upgrades_applied,
                         trained, moved, damage_events, siege_log)
        send_turn_result(agentB, eng, turn, "B", 5, 5, upgrades_applied,
                         trained, moved, damage_events, siege_log)

        if hq_a_dead or hq_b_dead:
            if hq_a_dead and hq_b_dead:
                winner = None
                reason = f"both HQs destroyed simultaneously on day {turn}"
            elif hq_a_dead:
                winner = "B"
                reason = f"A's HQ destroyed on day {turn}"
            else:
                winner = "A"
                reason = f"B's HQ destroyed on day {turn}"
            break

    if winner is None and reason is None:
        # ran out of turns
        hqa, hqb = eng.hq_hp("A"), eng.hq_hp("B")
        if hqa > hqb:
            winner, reason = "A", f"{MAX_TURN} days elapsed, HQ hp {hqa} vs {hqb}"
        elif hqb > hqa:
            winner, reason = "B", f"{MAX_TURN} days elapsed, HQ hp {hqa} vs {hqb}"
        else:
            winner, reason = None, f"{MAX_TURN} days elapsed, HQ hp tied at {hqa} -> draw"

    agentA.send("FINISH")
    agentB.send("FINISH")
    agentA.close()
    agentB.close()

    if record:
        dt = time.time() - t0
        print(f"[judge] simulation finished in {dt:.1f}s")
        print(f"[judge] RESULT: {'draw' if winner is None else 'winner=' + winner} ({reason})")

    # a_first_center: A(agent1)가 가운데 거점을 먼저 먹었는가(먼저 기지를 지었는가)
    a_first_center = (center_first == "A")
    return winner, reason, seed, (M if record else None), log_rows, frames, a_first_center


def write_replay(exe_a, exe_b, seed, winner, reason, M, log_rows, frames):
    import os, csv, json

    script_dir = os.path.dirname(os.path.abspath(__file__))
    name_a = os.path.splitext(os.path.basename(exe_a))[0]
    name_b = os.path.splitext(os.path.basename(exe_b))[0]

    log_path = os.path.join(script_dir, "replay.csv")
    if os.path.exists(log_path):
        os.remove(log_path)
    with open(log_path, "w", newline="") as f:
        wtr = csv.DictWriter(f, fieldnames=list(log_rows[0].keys()))
        wtr.writeheader()
        wtr.writerows(log_rows)
    print(f"[judge] wrote log to {log_path}")

    replay = dict(
        map=dict(N=M["N"], K=M["K"], x=M["x"], y=M["y"], adj=M["adj"], strongholds=M["strongholds"], polys=M["polys"]),
        frames=frames,
        result=dict(winner=winner, reason=reason, seed=seed, nameA=name_a, nameB=name_b),
    )
    replay_json = json.dumps(replay)

    template_path = os.path.join(script_dir, "viewer_template.html")
    with open(template_path, "r", encoding="utf-8") as f:
        template = f.read()
    html = template.replace("__REPLAY_JSON__", replay_json)
    html_path = os.path.join(script_dir, "replay.html")
    if os.path.exists(html_path):
        os.remove(html_path)
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"[judge] wrote replay viewer to {html_path}")


def write_intel_debug(frames, sides):
    """Merge strategy-side fog estimates with the judge's hidden truth.

    The C++ log and decision_truth are both sampled immediately before the
    command for turn n, avoiding the common one-turn offset in replay checks.
    """
    import csv
    import os

    strategy_fields = (
        "enemy_gold_upper",
        "enemy_hq_visible",
        "observed_enemy",
        "observed_enemy_at_hq",
        "remembered_enemy",
        "remembered_enemy_in_visible_regions",
        "remembered_enemy_at_hq",
        "hq_scout_region",
    )
    fieldnames = [
        "turn", "side",
        *[f"agent_{name}" for name in strategy_fields],
        "judge_enemy_hq_visible",
        "judge_visible_enemy",
        "judge_visible_enemy_at_hq",
        "actual_enemy_alive",
        "actual_enemy_at_hq",
        "judge_actual_enemy_gold",
        "enemy_gold_error",
        "agent_tracked_enemy_units",
        "missing_enemy_count",
        "extra_enemy_count",
        "region_mismatch_count",
        "hp_mismatch_count",
        "visible_mismatch_count",
        "missing_enemy_ids",
        "extra_enemy_ids",
        "region_mismatch_ids",
        "hp_mismatch_ids",
        "visible_mismatch_ids",
    ]
    unit_fieldnames = [
        "turn", "side", "id",
        "agent_region", "actual_region", "region_match",
        "agent_hp", "actual_hp", "hp_match",
        "agent_visible", "judge_visible", "visible_match",
        "agent_last_seen", "agent_last_event",
    ]
    truth_by_turn = {frame["t"]: frame.get("decision_truth", {})
                     for frame in frames}
    script_dir = os.path.dirname(os.path.abspath(__file__))

    for side in sides:
        debug_path = os.path.abspath(f"debug_{side}.txt")
        if not os.path.exists(debug_path):
            print(f"[judge] warning: strategy debug log not found: {debug_path}",
                  file=sys.stderr)
            continue

        views = {}
        unit_views = {}
        with open(debug_path, "r", encoding="utf-8") as f:
            for raw in f:
                parts = raw.strip().split()
                if len(parts) < 2 or not parts[0].startswith("T"):
                    continue
                try:
                    turn = int(parts[0][1:])
                except ValueError:
                    continue
                values = {}
                for item in parts[2:]:
                    if "=" not in item:
                        continue
                    key, value = item.split("=", 1)
                    values[key] = value
                if parts[1] == "VIEW":
                    views[turn] = values
                elif parts[1] == "ENEMY_UNIT" and "id" in values:
                    unit_views.setdefault(turn, {})[values["id"]] = values

        rows = []
        unit_rows = []
        for turn in sorted(truth_by_turn):
            truth = truth_by_turn[turn].get(side)
            if truth is None:
                continue
            view = views.get(turn, {})
            agent_units = unit_views.get(turn, {})
            actual_units = {unit["id"]: unit
                            for unit in truth.get("enemy_units", [])}
            missing_ids = sorted(set(actual_units) - set(agent_units))
            extra_ids = sorted(set(agent_units) - set(actual_units))
            common_ids = sorted(set(agent_units) & set(actual_units))
            region_mismatch_ids = [
                unit_id for unit_id in common_ids
                if int(agent_units[unit_id]["region"]) !=
                   actual_units[unit_id]["region"]
            ]
            hp_mismatch_ids = [
                unit_id for unit_id in common_ids
                if int(agent_units[unit_id]["hp"]) !=
                   actual_units[unit_id]["hp"]
            ]
            visible_mismatch_ids = [
                unit_id for unit_id in common_ids
                if int(agent_units[unit_id]["visible"]) !=
                   actual_units[unit_id]["visible"]
            ]
            row = {"turn": turn, "side": side}
            for name in strategy_fields:
                row[f"agent_{name}"] = view.get(name, "")
            row.update(
                judge_enemy_hq_visible=truth["enemy_hq_visible"],
                judge_visible_enemy=truth["visible_enemy"],
                judge_visible_enemy_at_hq=truth["visible_enemy_at_hq"],
                actual_enemy_alive=truth["actual_enemy_alive"],
                actual_enemy_at_hq=truth["actual_enemy_at_hq"],
                judge_actual_enemy_gold=truth["actual_enemy_gold"],
                enemy_gold_error=(
                    int(view["enemy_gold_upper"]) -
                    truth["actual_enemy_gold"]
                    if view.get("enemy_gold_upper", "") != "" else ""
                ),
                agent_tracked_enemy_units=len(agent_units),
                missing_enemy_count=len(missing_ids),
                extra_enemy_count=len(extra_ids),
                region_mismatch_count=len(region_mismatch_ids),
                hp_mismatch_count=len(hp_mismatch_ids),
                visible_mismatch_count=len(visible_mismatch_ids),
                missing_enemy_ids="|".join(missing_ids),
                extra_enemy_ids="|".join(extra_ids),
                region_mismatch_ids="|".join(region_mismatch_ids),
                hp_mismatch_ids="|".join(hp_mismatch_ids),
                visible_mismatch_ids="|".join(visible_mismatch_ids),
            )
            rows.append(row)

            for unit_id in sorted(set(agent_units) | set(actual_units)):
                agent_unit = agent_units.get(unit_id, {})
                actual_unit = actual_units.get(unit_id, {})
                agent_region = agent_unit.get("region", "")
                actual_region = actual_unit.get("region", "")
                agent_hp = agent_unit.get("hp", "")
                actual_hp = actual_unit.get("hp", "")
                agent_visible = agent_unit.get("visible", "")
                judge_visible = actual_unit.get("visible", "")
                unit_rows.append(dict(
                    turn=turn,
                    side=side,
                    id=unit_id,
                    agent_region=agent_region,
                    actual_region=actual_region,
                    region_match=int(str(agent_region) == str(actual_region)),
                    agent_hp=agent_hp,
                    actual_hp=actual_hp,
                    hp_match=int(str(agent_hp) == str(actual_hp)),
                    agent_visible=agent_visible,
                    judge_visible=judge_visible,
                    visible_match=int(
                        str(agent_visible) == str(judge_visible)),
                    agent_last_seen=agent_unit.get("last_seen", ""),
                    agent_last_event=agent_unit.get("last_event", ""),
                ))

        out_path = os.path.join(script_dir, f"debug_intel_{side}.csv")
        with open(out_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        print(f"[judge] wrote strategy/truth intel comparison to {out_path}")

        unit_out_path = os.path.join(
            script_dir, f"debug_intel_units_{side}.csv")
        with open(unit_out_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=unit_fieldnames)
            writer.writeheader()
            writer.writerows(unit_rows)
        print(f"[judge] wrote unit-level intel comparison to {unit_out_path}")


# ---------------------------------------------------------------------------
# Tournament mode: NYPC 규정에 맞춘 스위스/풀리그 토너먼트.
#
#   시드      = 인자로 준 순서(앞일수록 작은 시드 = 상위). 동점 최종 타이브레이커.
#   점수      = 승 1 / 무 0.5 / 패·프로그램오류 0. 봇 크래시는 그 쪽 0점으로 처리하고
#               토너먼트는 계속 진행한다.
#   진영      = 한 매치의 여러 판에서 선공(A)/후공(B)을 번갈아 배정.
#   풀리그    = 모든 쌍이 games판. 최종순위 = 총점 → Sonneborn-Berger → 시드.
#   스위스    = rounds회. 매 라운드 부전승/점수·시드 기반 매칭(재대결 회피).
#               타이브레이커 = 우세 라운드 수 → 상대 총점합 → 초기라운드 가중 내점수
#               → 초기라운드 가중 상대점수 → 시드.
# ---------------------------------------------------------------------------
def _team_name(path):
    import os
    return os.path.splitext(os.path.basename(path))[0]


def _play_games(exe_list, specs, workers, desc):
    """specs: [(key, i, j, seed, i_is_A), ...] 를 병렬 실행하고
    key -> [pts_i, pts_j] (승 1/무 0.5/패 0 누적)를 돌려준다. 봇이 크래시하면
    (fut.result 예외) 메시지의 진영으로 패자를 판정하고, 불명확하면 무승부 처리."""
    from collections import defaultdict
    out = defaultdict(lambda: [0.0, 0.0])
    futs = {}
    with ProcessPoolExecutor(max_workers=workers) as pool:
        for (key, i, j, seed, i_is_A) in specs:
            a, b = (i, j) if i_is_A else (j, i)  # a=선공(A) 팀, b=후공(B) 팀
            fut = pool.submit(run_one_game, exe_list[a], exe_list[b], seed, False)
            futs[fut] = (key, i, j, i_is_A)
        with tqdm(total=len(specs), desc=desc, unit="game") as bar:
            for fut in as_completed(futs):
                key, i, j, i_is_A = futs[fut]
                try:
                    winner = fut.result()[0]  # "A"/"B"/None
                    outcome = "draw" if winner is None else winner
                except Exception as e:
                    # 봇(에이전트) 크래시/오류만 그 쪽 0점 처리하고 계속 진행한다.
                    # judge 자체 오류(맵 생성 실패, 의존성 누락 등)는 무승부로
                    # 뭉개면 순위가 오염되므로 그대로 터뜨린다.
                    msg = str(e)
                    if "agent A" in msg:
                        outcome = "B"
                    elif "agent B" in msg:
                        outcome = "A"
                    else:
                        raise
                    print(f"\n[judge] warning: 봇 오류로 패 처리 ({msg.splitlines()[0]})",
                          file=sys.stderr)
                if outcome == "draw":
                    pa, pb = 0.5, 0.5
                elif outcome == "A":
                    pa, pb = 1.0, 0.0
                else:
                    pa, pb = 0.0, 1.0
                # a=선공 팀. i가 선공이면 (pi,pj)=(pa,pb), 아니면 뒤집는다.
                pi, pj = (pa, pb) if i_is_A else (pb, pa)
                out[key][0] += pi
                out[key][1] += pj
                bar.update(1)
    return out


def _match_specs(i, j, games, rng):
    """i와 j의 한 매치(games판, 진영 번갈아) 게임 스펙 목록."""
    key = (i, j)
    return [(key, i, j, rng.randrange(1 << 30), (g % 2 == 0)) for g in range(games)]


def run_roundrobin(exe_list, names, games, workers, rng):
    P = len(exe_list)
    pts = [0.0] * P                       # 총점
    pair = [[0.0] * P for _ in range(P)]  # pair[i][j] = i가 j 상대로 딴 점수
    specs = []
    for i in range(P):
        for j in range(i + 1, P):
            specs += _match_specs(i, j, games, rng)
    print(f"[judge] 풀 리그: {P}팀, 쌍마다 {games}판, 총 {len(specs)}판")
    res = _play_games(exe_list, specs, workers, "[judge] round-robin")
    for (i, j), (pi, pj) in res.items():
        pts[i] += pi
        pts[j] += pj
        pair[i][j] += pi
        pair[j][i] += pj

    # Sonneborn-Berger: Σ_j (i가 j 상대로 딴 점수 × j의 총점)
    sb = [sum(pair[i][j] * pts[j] for j in range(P) if j != i) for i in range(P)]

    order = sorted(range(P), key=lambda i: (-pts[i], -sb[i], i))
    print("\n[judge] ===== 풀 리그 최종 순위 =====")
    print(f"[judge] {'순위':<4}{'팀':<22}{'총점':>8}{'SB':>10}{'시드':>6}")
    for rank, i in enumerate(order, 1):
        print(f"[judge] {rank:<4}{names[i]:<22}{pts[i]:>8.1f}{sb[i]:>10.1f}{i + 1:>6}")
    print(f"\n[judge] 🏆 우승: {names[order[0]]}")
    return order


def run_swiss(exe_list, names, games, workers, rounds, rng):
    P = len(exe_list)
    pts = [0.0] * P
    had_bye = [False] * P
    played = [set() for _ in range(P)]
    # rec[i] = 라운드별 (상대 idx 또는 None, 내 매치점수, 상대 매치점수)
    rec = [[] for _ in range(P)]

    for r in range(1, rounds + 1):
        active = list(range(P))
        bye = None
        if P % 2 == 1:
            cands = [i for i in active if not had_bye[i]] or active
            bye = min(cands, key=lambda i: (pts[i], i))  # 최저점 -> 최소시드
            active.remove(bye)
            had_bye[bye] = True

        # 점수 큰 순 -> 시드(=idx) 작은 순
        remaining = sorted(active, key=lambda i: (-pts[i], i))
        pairs = []
        while remaining:
            p = remaining.pop(0)
            unplayed = [q for q in remaining if q not in played[p]]
            pool_c = unplayed if unplayed else remaining
            # 후보 중 점수 최대 -> 시드 최대(=idx 최대)
            opp = max(pool_c, key=lambda q: (pts[q], q))
            remaining.remove(opp)
            pairs.append((p, opp))
            played[p].add(opp)
            played[opp].add(p)

        specs = []
        for (i, j) in pairs:
            specs += _match_specs(i, j, games, rng)
        res = _play_games(exe_list, specs, workers,
                          f"[judge] swiss R{r}/{rounds}")

        for (i, j) in pairs:
            pi, pj = res[(i, j)]
            pts[i] += pi
            pts[j] += pj
            rec[i].append((j, pi, pj))
            rec[j].append((i, pj, pi))
        if bye is not None:
            bp = games * 1.0  # 부전승 = 매치 완승
            pts[bye] += bp
            rec[bye].append((None, bp, 0.0))

    # ---- 타이브레이커 (모두 클수록 상위, 마지막에 시드 작을수록 상위) ----
    def tb1(i):  # 승점이 상대보다 큰 라운드 수
        return sum(1 for (o, mp, op) in rec[i] if o is not None and mp > op)

    def tb2(i):  # 대전 상대들의 총점 합
        return sum(pts[o] for (o, mp, op) in rec[i] if o is not None)

    def tb3(i):  # Σ i번째라운드 내점수 × (R - i + 1)
        return sum(mp * (rounds - k) for k, (o, mp, op) in enumerate(rec[i]))

    def tb4(i):  # Σ i번째라운드 상대점수 × (R - i + 1)
        return sum(op * (rounds - k) for k, (o, mp, op) in enumerate(rec[i]))

    order = sorted(range(P),
                   key=lambda i: (-pts[i], -tb1(i), -tb2(i), -tb3(i), -tb4(i), i))
    print("\n[judge] ===== 스위스 토너먼트 최종 순위 =====")
    print(f"[judge] {'순위':<4}{'팀':<22}{'총점':>7}{'우세R':>7}{'상대총점':>9}"
          f"{'가중내':>8}{'가중상대':>9}{'시드':>6}")
    for rank, i in enumerate(order, 1):
        print(f"[judge] {rank:<4}{names[i]:<22}{pts[i]:>7.1f}{tb1(i):>7}"
              f"{tb2(i):>9.1f}{tb3(i):>8.1f}{tb4(i):>9.1f}{i + 1:>6}")
    print(f"\n[judge] 🏆 우승: {names[order[0]]}")
    return order


def _pool_sort_key(fp):
    """제출 순서(=시드 순): 파일명이 숫자(제출 ID)면 그 값 오름차순, 아니면 이름순."""
    import os
    stem = os.path.splitext(os.path.basename(fp))[0]
    return (0, int(stem)) if stem.isdigit() else (1, stem.lower())


def _expand_pool(paths):
    """토너먼트는 컴파일하지 않고 이미 빌드된 .exe만 실행한다. 인자에 디렉터리가
    있으면 그 안의 .exe를 모으고, 소스(.cpp 등)를 직접 주면 옆의 같은 이름 .exe로
    바꾼다(없으면 에러). 디렉터리 안 파일은 제출 순서(_pool_sort_key)로 정렬해
    시드를 매긴다. 파일을 직접 나열하면 준 순서를 그대로 시드로 쓴다."""
    import os
    out = []
    for p in paths:
        if os.path.isdir(p):
            exes = [os.path.join(p, f) for f in os.listdir(p)
                    if f.lower().endswith(".exe")]
            if not exes:
                sys.exit(f"[judge] '{p}' 안에 .exe가 없습니다. 먼저 컴파일하세요.")
            out += sorted(exes, key=_pool_sort_key)
        else:
            root, ext = os.path.splitext(p)
            exe = p if ext.lower() == ".exe" else root + ".exe"
            if not os.path.exists(exe):
                sys.exit(f"[judge] '{exe}' 가 없습니다. 먼저 컴파일하세요 "
                         f"(토너먼트는 .exe만 실행합니다).")
            out.append(exe)
    # subprocess가 cwd 대신 PATH를 뒤지지 않도록 절대경로로 돌려준다.
    return [os.path.abspath(e) for e in out]


def run_tournament(files, mode, games, workers, rounds, seed):
    rng = random.Random(seed)
    exe_list = _expand_pool(files)  # 컴파일 없이 .exe 경로만 사용 (디렉터리는 펼침)
    names = [_team_name(e) for e in exe_list]
    if len(exe_list) < 2:
        sys.exit("[judge] 토너먼트에는 최소 2개의 에이전트가 필요합니다.")
    print(f"[judge] 시드 배정(인자 순): " +
          ", ".join(f"{k + 1}={names[k]}" for k in range(len(names))))
    if mode == "swiss":
        run_swiss(exe_list, names, games, workers, rounds, rng)
    else:
        run_roundrobin(exe_list, names, games, workers, rng)


def tournament_main(argv):
    ap = argparse.ArgumentParser(
        prog="judge.py tournament",
        description="에이전트 풀을 스위스/풀리그로 붙여 NYPC 규정대로 순위를 매긴다.")
    ap.add_argument("agents", nargs="+",
                    help="풀에 넣을 에이전트들 (.cpp 소스 또는 .exe, 2개 이상). "
                         "준 순서가 곧 시드다(앞일수록 상위 시드).")
    ap.add_argument("--mode", choices=("roundrobin", "swiss"), default="roundrobin",
                    help="대회 방식 (기본 roundrobin)")
    ap.add_argument("--games", type=int, default=5,
                    help="한 매치(쌍)당 경기 수 (기본 5)")
    ap.add_argument("--rounds", type=int, default=50,
                    help="스위스 모드 라운드 수 (기본 50)")
    ap.add_argument("--workers", type=int, default=8, help="병렬 워커 수 (기본 8)")
    ap.add_argument("--seed", type=int, default=None,
                    help="재현용 시드 (맵/대진 시드 생성의 기준)")
    args = ap.parse_args(argv)
    run_tournament(args.agents, args.mode, args.games, args.workers,
                   args.rounds, args.seed)


def main():
    ap = argparse.ArgumentParser(
        description="NEXT VISION finals local judge and authoritative replay debugger.")
    ap.add_argument("agent1", help="path to agent A's (LEFT) source (.cpp) or executable")
    ap.add_argument("agent2", nargs="?", default=None,
                    help="path to agent B's (RIGHT) source (.cpp) or executable "
                         "(--replay 사용 시 생략 가능: 상대는 로그로 대체됨)")
    ap.add_argument("--seed", type=int, default=None, help="optional map seed for reproducibility (only used for game 1)")
    ap.add_argument("--games", type=int, default=100, help="number of games to simulate (default: 100)")
    ap.add_argument("--workers", type=int, default=8, help="parallel worker processes (default: 8)")
    ap.add_argument("--map-file", default=None,
                    help="디버그: 이 파일(.txt 로그 등)의 MAP 블록을 맵으로 그대로 써서 "
                         "한 판만 재현·기록한다(시드 무시, 승패 무관 항상 기록).")
    ap.add_argument("--replay", default=None,
                    help="디버그: 이 로그 파일에서 상대 쪽 명령을 그대로 재생한다. "
                         "맵도(--map-file 없으면) 이 로그에서 읽는다. 내 에이전트(agent1)만 "
                         "라이브로 돌고 상대(agent2)는 생략 가능 — cpp에 붙여넣을 필요 없음.")
    ap.add_argument("--replay-side", default="B",
                    help="--replay 때 로그로 재생할 쪽 (A/LEFT 또는 B/RIGHT, 기본 B).")
    ap.add_argument("--transcript", default=None,
                    help="본선 공식 로그의 결과를 agent1에 그대로 재입력한다. 매 턴 명령도 "
                         "로그와 대조하므로 해당 리플레이의 내부 판단 로그를 정확히 재현한다.")
    ap.add_argument("--side", default="A", choices=("A", "B", "LEFT", "RIGHT"),
                    help="--transcript로 재현할 진영 (기본 A/LEFT).")
    ap.add_argument("--debug", action="store_true",
                    help="C++ 소스를 ENABLE_DEBUG_LOG=1로 빌드해 debug_A.txt/debug_B.txt를 생성한다.")
    args = ap.parse_args()

    import os

    exe_a = resolve_runnable(args.agent1, debug=args.debug)
    exe_b = resolve_runnable(args.agent2, debug=args.debug) if args.agent2 else None

    # ---- 정확한 디버그 재현: 공식 결과와 시야 스냅샷을 라이브 봇에 재입력 ----
    if args.transcript:
        if args.replay or args.map_file:
            ap.error("--transcript는 --replay/--map-file과 함께 사용할 수 없습니다")
        side = "A" if args.side in ("A", "LEFT") else "B"
        print(f"[judge] authoritative transcript: {args.transcript} side={side}")
        run_transcript_replay(exe_a, args.transcript, side)
        debug_path = os.path.abspath(f"debug_{side}.txt")
        if args.debug and os.path.exists(debug_path):
            print(f"[judge] wrote strategy debug log to {debug_path}")
        elif args.debug:
            print(f"[judge] warning: debug log was not created: {debug_path}",
                  file=sys.stderr)
        return

    # ---- 디버그: 상대를 로그로 재생 (맵도 로그에서). 내 에이전트만 라이브 ----
    if args.replay:
        rside = "A" if args.replay_side.upper() in ("A", "LEFT") else "B"
        replay_cmds = parse_log_commands(args.replay)
        M = load_map_from_file(args.map_file or args.replay)
        live = "A(agent1)" if rside == "B" else "B(agent2)"
        print(f"[judge] replay 모드: 상대 {rside} = 로그 '{args.replay}' 대본, "
              f"라이브 = {live}, 맵 = '{args.map_file or args.replay}' "
              f"(N={M['N']} K={M['K']})")
        tag = f"replay:{os.path.basename(args.replay)}"
        disp_a = tag if rside == "A" else exe_a
        disp_b = tag if rside == "B" else (exe_b or "agent2")
        winner, reason, seed, M2, log_rows, frames, _ = run_one_game(
            exe_a, exe_b, 0, record=True, map_override=M,
            replay_side=rside, replay_cmds=replay_cmds)
        print(f"[judge] result: winner={winner} reason={reason}")
        write_replay(disp_a, disp_b, 0, winner, reason, M2, log_rows, frames)
        if args.debug:
            write_intel_debug(frames, (OTHER[rside],))
        return

    # ---- 디버그: 맵만 로그에서 고정, 양쪽 다 라이브 ----
    if args.map_file:
        M = load_map_from_file(args.map_file)
        print(f"[judge] map-file 디버그 모드: {args.map_file} "
              f"(N={M['N']} K={M['K']}, strongholds={len(M['strongholds'])})")
        winner, reason, seed, M2, log_rows, frames, _ = run_one_game(
            exe_a, exe_b, 0, record=True, map_override=M)
        print(f"[judge] result: winner={winner} reason={reason}")
        write_replay(exe_a, exe_b, 0, winner, reason, M2, log_rows, frames)
        if args.debug:
            write_intel_debug(frames, ("A", "B"))
        return

    # 일반(토너먼트) 모드는 두 에이전트가 다 필요하다.
    if exe_b is None:
        sys.exit("[judge] agent2가 필요합니다. (상대를 로그로 대체하려면 --replay <로그> 를 쓰세요)")

    seeds = [
        args.seed if (args.seed is not None and i == 0) else random.randrange(1 << 30)
        for i in range(args.games)
    ]

    winsA = winsB = draws = 0
    a_loss_seed = None  # seed of a game where A (player 1) lost, for the replay

    # 가운데 거점을 A가 먼저 먹었는지 여부로 나눈 A(agent1) 승/패/무 집계.
    # cen = 먼저 먹은 판, noc = 못 먹은 판(B가 먼저 먹었거나 아무도 안 지음).
    cen = {"win": 0, "loss": 0, "draw": 0}
    noc = {"win": 0, "loss": 0, "draw": 0}

    def tally_center(winner, a_first):
        bucket = cen if a_first else noc
        if winner == "A":
            bucket["win"] += 1
        elif winner == "B":
            bucket["loss"] += 1
        else:
            bucket["draw"] += 1

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(run_one_game, exe_a, exe_b, seeds[i], record=False): i
            for i in range(args.games)
        }
        with tqdm(total=args.games, desc="[judge] games", unit="game") as bar:
            for fut in as_completed(futures):
                winner, reason, seed, M, log_rows, frames, a_first_center = fut.result()
                if winner == "A":
                    winsA += 1
                elif winner == "B":
                    winsB += 1
                    if a_loss_seed is None:
                        a_loss_seed = seed
                else:
                    draws += 1
                tally_center(winner, a_first_center)
                bar.update(1)
                bar.set_postfix(A=winsA, B=winsB, draw=draws)

    # keep sampling extra games until we find one where A lost, so the replay
    # always shows a loss for player 1 (rather than falling back to any game)
    extra_tries = 0
    while a_loss_seed is None and extra_tries < 200:
        extra_tries += 1
        seed = random.randrange(1 << 30)
        winner, reason, seed, M, log_rows, frames, a_first_center = run_one_game(exe_a, exe_b, seed, record=False)
        if winner == "A":
            winsA += 1
        elif winner == "B":
            winsB += 1
            a_loss_seed = seed
        else:
            draws += 1
        tally_center(winner, a_first_center)

    if a_loss_seed is None:
        print("[judge] warning: no game where A lost was found; skipping replay generation")
    else:
        print(f"[judge] re-simulating seed {a_loss_seed} (A lost) to build the replay")
        winner, reason, seed, M, log_rows, frames, _ = run_one_game(exe_a, exe_b, a_loss_seed, record=True)
        write_replay(exe_a, exe_b, seed, winner, reason, M, log_rows, frames)
        if args.debug:
            write_intel_debug(frames, ("A", "B"))

    total = winsA + winsB + draws
    print(f"[judge] ==================================================")
    print(f"[judge] played {total} games: A won {winsA} ({winsA / total:.1%}), "
          f"B won {winsB} ({winsB / total:.1%}), draws {draws} ({draws / total:.1%})")

    def _rate(d):
        t = d["win"] + d["loss"] + d["draw"]
        return f"{d['win'] / t:.1%}" if t else "n/a"

    cen_total = cen["win"] + cen["loss"] + cen["draw"]
    noc_total = noc["win"] + noc["loss"] + noc["draw"]
    print(f"[judge] --- 가운데 거점 선점 여부별 (A=agent1 기준) ---")
    print(f"[judge] A가 먼저 먹은 판 {cen_total}: A승 {cen['win']}, A패 {cen['loss']}, "
          f"무 {cen['draw']} (A승률 {_rate(cen)})")
    print(f"[judge] 못 먹은 판 {noc_total}: A승 {noc['win']}, A패 {noc['loss']}, "
          f"무 {noc['draw']} (A승률 {_rate(noc)})")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "tournament":
        tournament_main(sys.argv[2:])
    else:
        main()
