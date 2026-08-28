#!/usr/bin/env python3
"""Inspect the T46 R27 -> R49 direct-attack counterfactual."""

from __future__ import annotations

import json
from pathlib import Path

import judge


REPLAY = Path(
    "first_mandatory_first_attack_outcome_replays_6/"
    "failure_vs_83284_pretest28_map1558_battle233427/replay.html"
)
START_TURN = 46
SOURCE = 27
TARGET = 49


def load_data() -> dict:
    text = REPLAY.read_text(encoding="utf-8")
    start = text.index("const DATA = ") + len("const DATA = ")
    end = text.index("const M = DATA.map;", start)
    payload = text[start:end].strip()
    if payload.endswith(";"):
        payload = payload[:-1]
    return json.loads(payload)


def restore(game_map: dict, frame: dict) -> judge.Engine:
    eng = judge.Engine(game_map)
    eng.gold = {side: int(frame["gold"][side]) for side in ("A", "B")}
    eng.buildings = {
        int(raw["r"]): judge.Building(
            int(raw["r"]), raw["side"], raw["type"],
            int(raw["level"]), int(raw["hp"])
        )
        for raw in frame["buildings"]
    }
    eng.warriors = [
        judge.Warrior(
            side=raw["side"], num=int(raw["id"][1:]),
            region=int(raw["r"]), hp=int(raw["hp"]),
            state=raw["state"], target=int(raw["target"]), prev_region=-1,
        )
        for raw in frame["warrior_units"]
    ]
    eng.next_num = {
        side: max(w.num for w in eng.warriors if w.side == side) + 1
        for side in ("A", "B")
    }
    return eng


def path(eng: judge.Engine, source: int, target: int) -> list[int]:
    out = [source]
    while out[-1] != target and len(out) <= eng.N:
        out.append(eng.next_hop(out[-1], target))
    return out


def closest_reserve(eng: judge.Engine) -> tuple[int, list[int], list[int]]:
    best: tuple[int, float, int, list[int], list[int]] | None = None
    for building in eng.buildings.values():
        if building.side != "B" or building.region == TARGET:
            continue
        units = [
            w.num for w in eng.warriors
            if w.side == "B" and w.region == building.region
            and w.state == "STATIONARY"
        ]
        if not units:
            continue
        route = path(eng, building.region, TARGET)
        candidate = (len(route) - 1, eng.dist[building.region][TARGET],
                     building.region, units, route)
        if best is None or candidate[:2] < best[:2]:
            best = candidate
    assert best is not None
    return best[2], best[3], best[4]


def describe_target(eng: judge.Engine) -> str:
    building = eng.buildings.get(TARGET)
    building_text = "none" if building is None else (
        f"{building.side}-{building.btype}-L{building.level}-hp{building.hp}"
    )
    units = sorted(
        (w.side, w.num, w.hp) for w in eng.warriors if w.region == TARGET
    )
    return f"building={building_text} units={units}"


def simulate(base: judge.Engine, reserve_turn: int | None, reserve_ids: list[int]) -> None:
    eng = restore(base.M, {
        "gold": base.gold,
        "buildings": [
            {"r": b.region, "side": b.side, "type": b.btype,
             "level": b.level, "hp": b.hp}
            for b in base.buildings.values()
        ],
        "warrior_units": [
            {"id": w.id, "side": w.side, "r": w.region, "hp": w.hp,
             "state": w.state, "target": w.target}
            for w in base.warriors
        ],
    })
    attackers = sorted(
        w.num for w in eng.warriors
        if w.side == "A" and w.region == SOURCE and w.state == "STATIONARY"
    )
    print(f"\nscenario reserve_turn={reserve_turn} reserve_ids={reserve_ids}")
    for turn in range(START_TURN, 60):
        if turn == START_TURN:
            eng.apply_move_orders("A", [(num, TARGET) for num in attackers])
        if reserve_turn is not None and turn == reserve_turn:
            eng.apply_move_orders("B", [(num, TARGET) for num in reserve_ids])
        eng.step_moves()
        eng.resolve_combat()
        a_alive = [
            (w.num, w.region, w.hp, w.state)
            for w in eng.warriors if w.side == "A" and w.num in attackers
        ]
        reserve_alive = [
            (w.num, w.region, w.hp, w.state)
            for w in eng.warriors if w.side == "B" and w.num in reserve_ids
        ]
        print(
            f"T{turn} {describe_target(eng)} "
            f"attackers={a_alive} reserve={reserve_alive}"
        )
        building = eng.buildings.get(TARGET)
        if building is None or not a_alive:
            break


def main() -> None:
    data = load_data()
    snapshot = data["frames"][START_TURN - 1]
    eng = restore(data["map"], snapshot)
    attackers = sorted(
        (w.id, w.hp) for w in eng.warriors
        if w.side == "A" and w.region == SOURCE and w.state == "STATIONARY"
    )
    reserve_region, reserve_ids, reserve_route = closest_reserve(eng)
    print(f"snapshot=start T{START_TURN} gold={eng.gold}")
    print(f"attackers_at_R{SOURCE}={attackers}")
    print(f"attack_route={path(eng, SOURCE, TARGET)}")
    print(f"target={describe_target(eng)}")
    print(
        f"closest_reserve=R{reserve_region} ids={reserve_ids} "
        f"route={reserve_route}"
    )
    simulate(eng, None, [])
    simulate(eng, START_TURN + 1, reserve_ids)


if __name__ == "__main__":
    main()
