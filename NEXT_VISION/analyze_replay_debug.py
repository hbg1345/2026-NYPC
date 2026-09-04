#!/usr/bin/env python3
"""Explain movement decisions and hazardous routes in a NEXT VISION log."""

from __future__ import annotations

import argparse
import heapq
import math
from pathlib import Path

from build_sample_replays import HQ, START_WARRIORS, parse_log


INF = 10**30


def distances_to(game_map: dict, target: int) -> list[int]:
    dist = [INF] * game_map["N"]
    dist[target] = 0
    queue = [(0, target)]
    while queue:
        value, region = heapq.heappop(queue)
        if value != dist[region]:
            continue
        for neighbor in game_map["adj"][region]:
            weight = math.ceil(math.hypot(
                game_map["x"][region] - game_map["x"][neighbor],
                game_map["y"][region] - game_map["y"][neighbor],
            ))
            candidate = value + weight
            if candidate < dist[neighbor]:
                dist[neighbor] = candidate
                heapq.heappush(queue, (candidate, neighbor))
    return dist


def shortest_route(game_map: dict, source: int, target: int) -> list[int]:
    if source == target:
        return [source]
    dist = distances_to(game_map, target)
    route = [source]
    for _ in range(game_map["N"]):
        region = route[-1]
        best = None
        for neighbor in sorted(game_map["adj"][region]):
            weight = math.ceil(math.hypot(
                game_map["x"][region] - game_map["x"][neighbor],
                game_map["y"][region] - game_map["y"][neighbor],
            ))
            score = weight + dist[neighbor]
            if best is None or score < best[0]:
                best = (score, neighbor)
        if best is None or best[0] != dist[region]:
            return route
        route.append(best[1])
        if route[-1] == target:
            return route
    return route


def initial_snapshot(game_map: dict) -> dict:
    n = game_map["N"]
    warriors = []
    for side, region in (("A", 0), ("B", n - 1)):
        for num in range(1, START_WARRIORS + 1):
            warriors.append({
                "id": f"{side}{num}", "side": side, "r": region,
                "hp": HQ[1]["warrior_hp"], "state": "STATIONARY",
                "target": region,
            })
    return {
        "t": 0,
        "buildings": [
            {"r": 0, "side": "A", "type": "HQ", "level": 1, "hp": HQ[1]["hp"]},
            {"r": n - 1, "side": "B", "type": "HQ", "level": 1, "hp": HQ[1]["hp"]},
        ],
        "warrior_units": warriors,
    }


def unit_at(frame: dict, warrior_id: str) -> dict | None:
    return next((unit for unit in frame["warrior_units"]
                 if unit["id"] == warrior_id), None)


def route_hazards(frame: dict, side: str, route: list[int]) -> list[str]:
    enemy = "B" if side == "A" else "A"
    hazards = []
    for region in route[1:]:
        building = next((item for item in frame["buildings"]
                         if item["r"] == region and item["side"] == enemy), None)
        enemies = [unit for unit in frame["warrior_units"]
                   if unit["r"] == region and unit["side"] == enemy]
        details = []
        if building is not None:
            details.append(
                f"{enemy}-{building['type']}-L{building['level']}"
            )
        if enemies:
            details.append("units=" + ",".join(unit["id"] for unit in enemies))
        if details:
            hazards.append(f"R{region}({'; '.join(details)})")
    return hazards


def analyze(path: Path, selected: set[str], all_moves: bool) -> int:
    replay = parse_log(path)
    game_map = replay["map"]
    previous = initial_snapshot(game_map)
    findings = 0

    print(f"replay={path} N={game_map['N']} turns={len(replay['frames'])}")
    for frame in replay["frames"]:
        turn = frame["t"]
        commands = []
        for side in ("A", "B"):
            commands.extend((side, item) for item in frame["orders"][side]["move"])

        for side, command in commands:
            warrior_id = command["id"]
            if not all_moves and warrior_id not in selected:
                continue
            unit = unit_at(previous, warrior_id)
            if unit is None:
                print(f"T{turn:03d} {warrior_id} command target=R{command['to']} (unit unavailable)")
                continue
            route = shortest_route(game_map, unit["r"], command["to"])
            hazards = route_hazards(previous, side, route)
            actual = next((move for move in frame["moved"]
                           if move["id"] == warrior_id), None)
            actual_text = "blocked" if actual is None else f"R{actual['src']}->R{actual['to']}"
            print(
                f"T{turn:03d} COMMAND {warrior_id} R{unit['r']} -> R{command['to']} "
                f"actual={actual_text}\n"
                f"      route=" + " -> ".join(f"R{region}" for region in route)
            )
            if hazards:
                findings += 1
                print("      HAZARD=" + ", ".join(hazards))

        for warrior_id in sorted(selected):
            if any(item["id"] == warrior_id for _, item in commands):
                continue
            move = next((item for item in frame["moved"]
                         if item["id"] == warrior_id), None)
            damage = [item for item in frame["damaged"] if item["id"] == warrior_id]
            if move is None and not damage:
                continue
            before = unit_at(previous, warrior_id)
            target = before["target"] if before is not None else -1
            movement = "no move" if move is None else f"R{move['src']}->R{move['to']}"
            suffix = ""
            if damage:
                suffix = " damage=" + ",".join(
                    f"{item['cause']}:{item['dmg']}" for item in damage)
            print(f"T{turn:03d} CONTINUE {warrior_id} toward=R{target} {movement}{suffix}")

        previous = frame

    print(f"hazardous_commands={findings}")
    return 1 if findings else 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=Path)
    parser.add_argument(
        "--unit", action="append",
        help="warrior ID to trace; repeatable (default: A4 and B4)",
    )
    parser.add_argument("--all-moves", action="store_true")
    args = parser.parse_args()
    selected = set(args.unit or ("A4", "B4"))
    raise SystemExit(analyze(args.replay, selected, args.all_moves))


if __name__ == "__main__":
    main()
