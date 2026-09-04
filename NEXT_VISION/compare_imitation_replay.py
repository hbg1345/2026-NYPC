#!/usr/bin/env python3
"""Run the imitation AI against a recorded fixed opponent and compare orders."""

from __future__ import annotations

import argparse
import collections
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from build_sample_replays import parse_log


def write_map(path: Path, game_map: dict) -> None:
    rows = [
        f"{game_map['N']} {game_map['K']}",
        " ".join(map(str, game_map["x"])),
        " ".join(map(str, game_map["y"])),
        " ".join(map(str, game_map["strongholds"])),
    ]
    rows.extend(
        f"{len(neighbors)} " + " ".join(map(str, neighbors))
        for neighbors in game_map["adj"]
    )
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")


def normalized_moves(orders: dict) -> list[tuple[str, int]]:
    return sorted((move["id"], move["to"]) for move in orders["move"])


def target_multiset(orders: dict) -> collections.Counter[int]:
    return collections.Counter(move["to"] for move in orders["move"])


def successful_upgrades(replay: dict, side: str) -> list[tuple[int, int]]:
    return [
        (frame["t"], item["r"])
        for frame in replay["frames"]
        for item in frame["upgraded"]
        if item["side"] == side
    ]


def command_attacks(replay: dict, side: str) -> list[tuple[int, int, int]]:
    buildings_by_turn = {
        frame["t"]: {
            building["r"]: building
            for building in frame["buildings"]
        }
        for frame in replay["frames"]
    }
    enemy = "B" if side == "A" else "A"
    attacks = []
    for frame in replay["frames"]:
        counts: collections.Counter[int] = collections.Counter()
        for move in frame["orders"][side]["move"]:
            building = buildings_by_turn[frame["t"]].get(move["to"])
            if building is not None and building["side"] == enemy:
                counts[move["to"]] += 1
        attacks.extend((frame["t"], target, count)
                       for target, count in sorted(counts.items()))
    return attacks


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    root = Path(__file__).resolve().parent
    original = parse_log(args.replay.resolve())
    output = args.output or Path(tempfile.gettempdir()) / "imitation_fixed_left.log"

    with tempfile.TemporaryDirectory(prefix="next-vision-compare-") as temp:
        map_path = Path(temp) / "map.txt"
        write_map(map_path, original["map"])
        fixed_command = " ".join([
            shlex.quote(sys.executable),
            shlex.quote(str(root / "fixed_replay_bot.py")),
            shlex.quote(str(args.replay.resolve())),
            "LEFT",
        ])
        command = [
            sys.executable,
            str(root / "nation-fr-providing" / "testing-tool.py"),
            "-i", str(map_path),
            "-l", str(output),
            "-a", fixed_command,
            "-b", str(args.binary.resolve()),
        ]
        completed = subprocess.run(command, check=False)
        if completed.returncode != 0:
            raise SystemExit(completed.returncode)

    generated = parse_log(output)
    original_frames = original["frames"]
    generated_frames = generated["frames"]
    common = min(len(original_frames), len(generated_frames))

    exact = upgrades = trains = moves = targets = 0
    first_difference = None
    for index in range(common):
        expected = original_frames[index]["orders"]["B"]
        actual = generated_frames[index]["orders"]["B"]
        exact_now = expected == actual
        exact += exact_now
        upgrades += sorted(expected["upgrade"]) == sorted(actual["upgrade"])
        trains += expected["train"] == actual["train"]
        moves += normalized_moves(expected) == normalized_moves(actual)
        targets += target_multiset(expected) == target_multiset(actual)
        if first_difference is None and not exact_now:
            first_difference = (
                original_frames[index]["t"], expected, actual
            )

    def rate(value: int) -> str:
        return "n/a" if common == 0 else f"{value}/{common} ({value / common:.1%})"

    print(f"original={args.replay}")
    print(f"generated={output}")
    print(f"turns original={len(original_frames)} generated={len(generated_frames)}")
    print(f"result original={original['result']} generated={generated['result']}")
    print(f"exact_orders={rate(exact)}")
    print(f"upgrade_orders={rate(upgrades)}")
    print(f"train_orders={rate(trains)}")
    print(f"move_id_and_target_orders={rate(moves)}")
    print(f"move_target_multisets={rate(targets)}")
    if first_difference is not None:
        turn, expected, actual = first_difference
        print(f"first_difference=T{turn}")
        print(f"  expected={expected}")
        print(f"  actual={actual}")

    print("successful_upgrades_original_B=", successful_upgrades(original, "B"))
    print("successful_upgrades_generated_B=", successful_upgrades(generated, "B"))
    print("attacks_original_B=", command_attacks(original, "B"))
    print("attacks_generated_B=", command_attacks(generated, "B"))


if __name__ == "__main__":
    main()
