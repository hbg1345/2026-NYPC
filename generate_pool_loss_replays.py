#!/usr/bin/env python3
"""Generate replay HTML and candidate debug logs for every fixed-pool loss."""

from __future__ import annotations

import argparse
import contextlib
import csv
import hashlib
import html
import io
import json
import os
import random
import re
import shutil
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import judge


def map_digest(path: Path) -> str:
    data = path.read_bytes()
    marker = b"END MAP"
    end = data.find(marker)
    if end < 0:
        raise ValueError(f"END MAP missing: {path}")
    return hashlib.sha256(data[: end + len(marker)]).hexdigest()


def unique_maps(root: Path) -> list[Path]:
    by_digest: dict[str, Path] = {}
    for path in sorted(root.rglob("*.log"), key=lambda item: str(item)):
        by_digest.setdefault(map_digest(path), path)
    return list(by_digest.values())


def play_one(candidate: str, opponent: str, map_path: str) -> tuple[str, str]:
    try:
        game_map = judge.load_map_from_file(map_path)
        winner, reason, *_ = judge.run_one_game(
            candidate, opponent, 0, record=False, map_override=game_map
        )
        if winner is None:
            return "D", reason or ""
        return ("W" if winner == "A" else "L"), reason or ""
    except Exception as exc:
        judge.Agent.close_all()
        return "E", repr(exc)


def map_labels(path: Path) -> tuple[str, str, str]:
    text = str(path)
    pretest_match = re.search(r"Pretest #(\d+)", text)
    battle_match = re.search(r"battle-(\d+)", text)
    map_match = re.search(r"c-1-(\d+)-vs-\d+\.log$", text)
    return (
        pretest_match.group(1) if pretest_match else "unknown",
        map_match.group(1) if map_match else path.stem,
        battle_match.group(1) if battle_match else "unknown",
    )


def write_replay_file(
    template: str,
    output_path: Path,
    candidate: str,
    opponent: str,
    winner: str | None,
    reason: str,
    game_map: dict,
    frames: list,
) -> None:
    replay = {
        "map": {
            "N": game_map["N"],
            "K": game_map["K"],
            "x": game_map["x"],
            "y": game_map["y"],
            "adj": game_map["adj"],
            "strongholds": game_map["strongholds"],
            "polys": game_map["polys"],
        },
        "frames": frames,
        "result": {
            "winner": winner,
            "reason": reason,
            "seed": 0,
            "nameA": Path(candidate).stem,
            "nameB": Path(opponent).stem,
        },
    }
    replay_json = json.dumps(replay, ensure_ascii=False, separators=(",", ":"))
    output_path.write_text(
        template.replace("__REPLAY_JSON__", replay_json), encoding="utf-8"
    )


def record_loss(
    candidate: str,
    opponent: str,
    model: str,
    map_path: str,
    html_path: str,
    debug_path: str,
    template_path: str,
    work_root: str,
) -> dict[str, object]:
    destination = Path(html_path)
    debug_destination = Path(debug_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    work_dir = Path(tempfile.mkdtemp(prefix=f"{model}_", dir=work_root))
    old_cwd = Path.cwd()
    captured = io.StringIO()
    try:
        os.chdir(work_dir)
        with contextlib.redirect_stdout(captured), contextlib.redirect_stderr(captured):
            game_map = judge.load_map_from_file(map_path)
            winner, reason, _seed, recorded_map, _rows, frames, _ = (
                judge.run_one_game(
                    candidate,
                    opponent,
                    0,
                    record=True,
                    map_override=game_map,
                )
            )
        if winner != "B":
            raise RuntimeError(
                f"expected candidate loss but got winner={winner!r}: {reason}"
            )
        template = Path(template_path).read_text(encoding="utf-8")
        write_replay_file(
            template,
            destination,
            candidate,
            opponent,
            winner,
            reason or "",
            recorded_map,
            frames,
        )
        debug_source = work_dir / "debug_A.txt"
        if debug_source.exists():
            shutil.copyfile(debug_source, debug_destination)
        else:
            debug_destination.write_text(
                "candidate did not emit debug_A.txt\n" + captured.getvalue(),
                encoding="utf-8",
            )
        pretest, map_id, battle = map_labels(Path(map_path))
        return {
            "model": model,
            "pretest": pretest,
            "map_id": map_id,
            "battle": battle,
            "map": map_path,
            "reason": reason or "",
            "replay": str(destination),
            "debug": str(debug_destination),
            "html_bytes": destination.stat().st_size,
            "debug_bytes": debug_destination.stat().st_size,
        }
    finally:
        os.chdir(old_cwd)
        judge.Agent.close_all()
        shutil.rmtree(work_dir, ignore_errors=True)


def write_index(output_dir: Path, records: list[dict[str, object]]) -> None:
    grouped: dict[str, list[dict[str, object]]] = {}
    for record in records:
        grouped.setdefault(str(record["model"]), []).append(record)
    lines = [
        "<!doctype html>",
        '<html lang="ko"><head><meta charset="utf-8">',
        "<title>110584 최고판 pool 패배 리플레이</title>",
        "<style>",
        "body{font:15px/1.5 system-ui,sans-serif;max-width:1100px;margin:32px auto;padding:0 20px}",
        "table{border-collapse:collapse;width:100%;margin:8px 0 28px}",
        "th,td{border:1px solid #ccc;padding:6px 8px;text-align:left}",
        "th{background:#f3f3f3}code{font-size:13px}",
        "</style></head><body>",
        f"<h1>110584 최고판 pool 패배 리플레이 ({len(records)}판)</h1>",
        "<p>공식 고유 맵 100개(seed 42), 후보 LEFT 기준. 각 판의 HTML과 후보 디버그 로그를 함께 제공한다.</p>",
    ]
    for model in sorted(grouped, key=int):
        items = sorted(
            grouped[model],
            key=lambda row: (
                int(str(row["pretest"])) if str(row["pretest"]).isdigit() else 999,
                int(str(row["map_id"])) if str(row["map_id"]).isdigit() else 999999,
            ),
        )
        lines.append(f"<h2>vs {html.escape(model)} ({len(items)}패)</h2>")
        lines.append("<table><thead><tr><th>#</th><th>맵</th><th>종료 사유</th><th>파일</th></tr></thead><tbody>")
        for number, item in enumerate(items, 1):
            replay_path = Path(str(item["replay"]))
            debug_path = Path(str(item["debug"]))
            replay_rel = replay_path.relative_to(output_dir).as_posix()
            debug_rel = debug_path.relative_to(output_dir).as_posix()
            map_label = (
                f"Pretest #{item['pretest']} / {item['map_id']} "
                f"(battle-{item['battle']})"
            )
            lines.append(
                "<tr>"
                f"<td>{number}</td><td>{html.escape(map_label)}</td>"
                f"<td>{html.escape(str(item['reason']))}</td>"
                f'<td><a href="{html.escape(replay_rel)}">replay</a> · '
                f'<a href="{html.escape(debug_rel)}">debug</a></td>'
                "</tr>"
            )
        lines.append("</tbody></table>")
    lines.append("</body></html>")
    (output_dir / "index.html").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--pool-dir", type=Path, default=Path("pool"))
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument(
        "--logs-root", type=Path, default=Path("summission_result/tournament")
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--games", type=int, default=100)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--workers", type=int, default=16)
    parser.add_argument(
        "--record-limit",
        type=int,
        default=0,
        help=(
            "record at most this many losses after verifying the full loss set; "
            "selection is round-robin across opponent models (0 records all)"
        ),
    )
    args = parser.parse_args()

    candidate = args.candidate.resolve()
    pool_dir = args.pool_dir.resolve()
    logs_root = args.logs_root.resolve()
    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise SystemExit(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    work_root = output_dir / ".work"
    work_root.mkdir(exist_ok=True)

    with args.summary.open(newline="", encoding="utf-8-sig") as stream:
        summary_rows = list(csv.DictReader(stream))
    expected_losses = {row["model"]: int(row["losses"]) for row in summary_rows}
    opponents = {
        model: (pool_dir / f"{model}.exe").resolve() for model in expected_losses
    }
    missing = [str(path) for path in opponents.values() if not path.exists()]
    if missing:
        raise SystemExit("missing pool executable(s): " + ", ".join(missing))

    maps = unique_maps(logs_root)
    if args.games > len(maps):
        raise SystemExit(f"requested {args.games} maps, only {len(maps)} available")
    chosen = random.Random(args.seed).sample(maps, args.games)
    tasks = [
        (model, str(opponent), str(map_path.resolve()))
        for model, opponent in opponents.items()
        for map_path in chosen
    ]
    print(
        f"[scan] models={len(opponents)} maps={len(chosen)} games={len(tasks)} "
        f"workers={args.workers}",
        flush=True,
    )
    started = time.monotonic()
    results: list[dict[str, str]] = []
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(play_one, str(candidate), opponent, map_path): (
                model,
                map_path,
            )
            for model, opponent, map_path in tasks
        }
        for done, future in enumerate(as_completed(futures), 1):
            model, map_path = futures[future]
            outcome, reason = future.result()
            results.append(
                {
                    "model": model,
                    "map": map_path,
                    "outcome": outcome,
                    "reason": reason,
                }
            )
            if done % 100 == 0 or done == len(futures):
                print(
                    f"[scan] {done}/{len(futures)} elapsed={time.monotonic()-started:.1f}s",
                    flush=True,
                )

    errors = [row for row in results if row["outcome"] == "E"]
    if errors:
        raise SystemExit(f"scan produced {len(errors)} errors; first={errors[0]}")
    losses = [row for row in results if row["outcome"] == "L"]
    actual_losses = {
        model: sum(row["model"] == model for row in losses)
        for model in expected_losses
    }
    mismatches = {
        model: (expected_losses[model], actual_losses[model])
        for model in expected_losses
        if expected_losses[model] != actual_losses[model]
    }
    if mismatches:
        raise SystemExit(f"loss-count mismatch against source summary: {mismatches}")
    print(f"[scan] verified all {len(losses)} losses against summary", flush=True)

    losses_to_record = sorted(
        losses, key=lambda row: (int(row["model"]), row["map"])
    )
    if args.record_limit > 0 and len(losses_to_record) > args.record_limit:
        grouped_losses: dict[str, list[dict[str, str]]] = {}
        for loss in losses_to_record:
            grouped_losses.setdefault(loss["model"], []).append(loss)
        losses_to_record = []
        depth = 0
        ordered_models = sorted(grouped_losses, key=int)
        while len(losses_to_record) < args.record_limit:
            added = False
            for model in ordered_models:
                items = grouped_losses[model]
                if depth >= len(items):
                    continue
                losses_to_record.append(items[depth])
                added = True
                if len(losses_to_record) >= args.record_limit:
                    break
            if not added:
                break
            depth += 1
        print(
            f"[record] sampled={len(losses_to_record)}/{len(losses)} "
            "round-robin by model",
            flush=True,
        )

    record_tasks = []
    per_model_index: dict[str, int] = {}
    for loss in losses_to_record:
        model = loss["model"]
        per_model_index[model] = per_model_index.get(model, 0) + 1
        pretest, map_id, battle = map_labels(Path(loss["map"]))
        stem = (
            f"loss_{per_model_index[model]:02d}_pretest{pretest}_"
            f"map{map_id}_battle{battle}"
        )
        model_dir = output_dir / f"vs_{model}"
        record_tasks.append(
            (
                str(candidate),
                str(opponents[model]),
                model,
                loss["map"],
                str(model_dir / f"{stem}.html"),
                str(model_dir / f"{stem}.debug.txt"),
                str((Path(__file__).resolve().parent / "viewer_template.html")),
                str(work_root),
            )
        )

    records: list[dict[str, object]] = []
    print(f"[record] losses={len(record_tasks)} workers={args.workers}", flush=True)
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(record_loss, *task): task for task in record_tasks}
        for done, future in enumerate(as_completed(futures), 1):
            records.append(future.result())
            if done % 10 == 0 or done == len(futures):
                print(f"[record] {done}/{len(futures)}", flush=True)

    records.sort(key=lambda row: (int(str(row["model"])), str(row["map"])))
    manifest_path = output_dir / "loss_manifest.csv"
    with manifest_path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0].keys()))
        writer.writeheader()
        writer.writerows(records)
    write_index(output_dir, records)
    shutil.rmtree(work_root, ignore_errors=True)
    total_html = sum(int(row["html_bytes"]) for row in records)
    total_debug = sum(int(row["debug_bytes"]) for row in records)
    print(
        f"[done] replays={len(records)} html={total_html / 1048576:.1f}MiB "
        f"debug={total_debug / 1048576:.1f}MiB output={output_dir}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
