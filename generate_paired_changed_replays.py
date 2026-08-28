#!/usr/bin/env python3
"""Generate paired replay HTML for games whose W/D/L result changed."""

from __future__ import annotations

import argparse
import base64
import contextlib
import csv
import gzip
import html
import io
import json
import os
import random
import shutil
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import judge
from generate_pool_loss_replays import map_labels, unique_maps, write_replay_file


def outcome_code(winner: str | None) -> str:
    if winner is None:
        return "D"
    return "W" if winner == "A" else "L"


def play_pair(
    baseline: str, candidate: str, opponent: str, map_path: str
) -> dict[str, str]:
    try:
        base_map = judge.load_map_from_file(map_path)
        base_winner, base_reason, *_ = judge.run_one_game(
            baseline, opponent, 0, record=False, map_override=base_map
        )
        judge.Agent.close_all()
        cand_map = judge.load_map_from_file(map_path)
        cand_winner, cand_reason, *_ = judge.run_one_game(
            candidate, opponent, 0, record=False, map_override=cand_map
        )
        return {
            "baseline_outcome": outcome_code(base_winner),
            "baseline_reason": base_reason or "",
            "candidate_outcome": outcome_code(cand_winner),
            "candidate_reason": cand_reason or "",
        }
    except Exception as exc:
        return {
            "baseline_outcome": "E",
            "baseline_reason": repr(exc),
            "candidate_outcome": "E",
            "candidate_reason": repr(exc),
        }
    finally:
        judge.Agent.close_all()


def record_one(
    executable: str,
    opponent: str,
    map_path: str,
    replay_path: Path,
    debug_path: Path,
    template_path: str,
    work_root: str,
    tag: str,
) -> dict[str, str]:
    work_dir = Path(tempfile.mkdtemp(prefix=f"{tag}_", dir=work_root))
    old_cwd = Path.cwd()
    captured = io.StringIO()
    try:
        os.chdir(work_dir)
        with contextlib.redirect_stdout(captured), contextlib.redirect_stderr(captured):
            game_map = judge.load_map_from_file(map_path)
            winner, reason, _seed, recorded_map, _rows, frames, _ = (
                judge.run_one_game(
                    executable,
                    opponent,
                    0,
                    record=True,
                    map_override=game_map,
                )
            )
        template = Path(template_path).read_text(encoding="utf-8")
        write_replay_file(
            template,
            replay_path,
            executable,
            opponent,
            winner,
            reason or "",
            recorded_map,
            frames,
        )
        debug_source = work_dir / "debug_A.txt"
        if debug_source.exists():
            shutil.copyfile(debug_source, debug_path)
        else:
            debug_path.write_text(
                "candidate did not emit debug_A.txt\n" + captured.getvalue(),
                encoding="utf-8",
            )
        return {"outcome": outcome_code(winner), "reason": reason or ""}
    finally:
        os.chdir(old_cwd)
        judge.Agent.close_all()
        shutil.rmtree(work_dir, ignore_errors=True)


def record_pair(
    baseline: str,
    candidate: str,
    opponent: str,
    model: str,
    map_path: str,
    case_dir: str,
    template_path: str,
    work_root: str,
) -> dict[str, str]:
    destination = Path(case_dir)
    destination.mkdir(parents=True, exist_ok=True)
    baseline_replay = destination / "baseline.html"
    baseline_debug = destination / "baseline.debug.txt"
    candidate_replay = destination / "candidate.html"
    candidate_debug = destination / "candidate.debug.txt"
    base = record_one(
        baseline,
        opponent,
        map_path,
        baseline_replay,
        baseline_debug,
        template_path,
        work_root,
        f"{model}_base",
    )
    cand = record_one(
        candidate,
        opponent,
        map_path,
        candidate_replay,
        candidate_debug,
        template_path,
        work_root,
        f"{model}_cand",
    )
    pretest, map_id, battle = map_labels(Path(map_path))
    return {
        "model": model,
        "pretest": pretest,
        "map_id": map_id,
        "battle": battle,
        "map": map_path,
        "baseline_outcome": base["outcome"],
        "baseline_reason": base["reason"],
        "candidate_outcome": cand["outcome"],
        "candidate_reason": cand["reason"],
        "baseline_replay": str(baseline_replay),
        "baseline_debug": str(baseline_debug),
        "candidate_replay": str(candidate_replay),
        "candidate_debug": str(candidate_debug),
    }


SHELL = r'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>최고판 ↔ 탐지형 12턴 수정판 결과 변경 리플레이</title>
<style>
html,body{height:100%;margin:0;background:#11151b;color:#eef1f4;font-family:"Segoe UI",system-ui,sans-serif;overflow:hidden}
body{display:grid;grid-template-rows:auto 1fr}.bar{display:flex;align-items:center;gap:8px;padding:8px 12px;background:#1b1f26;border-bottom:1px solid #000}
.title{font-weight:800;white-space:nowrap}select{flex:1;min-width:220px;background:#2a3039;color:#fff;border:1px solid #4b5563;border-radius:5px;padding:7px 9px}
button{background:#2a3039;color:#fff;border:1px solid #4b5563;border-radius:5px;padding:7px 11px;cursor:pointer}button.active{background:#8a6517;border-color:#e0b34a}
.status{color:#e0b34a;font-size:12px;white-space:nowrap}iframe{width:100%;height:100%;border:0;background:#1b1f26}
</style></head><body><div class="bar">
<span class="title">결과 변경 비교</span><button id="prev">◀</button><select id="case"></select><button id="next">▶</button>
<button id="baseline">기존 최고판</button><button id="candidate">12턴 수정판</button><span class="status" id="status"></span>
</div><iframe id="viewer" title="NYPC replay viewer"></iframe><script>
const CASES=__CASES__;
const select=document.getElementById("case"),viewer=document.getElementById("viewer"),status=document.getElementById("status");
const groups=new Map();CASES.forEach((item,index)=>{if(!groups.has(item.model)){const g=document.createElement("optgroup");g.label=`vs ${item.model}`;groups.set(item.model,g);select.appendChild(g)}const o=document.createElement("option");o.value=index;o.textContent=item.label;groups.get(item.model).appendChild(o)});
function bytes(value){const raw=atob(value),out=new Uint8Array(raw.length);for(let i=0;i<raw.length;i++)out[i]=raw.charCodeAt(i);return out}
async function text(value){const stream=new Blob([bytes(value)]).stream().pipeThrough(new DecompressionStream("gzip"));return await new Response(stream).text()}
let variant="baseline",token=0;
async function load(index,nextVariant=variant){index=Math.max(0,Math.min(CASES.length-1,Number(index)||0));variant=nextVariant;select.value=String(index);const item=CASES[index],mine=++token;history.replaceState(null,"",`#${index+1}-${variant}`);document.getElementById("baseline").classList.toggle("active",variant==="baseline");document.getElementById("candidate").classList.toggle("active",variant==="candidate");status.textContent="불러오는 중…";const replay=await text(item[variant]);if(mine!==token)return;viewer.srcdoc=replay;status.textContent=`${index+1}/${CASES.length} · ${variant==="baseline"?"최고판":"수정판"} ${item[variant+"Outcome"]}`}
select.addEventListener("change",()=>load(select.value));document.getElementById("prev").addEventListener("click",()=>load(Number(select.value)-1));document.getElementById("next").addEventListener("click",()=>load(Number(select.value)+1));document.getElementById("baseline").addEventListener("click",()=>load(select.value,"baseline"));document.getElementById("candidate").addEventListener("click",()=>load(select.value,"candidate"));
document.addEventListener("keydown",e=>{if(e.key==="1")load(select.value,"baseline");if(e.key==="2")load(select.value,"candidate");if(e.altKey&&e.key==="ArrowLeft")load(Number(select.value)-1);if(e.altKey&&e.key==="ArrowRight")load(Number(select.value)+1)});
const match=location.hash.match(/^#(\d+)-(baseline|candidate)$/);load(match?Number(match[1])-1:0,match?match[2]:"baseline");
</script></body></html>'''


def packed(path: Path) -> str:
    return base64.b64encode(
        gzip.compress(path.read_bytes(), compresslevel=9, mtime=0)
    ).decode("ascii")


def write_bundle(records: list[dict[str, str]], output: Path) -> None:
    cases = []
    for row in records:
        label = (
            f"Pretest #{row['pretest']} / {row['map_id']} "
            f"(battle-{row['battle']}) — "
            f"{row['baseline_outcome']} → {row['candidate_outcome']}"
        )
        cases.append(
            {
                "model": row["model"],
                "label": label,
                "baselineOutcome": row["baseline_outcome"],
                "candidateOutcome": row["candidate_outcome"],
                "baseline": packed(Path(row["baseline_replay"])),
                "candidate": packed(Path(row["candidate_replay"])),
            }
        )
    data = json.dumps(cases, ensure_ascii=False, separators=(",", ":")).replace(
        "</", "<\\/"
    )
    output.write_text(SHELL.replace("__CASES__", data), encoding="utf-8")


def write_index(output_dir: Path, records: list[dict[str, str]]) -> None:
    lines = [
        "<!doctype html><html lang=\"ko\"><head><meta charset=\"utf-8\">",
        "<title>결과 변경 리플레이 목록</title>",
        "<style>body{font:15px/1.5 system-ui;max-width:1200px;margin:30px auto;padding:0 16px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ccc;padding:6px}th{background:#eee}</style></head><body>",
        f"<h1>최고판 ↔ 12턴 수정판 결과 변경 ({len(records)}판)</h1>",
        '<p><a href="paired_changed_replays.html">한 파일 비교 뷰어 열기</a> — 1: 최고판, 2: 수정판</p>',
        "<table><thead><tr><th>상대</th><th>맵</th><th>변경</th><th>최고판</th><th>수정판</th><th>로그</th></tr></thead><tbody>",
    ]
    for row in records:
        base_replay = Path(row["baseline_replay"]).relative_to(output_dir).as_posix()
        cand_replay = Path(row["candidate_replay"]).relative_to(output_dir).as_posix()
        base_debug = Path(row["baseline_debug"]).relative_to(output_dir).as_posix()
        cand_debug = Path(row["candidate_debug"]).relative_to(output_dir).as_posix()
        label = f"Pretest #{row['pretest']} / {row['map_id']} (battle-{row['battle']})"
        lines.append(
            "<tr>"
            f"<td>{html.escape(row['model'])}</td><td>{html.escape(label)}</td>"
            f"<td>{row['baseline_outcome']} → {row['candidate_outcome']}</td>"
            f'<td><a href="{base_replay}">replay</a></td>'
            f'<td><a href="{cand_replay}">replay</a></td>'
            f'<td><a href="{base_debug}">최고</a> · <a href="{cand_debug}">수정</a></td>'
            "</tr>"
        )
    lines.append("</tbody></table></body></html>")
    (output_dir / "index.html").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--pool-dir", type=Path, default=Path("pool"))
    parser.add_argument(
        "--logs-root", type=Path, default=Path("summission_result/tournament")
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--games", type=int, default=10)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--workers", type=int, default=16)
    args = parser.parse_args()

    baseline = args.baseline.resolve()
    candidate = args.candidate.resolve()
    pool_dir = args.pool_dir.resolve()
    logs_root = args.logs_root.resolve()
    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise SystemExit(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    work_root = output_dir / ".work"
    work_root.mkdir(exist_ok=True)

    opponents = {
        path.stem: path.resolve()
        for path in pool_dir.glob("*.exe")
        if path.stem.isdigit()
    }
    maps = unique_maps(logs_root)
    chosen = random.Random(args.seed).sample(maps, args.games)
    tasks = [
        (model, str(opponent), str(map_path.resolve()))
        for model, opponent in opponents.items()
        for map_path in chosen
    ]
    print(f"[scan] pairs={len(tasks)} workers={args.workers}", flush=True)
    started = time.monotonic()
    changed: list[dict[str, str]] = []
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(
                play_pair, str(baseline), str(candidate), opponent, map_path
            ): (model, opponent, map_path)
            for model, opponent, map_path in tasks
        }
        for done, future in enumerate(as_completed(futures), 1):
            model, opponent, map_path = futures[future]
            result = future.result()
            if "E" in (result["baseline_outcome"], result["candidate_outcome"]):
                raise RuntimeError(f"game error: {model} {map_path}: {result}")
            if result["baseline_outcome"] != result["candidate_outcome"]:
                changed.append(
                    {
                        "model": model,
                        "opponent": opponent,
                        "map": map_path,
                        **result,
                    }
                )
            if done % 100 == 0 or done == len(futures):
                print(
                    f"[scan] {done}/{len(futures)} changed={len(changed)} "
                    f"elapsed={time.monotonic()-started:.1f}s",
                    flush=True,
                )

    changed.sort(key=lambda row: (int(row["model"]), row["map"]))
    template_path = str(Path(__file__).resolve().parent / "viewer_template.html")
    record_tasks = []
    for index, row in enumerate(changed, 1):
        pretest, map_id, battle = map_labels(Path(row["map"]))
        case_dir = output_dir / f"vs_{row['model']}" / (
            f"change_{index:02d}_pretest{pretest}_map{map_id}_battle{battle}"
        )
        record_tasks.append(
            (
                str(baseline),
                str(candidate),
                row["opponent"],
                row["model"],
                row["map"],
                str(case_dir),
                template_path,
                str(work_root),
            )
        )

    print(f"[record] changed={len(record_tasks)}", flush=True)
    records: list[dict[str, str]] = []
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(record_pair, *task): task for task in record_tasks}
        for done, future in enumerate(as_completed(futures), 1):
            record = future.result()
            if record["baseline_outcome"] == record["candidate_outcome"]:
                raise RuntimeError(f"recording no longer differs: {record}")
            records.append(record)
            if done % 10 == 0 or done == len(futures):
                print(f"[record] {done}/{len(futures)}", flush=True)

    records.sort(
        key=lambda row: (
            int(row["model"]),
            int(row["pretest"]),
            int(row["map_id"]),
        )
    )
    manifest = output_dir / "changed_manifest.csv"
    with manifest.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0].keys()))
        writer.writeheader()
        writer.writerows(records)
    write_index(output_dir, records)
    write_bundle(records, output_dir / "paired_changed_replays.html")
    shutil.rmtree(work_root, ignore_errors=True)
    transitions: dict[str, int] = {}
    for row in records:
        key = f"{row['baseline_outcome']}->{row['candidate_outcome']}"
        transitions[key] = transitions.get(key, 0) + 1
    print(
        f"[done] changed={len(records)} transitions={transitions} "
        f"bundle={(output_dir / 'paired_changed_replays.html').stat().st_size / 1048576:.1f}MiB",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
