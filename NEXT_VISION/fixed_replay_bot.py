#!/usr/bin/env python3
"""Replay one side's recorded commands for controlled opponent experiments."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from build_sample_replays import parse_log


START_GOLD = 750
TRAIN_COST = 120
MOVE_COST = 10
WORK_INCOME = 15
UPKEEP = 2
HQ_WORK_CAP = (0, 1, 2, 3, 4, 5)
HQ_TRAIN_CAP = (0, 1, 1, 2, 2, 3)
HQ_UPGRADE_COST = (0, 0, 600, 1000, 2000, 3000)
BASE_WORK_CAP = (0, 1, 2, 3)
BASE_UPGRADE_COST = (0, 500, 550, 600)


class LiveState:
    def __init__(self, side: str, region_count: int) -> None:
        self.side = side
        self.letter = "A" if side == "LEFT" else "B"
        self.hq_region = 0 if side == "LEFT" else region_count - 1
        self.region_count = region_count
        self.gold = START_GOLD
        self.warriors = {
            f"{self.letter}{number}": {
                "region": self.hq_region,
                "hp": 4,
                "target": None,
            }
            for number in range(1, 4)
        }
        self.buildings = {
            self.hq_region: {
                "side": self.letter,
                "kind": "HQ",
                "level": 1,
                "hp": 10,
            }
        }
        self.visible_enemy_regions: set[int] = set()

    def upgrade_cost(self, region: int) -> int | None:
        building = self.buildings.get(region)
        if building is None:
            return BASE_UPGRADE_COST[1]
        if building["side"] != self.letter:
            return None
        level = building["level"]
        if building["kind"] == "HQ":
            return 1000 if level >= 5 else HQ_UPGRADE_COST[level + 1]
        return 500 if level >= 3 else BASE_UPGRADE_COST[level + 1]

    def choose_orders(self, recorded: dict) -> dict:
        chosen = {"move": [], "upgrade": [], "train": 0}
        budget = self.gold
        own_regions = {warrior["region"] for warrior in self.warriors.values()}

        for region in recorded["upgrade"]:
            cost = self.upgrade_cost(region)
            if (cost is None or region not in own_regions or
                    region in self.visible_enemy_regions or budget < cost):
                continue
            chosen["upgrade"].append(region)
            budget -= cost

        upgraded_regions = set(chosen["upgrade"])
        for move in recorded["move"]:
            warrior = self.warriors.get(move["id"])
            target = move["to"]
            if (warrior is None or warrior["target"] is not None or
                    target < 0 or target >= self.region_count):
                continue
            building = self.buildings.get(target)
            own_target = (
                target in upgraded_regions or
                (building is not None and building["side"] == self.letter)
            )
            cost = 0 if own_target else MOVE_COST
            if budget < cost:
                continue
            chosen["move"].append(move)
            budget -= cost

        hq = self.buildings.get(self.hq_region)
        requested = recorded["train"]
        if hq is not None and hq["side"] == self.letter:
            cap = HQ_TRAIN_CAP[hq["level"]]
            chosen["train"] = min(requested, cap, budget // TRAIN_COST)
            budget -= chosen["train"] * TRAIN_COST

        self.gold = budget
        for move in chosen["move"]:
            self.warriors[move["id"]]["target"] = move["to"]
        return chosen

    def apply_result(self, sections: dict[str, list[list[str]]]) -> None:
        for row in sections["UPGRADE"]:
            owner, raw_region = row
            region = int(raw_region)
            if owner != self.letter:
                continue
            building = self.buildings.get(region)
            if building is None:
                self.buildings[region] = {
                    "side": owner, "kind": "BASE", "level": 1, "hp": 6
                }
            else:
                maximum = 5 if building["kind"] == "HQ" else 3
                building["level"] = min(maximum, building["level"] + 1)

        for row in sections["TRAIN"]:
            for warrior_id in row:
                if warrior_id.startswith(self.letter):
                    self.warriors[warrior_id] = {
                        "region": self.hq_region,
                        "hp": 0,
                        "target": None,
                    }

        for warrior_id, raw_region in sections["MOVE"]:
            warrior = self.warriors.get(warrior_id)
            if warrior is None:
                continue
            warrior["region"] = int(raw_region)
            if warrior["region"] == warrior["target"]:
                warrior["target"] = None

        snapshot = {}
        visible_enemy_regions = set()
        for warrior_id, raw_region, raw_hp in sections["WARRIOR"]:
            region = int(raw_region)
            if warrior_id.startswith(self.letter):
                old = self.warriors.get(warrior_id, {"target": None})
                snapshot[warrior_id] = {
                    "region": region,
                    "hp": int(raw_hp),
                    "target": old["target"],
                }
            else:
                visible_enemy_regions.add(region)
        self.warriors = snapshot
        self.visible_enemy_regions = visible_enemy_regions

        own_buildings = {}
        for owner, raw_region, kind, raw_level, raw_hp in sections["BUILDING"]:
            if owner != self.letter:
                continue
            region = int(raw_region)
            own_buildings[region] = {
                "side": owner,
                "kind": kind,
                "level": int(raw_level),
                "hp": int(raw_hp),
            }
        self.buildings = own_buildings

        income = 0
        for region, building in self.buildings.items():
            workers = sum(
                warrior["region"] == region
                for warrior in self.warriors.values()
            )
            table = HQ_WORK_CAP if building["kind"] == "HQ" else BASE_WORK_CAP
            income += WORK_INCOME * min(workers, table[building["level"]])
        self.gold += income
        self.gold -= UPKEEP * min(len(self.warriors), self.gold // UPKEEP)


def read_result(first_line: str) -> dict[str, list[list[str]]]:
    del first_line
    time_line = sys.stdin.readline()
    if not time_line:
        raise EOFError
    sections: dict[str, list[list[str]]] = {}
    for name in ("UPGRADE", "TRAIN", "MOVE", "DAMAGE", "SIEGE",
                 "WARRIOR", "BUILDING"):
        header = sys.stdin.readline().split()
        if len(header) != 2 or header[0] != name:
            raise RuntimeError(f"expected {name}, got {' '.join(header)}")
        count = int(header[1])
        if name == "TRAIN":
            sections[name] = [sys.stdin.readline().split()] if count else []
        else:
            sections[name] = [sys.stdin.readline().split() for _ in range(count)]
    if sys.stdin.readline().strip() != "END":
        raise RuntimeError("missing result END")
    return sections


def emit_orders(orders: dict) -> None:
    print("COMMAND")
    for move in orders["move"]:
        print(f"MOVE {move['id']} {move['to']}")
    for region in orders["upgrade"]:
        print(f"UPGRADE {region}")
    if orders["train"] > 0:
        print(f"TRAIN {orders['train']}")
    print("END", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=Path)
    parser.add_argument("side", choices=("LEFT", "RIGHT"))
    args = parser.parse_args()

    replay = parse_log(args.replay)
    side = "A" if args.side == "LEFT" else "B"
    orders_by_turn = {
        frame["t"]: frame["orders"][side] for frame in replay["frames"]
    }

    ready = sys.stdin.readline().strip()
    if not ready.startswith("READY "):
        return
    size = sys.stdin.readline().split()
    if len(size) < 2:
        return
    region_count = int(size[0])
    for _ in range(3 + region_count):
        if not sys.stdin.readline():
            return
    print("OK", flush=True)

    state = LiveState(args.side, region_count)

    while True:
        raw = sys.stdin.readline()
        if not raw:
            return
        line = raw.strip()
        if line == "FINISH":
            return
        if line.startswith("TURN "):
            state.apply_result(read_result(line))
            continue
        if not line.startswith("START TURN "):
            continue
        turn = int(line.split()[2])
        recorded = orders_by_turn.get(
            turn, {"move": [], "upgrade": [], "train": 0}
        )
        emit_orders(state.choose_orders(recorded))


if __name__ == "__main__":
    main()
