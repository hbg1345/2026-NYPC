#!/usr/bin/env python3
"""Convert official NEXT VISION sample AI text logs into replay HTML files."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


START_GOLD = 750
START_WARRIORS = 3
MOVE_COST = 10
TRAIN_COST = 120
WORK_INCOME = 15
UPKEEP = 2
MAX_TURN = 400

HQ = {
    1: dict(upgrade=0, warrior_hp=4, hp=10, work=1),
    2: dict(upgrade=600, warrior_hp=5, hp=15, work=2),
    3: dict(upgrade=1000, warrior_hp=6, hp=20, work=3),
    4: dict(upgrade=2000, warrior_hp=7, hp=25, work=4),
    5: dict(upgrade=3000, warrior_hp=8, hp=30, work=5),
}
BASE = {
    1: dict(cost=500, hp=6, work=1),
    2: dict(cost=550, hp=12, work=2),
    3: dict(cost=600, hp=18, work=3),
}


def _ints(line: str) -> list[int]:
    return [int(value) for value in line.split()]


def parse_map(lines: list[str]) -> tuple[dict, int]:
    map_line = lines.index("MAP")
    pos = map_line + 1
    n, k = _ints(lines[pos])[:2]
    xs = _ints(lines[pos + 1])
    ys = _ints(lines[pos + 2])
    strong_tokens = lines[pos + 3].split()
    if strong_tokens[0] == "STRONGHOLDS":
        strong_tokens = strong_tokens[1:]
    strongholds = [int(value) for value in strong_tokens]
    pos += 4
    adj = []
    for _ in range(n):
        row = _ints(lines[pos])
        adj.append(row[1:1 + row[0]])
        pos += 1
    if lines[pos] != "END MAP":
        raise ValueError(f"expected END MAP, got {lines[pos]!r}")
    if len(xs) != n or len(ys) != n or len(strongholds) != k:
        raise ValueError("invalid MAP block sizes")
    return dict(
        N=n,
        K=k,
        x=xs,
        y=ys,
        strongholds=strongholds,
        adj=adj,
    ), pos + 1


def empty_orders() -> dict:
    return {
        "A": {"move": [], "upgrade": [], "train": 0},
        "B": {"move": [], "upgrade": [], "train": 0},
    }


def parse_log(path: Path) -> dict:
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    game_map, pos = parse_map(lines)
    n = game_map["N"]

    side_name = {"LEFT": "A", "RIGHT": "B"}
    orders_by_turn = []
    results_by_turn = []

    while pos < len(lines) and re.fullmatch(r"TURN \d+", lines[pos]):
        turn = int(lines[pos].split()[1])
        pos += 1
        orders = empty_orders()
        while pos < len(lines) and lines[pos] != f"TURN {turn} RESULT":
            match = re.fullmatch(r"COMMAND (LEFT|RIGHT) START", lines[pos])
            if not match:
                raise ValueError(f"unexpected command line: {lines[pos]!r}")
            label = match.group(1)
            side = side_name[label]
            pos += 1
            end_marker = f"COMMAND {label} END"
            while lines[pos] != end_marker:
                token = lines[pos].split()
                if token[0] == "MOVE":
                    orders[side]["move"].append({"id": token[1], "to": int(token[2])})
                elif token[0] == "UPGRADE":
                    orders[side]["upgrade"].append(int(token[1]))
                elif token[0] == "TRAIN":
                    orders[side]["train"] = int(token[1])
                else:
                    raise ValueError(f"unknown command: {lines[pos]!r}")
                pos += 1
            pos += 1

        if pos >= len(lines):
            raise ValueError(f"missing TURN {turn} RESULT")
        pos += 1
        result = dict(upgraded=[], trained=[], moved=[], damaged=[], sieged=[])
        while pos < len(lines) and lines[pos] != f"END TURN {turn}":
            token = lines[pos].split()
            if token[0] == "TIME":
                pass
            elif token[0] == "UPGRADE":
                result["upgraded"].append({"side": token[1], "r": int(token[2])})
            elif token[0] == "TRAIN":
                for warrior_id in token[1:]:
                    result["trained"].append({"id": warrior_id, "side": warrior_id[0]})
            elif token[0] == "MOVE":
                result["moved"].append({"id": token[1], "side": token[1][0], "to": int(token[2])})
            elif token[0] == "DAMAGE":
                result["damaged"].append({
                    "cause": token[1], "id": token[2], "side": token[2][0], "dmg": int(token[3])
                })
            elif token[0] == "SIEGE":
                result["sieged"].append({"side": token[1], "r": int(token[2]), "dmg": int(token[3])})
            else:
                raise ValueError(f"unknown result line: {lines[pos]!r}")
            pos += 1
        if pos >= len(lines):
            raise ValueError(f"missing END TURN {turn}")
        pos += 1
        orders_by_turn.append((turn, orders))
        results_by_turn.append(result)

    winner = None
    reason = "로그 종료"
    if pos < len(lines) and lines[pos].startswith("RESULT "):
        token = lines[pos].split()
        if token[1] == "LEFT_WIN":
            winner = "A"
        elif token[1] == "RIGHT_WIN":
            winner = "B"
        reason = " ".join(token[2:]).replace("_", " ") or token[1].replace("_", " ")

    gold = {"A": START_GOLD, "B": START_GOLD}
    buildings = {
        0: dict(r=0, side="A", type="HQ", level=1, hp=HQ[1]["hp"]),
        n - 1: dict(r=n - 1, side="B", type="HQ", level=1, hp=HQ[1]["hp"]),
    }
    warriors = {}
    for side, region in (("A", 0), ("B", n - 1)):
        for num in range(1, START_WARRIORS + 1):
            warrior_id = f"{side}{num}"
            warriors[warrior_id] = dict(
                id=warrior_id, side=side, r=region, hp=HQ[1]["warrior_hp"],
                state="STATIONARY", target=region,
            )

    # Per-player fog-of-war estimator.  Public result events update known
    # production, damage, and spending; end-of-turn vision snapshots correct
    # remembered regional troop counts, including a confirmed zero.
    intel = {}
    for observer, enemy, enemy_hq in (("A", "B", n - 1), ("B", "A", 0)):
        enemy_units = {}
        for num in range(1, START_WARRIORS + 1):
            warrior_id = f"{enemy}{num}"
            enemy_units[warrior_id] = dict(
                id=warrior_id, r=enemy_hq, hp=HQ[1]["warrior_hp"],
                last_seen=-1,
            )
        region_count = [0] * n
        region_turn = [-1] * n
        region_count[enemy_hq] = START_WARRIORS
        region_turn[enemy_hq] = 0
        intel[observer] = dict(
            enemy=enemy,
            gold_upper=START_GOLD,
            units=enemy_units,
            region_count=region_count,
            region_turn=region_turn,
        )

    def observer_for(enemy_side: str) -> str:
        return "B" if enemy_side == "A" else "A"

    def visible_regions(observer: str) -> set[int]:
        visible: set[int] = set()
        frontier = [building["r"] for building in buildings.values()
                    if building["side"] == observer]
        frontier += [unit["r"] for unit in warriors.values()
                     if unit["side"] == observer]
        for region in frontier:
            visible.add(region)
        for _ in range(2):
            next_frontier = []
            for region in frontier:
                for neighbor in game_map["adj"][region]:
                    if neighbor in visible:
                        continue
                    visible.add(neighbor)
                    next_frontier.append(neighbor)
            frontier = next_frontier
        return visible

    def upgrade_cost(building: dict | None) -> int:
        if building is None:
            return BASE[1]["cost"]
        if building["type"] == "HQ":
            return 1000 if building["level"] >= 5 else HQ[building["level"] + 1]["upgrade"]
        return 500 if building["level"] >= 3 else BASE[building["level"] + 1]["cost"]

    def apply_upgrade(side: str, region: int) -> None:
        building = buildings.get(region)
        if building is None:
            buildings[region] = dict(r=region, side=side, type="BASE", level=1, hp=BASE[1]["hp"])
            return
        max_level = 5 if building["type"] == "HQ" else 3
        if building["level"] < max_level:
            building["level"] += 1
        table = HQ if building["type"] == "HQ" else BASE
        building["hp"] = table[building["level"]]["hp"]

    frames = []
    for (turn, orders), result in zip(orders_by_turn, results_by_turn):
        hunger_sides = {damage["side"] for damage in result["damaged"]
                        if damage["cause"] == "HUNGER"}
        # Costs are paid for the actions that the official result confirms.
        for item in result["upgraded"]:
            price = upgrade_cost(buildings.get(item["r"]))
            gold[item["side"]] -= price
            state = intel[observer_for(item["side"])]
            state["gold_upper"] = max(0, state["gold_upper"] - price)

        for side in ("A", "B"):
            for move in orders[side]["move"]:
                target_building = buildings.get(move["to"])
                if target_building is None or target_building["side"] != side:
                    gold[side] -= MOVE_COST
                unit = warriors.get(move["id"])
                if unit is not None:
                    unit["state"] = "MOVING"
                    unit["target"] = move["to"]

        trained_by_side = {"A": 0, "B": 0}
        for item in result["trained"]:
            trained_by_side[item["side"]] += 1
        for side in ("A", "B"):
            gold[side] -= trained_by_side[side] * TRAIN_COST
            state = intel[observer_for(side)]
            state["gold_upper"] = max(
                0, state["gold_upper"] - trained_by_side[side] * TRAIN_COST)

        for item in result["upgraded"]:
            apply_upgrade(item["side"], item["r"])

        moved_for_frame = []
        for move in result["moved"]:
            unit = warriors.get(move["id"])
            if unit is None:
                continue
            source = unit["r"]
            unit["r"] = move["to"]
            if unit["r"] == unit["target"]:
                unit["state"] = "STATIONARY"
            moved_for_frame.append({**move, "src": source})
            state = intel[observer_for(move["side"])]
            remembered = state["units"].get(move["id"])
            if remembered is not None:
                remembered["r"] = move["to"]

        for item in result["trained"]:
            side = item["side"]
            hq_region = 0 if side == "A" else n - 1
            hq_level = buildings[hq_region]["level"]
            warriors[item["id"]] = dict(
                id=item["id"], side=side, r=hq_region,
                hp=HQ[hq_level]["warrior_hp"], state="STATIONARY", target=hq_region,
            )
            state = intel[observer_for(side)]
            state["units"][item["id"]] = dict(
                id=item["id"], r=hq_region,
                hp=HQ[hq_level]["warrior_hp"], last_seen=-1,
            )

        for damage in result["damaged"]:
            unit = warriors.get(damage["id"])
            if unit is not None:
                damage["r"] = unit["r"]
            if damage["cause"] == "HUNGER":
                continue
            if unit is not None:
                unit["hp"] -= damage["dmg"]
            state = intel[observer_for(damage["side"])]
            remembered = state["units"].get(damage["id"])
            if remembered is not None:
                remembered["hp"] -= damage["dmg"]
                if remembered["hp"] <= 0:
                    del state["units"][damage["id"]]
        warriors = {key: unit for key, unit in warriors.items() if unit["hp"] > 0}

        for siege in result["sieged"]:
            building = buildings.get(siege["r"])
            if building is not None:
                building["hp"] -= siege["dmg"]
        buildings = {region: building for region, building in buildings.items() if building["hp"] > 0}

        for side in ("A", "B"):
            for building in buildings.values():
                if building["side"] != side:
                    continue
                workers = sum(1 for unit in warriors.values()
                              if unit["side"] == side and unit["r"] == building["r"])
                table = HQ if building["type"] == "HQ" else BASE
                gold[side] += WORK_INCOME * min(workers, table[building["level"]]["work"])

        for side in ("A", "B"):
            alive = sorted(
                (unit for unit in warriors.values() if unit["side"] == side),
                key=lambda unit: int(unit["id"][1:]),
            )
            for _ in alive:
                if gold[side] >= UPKEEP:
                    gold[side] -= UPKEEP

        for damage in result["damaged"]:
            if damage["cause"] != "HUNGER":
                continue
            unit = warriors.get(damage["id"])
            if unit is not None:
                unit["hp"] -= damage["dmg"]
            state = intel[observer_for(damage["side"])]
            remembered = state["units"].get(damage["id"])
            if remembered is not None:
                remembered["hp"] -= damage["dmg"]
                if remembered["hp"] <= 0:
                    del state["units"][damage["id"]]
        warriors = {key: unit for key, unit in warriors.items() if unit["hp"] > 0}

        frame_intel = {}
        for observer in ("A", "B"):
            state = intel[observer]
            enemy = state["enemy"]
            visible = visible_regions(observer)
            visible_enemy = 0
            for region in visible:
                count = sum(1 for unit in warriors.values()
                            if unit["side"] == enemy and unit["r"] == region)
                state["region_count"][region] = count
                state["region_turn"][region] = turn
                visible_enemy += count
            for unit in warriors.values():
                if unit["side"] != enemy or unit["r"] not in visible:
                    continue
                state["units"][unit["id"]] = dict(
                    id=unit["id"], r=unit["r"], hp=unit["hp"], last_seen=turn)

            estimated_workers = 0
            oldest_age = 0
            for building in buildings.values():
                if building["side"] != enemy:
                    continue
                seen_turn = state["region_turn"][building["r"]]
                age = turn + 1 if seen_turn < 0 else turn - seen_turn
                oldest_age = max(oldest_age, age)
                possible = state["region_count"][building["r"]] + max(0, age)
                table = HQ if building["type"] == "HQ" else BASE
                estimated_workers += min(possible, table[building["level"]]["work"])

            estimated_army = len(state["units"])
            estimated_workers = min(estimated_workers, estimated_army)
            state["gold_upper"] += WORK_INCOME * estimated_workers
            state["gold_upper"] -= min(
                state["gold_upper"], UPKEEP * estimated_army)
            if enemy in hunger_sides:
                state["gold_upper"] = min(state["gold_upper"], 1)

            actual_army = sum(1 for unit in warriors.values()
                              if unit["side"] == enemy)
            estimated_regions = {}
            for unit in state["units"].values():
                estimated_regions[str(unit["r"])] = \
                    estimated_regions.get(str(unit["r"]), 0) + 1
            frame_intel[observer] = dict(
                enemy=enemy,
                gold=state["gold_upper"],
                warriors=estimated_army,
                workers=estimated_workers,
                visibleWarriors=visible_enemy,
                oldestAge=oldest_age,
                actualGold=gold[enemy],
                actualWarriors=actual_army,
                regions=estimated_regions,
            )

        warriors_by_region = {}
        for unit in warriors.values():
            counts = warriors_by_region.setdefault(str(unit["r"]), {"A": 0, "B": 0})
            counts[unit["side"]] += 1

        frames.append(dict(
            t=turn,
            gold=dict(gold),
            hq={
                "A": buildings.get(0, {}).get("hp", 0),
                "B": buildings.get(n - 1, {}).get("hp", 0),
            },
            orders=orders,
            buildings=sorted((dict(building) for building in buildings.values()), key=lambda b: b["r"]),
            warriors=warriors_by_region,
            warrior_units=sorted((dict(unit) for unit in warriors.values()), key=lambda u: (u["side"], int(u["id"][1:]))),
            upgraded=result["upgraded"],
            moved=moved_for_frame,
            trained=result["trained"],
            damaged=result["damaged"],
            sieged=result["sieged"],
            intel=frame_intel,
        ))

    return dict(
        map=game_map,
        frames=frames,
        result=dict(
            winner=winner,
            reason=reason,
            seed=path.stem,
            nameA="Sample AI · LEFT",
            nameB="Sample AI · RIGHT",
            maxTurn=MAX_TURN,
            source=path.name,
        ),
    )


def render_html(template: str, replay: dict) -> str:
    if template.count("__REPLAY_JSON__") != 1:
        raise ValueError("viewer template must contain exactly one __REPLAY_JSON__ placeholder")
    payload = json.dumps(replay, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    return template.replace("__REPLAY_JSON__", payload)


def write_index(output_dir: Path, summaries: list[dict]) -> None:
    cards = []
    for item in summaries:
        winner = "LEFT" if item["winner"] == "A" else "RIGHT" if item["winner"] == "B" else "DRAW"
        cards.append(
            f'<a class="card" href="{item["html"]}">'
            f'<strong>{item["source"]}</strong><span>{item["turns"]}턴 · {winner}</span>'
            f'<small>{item["reason"]}</small></a>'
        )
    body = "\n".join(cards)
    index = f"""<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NEXT VISION sample_ai_replay</title><style>
*{{box-sizing:border-box}}body{{margin:0;min-height:100vh;background:#171b20;color:#eef1f4;font-family:system-ui,-apple-system,sans-serif;padding:40px}}
main{{max-width:860px;margin:auto}}h1{{margin:0 0 8px}}p{{color:#9aa4b1;margin:0 0 26px}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:14px}}
.card{{display:flex;flex-direction:column;gap:8px;padding:18px;border:1px solid #343b45;border-radius:10px;background:#222831;color:inherit;text-decoration:none}}
.card:hover{{border-color:#e0b34a;transform:translateY(-1px)}}.card span{{color:#e0b34a}}.card small{{color:#9aa4b1}}
kbd{{border:1px solid #535d69;border-bottom-width:2px;border-radius:4px;padding:1px 5px;background:#2c333d}}
</style></head><body><main><h1>NEXT VISION · Sample AI Replays</h1>
<p><kbd>0</kbd> 전지 · <kbd>1</kbd> LEFT · <kbd>2</kbd> RIGHT · <kbd>Space</kbd> 홀드 추정↔실제 비교 · <kbd>P</kbd> 재생</p>
<div class="grid">{body}</div></main></body></html>"""
    (output_dir / "index.html").write_text(index, encoding="utf-8")


def main() -> None:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path,
                        help="sample replay .txt files (default: sample_ai_replay/*.txt)")
    parser.add_argument("--template", type=Path, default=here / "viewer_template.html")
    parser.add_argument("--output-dir", type=Path, default=here / "sample_ai_replay")
    args = parser.parse_args()

    inputs = args.inputs or sorted((here / "sample_ai_replay").glob("*.txt"),
                                   key=lambda path: int(path.stem) if path.stem.isdigit() else path.stem)
    if not inputs:
        raise SystemExit("no replay txt files found")
    template = args.template.read_text(encoding="utf-8")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    summaries = []
    first_html = None
    for source in inputs:
        replay = parse_log(source)
        output = args.output_dir / f"{source.stem}.html"
        html = render_html(template, replay)
        output.write_text(html, encoding="utf-8")
        if first_html is None:
            first_html = html
        summaries.append(dict(
            source=source.name,
            html=output.name,
            turns=len(replay["frames"]),
            winner=replay["result"]["winner"],
            reason=replay["result"]["reason"],
        ))
        print(f"[replay] {source.name} -> {output}")

    write_index(args.output_dir, summaries)
    if first_html is not None:
        (here / "replay.html").write_text(first_html, encoding="utf-8")
    print(f"[replay] index -> {args.output_dir / 'index.html'}")


if __name__ == "__main__":
    main()
