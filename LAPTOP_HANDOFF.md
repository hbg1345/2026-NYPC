# 노트북 작업 인계

## 현재 기준

- 채택된 최고판 wrapper:
  `summission_result/submission/NEXT NATION/110584_highest_attack_plan_margin3.cpp`
- 전략 본체:
  `summission_result/submission/NEXT NATION/110584_territory_push_adaptive_reinforce_highest_skip_enemy_preclaim.cpp`
- 고정 pool 1,900판: 1462승 395무 43패, 1659.5점(87.3421%)
- 기준 결과:
  `highest_attack_plan_margin3_vs_fixed_pool_100maps.csv`

## 마지막 비채택 실험

- wrapper:
  `summission_result/submission/NEXT NATION/110584_highest_mandatory_pre_reinforcement_base_race.cpp`
- 내용: 필수 탈환에서 상대 최단 증원 전에 BASE를 파괴하고 이탈하는 양수
  교환 분기. 집결지 노동자 한 명 동원과 상대 HQ 반응 생산 ETA까지 포함.
- 결과: 1390승 448무 62패, 1614점(84.9474%)으로 비채택.
- 결과 CSV:
  `highest_mandatory_pre_reinforcement_base_race_vs_fixed_pool_100maps.csv`
- 문제 맵 리플레이/로그:
  `mandatory_base_race_vs_83284_pretest28_map1558_battle233427/`

## 빌드와 검증

MSYS2 UCRT64의 `g++`가 PATH에 있다면 다음처럼 실행한다.

```powershell
g++ -std=c++17 -O2 -pipe -s `
  "summission_result/submission/NEXT NATION/110584_highest_attack_plan_margin3.cpp" `
  -o current_highest.exe

python evaluate_all_models.py `
  --candidate current_highest.exe `
  --models-dir pool `
  --logs-root "summission_result/tournament" `
  --sample-unique-maps 100 --map-seed 42 `
  --candidate-side left `
  --compile-workers 16 --game-workers 16 `
  --output laptop_check.csv
```

맵은 좌우 대칭으로 검증했으므로 고정 비교는 `--candidate-side left`를 쓴다.
변경 이력과 실험 결과는 `110584_strategy_experiments.md`에서 이어서 기록한다.
