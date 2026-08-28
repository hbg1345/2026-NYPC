#!/usr/bin/env python3
"""Generate representative first mandatory-reclaim success/failure replays."""

from __future__ import annotations

import argparse
import base64
import csv
import gzip
import html
import json
import shutil
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

from generate_paired_changed_replays import record_one


def choose_cases(rows: list[dict[str, str]], per_result: int) -> list[dict[str, str]]:
    selected: list[dict[str, str]] = []
    relations = ("BEHIND", "TIED", "AHEAD")
    outcome_rank = {"L": 0, "D": 1, "W": 2}

    for category in ("SUCCESS", "FAILURE"):
        if category == "SUCCESS":
            candidates = [row for row in rows if row["first_attack_result"] == "SUCCESS"]
        else:
            candidates = [
                row
                for row in rows
                if row["first_attack_result"]
                in ("FAILURE", "FAILURE_RELEASED", "UNRESOLVED_AFTER_LAUNCH")
            ]
        used_models: set[str] = set()
        picked: list[dict[str, str]] = []

        for relation in relations:
            group = [row for row in candidates if row["cf_relation"] == relation]
            group.sort(
                key=lambda row: (
                    outcome_rank.get(row["outcome"], 9),
                    int(row["model"]),
                    int(row["trigger_turn"]),
                    row["map"],
                )
            )
            choice = next((row for row in group if row["model"] not in used_models), None)
            if choice is None and group:
                choice = group[0]
            if choice is not None:
                copy = dict(choice)
                copy["replay_category"] = category
                picked.append(copy)
                used_models.add(choice["model"])

        if len(picked) < per_result:
            remaining = [row for row in candidates if row not in picked]
            remaining.sort(
                key=lambda row: (
                    outcome_rank.get(row["outcome"], 9),
                    row["model"] in used_models,
                    int(row["model"]),
                    row["map"],
                )
            )
            for row in remaining:
                copy = dict(row)
                copy["replay_category"] = category
                picked.append(copy)
                used_models.add(row["model"])
                if len(picked) >= per_result:
                    break
        selected.extend(picked[:per_result])
    return selected


def record_case(task: tuple[dict[str, str], str, str, str, str]) -> dict[str, str]:
    row, candidate, opponent, template, output_dir_s = task
    output_dir = Path(output_dir_s)
    category = row["replay_category"].lower()
    case_name = (
        f"{category}_vs_{row['model']}_pretest{row['pretest']}_"
        f"map{row['map_id']}_battle{row['battle']}"
    )
    case_dir = output_dir / case_name
    case_dir.mkdir(parents=True, exist_ok=True)
    replay = case_dir / "replay.html"
    debug = case_dir / "debug.txt"
    result = record_one(
        candidate,
        opponent,
        row["map"],
        replay,
        debug,
        template,
        str(output_dir / ".work"),
        case_name,
    )
    output = dict(row)
    output["recorded_outcome"] = result["outcome"]
    output["recorded_reason"] = result["reason"]
    output["replay"] = str(replay)
    output["debug"] = str(debug)
    return output


def packed(path: Path) -> str:
    return base64.b64encode(
        gzip.compress(path.read_bytes(), compresslevel=9, mtime=0)
    ).decode("ascii")


SHELL = r'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>첫 필수 거점 탈환 성공/실패 리플레이</title>
<style>
html,body{height:100%;margin:0;background:#11151b;color:#eef1f4;font-family:"Segoe UI",system-ui,sans-serif;overflow:hidden}
body{display:grid;grid-template-rows:auto 1fr}.bar{display:flex;align-items:center;gap:8px;padding:8px 12px;background:#1b1f26;border-bottom:1px solid #000}
.title{font-weight:800;white-space:nowrap}select{flex:1;min-width:260px;background:#2a3039;color:#fff;border:1px solid #4b5563;border-radius:5px;padding:7px 9px}
button{background:#2a3039;color:#fff;border:1px solid #4b5563;border-radius:5px;padding:7px 11px;cursor:pointer}.status{color:#e0b34a;font-size:12px;white-space:nowrap}
iframe{width:100%;height:100%;border:0;background:#1b1f26}
</style></head><body><div class="bar">
<span class="title">첫 필수 탈환</span><button id="prev">◀</button><select id="case"></select><button id="next">▶</button><span class="status" id="status"></span>
</div><iframe id="viewer" title="NYPC replay viewer"></iframe><script>
const CASES=__CASES__,select=document.getElementById("case"),viewer=document.getElementById("viewer"),status=document.getElementById("status");
const groups=new Map();CASES.forEach((item,index)=>{if(!groups.has(item.category)){const g=document.createElement("optgroup");g.label=item.category==="SUCCESS"?"첫 탈환 성공":"첫 탈환 실패·해제";groups.set(item.category,g);select.appendChild(g)}const o=document.createElement("option");o.value=index;o.textContent=item.label;groups.get(item.category).appendChild(o)});
function bytes(value){const raw=atob(value),out=new Uint8Array(raw.length);for(let i=0;i<raw.length;i++)out[i]=raw.charCodeAt(i);return out}
async function unpack(value){const stream=new Blob([bytes(value)]).stream().pipeThrough(new DecompressionStream("gzip"));return await new Response(stream).text()}
let token=0;async function load(index){index=Math.max(0,Math.min(CASES.length-1,Number(index)||0));select.value=String(index);history.replaceState(null,"",`#${index+1}`);const item=CASES[index],mine=++token;status.textContent="불러오는 중…";const replay=await unpack(item.replay);if(mine!==token)return;viewer.srcdoc=replay;status.textContent=`${index+1}/${CASES.length} · ${item.status}`}
select.addEventListener("change",()=>load(select.value));document.getElementById("prev").addEventListener("click",()=>load(Number(select.value)-1));document.getElementById("next").addEventListener("click",()=>load(Number(select.value)+1));
document.addEventListener("keydown",e=>{if(e.altKey&&e.key==="ArrowLeft")load(Number(select.value)-1);if(e.altKey&&e.key==="ArrowRight")load(Number(select.value)+1)});const m=location.hash.match(/^#(\d+)$/);load(m?Number(m[1])-1:0);
</script></body></html>'''


def write_bundle(records: list[dict[str, str]], output: Path) -> None:
    cases = []
    for row in records:
        relation_ko = {"BEHIND": "열세", "TIED": "동률", "AHEAD": "우세"}[
            row["cf_relation"]
        ]
        if row["replay_category"] == "SUCCESS":
            category_ko = "성공"
        elif row["first_attack_result"] == "UNRESOLVED_AFTER_LAUNCH":
            category_ko = "실패·경기 종료까지 미확보"
        else:
            category_ko = "실패·출격 후 해제"
        label = (
            f"{category_ko} · vs {row['model']} · Pretest #{row['pretest']} / "
            f"{row['map_id']} · T{row['launch_turn']} {row['launch_count']}명 → R{row['target']}"
        )
        status = (
            f"확보전 {row['cf_final_bases_a']}:{row['cf_final_bases_b']} {relation_ko} · "
            f"실제 {row['recorded_outcome']}"
        )
        cases.append(
            {
                "category": row["replay_category"],
                "label": label,
                "status": status,
                "replay": packed(Path(row["replay"])),
            }
        )
    data = json.dumps(cases, ensure_ascii=False, separators=(",", ":")).replace(
        "</", "<\\/"
    )
    output.write_text(SHELL.replace("__CASES__", data), encoding="utf-8")


def write_index(records: list[dict[str, str]], output_dir: Path) -> None:
    lines = [
        '<!doctype html><html lang="ko"><head><meta charset="utf-8">',
        "<title>첫 필수 거점 탈환 성공/실패 리플레이</title>",
        "<style>body{font:15px/1.5 system-ui;max-width:1400px;margin:30px auto;padding:0 16px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ccc;padding:6px}th{background:#eee}</style></head><body>",
        "<h1>첫 필수 거점 탈환 성공/실패 대표 리플레이</h1>",
        '<p><a href="first_mandatory_outcomes_replay.html">통합 리플레이 뷰어 열기</a></p>',
        "<table><thead><tr><th>분류</th><th>상대/맵</th><th>첫 작전</th><th>확보전 반사실</th><th>실제 결과</th><th>파일</th></tr></thead><tbody>",
    ]
    for row in records:
        replay = Path(row["replay"]).relative_to(output_dir).as_posix()
        debug = Path(row["debug"]).relative_to(output_dir).as_posix()
        if row["replay_category"] == "SUCCESS":
            category = "성공"
        elif row["first_attack_result"] == "UNRESOLVED_AFTER_LAUNCH":
            category = "실패·경기 종료까지 미확보"
        else:
            category = "실패·출격 후 목표 해제"
        relation = {"BEHIND": "열세", "TIED": "동률", "AHEAD": "우세"}[
            row["cf_relation"]
        ]
        map_label = (
            f"vs {row['model']} · Pretest #{row['pretest']} / {row['map_id']} "
            f"(battle-{row['battle']})"
        )
        result_label = (
            f"T{row['result_turn']}"
            if row["result_turn"]
            else "경기 종료까지 미확보"
        )
        operation = (
            f"발동 T{row['trigger_turn']}, R{row['target']} · 출격 "
            f"T{row['launch_turn']} {row['launch_count']}명 · 판정 {result_label}"
        )
        counterfactual = (
            f"{row['cf_start_bases_a']}:{row['cf_start_bases_b']} → "
            f"{row['cf_final_bases_a']}:{row['cf_final_bases_b']} ({relation})"
        )
        lines.append(
            "<tr>"
            f"<td>{category}</td><td>{html.escape(map_label)}</td>"
            f"<td>{html.escape(operation)}</td><td>{html.escape(counterfactual)}</td>"
            f"<td>{html.escape(row['recorded_outcome'])}</td>"
            f'<td><a href="{replay}">replay</a> · <a href="{debug}">debug</a></td>'
            "</tr>"
        )
    lines.append("</tbody></table></body></html>")
    (output_dir / "index.html").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--pool", type=Path, default=Path("pool"))
    parser.add_argument("--template", type=Path, default=Path("viewer_template.html"))
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("first_mandatory_capture_only_outcome_replays_6"),
    )
    parser.add_argument("--per-result", type=int, default=3)
    parser.add_argument("--workers", type=int, default=16)
    args = parser.parse_args()

    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise SystemExit(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / ".work").mkdir(exist_ok=True)

    with args.csv.resolve().open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    chosen = choose_cases(rows, args.per_result)
    opponents = {path.stem: path.resolve() for path in args.pool.resolve().glob("*.exe")}
    tasks = [
        (
            row,
            str(args.candidate.resolve()),
            str(opponents[row["model"]]),
            str(args.template.resolve()),
            str(output_dir),
        )
        for row in chosen
    ]
    records: list[dict[str, str]] = []
    with ProcessPoolExecutor(max_workers=min(args.workers, len(tasks))) as executor:
        futures = [executor.submit(record_case, task) for task in tasks]
        for future in as_completed(futures):
            records.append(future.result())
    order = {id(row): i for i, row in enumerate(chosen)}
    key_order = {
        (row["replay_category"], row["model"], row["map"]): i
        for i, row in enumerate(chosen)
    }
    records.sort(
        key=lambda row: key_order[(row["replay_category"], row["model"], row["map"])]
    )

    write_bundle(records, output_dir / "first_mandatory_outcomes_replay.html")
    write_index(records, output_dir)
    manifest = output_dir / "manifest.csv"
    with manifest.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0].keys()))
        writer.writeheader()
        writer.writerows(records)
    shutil.rmtree(output_dir / ".work", ignore_errors=True)
    print(f"[done] {output_dir / 'first_mandatory_outcomes_replay.html'}")
    for row in records:
        print(
            f"{row['replay_category']:7s} vs {row['model']} "
            f"Pretest#{row['pretest']} map{row['map_id']} "
            f"T{row['launch_turn']} {row['launch_count']} -> R{row['target']} "
            f"cf={row['cf_final_bases_a']}:{row['cf_final_bases_b']} "
            f"actual={row['recorded_outcome']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
