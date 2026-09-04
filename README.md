# NYPC 2026 Master Track Agents

[한국어](#한국어) | [日本語](#日本語) | [English](#english)

## 한국어

### 저장소 개요

NYPC 2026 Master Track에 개인으로 참가해 **예선 1,603팀 중 19위(상위 약 1.2%)**로 본선에 진출했습니다.

이 저장소에는 예선과 본선 에이전트가 함께 있습니다. 두 라운드는 전장 크기, 초기 자원, 최대 턴, 시야 및 인터랙션 프로토콜이 다르므로 작업 경로를 분리했습니다.

| 경로 | 용도 |
|---|---|
| `main.c++` | 예선 최종 휴리스틱 에이전트 |
| `judge.py` | 예선 로컬 심판, 맵 생성, 배치 평가 및 토너먼트 |
| `GameRules.md` | 예선 규칙 정리 |
| `pool/*.cpp` | 예선 비교 평가에 사용한 후보 전략 |
| [`NEXT_VISION/`](NEXT_VISION/README.md) | 본선 전용 소스, 규칙, 로컬 심판 및 분석 도구 |
| `NEXT_VISION/master_main.cpp` | 현재 본선 전략의 기준 소스 |
| `NEXT_VISION/judge.py` | 본선 규칙과 시야 프로토콜을 반영한 로컬 심판 |

### 예선과 본선

| 항목 | 예선 | 본선 `NEXT VISION` |
|---|---:|---:|
| 전장 구역 수 | 51–109 | 181–249 |
| 초기 금화 | 500 | 750 |
| 최대 턴 | 200 | 400 |
| 관측 | 전체 상태 | 아군 전사·건물에서 그래프 거리 2 이내 |
| 기준 C++ | C++17 | C++20 |

### 빠른 시작

예선 에이전트 두 개를 100게임 비교합니다.

```bash
python3 judge.py main.c++ pool/110584.cpp --games 100 --workers 8 --seed 42
```

본선 기준 에이전트를 빌드하고 로컬 대전을 실행합니다.

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  NEXT_VISION/master_main.cpp -o NEXT_VISION/master_main

python3 NEXT_VISION/judge.py \
  NEXT_VISION/master_main.cpp NEXT_VISION/sample_ai_imitation.cpp \
  --games 1 --seed 42
```

본선 폴더의 자세한 파일 설명과 리플레이 분석 방법은 [`NEXT_VISION/README.md`](NEXT_VISION/README.md)를 참고하세요. 로컬 실행 파일, 디버그 로그, 원본 리플레이와 생성된 HTML은 Git에 포함하지 않습니다.

## 日本語

### 結果

NYPC 2026 Master Trackに個人で参加し、**予選1,603チーム中19位（上位約1.2%）**でFinal Roundへ進出しました。

repository rootには予選用agentとlocal judgeを保存し、ルール・視野・protocolが異なるFinal Round用実装は[`NEXT_VISION/`](NEXT_VISION/README.md)に分離しています。Final Roundの基準sourceは`NEXT_VISION/master_main.cpp`、local judgeは`NEXT_VISION/judge.py`です。

この競技では、2人のagentがgraph状の戦場で戦士を移動させ、拠点へbaseを建設・upgradeし、HQで兵力を訓練しながら相手HQの破壊を目指します。各turnで限られたgoldを移動、建設、訓練へ配分するため、最短経路だけでなく、将来の収入、維持費、増援、turret damage、残りturnを同時に判断する必要があります。

私はC++17によるheuristic agent、公式ルールを再現するPython judge、map generator、replay viewer、候補戦略のtournament環境を設計・実装しました。

### 戦略の構造

```mermaid
flowchart LR
    I["Current game state"] --> P["All-pairs paths"]
    P --> E["Economy / expansion forecast"]
    P --> C["Combat forward simulation"]
    E --> D["Defense and build allocation"]
    C --> A["Attack force and synchronized arrival"]
    D --> O["MOVE / BUILD / ATTACK / TRAIN commands"]
    A --> O
```

### 技術的に注力した点

#### 1. Floyd-Warshallによる全点対最短経路

戦場は51～109 regionのweighted graphです。初期化時にFloyd-Warshallで全region間の距離を計算し、各source-target pairのnext hopとhop数も保存します。その後の拠点候補、援軍、敵HQへの進軍、移動中の敵兵の目標推定は同じpath tableを使用します。

距離だけで建設を決めるのではなく、到着・建設までのturn、残りturnで得られるincome、建設・upgrade・移動・維持costを比較します。費用を制限時間内に回収できないbaseは候補から除外し、短期的な領土拡大が終盤のgold不足につながらないようにしました。

#### 2. `assaultOutcome`による攻撃・防御のforward simulation

「最も近い敵へ進む」規則では、中間拠点、両軍の増援、turret、訓練、gold変化を扱えません。そこで、敵HQやbaseへ到達するまでをturn単位で進める`assaultOutcome`を実装しました。

simulationは兵力の到着時点、building HP、turret/unit damage、双方のgold・income・upgrade・追加訓練を更新します。`plan_attack_force`は新規投入人数を増やしながら勝てる最小兵力を探索し、予測誤差に対して`ATTACK_SAFETY_MARGIN`を追加します。逆向きのsimulationでは、現在の敵進軍を防ぐために必要な守備兵力を計算します。

#### 3. 複数turnにまたがる部隊の目的と同期

戦士には`NONE`、`MOVE`、`BUILD`、`ATTACK`の目的を保持します。これにより、すでに建設や総攻撃へcommitした兵力を、次turnの別判断で誤って再配置しません。

異なる位置の兵力を同時に到着させるため、path長に合わせた`holdTurns`をsimulationへ反映します。攻撃対象が変わっても、集結地点と目的を追跡し、投入済みの兵力、追加訓練、移動costを重複計上しないようにしました。

#### 4. 公式ルールを再現したlocal judge

1つのmapや開始sideで勝っただけでは、戦略改善を判断できません。`judge.py`では、規則書に基づき次を実装しました。

- point symmetryを持つmapの生成
- 2段階のVoronoi centroid relaxation
- Delaunay/Voronoi ridgeに基づくgraph edge
- 建設、移動、訓練、戦闘、income、upkeepを最大200 turn実行
- seed、LEFT/RIGHT side、mapを変えた並列対戦
- CSV log、JSON replay、self-contained HTML viewer
- round-robin / Swiss tournament

`pool/`には19個のC++候補戦略を保存しています。同じseed条件でsideを入れ替えながら対戦させ、win countとscoreを比較し、特定のopponentやmapにだけ有効な変更を除外した後でfinal agentへ反映しました。

> `judge.py`は規則書を可能な限り再現したlocal simulatorであり、公式judge binaryではありません。post-relaxation centroidの整数化など、protocol上必要な差分はsource headerに明記しています。

### Repository layout

| Path | Role |
|---|---|
| `main.c++` | 予選の最終heuristic agent |
| `judge.py` | 予選用game engine、map generation、batch evaluation、tournament |
| `GameRules.md` | 予選ルールのreference |
| `pool/*.cpp` | 比較に使用した19個の予選候補strategy |
| `replay_bot.cpp` | 予選のreplay/debug helper agent |
| `viewer_template.html` | 予選のself-contained replay viewer template |
| `NEXT_VISION/master_main.cpp` | Final Round agentの基準source |
| `NEXT_VISION/judge.py` | Final Roundの視野とprotocolを実装したlocal judge |
| `NEXT_VISION/README.md` | Final Roundのbuild・replay・analysis guide |

### 実行方法

Requirements:

- Python 3.11+
- `numpy`, `scipy`, `tqdm`
- `g++` with C++17 support

2つのsourceを100 gameで比較します。sourceは必要に応じて自動compileされます。

```bash
python judge.py main.c++ pool/110584.cpp --games 100 --workers 8 --seed 42
```

固定mapや相手commandを再現するdebug mode:

```bash
python judge.py main.c++ pool/110584.cpp --map-file <log-or-map-file>
python judge.py main.c++ --replay <match-log> --replay-side B
```

candidate poolをround-robinで比較する場合は、先にexecutableを用意します。

```bash
for f in pool/*.cpp; do g++ -O2 -std=c++17 -o "${f%.cpp}.exe" "$f"; done
python judge.py tournament pool --mode roundrobin --games 5 --workers 8 --seed 42
```

Final Round agentのbuildとlocal match:

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  NEXT_VISION/master_main.cpp -o NEXT_VISION/master_main
python3 NEXT_VISION/judge.py \
  NEXT_VISION/master_main.cpp NEXT_VISION/sample_ai_imitation.cpp \
  --games 1 --seed 42
```

---

## English

### Result and problem

This is my solo entry for the NYPC 2026 Master Track. It ranked **19th among 1,603 teams in the qualifier (top ~1.2%)** and advanced to the Final Round.

The repository root contains the qualifier agent and evaluation environment. The Final Round used different map constraints, resources, fog of war, and interaction protocol, so its implementation lives separately in [`NEXT_VISION/`](NEXT_VISION/README.md). The canonical finals source is `NEXT_VISION/master_main.cpp`, with `NEXT_VISION/judge.py` as its local simulator.

Two agents move warriors across a graph battlefield, build and upgrade bases, train units at their headquarters, and try to destroy the opposing HQ. Every turn requires a joint decision over routes, gold, future income, upkeep, reinforcements, and combat.

I implemented the C++17 heuristic agent and a Python evaluation environment containing the game engine, map generator, replay viewer, batch runner, and candidate tournament.

### Engineering highlights

#### Graph planning and economy

Floyd-Warshall precomputes all-pairs weighted distance, next hop, and hop count for the 51–109 region graph. Expansion decisions compare travel/build time and remaining-turn income against movement, construction, upgrade, and upkeep cost; bases that cannot repay their cost before the 200-turn limit are excluded.

#### Turn-by-turn combat simulation

`assaultOutcome` simulates arrivals, building and turret damage, unit combat, gold, income, upgrades, and additional training. `plan_attack_force` searches for the minimum winning force and adds a safety margin. The inverse defensive simulation estimates how many units a threatened base must retain.

#### Persistent intent across turns

Each warrior retains one of `NONE`, `MOVE`, `BUILD`, or `ATTACK`. Already committed units are therefore not accidentally reassigned by the next turn's heuristic. `holdTurns` synchronizes forces coming from paths of different lengths, while committed units and future training are accounted for only once.

#### Reproducible local evaluation

The local judge reproduces the written rules, including symmetric map generation, two Voronoi centroid-relaxation passes, Delaunay/ridge adjacency, and up to 200 turns of build/move/train/combat/economy. It varies map seeds and starting sides, emits CSV/JSON/HTML replays, and supports parallel head-to-head, round-robin, and Swiss evaluation.

Nineteen candidate agents in `pool/` were compared under repeated conditions before changes were integrated into `main.c++`.

### Quick start

```bash
# Qualifier: compare two sources; they are compiled automatically when needed
python judge.py main.c++ pool/110584.cpp --games 100 --workers 8 --seed 42

# Final Round: build the canonical agent
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  NEXT_VISION/master_main.cpp -o NEXT_VISION/master_main

# Final Round: run a reproducible local match
python3 NEXT_VISION/judge.py \
  NEXT_VISION/master_main.cpp NEXT_VISION/sample_ai_imitation.cpp \
  --games 1 --seed 42
```

See the Korean and Japanese sections above for the repository layout, debugging and tournament commands, and simulator caveats. Finals-specific replay and analysis instructions are in [`NEXT_VISION/README.md`](NEXT_VISION/README.md).
