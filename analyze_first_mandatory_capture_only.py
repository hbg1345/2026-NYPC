#!/usr/bin/env python3
"""Analyze the first mandatory-reclaim trigger with a capture-only counterfactual.

For every match, the real game is recorded just long enough to restore the state
at the beginning of the first ``RECLAIM_MANDATORY_TRIGGER`` turn.  From that
snapshot, both sides are then controlled by the same deterministic policy:

* never attack an enemy-owned region and never send defensive reinforcements;
* preserve only movements already aimed at an unbuilt stronghold;
* build level-1 bases, send one builder per neutral stronghold, and train only
  builders needed for still-available neutral strongholds;
* avoid a target when the other side has an earlier estimated completion turn;
  equal estimates are left contested/unclaimed.

The real first reclaim operation is independently classified as first-wave
SUCCESS, FAILURE, NO_LAUNCH, or UNRESOLVED from the candidate debug log.
"""

from __future__ import annotations

import argparse
import contextlib
import csv
import io
import json
import math
import os
import random
import re
import shutil
import tempfile
import time
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import Any

import judge
from evaluate_all_models import select_unique_maps


LINE_RE = re.compile(r"^T(\d+)\s+(.+)$")
TRIGGER_RE = re.compile(r"RECLAIM_MANDATORY_TRIGGER target=R(\d+)")
LAUNCH_RE = re.compile(
    r"RECLAIM_WAVE launched=(\d+) target=R(\d+) mode=MANDATORY"
)
LOSS_RE = re.compile(
    r"RECLAIM_MANDATORY_WAVE_LOST target=R(\d+) engaged=([01]) retry=(\d+)"
)
RESOLVED_RE = re.compile(
    r"RECLAIM_MANDATORY_RESOLVED target=R(\d+) engaged=([01]) failed_waves=(\d+)"
)
RELEASE_RE = re.compile(r"RECLAIM_MANDATORY_RELEASE target=R(\d+)")


def map_labels(path: Path) -> tuple[str, str, str]:
    text = str(path)
    pretest = re.search(r"Pretest #(\d+)", text)
    battle = re.search(r"battle-(\d+)", text)
    map_id = re.search(r"tc-1-(\d+)-vs-\d+\.log$", text)
    return (
        pretest.group(1) if pretest else "unknown",
        map_id.group(1) if map_id else path.stem,
        battle.group(1) if battle else "unknown",
    )


def debug_events(debug: str) -> list[tuple[int, str]]:
    events: list[tuple[int, str]] = []
    for raw in debug.splitlines():
        match = LINE_RE.match(raw.strip())
        if match:
            events.append((int(match.group(1)), match.group(2)))
    return events


def classify_first_operation(debug: str) -> dict[str, Any] | None:
    events = debug_events(debug)
    trigger_index = -1
    trigger_turn = -1
    target = -1
    for i, (turn, message) in enumerate(events):
        match = TRIGGER_RE.search(message)
        if match:
            trigger_index = i
            trigger_turn = turn
            target = int(match.group(1))
            break
    if trigger_index < 0:
        return None

    launched = False
    result: dict[str, Any] = {
        "trigger_turn": trigger_turn,
        "target": target,
        "launch_turn": "",
        "launch_count": "",
        "result_turn": "",
        "first_attack_result": "UNRESOLVED_NO_LAUNCH",
        "engaged": "",
        "failed_waves": "",
    }
    for turn, message in events[trigger_index + 1 :]:
        launch = LAUNCH_RE.search(message)
        if launch and int(launch.group(2)) == target and not launched:
            launched = True
            result["launch_turn"] = turn
            result["launch_count"] = int(launch.group(1))
            result["first_attack_result"] = "UNRESOLVED_AFTER_LAUNCH"
            continue

        lost = LOSS_RE.search(message)
        if launched and lost and int(lost.group(1)) == target:
            result["result_turn"] = turn
            result["first_attack_result"] = "FAILURE"
            result["engaged"] = int(lost.group(2))
            result["failed_waves"] = int(lost.group(3))
            return result

        resolved = RESOLVED_RE.search(message)
        if resolved and int(resolved.group(1)) == target:
            result["result_turn"] = turn
            result["engaged"] = int(resolved.group(2))
            result["failed_waves"] = int(resolved.group(3))
            result["first_attack_result"] = (
                "SUCCESS" if launched else "NO_LAUNCH_RESOLVED"
            )
            return result

        release = RELEASE_RE.search(message)
        if release and int(release.group(1)) == target:
            result["result_turn"] = turn
            result["first_attack_result"] = (
                "FAILURE_RELEASED" if launched else "NO_LAUNCH_RELEASED"
            )
            return result

        next_trigger = TRIGGER_RE.search(message)
        if next_trigger and not launched:
            result["result_turn"] = turn
            result["first_attack_result"] = "NO_LAUNCH_REPLACED"
            return result
    return result


def restore_engine(game_map: dict[str, Any], frame: dict[str, Any] | None) -> judge.Engine:
    eng = judge.Engine(game_map)
    if frame is None:
        return eng
    eng.gold = {side: int(frame["gold"][side]) for side in ("A", "B")}
    eng.buildings = {
        int(raw["r"]): judge.Building(
            int(raw["r"]),
            str(raw["side"]),
            str(raw["type"]),
            int(raw["level"]),
            int(raw["hp"]),
        )
        for raw in frame["buildings"]
    }
    eng.warriors = []
    greatest = {"A": 0, "B": 0}
    for raw in frame["warrior_units"]:
        side = str(raw["side"])
        wid = str(raw["id"])
        num = int(wid[1:])
        greatest[side] = max(greatest[side], num)
        eng.warriors.append(
            judge.Warrior(
                side=side,
                num=num,
                region=int(raw["r"]),
                hp=int(raw["hp"]),
                state=str(raw["state"]),
                target=int(raw["target"]),
                prev_region=-1,
            )
        )
    eng.next_num = {side: greatest[side] + 1 for side in ("A", "B")}
    return eng


def hop_count(eng: judge.Engine, source: int, target: int) -> int:
    if source == target:
        return 0
    current = source
    for steps in range(1, eng.N + 1):
        current = eng.next_hop(current, target)
        if current is None or current < 0:
            return 10**6
        if current == target:
            return steps
    return 10**6


def base_counts(eng: judge.Engine) -> dict[str, int]:
    return {
        side: sum(
            b.side == side and b.btype == "BASE" for b in eng.buildings.values()
        )
        for side in ("A", "B")
    }


def current_net_income(eng: judge.Engine, side: str) -> int:
    gross = 0
    for building in eng.buildings.values():
        if building.side != side:
            continue
        workers = len(eng.warriors_at(building.region, side))
        gross += judge.WORK_INCOME * min(workers, building.work_cap())
    upkeep = judge.UPKEEP_PER_WARRIOR * sum(w.side == side for w in eng.warriors)
    return gross - upkeep


def gold_wait(gold: int, cost: int, net: int) -> int:
    if gold >= cost:
        return 0
    if net <= 0:
        return 10**6
    return math.ceil((cost - gold) / net)


def available_workers(eng: judge.Engine, side: str) -> list[judge.Warrior]:
    """Idle workers that can leave without reducing current staffed work caps."""
    result: list[judge.Warrior] = []
    by_region: dict[int, list[judge.Warrior]] = defaultdict(list)
    for warrior in eng.warriors:
        if warrior.side == side and warrior.state == "STATIONARY":
            by_region[warrior.region].append(warrior)
    for region, workers in by_region.items():
        workers.sort(key=lambda w: w.num)
        enemy_here = any(
            w.side != side and w.region == region for w in eng.warriors
        )
        if enemy_here:
            continue
        building = eng.buildings.get(region)
        if building is None:
            if region not in eng.strongholds:
                result.extend(workers)
            # A stationary unit on a neutral stronghold is its reserved builder.
            continue
        if building.side != side:
            continue
        result.extend(workers[min(len(workers), building.work_cap()) :])
    return result


def active_builder_targets(eng: judge.Engine, side: str) -> set[int]:
    targets: set[int] = set()
    for warrior in eng.warriors:
        if warrior.side != side:
            continue
        if warrior.state == "MOVING" and warrior.target in eng.strongholds:
            if warrior.target not in eng.buildings:
                targets.add(warrior.target)
        elif (
            warrior.state == "STATIONARY"
            and warrior.region in eng.strongholds
            and warrior.region not in eng.buildings
        ):
            targets.add(warrior.region)
    return targets


def completion_eta(
    eng: judge.Engine,
    side: str,
    target: int,
    workers: list[judge.Warrior],
) -> tuple[int, int | None]:
    """Estimated turns until a base can be built and the chosen worker id."""
    net = current_net_income(eng, side)
    gold = eng.gold[side]
    candidates: list[tuple[int, int]] = []

    for warrior in eng.warriors:
        if warrior.side != side:
            continue
        if warrior.state == "MOVING" and warrior.target == target:
            travel = hop_count(eng, warrior.region, target)
            eta = max(travel + 1, gold_wait(gold, 300, net))
            candidates.append((eta, warrior.num))
        elif (
            warrior.state == "STATIONARY"
            and warrior.region == target
            and target not in eng.buildings
        ):
            candidates.append((gold_wait(gold, 300, net), warrior.num))

    for warrior in workers:
        travel = hop_count(eng, warrior.region, target)
        eta = max(travel + 1, gold_wait(gold, judge.MOVE_COST + 300, net))
        candidates.append((eta, warrior.num))

    if candidates:
        return min(candidates)

    # No spare worker: estimate waiting for one HQ-trained builder.  Training
    # happens after movement, so the new unit can begin moving the next day.
    wait_train = gold_wait(gold, judge.TRAIN_COST, net)
    if wait_train >= 10**6:
        return 10**6, None
    remaining_gold = gold + wait_train * net - judge.TRAIN_COST
    travel = hop_count(eng, eng.hq_region(side), target)
    after_train = gold_wait(remaining_gold, judge.MOVE_COST + 300, net)
    eta = max(wait_train + travel + 2, wait_train + after_train)
    return eta, None


def normalize_capture_only_state(eng: judge.Engine) -> None:
    """Cancel inherited movements that are not attempts to claim neutral bases."""
    seen: set[tuple[str, int]] = set()
    for warrior in sorted(eng.warriors, key=lambda w: (w.side, w.num)):
        keep = (
            warrior.state == "MOVING"
            and warrior.target in eng.strongholds
            and warrior.target not in eng.buildings
            and (warrior.side, warrior.target) not in seen
        )
        if keep:
            seen.add((warrior.side, warrior.target))
        elif warrior.state == "MOVING":
            warrior.state = "STATIONARY"
            warrior.target = warrior.region


def step_capture_only_moves(eng: judge.Engine) -> None:
    """Advance builders without turning incidental path crossings into combat.

    The counterfactual measures the neutral-base race, not which side wins a
    chance meeting on a shortest path.  Builders therefore pass through one
    another; only simultaneous presence on the destination prevents building.
    """
    for warrior in eng.warriors:
        if warrior.state != "MOVING":
            continue
        if warrior.region == warrior.target:
            warrior.state = "STATIONARY"
            continue
        next_region = eng.next_hop(warrior.region, warrior.target)
        if next_region is None or next_region < 0:
            continue
        warrior.prev_region = warrior.region
        warrior.region = next_region
        if warrior.region == warrior.target:
            warrior.state = "STATIONARY"


def simulate_capture_only(
    game_map: dict[str, Any],
    frame: dict[str, Any] | None,
    trigger_turn: int,
) -> dict[str, Any]:
    eng = restore_engine(game_map, frame)
    normalize_capture_only_state(eng)
    start = base_counts(eng)
    built = {"A": [], "B": []}

    for turn in range(trigger_turn, judge.MAX_TURN):
        neutral = sorted(r for r in eng.strongholds if r not in eng.buildings)
        if not neutral:
            break

        # BUILD: only uncontested neutral strongholds, no upgrades or repairs.
        for side in ("A", "B"):
            buildable = []
            for target in neutral:
                mine = eng.warriors_at(target, side)
                enemy = eng.warriors_at(target, judge.OTHER[side])
                if mine and not enemy:
                    buildable.append(target)
            buildable.sort(key=lambda r: (hop_count(eng, eng.hq_region(side), r), r))
            for target in buildable:
                if target in eng.buildings or eng.gold[side] < 300:
                    continue
                if eng.apply_build(side, [target]):
                    built[side].append((turn, target))

        # Any inherited/pending builder whose target was taken stops; it does
        # not turn into an attack in this counterfactual.
        for warrior in eng.warriors:
            if warrior.state == "MOVING" and warrior.target in eng.buildings:
                warrior.state = "STATIONARY"
                warrior.target = warrior.region

        neutral = sorted(r for r in eng.strongholds if r not in eng.buildings)
        if not neutral:
            eng.resolve_income()
            eng.resolve_upkeep()
            break

        workers = {side: available_workers(eng, side) for side in ("A", "B")}
        active = {side: active_builder_targets(eng, side) for side in ("A", "B")}
        eta: dict[str, dict[int, tuple[int, int | None]]] = {
            side: {
                target: completion_eta(eng, side, target, workers[side])
                for target in neutral
            }
            for side in ("A", "B")
        }

        assigned: dict[str, list[int]] = {"A": [], "B": []}
        for target in neutral:
            a_eta = eta["A"][target][0]
            b_eta = eta["B"][target][0]
            if a_eta < b_eta:
                assigned["A"].append(target)
            elif b_eta < a_eta:
                assigned["B"].append(target)
            # Equal ETA: neither side spends resources on a no-combat stalemate.

        for side in ("A", "B"):
            assigned[side].sort(
                key=lambda target: (eta[side][target][0], target)
            )
            unused = {worker.num: worker for worker in workers[side]}
            orders: list[tuple[int, int]] = []
            for target in assigned[side]:
                if target in active[side]:
                    continue
                choices = sorted(
                    (
                        hop_count(eng, worker.region, target),
                        worker.num,
                        worker,
                    )
                    for worker in unused.values()
                )
                if not choices:
                    continue
                _distance, wid, worker = choices[0]
                if eng.gold[side] < judge.MOVE_COST:
                    break
                orders.append((wid, target))
                del unused[wid]
                active[side].add(target)
            eng.apply_move_orders(side, orders)

        step_capture_only_moves(eng)

        # TRAIN: create builders only for targets provisionally won by this
        # side but not yet covered.  Do not consume 300 gold already needed by
        # a builder waiting on a neutral stronghold.
        for side in ("A", "B"):
            active_now = active_builder_targets(eng, side)
            uncovered = [target for target in assigned[side] if target not in active_now]
            if not uncovered:
                continue
            waiting = sum(
                warrior.side == side
                and warrior.state == "STATIONARY"
                and warrior.region in eng.strongholds
                and warrior.region not in eng.buildings
                for warrior in eng.warriors
            )
            spendable = max(0, eng.gold[side] - 300 * waiting)
            hq = eng.buildings.get(eng.hq_region(side))
            cap = judge.HQ_LEVELS[hq.level]["train_cap"] if hq else 0
            count = min(len(uncovered), cap, spendable // judge.TRAIN_COST)
            if count:
                eng.apply_train(side, count)

        # Deliberately no resolve_combat(): attack and defense are disabled.
        eng.resolve_income()
        eng.resolve_upkeep()

    final = base_counts(eng)
    relation = "AHEAD" if final["A"] > final["B"] else (
        "BEHIND" if final["A"] < final["B"] else "TIED"
    )
    unclaimed = sum(r not in eng.buildings for r in eng.strongholds)
    return {
        "cf_start_bases_a": start["A"],
        "cf_start_bases_b": start["B"],
        "cf_final_bases_a": final["A"],
        "cf_final_bases_b": final["B"],
        "cf_relation": relation,
        "cf_unclaimed": unclaimed,
        "cf_built_a": " ".join(f"T{turn}:R{region}" for turn, region in built["A"]),
        "cf_built_b": " ".join(f"T{turn}:R{region}" for turn, region in built["B"]),
    }


def outcome_code(winner: str | None) -> str:
    return "D" if winner is None else ("W" if winner == "A" else "L")


def analyze_one(task: tuple[str, str, str, str]) -> dict[str, Any]:
    candidate, opponent, model, map_path_s = task
    map_path = Path(map_path_s)
    old_cwd = Path.cwd()
    captured = io.StringIO()
    temp_dir = Path(tempfile.mkdtemp(prefix=f"first_reclaim_{model}_"))
    try:
        os.chdir(temp_dir)
        with contextlib.redirect_stdout(captured), contextlib.redirect_stderr(captured):
            game_map = judge.load_map_from_file(map_path)
            winner, reason, _seed, recorded_map, _rows, frames, _center = (
                judge.run_one_game(
                    candidate, opponent, 0, record=True, map_override=game_map
                )
            )
        debug_path = temp_dir / "debug_A.txt"
        if not debug_path.exists():
            raise RuntimeError("candidate did not emit debug_A.txt")
        debug = debug_path.read_text(encoding="utf-8", errors="replace")
        operation = classify_first_operation(debug)
        pretest, map_id, battle = map_labels(map_path)
        row: dict[str, Any] = {
            "model": model,
            "pretest": pretest,
            "map_id": map_id,
            "battle": battle,
            "map": str(map_path),
            "outcome": outcome_code(winner),
            "reason": reason or "",
            "triggered": int(operation is not None),
        }
        if operation is None:
            row.update(
                {
                    "trigger_turn": "",
                    "target": "",
                    "launch_turn": "",
                    "launch_count": "",
                    "result_turn": "",
                    "first_attack_result": "NO_TRIGGER",
                    "engaged": "",
                    "failed_waves": "",
                    "cf_start_bases_a": "",
                    "cf_start_bases_b": "",
                    "cf_final_bases_a": "",
                    "cf_final_bases_b": "",
                    "cf_relation": "NO_TRIGGER",
                    "cf_unclaimed": "",
                    "cf_built_a": "",
                    "cf_built_b": "",
                }
            )
            return row

        row.update(operation)
        trigger_turn = int(operation["trigger_turn"])
        snapshot = None if trigger_turn == 0 else frames[trigger_turn - 1]
        row.update(simulate_capture_only(recorded_map, snapshot, trigger_turn))
        return row
    finally:
        os.chdir(old_cwd)
        judge.Agent.close_all()
        shutil.rmtree(temp_dir, ignore_errors=True)


def cohort(rows: list[dict[str, Any]]) -> dict[str, Any]:
    outcomes = Counter(str(row["outcome"]) for row in rows)
    games = len(rows)
    return {
        "games": games,
        "outcomes": {key: outcomes.get(key, 0) for key in ("W", "D", "L")},
        "score_rate": (
            (outcomes.get("W", 0) + 0.5 * outcomes.get("D", 0)) / games
            if games
            else None
        ),
    }


def summarize(rows: list[dict[str, Any]]) -> dict[str, Any]:
    triggered = [row for row in rows if int(row["triggered"])]
    relations = Counter(str(row["cf_relation"]) for row in triggered)
    results = Counter(str(row["first_attack_result"]) for row in triggered)

    cross: dict[str, dict[str, int]] = {}
    for relation in ("BEHIND", "TIED", "AHEAD"):
        group = [row for row in triggered if row["cf_relation"] == relation]
        counts = Counter(str(row["first_attack_result"]) for row in group)
        cross[relation] = dict(sorted(counts.items()))

    operation_cohorts = {
        result: cohort(
            [row for row in triggered if row["first_attack_result"] == result]
        )
        for result in sorted(results)
    }
    relation_cohorts = {
        relation: cohort(
            [row for row in triggered if row["cf_relation"] == relation]
        )
        for relation in ("BEHIND", "TIED", "AHEAD")
    }

    per_opponent: dict[str, Any] = {}
    for model in sorted({str(row["model"]) for row in rows}, key=int):
        group = [row for row in rows if str(row["model"]) == model]
        model_triggered = [row for row in group if int(row["triggered"])]
        per_opponent[model] = {
            "all": cohort(group),
            "triggered_games": len(model_triggered),
            "capture_only_relations": dict(
                Counter(str(row["cf_relation"]) for row in model_triggered)
            ),
            "first_attack_results": dict(
                Counter(str(row["first_attack_result"]) for row in model_triggered)
            ),
        }

    launched = [
        row
        for row in triggered
        if row["first_attack_result"]
        in ("SUCCESS", "FAILURE", "FAILURE_RELEASED", "UNRESOLVED_AFTER_LAUNCH")
    ]
    decided = [
        row
        for row in launched
        if row["first_attack_result"] in ("SUCCESS", "FAILURE", "FAILURE_RELEASED")
    ]
    successes = sum(row["first_attack_result"] == "SUCCESS" for row in decided)
    failures = len(decided) - successes
    return {
        "definition": {
            "snapshot": (
                "state at the start of the first RECLAIM_MANDATORY_TRIGGER turn"
            ),
            "counterfactual": (
                "both sides use the same deterministic capture-only policy; "
                "no attacks, defensive moves, combat, upgrades, or repairs"
            ),
            "success": (
                "the first mandatory wave for the first triggered target is "
                "followed by RECLAIM_MANDATORY_RESOLVED before WAVE_LOST"
            ),
            "failure": (
                "the first mandatory operation for the first triggered target "
                "is followed by WAVE_LOST, or its lock is released after launch"
            ),
        },
        "all_games": cohort(rows),
        "triggered_games": cohort(triggered),
        "capture_only_relation_counts": {
            key: relations.get(key, 0) for key in ("BEHIND", "TIED", "AHEAD")
        },
        "capture_only_behind_rate_among_triggered": (
            relations.get("BEHIND", 0) / len(triggered) if triggered else None
        ),
        "first_attack_result_counts": dict(sorted(results.items())),
        "first_attack_decided": len(decided),
        "first_attack_successes": successes,
        "first_attack_failures": failures,
        "first_attack_success_rate": successes / len(decided) if decided else None,
        "capture_relation_by_first_attack_result": cross,
        "match_outcome_by_capture_relation": relation_cohorts,
        "match_outcome_by_first_attack_result": operation_cohorts,
        "per_opponent": per_opponent,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--pool", type=Path, default=Path("pool"))
    parser.add_argument(
        "--maps-root", type=Path, default=Path("summission_result/tournament")
    )
    parser.add_argument("--maps", type=int, default=100)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--workers", type=int, default=16)
    parser.add_argument(
        "--output-prefix",
        type=Path,
        default=Path("first_mandatory_capture_only_highest_pool_100maps"),
    )
    args = parser.parse_args()

    candidate = args.candidate.resolve()
    opponents = sorted(args.pool.resolve().glob("*.exe"), key=lambda p: int(p.stem))
    maps = random.Random(args.seed).sample(
        select_unique_maps(args.maps_root.resolve()), args.maps
    )
    tasks = [
        (str(candidate), str(opponent), opponent.stem, str(map_path.resolve()))
        for opponent in opponents
        for map_path in maps
    ]
    print(
        f"[first-reclaim-capture-only] opponents={len(opponents)} "
        f"maps={len(maps)} games={len(tasks)} workers={args.workers}",
        flush=True,
    )
    started = time.monotonic()
    last_report = started
    rows: list[dict[str, Any]] = []
    with ProcessPoolExecutor(max_workers=args.workers) as executor:
        futures = [executor.submit(analyze_one, task) for task in tasks]
        for done, future in enumerate(as_completed(futures), 1):
            rows.append(future.result())
            now = time.monotonic()
            if done == len(futures) or now - last_report >= 20:
                rate = done / max(0.001, now - started)
                eta = (len(futures) - done) / max(0.001, rate)
                print(
                    f"[first-reclaim-capture-only] {done}/{len(futures)} "
                    f"({done / len(futures):.1%}) rate={rate:.1f}/s eta={eta:.0f}s",
                    flush=True,
                )
                last_report = now

    rows.sort(key=lambda row: (int(str(row["model"])), str(row["map"])))
    csv_path = args.output_prefix.with_suffix(".csv").resolve()
    json_path = args.output_prefix.with_suffix(".json").resolve()
    with csv_path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    summary = summarize(rows)
    json_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)
    print(f"[done] csv={csv_path} json={json_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
