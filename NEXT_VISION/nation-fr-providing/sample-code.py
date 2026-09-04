#!/usr/bin/env python3
from __future__ import annotations

import heapq
import math
import sys
from dataclasses import dataclass, field
from enum import Enum
from typing import NamedTuple

MAX_TURN = 400          # maximum turn (days)
START_GOLD = 750        # initial gold
START_WARRIORS = 3      # initial warriors
MOVE_COST = 10          # move cost
TRAIN_COST = 120        # train cost
WORK_INCOME = 15        # income per warrior
UPKEEP_PER_WARRIOR = 2  # upkeep per warrior
HQ_MAX_LEVEL = 5        # HQ max level
BASE_MAX_LEVEL = 3      # base max level
HQ_HEAL_COST = 1000     # HQ fix cost
BASE_HEAL_COST = 500    # base fix cost
HOP_VISION = 2          # vision radius shared by all units


class HqLevelEntry(NamedTuple):
    upgrade_cost: int
    warrior_hp: int
    hp: int
    turret: int
    train_cap: int
    work_cap: int


class BaseLevelEntry(NamedTuple):
    cost: int
    hp: int
    turret: int
    work_cap: int


HQ_LEVELS: tuple[HqLevelEntry, ...] = (
    HqLevelEntry(0,     0, 0,  0, 0, 0),
    HqLevelEntry(0,     4, 10, 1, 1, 1),
    HqLevelEntry(600,   5, 15, 2, 1, 2),
    HqLevelEntry(1000,  6, 20, 2, 2, 3),
    HqLevelEntry(2000,  7, 25, 3, 2, 4),
    HqLevelEntry(3000,  8, 30, 3, 3, 5),
)
BASE_LEVELS: tuple[BaseLevelEntry, ...] = (
    BaseLevelEntry(0,    0,  0, 0),
    BaseLevelEntry(500,  6, 1, 1),
    BaseLevelEntry(550, 12, 1, 2),
    BaseLevelEntry(600, 18, 2, 3),
)


class Side(Enum):
    LEFT = "A"
    RIGHT = "B"

    @property
    def opposite(self) -> "Side":
        return Side.RIGHT if self is Side.LEFT else Side.LEFT

    @classmethod
    def from_word(cls, w: str) -> "Side":
        return cls.LEFT if w == "LEFT" else cls.RIGHT

    @classmethod
    def from_char(cls, c: str) -> "Side":
        return cls.LEFT if c == "A" else cls.RIGHT


class BType(Enum):
    HQ = "HQ"
    BASE = "BASE"


class WState(Enum):
    STATIONARY = 0
    MOVING = 1


@dataclass(frozen=True)
class WarriorId:
    side: Side
    num: int

    def __str__(self) -> str:
        return f"{self.side.value}{self.num}"

    @classmethod
    def parse(cls, tok: str) -> "WarriorId":
        assert tok and tok[0] in ("A", "B")
        return cls(Side.from_char(tok[0]), int(tok[1:]))


@dataclass
class Warrior:
    id: WarriorId
    region: int
    hp: int
    state: WState = WState.STATIONARY
    target: int = 0


@dataclass
class Building:
    region: int
    side: Side
    type: BType
    level: int = 1
    hp: int = 10

    def current_hp(self) -> int:
        return HQ_LEVELS[self.level].hp if self.type is BType.HQ else BASE_LEVELS[self.level].hp

    def work_cap(self) -> int:
        return HQ_LEVELS[self.level].work_cap if self.type is BType.HQ else BASE_LEVELS[self.level].work_cap

    def apply_upgrade(self) -> None:
        self.level += 1
        self.hp = self.current_hp()

    def upgrade_cost(self) -> int:
        if self.type is BType.HQ:
            return HQ_LEVELS[self.level + 1].upgrade_cost
        else:
            return BASE_LEVELS[self.level + 1].cost


@dataclass
class GameMap:
    N: int = 0
    K: int = 0
    x: list[int] = field(default_factory=list)
    y: list[int] = field(default_factory=list)
    strongholds: list[int] = field(default_factory=list)
    adj: list[list[int]] = field(default_factory=list)
    my_side: Side = Side.LEFT
    my_hq: int = 0
    opp_hq: int = 0

    def hq_of(self, s: Side) -> int:
        return 0 if s is Side.LEFT else self.N - 1


@dataclass
class GameState:
    gold: int = START_GOLD
    my_countdown: int = 5
    opp_countdown: int = 5
    warriors: list[Warrior] = field(default_factory=list)
    buildings: list[Building] = field(default_factory=list)
    visible: list[int] = field(default_factory=list)

    def find_building(self, region: int) -> Building | None:
        return next((b for b in self.buildings if b.region == region), None)

    def find_warrior(self, wid: WarriorId) -> Warrior | None:
        return next((w for w in self.warriors if w.id == wid), None)


@dataclass
class Actions:
    train_n: int = 0
    moves: list[tuple[WarriorId, int]] = field(default_factory=list)
    upgrades: list[int] = field(default_factory=list)


def make_base(region: int, s: Side) -> Building:
    return Building(region, s, BType.BASE, 1, BASE_LEVELS[1].hp)


def compute_visible(S: GameState, M: GameMap) -> list[int]:
    visible: set[int] = set()

    def add_hops(start: int, radius: int) -> None:
        seen = {start}
        frontier = [start]
        for _ in range(radius):
            next_frontier: list[int] = []
            for region in frontier:
                for neighbor in M.adj[region]:
                    if neighbor not in seen:
                        seen.add(neighbor)
                        next_frontier.append(neighbor)
            frontier = next_frontier
        visible.update(seen)

    for w in S.warriors:
        if w.id.side is M.my_side:
            add_hops(w.region, HOP_VISION)
    for b in S.buildings:
        if b.side is M.my_side:
            add_hops(b.region, HOP_VISION)
    return sorted(visible)


def readln() -> str:
    line = sys.stdin.readline()
    if not line:
        sys.exit(0)
    return line.rstrip("\n")


def read_tokens() -> list[str]:
    return readln().split()


def parse_init() -> tuple[GameMap, GameState]:
    M = GameMap()

    t = read_tokens()
    assert len(t) >= 2 and t[0] == "READY"
    M.my_side = Side.from_word(t[1])

    t = read_tokens()
    M.N, M.K = int(t[0]), int(t[1])

    M.x = [int(v) for v in read_tokens()]  # x_0 x_1 ... x_{N-1}
    M.y = [int(v) for v in read_tokens()]  # y_0 y_1 ... y_{N-1}

    M.strongholds = sorted(int(v) for v in read_tokens())  # K strongholds

    M.adj = [[] for _ in range(M.N)]
    for r in range(M.N):
        t = read_tokens()  # deg n_1 n_2 ...
        deg = int(t[0])
        M.adj[r] = sorted(int(v) for v in t[1:1 + deg])

    M.my_hq = M.hq_of(M.my_side)
    M.opp_hq = M.hq_of(M.my_side.opposite)

    S = GameState()
    opp = M.my_side.opposite
    for sfx in range(1, START_WARRIORS + 1):
        S.warriors.append(Warrior(WarriorId(M.my_side, sfx), M.my_hq, HQ_LEVELS[1].warrior_hp))
        S.warriors.append(Warrior(WarriorId(opp, sfx), M.opp_hq, HQ_LEVELS[1].warrior_hp))
    S.buildings.append(
        Building(0, Side.LEFT, BType.HQ, 1, HQ_LEVELS[1].hp)
    )
    S.buildings.append(
        Building(M.N - 1, Side.RIGHT, BType.HQ, 1, HQ_LEVELS[1].hp)
    )

    print("OK", flush=True)
    return M, S


def read_turn_start() -> int | None:
    line = readln()
    if line == "FINISH":
        return None
    t = line.split()
    assert t and t[0] == "START"
    return int(t[2])


def read_turn_result(S: GameState, M: GameMap, submitted: Actions) -> None:
    for region in submitted.upgrades:
        b = S.find_building(region)
        if b is None:
            S.gold -= BASE_LEVELS[1].cost
        else:
            max_level = HQ_MAX_LEVEL if b.type is BType.HQ else BASE_MAX_LEVEL
            if b.level >= max_level:
                cost = HQ_HEAL_COST if b.type is BType.HQ else BASE_HEAL_COST
                S.gold -= cost
            else:
                S.gold -= b.upgrade_cost()

    built = set(submitted.upgrades)

    for wid, target in submitted.moves:
        b = S.find_building(target)
        own = target in built or (b is not None and b.side is M.my_side)
        cost = 0 if own else MOVE_COST
        S.gold -= cost
        w = S.find_warrior(wid)
        if w is not None:
            w.state = WState.MOVING
            w.target = target

    S.gold -= TRAIN_COST * submitted.train_n

    line = readln()
    if line == "FINISH":
        sys.exit(0)
    t = line.split()
    assert t and t[0] == "TURN"

    t = read_tokens()
    S.my_countdown = int(t[2])
    S.opp_countdown = int(t[4])

    # UPGRADE
    t = read_tokens()  # "UPGRADE N"
    n = int(t[1])
    for _ in range(n):
        r = read_tokens()  # "<A|B> <region>"
        region = int(r[1])
        b = S.find_building(region)
        if b is None:
            S.buildings.append(make_base(region, M.my_side))
        else:
            max_level = HQ_MAX_LEVEL if b.type is BType.HQ else BASE_MAX_LEVEL
            if b.level >= max_level:
                b.hp = b.current_hp()
            else:
                b.apply_upgrade()

    # TRAIN
    t = read_tokens()  # "TRAIN N"
    n = int(t[1])
    if n > 0:
        ids = read_tokens()
        for i in range(n):
            wid = WarriorId.parse(ids[i])
            hq_b = S.find_building(M.my_hq)
            hq_level = hq_b.level if hq_b is not None else 1
            S.warriors.append(Warrior(wid, M.my_hq, HQ_LEVELS[hq_level].warrior_hp))

    # MOVE
    t = read_tokens()  # "MOVE N"
    n = int(t[1])
    for _ in range(n):
        r = read_tokens()
        wid = WarriorId.parse(r[0])
        region = int(r[1])
        w = S.find_warrior(wid)
        if w is not None:
            w.region = region
            if w.state is WState.MOVING and w.region == w.target:
                w.state = WState.STATIONARY

    # DAMAGE
    t = read_tokens()  # "DAMAGE N"
    n = int(t[1])
    starved: list[tuple[WarriorId, int]] = []
    for _ in range(n):
        r = read_tokens()  # "<cause> <id> <damage>"
        wid = WarriorId.parse(r[1])
        damage = int(r[2])
        if r[0] == "HUNGER":
            starved.append((wid, damage))
            continue
        w = S.find_warrior(wid)
        if w is not None:
            w.hp -= damage
    S.warriors = [w for w in S.warriors if w.hp > 0]

    # SIEGE
    t = read_tokens()  # "SIEGE N"
    n = int(t[1])
    for _ in range(n):
        r = read_tokens()
        region = int(r[1])
        dmg = int(r[2])
        b = S.find_building(region)
        if b is not None:
            b.hp -= dmg
    S.buildings = [b for b in S.buildings if b.hp > 0]

    S.visible = compute_visible(S, M)

    # WARRIOR
    t = read_tokens()  # "WARRIOR W"
    w_count = int(t[1])
    seen_warriors: list[Warrior] = []
    for _ in range(w_count):
        r = read_tokens()  # "<id> <region> <hp>"
        wid = WarriorId.parse(r[0])
        if wid.side is M.my_side:
            continue
        seen_warriors.append(Warrior(wid, int(r[1]), int(r[2])))
    S.warriors = [w for w in S.warriors if w.id.side is M.my_side] + seen_warriors

    # BUILDING
    t = read_tokens()  # "BUILDING B"
    b_count = int(t[1])
    seen_buildings: list[Building] = []
    for _ in range(b_count):
        r = read_tokens()  # "<side> <region> <kind> <level> <hp>"
        side = Side.from_char(r[0])
        if side is M.my_side:
            continue
        btype = BType.HQ if r[2] == "HQ" else BType.BASE
        seen_buildings.append(Building(int(r[1]), side, btype, int(r[3]), int(r[4])))
    S.buildings = [b for b in S.buildings if b.side is M.my_side] + seen_buildings

    readln()  # "END"

    income = 0
    for b in S.buildings:
        if b.side is not M.my_side:
            continue
        count = sum(
            1 for w in S.warriors
            if w.id.side is M.my_side and w.region == b.region
        )
        income += WORK_INCOME * min(count, b.work_cap())
    S.gold += income

    alive = sum(1 for w in S.warriors if w.id.side is M.my_side)
    S.gold -= UPKEEP_PER_WARRIOR * min(alive, S.gold // UPKEEP_PER_WARRIOR)

    for wid, damage in starved:
        w = S.find_warrior(wid)
        if w is not None:
            w.hp -= damage
    S.warriors = [w for w in S.warriors if w.hp > 0]


def euclid_ceil(M: GameMap, u: int, v: int) -> float:
    return math.ceil(math.hypot(M.x[u] - M.x[v], M.y[u] - M.y[v]))


class Paths:
    def __init__(self, M: GameMap) -> None:
        self.M = M
        self._dist_to: dict[int, list[float]] = {}
        self._nxt_to: dict[int, list[int]] = {}

    def toward(self, v: int) -> tuple[list[float], list[int]]:
        if v not in self._dist_to:
            M = self.M
            INF = math.inf
            dist = [INF] * M.N
            dist[v] = 0.0
            pq = [(0.0, v)]
            while pq:
                d, u = heapq.heappop(pq)
                if d > dist[u]:
                    continue
                for nb in M.adj[u]:
                    cand = d + euclid_ceil(M, u, nb)
                    if cand < dist[nb]:
                        dist[nb] = cand
                        heapq.heappush(pq, (cand, nb))
            nxt = [-1] * M.N
            nxt[v] = v
            for u in range(M.N):
                if u == v or dist[u] == INF:
                    continue
                best_score = INF
                for nb in M.adj[u]:
                    if dist[nb] == INF:
                        continue
                    score = euclid_ceil(M, u, nb) + dist[nb]
                    if score < best_score:
                        best_score = score
                        nxt[u] = nb
            self._dist_to[v] = dist
            self._nxt_to[v] = nxt
        return self._dist_to[v], self._nxt_to[v]

    def dist(self, u: int, v: int) -> float:
        return self.toward(v)[0][u]


def calculate_paths(M: GameMap) -> Paths:
    return Paths(M)


def next_step(P: Paths, u: int, v: int) -> int:
    return P.toward(v)[1][u]


def path(P: Paths, u: int, v: int) -> list[int]:
    if next_step(P, u, v) == -1:
        return []
    out = [u]
    while u != v:
        u = next_step(P, u, v)
        out.append(u)
    return out


def emit(a: Actions) -> None:
    out: list[str] = ["COMMAND"]
    for wid, target in a.moves:
        out.append(f"MOVE {wid} {target}")
    for r in a.upgrades:
        out.append(f"UPGRADE {r}")
    if a.train_n > 0:
        out.append(f"TRAIN {a.train_n}")
    out.append("END")
    sys.stdout.write("\n".join(out) + "\n")
    sys.stdout.flush()


def decide(S: GameState, M: GameMap, P: Paths, turn: int) -> Actions:
    """Write your strategy here."""
    a = Actions()
    if turn == 1:
        for w in S.warriors:
            if w.id.side is M.my_side:
                a.moves.append((w.id, M.opp_hq))
    return a


def main() -> None:
    M, S = parse_init()
    P = calculate_paths(M)

    while (turn := read_turn_start()) is not None:
        a = decide(S, M, P, turn)
        emit(a)
        read_turn_result(S, M, a)


if __name__ == "__main__":
    main()
