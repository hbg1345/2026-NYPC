#!/usr/bin/env python3
"""Evaluate one candidate against every numeric submission source on fixed maps.

Exact duplicate sources share one compiled executable and one set of matches, but
every source is still emitted as its own CSV row with a duplicate_of column.
Only standard-library modules plus the local judge.py are required.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import random
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

import judge


SOURCE_SUFFIXES = {".cpp", ".c++", ".cc", ".c"}


@dataclass(frozen=True)
class Model:
    label: str
    source: Path | None
    executable: Path
    canonical: str


def numeric_sources(models_dir: Path) -> list[Path]:
    files = [
        p
        for p in models_dir.iterdir()
        if p.is_file()
        and p.suffix.lower() in SOURCE_SUFFIXES
        and re.fullmatch(r"\d+", p.stem)
    ]
    return sorted(files, key=lambda p: int(p.stem))


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def evaluation_source_text(source: Path) -> str | None:
    """Return an evaluation-only repaired source for known broken artifacts.

    The originals are deliberately left untouched.  These three cases are
    mechanical recovery of data already present in the submission directory,
    not strategy changes.
    """
    if source.stem == "58870":
        text = source.read_text(encoding="utf-8", errors="replace")
        start = text.index("static char *readln(void) {")
        end = text.index("\n}\n", start) + 3
        portable = r'''static char *readln(void) {
  static char buf[1048576];
  if (fgets(buf, sizeof(buf), stdin) == NULL)
    exit(0);
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    buf[--len] = '\0';
  return buf;
}
'''
        return text[:start] + portable + text[end:]

    if source.stem == "59963":
        text = source.read_text(encoding="utf-8", errors="replace")
        broken = """for (const auto &w : S.warriors)\n    if (w.id.side == defender) {\n      int h = hops(w->region);"""
        repaired = """for (const auto &w : S.warriors)\n    if (w.id.side == defender) {\n      int h = hops(w.region);"""
        if broken not in text:
            raise RuntimeError("59963 repair anchor not found")
        return text.replace(broken, repaired, 1)

    if source.stem == "60103":
        # 60103 contains a complete decide() but lost the common protocol/type
        # shell.  Its immediate predecessor retains that shell, so join its
        # prefix and main() around the preserved 60103 function body.
        base_path = source.with_name("60064.cpp")
        base = base_path.read_text(encoding="utf-8", errors="replace")
        fragment = source.read_text(encoding="utf-8", errors="replace")
        decide_at = base.index("static Actions decide(")
        main_at = base.index("int main()", decide_at)
        return base[:decide_at] + fragment.rstrip() + "\n\n" + base[main_at:]

    return None


def compile_one(source: Path, output: Path) -> tuple[Path, bool, str]:
    output.parent.mkdir(parents=True, exist_ok=True)
    repaired_text = evaluation_source_text(source)
    if (
        output.exists()
        and repaired_text is None
        and output.stat().st_mtime_ns >= source.stat().st_mtime_ns
    ):
        return source, True, "cached"

    if source.suffix.lower() == ".c":
        compiler = shutil.which("gcc")
        cmd = [compiler or "gcc", "-O2", "-std=gnu11"]
        if source.stem == "58870":
            cmd.append("-D_GNU_SOURCE")
        if repaired_text is None:
            cmd.extend([str(source), "-o", str(output)])
        else:
            cmd.extend(["-x", "c", "-", "-o", str(output)])
    else:
        compiler = shutil.which("g++")
        if repaired_text is None:
            cmd = [compiler or "g++", "-O2", "-std=gnu++20", str(source), "-o", str(output)]
        else:
            cmd = [compiler or "g++", "-O2", "-std=gnu++20", "-x", "c++", "-", "-o", str(output)]

    try:
        cp = subprocess.run(
            cmd,
            input=repaired_text,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=180,
            check=False,
        )
    except Exception as exc:  # pragma: no cover - diagnostic path
        return source, False, repr(exc)
    if cp.returncode != 0:
        return source, False, cp.stdout[-4000:]
    return source, True, "compiled"


def pretest_number(path: Path) -> int | None:
    m = re.search(r"Pretest #(\d+)", str(path))
    return int(m.group(1)) if m else None


def select_screen_maps(logs_root: Path) -> list[Path]:
    groups: dict[int, list[Path]] = {}
    for p in logs_root.rglob("*.log"):
        number = pretest_number(p)
        if number is not None:
            groups.setdefault(number, []).append(p)
    return [sorted(groups[n], key=lambda p: str(p))[0] for n in sorted(groups)]


def select_recent_maps(logs_root: Path, minimum_pretest: int) -> list[Path]:
    out = []
    for p in logs_root.rglob("*.log"):
        number = pretest_number(p)
        if number is not None and number >= minimum_pretest:
            out.append(p)
    return sorted(out, key=lambda p: (pretest_number(p) or -1, str(p)))


def map_digest(path: Path) -> str:
    """Hash only the MAP block so repeated official battles share one map."""
    data = path.read_bytes()
    marker = b"END MAP"
    end = data.find(marker)
    if end < 0:
        raise ValueError(f"END MAP missing: {path}")
    return hashlib.sha256(data[: end + len(marker)]).hexdigest()


def select_unique_maps(logs_root: Path) -> list[Path]:
    by_digest: dict[str, Path] = {}
    for path in sorted(logs_root.rglob("*.log"), key=lambda p: str(p)):
        by_digest.setdefault(map_digest(path), path)
    return list(by_digest.values())


def play_one(
    candidate: str, opponent: str, map_path: str, candidate_as_a: bool
) -> tuple[str | None, str | None]:
    try:
        game_map = judge.load_map_from_file(map_path)
        exe_a, exe_b = (candidate, opponent) if candidate_as_a else (opponent, candidate)
        winner, reason, *_ = judge.run_one_game(
            exe_a, exe_b, 0, record=False, map_override=game_map
        )
        if winner is None:
            return "D", reason
        candidate_won = (candidate_as_a and winner == "A") or (
            not candidate_as_a and winner == "B"
        )
        return ("W" if candidate_won else "L"), reason
    except Exception as exc:  # pragma: no cover - diagnostic path
        judge.Agent.close_all()
        return None, repr(exc)


def evaluate(
    candidate: Path,
    models: list[Model],
    maps: list[Path],
    workers: int,
    progress_label: str,
    candidate_sides: tuple[bool, ...] = (True, False),
) -> dict[str, dict[str, int]]:
    candidate_s = str(candidate.resolve())
    results = {m.canonical: {"wins": 0, "losses": 0, "draws": 0, "errors": 0} for m in models}
    canonical_models = {m.canonical: m for m in models}
    tasks = []
    for canonical, model in canonical_models.items():
        for map_path in maps:
            for candidate_as_a in candidate_sides:
                tasks.append(
                    (
                        canonical,
                        str(model.executable.resolve()),
                        str(map_path.resolve()),
                        candidate_as_a,
                    )
                )

    total = len(tasks)
    started = time.monotonic()
    done = 0
    last_report = started
    print(
        f"[{progress_label}] opponents={len(canonical_models)} maps={len(maps)} "
        f"games={total} workers={workers}",
        flush=True,
    )
    with ProcessPoolExecutor(max_workers=workers) as pool:
        futures = {
            pool.submit(play_one, candidate_s, opponent, map_path, as_a): canonical
            for canonical, opponent, map_path, as_a in tasks
        }
        for future in as_completed(futures):
            canonical = futures[future]
            outcome, _reason = future.result()
            if outcome == "W":
                results[canonical]["wins"] += 1
            elif outcome == "L":
                results[canonical]["losses"] += 1
            elif outcome == "D":
                results[canonical]["draws"] += 1
            else:
                results[canonical]["errors"] += 1
            done += 1
            now = time.monotonic()
            if done == total or now - last_report >= 20:
                rate = done / max(0.001, now - started)
                eta = (total - done) / max(0.001, rate)
                print(
                    f"[{progress_label}] {done}/{total} ({done / total:.1%}) "
                    f"rate={rate:.1f}/s eta={eta:.0f}s",
                    flush=True,
                )
                last_report = now
    return results


def write_rows(
    output: Path,
    models: list[Model],
    results: dict[str, dict[str, int]],
    stage: str,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "stage",
                "model",
                "canonical",
                "duplicate_of",
                "wins",
                "losses",
                "draws",
                "errors",
                "games",
                "score_rate",
            ],
        )
        writer.writeheader()
        for model in sorted(models, key=lambda m: (int(m.label) if m.label.isdigit() else 10**18, m.label)):
            r = results[model.canonical]
            games = r["wins"] + r["losses"] + r["draws"]
            score = (r["wins"] + 0.5 * r["draws"]) / games if games else 0.0
            writer.writerow(
                {
                    "stage": stage,
                    "model": model.label,
                    "canonical": model.canonical,
                    "duplicate_of": "" if model.label == model.canonical else model.canonical,
                    **r,
                    "games": games,
                    "score_rate": f"{score:.6f}",
                }
            )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--candidate", required=True)
    ap.add_argument("--models-dir", required=True)
    ap.add_argument("--logs-root", required=True)
    ap.add_argument("--compile-workers", type=int, default=8)
    ap.add_argument("--game-workers", type=int, default=8)
    ap.add_argument("--output", default="all_models_screen.csv")
    ap.add_argument("--recent-min", type=int, default=None)
    ap.add_argument("--single-map", default=None)
    ap.add_argument(
        "--sample-unique-maps",
        type=int,
        default=None,
        help="sample this many unique official maps and use them for every opponent",
    )
    ap.add_argument(
        "--map-seed",
        type=int,
        default=42,
        help="seed used by --sample-unique-maps (default: 42)",
    )
    ap.add_argument(
        "--screen-min-pretest",
        type=int,
        default=None,
        help="when screening one map per pretest, keep this pretest and later",
    )
    ap.add_argument(
        "--candidate-side",
        choices=("both", "left", "right"),
        default="both",
    )
    ap.add_argument(
        "--only-model",
        action="append",
        default=[],
        help="numeric model stem to include; may be repeated",
    )
    ap.add_argument(
        "--exclude-model",
        action="append",
        default=[],
        help="numeric model stem to exclude; may be repeated",
    )
    ap.add_argument(
        "--extra-opponent",
        action="append",
        default=[],
        help="LABEL=EXE path; may be repeated",
    )
    args = ap.parse_args()

    candidate = Path(args.candidate).resolve()
    models_dir = Path(args.models_dir).resolve()
    logs_root = Path(args.logs_root).resolve()
    excluded = set(args.exclude_model)
    sources = [p for p in numeric_sources(models_dir) if p.stem not in excluded]
    if args.only_model:
        selected = set(args.only_model)
        sources = [p for p in sources if p.stem in selected]
        missing = sorted(selected - {p.stem for p in sources})
        if missing:
            raise SystemExit(f"unknown --only-model value(s): {', '.join(missing)}")

    digest_groups: dict[str, list[Path]] = {}
    for source in sources:
        digest_groups.setdefault(digest(source), []).append(source)
    representatives = [group[0] for group in digest_groups.values()]
    representatives.sort(key=lambda p: int(p.stem))

    bin_dir = models_dir / ".all_model_eval_bins"
    compiled: dict[Path, Path] = {}
    failures: dict[Path, str] = {}
    print(
        f"[compile] numeric={len(sources)} unique={len(representatives)} "
        f"workers={args.compile_workers}",
        flush=True,
    )
    with ThreadPoolExecutor(max_workers=args.compile_workers) as pool:
        future_map = {}
        for source in representatives:
            output = bin_dir / f"{source.stem}.exe"
            future_map[pool.submit(compile_one, source, output)] = output
        done = 0
        for future in as_completed(future_map):
            output = future_map[future]
            source, ok, message = future.result()
            if ok:
                compiled[source] = output
            else:
                failures[source] = message
            done += 1
            if done % 20 == 0 or done == len(future_map):
                print(
                    f"[compile] {done}/{len(future_map)} ok={len(compiled)} "
                    f"failed={len(failures)}",
                    flush=True,
                )

    if failures:
        fail_path = Path(args.output).with_suffix(".compile_failures.txt")
        fail_path.write_text(
            "\n\n".join(f"{p}\n{msg}" for p, msg in failures.items()),
            encoding="utf-8",
        )
        print(f"[compile] failures written to {fail_path}", flush=True)

    source_to_canonical: dict[Path, str] = {}
    executable_by_canonical: dict[str, Path] = {}
    for group in digest_groups.values():
        representative = group[0]
        if representative not in compiled:
            continue
        canonical = representative.stem
        executable_by_canonical[canonical] = compiled[representative]
        for source in group:
            source_to_canonical[source] = canonical

    models = [
        Model(
            label=source.stem,
            source=source,
            executable=executable_by_canonical[source_to_canonical[source]],
            canonical=source_to_canonical[source],
        )
        for source in sources
        if source in source_to_canonical
    ]
    for item in args.extra_opponent:
        if "=" not in item:
            raise SystemExit(f"bad --extra-opponent: {item!r}")
        label, raw_path = item.split("=", 1)
        exe = Path(raw_path).resolve()
        models.append(Model(label=label, source=None, executable=exe, canonical=label))

    if args.single_map is not None:
        maps = [Path(args.single_map).resolve()]
        stage = "single_map"
    elif args.sample_unique_maps is not None:
        available_maps = select_unique_maps(logs_root)
        if args.sample_unique_maps > len(available_maps):
            raise SystemExit(
                f"requested {args.sample_unique_maps} unique maps, "
                f"only {len(available_maps)} available"
            )
        maps = random.Random(args.map_seed).sample(
            available_maps, args.sample_unique_maps
        )
        stage = f"unique_maps_{args.sample_unique_maps}_seed_{args.map_seed}"
    elif args.recent_min is None:
        maps = select_screen_maps(logs_root)
        if args.screen_min_pretest is not None:
            maps = [
                path
                for path in maps
                if (pretest_number(path) or -1) >= args.screen_min_pretest
            ]
        stage = "one_map_per_pretest"
    else:
        maps = select_recent_maps(logs_root, args.recent_min)
        stage = f"all_maps_pretest_{args.recent_min}_plus"
    if not maps:
        raise SystemExit("no maps selected")

    # Evaluate canonical executables once, while preserving aliases for the CSV.
    canonical_models = []
    seen = set()
    for model in models:
        if model.canonical not in seen:
            canonical_models.append(model)
            seen.add(model.canonical)
    candidate_sides = {
        "both": (True, False),
        "left": (True,),
        "right": (False,),
    }[args.candidate_side]
    results = evaluate(
        candidate,
        canonical_models,
        maps,
        args.game_workers,
        stage,
        candidate_sides,
    )
    write_rows(Path(args.output), models, results, stage)

    ranked = []
    for canonical, r in results.items():
        games = r["wins"] + r["losses"] + r["draws"]
        score = (r["wins"] + 0.5 * r["draws"]) / games if games else -1.0
        ranked.append((score, canonical, r))
    ranked.sort()
    print(f"[result] wrote {args.output}", flush=True)
    print("[result] hardest 20 opponents (candidate perspective):", flush=True)
    for score, canonical, r in ranked[:20]:
        print(
            f"  {canonical}: W{r['wins']} L{r['losses']} D{r['draws']} "
            f"E{r['errors']} score={score:.1%}",
            flush=True,
        )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
