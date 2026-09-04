# NEXT VISION 본선 작업 폴더

이 폴더를 본선 작업의 기준으로 사용합니다.

## 파일

| 파일 | 용도 |
| --- | --- |
| `master_main.cpp` | 현재 본선 전략 소스이자 수정 기준 파일 |
| `GameRules.md` | 본선 규칙과 인터랙션 정리 |
| `viewer_template.html` | 시야 전환을 지원하는 본선 리플레이 템플릿 |
| `build_sample_replays.py` | `sample_ai_replay/*.txt`를 HTML 리플레이로 변환 |
| `analyze_replay_debug.py` | 전사 이동 명령의 전체 최단 경로와 적 건물·병력 경유 위험 분석 |

## 로컬 빌드

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pedantic master_main.cpp -o master_main
```

대회 제출 시에는 `master_main.cpp`를 사용합니다. 빌드된 `master_main` 실행 파일은 로컬 산출물이므로 Git에는 포함하지 않습니다.

## 샘플 AI 리플레이

공식 샘플 로그를 로컬 `sample_ai_replay/` 폴더에 둔 뒤 HTML로 변환하면 리플레이를 확인할 수 있습니다. 원본 로그와 생성된 HTML은 로컬 산출물이므로 Git에는 포함하지 않습니다.

- `0`: 양쪽을 모두 표시하는 전지 시야
- `1`: LEFT의 실제 시야
- `2`: RIGHT의 실제 시야
- `Space`를 누르는 동안: 상대 금화·병력 추정값과 실제값을 비교하고 실제 배치를 표시
- `P`: 재생 또는 일시정지
- `←`, `→`: 이전 또는 다음 턴

샘플 로그가 바뀌면 다음 명령으로 HTML을 다시 생성합니다.

```bash
python3 build_sample_replays.py
```

특정 전사의 장거리 이동이 적 거점으로 이어진 원인을 확인할 때는 다음처럼 실행합니다.

```bash
python3 analyze_replay_debug.py "sample_ai_replay/1 (14).txt" --unit A4
```

명령 당시의 엔진 최단 경로와 그 경로 위에 이미 존재하던 적 건물·병력을
`HAZARD`로 표시합니다. 모든 전사의 이동을 검사하려면 `--all-moves`를 붙입니다.

## 현재 상태

- 새 초기 금화·건물 비용·400턴 규칙 반영
- `START TURN`, `TIME`, 시야 스냅샷, `FINISH` 프로토콜 반영
- 중앙·자연 거점 확장, 건설 목적 유지, 경제 업그레이드, 긴급 방어, 동시 공세, 정찰 전략 적용
- 본선 시야와 Space 상상 오버레이를 지원하는 샘플 AI 리플레이 뷰어 적용
- C++20 컴파일 및 LEFT/RIGHT 기본 프로토콜 검사 완료
- 새 공식 시뮬레이터 상대 성능 평가는 아직 진행 전
