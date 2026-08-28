#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// 실험 wrapper에서만 덮어쓰는 필수 거점 첫 출격 옵션. 기본값은 80.79%
// 최고 후보의 동작을 그대로 보존한다.
#ifndef MANDATORY_RECLAIM_INITIAL_MARGIN
#define MANDATORY_RECLAIM_INITIAL_MARGIN 1
#endif
#ifndef INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM
#define INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM 0
#endif
#ifndef REINFORCE_RESPONSE_LOOKAHEAD
#define REINFORCE_RESPONSE_LOOKAHEAD 3
#endif
#ifndef REINFORCE_CAPTURE_SURVIVOR_MARGIN
#define REINFORCE_CAPTURE_SURVIVOR_MARGIN 0
#endif
#ifndef SHARED_MANDATORY_DEFICIT_RECLAIM
#define SHARED_MANDATORY_DEFICIT_RECLAIM 0
#endif
#ifndef MANDATORY_COUNT_DEFICIT_RECLAIM
#define MANDATORY_COUNT_DEFICIT_RECLAIM 0
#endif
#ifndef ALL_BASES_RALLY_SIM
#define ALL_BASES_RALLY_SIM 0
#endif
#ifndef EXCLUDE_OUTERMOST_RALLY_BASE
#define EXCLUDE_OUTERMOST_RALLY_BASE 0
#endif
#ifndef RESELECT_OPTIONAL_PUSH_BEFORE_LAUNCH
#define RESELECT_OPTIONAL_PUSH_BEFORE_LAUNCH 0
#endif
#ifndef DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
#define DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY 0
#endif
#ifndef DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
#define DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY 0
#endif
#ifndef DYNAMIC_OPTIONAL_PUSH_HQ_CENTER_RALLY
#define DYNAMIC_OPTIONAL_PUSH_HQ_CENTER_RALLY 0
#endif
#ifndef DYNAMIC_OPTIONAL_PUSH_CENTER_RALLY
#define DYNAMIC_OPTIONAL_PUSH_CENTER_RALLY 0
#endif
#ifndef DIRECT_ACTIVE_FIELD_RECLAIM
#define DIRECT_ACTIVE_FIELD_RECLAIM 0
#endif
#ifndef TRAIN_HQ_LABOR_INSTEAD_OF_TRANSFER
#define TRAIN_HQ_LABOR_INSTEAD_OF_TRANSFER 0
#endif
#ifndef RESELECT_OPTIONAL_PUSH_AT_LAUNCH
#define RESELECT_OPTIONAL_PUSH_AT_LAUNCH 0
#endif
#ifndef PRECISE_OPTIONAL_PUSH_AT_LAUNCH
#define PRECISE_OPTIONAL_PUSH_AT_LAUNCH 0
#endif
#ifndef PRECISE_ALL_OFFENSE_AT_LAUNCH
#define PRECISE_ALL_OFFENSE_AT_LAUNCH 0
#endif
#ifndef MANDATORY_MARGIN_TWO_WHEN_BASE_AHEAD
#define MANDATORY_MARGIN_TWO_WHEN_BASE_AHEAD 0
#endif
#ifndef MANDATORY_MARGIN_TWO_UNMATCHED_ONLY
#define MANDATORY_MARGIN_TWO_UNMATCHED_ONLY 0
#endif
#ifndef MANDATORY_MARGIN_TWO_AT_LAUNCH
#define MANDATORY_MARGIN_TWO_AT_LAUNCH 0
#endif
#ifndef MANDATORY_RECLAIM_AHEAD_MARGIN
#define MANDATORY_RECLAIM_AHEAD_MARGIN 2
#endif
#ifndef BASE_REINFORCEMENT_RACE_LOOKUP
#define BASE_REINFORCEMENT_RACE_LOOKUP 0
#endif
#ifndef BASE_RACE_BORROW_STAGING_WORKER
#define BASE_RACE_BORROW_STAGING_WORKER 0
#endif
#ifndef BASE_RACE_INCLUDE_REACTIVE_HQ_TRAIN
#define BASE_RACE_INCLUDE_REACTIVE_HQ_TRAIN 0
#endif
#ifndef PROFITABLE_PRE_REINFORCEMENT_RAID
#define PROFITABLE_PRE_REINFORCEMENT_RAID 0
#endif
#ifndef PROFITABLE_FAILED_GARRISON_TRADE
#define PROFITABLE_FAILED_GARRISON_TRADE 0
#endif
#ifndef HOLD_EMPTY_CAPTURE_AGAINST_INCOMING
#define HOLD_EMPTY_CAPTURE_AGAINST_INCOMING 0
#endif
#ifndef HOLD_WON_EMPTY_STRONGHOLD
#define HOLD_WON_EMPTY_STRONGHOLD 0
#endif
#ifndef PRECISE_DEFENSE_CURRENT_HP
#define PRECISE_DEFENSE_CURRENT_HP 0
#endif
#ifndef ANTICIPATE_MANDATORY_PRECLAIM
#define ANTICIPATE_MANDATORY_PRECLAIM 0
#endif
#ifndef REINFORCE_FUTURE_EMPTY_STRONGHOLD_BATTLE
#define REINFORCE_FUTURE_EMPTY_STRONGHOLD_BATTLE 0
#endif
#ifndef PRECISE_MANDATORY_RECLAIM_AT_LAUNCH
#define PRECISE_MANDATORY_RECLAIM_AT_LAUNCH 0
#endif
#ifndef SNAPSHOT_MANDATORY_MOVERS_AT_LAUNCH
#define SNAPSHOT_MANDATORY_MOVERS_AT_LAUNCH 0
#endif
#ifndef SNAPSHOT_MANDATORY_NEAREST_RESERVE_AT_LAUNCH
#define SNAPSHOT_MANDATORY_NEAREST_RESERVE_AT_LAUNCH 0
#endif
#ifndef BLOCK_OPTIONAL_PUSH_ROUTE_COLLISION
#define BLOCK_OPTIONAL_PUSH_ROUTE_COLLISION 0
#endif
#ifndef RELEASE_INFEASIBLE_MANDATORY_RECLAIM
#define RELEASE_INFEASIBLE_MANDATORY_RECLAIM 0
#endif
#ifndef PRETRAIN_SAFE_OFFENSIVE_RESERVE
#define PRETRAIN_SAFE_OFFENSIVE_RESERVE 0
#endif
#ifndef SAFE_OFFENSIVE_RESERVE_TARGET
#define SAFE_OFFENSIVE_RESERVE_TARGET 1
#endif
#ifndef DISABLE_MANDATORY_TERRITORY
#define DISABLE_MANDATORY_TERRITORY 0
#endif
#ifndef DISABLE_FORCED_MANDATORY_RECLAIM
#define DISABLE_FORCED_MANDATORY_RECLAIM 0
#endif
#ifndef SKIP_FORCED_MANDATORY_RECLAIM_WHEN_BASE_AHEAD
#define SKIP_FORCED_MANDATORY_RECLAIM_WHEN_BASE_AHEAD 0
#endif
#ifndef PARALLEL_EXPANSION_DURING_MANDATORY_RECLAIM
#define PARALLEL_EXPANSION_DURING_MANDATORY_RECLAIM 0
#endif
#ifndef DEFER_MANDATORY_RECLAIM_WHILE_NEUTRAL_MANDATORY
#define DEFER_MANDATORY_RECLAIM_WHILE_NEUTRAL_MANDATORY 0
#endif
#ifndef DEFER_MANDATORY_RECLAIM_OPP_BASE_MAX
#define DEFER_MANDATORY_RECLAIM_OPP_BASE_MAX 9999
#endif
#ifndef FORCED_MANDATORY_RECLAIM_OPP_BASE_MAX
#define FORCED_MANDATORY_RECLAIM_OPP_BASE_MAX 9999
#endif

#if BASE_REINFORCEMENT_RACE_LOOKUP
#include "base_combat_lookup_table.inc"
#endif

constexpr int MAX_TURN = 200;         // maximum turn (days)
constexpr int START_GOLD = 500;       // initial gold
constexpr int START_WARRIORS = 3;     // initial warriors
constexpr int MOVE_COST = 10;         // move cost
constexpr int TRAIN_COST = 120;       // train cost
constexpr int WORK_INCOME = 15;       // income per warrior
constexpr int UPKEEP_PER_WARRIOR = 2; // upkeep per warrior
constexpr int HQ_MAX_LEVEL = 5;       // HQ max level
constexpr int BASE_MAX_LEVEL = 3;     // base max level
constexpr int HQ_HEAL_COST = 1000;    // HQ fix cost
constexpr int BASE_HEAL_COST = 500;   // base fix cost

struct HqLevelEntry {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
};

struct BaseLevelEntry {
  int cost;
  int hp;
  int turret;
  int work_cap;
};

constexpr HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1200, 6, 20, 2, 2, 3}, {2400, 7, 25, 3, 2, 4}, {3600, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {300, 6, 1, 1},
    {600, 12, 1, 2},
    {1000, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };
// 이 유닛을 왜 보냈는지: 단순 이동/지원인지, 빈 거점 건설을 위한 파견인지,
// 총공세(집결/돌격)를 위한 파견인지. ATTACK은 중간에 어느 경유지를
// 거치는지와 무관하게 "이 작전에 이미 커밋된 병력"임을 표시하는 용도라,
// pickWaypoint가 매 턴 다른 경유지를 골라도 정체성이 유지된다.
enum class WPurpose : int { NONE, MOVE, BUILD, ATTACK };

inline Side opposite(Side s) {
  return s == Side::LEFT ? Side::RIGHT : Side::LEFT;
}
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) {
  return c == 'A' ? Side::LEFT : Side::RIGHT;
}

struct WarriorId {
  Side side = Side::LEFT;
  int num = 0;
  bool operator==(const WarriorId &o) const {
    return side == o.side && num == o.num;
  }
};

struct Warrior {
  WarriorId id;
  int region = 0;
  int hp = 0;
  WState state = WState::STATIONARY;
  int target = 0;
  WPurpose purpose = WPurpose::NONE; // 이전 턴 결정으로 생긴 부채(용도)
  int prev_region = -1; // 직전 턴의 위치(상대 이동 방향으로 목표 거점을 추정하는 데 사용, -1이면 이동 이력 없음)
  bool move_pending = false; // 상대의 미청구 이동. 멈춘 위치가 아군 건물이 아니면 그때 MOVE_COST를 청구한다.
};

struct Building {
  int region = 0;
  Side side = Side::LEFT;
  BType type = BType::HQ;
  int level = 1;
  int hp = 10;

  int current_hp() const {
    return type == BType::HQ ? HQ_LEVELS[level].hp : BASE_LEVELS[level].hp;
  }
  int work_cap() const {
    return type == BType::HQ ? HQ_LEVELS[level].work_cap
                             : BASE_LEVELS[level].work_cap;
  }
};

struct GameMap {
  int N = 0, K = 0;
  std::vector<long long> x, y;
  std::vector<int> strongholds;
  std::vector<std::vector<int>> adj;

  Side my_side = Side::LEFT;
  int my_hq = 0;
  int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD;     // current gold
  int opp_gold = START_GOLD; // 상대 골드 추정치 (상대의 공개된 행동/수입으로 역산)
  int my_countdown = 5;      // my remaining countdowns
  int opp_countdown = 5;     // opponent's remaining countdowns
  std::vector<Warrior> warriors;
  std::vector<Building> buildings;
};

struct MoveOrder {
  WarriorId id;
  int target = 0;
  WPurpose purpose = WPurpose::NONE;
};

struct Actions {
  int train_n = 0;
  std::vector<MoveOrder> moves;
  std::vector<int> upgrades;
};

// 현재 배치를 그대로 얼린 채, 남은 턴 동안 양측이 HQ 업그레이드(만렙 HQ는
// 필요할 때 수리)만 한다고 가정했을 때의 200턴 판정 결과. 실제 이동 중인
// 병력도 이 지표 안에서는 현재 지역에 고정한다. 상대 병력의 최종 목적지는
// 관측할 수 없으므로, 이동을 임의로 추정하지 않고 순수한 경제/HQ 경쟁만
// 비교하기 위한 의도적인 모델링이다.
enum class PassiveHqVerdict : int { LOSS = -1, DRAW = 0, WIN = 1 };

struct PassiveHqResult {
  PassiveHqVerdict verdict = PassiveHqVerdict::DRAW;
  int my_final_level = 0;
  int opp_final_level = 0;
  int my_final_hp = 0;
  int opp_final_hp = 0;
  int my_final_gold = 0;
  int opp_final_gold = 0;
};

static std::string readln() {
  std::string s;
  if (!std::getline(std::cin, s))
    std::exit(0);
  return s;
}

static std::vector<std::string> tokens(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream is(s);
  for (std::string t; is >> t;)
    out.push_back(t);
  return out;
}

static WarriorId parse_warrior(const std::string &tok) {
  assert(!tok.empty() && (tok[0] == 'A' || tok[0] == 'B'));
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = std::stoi(tok.substr(1));
  return id;
}

static std::string format_warrior(WarriorId id) {
  std::string s;
  s.push_back(side_char(id.side));
  s += std::to_string(id.num);
  return s;
}

// ===== 디버깅 로그 (리플레이 분석용) =====
// 아래 DEBUG_LOG를 true로 두면 이동 판단 이유를 파일에 기록한다. false면
// 모든 함수가 즉시 반환하는 완전한 no-op이라 파일 I/O가 전혀 안 생긴다.
// 제출할 땐 false로 바꿔서 빌드하면 된다. 좌/우 봇이 같은 리플레이에서 서로
// 파일을 덮어쓰지 않도록 파일명에 진영 문자(_A/_B)를 붙인다.
namespace dbg {
constexpr bool DEBUG_LOG = true;                 // <-- 여기만 켜고/끄면 됨
constexpr const char *LOG_PATH = "debug.txt";    // 실제로는 debug_A.txt / debug_B.txt

static std::ofstream g_log;
static bool g_on = false;

inline const char *purpose_name(WPurpose p) {
  switch (p) {
    case WPurpose::MOVE: return "MOVE";
    case WPurpose::BUILD: return "BUILD";
    case WPurpose::ATTACK: return "ATTACK";
    default: return "NONE";
  }
}

static void init(Side mySide) {
  if (!DEBUG_LOG) return;
  std::string path(LOG_PATH);
  std::string tag = std::string("_") + side_char(mySide);
  auto dot = path.find_last_of('.');
  auto slash = path.find_last_of("/\\");
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    path.insert(dot, tag); // "debug.txt" -> "debug_A.txt"
  else
    path += tag;           // 확장자 없으면 그냥 뒤에 붙임
  g_log.open(path, std::ios::out | std::ios::trunc);
  g_on = g_log.is_open();
}

static void turn_header(int turn, const GameState &S, const GameMap &M) {
  if (!g_on) return;
  int mine = 0, opp = 0;
  for (const auto &w : S.warriors)
    (w.id.side == M.my_side ? mine : opp)++;
  g_log << "===== TURN " << turn << " | gold=" << S.gold
        << " opp_gold~=" << S.opp_gold << " | warriors=" << mine
        << " opp=" << opp << " =====\n";
  g_log.flush();
}

static void move(int turn, WarriorId id, int from, int to, WPurpose purpose,
                 const std::string &reason) {
  if (!g_on) return;
  g_log << "T" << turn << "  MOVE " << format_warrior(id) << ' ' << from << "->"
        << to << "  [" << purpose_name(purpose) << "]  " << reason << '\n';
  g_log.flush();
}

static void note(int turn, const std::string &reason) {
  if (!g_on) return;
  g_log << "T" << turn << "  " << reason << '\n';
  g_log.flush();
}
} // namespace dbg

static int hq_of(const GameMap &M, Side s) {
  return (s == Side::LEFT) ? 0 : M.N - 1;
}

static Building make_base(int region, Side s) {
  return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp};
}

static void apply_upgrade(Building &b) {
  b.level += 1;
  b.hp = b.current_hp();
}

static int upgrade_cost(const Building &b) {
  if (b.type == BType::HQ)
    return HQ_LEVELS[b.level + 1].upgrade_cost;
  else
    return BASE_LEVELS[b.level + 1].cost;
}

static int max_level(const Building &b) {
  return b.type == BType::HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}

static PassiveHqResult evaluate_passive_hq_race(const GameState &S,
                                                 const GameMap &M,
                                                 int turn) {
  struct SimWarrior {
    int num = 0;
    int region = 0;
    int hp = 0;
  };
  struct SimSide {
    Side side = Side::LEFT;
    int gold = 0;
    int hq_region = 0;
    int hq_level = 0;
    int hq_hp = 0;
    std::vector<SimWarrior> warriors;
  };

  auto make_side = [&](Side side, int gold) {
    SimSide out;
    out.side = side;
    out.gold = std::max(0, gold);
    out.hq_region = hq_of(M, side);
    for (const auto &b : S.buildings) {
      if (b.side == side && b.type == BType::HQ &&
          b.region == out.hq_region) {
        out.hq_level = b.level;
        out.hq_hp = b.hp;
        break;
      }
    }
    for (const auto &w : S.warriors) {
      if (w.id.side == side && w.hp > 0)
        out.warriors.push_back({w.id.num, w.region, w.hp});
    }
    std::sort(out.warriors.begin(), out.warriors.end(),
              [](const SimWarrior &a, const SimWarrior &b) {
                return a.num < b.num;
              });
    return out;
  };

  SimSide me = make_side(M.my_side, S.gold);
  SimSide opp = make_side(opposite(M.my_side), S.opp_gold);

  auto count_at = [](const SimSide &side, int region) {
    int count = 0;
    for (const auto &w : side.warriors)
      if (w.region == region) ++count;
    return count;
  };

  // 실제 UPGRADE 명령과 같은 합법성 조건이다. HQ에 자기 병력이 하나 이상
  // 있고 상대 병력은 없어야 한다. 정적 롤아웃이므로 병력이 유지비로
  // 사망하지 않는 한 이 조건도 그대로 유지된다.
  auto can_touch_hq = [&](const SimSide &side, const SimSide &enemy) {
    return side.hq_level > 0 && count_at(side, side.hq_region) > 0 &&
           count_at(enemy, side.hq_region) == 0;
  };

  auto upgrade_or_repair_hq = [](SimSide &side, bool legal) {
    if (!legal || side.hq_level <= 0) return;
    if (side.hq_level < HQ_MAX_LEVEL) {
      int cost = HQ_LEVELS[side.hq_level + 1].upgrade_cost;
      if (side.gold >= cost) {
        side.gold -= cost;
        ++side.hq_level;
        side.hq_hp = HQ_LEVELS[side.hq_level].hp;
      }
      return;
    }
    int maxHp = HQ_LEVELS[side.hq_level].hp;
    if (side.hq_hp < maxHp && side.gold >= HQ_HEAL_COST) {
      side.gold -= HQ_HEAL_COST;
      side.hq_hp = maxHp;
    }
  };

  auto resolve_income_and_upkeep = [&](SimSide &side) {
    int income = 0;
    for (const auto &b : S.buildings) {
      if (b.side != side.side) continue;
      int workers = count_at(side, b.region);
      int cap = b.type == BType::HQ
                    ? (side.hq_level > 0
                           ? HQ_LEVELS[side.hq_level].work_cap
                           : 0)
                    : BASE_LEVELS[b.level].work_cap;
      income += WORK_INCOME * std::min(workers, cap);
    }
    side.gold += income;

    // 실제 엔진과 동일하게 번호가 작은 병력부터 유지비를 지불한다. 돈이
    // 모자란 병력은 체력을 1 잃고, 0이 되면 다음 날 수입/유지비에서 빠진다.
    for (auto &w : side.warriors) {
      if (side.gold >= UPKEEP_PER_WARRIOR)
        side.gold -= UPKEEP_PER_WARRIOR;
      else
        --w.hp;
    }
    side.warriors.erase(
        std::remove_if(side.warriors.begin(), side.warriors.end(),
                       [](const SimWarrior &w) { return w.hp <= 0; }),
        side.warriors.end());
  };

  // decide()는 turn일의 명령을 내기 직전 호출되므로 현재 턴도 남은 턴에
  // 포함한다. 업그레이드 -> 수입 -> 유지비 순서는 실제 엔진과 같다.
  for (int day = std::max(0, turn); day < MAX_TURN; ++day) {
    bool myLegal = can_touch_hq(me, opp);
    bool oppLegal = can_touch_hq(opp, me);
    upgrade_or_repair_hq(me, myLegal);
    upgrade_or_repair_hq(opp, oppLegal);
    resolve_income_and_upkeep(me);
    resolve_income_and_upkeep(opp);
  }

  PassiveHqResult result;
  result.my_final_level = me.hq_level;
  result.opp_final_level = opp.hq_level;
  result.my_final_hp = me.hq_hp;
  result.opp_final_hp = opp.hq_hp;
  result.my_final_gold = me.gold;
  result.opp_final_gold = opp.gold;
  if (me.hq_hp > opp.hq_hp)
    result.verdict = PassiveHqVerdict::WIN;
  else if (me.hq_hp < opp.hq_hp)
    result.verdict = PassiveHqVerdict::LOSS;
  else
    result.verdict = PassiveHqVerdict::DRAW;
  return result;
}

static const char *passive_hq_verdict_name(PassiveHqVerdict verdict) {
  switch (verdict) {
    case PassiveHqVerdict::WIN: return "WIN";
    case PassiveHqVerdict::LOSS: return "LOSS";
    default: return "DRAW";
  }
}

static void parse_init(GameMap &M, GameState &S) {
  {
    auto t = tokens(readln());
    assert(t.size() >= 2 && t[0] == "READY");
    M.my_side = (t[1] == "LEFT") ? Side::LEFT : Side::RIGHT;
  }
  {
    auto t = tokens(readln());
    M.N = std::stoi(t.at(0));
    M.K = std::stoi(t.at(1));
  }
  M.x.assign(M.N, 0);
  M.y.assign(M.N, 0);
  {
    auto t = tokens(readln()); // x_0 x_1 ... x_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.x[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // y_0 y_1 ... y_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.y[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // K strongholds
    M.strongholds.clear();
    M.strongholds.reserve(t.size());
    for (const auto &s : t)
      M.strongholds.push_back(std::stoi(s));
    std::sort(M.strongholds.begin(), M.strongholds.end());
  }
  M.adj.assign(M.N, {});
  for (int r = 0; r < M.N; ++r) {
    auto t = tokens(readln()); // deg n_1 n_2 ...
    int deg = std::stoi(t.at(0));
    auto &nb = M.adj[r];
    nb.reserve(deg);
    for (int j = 0; j < deg; ++j)
      nb.push_back(std::stoi(t.at(1 + j)));
    std::sort(nb.begin(), nb.end());
  }

  M.my_hq = hq_of(M, M.my_side);
  M.opp_hq = hq_of(M, opposite(M.my_side));

  S = GameState{};
  S.gold = START_GOLD;
  S.opp_gold = START_GOLD;
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx},
                                 .region = M.my_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx},
                                 .region = M.opp_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1,
                                 HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ,
                                 1, HQ_LEVELS[1].hp});

  std::cout << "OK" << std::endl;
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln();
  if (line == "FINISH")
    return false;
  auto t = tokens(line);
  assert(!t.empty() && t[0] == "START");
  turn_index = std::stoi(t.at(2));
  return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings)
    if (b.region == region)
      return &b;
  return nullptr;
}

static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors)
    if (w.id == id)
      return &w;
  return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M,
                             const Actions &submitted) {
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) {
      S.gold -= BASE_LEVELS[1].cost;
      S.buildings.push_back(make_base(region, M.my_side));
      // 건설이 실제로 완료된 시점에야 부채가 상환됨
      for (auto &w : S.warriors)
        if (w.id.side == M.my_side && w.region == region &&
            w.purpose == WPurpose::BUILD)
          w.purpose = WPurpose::NONE;
    } else {
      if (b->level >= max_level(*b)) {
        int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
        b->hp = b->current_hp();
      } else {
        S.gold -= upgrade_cost(*b);
        apply_upgrade(*b);
      }
    }
  }

  for (const auto &mv : submitted.moves) {
    Building *b = find_building(S, mv.target);
    int cost = (b != nullptr && b->side == M.my_side) ? 0 : MOVE_COST;
    S.gold -= cost;
    if (Warrior *w = find_warrior(S, mv.id)) {
      w->state = WState::MOVING;
      w->target = mv.target;
      w->purpose = mv.purpose;
    }
  }

  S.gold -= TRAIN_COST * submitted.train_n;

  {
    std::string line = readln();
    if (line == "FINISH")
      std::exit(0);
    auto t = tokens(line);
    assert(!t.empty() && t[0] == "TURN");
  }
  {
    auto t = tokens(readln());
    S.my_countdown = std::stoi(t.at(2));
    S.opp_countdown = std::stoi(t.at(4));
  }
  // UPGRADE
  {
    auto t = tokens(readln()); // "UPGRADE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<A|B> <region>"
      Side s = parse_side_char(r.at(0)[0]);
      int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (b == nullptr) {
        if (s != M.my_side) S.opp_gold -= BASE_LEVELS[1].cost;
        S.buildings.push_back(make_base(region, s));
      } else if (b->side != M.my_side) {
        if (b->level >= max_level(*b)) {
          int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
          S.opp_gold -= cost;
          b->hp = b->current_hp();
        } else {
          S.opp_gold -= upgrade_cost(*b);
          apply_upgrade(*b);
        }
      }
    }
  }
  // TRAIN
  {
    auto t = tokens(readln()); // "TRAIN N"
    int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i));
        if (id.side != M.my_side) S.opp_gold -= TRAIN_COST;
        int hq_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id,
                                     .region = hq_region,
                                     .hp = HQ_LEVELS[hq_level].warrior_hp});
      }
    }
  }
  // MOVE
  {
    auto t = tokens(readln()); // "MOVE N"
    int n = std::stoi(t.at(1));
    std::vector<WarriorId> moved;
    moved.reserve(n);
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(0));
      int region = std::stoi(r.at(1));
      moved.push_back(id);
      if (Warrior *w = find_warrior(S, id)) {
        // 상대 유닛이 이전에 정지 상태였다면 이번 이동은 새로 낸 명령이므로
        // 비용이 든다. 이미 이동 중이었다면 자동 진행일 뿐 추가 비용 없음.
        if (id.side != M.my_side && w->state == WState::STATIONARY) {
          // 이동비는 최종 목적지 기준(아군 건물이면 무료)인데 상대의 목적지는
          // 볼 수 없다(MOVE 이벤트는 한 칸 경유지만 준다). 청구를 미뤄두고,
          // 이 전사가 멈춘 위치(=목적지)를 보고 아래 도착 처리에서 정산한다.
          w->move_pending = true;
        }
        w->prev_region = w->region; // 이번 이동 직전 위치(방향 추정용)
        w->region = region;
        w->state = WState::MOVING;
        if (id.side == M.my_side && w->region == w->target) {
          w->state = WState::STATIONARY;
          // purpose는 여기서 지우지 않음: BUILD 목적이면 실제로 건설이
          // 완료될 때까지(위 UPGRADE 처리 지점) 부채가 남아있어야 함
        }
      }
    }
    // 이번 턴에 위치가 안 바뀐(=목록에 없는) 상대 유닛은 도착해서 멈춘 것
    for (auto &w : S.warriors) {
      if (w.id.side == M.my_side) continue;
      bool didMove = false;
      for (const auto &id : moved)
        if (id == w.id) { didMove = true; break; }
      if (!didMove) {
        // 이번 턴 MOVE 목록에 없던 상대 전사 = 더 안 움직임 → 목적지 도착해 멈춤.
        w.state = WState::STATIONARY;
        if (w.move_pending) {
          // 멈춘 위치(=목적지)가 아군 건물이 아니면 이동비 발생, 아군 건물이면 무료.
          Building *tb = find_building(S, w.region);
          if (tb == nullptr || tb->side != w.id.side)
            S.opp_gold -= MOVE_COST;
          w.move_pending = false;
        }
      }
    }
  }
  // DAMAGE
  {
    auto t = tokens(readln()); // "DAMAGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Warrior *w = find_warrior(S, id))
        w->hp -= damage;
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [](const Warrior &w) { return w.hp <= 0; }),
                     S.warriors.end());
  }
  // SIEGE
  {
    auto t = tokens(readln()); // "SIEGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      int region = std::stoi(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Building *b = find_building(S, region))
        b->hp -= damage;
    }
    S.buildings.erase(
        std::remove_if(S.buildings.begin(), S.buildings.end(),
                       [](const Building &b) { return b.hp <= 0; }),
        S.buildings.end());
  }
  (void)readln(); // "END"

  int income = 0, opp_income = 0;
  for (const auto &b : S.buildings) {
    int count = 0;
    for (const auto &w : S.warriors)
      if (w.id.side == b.side && w.region == b.region) ++count;
    int add = WORK_INCOME * std::min(count, b.work_cap());
    if (b.side == M.my_side) income += add;
    else opp_income += add;
  }
  S.gold += income;
  S.opp_gold += opp_income;

  int alive = 0, opp_alive = 0;
  for (const auto &w : S.warriors)
    (w.id.side == M.my_side ? alive : opp_alive)++;
  S.gold = std::max(0, S.gold - UPKEEP_PER_WARRIOR * alive);
  S.opp_gold = std::max(0, S.opp_gold - UPKEEP_PER_WARRIOR * opp_alive);
}

struct Paths {
  std::vector<std::vector<double>> dist;
  std::vector<std::vector<int>> nxt;
  std::vector<std::vector<int>> hops; // 실제 이동 규칙상 간선 1개 = 1턴이므로, 이동에 걸리는
                                       // 정확한 턴 수는 이 hop 수다(가중치 거리는 nxt가 어느
                                       // 경로를 따를지 정하는 데만 쓰인다).
};

static double euclid_ceil(const GameMap &M, int u, int v) {
  double dx = (double)(M.x[u] - M.x[v]);
  double dy = (double)(M.y[u] - M.y[v]);
  return std::ceil(std::sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap &M) {
  const double INF = std::numeric_limits<double>::infinity();
  Paths P;
  P.dist.assign(M.N, std::vector<double>(M.N, INF));
  P.nxt.assign(M.N, std::vector<int>(M.N, -1));

  for (int i = 0; i < M.N; ++i) {
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < M.N; ++u) {
    for (int v : M.adj[u]) {
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < M.N; ++k) {
    for (int u = 0; u < M.N; ++u) {
      if (P.dist[u][k] == INF)
        continue;
      for (int v = 0; v < M.N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.dist[u][v] == INF)
        continue;
      double best_score = INF;
      for (int nb : M.adj[u]) {
        if (P.dist[nb][v] == INF)
          continue;
        double score = euclid_ceil(M, u, nb) + P.dist[nb][v];
        if (score < best_score) {
          best_score = score;
          P.nxt[u][v] = nb;
        }
      }
    }
  }

  P.hops.assign(M.N, std::vector<int>(M.N, -1));
  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (P.nxt[u][v] == -1)
        continue;
      int c = 0, cur = u;
      while (cur != v) {
        cur = P.nxt[cur][v];
        ++c;
      }
      P.hops[u][v] = c;
    }
  }
  return P;
}

static int next_step(const Paths &P, int u, int v) { return P.nxt[u][v]; }

static std::vector<int> path(const Paths &P, int u, int v) {
  std::vector<int> out;
  if (P.nxt[u][v] == -1)
    return out;
  out.push_back(u);
  while (u != v) {
    u = P.nxt[u][v];
    out.push_back(u);
  }
  return out;
}

static void emit_command() { std::cout << "COMMAND\n"; }

static void emit_actions(const Actions &a) {
  for (const auto &mv : a.moves) {
    std::cout << "MOVE " << format_warrior(mv.id) << ' ' << mv.target << '\n';
  }
  for (int r : a.upgrades) {
    std::cout << "UPGRADE " << r << '\n';
  }
  if (a.train_n > 0) {
    std::cout << "TRAIN " << a.train_n << '\n';
  }
}

static void emit_end() { std::cout << "END" << std::endl; }

constexpr int MIN_ATTACK_FORCE = 3;

// 지금 거점을 짓기로 하면(도착까지 extra_turns만큼 더 걸린다면), 게임 종료
// (MAX_TURN)까지 남은 기간 동안 벌어들일 수 있는 예상 수입이 건설 비용보다
// 적으면 지어봤자 손해다. 레벨1 기지(일자리 1칸) 기준으로 계산한다.
static bool worth_building_base(int turn, int extra_turns) {
  int remaining = MAX_TURN - turn - extra_turns;
  if (remaining <= 0)
    return false;
  long long expected_income =
      (long long)remaining * WORK_INCOME * BASE_LEVELS[1].work_cap;
  return expected_income >= BASE_LEVELS[1].cost;
}

// 기지를 한 단계 더 올리는 것도 마찬가지 기준으로 판단하되, 신규 거점
// 건설(300골드)보다 레벨업(600/1000골드)이 일자리 1칸당 훨씬 비싸므로
// 손익분기점을 살짝 넘는 정도로는 부족하다. 순이익(예상 수입 - 비용)이
// 900골드 이상 남을 때만 "확실히 이득"으로 보고 레벨업한다.
// 레벨업으로 늘어나는 최대 체력/포탑 공격력도 수입 못지않은 실질 가치다:
// 늘어난 체력은 그 거점을 지키는 데 필요한 병력을 그만큼 아껴준다는
// 뜻이고(현재 사령부 레벨 기준 전사 1명 체력으로 나눠 "전사 몇 명분"인지
// 환산), 늘어난 포탑 공격력 1점은 매일 전사 1명이 추가로 공격하는 것과
// 같은 화력이다(combatDay에서 터렛도 병력과 동일하게 deliverAttacks를
// 수행). 이 "전사 몇 명분" 가치를 훈련비(TRAIN_COST) 기준으로 환산해
// 이익에 더한다.
static bool worth_upgrading_base(int turn, int level, int hqLevel) {
  int remaining = MAX_TURN - turn;
  if (remaining <= 0)
    return false;
  int workGain = BASE_LEVELS[level + 1].work_cap - BASE_LEVELS[level].work_cap;
  long long expected_income = (long long)remaining * WORK_INCOME * workGain;

  int hpGain = BASE_LEVELS[level + 1].hp - BASE_LEVELS[level].hp;
  int turretGain = BASE_LEVELS[level + 1].turret - BASE_LEVELS[level].turret;
  int myWarriorHp = HQ_LEVELS[hqLevel].warrior_hp;
  double warriorEquiv = (double)hpGain / myWarriorHp + turretGain;
  long long durability_value = (long long)(warriorEquiv * TRAIN_COST);

  long long profit = expected_income + durability_value - BASE_LEVELS[level + 1].cost;
  return profit >= 900;
}

static int my_hq_train_cap(const GameState &S, const GameMap &M) {
  for (const auto &b : S.buildings)
    if (b.side == M.my_side && b.type == BType::HQ)
      return HQ_LEVELS[b.level].train_cap;
  return HQ_LEVELS[1].train_cap;
}

struct CW {
  int hp;
  int num;
};

static void deliverAttacks(int count, std::vector<CW> &def, int &defBldHp) {
  for (int k = 0; k < count; ++k) {
    int idx = -1;
    for (int i = 0; i < (int)def.size(); ++i) {
      if (def[i].hp <= 0)
        continue;
      if (idx == -1 || def[i].hp < def[idx].hp ||
          (def[i].hp == def[idx].hp && def[i].num < def[idx].num))
        idx = i;
    }
    if (idx != -1)
      def[idx].hp -= 1;
    else if (defBldHp > 0)
      defBldHp -= 1;
  }
}

static void combatDay(std::vector<CW> &myWar, int myTurret, int &myBldHp,
                      std::vector<CW> &opWar, int opTurret, int &opBldHp) {
  int a = (int)myWar.size();
  int b = (int)opWar.size();
  deliverAttacks(myTurret, opWar, opBldHp);
  deliverAttacks(opTurret, myWar, myBldHp);
  deliverAttacks(a, opWar, opBldHp);
  deliverAttacks(b, myWar, myBldHp);
  auto dead = [](const CW &c) { return c.hp <= 0; };
  myWar.erase(std::remove_if(myWar.begin(), myWar.end(), dead), myWar.end());
  opWar.erase(std::remove_if(opWar.begin(), opWar.end(), dead), opWar.end());
}

struct StreamArrival {
  int day;
  int hp;
  int num;
};

struct StreamCombatForecast {
  bool captured = false;
  int captureDay = -1;
  int attackerArrivals = 0;
  int defenderArrivals = 0;
  int attackerSurvivors = 0;
};

struct EmptyDefenseForecast {
  bool held = false;
  bool built = false;
  int defenderSurvivors = 0;
  int attackerSurvivors = 0;
  int buildingHp = 0;
};

// 이미 차지한 빈 거점을 지킬 수 있는지 실제 병력별 현재 HP와 도착일로
// 계산한다. 건설 예정일에는 이동/전투보다 먼저 BASE를 짓는 실제 턴 순서를
// 사용한다. 따라서 적 도착과 건설이 같은 날이어도 건설 포탑이 그 전투부터
// 적용된다.
static EmptyDefenseForecast simulate_empty_defense_stream(
    std::vector<StreamArrival> defenderArrivals,
    std::vector<StreamArrival> attackerArrivals, int plannedBuildDay,
    int horizon) {
  auto byDay = [](const StreamArrival &a, const StreamArrival &b) {
    if (a.day != b.day) return a.day < b.day;
    return a.num < b.num;
  };
  std::sort(defenderArrivals.begin(), defenderArrivals.end(), byDay);
  std::sort(attackerArrivals.begin(), attackerArrivals.end(), byDay);

  std::vector<CW> def, atk;
  size_t di = 0, ai = 0;
  int buildingHp = 0, turret = 0;
  bool built = false;
  int attackerBuildingDummy = -1;

  for (int day = 0; day <= horizon; ++day) {
    // 그 턴 명령 시점에 이미 도착해 있는 아군으로 먼저 건설할 수 있다.
    while (di < defenderArrivals.size() && defenderArrivals[di].day <= day) {
      def.push_back({defenderArrivals[di].hp, defenderArrivals[di].num});
      ++di;
    }
    if (!built && plannedBuildDay >= 0 && day >= plannedBuildDay &&
        !def.empty() && atk.empty()) {
      built = true;
      buildingHp = BASE_LEVELS[1].hp;
      turret = BASE_LEVELS[1].turret;
    }
    while (ai < attackerArrivals.size() && attackerArrivals[ai].day <= day) {
      atk.push_back({attackerArrivals[ai].hp, attackerArrivals[ai].num});
      ++ai;
    }

    if (!atk.empty()) {
      combatDay(def, turret, buildingHp, atk, 0, attackerBuildingDummy);
      if (built && buildingHp <= 0) {
        built = false;
        buildingHp = 0;
        turret = 0;
      }
    }
  }

  EmptyDefenseForecast out;
  out.built = built;
  out.defenderSurvivors = (int)def.size();
  out.attackerSurvivors = (int)atk.size();
  out.buildingHp = buildingHp;
  out.held = atk.empty() && (built || !def.empty());
  return out;
}

struct IsolatedBaseCombatOutcome {
  bool destroyed = false;
  bool resolved = false;
  int combatTurns = -1;
  int attackerSurvivors = 0;
  int defenderSurvivors = 0;
  int buildingHpRemaining = 0;
};

// 증원이 합류하기 전의 현재 주둔군+기지만 따로 싸웠을 때의 결과다.
// 정적 표와 달리 실제 병력별 현재 HP와 손상된 기지 HP를 그대로 사용한다.
static IsolatedBaseCombatOutcome simulate_isolated_base_combat(
    int buildingHp, int turret, const std::vector<int> &attackerHps,
    const std::vector<CW> &garrison, int maxCombatTurns) {
  std::vector<CW> attackers;
  attackers.reserve(attackerHps.size());
  for (int i = 0; i < (int)attackerHps.size(); ++i)
    attackers.push_back({attackerHps[i], 8000000 + i});
  std::vector<CW> defenders = garrison;
  int attackerBuildingDummy = -1;
  int hp = buildingHp;
  int turns = 0;
  for (int combatTurn = 1; combatTurn <= maxCombatTurns; ++combatTurn) {
    if (attackers.empty()) break;
    turns = combatTurn;
    combatDay(attackers, 0, attackerBuildingDummy, defenders, turret, hp);
    if (hp <= 0)
      return {true, true, combatTurn, (int)attackers.size(),
              (int)defenders.size(), 0};
  }
  return {false, attackers.empty(), turns, (int)attackers.size(),
          (int)defenders.size(), std::max(0, hp)};
}

// 현재 교전 상태에서 병력이 실제 도착하는 날에만 합류시키며 전투를
// 진행한다. 중간에 공격 병력이 끊겨도 이미 출발한 후속 파동과 이후 생산은
// 계속 같은 목표로 온다는 연속 증원 정책 자체를 검사하는 함수이므로,
// 즉시 실패로 끝내지 않고 horizon까지 기다린다.
static StreamCombatForecast simulate_reinforcement_stream(
    int bldHp, int turret, bool targetHasBuilding,
    std::vector<StreamArrival> attackerArrivals,
    std::vector<StreamArrival> defenderArrivals, int horizon,
    bool requireAllDefenderArrivals = false) {
  auto byDay = [](const StreamArrival &a, const StreamArrival &b) {
    if (a.day != b.day) return a.day < b.day;
    return a.num < b.num;
  };
  std::sort(attackerArrivals.begin(), attackerArrivals.end(), byDay);
  std::sort(defenderArrivals.begin(), defenderArrivals.end(), byDay);

  StreamCombatForecast out;
  out.attackerArrivals = (int)attackerArrivals.size();
  out.defenderArrivals = (int)defenderArrivals.size();
  std::vector<CW> atk, def;
  size_t ai = 0, di = 0;
  int hp = bldHp;
  int attackerBuildingDummy = -1;

  for (int day = 0; day <= horizon; ++day) {
    while (ai < attackerArrivals.size() && attackerArrivals[ai].day <= day) {
      atk.push_back({attackerArrivals[ai].hp, attackerArrivals[ai].num});
      ++ai;
    }
    while (di < defenderArrivals.size() && defenderArrivals[di].day <= day) {
      def.push_back({defenderArrivals[di].hp, defenderArrivals[di].num});
      ++di;
    }

    if (targetHasBuilding && hp <= 0) {
      if (!requireAllDefenderArrivals)
        return {true, day, out.attackerArrivals,
                out.defenderArrivals, (int)atk.size()};
      // 필수 거점의 정밀 출격 게이트는 건물 파괴가 아니라 실제 점유가
      // 목적이다. 이미 이곳으로 이동 중인 수비대가 남아 있으면 포탑이
      // 사라진 야전 상태로 바꾼 뒤 마지막 관측 병력까지 계속 싸운다.
      targetHasBuilding = false;
      turret = 0;
    }
    if (!targetHasBuilding && def.empty() && !atk.empty() &&
               (!requireAllDefenderArrivals ||
                di >= defenderArrivals.size())) {
      return {true, day, out.attackerArrivals, out.defenderArrivals,
              (int)atk.size()};
    }
    if (atk.empty()) continue;

    combatDay(atk, 0, attackerBuildingDummy, def, turret, hp);
    if (targetHasBuilding && hp <= 0) {
      if (!requireAllDefenderArrivals)
        return {true, day, out.attackerArrivals, out.defenderArrivals,
                (int)atk.size()};
      targetHasBuilding = false;
      turret = 0;
    }
    if (!targetHasBuilding && def.empty() && !atk.empty() &&
        (!requireAllDefenderArrivals || di >= defenderArrivals.size()))
      return {true, day, out.attackerArrivals, out.defenderArrivals,
              (int)atk.size()};
  }
  return out;
}

#ifdef UNIFIED_RECLAIM_STREAM
// 필수/선택 거점 공세의 출격 전과 출격 후가 같은 시간축 입력 생성기를
// 사용하도록 한다. 예전 출격 게이트는 목표의 현재 주둔군만 즉시 전투에
// 넣었고, 출격 후 롤아웃은 아군 미래 생산은 남은 게임 전체, 상대 생산은
// 반응 관측 뒤 3턴만 넣었다. 이 함수는 실제 도착일, 반응 가능한 정지
// 예비대, 양측의 동일한 미래 생산 창을 한 곳에서 만든다.
constexpr int UNIFIED_RECLAIM_HORIZON = 12;
constexpr int UNIFIED_RECLAIM_SAFETY_MARGIN = 1;

struct UnifiedReclaimStreamStats {
  int atTarget = 0;
  int inFlight = 0;
  int fresh = 0;
  int futureAttackers = 0;
  int targetDefenders = 0;
  int movingDefenders = 0;
  int reserveDefenders = 0;
  int futureDefenders = 0;
  int horizon = 0;
  bool predictedFortify = false;
  int fortifyIncome = 0;
};

static bool contains_num(const std::vector<int> &ids, int num) {
  return std::find(ids.begin(), ids.end(), num) != ids.end();
}

static bool opponent_can_fortify_empty_target_now(
    const GameState &S, const GameMap &M, int target) {
  if (S.opp_gold < BASE_LEVELS[1].cost ||
      std::find(M.strongholds.begin(), M.strongholds.end(), target) ==
          M.strongholds.end())
    return false;
  for (const auto &b : S.buildings)
    if (b.region == target) return false;
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side && w.region == target &&
        w.state == WState::STATIONARY)
      return true;
  return false;
}

static StreamCombatForecast forecast_unified_reclaim_stream(
    const GameState &S, const GameMap &M, const Paths &P, int turn,
    int target, const std::vector<int> &committedIds,
    const std::vector<const Warrior *> &freshAttackers,
    int hypotheticalAtRegion, int hypotheticalCount, int myWarriorHp,
    int oppWarriorHp, int myNetIncome, int oppNetIncome,
    bool includeFutureProduction, UnifiedReclaimStreamStats *stats) {
  UnifiedReclaimStreamStats local;
  local.horizon = std::max(
      0, std::min(UNIFIED_RECLAIM_HORIZON, MAX_TURN - turn - 1));
  const int horizon = local.horizon;

  auto hops = [&](int from, int to) {
    if (from < 0 || from >= M.N || to < 0 || to >= M.N) return 9999;
    int h = P.hops[from][to];
    return h < 0 ? 9999 : h;
  };

  const Building *targetBuilding = nullptr;
  std::vector<const Building *> buildingAt(M.N, nullptr);
  for (const auto &b : S.buildings) {
    buildingAt[b.region] = &b;
    if (b.region == target) targetBuilding = &b;
  }
  bool targetHasBuilding = targetBuilding != nullptr;
  int targetHp = targetBuilding != nullptr ? targetBuilding->hp : 0;
  int targetTurret = 0;
  if (targetBuilding != nullptr)
    targetTurret = targetBuilding->type == BType::HQ
        ? HQ_LEVELS[targetBuilding->level].turret
        : BASE_LEVELS[targetBuilding->level].turret;
  int opponentCommittedSpend = 0;
  if (!targetHasBuilding &&
      opponent_can_fortify_empty_target_now(S, M, target)) {
    targetHasBuilding = true;
    targetHp = BASE_LEVELS[1].hp;
    targetTurret = BASE_LEVELS[1].turret;
    opponentCommittedSpend = BASE_LEVELS[1].cost;
    local.predictedFortify = true;
    int targetWorkers = 0;
    for (const auto &w : S.warriors)
      if (w.id.side != M.my_side && w.region == target)
        ++targetWorkers;
    local.fortifyIncome = WORK_INCOME *
        std::min(targetWorkers, BASE_LEVELS[1].work_cap);
  }
  const int effectiveOppNetIncome = oppNetIncome + local.fortifyIncome;

  std::vector<int> enemyAt(M.N, 0);
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side) ++enemyAt[w.region];

  std::vector<StreamArrival> atkArrivals, defArrivals;
  std::vector<int> includedAttackers, includedDefenders;
  int nextAtkNum = 3000000;
  int nextDefNum = 4000000;

  auto addAttacker = [&](int day, int hp, int num, int kind) {
    if (day > horizon || contains_num(includedAttackers, num)) return;
    atkArrivals.push_back({std::max(0, day), hp, num});
    includedAttackers.push_back(num);
    if (kind == 0) ++local.atTarget;
    else if (kind == 1) ++local.inFlight;
    else if (kind == 2) ++local.fresh;
    else ++local.futureAttackers;
  };

  // 이미 목표에 있는 병력과 이미 실행된 작전 이동은 출격 전후 모두 같은
  // 방식으로 고정 입력한다.
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && w.region == target)
      addAttacker(0, w.hp, w.id.num, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.region == target ||
        w.state != WState::MOVING ||
        !contains_num(committedIds, w.id.num) || enemyAt[w.region] > 0)
      continue;
    int h = hops(w.region, target);
    if (h < 9999) addAttacker(std::max(0, h - 1), w.hp, w.id.num, 1);
  }

  // 이번 판단에서 실제로 파견할 유휴 병력. 명령 당일 첫 간선을 이동하므로
  // 현재 위치에서 h-1일 뒤 도착한다.
  for (const Warrior *w : freshAttackers) {
    if (w == nullptr || contains_num(includedAttackers, w->id.num)) continue;
    int h = hops(w->region, target);
    if (h < 9999) addAttacker(std::max(0, h - 1), w->hp, w->id.num, 2);
  }
  int hypotheticalTravel = hops(hypotheticalAtRegion, target);
  for (int i = 0; i < hypotheticalCount && hypotheticalTravel < 9999; ++i)
    addAttacker(std::max(0, hypotheticalTravel - 1), myWarriorHp,
                nextAtkNum++, 2);

  auto addDefender = [&](int day, int hp, int num, int kind) {
    if (day > horizon || contains_num(includedDefenders, num)) return;
    defArrivals.push_back({std::max(0, day), hp, num});
    includedDefenders.push_back(num);
    if (kind == 0) ++local.targetDefenders;
    else if (kind == 1) ++local.movingDefenders;
    else if (kind == 2) ++local.reserveDefenders;
    else ++local.futureDefenders;
  };

  // 현재 주둔군과 이미 목표 방향으로 움직인 것이 관측된 병력.
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side) continue;
    if (w.region == target) {
      addDefender(0, w.hp, w.id.num, 0);
      continue;
    }
    if (w.state != WState::MOVING || w.prev_region < 0 ||
        P.nxt[w.prev_region][target] != w.region)
      continue;
    int h = hops(w.region, target);
    if (h < 9999) addDefender(std::max(0, h - 1), w.hp, w.id.num, 1);
  }

  int firstAttackArrival = horizon + 1;
  for (const auto &a : atkArrivals)
    firstAttackArrival = std::min(firstAttackArrival, a.day);

  // 목표가 실제로 드러나는 날부터 대응할 수 있는 정지 예비대만 넣는다.
  // 전 맵의 모든 잉여를 확정 수비로 잡았던 과거 시간축판은 지나치게
  // 보수적이었다. 여기서는 목표에 가장 가까운 적 건물 한 곳을 실제 대응
  // 집결지로 보고 그곳의 병력만 넣는다. 노동자까지 포함하는 이유는 #5의
  // R41처럼 HQ 보충 인력이 도착하면 기존 노동자도 곧바로 교대 출격할 수
  // 있기 때문이다. 단, 전투에는 각자의 실제 도착일에만 합류한다.
  long long reserveBudget = std::max<long long>(
      0, S.opp_gold - opponentCommittedSpend +
             (long long)std::min(firstAttackArrival, horizon) *
                 effectiveOppNetIncome);
  bool freeDefenseMove = local.predictedFortify ||
      (targetBuilding != nullptr && targetBuilding->side != M.my_side);
  int reserveMoveCost = freeDefenseMove ? 0 : MOVE_COST;
  int reserveSpend = 0;
  int reserveRegion = -1;
  int reserveRegionHops = 9999;
  double reserveRegionDist = std::numeric_limits<double>::infinity();
  std::vector<int> stationaryEnemyAt(M.N, 0);
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side && w.state == WState::STATIONARY &&
        w.region != target)
      ++stationaryEnemyAt[w.region];
  for (const auto &b : S.buildings) {
    if (b.side == M.my_side || b.region == target ||
        stationaryEnemyAt[b.region] == 0)
      continue;
    int h = hops(b.region, target);
    double d = P.dist[b.region][target];
    if (h < reserveRegionHops ||
        (h == reserveRegionHops && d < reserveRegionDist)) {
      reserveRegion = b.region;
      reserveRegionHops = h;
      reserveRegionDist = d;
    }
  }
  if (firstAttackArrival <= horizon) {
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side || w.region != reserveRegion ||
          w.state != WState::STATIONARY)
        continue;
      if (reserveBudget < reserveMoveCost) continue;
      int h = hops(w.region, target);
      if (h >= 9999) continue;
      int arrival = firstAttackArrival + std::max(0, h - 1);
      if (arrival > horizon) continue;
      reserveBudget -= reserveMoveCost;
      reserveSpend += reserveMoveCost;
      addDefender(arrival, w.hp, w.id.num, 2);
    }
  }

  // 출격 뒤 실제 연속 증원 판단에서는 양측 모두 같은 전술 창만큼만 미래
  // 생산을 넣는다. 어느 한쪽만 남은 게임 전체 또는 3턴으로 잘라 보지 않는다.
  if (includeFutureProduction) {
    int myHqTravel = hops(M.my_hq, target);
    int oppHqTravel = hops(M.opp_hq, target);
    long long projectedMyGold = std::max<long long>(
        0, S.gold -
               (long long)(freshAttackers.size() + hypotheticalCount) *
                   MOVE_COST);
    long long projectedOppGold = std::max<long long>(
        0, S.opp_gold - opponentCommittedSpend - reserveSpend);
    long long projectedMyIncome = myNetIncome;
    long long projectedOppIncome = effectiveOppNetIncome;
    int oppProductionCost = TRAIN_COST + (freeDefenseMove ? 0 : MOVE_COST);
    for (int day = 0; day <= horizon; ++day) {
      if (myHqTravel < 9999 &&
          projectedMyGold >= TRAIN_COST + MOVE_COST) {
        projectedMyGold -= TRAIN_COST + MOVE_COST;
        projectedMyIncome -= UPKEEP_PER_WARRIOR;
        int arrival = day + myHqTravel;
        if (arrival <= horizon) {
          addAttacker(arrival, myWarriorHp, nextAtkNum++, 3);
        }
      }
      if (oppHqTravel < 9999 && projectedOppGold >= oppProductionCost) {
        projectedOppGold -= oppProductionCost;
        projectedOppIncome -= UPKEEP_PER_WARRIOR;
        int arrival = day + oppHqTravel;
        if (arrival <= horizon)
          addDefender(arrival, oppWarriorHp, nextDefNum++, 3);
      }
      projectedMyGold = std::max<long long>(
          0, projectedMyGold + projectedMyIncome);
      projectedOppGold = std::max<long long>(
          0, projectedOppGold + projectedOppIncome);
    }
  }

  if (stats != nullptr) *stats = local;
  return simulate_reinforcement_stream(
      targetHp, targetTurret, targetHasBuilding,
      std::move(atkArrivals), std::move(defArrivals), horizon);
}
#endif

static const int NEVER = std::numeric_limits<int>::max();
struct Outcome {
  int breakDay;
  int hqHpLeft;
};

static Outcome assaultOutcome(const GameState &S, const GameMap &M,
                              const Paths &P, int turn, Side attacker,
                              bool reserveLabor = false, int holdTurns = 0) {
  const int UNREACH = std::numeric_limits<int>::max();
  const Side defender = opposite(attacker);
  const int finalTarget = hq_of(M, defender);
  const bool attackerIsMe = (attacker == M.my_side);

  const Building *dhq = nullptr;
  for (const auto &b : S.buildings)
    if (b.region == finalTarget && b.side == defender && b.type == BType::HQ)
      dhq = &b;
  if (dhq == nullptr)
    return {0, 0};

  // 임의의 두 지점 사이 최단 hop 수 (미리 계산해 둔 P.hops 재사용)
  auto hopsBetween = [&](int u, int v) -> int {
    int h = P.hops[u][v];
    return h < 0 ? UNREACH : h;
  };

  // 진격 경로: 상대가 점유한 건물(HQ 제외)을 내 본진에서 가까운 순으로 정렬하고
  // 맨 끝에 상대 HQ를 붙인다. 실제 총공세 파견도 이 순서를 그대로 따른다는 전제.
  // 가까운 단계가 이미 뚫려있으면 그냥 통과하고, 아직 안 뚫린 가장 가까운
  // 단계에서만 전투가 벌어진다.
  struct Stage { int region, hp, turret; bool isHQ; };
  std::vector<Stage> chain;
  for (const auto &b : S.buildings) {
    if (b.side != defender || b.region == finalTarget) continue;
    int trt = (b.type == BType::HQ) ? HQ_LEVELS[b.level].turret
                                     : BASE_LEVELS[b.level].turret;
    chain.push_back({b.region, b.hp, trt, false});
  }
  const int myHqRegion = hq_of(M, attacker);
  std::sort(chain.begin(), chain.end(), [&](const Stage &x, const Stage &y) {
    return hopsBetween(myHqRegion, x.region) < hopsBetween(myHqRegion, y.region);
  });
  chain.push_back({finalTarget, dhq->hp, HQ_LEVELS[dhq->level].turret, true});

  std::vector<int> interDist(chain.size());
  {
    int prevRegion = myHqRegion;
    for (size_t i = 0; i < chain.size(); ++i) {
      interDist[i] = hopsBetween(prevRegion, chain[i].region);
      prevRegion = chain[i].region;
    }
  }

  const int horizon = MAX_TURN - turn;

  struct Arr {
    int day, hp, num;
  };
  std::vector<Arr> pool; // chain[0]에 도착하는 날 기준

  int gold = S.gold;

  // 노동 가능 인구 보존: HQ 포함 모든 건물마다 work_cap만큼은 공격 인원
  // 계산에서 제외하고 수입원으로 남겨둔다고 가정 (아래 수입 계산이
  // HQ도 work_cap만큼 일하고 있다고 가정하는 것과 일치시키기 위함)
  std::vector<int> laborNeed(M.N, 0);
  if (reserveLabor) {
    for (const auto &b : S.buildings)
      if (b.side == attacker)
        laborNeed[b.region] = b.work_cap();
  }
  std::vector<int> laborKept(M.N, 0);

  std::vector<const Warrior *> atk;
  for (const auto &w : S.warriors)
    if (w.id.side == attacker)
      atk.push_back(&w);
  std::sort(atk.begin(), atk.end(), [&](const Warrior *x, const Warrior *y) {
    return hopsBetween(x->region, chain[0].region) <
           hopsBetween(y->region, chain[0].region);
  });
  for (const Warrior *w : atk) {
    if (reserveLabor && laborKept[w->region] < laborNeed[w->region]) {
      ++laborKept[w->region];
      continue; // 노동 인구로 보존, 공격 물결에 포함하지 않음
    }
    int h = hopsBetween(w->region, chain[0].region);
    if (h == UNREACH)
      continue;
    bool going = (w->state == WState::MOVING); // 이미 출발한 유닛은 비용 재청구 안 함
    if (attackerIsMe && !going) {
      if (gold < MOVE_COST)
        continue;
      gold -= MOVE_COST;
    }
    // holdTurns만큼은 출발을 늦춰서 한꺼번에 몰려 도착하게 함
    pool.push_back({std::max(h, holdTurns), w->hp, w->id.num});
  }

  int aLevel = 1;
  for (const auto &b : S.buildings)
    if (b.side == attacker && b.type == BType::HQ)
      aLevel = b.level;
  // HQ를 제외한 나머지 건물 수입은 레벨업 시뮬레이션과 무관하게 고정
  int aBaseIncome = 0;
  for (const auto &b : S.buildings)
    if (b.side == attacker && b.type != BType::HQ)
      aBaseIncome += WORK_INCOME * BASE_LEVELS[b.level].work_cap;

  // 상대가 공격자면 실제 골드는 모르지만, 상대의 공개된 행동/수입으로
  // 역산해둔 S.opp_gold를 추정치로 사용한다 (0으로 가정하면 상대 전력을
  // 과소평가하게 됨)
  long long aPool = attackerIsMe ? gold : S.opp_gold;
  int aNextNum = 1000000;
  int aSimLevel = aLevel; // 시뮬레이션 중 예정된 업그레이드로 계속 성장 가능
  if (interDist[0] != UNREACH)
    for (int d = 0; d < horizon; ++d) {
      // 수입은 공격자가 나든 상대든 매일 계속 쌓인다 (이전엔 내가 공격자일 때
      // 이 누적이 빠져 있어서, 기다려도 쓸 돈이 안 느는 버그가 있었음)
      aPool += aBaseIncome + WORK_INCOME * HQ_LEVELS[aSimLevel].work_cap;

      // 예정된 업그레이드: 여유 골드가 되면 계속 HQ를 레벨업해서
      // train_cap/유닛체력/수입을 키운다
      while (aSimLevel < HQ_MAX_LEVEL &&
            aPool >= HQ_LEVELS[aSimLevel + 1].upgrade_cost) {
        aPool -= HQ_LEVELS[aSimLevel + 1].upgrade_cost;
        ++aSimLevel;
      }

      int curTrainCap = HQ_LEVELS[aSimLevel].train_cap;
      int curWarHp = HQ_LEVELS[aSimLevel].warrior_hp;
      for (int k = 0; k < curTrainCap && aPool >= TRAIN_COST + MOVE_COST; ++k) {
        aPool -= TRAIN_COST + MOVE_COST;
        // holdTurns 이전에 훈련된 신병도 holdTurns까지 대기했다가 함께 출발
        pool.push_back({std::max(d, holdTurns) + interDist[0], curWarHp, aNextNum++});
      }
    }

  // 수비측(진짜 HQ에 한해서만)도 가만히 당하지 않는다: 필드에 있는 기존
  // 유닛들을 회군시키고, 자기 HQ 레벨의 수입/훈련 한도로 계속 신병을 뽑아
  // 방어에 투입한다. 중간 거점들은 자체 터렛/체력만으로 방어한다고 가정
  // (거기까지 증원하는 건 모델링하지 않음 - 단순화).
  std::vector<Arr> defWaves;
  for (const auto &w : S.warriors) {
    if (w.id.side != defender) continue;
    int h = hopsBetween(w.region, finalTarget);
    if (h == UNREACH) continue;
    defWaves.push_back({h, w.hp, w.id.num});
  }

  // HQ를 제외한 나머지 건물 수입은 레벨업 시뮬레이션과 무관하게 고정
  int dBaseIncome = 0;
  for (const auto &b : S.buildings)
    if (b.side == defender && b.type != BType::HQ)
      dBaseIncome += WORK_INCOME * BASE_LEVELS[b.level].work_cap;

  // 수비측이 나(defender==my_side)면 실제 보유 골드를, 상대면 추적해둔
  // S.opp_gold 추정치를 사용한다 (attackerIsMe가 참일 때가 defenderIsMe==false)
  long long dPool = attackerIsMe ? S.opp_gold : S.gold;
  int dNextNum = 2000000;
  int dSimLevel = dhq->level; // 시뮬레이션 중 예정된 업그레이드로 계속 성장 가능
  for (int d = 0; d < horizon; ++d) {
    dPool += dBaseIncome + WORK_INCOME * HQ_LEVELS[dSimLevel].work_cap;

    // 예정된 업그레이드: 여유 골드가 되면 수비측도 계속 HQ를 레벨업한다
    while (dSimLevel < HQ_MAX_LEVEL &&
          dPool >= HQ_LEVELS[dSimLevel + 1].upgrade_cost) {
      dPool -= HQ_LEVELS[dSimLevel + 1].upgrade_cost;
      ++dSimLevel;
    }

    int curDTrainCap = HQ_LEVELS[dSimLevel].train_cap;
    int curDWarHp = HQ_LEVELS[dSimLevel].warrior_hp;
    for (int k = 0; k < curDTrainCap && dPool >= TRAIN_COST; ++k) {
      dPool -= TRAIN_COST;
      defWaves.push_back({d, curDWarHp, dNextNum++});
    }
  }

  // 단계별로 순차 공성 시뮬레이션. 이미 뚫린 단계는 통과하고,
  // 아직 뚫리지 않은 채로 도착한 물결만 그 단계의 전투에 합류한다.
  for (size_t i = 0; i < chain.size(); ++i) {
    std::sort(pool.begin(), pool.end(),
             [](const Arr &x, const Arr &y) { return x.day < y.day; });

    std::vector<CW> aF, defF;
    int stageBldHp = chain[i].hp;
    int aBldHpDummy = -1;
    size_t idx = 0;
    int clearDay = NEVER;

    for (int day = 0; day <= horizon; ++day) {
      while (idx < pool.size() && pool[idx].day <= day) {
        aF.push_back({pool[idx].hp, pool[idx].num});
        ++idx;
      }
      if (chain[i].isHQ) {
        for (const auto &w : defWaves)
          if (w.day == day)
            defF.push_back({w.hp, w.num});
      }
      if (stageBldHp <= 0) { clearDay = day; break; }
      if (aF.empty()) {
        bool more = (idx < pool.size());
        if (!more) break; // 더 올 병력이 없으면 이 단계를 영영 못 깬다
        continue;
      }
      combatDay(aF, 0, aBldHpDummy, defF, chain[i].turret, stageBldHp);
      if (stageBldHp <= 0) { clearDay = day; break; }
    }

    if (clearDay == NEVER) {
      // 진짜 HQ에서 막히면 그 남은 체력을, 중간 거점에서 막히면 아직
      // 손도 못 댄 진짜 HQ 체력을 그대로 반환
      return {NEVER, chain[i].isHQ ? stageBldHp : dhq->hp};
    }

    if (i + 1 == chain.size())
      return {clearDay, 0}; // 마지막 단계(진짜 HQ)까지 뚫음

    int nextDist = interDist[i + 1];
    std::vector<Arr> nextPool;
    if (nextDist != UNREACH) {
      for (const auto &w : aF)
        if (w.hp > 0)
          nextPool.push_back({clearDay + nextDist, w.hp, w.num});
      for (size_t j = idx; j < pool.size(); ++j)
        nextPool.push_back({pool[j].day + nextDist, pool[j].hp, pool[j].num});
    }
    pool = std::move(nextPool);
  }

  return {NEVER, dhq->hp};
}

// 상대의 공세(assaultOutcome, attacker=opponent)가 뚫리지 않게 만드는 데
// 필요한 최소 추가 병력 수. 이미 지금 전력으로 막아낼 수 있으면(breakDay ==
// NEVER) 0을 반환하고, 아니라면 내 HQ에 병력을 하나씩 더 얹어보며(최대
// cap까지) 막아지는 최소 수를 찾는다.
static int min_defenders_needed(const GameState &S, const GameMap &M,
                                const Paths &P, int turn, int cap) {
  Outcome base = assaultOutcome(S, M, P, turn, opposite(M.my_side));
  if (base.breakDay == NEVER)
    return 0;

  int hqLevel = 1;
  for (const auto &b : S.buildings)
    if (b.side == M.my_side && b.type == BType::HQ)
      hqLevel = b.level;

  for (int k = 1; k <= cap; ++k) {
    GameState S2 = S;
    for (int i = 0; i < k; ++i)
      S2.warriors.push_back(Warrior{WarriorId{M.my_side, -1000 - i}, M.my_hq,
                                    HQ_LEVELS[hqLevel].warrior_hp});
    Outcome o = assaultOutcome(S2, M, P, turn, opposite(M.my_side));
    if (o.breakDay == NEVER)
      return k;
  }
  return cap;
}

struct AttackPlan {
  int sendCount;   // 지금 있는 유휴 병력 중 실제로 보낼 인원 수
  int extraToTrain; // 그래도 부족해서 추가로 훈련해야 하는 인원 수(-1이면 훈련해도 답이 안 나옴)
};

// 필수 영토 회수전의 두 단계를 기억한다.
//
// 1) ASSEMBLE: 현재 수비를 이기는 최소 병력 + 1명을 집결시킨다.
// 2) REINFORCE: 선발대가 출발한 뒤에는 목표를 바꾸지 않고 신병/잉여를
//    계속 보낸다. 단, 선발대가 전멸하면 무의미한 1명씩의 투입을 멈추고
//    다시 ASSEMBLE로 돌아간다.
//
// spearhead에는 실제로 최초 돌격 명령을 받은 병력 번호만 넣는다. 후속
// 증원까지 여기에 섞으면 선발대가 전멸해도 이동 중인 신병 한 명 때문에
// REINFORCE 상태가 영원히 유지될 수 있다.
struct ReclaimAssaultState {
  int target = -1;
  bool launched = false;
  int selectedTurn = -1;
  int assemblyDeadline = -1;
  int failedWaves = 0;
  bool defenseResponseSeen = false;
  int lastTargetDefenders = -1;
  // 정적 전투표로 "상대 증원 전에 BASE 파괴"를 노리고 출격한 파동인지.
  // 파괴 뒤 전투를 피할 수 있으면 철수하고, 이미 맞물렸으면 postBreachHold
  // 상태로 전환해 마지막 관측 증원까지 야전 전투를 계속한다.
  bool baseRaceStrike = false;
  // BASE는 살아남지만 주둔군 손실 가치가 더 큰 확정 자살 교환. 전멸한
  // 뒤 같은 목표에 두 번째 파동을 자동 재시도하지 않고 작전을 끝낸다.
  bool failedGarrisonTrade = false;
  bool postBreachHold = false;
  int baseRaceImmediateNet = 0;
  int raidReengageAfterTurn = -1;
  // 필수 탈환의 기존 정적 파동이 처음 출격 가능해진 순간, 관측 이동군을
  // 넣어 한 번만 보정한 필요 인원. 집결 중 계속 올려 상대 증원을 끝없이
  // 뒤쫓지 않도록 실제 출격 또는 작전 reset 전까지 고정한다.
  int preciseLaunchRequired = -1;
  int preciseLaunchSnapshotTurn = -1;
  // 진단용: 이번 필수 탈환 파동이 실제 목표 지역의 적과 한 번이라도
  // 맞붙었는지 기록한다. 판단에는 사용하지 않는다.
  bool engagedReclaimTarget = false;
  std::vector<int> spearhead;
  // 집결지에 도착해 STATIONARY가 되어도 공격 임무를 잃지 않도록
  // 유닛 번호를 작전이 끝날 때까지 별도로 기억한다.
  std::vector<int> committed;

  bool isCommitted(int num) const {
    return std::find(committed.begin(), committed.end(), num) != committed.end();
  }

  void commit(int num) {
    if (!isCommitted(num)) committed.push_back(num);
  }

  void reset(int newTarget = -1, int turn = -1) {
    target = newTarget;
    launched = false;
    selectedTurn = turn;
    assemblyDeadline = (newTarget == -1 || turn < 0) ? -1 : turn + 20;
    failedWaves = 0;
    defenseResponseSeen = false;
    lastTargetDefenders = -1;
    baseRaceStrike = false;
    failedGarrisonTrade = false;
    postBreachHold = false;
    baseRaceImmediateNet = 0;
    raidReengageAfterTurn = -1;
    preciseLaunchRequired = -1;
    preciseLaunchSnapshotTurn = -1;
    engagedReclaimTarget = false;
    spearhead.clear();
    committed.clear();
  }
};

// 시뮬레이션으로 계산한 최소 필요 인원은 어디까지나 지금 보이는 정보만
// 반영한 값이라 딱 그만큼만 보내면 예측이 살짝만 틀려도 실패할 수 있다.
// 그래서 최소 인원보다 항상 이 마진만큼 더 모아서/훈련해서 보낸다.
// 일반 공세의 계획 단계 안전 마진. 실제 출격 직전 정밀 시뮬레이션과는
// 별개로, 집결ㆍ생산할 목표 인원을 최소 승리 인원보다 얼마나 더 잡을지
// 결정한다. 최고판의 기본값은 +5이며 실험 wrapper에서만 덮어쓴다.
#ifndef ATTACK_SAFETY_MARGIN_VALUE
#define ATTACK_SAFETY_MARGIN_VALUE 5
#endif
constexpr int ATTACK_SAFETY_MARGIN = ATTACK_SAFETY_MARGIN_VALUE;

// 예측(threat_count) 기반 방어 파병에서 경유지를 거치느라 직행보다 더
// 돌아가도 되는 최대 hop(턴) 수. 이분탐색으로 이 손해 이하를 만족하는
// 가장 작은 x(=경유지까지의 hop 수)를 찾는다.
constexpr int MAX_WAYPOINT_LOSS = 3;

// 상대 거점(기지 또는 사령부)을 공격하기로 했을 때, 그 거점의 포탑과
// 지금 상주 중인 병력(hp, 마릿수)만 보고 — 앞으로의 증원은 고려하지
// 않고 — 실제로 몇 명을 더 보내야 하는지 계산한다.
//
// fixedHps: 지난 턴들에 이미 이 작전을 위해 파병되어 지금 집결지나 목표로
// 이동 중인(아직 유휴 상태가 아닌) 병력의 체력. 이미 결정되어 오고 있는
// 몫이므로 무조건 다 쓴다고 보고 계산에 항상 포함시킨다 — 이걸 빼먹으면
// 매 턴 이미 오고 있는 병력을 없는 셈 치고 또 훈련/파병을 지시하게 돼
// 필요 인원을 과하게 책정하게 된다.
// freshHpsDesc: 지금 새로 고를 수 있는 유휴 병력의 체력(체력 내림차순).
// fixedHps만으로 충분한지 먼저 보고(0명부터 시작), 부족하면
// freshHpsDesc를 체력이 높은 순서로 하나씩 늘려가며 충분해지는 최소
// 인원을 찾는다. 그것도 다 써서 모자라면 새로 훈련한 병력(myWarriorHp)을
// 하나씩 더해가며 최소 추가 인원(extraToTrain)을 구한다.
static AttackPlan plan_attack_force(int bldHp, int turret,
                                    const std::vector<CW> &garrison,
                                    const std::vector<int> &fixedHps,
                                    const std::vector<int> &freshHpsDesc,
                                    int myWarriorHp, int trainCap,
                                    int safetyMargin = ATTACK_SAFETY_MARGIN,
                                    bool targetHasBuilding = true) {
  auto simulate = [&](int freshCount, int extra) {
    std::vector<CW> aF;
    aF.reserve(fixedHps.size() + freshCount + extra);
    int num = 0;
    for (int hp : fixedHps) aF.push_back({hp, num++});
    for (int i = 0; i < freshCount; ++i) aF.push_back({freshHpsDesc[i], num++});
    for (int i = 0; i < extra; ++i) aF.push_back({myWarriorHp, num++});
    std::vector<CW> defF = garrison;
    int hp = bldHp;
    int aBldHpDummy = -1;
    while (!aF.empty() && (targetHasBuilding ? hp > 0 : !defF.empty())) {
      combatDay(aF, 0, aBldHpDummy, defF, turret, hp);
    }
    return targetHasBuilding ? hp <= 0 : (defF.empty() && !aF.empty());
  };

  int n = (int)freshHpsDesc.size();
  for (int k = 0; k <= n; ++k) {
    if (!simulate(k, 0)) continue;
    // k명이면 이론상 충분하다는 걸 확인했으니, 실제 목표는 여기에
    // safetyMargin을 더한 값이다. 지금 가진 유휴 병력(n명)으로
    // 그 목표를 채울 수 있으면 그만큼 보내고, 못 채우면 가진 전부(n명)를
    // 보내고 모자란 만큼만 훈련한다.
    int desired = k + safetyMargin;
    if (desired <= n) return {desired, 0};
    return {n, desired - n};
  }

  for (int extra = 1; extra <= trainCap; ++extra)
    if (simulate(n, extra)) return {n, extra + safetyMargin};

  return {n, -1};
}

// plan_attack_force와 반대 방향 시뮬레이션: 지금 이 거점을 위협하는 상대
// 병력(attackerHps, 실제 체력 그대로, 앞으로 증원 없이 고정)을 상대로
// 건물이 버티는(공격측이 전멸하는) 데 필요한 최소 수비 인원을 구한다.
// 수비 인원은 전부 myWarriorHp짜리 신병이라고 가정한다.
static int min_regional_defenders_precise(
    int bldHp, int turret, const std::vector<int> &attackerHps,
    std::vector<int> actualDefenderHps, int reinforcementHp,
    bool requireSurvivingDefender) {
  std::sort(actualDefenderHps.begin(), actualDefenderHps.end(),
            std::greater<int>());
  auto simulate = [&](int defenders) {
    std::vector<CW> defF;
    defF.reserve(defenders);
    for (int i = 0; i < defenders; ++i) {
      int hp = i < (int)actualDefenderHps.size()
                   ? actualDefenderHps[i]
                   : reinforcementHp;
      defF.push_back({hp, i});
    }
    std::vector<CW> aF;
    aF.reserve(attackerHps.size());
    int num = 0;
    for (int hp : attackerHps) aF.push_back({hp, num++});
    int hp = bldHp;
    int aBldHpDummy = -1; // 공격측은 필드 병력이라 건물이 없음
    while (!aF.empty() && hp > 0) {
      combatDay(defF, turret, hp, aF, 0, aBldHpDummy);
    }
    return hp > 0 && aF.empty() &&
           (!requireSurvivingDefender || !defF.empty());
  };

  // 상한: 머릿수만 맞춰도(+포탑 보너스) 대개 버티므로 공격 인원수의 2배 +
  // 여유분이면 충분하다. 그래도 안 되면(수비측 체력이 크게 밀리는 극단적
  // 상황) 그 상한을 그대로 반환한다.
  int cap = (int)attackerHps.size() * 2 + 5;
  for (int k = 0; k <= cap; ++k)
    if (simulate(k)) return k;
  return cap;
}

static int min_regional_defenders(int bldHp, int turret,
                                  const std::vector<int> &attackerHps,
                                  int myWarriorHp) {
  return min_regional_defenders_precise(bldHp, turret, attackerHps, {},
                                        myWarriorHp, false);
}

// 건물/포탑이 아직 없는 빈 거점을 이미 점거한 병력이, 관측된 이동 적과
// 야전에서 싸워 최소 1명이 살아남는 데 필요한 수비 인원 수.
static int min_field_defenders_precise(const std::vector<int> &attackerHps,
                                       std::vector<int> actualDefenderHps,
                                       int reinforcementHp) {
  if (attackerHps.empty()) return 0;
  std::sort(actualDefenderHps.begin(), actualDefenderHps.end(),
            std::greater<int>());
  auto simulate = [&](int defenders) {
    std::vector<CW> defF;
    for (int i = 0; i < defenders; ++i)
      defF.push_back({i < (int)actualDefenderHps.size()
                          ? actualDefenderHps[i]
                          : reinforcementHp,
                      i});
    std::vector<CW> atkF;
    for (int i = 0; i < (int)attackerHps.size(); ++i)
      atkF.push_back({attackerHps[i], 1000000 + i});
    int noBuilding = -1;
    while (!defF.empty() && !atkF.empty())
      combatDay(defF, 0, noBuilding, atkF, 0, noBuilding);
    return atkF.empty() && !defF.empty();
  };
  int cap = (int)attackerHps.size() * 2 + 3;
  for (int k = 1; k <= cap; ++k)
    if (simulate(k)) return k;
  return cap;
}

static int min_field_defenders(const std::vector<int> &attackerHps,
                               int myWarriorHp) {
  return min_field_defenders_precise(attackerHps, {}, myWarriorHp);
}

#if ALL_BASES_RALLY_SIM
struct AllBasesRallyChoice {
  int region = -1;
  int reserveCount = 0;
  int failedTargets = std::numeric_limits<int>::max();
  bool hqFailed = true;
  long long lostValue = std::numeric_limits<long long>::max();
  int worstDelay = std::numeric_limits<int>::max();
  int totalDelay = std::numeric_limits<int>::max();
  int enemyDistance = std::numeric_limits<int>::max();
  int enemyHqDistance = std::numeric_limits<int>::max();
};

// 상대 비본부 거점에 실제로 정지해 있는 노동 초과 병력이 지금 당장 아군
// 건물 하나를 골라 전부 돌격한다고 가정한다. 후보 허브에 모아둘 수 있는
// 현재의 실제 잉여 병력은 상대 공격파 머릿수까지만 사용하고, 각 아군 건물의
// 주둔군/현재 HP/포탑과 함께 실제 도착일 순서로 전투를 돌린다.
//
// 모든 건물을 막는 후보끼리는 상대 집결군에 가까운 곳을 고른다. 전부 막는
// 후보가 없을 때만 HQ 파괴 여부 -> 실패 거점 수 -> 손실 가치 -> 대응 지연
// 순으로 후퇴한다. 각 목표 공격은 상대가 그중 하나를 택하는 독립 시나리오라
// 동일 예비대를 모든 목표 시뮬레이션에 각각 투입하는 것이 맞다.
static AllBasesRallyChoice choose_all_bases_rally_hub(
    const GameState &S, const GameMap &M, const Paths &P, int turn,
    int enemyRegion, const std::vector<int> &enemyWaveHps) {
  auto hops = [&](int u, int v) {
    int h = P.hops[u][v];
    return h < 0 ? 9999 : h;
  };

  std::vector<const Building *> bld(M.N, nullptr);
  std::vector<int> enemyAt(M.N, 0);
  for (const auto &b : S.buildings) bld[b.region] = &b;
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side) ++enemyAt[w.region];

  // 각 건물의 work_cap명은 그 자리에 남기고, 체력이 높은 실제 초과 병력만
  // 공용 예비대 후보로 뽑는다. 교전 중이거나 이미 공격 임무에 잠긴 병력은
  // 새 허브로 순간이동한다고 가정하지 않는다.
  std::vector<const Warrior *> reservePool;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || enemyAt[b.region] > 0) continue;
    std::vector<const Warrior *> local;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          w.region != b.region || w.purpose == WPurpose::ATTACK)
        continue;
      local.push_back(&w);
    }
    std::sort(local.begin(), local.end(), [](const Warrior *x,
                                             const Warrior *y) {
      if (x->hp != y->hp) return x->hp > y->hp;
      return x->id.num < y->id.num;
    });
    int surplus = std::max(0, (int)local.size() - b.work_cap());
    reservePool.insert(reservePool.end(), local.begin(),
                       local.begin() + surplus);
  }
  std::sort(reservePool.begin(), reservePool.end(),
            [](const Warrior *x, const Warrior *y) {
              if (x->hp != y->hp) return x->hp > y->hp;
              return x->id.num < y->id.num;
            });
  if (reservePool.size() > enemyWaveHps.size())
    reservePool.resize(enemyWaveHps.size());

  auto isReserve = [&](int num) {
    for (const Warrior *w : reservePool)
      if (w->id.num == num) return true;
    return false;
  };

  auto better = [](const AllBasesRallyChoice &a,
                   const AllBasesRallyChoice &b) {
    if (b.region == -1) return true;
    if (a.hqFailed != b.hqFailed) return !a.hqFailed;
    if (a.failedTargets != b.failedTargets)
      return a.failedTargets < b.failedTargets;
    if (a.failedTargets == 0) {
      // 모두 막을 수 있다면 가장 전진한 안전 후보를 택한다.
      if (a.enemyDistance != b.enemyDistance)
        return a.enemyDistance < b.enemyDistance;
      if (a.enemyHqDistance != b.enemyHqDistance)
        return a.enemyHqDistance < b.enemyHqDistance;
      if (a.worstDelay != b.worstDelay)
        return a.worstDelay < b.worstDelay;
      if (a.totalDelay != b.totalDelay)
        return a.totalDelay < b.totalDelay;
    } else {
      if (a.lostValue != b.lostValue) return a.lostValue < b.lostValue;
      if (a.worstDelay != b.worstDelay)
        return a.worstDelay < b.worstDelay;
      if (a.totalDelay != b.totalDelay)
        return a.totalDelay < b.totalDelay;
      if (a.enemyDistance != b.enemyDistance)
        return a.enemyDistance < b.enemyDistance;
      if (a.enemyHqDistance != b.enemyHqDistance)
        return a.enemyHqDistance < b.enemyHqDistance;
    }
    return a.region < b.region;
  };

  AllBasesRallyChoice best;
  for (const auto &candidate : S.buildings) {
    if (candidate.side != M.my_side) continue;
    AllBasesRallyChoice cur;
    cur.region = candidate.region;
    cur.reserveCount = (int)reservePool.size();
    cur.failedTargets = 0;
    cur.hqFailed = false;
    cur.lostValue = 0;
    cur.worstDelay = 0;
    cur.totalDelay = 0;
    cur.enemyDistance = hops(candidate.region, enemyRegion);
    cur.enemyHqDistance = hops(candidate.region, M.opp_hq);

    for (const auto &target : S.buildings) {
      if (target.side != M.my_side) continue;
      int enemyTravel = hops(enemyRegion, target.region);
      int ownTravel = hops(candidate.region, target.region);
      if (enemyTravel >= 9999) continue;

      int ownArrival = candidate.region == target.region
                           ? 0
                           : (ownTravel >= 9999 ? 9999 : 1 + ownTravel);
      int delay = ownArrival >= 9999
                      ? 9999
                      : std::max(0, ownArrival - enemyTravel);
      cur.worstDelay = std::max(cur.worstDelay, delay);
      cur.totalDelay = std::min(1000000000, cur.totalDelay + delay);

      std::vector<StreamArrival> attackers;
      std::vector<StreamArrival> defenders;
      int serial = 0;
      for (int hp : enemyWaveHps)
        attackers.push_back({enemyTravel, hp, -200000 - serial++});
      // 이미 목표 안에 들어와 있는 상대와 주둔 아군은 day 0부터 싸운다.
      for (const auto &w : S.warriors) {
        if (w.region != target.region || w.state != WState::STATIONARY)
          continue;
        if (w.id.side != M.my_side)
          attackers.push_back({0, w.hp, w.id.num});
        else if (!isReserve(w.id.num))
          defenders.push_back({0, w.hp, w.id.num});
      }
      if (ownArrival < 9999)
        for (const Warrior *w : reservePool)
          defenders.push_back({ownArrival, w->hp, w->id.num});

      int turret = target.type == BType::HQ
                       ? HQ_LEVELS[target.level].turret
                       : BASE_LEVELS[target.level].turret;
      int maxArrival = std::max(enemyTravel,
                                ownArrival >= 9999 ? 0 : ownArrival);
      int remaining = std::max(0, MAX_TURN - turn);
      int horizon = std::min(remaining, maxArrival + 100);
      StreamCombatForecast forecast = simulate_reinforcement_stream(
          target.hp, turret, true, attackers, defenders, horizon);
      if (!forecast.captured) continue;

      ++cur.failedTargets;
      if (target.type == BType::HQ) cur.hqFailed = true;
      long long rebuild = target.type == BType::HQ
                              ? 1000000000LL
                              : (target.level == 1
                                     ? BASE_LEVELS[1].cost
                                     : target.level == 2
                                           ? BASE_LEVELS[1].cost +
                                                 BASE_LEVELS[2].cost
                                           : BASE_LEVELS[1].cost +
                                                 BASE_LEVELS[2].cost +
                                                 BASE_LEVELS[3].cost);
      long long incomeLoss =
          (long long)std::max(0, MAX_TURN - turn) * WORK_INCOME *
          target.work_cap();
      cur.lostValue += rebuild + incomeLoss;
    }

    if (better(cur, best)) best = cur;
  }
  return best;
}
#endif

// ================= ULTIMATE STRATEGY DECIDE FUNCTION =================
static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  Actions a;
  int gold = S.gold;
  const int N = M.N;

  // 필수 영토 탈환/선택 공세의 상태는 방어 재배치보다 먼저 보여야 한다.
  // 그래야 이미 공격에 커밋된 병력을 다음 턴 방어 병력으로 다시 빼앗지
  // 않는다.
  static int territoryReclaimTarget = -1;
  static bool territoryReclaimIsExpansion = false;
#if ANTICIPATE_MANDATORY_PRECLAIM
  // 상대가 빈 필수 거점에 도착해 건설하는 것이 우리보다 빠르다고 관측된
  // 동안만 유지하는 사전 탈환 준비 상태. 실제 점유/건설이 확인되면 일반
  // 필수 탈환 상태로 승격하고 이 표식은 즉시 해제한다.
  static int anticipatedMandatoryTarget = -1;
  static int anticipatedMandatoryBuildTurn = -1;
#endif
  static int optionalPushCooldownUntil = 0;
#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
  // 선택 공세의 병력은 특정 목표가 아니라 이 공통 집결지에 귀속한다.
  // 출격 전 목표가 바뀌어도 이 위치는 유지해 왕복 이동을 막는다.
  static int optionalPushFixedRally = -1;
#endif
  static ReclaimAssaultState reclaimAssault;
  // 현재 전력만으로 제한 시간 안에 상대 HQ를 파괴할 수 있다고 확인해
  // 결전을 시작한 뒤에는, 이동 중 병력이 유휴 후보에서 빠져도 목표를
  // 다른 거점으로 바꾸지 않는다.
  static bool directHqAssault = false;
  auto cautiousMandatoryRetry = [&]() {
    return territoryReclaimTarget != -1 && !territoryReclaimIsExpansion &&
           reclaimAssault.failedWaves > 0;
  };
  reclaimAssault.committed.erase(
      std::remove_if(reclaimAssault.committed.begin(),
                     reclaimAssault.committed.end(), [&](int num) {
        for (const auto &w : S.warriors)
          if (w.id.side == M.my_side && w.id.num == num && w.hp > 0)
            return false;
        return true;
      }),
      reclaimAssault.committed.end());

  // 매 턴 현재 상태의 정적 HQ 경쟁 결과를 계산한다. 아직 기존 전략 분기를
  // 바꾸지는 않고 로그로 검증할 수 있게 노출한다.
  const PassiveHqResult passiveHq = evaluate_passive_hq_race(S, M, turn);
  dbg::note(turn,
            std::string("PASSIVE_HQ ") +
                passive_hq_verdict_name(passiveHq.verdict) + " | me=L" +
                std::to_string(passiveHq.my_final_level) + " hp=" +
                std::to_string(passiveHq.my_final_hp) + " gold=" +
                std::to_string(passiveHq.my_final_gold) + " | opp=L" +
                std::to_string(passiveHq.opp_final_level) + " hp=" +
                std::to_string(passiveHq.opp_final_hp) + " gold=" +
                std::to_string(passiveHq.opp_final_gold));

  auto hops = [&](int u, int v) -> int {
    int h = P.hops[u][v];
    return h < 0 ? 9999 : h;
  };

  // 전선(frontline): 어떤 거점이 "내 사령부에서 가는 거리 vs 상대
  // 사령부에서 가는 거리"가 얼마나 비슷한지로 잰다(hop 기준). 둘이
  // 비슷하면(차이가 작으면) 나도 상대도 비슷한 시간에 갈 수 있는 진짜
  // 경합 지역이고, 한쪽 사령부에 훨씬 가까우면 사실상 그 진영 영토라
  // 긴급성이 낮다.
  auto frontlineDist = [&](int r) -> double {
    int myH = hops(M.my_hq, r);
    int oppH = hops(M.opp_hq, r);
    if (myH >= 9999 || oppH >= 9999) return 1e9; // 갈 수 없는 쪽이 있으면 전선 판단 불가
    return std::fabs((double)(myH - oppH));
  };

  // 경유지 경로 탐색(pickWaypoint)에서 "징검다리"로 쓸 수 있는 아군 건물
  // 목록. 건물 수가 적어(K가 N의 15~20%) 이 목록 위에서 다익스트라를
  // 돌려도 충분히 가볍다.
  std::vector<int> myBuildingRegions;
  for (const auto &b : S.buildings)
    if (b.side == M.my_side) myBuildingRegions.push_back(b.region);

  // 목적지가 예측/계획 단계에서 정해졌을 뿐 아직 확정된 상황이 아닐 때,
  // 병력을 목적지까지 한 번에 못 박지 않고 중간 아군 건물(경유지)에
  // 세워두기 위한 다음 정거장을 계산한다. "한 걸음에 x hop 이하만
  // 이동하고, 반드시 아군 건물에서만 멈춘다"는 규칙으로 아군 건물들을
  // 징검다리 삼아 반복 이동했을 때 r에서 target까지 갈 수 있는 최소 총
  // hop 수를 y(x)라 하면, x가 커질수록 간선이 늘어나 y(x)는 단조
  // 비증가(=직행 대비 손해도 단조 비증가)한다. 손해(y(x) - 직행 hop 수)가
  // MAX_WAYPOINT_LOSS 이하가 되는 가장 작은 x를 이분탐색으로 찾고, 그
  // 경로에서 r 바로 다음(첫 번째) 노드만 돌려준다. target 자체가 아군
  // 건물이 아니어도(예: 적 거점) 동작한다 — target을 그래프의 별도
  // 노드로 취급하되, target이 이미 myBuildingRegions 안에 있으면 노드를
  // 중복시키지 않고 그 노드를 그대로 쓴다.
  auto pickWaypoint = [&](int r, int target) -> int {
    if (r == target) return target;
    const int UNREACH_Y = std::numeric_limits<int>::max();
    int directHops = hops(r, target);
    if (directHops >= 9999) return target; // 애초에 도달 불가면 그냥 직행 목표 유지

    int m = (int)myBuildingRegions.size();
    int targetIdx = -1;
    for (int i = 0; i < m; ++i)
      if (myBuildingRegions[i] == target) { targetIdx = i + 1; break; }
    bool targetIsExtra = (targetIdx == -1);
    int T = targetIsExtra ? (m + 1) : targetIdx;

    auto nodeRegion = [&](int idx) {
      if (idx == 0) return r;
      if (targetIsExtra && idx == m + 1) return target;
      return myBuildingRegions[idx - 1];
    };

    auto shortestVia = [&](int x) -> std::pair<int, int> {
      std::vector<int> dist(T + 1, UNREACH_Y);
      std::vector<int> prevNode(T + 1, -1);
      std::vector<char> visited(T + 1, 0);
      dist[0] = 0;
      for (int iter = 0; iter <= T; ++iter) {
        int u = -1, best = UNREACH_Y;
        for (int i = 0; i <= T; ++i)
          if (!visited[i] && dist[i] < best) { best = dist[i]; u = i; }
        if (u == -1) break;
        visited[u] = 1;
        int ur = nodeRegion(u);
        for (int v = 1; v <= T; ++v) {
          if (visited[v]) continue;
          int vr = nodeRegion(v);
          if (vr == ur) continue;
          int w = hops(ur, vr);
          if (w > x) continue;
          if (dist[u] != UNREACH_Y && dist[u] + w < dist[v]) {
            dist[v] = dist[u] + w;
            prevNode[v] = u;
          }
        }
      }
      if (dist[T] == UNREACH_Y) return {UNREACH_Y, -1};
      int cur = T;
      while (prevNode[cur] != 0) cur = prevNode[cur];
      return {dist[T], nodeRegion(cur)};
    };

    int lo = 0, hi = directHops, bestX = directHops;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      int y = shortestVia(mid).first;
      int loss = (y == UNREACH_Y) ? UNREACH_Y : y - directHops;
      if (loss <= MAX_WAYPOINT_LOSS) { bestX = mid; hi = mid - 1; }
      else lo = mid + 1;
    }
    int firstHop = shortestVia(bestX).second;
    return firstHop != -1 ? firstHop : target;
  };

  std::vector<int> myCnt(N, 0), oppCnt(N, 0);
  std::vector<const Building *> bld(N, nullptr);
  for (const auto &b : S.buildings) bld[b.region] = &b;
  if (bld[M.opp_hq] == nullptr || bld[M.opp_hq]->side == M.my_side)
    directHqAssault = false;
  for (const auto &w : S.warriors) (w.id.side == M.my_side ? myCnt : oppCnt)[w.region]++;
  std::vector<char> isStrong(N, 0);
  for (int s : M.strongholds) isStrong[s] = 1;

#if ANTICIPATE_MANDATORY_PRECLAIM
  // 상대 유닛별로 지금까지 실제로 지나온 이동 간선과 동시에 일치하는
  // '아직 빈 거점' 후보의 교집합을 유지한다. 한 번의 이동 간선은 공통
  // 경로 때문에 여러 목적지와 일치할 수 있지만, 계속 이동하면 후보가
  // 줄어든다. 교집합이 하나가 된 순간에만 경로상 목적지가 확정됐다고 본다.
  // ID는 같은 유닛의 연속 관측을 연결하는 키일 뿐이며, 판단 근거는 매 턴
  // prev_region -> region으로 실제 변한 경로다.
  struct EnemyPathIntentState {
    int num = -1;
    int lastRegion = -1;
    int lastSeenTurn = -1;
    std::vector<int> possibleTargets;
  };
  static std::vector<EnemyPathIntentState> enemyPathIntents;
  if (turn == 0) enemyPathIntents.clear();
  std::vector<std::pair<int, int>> confirmedEnemyPathTargets;
  std::vector<int> movingEnemyIds;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side || w.state != WState::MOVING ||
        w.prev_region < 0 || w.prev_region == w.region)
      continue;
    movingEnemyIds.push_back(w.id.num);
    std::vector<int> edgeTargets;
    for (int t : M.strongholds) {
      if (bld[t] != nullptr) continue;
      if (P.nxt[w.prev_region][t] == w.region)
        edgeTargets.push_back(t);
    }
    std::sort(edgeTargets.begin(), edgeTargets.end());

    auto it = std::find_if(
        enemyPathIntents.begin(), enemyPathIntents.end(),
        [&](const EnemyPathIntentState &x) { return x.num == w.id.num; });
    if (it == enemyPathIntents.end()) {
      enemyPathIntents.push_back(
          {w.id.num, w.region, turn, std::move(edgeTargets)});
      it = std::prev(enemyPathIntents.end());
    } else {
      bool continuous = it->lastSeenTurn == turn - 1 &&
                        it->lastRegion == w.prev_region;
      if (continuous) {
        std::vector<int> intersection;
        std::set_intersection(
            it->possibleTargets.begin(), it->possibleTargets.end(),
            edgeTargets.begin(), edgeTargets.end(),
            std::back_inserter(intersection));
        // 기존 목표와 양립하지 않는 방향 전환이 관측되면 과거 추측을
        // 버리고 이번 실제 이동 간선에서 다시 후보를 시작한다.
        it->possibleTargets = intersection.empty()
                                  ? std::move(edgeTargets)
                                  : std::move(intersection);
      } else {
        it->possibleTargets = std::move(edgeTargets);
      }
      it->lastRegion = w.region;
      it->lastSeenTurn = turn;
    }
    if (it->possibleTargets.size() == 1)
      confirmedEnemyPathTargets.push_back(
          {w.id.num, it->possibleTargets.front()});
  }
  enemyPathIntents.erase(
      std::remove_if(enemyPathIntents.begin(), enemyPathIntents.end(),
                     [&](const EnemyPathIntentState &x) {
                       return std::find(movingEnemyIds.begin(),
                                        movingEnemyIds.end(), x.num) ==
                              movingEnemyIds.end();
                     }),
      enemyPathIntents.end());

  if (anticipatedMandatoryTarget != -1) {
    const Building *anticipatedBuilding = bld[anticipatedMandatoryTarget];
    bool enemyArrived =
        (anticipatedBuilding != nullptr &&
         anticipatedBuilding->side != M.my_side) ||
        (anticipatedBuilding == nullptr &&
         oppCnt[anticipatedMandatoryTarget] > 0);
    if (enemyArrived) {
      dbg::note(turn, "RECLAIM_PRECLAIM_PROMOTE target=R" +
                          std::to_string(anticipatedMandatoryTarget) +
                          " predicted_build=T" +
                          std::to_string(anticipatedMandatoryBuildTurn));
      anticipatedMandatoryTarget = -1;
      anticipatedMandatoryBuildTurn = -1;
    } else if (anticipatedBuilding != nullptr &&
               anticipatedBuilding->side == M.my_side) {
      anticipatedMandatoryTarget = -1;
      anticipatedMandatoryBuildTurn = -1;
    }
  }
#endif

#if HOLD_WON_EMPTY_STRONGHOLD
  // 직전 턴에 양측이 싸우던 빈 거점에서 상대만 사라졌다면 방금 야전에서
  // 이긴 곳이다. 이 상태를 기억하지 않으면 다음 decide에서 즉시 일반 유휴
  // 병력으로 풀려 다른 공세/인력 보충에 끌려간다.
  static std::vector<int> previousMyCnt, previousOppCnt;
  static std::vector<char> wonEmptyStrongholdHold;
  if (turn == 0 || (int)wonEmptyStrongholdHold.size() != N) {
    previousMyCnt.assign(N, 0);
    previousOppCnt.assign(N, 0);
    wonEmptyStrongholdHold.assign(N, 0);
  }
  for (int r : M.strongholds) {
    bool justWonEmptyBattle =
        bld[r] == nullptr && myCnt[r] > 0 && oppCnt[r] == 0 &&
        previousMyCnt[r] > 0 && previousOppCnt[r] > 0;
    if (justWonEmptyBattle) {
      wonEmptyStrongholdHold[r] = 1;
      dbg::note(turn, "WON_EMPTY_HOLD_START target=R" +
                          std::to_string(r) + " survivors=" +
                          std::to_string(myCnt[r]));
    }
    // 건설에 성공했거나 아군 점유가 완전히 끝났으면 빈 거점 HOLD도 끝난다.
    // 적이 다시 들어와 교전 중일 때는 승패가 날 때까지 상태를 유지한다.
    if (bld[r] != nullptr || myCnt[r] == 0)
      wonEmptyStrongholdHold[r] = 0;
  }
  previousMyCnt = myCnt;
  previousOppCnt = oppCnt;
#endif

  // 지역별로 지금 실제로 그 자리에 있는 상대 유닛들의 체력 목록. need[r]
  // 계산에서 "머릿수 - 포탑"이 아니라 실제 combatDay 시뮬레이션으로 버틸
  // 수 있는 인원을 구하는 데 쓴다.
  std::vector<std::vector<int>> oppHpsAt(N);
  std::vector<std::vector<int>> myHpsAt(N);
  for (const auto &w : S.warriors)
    (w.id.side != M.my_side ? oppHpsAt : myHpsAt)[w.region].push_back(w.hp);

  // 병력 전투력 비교는 머릿수가 아니라 체력 총합으로 한다: 사령부 레벨이
  // 다르면 워리어 1명의 체력도 달라져서, 머릿수만 맞추면 실제 전투력은
  // 밀릴 수 있다. 총공세 게이트(canOffensive)와 평소 유지 병력 목표
  // (baseline_military) 모두 이 값을 재사용한다.
  // 단, 자기 건물에서 실제로 노동 중인 인구(work_cap만큼)는 전투에
  // 투입되지 않는 인력이므로 총 hp에서 제외한다. 그 건물에 있는 자기 편
  // 병력을 hp 오름차순으로 정렬해 낮은 쪽 work_cap명을 노동 인구로 보고
  // 빼고, 나머지(및 건물 위에 있지 않은 병력 전부)만 합산한다.
  std::vector<std::vector<int>> selfHpsAtOwnBuilding(N);
  for (const auto &w : S.warriors) {
    const Building *b = bld[w.region];
    if (b != nullptr && b->side == w.id.side)
      selfHpsAtOwnBuilding[w.region].push_back(w.hp);
  }
  int myTotalHp = 0, oppTotalHp = 0;
  for (const auto &w : S.warriors) {
    const Building *b = bld[w.region];
    if (b == nullptr || b->side != w.id.side)
      (w.id.side == M.my_side ? myTotalHp : oppTotalHp) += w.hp;
  }
  for (const auto &b : S.buildings) {
    auto &hps = selfHpsAtOwnBuilding[b.region];
    std::sort(hps.begin(), hps.end());
    int wc = b.work_cap();
    int extra = 0;
    for (size_t i = (size_t)wc; i < hps.size(); ++i) extra += hps[i];
    (b.side == M.my_side ? myTotalHp : oppTotalHp) += extra;
  }

  // 지금 실제로 관측되는(추측이 아닌) 순수입: 각 건물에 배치된 병력 수만큼
  // 일해서 버는 돈에서 유지비를 뺀 값. 상대 쪽도 우리가 볼 수 있는 현재
  // 배치(oppCnt)와 건물 레벨로 그대로 계산하므로 미래 훈련/이동을 가정하지
  // 않는다. 이 값이 공격 여부(canOffensive)와 업그레이드 허용 여부를 모두
  // 결정한다: 상대보다 수입이 적으면 공격으로 거점을 빼앗아 격차를 좁히고,
  // 그렇지 않으면 병력을 소모하지 않고 돈을 모아 사령부/거점을 올린다.
  int current_net_income = 0, oppNetIncome = 0;
  {
    int myAliveCnt = 0, oppAliveCnt = 0;
    for (const auto &b : S.buildings) {
      int c = 0;
      for (const auto &w : S.warriors)
        if (w.id.side == b.side && w.region == b.region) ++c;
      int add = WORK_INCOME * std::min(c, b.work_cap());
      if (b.side == M.my_side) current_net_income += add;
      else oppNetIncome += add;
    }
    for (const auto &w : S.warriors)
      (w.id.side == M.my_side ? myAliveCnt : oppAliveCnt)++;
    current_net_income -= myAliveCnt * UPKEEP_PER_WARRIOR;
    oppNetIncome -= oppAliveCnt * UPKEEP_PER_WARRIOR;
  }
  bool myIncomeAhead = (current_net_income >= oppNetIncome);

  // 사령부 레벨업은 공격 여부 판단보다 먼저 처리한다: 아래에서 총공세
  // 조건(total_offensive)이 성립하면 함수가 그 자리에서 바로 return 해버려서
  // 뒤쪽의 업그레이드 로직을 아예 못 타게 된다. 그러면 공격 가능한 턴마다
  // 사령부 레벨업 기회를 영영 놓치게 되므로, 공격 판단 전에 여유 골드로
  // 사령부부터 올릴 수 있으면 올려둔다.
  bool hqUpgradedThisTurn = false;
  int myHqLevel = 1;  // 내 사령부 레벨(총공세 게이트 판단에도 재사용)
  {
    const Building *myHq = nullptr;
    for (const auto &b : S.buildings) {
      if (b.side == M.my_side && b.type == BType::HQ) myHq = &b;
    }
    if (myHq != nullptr) myHqLevel = myHq->level;
    // 사령부 업그레이드는 수급 비교와 무관하게 한다: 공격(병력 소모)과
    // 업그레이드(골드 소모)는 서로 다른 자원을 쓰므로 배타적일 필요가
    // 없고, 사령부 업그레이드 자체도 work_cap을 늘려 수급을 개선하는
    // 수단이다.
    if (myHq != nullptr && myHq->level < HQ_MAX_LEVEL &&
        myCnt[myHq->region] > 0 && oppCnt[myHq->region] == 0) {
      int cost = HQ_LEVELS[myHq->level + 1].upgrade_cost;
      if (gold >= cost) {
        a.upgrades.push_back(myHq->region);
        gold -= cost;
        hqUpgradedThisTurn = true;
        dbg::note(turn, "UPGRADE HQ R" + std::to_string(myHq->region) + " L" +
                            std::to_string(myHq->level) + "->" +
                            std::to_string(myHq->level + 1) + " (cost=" +
                            std::to_string(cost) + ")");
      }
    }
  }

  int oppHqLevel = 1; // 상대 사령부 레벨(총공세 게이트 판단에도 재사용)
  for (const auto &b : S.buildings)
    if (b.side != M.my_side && b.type == BType::HQ) oppHqLevel = b.level;

  // 사령부 레벨에서 상대에게 밀리고 있으면 격차부터 좁히는 게 최우선이다.
  // 병력 생산과 거점 업그레이드를 모두 멈추고 골드를 사령부 업그레이드에만
  // 쓴다(사령부 업그레이드 자체는 위에서 이미 여유 골드로 시도했음).
  bool hqBehind = (myHqLevel < oppHqLevel);

  // 사령부가 이미 만렙(5렙)이면 더 올릴 업그레이드가 없으니, 남는 골드를
  // 계속 쌓아두지만 말고 병력으로 돌린다. 단, 다음 턴부터 늘어나는
  // 유지비까지 반영해서 상대 추정 골드/수입보다 밀리지 않는 한도까지만
  // 뽑는다(gold_lead_military 아래 상세 설명 참고). total_offensive
  // 분기가 이 계산보다 먼저 return해버리는 문제를 피하기 위해, 총공세
  // 판단 이전인 여기서 미리 계산해 두고 두 분기(공세/평시) 모두에서
  // 재사용한다.
  bool hqMaxed = (myHqLevel >= HQ_MAX_LEVEL);
  int gold_lead_military = 0;
  if (hqMaxed && myIncomeAhead) {
    // 이번 턴에 훈련비(TRAIN_COST)를 쓰고 다음 턴부터 유지비
    // (UPKEEP_PER_WARRIOR)가 늘어나는 것까지 반영해서, 그래도 다음 턴
    // 예상 골드 총량(보유+수입)이 상대 추정치보다 밀리지 않는 한도
    // (stock_limit)와, 다음 턴 순수입 자체가 상대보다 밀리지 않는 한도
    // (income_limit) 중 더 타이트한 쪽을 쓴다.
    int stock_limit = std::max(0,
        (int)(((long long)gold + current_net_income - S.opp_gold - oppNetIncome)
              / (TRAIN_COST + UPKEEP_PER_WARRIOR)));
    int income_limit = std::max(0,
        (current_net_income - oppNetIncome) / UPKEEP_PER_WARRIOR);
    gold_lead_military = std::min(stock_limit, income_limit);
  }

  // 상대 이동 병력의 예상 목표 예측: 이번 턴에 실제로 한 칸 이동한 상대
  // 유닛에 대해, 그 이동 방향이 내 거점으로 가는 최단 경로의 다음 칸과
  // 일치하면 그 거점을 향한다고 간주한다. (여러 거점과 동시에 일치할 수
  // 있는데, 그 경우 전부 후보로 카운트 - 애매하면 과소평가보다 안전)
  std::vector<int> threat_count(N, 0);
  std::vector<std::vector<int>> threat_hps(N);
  // 각 거점을 노리며 이동 중인 상대 병력의 최단 도착 예상 턴(ETA). 방어
  // 증원을 경유지로 돌릴지 직행시킬지 정하는 데 쓴다(임박하면 직행).
  // 이동이 감지된 위협에만 값이 들어가고, 아직 안 움직인 집결 예측은
  // INT_MAX로 남겨 "임박하지 않음"으로 취급한다.
  std::vector<int> threat_eta(N, std::numeric_limits<int>::max());
  // 집결 감지(아래)로 예약할 방어 인원. 이동 감지(threat_hps)와 달리 실제
  // 전투 시뮬레이션을 돌리지 않고, 모여 있는 상대 병력 수만큼 1:1로 수비를
  // 잡는다 — 아직 움직이지도 않은 예측이라 포탑 보너스/동시 도착 가정에
  // 기대는 최소 인원 계산이 너무 낙관적이기 때문이다(상대가 집결을 더
  // 불릴 수도 있다). threat_count에는 여전히 반영해 경유지 이동 경계는 켠다.
  std::vector<int> staging_defense(N, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side) continue;
    if (w.state != WState::MOVING) continue;
    if (w.prev_region < 0 || w.prev_region == w.region) continue;
    // 방향이 일치하는 내 거점이 여럿이면(공통 경로 초반 구간), 실제로는
    // 그중 가장 가까운 곳(경로상 가장 먼저 마주치는 곳)이 먼저 공격당하고,
    // 거기가 뚫리기 전에는 그 뒤쪽 거점까지 도달할 수 없다. 그러니 가장
    // 가까운 거점 하나에만 위협을 귀속시킨다.
    int bestRegion = -1, bestDist = -1;
    for (const auto &b : S.buildings) {
      if (b.side != M.my_side || b.region == w.prev_region) continue;
      if (P.nxt[w.prev_region][b.region] != w.region) continue;
      int d = hops(w.region, b.region);
      if (bestRegion == -1 || d < bestDist) { bestRegion = b.region; bestDist = d; }
    }
    if (bestRegion != -1) {
      ++threat_count[bestRegion];
      threat_hps[bestRegion].push_back(w.hp);
      // bestDist = hops(적 현재위치, bestRegion) = 이 위협의 도착 예상 턴.
      threat_eta[bestRegion] = std::min(threat_eta[bestRegion], bestDist);
    }
  }

  // 이동 방향 감지(threat_count/threat_hps)는 상대가 실제로 움직이기
  // 시작해야만 잡힌다. 그런데 상대가 한 거점에 일자리 수(work_cap)보다
  // 병력을 계속 쌓아두기만 하고 가만히 있으면(=공격을 준비하며 집결
  // 중이면), 그 동안은 threat_count가 0이라 방어 준비를 못 한다. 그래서
  // 상대 건물에 노동 인구보다 병력이 더 많이 정지해 있으면, 그 초과분을
  // "아직 안 움직였지만 예비된 공격 물결"로 보고, 그 집결지에서 가장
  // 가까운 내 건물에 미리 위협으로 반영해둔다. 실제로 움직이기 시작하면
  // 그때부터는 위 이동 방향 감지가 대신 잡아주므로 이중 계산은 안 된다
  // (여기서는 STATIONARY만, 위에서는 MOVING만 본다).
#if EXCLUDE_OUTERMOST_RALLY_BASE
  // 상대 집결군 대응 위치 후보에서만, 원점 (0, 0)에서 가장 먼 아군
  // 비-HQ 기지 한 곳을 제외한다. HQ는 가장자리 거점 판정 대상에서 빼며
  // 대응 위치 후보로는 그대로 남긴다.
  int outermostRallyBase = -1;
  long double outermostOriginDist2 = -1.0L;
  for (const auto &own : S.buildings) {
    if (own.side != M.my_side || own.type == BType::HQ) continue;
    long double ox = (long double)M.x[own.region];
    long double oy = (long double)M.y[own.region];
    long double d2 = ox * ox + oy * oy;
    if (d2 < outermostOriginDist2) continue;
    if (d2 == outermostOriginDist2 && outermostRallyBase != -1 &&
        own.region > outermostRallyBase)
      continue;
    outermostOriginDist2 = d2;
    outermostRallyBase = own.region;
  }
#endif
  for (const auto &b : S.buildings) {
    if (b.side == M.my_side) continue;
    // 상대 본부(HQ)는 집결 감지에서 제외한다: 신병이 생산되는 홈이라 늘
    // 병력이 쌓여 있어 일자리 초과분이 상시 잡히는데, 그건 공격을 위한
    // 집결이 아니라 그냥 대기 병력일 때가 많아 오탐이 된다.
    if (b.type == BType::HQ) continue;
    std::vector<int> excessHps;
    int wc = b.work_cap();
    int cnt = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side || w.region != b.region) continue;
      if (w.state != WState::STATIONARY) continue;
      ++cnt;
      if (cnt > wc) excessHps.push_back(w.hp);
    }
    if (excessHps.empty()) continue;

    int mirrorTarget = -1;
#if ALL_BASES_RALLY_SIM
    // 감지 인원·방어 예약량은 기존 그대로 두고, 이 집결군에 대응할 아군
    // 거점의 위치만 바꾼다. 현재 잉여 예비대가 여기 모였다고 할 때 상대가
    // 보유 건물 어느 곳으로 급습해도 가장 잘 막는 위치를 전투 시뮬레이션
    // 결과로 고른다.
    AllBasesRallyChoice choice = choose_all_bases_rally_hub(
        S, M, P, turn, b.region, excessHps);
    mirrorTarget = choice.region;
    dbg::note(turn, "ALL_BASES_RALLY enemy=R" +
                        std::to_string(b.region) + " hub=R" +
                        std::to_string(mirrorTarget) + " enemy_force=" +
                        std::to_string(excessHps.size()) + " reserve=" +
                        std::to_string(choice.reserveCount) + " failed=" +
                        std::to_string(choice.failedTargets) + " hq_fail=" +
                        std::to_string(choice.hqFailed ? 1 : 0) + " lost=" +
                        std::to_string(choice.lostValue) + " worst_delay=" +
                        std::to_string(choice.worstDelay) + " enemy_hops=" +
                        std::to_string(choice.enemyDistance));
#else
    // 이 집결지에서 가장 가까운 내 건물을 "노려질 만한 곳"으로 본다.
    // hops가 같으면(동률), 상대 HQ에 hop이 더 적은(=더 전방인) 쪽을
    // 우선한다. 좌표상 더 바깥이라도 그래프 위상에서 상대 쪽으로 더 깊은
    // 거점이 상대 진입로에서 먼저 마주치는 곳이라 실제로 먼저 털린다.
    // 전방성(oppH)까지 동률이면 전선(수직이등분선)에 더 가까운 쪽으로 깬다.
    int mirrorH = std::numeric_limits<int>::max();
    int mirrorOppH = std::numeric_limits<int>::max();
    double mirrorFrontD = std::numeric_limits<double>::infinity();
    for (const auto &mb : S.buildings) {
      if (mb.side != M.my_side) continue;
#if EXCLUDE_OUTERMOST_RALLY_BASE
      if (mb.region == outermostRallyBase) continue;
#endif
      int h = hops(b.region, mb.region);
      if (h > mirrorH) continue;
      int oppH = hops(mb.region, M.opp_hq);
      double frontD = frontlineDist(mb.region);
      if (h == mirrorH) {
        if (oppH > mirrorOppH) continue;
        if (oppH == mirrorOppH && frontD >= mirrorFrontD) continue;
      }
      mirrorH = h; mirrorOppH = oppH; mirrorFrontD = frontD;
      mirrorTarget = mb.region;
    }
#endif
    if (mirrorTarget == -1) continue;

    threat_count[mirrorTarget] += (int)excessHps.size();
    // 시뮬레이션(threat_hps)이 아니라 머릿수 1:1로 방어를 예약한다.
    staging_defense[mirrorTarget] += (int)excessHps.size();
  }

  // need[r]: r에 상시 배치해두어야 할 최소 병력. 평소엔 일자리(work_cap)만큼
  // 이지만, 위협이 있으면 실제 combatDay 시뮬레이션으로 "지금 위협 병력
  // (앞으로 증원 없이 고정)을 상대로 버티는 데 필요한 최소 인원"을 추가로
  // 요구한다. 이렇게 하면 기존 배치/훈련 로직(아래)이 방어 수요도 최소
  // 인원만 자연히 채우게 된다.
  int curWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;
  // 기존 threat_hps는 아군 "건물"만 목표 후보로 복원하므로, 방금 BASE를
  // 파괴해 아직 빈 땅인 점령지는 대상이 아니다. 빈 점령지를 향하는 이동
  // 적은 같은 방향 복원식을 여기서 별도로 계산한다.
  std::vector<std::vector<int>> emptyStrongholdIncomingHps(N);
  std::vector<std::vector<StreamArrival>> emptyStrongholdEnemyArrivals(N);
  std::vector<std::vector<StreamArrival>> emptyStrongholdOwnArrivals(N);
#if HOLD_EMPTY_CAPTURE_AGAINST_INCOMING
  for (const auto &w : S.warriors) {
    if (w.state != WState::MOVING) continue;
    for (int t : M.strongholds) {
      if (bld[t] != nullptr || myCnt[t] == 0 || oppCnt[t] > 0) continue;
      int eta = hops(w.region, t);
      if (eta >= 9999) continue;
      if (w.id.side == M.my_side) {
        if (w.target == t)
          emptyStrongholdOwnArrivals[t].push_back(
              {eta, w.hp, w.id.num});
        continue;
      }
      if (w.prev_region < 0 || w.prev_region == w.region) continue;
      if (P.nxt[w.prev_region][t] == w.region) {
        emptyStrongholdIncomingHps[t].push_back(w.hp);
        emptyStrongholdEnemyArrivals[t].push_back(
            {eta, w.hp, w.id.num});
      }
    }
  }
#endif
  std::vector<int> need(N, 0);
  std::vector<int> defenseSimNeed(N, 0);
  std::vector<int> emptyCaptureHoldNeed(N, 0);
  std::vector<char> wonEmptyNeedsReinforcement(N, 0);
  std::vector<int> wonEmptyPlannedBuildDay(N, -1);
  // need_train: 신규 훈련(missing_workers)을 유발해도 되는 방어 수요만 담는다.
  // need와 달리 집결 감지(staging_defense) 예측분은 제외한다 — 아직 움직이지도
  // 않은 상대에 대비해 병력을 새로 뽑으면 과잉 생산이 되므로, 그 몫은 기존
  // 유휴 병력 재배치(need 기반 kept/best_help)로만 메우고 훈련은 하지 않는다.
  std::vector<int> need_train(N, 0);
  for (int r = 0; r < N; ++r) {
    if (bld[r] != nullptr && bld[r]->side == M.my_side) {
      int turret = (bld[r]->type == BType::HQ) ? HQ_LEVELS[bld[r]->level].turret
                                                : BASE_LEVELS[bld[r]->level].turret;
      // 공격측 병력 목록: 이미 이 거점에 와 있는(oppHpsAt, 확정) 병력과
      // 이 거점 방향으로 이동 중인(threat_hps, 예측) 병력을 합친다. 서로
      // 다른 유닛 집합이라 겹치지 않는다.
      std::vector<int> attackerHps = oppHpsAt[r];
      attackerHps.insert(attackerHps.end(), threat_hps[r].begin(), threat_hps[r].end());
      int sim_need = 0;
      if (!attackerHps.empty()) {
#if PRECISE_DEFENSE_CURRENT_HP
        // 현재 주둔군은 병사별 손상 HP를 그대로 넣는다. 부족한 k명만 지금
        // HQ 레벨에서 새로 생산되는 만피 병사로 보충한다. 방어의 원래 성공
        // 조건(건물 생존)은 유지해, 정확한 HP 반영과 별개인 생존 마진을
        // 암묵적으로 추가하지 않는다.
        sim_need = min_regional_defenders_precise(
            bld[r]->hp, turret, attackerHps, myHpsAt[r], curWarriorHp, false);
#else
        sim_need = min_regional_defenders(bld[r]->hp, turret, attackerHps,
                                          curWarriorHp);
#endif
      }
      defenseSimNeed[r] = sim_need;
      // 집결 감지분은 시뮬레이션이 아니라 머릿수 1:1로 잡는다. 이동 감지
      // 시뮬레이션 결과와는 둘 중 큰 쪽만 취한다(합산하지 않음). 실제로
      // 움직이기 시작하면 위 threat_hps 쪽으로 잡히고 여기 staging_defense
      // 에는 안 들어오므로 이중 계산되지 않는다.
      int defense_need = std::max(sim_need, staging_defense[r]);
      need[r] = std::max(bld[r]->work_cap(), defense_need);
      // 훈련용 수요에는 집결 예측분(staging_defense)을 빼고 시뮬레이션분만 반영.
      need_train[r] = std::max(bld[r]->work_cap(), sim_need);
    }
    // 아직 건물은 없지만 이미 내 병력이 점거해 둔(상대는 없는) 거점은 평소엔
    // need를 강제로 예약하지 않는다. 다만 방금 파괴한 선택 공세 목표로 실제
    // 적 증원이 이동 중이라면(그 사이 더 급한 필수 탈환 때문에 작전 잠금이
    // 바뀌었더라도), 전원을 다른 거점 방어로 빼서 상대 한 명에게 재점거·
    // 재건설을 허용하지 않도록 야전 승리에 필요한 최소 인원만 남긴다.
#if HOLD_EMPTY_CAPTURE_AGAINST_INCOMING
    if (bld[r] == nullptr && isStrong[r] && myCnt[r] > 0 &&
        oppCnt[r] == 0 &&
        (!emptyStrongholdIncomingHps[r].empty()
#if HOLD_WON_EMPTY_STRONGHOLD
         || wonEmptyStrongholdHold[r]
#endif
        )) {
#if HOLD_WON_EMPTY_STRONGHOLD
      bool wonEmpty = wonEmptyStrongholdHold[r];
#else
      bool wonEmpty = false;
#endif
      int fieldNeed = 1;
      EmptyDefenseForecast forecast;
      int buildDay = -1;
      if (!emptyStrongholdEnemyArrivals[r].empty()) {
        if (gold >= BASE_LEVELS[1].cost) {
          buildDay = 0;
        } else if (current_net_income > 0) {
          buildDay = (BASE_LEVELS[1].cost - gold + current_net_income - 1) /
                     current_net_income;
        }
        std::vector<StreamArrival> ownArrivals =
            emptyStrongholdOwnArrivals[r];
        int localNum = 6000000;
        for (int hp : myHpsAt[r]) ownArrivals.push_back({0, hp, localNum++});
        int horizon = 24;
        for (const auto &x : emptyStrongholdEnemyArrivals[r])
          horizon = std::max(horizon, x.day + 12);
        forecast = simulate_empty_defense_stream(
            ownArrivals, emptyStrongholdEnemyArrivals[r], buildDay, horizon);
        if (wonEmpty) {
          // 방금 이긴 병력은 현재 구성 그대로도 후속 파동을 이길 때 전원
          // 남긴다. 지는 경우에도 상대가 한 명으로 공짜 점유하는 것은 막도록
          // 최강 생존자 한 명은 남기고, 아래에서 실제 ETA/HP 증원을 찾는다.
          fieldNeed = forecast.held ? myCnt[r] : 1;
          wonEmptyNeedsReinforcement[r] = forecast.held ? 0 : 1;
          wonEmptyPlannedBuildDay[r] = buildDay;
        } else {
#if PRECISE_DEFENSE_CURRENT_HP
          fieldNeed = min_field_defenders_precise(
              emptyStrongholdIncomingHps[r], myHpsAt[r], curWarriorHp);
#else
          fieldNeed = min_field_defenders(emptyStrongholdIncomingHps[r],
                                          curWarriorHp);
#endif
        }
      }
      emptyCaptureHoldNeed[r] = std::min(myCnt[r], fieldNeed);
      need[r] = std::max(need[r], emptyCaptureHoldNeed[r]);
      dbg::note(turn, "EMPTY_CAPTURE_HOLD target=R" + std::to_string(r) +
                          " keep=" +
                          std::to_string(emptyCaptureHoldNeed[r]) +
                          " incoming=" +
                          std::to_string(
                              emptyStrongholdIncomingHps[r].size()) +
                          " won=" + std::to_string(wonEmpty ? 1 : 0) +
                          " held=" + std::to_string(forecast.held ? 1 : 0) +
                          " build_day=" + std::to_string(buildDay));
    }
#endif
  }

  // 병력 재배치(best_help)가 건설(buildNowCandidates/stronghold-first)보다
  // 먼저 골드와 유휴 병력을 가져간다: 위협받는 기존 거점을 지키는 게 새
  // 거점을 짓거나 빈 거점을 확보하는 것보다 급하다. 순서가 반대면(건설이
  // 먼저 골드를 가져가면) 정작 위협받는 거점을 증원할 돈/훈련 여력이
  // 모자라는 일이 생긴다.
  std::vector<const Warrior *> idle;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && w.state == WState::STATIONARY) idle.push_back(&w);
  std::sort(idle.begin(), idle.end(), [](const Warrior *x, const Warrior *y) {
    return x->id.num < y->id.num;
  });
  // 지금 상대 병력과 같은 거점에서 교전 중인 병력은 재배치/파병 후보에서
  // 아예 제외한다. need[]의 방어 수요 예측이 부정확하면 실제로 싸우고
  // 있는 병력을 "잉여"로 오판해 빼낼 수 있는데, 전투 중인 병력을 빼면
  // 그 자리에서 바로 불리해지므로 예측 정확도와 무관하게 무조건 자리를
  // 지키게 한다. idle에서 걸러두면 이 아래의 모든 재배치/총공세/확정킬
  // 파병 로직에 공통으로 적용된다.
  idle.erase(std::remove_if(idle.begin(), idle.end(), [&](const Warrior *w) {
    return oppCnt[w->region] > 0;
  }), idle.end());

  // 아래의 승리한 빈 거점 후속파 증원 계산이 HOLD 머릿수를 바꿀 수 있어,
  // 실제 keeper ID 선정은 그 계산 뒤로 미룬다.
  std::vector<int> emptyCaptureKeeperIds;

  // 각 거점에 이미 배치돼 있거나(homeCnt) 오는 중인(incoming) 병력 수.
  // 총공세 판단(방어 공백 감지)과 그 아래 재배치 로직 모두에 필요해서
  // 여기서 미리 계산해 둔다.
  std::vector<int> homeCnt(N, 0), incoming(N, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) continue;
    // 공격 작전에 이미 배정된 병력은 현재 아군 건물 위에 있더라도
    // 방어/일자리 충원 인원으로 이중 계산하지 않는다.
    bool reservedForAssault = cautiousMandatoryRetry()
        ? (w.purpose == WPurpose::ATTACK)
        : reclaimAssault.isCommitted(w.id.num);
    if (reservedForAssault)
      continue;
    if (w.state == WState::MOVING) ++incoming[w.target];
    else ++homeCnt[w.region];
  }
  for (int r = 0; r < N; ++r) {
    if (threat_count[r] <= 0 && oppCnt[r] <= 0 &&
        staging_defense[r] <= 0)
      continue;
    dbg::note(turn, "DEFENSE_ACCOUNT target=R" + std::to_string(r) +
                        " need=" + std::to_string(need[r]) +
                        " sim=" + std::to_string(defenseSimNeed[r]) +
                        " staging=" + std::to_string(staging_defense[r]) +
                        " home=" + std::to_string(homeCnt[r]) +
                        " incoming=" + std::to_string(incoming[r]) +
                        " enemy_here=" + std::to_string(oppCnt[r]) +
                        " moving=" + std::to_string(threat_hps[r].size()));
  }

#if HOLD_WON_EMPTY_STRONGHOLD
  // 방금 승리한 빈 거점에서 현재 생존자만으로는 관측 후속파를 못 막을 때,
  // 정지 예비대의 실제 HP와 목표까지의 실제 ETA를 한 명씩 추가해 본다.
  // 거점 건설 가능일도 이동비를 낸 뒤의 골드/현재 순수입으로 다시 계산하고,
  // 전체 후속파가 끝난 뒤 거점 또는 아군 병사가 남는 최소 prefix만 직행시킨다.
  std::vector<int> wonEmptyDispatchedIds;
  for (int target : M.strongholds) {
    if (!wonEmptyNeedsReinforcement[target] ||
        emptyStrongholdEnemyArrivals[target].empty())
      continue;

    struct WonEmptyCandidate {
      const Warrior *warrior = nullptr;
      int eta = 9999;
    };
    std::vector<WonEmptyCandidate> candidates;
    std::vector<int> pickedAtSource(N, 0);
    for (const Warrior *w : idle) {
      int source = w->region;
      if (source == target || oppCnt[source] > 0) continue;
      if (w->purpose == WPurpose::BUILD && bld[source] == nullptr) continue;
      bool reservedForAssault = cautiousMandatoryRetry()
          ? (w->purpose == WPurpose::ATTACK)
          : reclaimAssault.isCommitted(w->id.num);
      if (reservedForAssault ||
          (directHqAssault && w->purpose == WPurpose::ATTACK))
        continue;
      int eta = hops(source, target);
      if (eta <= 0 || eta >= 9999) continue;
      candidates.push_back({w, eta});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const WonEmptyCandidate &x, const WonEmptyCandidate &y) {
                if (x.eta != y.eta) return x.eta < y.eta;
                if (x.warrior->hp != y.warrior->hp)
                  return x.warrior->hp > y.warrior->hp;
                return x.warrior->id.num < y.warrior->id.num;
              });

    // 출발지의 노동/방어 need는 남겨 둔 후보만 압축한다.
    std::vector<WonEmptyCandidate> usable;
    for (const auto &c : candidates) {
      int source = c.warrior->region;
      int available = std::max(0, homeCnt[source] - need[source]);
      if (pickedAtSource[source] >= available) continue;
      ++pickedAtSource[source];
      usable.push_back(c);
    }

    std::vector<StreamArrival> defenders =
        emptyStrongholdOwnArrivals[target];
    int localNum = 6100000;
    for (int hp : myHpsAt[target])
      defenders.push_back({0, hp, localNum++});
    int horizon = 24;
    for (const auto &x : emptyStrongholdEnemyArrivals[target])
      horizon = std::max(horizon, x.day + 12);

    int minimumSend = -1;
    EmptyDefenseForecast winningForecast;
    for (int k = 1; k <= (int)usable.size(); ++k) {
      const auto &c = usable[k - 1];
      defenders.push_back({c.eta, c.warrior->hp, c.warrior->id.num});
      int remainingGold = gold - k * MOVE_COST;
      if (remainingGold < 0) break;
      int buildDay = -1;
      if (remainingGold >= BASE_LEVELS[1].cost) {
        buildDay = 0;
      } else if (current_net_income > 0) {
        buildDay =
            (BASE_LEVELS[1].cost - remainingGold + current_net_income - 1) /
            current_net_income;
      }
      EmptyDefenseForecast forecast = simulate_empty_defense_stream(
          defenders, emptyStrongholdEnemyArrivals[target], buildDay, horizon);
      if (forecast.held) {
        minimumSend = k;
        winningForecast = forecast;
        wonEmptyPlannedBuildDay[target] = buildDay;
        break;
      }
    }

    if (minimumSend < 0) {
      dbg::note(turn, "WON_EMPTY_HOLD_SENTINEL target=R" +
                          std::to_string(target) + " keep=1 enemy=" +
                          std::to_string(
                              emptyStrongholdEnemyArrivals[target].size()) +
                          " candidates=" + std::to_string(usable.size()));
      continue;
    }

    // 이길 수 있는 경우에는 방금 이긴 생존자를 전부 유지하고, 계산된 최소
    // 증원도 경유하지 않고 실제 전장인 빈 거점으로 곧바로 보낸다.
    emptyCaptureHoldNeed[target] = myCnt[target];
    need[target] = std::max(need[target], myCnt[target]);
    dbg::note(turn, "WON_EMPTY_HOLD_REINFORCE target=R" +
                        std::to_string(target) + " send=" +
                        std::to_string(minimumSend) + " enemy=" +
                        std::to_string(
                            emptyStrongholdEnemyArrivals[target].size()) +
                        " build_day=" +
                        std::to_string(wonEmptyPlannedBuildDay[target]) +
                        " survivors=" +
                        std::to_string(winningForecast.defenderSurvivors) +
                        " building_hp=" +
                        std::to_string(winningForecast.buildingHp));
    for (int i = 0; i < minimumSend; ++i) {
      const auto &c = usable[i];
      const Warrior *w = c.warrior;
      a.moves.push_back({w->id, target, WPurpose::MOVE});
      dbg::move(turn, w->id, w->region, target, WPurpose::MOVE,
                "승리한 빈 거점 후속파 방어 직행 ->R" +
                    std::to_string(target) + " eta=" +
                    std::to_string(c.eta));
      gold -= MOVE_COST;
      --homeCnt[w->region];
      ++incoming[target];
      wonEmptyDispatchedIds.push_back(w->id.num);
      reclaimAssault.committed.erase(
          std::remove(reclaimAssault.committed.begin(),
                      reclaimAssault.committed.end(), w->id.num),
          reclaimAssault.committed.end());
      reclaimAssault.spearhead.erase(
          std::remove(reclaimAssault.spearhead.begin(),
                      reclaimAssault.spearhead.end(), w->id.num),
          reclaimAssault.spearhead.end());
    }
  }
  if (!wonEmptyDispatchedIds.empty()) {
    idle.erase(std::remove_if(idle.begin(), idle.end(),
                              [&](const Warrior *w) {
                                return std::find(
                                           wonEmptyDispatchedIds.begin(),
                                           wonEmptyDispatchedIds.end(),
                                           w->id.num) !=
                                       wonEmptyDispatchedIds.end();
                              }),
               idle.end());
  }
#endif

  // 빈 점령지 HOLD 인원은 같은 머릿수라면 현재 HP가 높은 병력부터 남긴다.
  // 손상 HP를 정확히 쓴 시뮬레이션과 실제로 남기는 병력이 일치해야 한다.
#if HOLD_EMPTY_CAPTURE_AGAINST_INCOMING
  for (int r = 0; r < N; ++r) {
    if (emptyCaptureHoldNeed[r] <= 0) continue;
    std::vector<const Warrior *> atRegion;
    for (const Warrior *w : idle)
      if (w->region == r) atRegion.push_back(w);
    std::sort(atRegion.begin(), atRegion.end(),
              [](const Warrior *x, const Warrior *y) {
                if (x->hp != y->hp) return x->hp > y->hp;
                return x->id.num < y->id.num;
              });
    for (int i = 0;
         i < emptyCaptureHoldNeed[r] && i < (int)atRegion.size(); ++i)
      emptyCaptureKeeperIds.push_back(atRegion[i]->id.num);
  }
#endif

#if REINFORCE_FUTURE_EMPTY_STRONGHOLD_BATTLE
  // 아직 아무도 도착하지 않은 빈 거점은 기존 threat_hps/best_help의 대상이
  // 아니었다. 하지만 우리 BUILD 병력과 상대 이동 병력이 같은 거점을 향하면
  // 그곳은 이미 예약된 미래 전장이다. 실제 도착일별 HP를 야전 시뮬레이션에
  // 넣고, 상대만 살아남는 패배일 때만 제때 도착할 수 있는 잉여를 보낸다.
  // 우리만 살아남는 승리는 물론 양쪽이 같이 전멸하는 무승부도 허용하며,
  // 별도의 안전 마진 없이 패배를 막는 최소 인원만 목표로 직행시킨다.
  struct FutureEmptyBattle {
    int target = -1;
    int ownEta = 9999;
    int enemyEta = 9999;
    int battleEta = 9999;
    std::vector<StreamArrival> ownArrivals;
    std::vector<StreamArrival> enemyArrivals;
  };
  std::vector<FutureEmptyBattle> futureEmptyBattles;
  for (int t : M.strongholds) {
    if (bld[t] != nullptr || myCnt[t] > 0 || oppCnt[t] > 0) continue;

    FutureEmptyBattle fb;
    fb.target = t;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::MOVING ||
          w.purpose != WPurpose::BUILD || w.target != t)
        continue;
      int eta = hops(w.region, t);
      if (eta >= 9999) continue;
      fb.ownEta = std::min(fb.ownEta, eta);
      fb.ownArrivals.push_back({eta, w.hp, w.id.num});
    }
    if (fb.ownArrivals.empty()) continue;

    // 상대의 명령 목표는 직접 알 수 없으므로, 직전 지역 -> 현재 지역의
    // 이동 방향이 t 최단 경로와 일치하는 병력만 t로 향하는 것으로 복원한다.
    // 너무 먼 후속 파동은 현재 충돌과 무관하므로 최초 아군 도착 뒤 관측
    // 반응 창 안에 들어오는 병력까지만 포함한다.
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side || w.state != WState::MOVING ||
          w.prev_region < 0 || w.prev_region == w.region ||
          P.nxt[w.prev_region][t] != w.region)
        continue;
      int eta = hops(w.region, t);
      if (eta >= 9999 || eta > fb.ownEta + REINFORCE_RESPONSE_LOOKAHEAD)
        continue;
      fb.enemyEta = std::min(fb.enemyEta, eta);
      fb.enemyArrivals.push_back({eta, w.hp, w.id.num});
    }
    if (fb.enemyArrivals.empty()) continue;

    // 아군이 한 턴 이상 먼저 멈추면 다음 결정에서 건설해 기존 거점 방어
    // 로직으로 넘어갈 수 있다. 같은 턴 또는 바로 다음 턴에 맞닥뜨리는 경우만
    // '빈 거점 야전'으로 선제 처리한다.
    if (fb.enemyEta > fb.ownEta + 1) continue;
    fb.battleEta = std::max(fb.ownEta, fb.enemyEta);
    futureEmptyBattles.push_back(std::move(fb));
  }
  std::sort(futureEmptyBattles.begin(), futureEmptyBattles.end(),
            [](const FutureEmptyBattle &x, const FutureEmptyBattle &y) {
              if (x.battleEta != y.battleEta)
                return x.battleEta < y.battleEta;
              return x.target < y.target;
            });

  std::vector<int> futureBattleDispatchedIds;
  std::vector<int> idleAt(N, 0);
  for (const Warrior *w : idle) ++idleAt[w->region];

  for (const FutureEmptyBattle &fb : futureEmptyBattles) {
    const int remaining = std::max(0, MAX_TURN - turn);
    const int horizon = std::min(remaining, fb.battleEta + 24);
    // 양쪽을 한 번씩 공격측으로 뒤집어 결과를 구분한다. 기존 파동만으로
    // 우리가 이기면 추가 파병하지 않는다. 양쪽 모두 captured가 아니면
    // 상호 전멸(무승부)이므로 그 자체로도 허용하되, 제때 도착할 잉여가
    // 실제로 있으면 신규 생산 없이 한 명만 보내 승리 쪽으로 기울인다.
    StreamCombatForecast currentOwnWin = simulate_reinforcement_stream(
        0, 0, false, fb.ownArrivals, fb.enemyArrivals, horizon, true);
    if (currentOwnWin.captured) continue;
    StreamCombatForecast currentEnemyWin = simulate_reinforcement_stream(
        0, 0, false, fb.enemyArrivals, fb.ownArrivals, horizon, true);
    bool currentDraw = !currentEnemyWin.captured;

    struct Candidate {
      const Warrior *warrior = nullptr;
      int eta = 9999;
      bool countedAtHome = false;
    };
    std::vector<Candidate> rawCandidates;
    for (const Warrior *w : idle) {
      if (std::find(futureBattleDispatchedIds.begin(),
                    futureBattleDispatchedIds.end(), w->id.num) !=
          futureBattleDispatchedIds.end())
        continue;
      int r = w->region;
      int eta = hops(r, fb.target);
      if (eta <= 0 || eta > fb.battleEta) continue;
      // 다른 빈 거점의 건설 담당자는 그 작전을 깨지 않는다. 상대 HQ를 이미
      // 직접 끝내러 가는 파동도 지역 야전 때문에 되돌리지 않는다.
      if ((w->purpose == WPurpose::BUILD && bld[r] == nullptr) ||
          (directHqAssault && w->purpose == WPurpose::ATTACK))
        continue;
      bool reservedForAssault = cautiousMandatoryRetry()
          ? (w->purpose == WPurpose::ATTACK)
          : reclaimAssault.isCommitted(w->id.num);
      rawCandidates.push_back({w, eta, !reservedForAssault});
    }
    std::sort(rawCandidates.begin(), rawCandidates.end(),
              [](const Candidate &x, const Candidate &y) {
                bool xAttack = x.warrior->purpose == WPurpose::ATTACK;
                bool yAttack = y.warrior->purpose == WPurpose::ATTACK;
                if (xAttack != yAttack) return xAttack > yAttack;
                if (x.eta != y.eta) return x.eta < y.eta;
                if (x.warrior->hp != y.warrior->hp)
                  return x.warrior->hp > y.warrior->hp;
                return x.warrior->id.num < y.warrior->id.num;
              });

    // 각 출발지에서는 그곳의 need만큼을 남긴다. 기존 공세용 ATTACK 병력을
    // 먼저 고르므로 같은 위치의 노동/방어 병력은 가능한 한 건드리지 않는다.
    std::vector<int> pickedAt(N, 0);
    std::vector<Candidate> candidates;
    for (const Candidate &c : rawCandidates) {
      int r = c.warrior->region;
      int surplus = std::max(0, idleAt[r] - need[r]);
      if (pickedAt[r] >= surplus) continue;
      ++pickedAt[r];
      candidates.push_back(c);
    }

    int minimumToAvoidLoss = currentDraw && !candidates.empty() ? 1 : -1;
    if (!currentDraw) {
      std::vector<StreamArrival> reinforced = fb.ownArrivals;
      for (int k = 1; k <= (int)candidates.size(); ++k) {
        const Candidate &c = candidates[k - 1];
        reinforced.push_back(
            {c.eta, c.warrior->hp, c.warrior->id.num});
        StreamCombatForecast enemyWin = simulate_reinforcement_stream(
            0, 0, false, fb.enemyArrivals, reinforced, horizon, true);
        if (!enemyWin.captured) {
          minimumToAvoidLoss = k;
          break;
        }
      }
    }
    if (minimumToAvoidLoss < 0) {
      dbg::note(turn, "FUTURE_EMPTY_BATTLE_UNSUPPORTED target=R" +
                          std::to_string(fb.target) + " own_eta=" +
                          std::to_string(fb.ownEta) + " enemy_eta=" +
                          std::to_string(fb.enemyEta) + " candidates=" +
                          std::to_string(candidates.size()));
      continue;
    }

    int sendCount = std::min(minimumToAvoidLoss, gold / MOVE_COST);
    if (sendCount < minimumToAvoidLoss) {
      dbg::note(turn, "FUTURE_EMPTY_BATTLE_NO_GOLD target=R" +
                          std::to_string(fb.target) + " need=" +
                          std::to_string(minimumToAvoidLoss) + " affordable=" +
                          std::to_string(gold / MOVE_COST));
      continue;
    }

    dbg::note(turn, "FUTURE_EMPTY_BATTLE target=R" +
                        std::to_string(fb.target) + " own_eta=" +
                        std::to_string(fb.ownEta) + " enemy_eta=" +
                        std::to_string(fb.enemyEta) + " min=" +
                        std::to_string(minimumToAvoidLoss) + " send=" +
                        std::to_string(sendCount) + " draw_upgrade=" +
                        std::to_string(currentDraw ? 1 : 0));
    for (int i = 0; i < sendCount; ++i) {
      const Candidate &c = candidates[i];
      const Warrior *w = c.warrior;
      a.moves.push_back({w->id, fb.target, WPurpose::MOVE});
      dbg::move(turn, w->id, w->region, fb.target, WPurpose::MOVE,
                "미래 빈 거점 교전 증원 직행 ->R" +
                    std::to_string(fb.target) + " eta=" +
                    std::to_string(c.eta) + " battle=" +
                    std::to_string(fb.battleEta));
      gold -= MOVE_COST;
      if (c.countedAtHome && homeCnt[w->region] > 0) --homeCnt[w->region];
      ++incoming[fb.target];
      futureBattleDispatchedIds.push_back(w->id.num);
      reclaimAssault.committed.erase(
          std::remove(reclaimAssault.committed.begin(),
                      reclaimAssault.committed.end(), w->id.num),
          reclaimAssault.committed.end());
      reclaimAssault.spearhead.erase(
          std::remove(reclaimAssault.spearhead.begin(),
                      reclaimAssault.spearhead.end(), w->id.num),
          reclaimAssault.spearhead.end());
    }
  }

  if (!futureBattleDispatchedIds.empty()) {
    idle.erase(std::remove_if(idle.begin(), idle.end(),
                              [&](const Warrior *w) {
                                return std::find(
                                           futureBattleDispatchedIds.begin(),
                                           futureBattleDispatchedIds.end(),
                                           w->id.num) !=
                                       futureBattleDispatchedIds.end();
                              }),
               idle.end());
  }
#endif

  std::vector<int> kept(N, 0);
  int train_reserved = 0;

  // 필수 탈환 중에는 새 거점 확장만 멈춘다. 방어 재배치(best_help)는
  // 아래 공격 분기에서도 실제 명령으로 보존하므로 여기서 정상 계산한다.
  bool mandatoryReclaimActiveNow = false;
  if (territoryReclaimTarget != -1 && !territoryReclaimIsExpansion) {
    const Building *targetBuilding = bld[territoryReclaimTarget];
    mandatoryReclaimActiveNow =
        (targetBuilding != nullptr && targetBuilding->side != M.my_side) ||
        oppCnt[territoryReclaimTarget] > 0;
  }

  // 빈 필수 거점에서 양측 병력이 이미 싸우고 있다면 신규 공격을 준비하는
  // 상황이 아니라 진행 중인 야전 전투다. 이 턴에 목표로 직행시킬 기존
  // ATTACK 병력을 인력 보충이 먼저 빼가지 못하게 아래 단계에 전달한다.
  bool activeMandatoryFieldBattleNow = false;
#if DIRECT_ACTIVE_FIELD_RECLAIM
  if (territoryReclaimTarget != -1 && !territoryReclaimIsExpansion &&
      bld[territoryReclaimTarget] == nullptr &&
      myCnt[territoryReclaimTarget] > 0 &&
      oppCnt[territoryReclaimTarget] > 0) {
    activeMandatoryFieldBattleNow = true;
    dbg::note(turn, "RECLAIM_ACTIVE_FIELD target=R" +
                        std::to_string(territoryReclaimTarget) +
                        " my=" +
                        std::to_string(myCnt[territoryReclaimTarget]) +
                        " enemy=" +
                        std::to_string(oppCnt[territoryReclaimTarget]));
  }
#endif

  // best_help 재배치로 실제 이동 명령을 받은 병력은 이번 턴에 이미 다른
  // 용도로 커밋된 것이므로, 뒤이은 공격 후보 선정(idle 재스캔)에서 다시
  // 뽑히면 같은 워리어에게 명령이 두 번 내려가는 셈이 된다. 그래서 이동
  // 명령을 받은 워리어를 따로 모아뒀다가 루프가 끝난 뒤 idle에서 제거한다.
  std::vector<const Warrior *> dispatchedForHelp;
  for (const Warrior *w : idle) {
    int r = w->region;

    // 집결지에 도착한 공격 병력은 STATIONARY가 되어도 계속 공격 소속이다.
    // 방어 이동 대상으로 다시 뽑지 않고 아래 공격 후보 단계까지 보존한다.
    bool reservedForAssault = cautiousMandatoryRetry()
        ? (w->purpose == WPurpose::ATTACK)
        : reclaimAssault.isCommitted(w->id.num);
#if DIRECT_ACTIVE_FIELD_RECLAIM
    // 직전 일반 공세에서 집결한 ATTACK 병력도 필수 거점의 진행 중 교전에
    // 바로 이어 붙인다. 목표 잠금이 바뀌며 committed ID가 초기화됐다는
    // 이유로 HQ 노동 보충에 빼앗기지 않게 한다.
    reservedForAssault = reservedForAssault ||
        (activeMandatoryFieldBattleNow &&
         w->purpose == WPurpose::ATTACK);
#endif
    if (reservedForAssault)
      continue;

    // 빈 거점에 도착해 건물이 지어지길 기다리는 중인 병력. 예전엔 여기서
    // 무조건 재배치 금지였지만, 실제 위협을 받는 아군 거점이 있으면 건설을
    // 미루고 방어에 동원한다. 위협이 없으면 아래 best_help가 대상을 못 찾아
    // 그대로 남아 건설을 이어간다(순수 일손 부족만으로는 끌려가지 않음).
    bool waitingBuilder = (w->purpose == WPurpose::BUILD && bld[r] == nullptr);

    // 사령부든 거점이든 구분 없이, 자기 일자리/방어 수요(need[r])만큼은
    // 우선 예약해서 남겨두고, 그 이상 남는 인원만 재배치 후보로 삼는다.
    bool designatedEmptyCaptureKeeper =
        std::find(emptyCaptureKeeperIds.begin(), emptyCaptureKeeperIds.end(),
                  w->id.num) != emptyCaptureKeeperIds.end();
    if (designatedEmptyCaptureKeeper) {
      ++kept[r];
      continue;
    }
    if (emptyCaptureHoldNeed[r] == 0 && kept[r] < need[r]) {
      ++kept[r];
      continue;
    }

    int best_help = -1;
    int min_help_h = std::numeric_limits<int>::max();
    double min_help_d = std::numeric_limits<double>::infinity();
    for (int t = 0; t < N; ++t) {
      if (t == r) continue;
      if (bld[t] != nullptr && bld[t]->side == M.my_side &&
          (homeCnt[t] + incoming[t] < need[t])) {
#if TRAIN_HQ_LABOR_INSTEAD_OF_TRANSFER
        // 평시 HQ 노동력 부족은 다른 거점의 병력을 장거리 회군시켜 채우지
        // 않는다. incoming을 늘리지 않고 남겨두면 아래 missing_workers가
        // 정확히 부족분을 HQ 신규 생산으로 잡는다. 실제 HQ 위협이 관측된
        // 경우에는 기존 방어 증원 이동을 그대로 허용한다.
        if (t == M.my_hq && threat_count[t] <= 0) continue;
#endif
        // 대기 중인 빌더는 순수 일손 부족이 아니라 실제 위협(threat_count)이
        // 있는 거점 방어에만 동원한다.
        if (waitingBuilder && threat_count[t] <= 0) continue;
        int h = hops(r, t);
        if (h > min_help_h) continue;
        if (h == min_help_h && P.dist[r][t] >= min_help_d) continue;
        min_help_h = h; min_help_d = P.dist[r][t]; best_help = t;
      }
    }

    // 빈 거점 확보(best_strong)는 뒤이은 stronghold-first 패스에서
    // 처리하므로, 여기 남는 idle 병력의 재배치는 기존 아군 건물
    // 인력 보충(best_help)만 고려하면 된다.
    int best_target = best_help;

    if (best_target == -1) {
      if (emptyCaptureHoldNeed[r] == 0 && kept[r] < need[r]) ++kept[r];
      continue;
    }

    // 목적지가 예측(threat_count) 기반 방어 수요 때문에 골라진 경우, 예측이
    // 틀리면 그 병력을 되돌릴 수 없다는 위험이 있다. 그래서 목적지까지 한
    // 번에 못 박지 않고 pickWaypoint가 계산한 경유지에만 이번 턴 이동시킨다.
    // 단, 경유지는 최대 MAX_WAYPOINT_LOSS턴만큼 도착이 늦어질 수 있어서,
    // 그 최악 지연(직행 hop + MAX_WAYPOINT_LOSS)까지 감안하면 상대가 먼저
    // 도착하는(=방어에 늦는) 임박 상황이면 회수 옵션을 포기하고 최단
    // 경로로 직행시킨다. ETA가 없는(아직 안 움직인 집결 예측) 위협은
    // 임박하지 않은 것으로 보고 경유지를 유지한다.
    int move_target = best_target;
    bool viaWaypoint = false;
    if (threat_count[best_help] > 0) {
      int directH = hops(r, best_help);
      int eta = threat_eta[best_help];
      bool imminent = (eta != std::numeric_limits<int>::max()) &&
                      (directH + MAX_WAYPOINT_LOSS >= eta);
      if (!imminent) {
        move_target = pickWaypoint(r, best_help);
        viaWaypoint = true;
      }
    }

    // best_help의 목적지는 항상 아군 건물이라 이동 비용이 무료다.
    bool needs_replacement = emptyCaptureHoldNeed[r] == 0 &&
                             (kept[r] < need[r]);
    a.moves.push_back({w->id, move_target, WPurpose::MOVE});
    dbg::move(turn, w->id, r, move_target, WPurpose::MOVE,
              "인력보충 best_help->R" + std::to_string(best_help) +
                  (viaWaypoint ? " [위협예측:경유지]"
                               : (threat_count[best_help] > 0
                                      ? " [위협임박:직행]" : "")) +
                  (needs_replacement ? " +훈련보충" : ""));
    --homeCnt[r];
    ++incoming[move_target];
    dispatchedForHelp.push_back(w);

    if (needs_replacement) {
      gold -= TRAIN_COST;
      train_reserved += TRAIN_COST;
    }
  }
  idle.erase(std::remove_if(idle.begin(), idle.end(), [&](const Warrior *w) {
    return std::find(dispatchedForHelp.begin(), dispatchedForHelp.end(), w) !=
           dispatchedForHelp.end();
  }), idle.end());
#if HOLD_EMPTY_CAPTURE_AGAINST_INCOMING
  // 위에서 야전 HOLD 담당으로 지정한 병력은 best_help뿐 아니라 뒤쪽 빈 거점
  // 파병/선택 공세 후보에서도 제외한다. need 머릿수만 남기면 다른 정렬에서
  // 같은 지역의 다른 병력을 예약하고 정작 체력 높은 담당자를 다시 공격에
  // 보내는 문제가 생긴다.
  idle.erase(std::remove_if(idle.begin(), idle.end(), [&](const Warrior *w) {
    return std::find(emptyCaptureKeeperIds.begin(),
                     emptyCaptureKeeperIds.end(), w->id.num) !=
           emptyCaptureKeeperIds.end();
  }), idle.end());
#endif

  // train_n의 실제 수요(missing_workers/baseline_military/gold_lead_military)를
  // buildNowCandidates(건설비)/reserved_build(건설 대기 부채)보다 먼저
  // 예약해 둔다. 안 그러면 새 거점을 짓거나 건설 대기 부채를 지키는 데
  // 골드가 먼저 빠져나가서, 정작 전체 병력 격차(HP 차이)를 메우는 훈련은
  // 남는 돈으로만 하게 되어 순위가 뒤바뀐다.
  int cap = static_cast<int>(my_hq_train_cap(S, M));

  int missing_workers = 0;
  for (int r = 0; r < N; ++r) {
    if (bld[r] != nullptr && bld[r]->side == M.my_side) {
      int staffed = homeCnt[r] + incoming[r];
      // need가 아니라 need_train을 쓴다: 집결 감지 예측분은 신규 훈련 수요로
      // 잡지 않고 기존 유휴 병력 재배치로만 메운다.
      if (staffed < need_train[r]) {
        missing_workers += (need_train[r] - staffed);
      }
    }
  }

  // 평소 유지 병력 규모: 머릿수가 아니라 체력 총합(myTotalHp/oppTotalHp,
  // 위에서 이미 계산해 둠)으로 비교한다. 상대는 사령부 레벨이 다르면
  // 워리어 1명의 체력도 나와 다를 수 있어서, 머릿수만 맞추면 실제 전투력은
  // 밀릴 수 있다. 다만 체력 총합만 보면 포탑 화력을 놓친다 — 포탑도
  // 매일 병력과 동일하게 공격하는 실질 전투력이므로, 내가 가진 건물 중
  // 포탑이 가장 약한 곳의 포탑 값만큼은 총 체력이 밀려도 괜찮다고 본다
  // (그 포탑 하나만큼의 화력 차이는 병력 없이도 메워진다는 뜻).
  int myWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;
  int myMinTurret = std::numeric_limits<int>::max();
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    int trt = (b.type == BType::HQ) ? HQ_LEVELS[b.level].turret
                                     : BASE_LEVELS[b.level].turret;
    myMinTurret = std::min(myMinTurret, trt);
  }
  if (myMinTurret == std::numeric_limits<int>::max()) myMinTurret = 0;
  int hpDeficit = oppTotalHp - myTotalHp - myMinTurret;
  int baseline_military = (hpDeficit <= 0) ? 0
                                            : (hpDeficit + myWarriorHp - 1) / myWarriorHp;

  // 건설비/건설 부채보다 먼저 지켜야 할 건 "상대 총 HP에 안 밀리는 것"
  // 하나뿐이다. missing_workers(일자리 보충)나 gold_lead_military(만렙+
  // 골드우위일 때 잉여 전환)는 급하지 않은 수요라 여기서 선점하지 않고,
  // 뒤쪽 최종 want 계산에서 남는 돈으로 처리되게 그대로 둔다.
  int trainBudgetWant = baseline_military;
  int trainBudgetReserved = std::min(cap, trainBudgetWant) * TRAIN_COST;
  trainBudgetReserved = std::min(trainBudgetReserved, std::max(0, gold));
  gold -= trainBudgetReserved;
  train_reserved += trainBudgetReserved;

  // ------------------------------------------------------------------
  // 거리 기반 필수 영토 + 탈환 목표 잠금
  // ------------------------------------------------------------------
  // 실제 이동 턴(hop)으로 내 본부가 더 가깝거나 같은 거점은 전부 필수
  // 영토다. 정적 승/무/패 반사실 필터는 사용하지 않는다.
  std::vector<char> mandatoryTerritory(N, 0);
  std::vector<char> opponentMandatoryTerritory(N, 0);
  std::vector<char> sharedMandatoryTerritory(N, 0);
  int myMandatoryOwned = 0;
  int oppMandatoryOwned = 0;
  int mySharedMandatoryOwned = 0;
  int oppSharedMandatoryOwned = 0;
  int neutralSharedMandatory = 0;
  for (int t : M.strongholds) {
    int myH = hops(M.my_hq, t), oppH = hops(M.opp_hq, t);
#if !DISABLE_MANDATORY_TERRITORY
    if (myH < 9999 && oppH < 9999 && myH <= oppH)
      mandatoryTerritory[t] = 1;
    if (myH < 9999 && oppH < 9999 && oppH <= myH)
      opponentMandatoryTerritory[t] = 1;
#endif
    if (mandatoryTerritory[t] && bld[t] != nullptr &&
        bld[t]->side == M.my_side)
      ++myMandatoryOwned;
    if (opponentMandatoryTerritory[t] && bld[t] != nullptr &&
        bld[t]->side != M.my_side)
      ++oppMandatoryOwned;
    // 양쪽 모두의 필수 조건(자기 HQ가 상대 HQ보다 가깝거나 같음)을
    // 동시에 만족하는 교집합은 hop이 정확히 같은 거점이다.
    if (myH >= 9999 || oppH >= 9999 || myH != oppH) continue;
    sharedMandatoryTerritory[t] = 1;
    if (bld[t] != nullptr && bld[t]->side == M.my_side)
      ++mySharedMandatoryOwned;
    else if (bld[t] != nullptr && bld[t]->side != M.my_side)
      ++oppSharedMandatoryOwned;
    else
      ++neutralSharedMandatory;
  }
  bool sharedMandatoryDeficit =
      mySharedMandatoryOwned < oppSharedMandatoryOwned;
  bool mandatoryCountDeficit = myMandatoryOwned < oppMandatoryOwned;
#if SHARED_MANDATORY_DEFICIT_RECLAIM
  dbg::note(turn, "SHARED_MANDATORY_BALANCE my=" +
                      std::to_string(mySharedMandatoryOwned) + " opp=" +
                      std::to_string(oppSharedMandatoryOwned) + " neutral=" +
                      std::to_string(neutralSharedMandatory) + " deficit=" +
                      std::to_string(sharedMandatoryDeficit ? 1 : 0));
#endif
#if MANDATORY_COUNT_DEFICIT_RECLAIM
  dbg::note(turn, "MANDATORY_COUNT_BALANCE my=" +
                      std::to_string(myMandatoryOwned) + " opp=" +
                      std::to_string(oppMandatoryOwned) + " deficit=" +
                      std::to_string(mandatoryCountDeficit ? 1 : 0));
#endif

  // 두 사령부 좌표의 중점에 가장 가까운 거점을 중앙 거점으로 본다.
  // judge.py의 center_stronghold와 같은 정의지만, 부동소수점 오차 없이
  // 2*x 좌표로 비교한다.
  int centerStronghold = -1;
  long double centerBestDist2 = std::numeric_limits<long double>::infinity();
  for (int t : M.strongholds) {
    long double dx = 2.0L * M.x[t] - M.x[M.my_hq] - M.x[M.opp_hq];
    long double dy = 2.0L * M.y[t] - M.y[M.my_hq] - M.y[M.opp_hq];
    long double d2 = dx * dx + dy * dy;
    if (d2 < centerBestDist2) {
      centerBestDist2 = d2;
      centerStronghold = t;
    }
  }

  bool allMandatorySecured = true;
  for (int t : M.strongholds) {
    if (!mandatoryTerritory[t]) continue;
    if (bld[t] == nullptr || bld[t]->side != M.my_side) {
      allMandatorySecured = false;
      break;
    }
  }
  bool ownsCenter = centerStronghold != -1 &&
                    bld[centerStronghold] != nullptr &&
                    bld[centerStronghold]->side == M.my_side;

  // 아직 어느 쪽 건물도 없고 상대 병력도 서 있지 않은 내 필수 권역은
  // 전투 없이 확보할 수 있는 대안이다. RELEASE 실험뿐 아니라 필수 탈환
  // 발동 게이트에서도 같은 정의를 공유한다.
  bool hasNeutralMandatoryAlternative = false;
  for (int t : M.strongholds) {
    if (!mandatoryTerritory[t] || bld[t] != nullptr || oppCnt[t] > 0)
      continue;
    hasNeutralMandatoryAlternative = true;
    break;
  }

#if RELEASE_INFEASIBLE_MANDATORY_RECLAIM
  // 필수 거점이라는 이유만으로 목표부터 잠근 뒤, 현재는 없는 병력을 계속
  // 생산해 언젠가 탈환한다고 가정하지 않는다. 현재 주둔군뿐 아니라 실제
  // 위치가 가장 가까운 상대 정지 예비대 한 곳과 이미 목표로 이동 중인
  // 병력을 시간축에 넣는다. 아군은 정적 계획이 요구한 신병까지 모두
  // 집결지에 건강한 상태로 모여 있다고 낙관적으로 가정한다. 이 낙관적
  // 조건에서도 점령 후 예비대를 이기지 못하면 잠금을 풀어야 하는 목표다.
  auto mandatoryReserveFeasible = [&](int target, int *plannedOut,
                                      int *reserveOut, int *trainOut,
                                      int *survivorsOut) -> bool {
    if (target < 0 || target >= N) return false;

    int staging = -1;
    int stagingH = std::numeric_limits<int>::max();
    double stagingD = std::numeric_limits<double>::infinity();
    for (const auto &base : S.buildings) {
      if (base.side != M.my_side) continue;
      int h = hops(base.region, target);
      double d = P.dist[base.region][target];
      if (h < stagingH || (h == stagingH && d < stagingD)) {
        staging = base.region;
        stagingH = h;
        stagingD = d;
      }
    }
    if (staging == -1 || stagingH >= 9999) return false;

    const Building *targetBuilding = bld[target];
    bool targetHasBuilding = targetBuilding != nullptr &&
                             targetBuilding->side != M.my_side;
    int targetHp = targetHasBuilding ? targetBuilding->hp : 0;
    int targetTurret = 0;
    if (targetHasBuilding)
      targetTurret = targetBuilding->type == BType::HQ
          ? HQ_LEVELS[targetBuilding->level].turret
          : BASE_LEVELS[targetBuilding->level].turret;

    // 빈 목표에 적이 이미 서 있고 당장 건설비가 있으면, 집결을 기다리는
    // 사이 최소 레벨 기지를 짓는 쪽이 낙관적 아군 가정보다도 현실적이다.
    if (!targetHasBuilding && targetBuilding == nullptr &&
        oppCnt[target] > 0 && S.opp_gold >= BASE_LEVELS[1].cost) {
      bool stationaryBuilder = false;
      for (const auto &w : S.warriors)
        if (w.id.side != M.my_side && w.region == target &&
            w.state == WState::STATIONARY) {
          stationaryBuilder = true;
          break;
        }
      if (stationaryBuilder) {
        targetHasBuilding = true;
        targetHp = BASE_LEVELS[1].hp;
        targetTurret = BASE_LEVELS[1].turret;
      }
    }

    std::vector<CW> targetGarrison;
    for (const auto &w : S.warriors)
      if (w.id.side != M.my_side && w.region == target)
        targetGarrison.push_back({w.hp, w.id.num});

    std::vector<int> committedHps;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side ||
          !reclaimAssault.isCommitted(w.id.num))
        continue;
      committedHps.push_back(w.hp);
    }

    // 필수 탈환 중 실제 동원 규칙과 같이 각 건물의 노동 인원만 남기고,
    // 이미 작전에 배정된 병력은 노동 예약과 무관하게 공격 후보로 센다.
    std::vector<int> freshHps;
    std::vector<int> keptWorkers(N, 0);
    for (const Warrior *w : idle) {
      if (reclaimAssault.isCommitted(w->id.num)) continue;
      const Building *home = bld[w->region];
      int keep = home != nullptr && home->side == M.my_side
          ? home->work_cap()
          : 0;
      if (keptWorkers[w->region] < keep) {
        ++keptWorkers[w->region];
        continue;
      }
      freshHps.push_back(w->hp);
    }
    std::sort(freshHps.begin(), freshHps.end(), std::greater<int>());

    int mandatoryMargin = MANDATORY_RECLAIM_INITIAL_MARGIN;
#if MANDATORY_MARGIN_TWO_WHEN_BASE_AHEAD
    int myBases = 0, oppBases = 0;
    for (const auto &base : S.buildings) {
      if (base.type != BType::BASE) continue;
      if (base.side == M.my_side) ++myBases;
      else ++oppBases;
    }
    if (myBases > oppBases)
      mandatoryMargin = std::max(
          mandatoryMargin, MANDATORY_RECLAIM_AHEAD_MARGIN);
#endif
    int myHp = HQ_LEVELS[myHqLevel].warrior_hp;
    int trainCap = std::max(
        1, my_hq_train_cap(S, M) * std::max(0, MAX_TURN - turn));
    AttackPlan reservePlan = plan_attack_force(
        targetHp, targetTurret, targetGarrison, committedHps, freshHps,
        myHp, trainCap, mandatoryMargin, targetHasBuilding);
    if (trainOut != nullptr) *trainOut = reservePlan.extraToTrain;
    if (reservePlan.extraToTrain < 0) return false;
    int planned = (int)committedHps.size() + reservePlan.sendCount +
                  reservePlan.extraToTrain;
    if (plannedOut != nullptr) *plannedOut = planned;
    if (planned <= 0) return false;

    int horizon = std::max(0, std::min(60, MAX_TURN - turn - 1));
    std::vector<StreamArrival> attackers, defenders;
    int ownArrival = std::max(0, stagingH - 1);
    for (int i = 0; i < planned; ++i)
      attackers.push_back({ownArrival, myHp, 6000000 + i});

    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side) continue;
      if (w.region == target) {
        defenders.push_back({0, w.hp, w.id.num});
        continue;
      }
      if (w.state != WState::MOVING || w.prev_region < 0 ||
          w.prev_region == w.region ||
          P.nxt[w.prev_region][target] != w.region)
        continue;
      int h = hops(w.region, target);
      if (h < 9999)
        defenders.push_back({std::max(0, h - 1), w.hp, w.id.num});
    }

    // 모든 지역의 병력을 목표에 순간이동시키지 않는다. 실제 정지 병력이
    // 있는 상대 건물 중 목표까지 가장 가까운 한 곳만 대응 예비대로 본다.
    int reserveRegion = -1;
    int reserveH = std::numeric_limits<int>::max();
    double reserveD = std::numeric_limits<double>::infinity();
    for (const auto &base : S.buildings) {
      if (base.side == M.my_side || base.region == target) continue;
      bool hasStationary = false;
      for (const auto &w : S.warriors)
        if (w.id.side != M.my_side && w.region == base.region &&
            w.state == WState::STATIONARY) {
          hasStationary = true;
          break;
        }
      if (!hasStationary) continue;
      int h = hops(base.region, target);
      double d = P.dist[base.region][target];
      if (h < reserveH || (h == reserveH && d < reserveD)) {
        reserveRegion = base.region;
        reserveH = h;
        reserveD = d;
      }
    }
    int reserveCount = 0;
    if (reserveRegion != -1 && reserveH < 9999) {
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side || w.region != reserveRegion ||
            w.state != WState::STATIONARY)
          continue;
        defenders.push_back({reserveH, w.hp, w.id.num});
        ++reserveCount;
      }
    }
    if (reserveOut != nullptr) *reserveOut = reserveCount;

    StreamCombatForecast forecast = simulate_reinforcement_stream(
        targetHp, targetTurret, targetHasBuilding, std::move(attackers),
        std::move(defenders), horizon, true);
    if (survivorsOut != nullptr)
      *survivorsOut = forecast.attackerSurvivors;
    return forecast.captured && forecast.attackerSurvivors > 0;
  };

  // 탈환을 포기해서 생기는 자유 병력이 실제로 할 일이 있을 때만 잠금을
  // 푼다. 아직 내 필수 권역에 적 없는 빈 거점이 하나라도 있으면 건설로
  // 전환할 수 있다. 반대로 빈 확장지가 모두 사라진 후반에는 단지 예비대가
  // 보인다는 이유만으로 필수 탈환을 영구 회피하지 않고 기존 작전을 유지한다.
  int rejectedMandatoryTarget = -1;
  if (territoryReclaimTarget != -1 && !territoryReclaimIsExpansion &&
      !reclaimAssault.launched && hasNeutralMandatoryAlternative) {
    const Building *locked = bld[territoryReclaimTarget];
    bool enemyStillThere =
        (locked != nullptr && locked->side != M.my_side) ||
        (locked == nullptr && oppCnt[territoryReclaimTarget] > 0);
    if (enemyStillThere) {
      int planned = 0, reserve = 0, train = 0, survivors = 0;
      bool feasible = mandatoryReserveFeasible(
          territoryReclaimTarget, &planned, &reserve, &train, &survivors);
      dbg::note(turn, "RECLAIM_RESERVE_RECHECK target=R" +
                          std::to_string(territoryReclaimTarget) +
                          " result=" + (feasible ? "CAPTURE" : "FAIL") +
                          " planned=" + std::to_string(planned) +
                          " reserve=" + std::to_string(reserve) +
                          " train=" + std::to_string(train) +
                          " survivors=" + std::to_string(survivors));
      if (!feasible && train != 0) {
        rejectedMandatoryTarget = territoryReclaimTarget;
        dbg::note(turn, "RECLAIM_MANDATORY_RELEASE target=R" +
                            std::to_string(territoryReclaimTarget));
        territoryReclaimTarget = -1;
        territoryReclaimIsExpansion = false;
        reclaimAssault.reset(-1, turn);
      }
    }
  }
#endif

  int offensiveReserveWant = 0;
#if PRETRAIN_SAFE_OFFENSIVE_RESERVE
  // 필수 권역과 중앙을 모두 확보한 뒤에도 정적 HQ 경쟁만으로 이기지
  // 못한다면, 상대 전투 HP에 맞추는 데서 생산을 멈추지 않고 다음 공세용
  // 예비대를 미리 만든다. 단, 지금 명령한 HQ 업그레이드까지 반영한 정적
  // HQ 결과(승/무/패, 최종 레벨/HP)를 악화시키지 않는 수량만 허용한다.
  // 이로써 HQ에 실제로 쓸 수 있는 골드까지 병력으로 바꾸지는 않는다.
  if (allMandatorySecured && ownsCenter && !hqBehind &&
      turn < optionalPushCooldownUntil &&
      passiveHq.verdict != PassiveHqVerdict::WIN &&
      baseline_military == 0 && missing_workers == 0) {
    int desiredReserve = std::max(1, SAFE_OFFENSIVE_RESERVE_TARGET);
    int reserveHpGap = oppTotalHp + desiredReserve * myWarriorHp - myTotalHp;
    int desiredTrain = reserveHpGap <= 0
        ? 0
        : (reserveHpGap + myWarriorHp - 1) / myWarriorHp;
    desiredTrain = std::min(cap, desiredTrain);

    GameState passiveBase = S;
    passiveBase.gold = std::max(0, gold);
    for (int region : a.upgrades) {
      Building *upgrade = find_building(passiveBase, region);
      if (upgrade != nullptr && upgrade->side == M.my_side &&
          upgrade->type == BType::HQ && upgrade->level < HQ_MAX_LEVEL)
        apply_upgrade(*upgrade);
    }
    PassiveHqResult reserveReference =
        evaluate_passive_hq_race(passiveBase, M, turn);
    int safeTrain = 0;
    int affordable = std::min(cap, passiveBase.gold / TRAIN_COST);
    for (int count = 1; count <= affordable; ++count) {
      GameState trial = passiveBase;
      trial.gold -= count * TRAIN_COST;
      const Building *trialHq = find_building(trial, M.my_hq);
      int trialHp = trialHq != nullptr
          ? HQ_LEVELS[trialHq->level].warrior_hp
          : myWarriorHp;
      for (int i = 0; i < count; ++i)
        trial.warriors.push_back(
            Warrior{.id = {M.my_side, 9000000 + i},
                    .region = M.my_hq, .hp = trialHp});
      PassiveHqResult after = evaluate_passive_hq_race(trial, M, turn);
      bool verdictPreserved =
          static_cast<int>(after.verdict) >=
          static_cast<int>(reserveReference.verdict);
      bool hqPreserved = after.my_final_level >=
                              reserveReference.my_final_level &&
                          after.my_final_hp >= reserveReference.my_final_hp;
      if (!verdictPreserved || !hqPreserved) break;
      safeTrain = count;
    }
    offensiveReserveWant = std::min(desiredTrain, safeTrain);

    // baseline_military는 위에서 이미 예약됐다. 두 수요는 같은 훈련 슬롯을
    // 공유하므로 합산하지 않고 더 큰 쪽까지의 차이만 추가 예약한다.
    int oldReservedCount = std::min(cap, trainBudgetWant);
    int newReservedCount =
        std::min(cap, std::max(trainBudgetWant, offensiveReserveWant));
    int extraReserveCount = std::max(0, newReservedCount - oldReservedCount);
    int extraReserveGold = std::min(
        extraReserveCount * TRAIN_COST, std::max(0, gold));
    gold -= extraReserveGold;
    train_reserved += extraReserveGold;
    dbg::note(turn, "OFFENSIVE_RESERVE_PRETRAIN passive=" +
                        std::string(passive_hq_verdict_name(
                            reserveReference.verdict)) +
                        " desired=" + std::to_string(desiredTrain) +
                        " safe=" + std::to_string(safeTrain) +
                        " want=" + std::to_string(offensiveReserveWant) +
                        " hp=" + std::to_string(myTotalHp) + "/" +
                        std::to_string(oppTotalHp));
  }
#endif

  // 한 번 빼앗긴 필수 영토를 고르면, 아군 기지를 완공할 때까지 목표를
  // 바꾸지 않는다. 적을 제거해 빈 땅이 된 중간 단계에서도 잠금을 유지해
  // 곧바로 건설까지 마친 뒤에야 정상 확장으로 돌아간다.
  if (territoryReclaimTarget != -1) {
    const Building *locked = bld[territoryReclaimTarget];
    bool completed = locked != nullptr && locked->side == M.my_side;
    bool invalidReclaim = !territoryReclaimIsExpansion &&
#if SHARED_MANDATORY_DEFICIT_RECLAIM
                          !sharedMandatoryTerritory[territoryReclaimTarget];
#else
                          !mandatoryTerritory[territoryReclaimTarget];
#endif
    // 상대 진영 확장 중 중앙/필수 영토를 잃으면 확장을 즉시 포기한다.
    // 다음 선택 패스에서 빼앗긴 필수 영토가 다시 최우선 목표가 된다.
    bool lostExpansionGate = territoryReclaimIsExpansion &&
                             (!allMandatorySecured || !ownsCenter);
    if (completed || invalidReclaim || lostExpansionGate) {
      if (completed && !territoryReclaimIsExpansion)
        dbg::note(turn, "RECLAIM_MANDATORY_RESOLVED target=R" +
                            std::to_string(territoryReclaimTarget) +
                            " engaged=" +
                            std::to_string(reclaimAssault.engagedReclaimTarget ? 1 : 0) +
                            " failed_waves=" +
                            std::to_string(reclaimAssault.failedWaves));
      if (territoryReclaimIsExpansion && completed)
        optionalPushCooldownUntil = std::max(optionalPushCooldownUntil, turn + 15);
      territoryReclaimTarget = -1;
      territoryReclaimIsExpansion = false;
    }
  }

  bool allowForcedMandatoryReclaim = true;
#if SKIP_FORCED_MANDATORY_RECLAIM_WHEN_BASE_AHEAD
  int mandatoryGateMyBases = 0, mandatoryGateOppBases = 0;
  for (const auto &base : S.buildings) {
    if (base.type != BType::BASE) continue;
    if (base.side == M.my_side)
      ++mandatoryGateMyBases;
    else
      ++mandatoryGateOppBases;
  }
  if (mandatoryGateMyBases > mandatoryGateOppBases) {
    allowForcedMandatoryReclaim = false;
    if (territoryReclaimTarget == -1)
      dbg::note(turn, "RECLAIM_MANDATORY_SKIP_BASE_AHEAD my=" +
                          std::to_string(mandatoryGateMyBases) + " opp=" +
                          std::to_string(mandatoryGateOppBases));
  }
#endif

#if DEFER_MANDATORY_RECLAIM_WHILE_NEUTRAL_MANDATORY
  // 싸워서 되찾을 필수 거점보다 공짜로 지을 수 있는 필수 거점을 먼저
  // 처리한다. 이미 시작한 작전을 취소하지 않고, 새 강제 탈환의 발동만
  // 빈 필수 거점이 모두 사라질 때까지 미룬다. 단, 실험 임계값을 넘을
  // 만큼 상대 확장이 진행된 후반에는 공짜 거점이 남았다는 이유로 전투를
  // 계속 피하지 않는다. 여기서 BASE 수는 HQ를 제외한다.
  int deferGateOppBases = 0;
  for (const auto &base : S.buildings)
    if (base.side != M.my_side && base.type == BType::BASE)
      ++deferGateOppBases;
  if (territoryReclaimTarget == -1 && hasNeutralMandatoryAlternative &&
      deferGateOppBases <= DEFER_MANDATORY_RECLAIM_OPP_BASE_MAX) {
    allowForcedMandatoryReclaim = false;
    dbg::note(turn, "RECLAIM_MANDATORY_DEFER_NEUTRAL opp_bases=" +
                        std::to_string(deferGateOppBases) + " max=" +
                        std::to_string(
                            DEFER_MANDATORY_RECLAIM_OPP_BASE_MAX));
  }
#endif

  // 별도 발동 조건 실험: 빈 거점 존재 여부와 무관하게 상대 BASE 수가
  // 임계값 이하일 때만 새 강제 필수 탈환을 허용한다. 이미 시작한 탈환은
  // 상대가 네 번째 BASE를 지었다는 이유로 중간 취소하지 않는다.
  int mandatoryGateOppBaseCount = 0;
  for (const auto &base : S.buildings)
    if (base.side != M.my_side && base.type == BType::BASE)
      ++mandatoryGateOppBaseCount;
  if (territoryReclaimTarget == -1 &&
      mandatoryGateOppBaseCount > FORCED_MANDATORY_RECLAIM_OPP_BASE_MAX) {
    allowForcedMandatoryReclaim = false;
    dbg::note(turn, "RECLAIM_MANDATORY_SKIP_OPP_BASE_COUNT opp=" +
                        std::to_string(mandatoryGateOppBaseCount) + " max=" +
                        std::to_string(FORCED_MANDATORY_RECLAIM_OPP_BASE_MAX));
  }

  if (territoryReclaimTarget == -1 && allowForcedMandatoryReclaim &&
      !DISABLE_FORCED_MANDATORY_RECLAIM
#if SHARED_MANDATORY_DEFICIT_RECLAIM
      && sharedMandatoryDeficit
#elif MANDATORY_COUNT_DEFICIT_RECLAIM
      && mandatoryCountDeficit
#endif
  ) {
    int bestH = std::numeric_limits<int>::max();
    double bestD = std::numeric_limits<double>::infinity();
    for (int t : M.strongholds) {
#if RELEASE_INFEASIBLE_MANDATORY_RECLAIM
      if (t == rejectedMandatoryTarget) continue;
#endif
#if SHARED_MANDATORY_DEFICIT_RECLAIM
      if (!sharedMandatoryTerritory[t]) continue;
#else
      if (!mandatoryTerritory[t]) continue;
#endif
      bool enemyBuilding = (bld[t] != nullptr && bld[t]->side != M.my_side);
      bool enemyOccupation = (bld[t] == nullptr && oppCnt[t] > 0);
      if (!enemyBuilding && !enemyOccupation) continue;
#if RELEASE_INFEASIBLE_MANDATORY_RECLAIM
      if (hasNeutralMandatoryAlternative) {
        int planned = 0, reserve = 0, train = 0, survivors = 0;
        bool feasible = mandatoryReserveFeasible(
            t, &planned, &reserve, &train, &survivors);
        dbg::note(turn, "RECLAIM_RESERVE_GATE target=R" +
                            std::to_string(t) + " result=" +
                            (feasible ? "CAPTURE" : "FAIL") +
                            " planned=" + std::to_string(planned) +
                            " reserve=" + std::to_string(reserve) +
                            " train=" + std::to_string(train) +
                            " survivors=" + std::to_string(survivors));
        if (!feasible && train != 0) continue;
      }
#endif
      int h = hops(M.my_hq, t);
      double d = P.dist[M.my_hq][t];
      if (h > bestH || (h == bestH && d >= bestD)) continue;
      bestH = h;
      bestD = d;
      territoryReclaimTarget = t;
      territoryReclaimIsExpansion = false;
    }
    if (territoryReclaimTarget != -1)
      dbg::note(turn, "RECLAIM_MANDATORY_TRIGGER target=R" +
                          std::to_string(territoryReclaimTarget));
  }

#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
  // 작전이 끝났거나 필수 탈환으로 전환됐으면 다음 선택 공세는 새 집결지를
  // 고른다. 기존 집결지가 파괴된 경우에도 현재 건물 상태로 다시 고른다.
  if (territoryReclaimTarget == -1 || !territoryReclaimIsExpansion) {
    optionalPushFixedRally = -1;
  } else if (optionalPushFixedRally != -1) {
    const Building *rallyBuilding = bld[optionalPushFixedRally];
    if (rallyBuilding == nullptr || rallyBuilding->side != M.my_side) {
      dbg::note(turn, "PUSH_DYNAMIC_RALLY_LOST old=R" +
                          std::to_string(optionalPushFixedRally));
      optionalPushFixedRally = -1;
    }
  }
#endif

#if RESELECT_OPTIONAL_PUSH_BEFORE_LAUNCH
  // 선택 공세는 아직 첫 파동이 출발하지 않았다면 오래된 목표를 고집하지
  // 않는다. 현재 병력/수비 상태로 아래 후보 점수를 매 턴 다시 계산한다.
  // 필수 거점 탈환과 이미 출발한 공세는 기존 잠금을 그대로 유지한다.
  int previousOptionalPushTarget = -1;
  if (territoryReclaimTarget != -1 && territoryReclaimIsExpansion &&
      !reclaimAssault.launched) {
    previousOptionalPushTarget = territoryReclaimTarget;
    territoryReclaimTarget = -1;
    territoryReclaimIsExpansion = false;
  }
#endif

#if DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
  // 이번 턴 전체 후보를 같은 시간축으로 비교해 얻은 범용 공세 계획이다.
  // 아래 실행 단계에서도 이 인원/훈련량을 그대로 사용해 목표 선택과 병력
  // 준비가 다시 갈라지지 않게 한다.
  int dynamicOptionalPlanTarget = -1;
  int dynamicOptionalPlanForce = -1;
  int dynamicOptionalPlanTrain = -1;
  int dynamicOptionalPlanCaptureDay = -1;
#endif

  // 내 쪽 필수 영토를 전부 완공했고 중앙 거점도 소유한 뒤에는 상대
  // 거점 하나를 더 고른다. 현재 유휴 병력과 남은 턴 동안의 HQ 훈련
  // 가능량으로 정적 전투 시뮬레이션을 통과하는 목표만 선택한다.
  // 목표가 정해진 뒤의 행동은 필수 영토 탈환과 완전히 동일하다.
  if (territoryReclaimTarget == -1 && allMandatorySecured && ownsCenter &&
      turn >= optionalPushCooldownUntil) {
    // 목표 선별과 실제 실행에서 동일한 방어 예약량(need)을 사용한다.
    // 예전에는 여기서 노동자만 남겨 공격 가능하다고 판정한 뒤, 실행 단계는
    // 위협 방어 인원까지 남겨 실제 가용 병력이 0명이 되는 불일치가 있었다.
    std::vector<const Warrior *> pushPool;
    std::vector<int> keptWorkers(N, 0);
    for (const Warrior *w : idle) {
      int keep = need[w->region];
      if (reclaimAssault.isCommitted(w->id.num)) {
        pushPool.push_back(w);
        continue;
      }
      if (keptWorkers[w->region] < keep) {
        ++keptWorkers[w->region];
        continue;
      }
      pushPool.push_back(w);
    }

#if DYNAMIC_OPTIONAL_PUSH_CENTER_RALLY
    // 선택 공세의 집결지는 HQ와 공격 목표의 위치를 보지 않고, 현재 아군
    // 건물 중 맵 중앙 거점에 가장 가까운 곳으로 고정한다.
    if (optionalPushFixedRally == -1) {
      int bestCenterHop = std::numeric_limits<int>::max();
      double bestCenterDistance = std::numeric_limits<double>::infinity();
      for (const auto &myB : S.buildings) {
        if (myB.side != M.my_side) continue;
        int centerHop = hops(myB.region, centerStronghold);
        if (centerHop >= 9999) continue;
        double centerDistance = P.dist[myB.region][centerStronghold];
        if (centerHop > bestCenterHop ||
            (centerHop == bestCenterHop &&
             centerDistance >= bestCenterDistance))
          continue;
        optionalPushFixedRally = myB.region;
        bestCenterHop = centerHop;
        bestCenterDistance = centerDistance;
      }
      dbg::note(turn, "PUSH_DYNAMIC_RALLY_CENTER start=R" +
                          std::to_string(optionalPushFixedRally) +
                          " center=R" + std::to_string(centerStronghold) +
                          " center_h=" + std::to_string(bestCenterHop));
    }
#endif

#if DYNAMIC_OPTIONAL_PUSH_HQ_CENTER_RALLY
    // 선택 공세의 집결지를 첫 공격 목표에 종속시키지 않는다. 아군 건물 중
    // 내 HQ와 맵 중앙 거점까지의 hop이 양쪽 모두 짧은 곳을 고른다.
    // max(hqHop, centerHop)를 먼저 최소화해 어느 한쪽에만 치우친 후보를
    // 피하고, 합과 실제 거리 합으로 타이브레이크한다.
    if (optionalPushFixedRally == -1) {
      int bestBalancedHop = std::numeric_limits<int>::max();
      int bestHopSum = std::numeric_limits<int>::max();
      double bestDistanceSum = std::numeric_limits<double>::infinity();
      for (const auto &myB : S.buildings) {
        if (myB.side != M.my_side) continue;
        int hqHop = hops(myB.region, M.my_hq);
        int centerHop = hops(myB.region, centerStronghold);
        if (hqHop >= 9999 || centerHop >= 9999) continue;
        int balancedHop = std::max(hqHop, centerHop);
        int hopSum = hqHop + centerHop;
        double distanceSum = P.dist[myB.region][M.my_hq] +
                             P.dist[myB.region][centerStronghold];
        if (balancedHop > bestBalancedHop ||
            (balancedHop == bestBalancedHop && hopSum > bestHopSum) ||
            (balancedHop == bestBalancedHop && hopSum == bestHopSum &&
             distanceSum >= bestDistanceSum))
          continue;
        optionalPushFixedRally = myB.region;
        bestBalancedHop = balancedHop;
        bestHopSum = hopSum;
        bestDistanceSum = distanceSum;
      }
      dbg::note(turn, "PUSH_DYNAMIC_RALLY_HQ_CENTER start=R" +
                          std::to_string(optionalPushFixedRally) +
                          " hq=R" + std::to_string(M.my_hq) +
                          " center=R" + std::to_string(centerStronghold) +
                          " max_h=" + std::to_string(bestBalancedHop) +
                          " sum_h=" + std::to_string(bestHopSum));
    }
#endif

#if DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
    // 첫 목표의 최근접 거점을 집결지로 쓰면 목표가 바뀌었을 때 다른 모든
    // 후보가 지나치게 멀어질 수 있다. 현재 상대 후보 전체까지의 hop 합,
    // 최악 hop 순으로 가장 중앙적인 아군 건물을 한 번 골라 작전 동안
    // 고정한다.
    if (optionalPushFixedRally == -1) {
      long long bestRallySum = std::numeric_limits<long long>::max();
      int bestRallyWorst = std::numeric_limits<int>::max();
      double bestRallyDist = std::numeric_limits<double>::infinity();
      for (const auto &myB : S.buildings) {
        if (myB.side != M.my_side) continue;
        long long sum = 0;
        int worst = 0;
        double distSum = 0.0;
        int targets = 0;
        bool reachable = true;
        for (int t : M.strongholds) {
          const Building *tb = bld[t];
          bool enemyBuilding = tb != nullptr && tb->side != M.my_side;
          bool enemyOccupation = tb == nullptr && oppCnt[t] > 0;
          if (!enemyBuilding && !enemyOccupation) continue;
          int h = hops(myB.region, t);
          if (h >= 9999) {
            reachable = false;
            break;
          }
          sum += h;
          worst = std::max(worst, h);
          distSum += P.dist[myB.region][t];
          ++targets;
        }
        if (!reachable || targets == 0) continue;
        if (sum > bestRallySum ||
            (sum == bestRallySum && worst > bestRallyWorst) ||
            (sum == bestRallySum && worst == bestRallyWorst &&
             distSum >= bestRallyDist))
          continue;
        optionalPushFixedRally = myB.region;
        bestRallySum = sum;
        bestRallyWorst = worst;
        bestRallyDist = distSum;
      }
      dbg::note(turn, "PUSH_DYNAMIC_RALLY_MEDOID start=R" +
                          std::to_string(optionalPushFixedRally));
    }

    struct DynamicOffensiveUnit {
      int hp = 0;
      int gatherDay = 0;
      int num = 0;
    };
    std::vector<DynamicOffensiveUnit> dynamicRoster;
    std::vector<int> dynamicRosterIds;
    auto addDynamicRoster = [&](const Warrior &w) {
      if (std::find(dynamicRosterIds.begin(), dynamicRosterIds.end(),
                    w.id.num) != dynamicRosterIds.end())
        return;
      int gatherDay = 0;
      if (w.region != optionalPushFixedRally) {
        if (w.state == WState::MOVING) {
          int finishCurrent = hops(w.region, w.target);
          int thenRally = hops(w.target, optionalPushFixedRally);
          if (finishCurrent >= 9999 || thenRally >= 9999) return;
          gatherDay = finishCurrent + thenRally;
        } else {
          gatherDay = hops(w.region, optionalPushFixedRally);
          if (gatherDay >= 9999) return;
        }
      }
      dynamicRoster.push_back({w.hp, gatherDay, w.id.num});
      dynamicRosterIds.push_back(w.id.num);
    };
    for (const auto &w : S.warriors)
      if (w.id.side == M.my_side && w.hp > 0 &&
          reclaimAssault.isCommitted(w.id.num))
        addDynamicRoster(w);
    for (const Warrior *w : pushPool) addDynamicRoster(*w);
    std::sort(dynamicRoster.begin(), dynamicRoster.end(),
              [](const DynamicOffensiveUnit &x,
                 const DynamicOffensiveUnit &y) {
                if (x.gatherDay != y.gatherDay)
                  return x.gatherDay < y.gatherDay;
                if (x.hp != y.hp) return x.hp > y.hp;
                return x.num < y.num;
              });
#endif

    int pushTrainCap = std::max(1, my_hq_train_cap(S, M) * (MAX_TURN - turn));
    int bestReadyTurns = std::numeric_limits<int>::max();
    int bestForce = std::numeric_limits<int>::max();
    double bestFrontDist = std::numeric_limits<double>::infinity();

    for (int t : M.strongholds) {
      const Building *tb = bld[t];
      bool enemyBuilding = tb != nullptr && tb->side != M.my_side;
      bool enemyOccupation = tb == nullptr && oppCnt[t] > 0;
      if (!enemyBuilding && !enemyOccupation) continue;

      bool simulatedEnemyBuilding = enemyBuilding;
      int simulatedEnemyHp = enemyBuilding ? tb->hp : 0;
#ifdef UNIFIED_RECLAIM_STREAM
      bool predictedEmptyFortify = !enemyBuilding && enemyOccupation &&
          opponent_can_fortify_empty_target_now(S, M, t);
      if (predictedEmptyFortify) {
        simulatedEnemyBuilding = true;
        simulatedEnemyHp = BASE_LEVELS[1].hp;
      }
#endif

      int turret = 0;
      if (simulatedEnemyBuilding)
#ifdef UNIFIED_RECLAIM_STREAM
        turret = predictedEmptyFortify
            ? BASE_LEVELS[1].turret
            : ((tb->type == BType::HQ) ? HQ_LEVELS[tb->level].turret
                                        : BASE_LEVELS[tb->level].turret);
#else
        turret = (tb->type == BType::HQ) ? HQ_LEVELS[tb->level].turret
                                         : BASE_LEVELS[tb->level].turret;
#endif
      std::vector<CW> garrison;
      for (const auto &w : S.warriors)
        if (w.id.side != M.my_side && w.region == t)
          garrison.push_back({w.hp, w.id.num});

      std::vector<int> fixedHps;
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side) continue;
        if ((w.state == WState::STATIONARY && w.region == t) ||
            (w.state == WState::MOVING && w.purpose == WPurpose::ATTACK))
          fixedHps.push_back(w.hp);
      }
      std::vector<int> freshHps;
      for (const Warrior *w : pushPool)
        if (w->region != t) freshHps.push_back(w->hp);
      std::sort(freshHps.begin(), freshHps.end(), std::greater<int>());

      int candidateReadyTurns = std::numeric_limits<int>::max();
      int candidateForce = std::numeric_limits<int>::max();
      int candidateTrain = -1;
      int candidateSurvivors = -1;
      int frontH = std::numeric_limits<int>::max();
      double frontD = std::numeric_limits<double>::infinity();
#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
      // 첫 턴에는 기존 규칙으로 목표의 최근접 아군 건물을 찾고, 작전이
      // 만들어진 다음 턴부터는 고정 집결지에서 각 후보까지의 실제 거리로
      // 모든 목표를 비교한다.
      if (optionalPushFixedRally != -1) {
        frontH = hops(optionalPushFixedRally, t);
        frontD = P.dist[optionalPushFixedRally][t];
      } else
#endif
      {
        for (const auto &myB : S.buildings) {
          if (myB.side != M.my_side) continue;
          int h = hops(myB.region, t);
          double d = P.dist[myB.region][t];
          if (h < frontH || (h == frontH && d < frontD)) {
            frontH = h;
            frontD = d;
          }
        }
      }
      if (frontH >= 9999) continue;
#if DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
      // 목표에 이미 있는 병력과 실제로 목표 방향으로 이동 중인 병력은
      // 현재 시점 기준 도착일에 넣는다. 아직 반응하지 않은 정지 예비대는
      // 상대가 출격을 본 다음에 움직인다고 보고, 가장 가까운 한 건물의
      // work_cap 초과 병력만 실제 출격 예정일 + hop에 합류시킨다.
      std::vector<StreamArrival> baseDefenderArrivals;
      std::vector<int> includedDefenders;
      auto addDynamicDefender = [&](int day, const Warrior &w) {
        if (std::find(includedDefenders.begin(), includedDefenders.end(),
                      w.id.num) != includedDefenders.end())
          return;
        baseDefenderArrivals.push_back(
            {std::max(0, day), w.hp, w.id.num});
        includedDefenders.push_back(w.id.num);
      };
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side) continue;
        if (w.region == t) {
          addDynamicDefender(0, w);
          continue;
        }
        if (w.state != WState::MOVING || w.prev_region < 0 ||
            w.prev_region == w.region ||
            P.nxt[w.prev_region][t] != w.region)
          continue;
        int h = hops(w.region, t);
        if (h < 9999) addDynamicDefender(std::max(0, h - 1), w);
      }

      int reserveRegion = -1;
      int reserveH = std::numeric_limits<int>::max();
      double reserveD = std::numeric_limits<double>::infinity();
      std::vector<const Warrior *> reserveUnits;
      for (const auto &eb : S.buildings) {
        if (eb.side == M.my_side || eb.region == t) continue;
        std::vector<const Warrior *> stationed;
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side && w.region == eb.region &&
              w.state == WState::STATIONARY)
            stationed.push_back(&w);
        std::sort(stationed.begin(), stationed.end(),
                  [](const Warrior *x, const Warrior *y) {
                    return x->id.num < y->id.num;
                  });
        if ((int)stationed.size() <= eb.work_cap()) continue;
        int h = hops(eb.region, t);
        double d = P.dist[eb.region][t];
        if (h < reserveH || (h == reserveH && d < reserveD)) {
          reserveRegion = eb.region;
          reserveH = h;
          reserveD = d;
          reserveUnits.assign(stationed.begin() + eb.work_cap(),
                              stationed.end());
        }
      }

      int trainPerTurn = std::max(1, my_hq_train_cap(S, M));
      int maxForce = std::min(
          40, (int)dynamicRoster.size() + pushTrainCap);
      int attackTravel = hops(optionalPushFixedRally, t);
      if (attackTravel >= 9999) continue;
      int horizon = std::max(0, std::min(60, MAX_TURN - turn - 1));
      for (int force = 1; force <= maxForce; ++force) {
        int existing = std::min(force, (int)dynamicRoster.size());
        int extraTrain = force - existing;
        int launchDay = 0;
        std::vector<int> selectedHps;
        selectedHps.reserve(force);
        for (int i = 0; i < existing; ++i) {
          launchDay = std::max(launchDay, dynamicRoster[i].gatherDay);
          selectedHps.push_back(dynamicRoster[i].hp);
        }
        for (int i = 0; i < extraTrain; ++i) {
          int trainDay = i / trainPerTurn + 1;
          int gatherDay = trainDay + hops(M.my_hq, optionalPushFixedRally);
          launchDay = std::max(launchDay, gatherDay);
          selectedHps.push_back(HQ_LEVELS[myHqLevel].warrior_hp);
        }
        int attackArrival = launchDay + std::max(0, attackTravel - 1);
        std::vector<StreamArrival> attackers, defenders = baseDefenderArrivals;
        for (int i = 0; i < force; ++i)
          attackers.push_back(
              {attackArrival, selectedHps[i], 8100000 + i});
        if (reserveRegion != -1 && reserveH < 9999)
          for (const Warrior *w : reserveUnits)
            defenders.push_back(
                {launchDay + reserveH, w->hp, w->id.num});
        StreamCombatForecast forecast = simulate_reinforcement_stream(
            simulatedEnemyHp, turret, simulatedEnemyBuilding,
            std::move(attackers), std::move(defenders), horizon, true);
        if (!forecast.captured || forecast.attackerSurvivors < 1) continue;
        bool betterForce =
            candidateReadyTurns == std::numeric_limits<int>::max() ||
            forecast.captureDay < candidateReadyTurns ||
            (forecast.captureDay == candidateReadyTurns &&
             extraTrain < candidateTrain) ||
            (forecast.captureDay == candidateReadyTurns &&
             extraTrain == candidateTrain && force < candidateForce) ||
            (forecast.captureDay == candidateReadyTurns &&
             extraTrain == candidateTrain && force == candidateForce &&
             forecast.attackerSurvivors > candidateSurvivors);
        if (!betterForce) continue;
        candidateReadyTurns = forecast.captureDay;
        candidateForce = force;
        candidateTrain = extraTrain;
        candidateSurvivors = forecast.attackerSurvivors;
      }
      if (candidateReadyTurns == std::numeric_limits<int>::max()) continue;
#else
      AttackPlan pushPlan = plan_attack_force(
          simulatedEnemyHp, turret, garrison, fixedHps, freshHps,
          myWarriorHp, pushTrainCap, 1, simulatedEnemyBuilding);
      if (pushPlan.extraToTrain < 0) continue;
      int perTurn = std::max(1, my_hq_train_cap(S, M));
      int trainTurns = (pushPlan.extraToTrain + perTurn - 1) / perTurn;
      candidateReadyTurns = frontH + trainTurns;
      candidateForce = pushPlan.sendCount + pushPlan.extraToTrain;
      candidateTrain = pushPlan.extraToTrain;
#endif
      if (candidateReadyTurns > bestReadyTurns) continue;
      if (candidateReadyTurns == bestReadyTurns &&
          candidateForce > bestForce)
        continue;
      if (candidateReadyTurns == bestReadyTurns &&
          candidateForce == bestForce &&
          frontD >= bestFrontDist)
        continue;
      bestReadyTurns = candidateReadyTurns;
      bestForce = candidateForce;
      bestFrontDist = frontD;
      territoryReclaimTarget = t;
      territoryReclaimIsExpansion = true;
#if DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
      dynamicOptionalPlanTarget = t;
      dynamicOptionalPlanForce = candidateForce;
      dynamicOptionalPlanTrain = candidateTrain;
      dynamicOptionalPlanCaptureDay = candidateReadyTurns;
#endif
    }

    if (territoryReclaimTarget != -1) {
#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
      if (optionalPushFixedRally == -1) {
        int rallyH = std::numeric_limits<int>::max();
        double rallyD = std::numeric_limits<double>::infinity();
        for (const auto &myB : S.buildings) {
          if (myB.side != M.my_side) continue;
          int h = hops(myB.region, territoryReclaimTarget);
          double d = P.dist[myB.region][territoryReclaimTarget];
          if (h < rallyH || (h == rallyH && d < rallyD)) {
            optionalPushFixedRally = myB.region;
            rallyH = h;
            rallyD = d;
          }
        }
        dbg::note(turn, "PUSH_DYNAMIC_RALLY start=R" +
                            std::to_string(optionalPushFixedRally) +
                            " first_target=R" +
                            std::to_string(territoryReclaimTarget));
      }
#endif
      dbg::note(turn, "PUSH_SELECT center=R" +
                          std::to_string(centerStronghold) + " target=R" +
                          std::to_string(territoryReclaimTarget) +
                          " ready=" + std::to_string(bestReadyTurns) +
                          " force=" + std::to_string(bestForce)
#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
                          + " rally=R" +
                          std::to_string(optionalPushFixedRally)
#endif
                          );
#if DYNAMIC_OPTIONAL_PUSH_PRECISE_ASSEMBLY
      dbg::note(turn, "PUSH_DYNAMIC_PRECISE_PLAN target=R" +
                          std::to_string(dynamicOptionalPlanTarget) +
                          " capture=" +
                          std::to_string(dynamicOptionalPlanCaptureDay) +
                          " force=" +
                          std::to_string(dynamicOptionalPlanForce) +
                          " train=" +
                          std::to_string(dynamicOptionalPlanTrain));
#endif
    }
  }

  // 회수 목표와 선발대 생존 여부를 매 턴 동기화한다. 목표가 바뀌면 새
  // 작전이고, 출발했던 선발대가 모두 죽었으면 같은 목표를 상대로 다시
  // 최소+1 집결 단계부터 시작한다. 상대가 제거되어 건설 단계로 넘어간
  // 경우에도 전투 상태는 즉시 끝낸다.
  if (reclaimAssault.target != territoryReclaimTarget) {
#if RESELECT_OPTIONAL_PUSH_BEFORE_LAUNCH
    // 출격 전 선택 공세의 목표만 바뀐 경우에는 이미 작전에 모으던 병력의
    // 소속을 유지한다. 이동 명령이 끝나 유휴 상태가 되는 즉시 새 목표의
    // 집결지로 다시 배치할 수 있게 하되, 선발대/실패 상태는 새 작전으로
    // 초기화한다.
    bool optionalRetarget = previousOptionalPushTarget != -1 &&
                            territoryReclaimTarget != -1 &&
                            territoryReclaimIsExpansion;
    if (optionalRetarget) {
      std::vector<int> committed = std::move(reclaimAssault.committed);
      reclaimAssault.reset(territoryReclaimTarget, turn);
      reclaimAssault.committed = std::move(committed);
      dbg::note(turn, "PUSH_RETARGET from=R" +
                          std::to_string(previousOptionalPushTarget) +
                          " to=R" + std::to_string(territoryReclaimTarget) +
                          " committed=" +
                          std::to_string(reclaimAssault.committed.size()));
    } else {
      reclaimAssault.reset(territoryReclaimTarget, turn);
    }
#else
    reclaimAssault.reset(territoryReclaimTarget, turn);
#endif
  }

  // 선택 공세가 20턴 안에 출발하지 못하면 목표를 고른 시뮬레이션이 이미
  // 현실과 어긋난 것이다. 무한 ASSEMBLE 대신 잠금을 풀고 경제로 복귀한다.
  if (territoryReclaimIsExpansion && !reclaimAssault.launched &&
      reclaimAssault.assemblyDeadline >= 0 &&
      turn >= reclaimAssault.assemblyDeadline) {
    dbg::note(turn, "PUSH_ABORT assembly_timeout target=R" +
                        std::to_string(territoryReclaimTarget));
    optionalPushCooldownUntil = std::max(optionalPushCooldownUntil, turn + 15);
    territoryReclaimTarget = -1;
    territoryReclaimIsExpansion = false;
    reclaimAssault.reset(-1, turn);
  }

  // 필수 영토는 선택 공세처럼 포기할 수 없지만, 오래된 committed 목록과
  // 집결 상태를 영구히 끌고 갈 수도 없다. 최초 집결은 20턴, 선발대 전멸
  // 뒤 재집결은 12턴이라는 기존 deadline을 넘기면 목표 잠금은 유지한 채
  // 작전 소속만 초기화한다. 도착해 정지한 병력은 이번 턴의 최신 전력
  // 계산에서 다시 선발되고, 이동 중 병력은 도착 뒤 다시 선발된다. 실패
  // 횟수로 목표를 제외하거나 탈환 자체를 포기하지 않는다.
  if (!territoryReclaimIsExpansion && territoryReclaimTarget != -1 &&
      !reclaimAssault.launched && reclaimAssault.assemblyDeadline >= 0 &&
      turn >= reclaimAssault.assemblyDeadline) {
    dbg::note(turn, "RECLAIM_RESYNC assembly_timeout target=R" +
                        std::to_string(territoryReclaimTarget) +
                        " committed=" +
                        std::to_string(reclaimAssault.committed.size()) +
                        " failed=" +
                        std::to_string(reclaimAssault.failedWaves));
    int failedWaves = reclaimAssault.failedWaves;
    reclaimAssault.reset(territoryReclaimTarget, turn);
    // 재동기화는 병력 소속만 다시 계산하는 것이며, 앞선 공격 실패라는
    // 관측까지 잊는 것은 아니다. 실패 뒤에는 아래의 동기화 재집결 조건을
    // 계속 사용한다.
    reclaimAssault.failedWaves = failedWaves;
  }

  if (reclaimAssault.launched) {
    if (!territoryReclaimIsExpansion && territoryReclaimTarget != -1) {
      const Building *engagedBuilding = bld[territoryReclaimTarget];
      bool enemyAtTarget =
          (engagedBuilding != nullptr &&
           engagedBuilding->side != M.my_side) ||
          oppCnt[territoryReclaimTarget] > 0;
      if (enemyAtTarget) {
        for (const auto &w : S.warriors) {
          if (w.id.side != M.my_side || w.hp <= 0 ||
              w.region != territoryReclaimTarget)
            continue;
          bool inWave = reclaimAssault.isCommitted(w.id.num) ||
                        std::find(reclaimAssault.spearhead.begin(),
                                  reclaimAssault.spearhead.end(), w.id.num) !=
                            reclaimAssault.spearhead.end();
          if (inWave) {
            reclaimAssault.engagedReclaimTarget = true;
            break;
          }
        }
      }
    }
    reclaimAssault.spearhead.erase(
        std::remove_if(reclaimAssault.spearhead.begin(),
                       reclaimAssault.spearhead.end(), [&](int num) {
          for (const auto &w : S.warriors)
            if (w.id.side == M.my_side && w.id.num == num && w.hp > 0 &&
                w.purpose == WPurpose::ATTACK)
              return false;
          return true;
        }),
        reclaimAssault.spearhead.end());
    if (reclaimAssault.spearhead.empty()) {
      // 선택 공세는 기존의 제한된 탐색 규칙을 유지한다. 후속 병력이 조금
      // 남았다는 이유로 끝까지 물고 늘어지면, 이미 이기고 있던 판에서도
      // 업그레이드와 다음 목표 전환을 막아 손해가 커진다.
      if (territoryReclaimIsExpansion) {
        reclaimAssault.launched = false;
        ++reclaimAssault.failedWaves;
        if (reclaimAssault.failedGarrisonTrade) {
          dbg::note(turn, "PUSH_PROFITABLE_FAILED_TRADE_COMPLETE target=R" +
                              std::to_string(territoryReclaimTarget) +
                              " net=" +
                              std::to_string(
                                  reclaimAssault.baseRaceImmediateNet));
          optionalPushCooldownUntil = std::max(
              optionalPushCooldownUntil, turn + 15);
          territoryReclaimTarget = -1;
          territoryReclaimIsExpansion = false;
          reclaimAssault.reset(-1, turn);
        } else if (reclaimAssault.failedWaves >= 2) {
          dbg::note(turn, "PUSH_ABORT two_failed_waves target=R" +
                              std::to_string(territoryReclaimTarget));
          optionalPushCooldownUntil = std::max(optionalPushCooldownUntil, turn + 15);
          territoryReclaimTarget = -1;
          territoryReclaimIsExpansion = false;
          reclaimAssault.reset(-1, turn);
        } else {
          reclaimAssault.assemblyDeadline = turn + 12;
          dbg::note(turn, "PUSH_WAVE spearhead_lost -> ASSEMBLE retry=" +
                              std::to_string(reclaimAssault.failedWaves));
        }
      } else {
        // 최초 선발대 ID가 모두 죽었더라도 같은 작전에 커밋된 후속 증원이
        // 목표에서 싸우거나 이동 중이면 공세 자체는 살아 있다. 이들을 새
        // 선발대로 승계해 REINFORCE를 계속한다. 예전에는 최초 ID만 보고
        // 실패로 판정하여 살아 있는 증원을 남겨둔 채 파이프라인을 끊었다.
        for (const auto &w : S.warriors) {
          if (w.id.side != M.my_side || w.hp <= 0 ||
              w.purpose != WPurpose::ATTACK ||
              !reclaimAssault.isCommitted(w.id.num))
            continue;
          reclaimAssault.spearhead.push_back(w.id.num);
        }
        if (!reclaimAssault.spearhead.empty()) {
          dbg::note(turn, "RECLAIM_WAVE spearhead_handoff active=" +
                              std::to_string(reclaimAssault.spearhead.size()) +
                              " target=R" +
                              std::to_string(territoryReclaimTarget));
        } else {
          reclaimAssault.launched = false;
          ++reclaimAssault.failedWaves;
          // 필수 작전 병력이 정말 한 명도 남지 않은 경우에만 재집결한다.
          int failedWaves = reclaimAssault.failedWaves;
          bool engagedTarget = reclaimAssault.engagedReclaimTarget;
          int failedTarget = territoryReclaimTarget;
          reclaimAssault.reset(territoryReclaimTarget, turn);
          reclaimAssault.failedWaves = failedWaves;
          reclaimAssault.assemblyDeadline = turn + 12;
          dbg::note(turn, "RECLAIM_MANDATORY_WAVE_LOST target=R" +
                              std::to_string(failedTarget) +
                              " engaged=" +
                              std::to_string(engagedTarget ? 1 : 0) +
                              " retry=" +
                              std::to_string(reclaimAssault.failedWaves));
        }
      }
    }
  }

  bool reclaimEnemyPresent = false;
  bool baseRaceHoldThreat = false;
  if (territoryReclaimTarget != -1) {
    const Building *rb = bld[territoryReclaimTarget];
    reclaimEnemyPresent =
        (rb != nullptr && rb->side != M.my_side) ||
        (rb == nullptr && oppCnt[territoryReclaimTarget] > 0);
  }
#if BASE_REINFORCEMENT_RACE_LOOKUP
  if (territoryReclaimTarget != -1 && reclaimAssault.baseRaceStrike &&
      bld[territoryReclaimTarget] == nullptr &&
      myCnt[territoryReclaimTarget] > 0) {
    int movingEnemyCount = 0;
    int earliestMovingEnemy = std::numeric_limits<int>::max();
    int latestMovingEnemy = -1;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side ||
          w.region == territoryReclaimTarget ||
          w.state != WState::MOVING || w.prev_region < 0 ||
          w.prev_region == w.region ||
          P.nxt[w.prev_region][territoryReclaimTarget] != w.region)
        continue;
      int h = hops(w.region, territoryReclaimTarget);
      if (h >= 9999) continue;
      ++movingEnemyCount;
      int arrival = std::max(0, h - 1);
      earliestMovingEnemy = std::min(earliestMovingEnemy, arrival);
      latestMovingEnemy = std::max(latestMovingEnemy, arrival);
    }
    bool enemyAlreadyEngaged = oppCnt[territoryReclaimTarget] > 0;
    baseRaceHoldThreat = enemyAlreadyEngaged || movingEnemyCount > 0;

    // 상대가 아직 목표에 도착하지 않았으면 이동 단계는 동시 처리되므로,
    // 이번 턴 목표에서 빠져나가는 병력과 목표로 들어오는 적은 서로
    // 엇갈려 지나가 전투하지 않는다. 가장 가까운 아군 건물을 최종
    // 목적지로 주면 이동비도 0이므로, 표의 즉시 순가치가 양수인 약탈은
    // 건설을 욕심내지 않고 전원 철수한다.
    int safeRetreat = -1;
    int bestRetreatHops = std::numeric_limits<int>::max();
    double bestRetreatDist = std::numeric_limits<double>::infinity();
    for (const auto &base : S.buildings) {
      if (base.side != M.my_side ||
          base.region == territoryReclaimTarget)
        continue;
      int h = hops(territoryReclaimTarget, base.region);
      double d = P.dist[territoryReclaimTarget][base.region];
      if (h >= 9999 || h > bestRetreatHops ||
          (h == bestRetreatHops && d >= bestRetreatDist))
        continue;
      safeRetreat = base.region;
      bestRetreatHops = h;
      bestRetreatDist = d;
    }

    std::vector<const Warrior *> troopsAtBreach;
    bool allCanLeave = true;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side ||
          w.region != territoryReclaimTarget)
        continue;
      troopsAtBreach.push_back(&w);
      if (w.state != WState::STATIONARY) allCanLeave = false;
    }
    bool canEvacuateBeforeContact =
        !enemyAlreadyEngaged && movingEnemyCount > 0 &&
        safeRetreat != -1 && allCanLeave &&
        reclaimAssault.baseRaceImmediateNet > 0;

    if (canEvacuateBeforeContact) {
      Actions evacuation;
      for (const Warrior *w : troopsAtBreach) {
        evacuation.moves.push_back(
            {w->id, safeRetreat, WPurpose::MOVE});
        dbg::move(turn, w->id, w->region, safeRetreat,
                  WPurpose::MOVE,
                  "BASE 약탈 완료, 상대 증원 전 철수 ->R" +
                      std::to_string(safeRetreat));
      }
      dbg::note(turn, "RECLAIM_BASE_RAID action=EVACUATE target=R" +
                          std::to_string(territoryReclaimTarget) +
                          " troops=" +
                          std::to_string(troopsAtBreach.size()) +
                          " safe=R" + std::to_string(safeRetreat) +
                          " enemy_moving=" +
                          std::to_string(movingEnemyCount) +
                          " enemy_arrival=" +
                          std::to_string(earliestMovingEnemy) +
                          " enemy_last=" +
                          std::to_string(latestMovingEnemy) +
                          " net=" +
                          std::to_string(
                              reclaimAssault.baseRaceImmediateNet));
      // 바로 다음 턴 같은 필수 거점을 다시 공격하면 철수의 의미가 없다.
      // 현재 관측된 마지막 증원이 목표에 도착해 한곳에 모일 때까지 잠금을
      // 유지하되 재교전/건설은 쉬고, 그 뒤 최신 상주군을 상대로 새 파동을
      // 계산한다.
      reclaimAssault.launched = false;
      reclaimAssault.spearhead.clear();
      reclaimAssault.committed.clear();
      reclaimAssault.baseRaceStrike = false;
      reclaimAssault.postBreachHold = false;
      reclaimAssault.raidReengageAfterTurn =
          turn + std::max(0, latestMovingEnemy) + 1;
      return evacuation;
    }

    if (baseRaceHoldThreat) {
      reclaimAssault.postBreachHold = true;
      dbg::note(turn, "RECLAIM_BASE_RAID action=HOLD target=R" +
                          std::to_string(territoryReclaimTarget) +
                          " engaged=" +
                          std::to_string(enemyAlreadyEngaged ? 1 : 0) +
                          " enemy_moving=" +
                          std::to_string(movingEnemyCount) +
                          " enemy_arrival=" +
                          (earliestMovingEnemy ==
                                   std::numeric_limits<int>::max()
                               ? std::string("INF")
                               : std::to_string(earliestMovingEnemy)));
    } else if (reclaimAssault.postBreachHold) {
      // 실제로 맞물린 야전 전투와 관측된 이동 증원이 모두 끝났다. 이제야
      // 기존 건설 단계가 작동하도록 전투 상태를 해제한다.
      dbg::note(turn, "RECLAIM_BASE_RAID action=SECURED target=R" +
                          std::to_string(territoryReclaimTarget));
      reclaimAssault.reset(territoryReclaimTarget, turn);
    }
  }
#endif
  bool raidReengageCooling =
      territoryReclaimTarget != -1 &&
      reclaimAssault.raidReengageAfterTurn >= turn;
  if (territoryReclaimTarget != -1 &&
      reclaimAssault.raidReengageAfterTurn >= 0 &&
      turn > reclaimAssault.raidReengageAfterTurn) {
    int cooledTarget = territoryReclaimTarget;
    dbg::note(turn, "RECLAIM_BASE_RAID action=REENGAGE target=R" +
                        std::to_string(cooledTarget));
    reclaimAssault.reset(cooledTarget, turn);
    raidReengageCooling = false;
  }
  if (!reclaimEnemyPresent && reclaimAssault.launched &&
      !reclaimAssault.postBreachHold)
    reclaimAssault.reset(territoryReclaimTarget, turn);

  // 전투가 끝나 목표가 빈 땅이 되었고 아군이 살아남아 서 있다면 이제
  // 회수의 마지막 단계는 건설이다. 이때 평시 병력 유지 예산이 건설비를
  // 먼저 가져가지 않도록 앞에서 잡아둔 훈련 예약을 풀어 건설을 우선한다.
  bool reclaimReadyToBuild =
      territoryReclaimTarget != -1 &&
      bld[territoryReclaimTarget] == nullptr &&
      oppCnt[territoryReclaimTarget] == 0 &&
      myCnt[territoryReclaimTarget] > 0 &&
      !reclaimAssault.postBreachHold &&
      !raidReengageCooling;
  if (reclaimReadyToBuild && trainBudgetReserved > 0) {
    gold += trainBudgetReserved;
    train_reserved -= trainBudgetReserved;
    trainBudgetReserved = 0;
  }

  std::vector<char> strategicWanted(N, 0);
  std::vector<int> strategicPriority(N, 0), strategicSwing(N, 0);
  std::vector<int> strategicWantedList;
  if (territoryReclaimTarget != -1) {
    if (!raidReengageCooling) {
      strategicWanted[territoryReclaimTarget] = 1;
      strategicPriority[territoryReclaimTarget] = 100;
      strategicWantedList.push_back(territoryReclaimTarget);
    }
  } else {
    for (int t : M.strongholds) {
      if (!mandatoryTerritory[t]) continue;
      if (bld[t] != nullptr) continue;
      strategicWanted[t] = 1;
      // 내 쪽 깊숙한 거점부터 안정적으로 확보하고, 같은 hop이면 실제
      // 거리로 완공이 빠른 곳을 고르는 기존 파견 로직에 맡긴다.
      strategicPriority[t] = 1;
      strategicWantedList.push_back(t);
    }
  }

  std::string strategicMsg =
      std::string("TERRITORY mode=") +
      (territoryReclaimTarget == -1
           ? "EXPAND"
           : (territoryReclaimIsExpansion ? "PUSH" : "RECLAIM")) +
      " target=" + std::to_string(territoryReclaimTarget) + " wanted=";
  if (strategicWantedList.empty()) {
    strategicMsg += "NONE";
  } else {
    for (size_t i = 0; i < strategicWantedList.size(); ++i) {
      if (i) strategicMsg += ',';
      int t = strategicWantedList[i];
      strategicMsg += "R" + std::to_string(t) + "(p" +
                      std::to_string(strategicPriority[t]) + ")";
    }
  }
  dbg::note(turn, strategicMsg);

  // 이미 내 병력이 점거한 빈 거점 건설: best_help가 위협받는 기존 거점
  // 증원을 이미 마친 뒤, 남은 골드로만 짓는다. total_offensive가 성립하면
  // 이 아래로는 return으로 빠져나가 버리므로, 공격 여부와 무관하게 지금
  // 당장 지을 수 있는 거점은 먼저 지어 둔다. 후보를 지역 인덱스 순서
  // 그대로 처리하면, gold가 모두를 감당 못 할 때 우연히 인덱스가 낮은 먼
  // 거점이 먼저 골드를 가져가고 정작 더 가까운 거점은 순서가 늦게 왔다는
  // 이유만으로 못 짓는 문제가 있었다. 그래서 내 사령부 기준 거리
  // 오름차순으로 정렬해 가까운 거점부터 짓는다.
  std::vector<int> buildNowCandidates;
  for (int r = 0; r < N; ++r) {
    if (!isStrong[r] || bld[r] != nullptr) continue;
    if (myCnt[r] == 0 || oppCnt[r] > 0) continue;
    // 회수 목표는 적을 몰아낸 직후 반드시 아군 기지까지 완공해야 목표
    // 잠금이 풀린다. 따라서 일반 확장에만 적용하던 수익성 게이트를 회수
    // 목표에는 적용하지 않는다.
    if (r != territoryReclaimTarget && !worth_building_base(turn, 0)) continue;
    if (!strategicWanted[r]) continue;
    buildNowCandidates.push_back(r);
  }
  std::sort(buildNowCandidates.begin(), buildNowCandidates.end(), [&](int x, int y) {
    if (strategicPriority[x] != strategicPriority[y])
      return strategicPriority[x] > strategicPriority[y];
    if (strategicSwing[x] != strategicSwing[y])
      return strategicSwing[x] > strategicSwing[y];
    int hx = hops(M.my_hq, x), hy = hops(M.my_hq, y);
    if (hx != hy) return hx < hy;
    return P.dist[M.my_hq][x] < P.dist[M.my_hq][y];
  });
  // 이번 턴에 확정된 신규 기지 건설이 만들어낼 수입. 이미 도착해 있는
  // 빌더가 바로 짓는 것이라 이동/경합 리스크가 없고, 그 수입은 이 턴 말
  // 정산(read_turn_result)부터 확실히 들어온다. 아래 확장 파병 판단
  // (projIncome)이 이 확정분을 반영하도록 따로 모아둔다 — 안 그러면 결정
  // 코드가 current_net_income(턴 시작 스냅샷)만 봐서 딱 이 한 채(들)만큼
  // 수입을 과소평가하고, 지금 파병해도 되는 빌더를 "돈 부족"으로 미룬다.
  int confirmedBuildIncome = 0;
  for (int r : buildNowCandidates) {
    // 가까운 순으로 정렬해뒀으니, 지금 순서(=가장 가까운 후보)에서 이미
    // 돈이 모자라면 그 뒤(더 먼 후보)는 어차피 같은 건설비(BASE_LEVELS[1].cost)라
    // 볼 필요 없이 즉시 멈춘다.
    if (gold < BASE_LEVELS[1].cost) break;
    a.upgrades.push_back(r);
    gold -= BASE_LEVELS[1].cost;
    dbg::note(turn, "BUILD R" + std::to_string(r) +
                        " 신규 기지 건설(제자리, 상주=" +
                        std::to_string(myCnt[r]) + ")");
    // 레벨1 기지 일자리(work_cap)만큼, 지금 그 자리에 있는 빌더가 일해서
    // 버는 수입. myCnt[r] >= 1(buildNowCandidates 조건)이라 실질 +WORK_INCOME.
    confirmedBuildIncome += WORK_INCOME * std::min(myCnt[r], BASE_LEVELS[1].work_cap);
  }

  // 건설 목적(BUILD)으로 파견해둔 유닛들이 남긴 부채: 이동 중이든, 이미
  // 도착해서 건설을 기다리는 중이든 실제로 건물이 지어지기 전까지는 그
  // 건설비만큼 골드를 계속 남겨둬야 한다. 이 계산을 총공세 판단보다 먼저
  // 해둬야, 총공세로 병력을 보내고 훈련하는 데 이 돈까지 써버리는 일이
  // 없다(공격 예산 계산에 건설 예산이 반영되지 않던 버그).
  std::vector<char> builtThisTurn(N, 0);
  for (int r : a.upgrades) builtThisTurn[r] = 1; // 이번 턴에 이미 지은 곳은 중복 반영 방지
  std::vector<char> buildIncoming(N, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.purpose == WPurpose::BUILD) {
      buildIncoming[w.target] = 1;
    }
  }
  int reserved_build = 0;
  for (int t : M.strongholds) {
    // 상대 병력이 먼저 그 자리를 점거했으면 도착해도 건설이 아니라 전투가
    // 벌어지므로, 건물이 아직 없어도 더 이상 건설비를 묶어둘 이유가 없다.
    if (bld[t] == nullptr && buildIncoming[t] && !builtThisTurn[t] &&
        oppCnt[t] == 0) {
      reserved_build += BASE_LEVELS[1].cost;
    }
  }
  // ATTACK 용도로 들어간 생존자가 회수 목표를 점거한 경우에는 BUILD
  // 표식이 없더라도 기지를 지을 300골드를 똑같이 예약한다.
  if (reclaimReadyToBuild && !builtThisTurn[territoryReclaimTarget] &&
      !buildIncoming[territoryReclaimTarget])
    reserved_build += BASE_LEVELS[1].cost;
  int available_gold = gold - reserved_build; // 다음 유닛 판단 기준이 될 실시간 가용 예산 (기존 부채 차감)

  // 방어(빈 거점 확보)를 공격 여부 판단보다 먼저 결정한다. 예전에는 이
  // 로직 전체가 "공격 안 하기로 한 턴"에만 실행돼서, 총공세가 켜지면 그
  // 턴엔 방어/확장 재배치가 통째로 안 돌아갔다(공격이냐 방어냐 양자택일).
  // 이제는 순서를 바꿔 먼저 방어/확장에 필요한 인원을 실제로 배치하고
  // idle에서 빼둔 다음, 그러고도 남는 순수 잉여만 공격 후보로 넘긴다 —
  // 그러면 방어와 공격이 같은 턴에 같이 일어날 수 있다.

  // 인력 부족(방어) 채우기는 처리 순서가 중요하다: 워리어별 개방 탐색
  // 자체는 유지하되, 처리 순서를 "자기 위치에서 가장 가까운 부족 거점까지의
  // 거리(helpDist)" 오름차순으로 정렬해서, 진짜로 가까운 워리어가 먼저
  // 그 자리를 채우게 한다. 이렇게 하면 우연히 먼 워리어가 먼저 처리돼
  // 가까운 워리어보다 먼저 그 거점을 채가는 일이 줄어든다. best_help가
  // 이미 일부 idle을 소모했으므로 homeCnt/incoming의 최신 상태로 다시
  // 계산한다.
  std::vector<int> helpDist(N, std::numeric_limits<int>::max());
  for (int r = 0; r < N; ++r) {
    for (int t = 0; t < N; ++t) {
      if (t == r) continue;
      if (bld[t] == nullptr || bld[t]->side != M.my_side) continue;
      if (homeCnt[t] + incoming[t] >= need[t]) continue;
      int h = hops(r, t);
      if (h < helpDist[r]) helpDist[r] = h;
    }
  }
  std::stable_sort(idle.begin(), idle.end(), [&](const Warrior *x, const Warrior *y) {
    return helpDist[x->region] < helpDist[y->region];
  });

  // === 빈 거점 확보: "이 거점은 이 건물 담당"이라고 미리 고정하지 않는다.
  // 고정하면 그 담당 건물에 마침 남는 병력이 없을 때, 더 멀어도 병력이
  // 남는 다른 건물이 있어도 그 거점을 못 채우는 문제가 있었다. 대신
  // 반복마다 "아직 부족한 거점 × 지금 실제로 남는 병력이 있는 아군 건물"
  // 모든 조합 중 hop이 가장 작은 조합을 그때그때 찾아 하나씩 파병한다
  // (그리디 최근접 매칭). 남는 병력이 아무 데도 없으면 본부에서 새로
  // 훈련해서라도 채울 몫으로 잡아둔다(strongholdTrainNeed, 아래 최종
  // train_n 계산에 반영). 훈련조차 예산/훈련 한도가 안 되면 그 시점에
  // 전체를 멈춘다.
  int strongholdTrainNeed = 0;
  int strongholdTrainCapLeft = my_hq_train_cap(S, M);
  // 기본판은 필수 탈환이 실제 교전 상태이면 빈 거점 확보 패스 전체를
  // 닫는다. 실험판에서는 둘을 양자택일로 보지 않고, 탈환 작전에 이미
  // 커밋된 병력을 보존한 채 남는 인력으로 신규 거점 확보를 계속한다.
#if PARALLEL_EXPANSION_DURING_MANDATORY_RECLAIM
  bool strongholdDone = false;
#else
  bool strongholdDone = mandatoryReclaimActiveNow;
#endif
  // 이 루프에서 신병을 훈련하기로 결정할 때마다(strongholdTrainNeed 증가)
  // 그 병사의 유지비만큼 다음 턴부터 실제 순수입이 줄어든다. current_net_income을
  // 그대로 쓰면 이미 결정한 훈련이 없는 셈 치고 미래 수입을 낙관적으로
  // 추정하게 되므로, 루프 진행에 따라 갱신되는 예상치를 따로 둔다.
  // current_net_income은 턴 시작 스냅샷이라 이번 턴 확정 건설분을 아직
  // 모른다. 그 수입(confirmedBuildIncome)을 더해, 다른 변화가 없다면 다음
  // 턴 current_net_income이 실제로 갖게 될 값에서 파병 판단을 시작한다.
  int projIncome = current_net_income + confirmedBuildIncome;
  // 상대도 같은 빈 거점을 노릴 수 있다. 단, 단순히 정지해 있는 상대 여유
  // 병력의 가능한 선택까지 추측하지 않고, 이전 지역 -> 현재 지역의 이동
  // 방향으로 보아 이미 해당 빈 거점 방향으로 이동 중인 병력만 선점 예측에
  // 사용한다. 그 병력이 내 빌더보다 먼저 도착해 기지까지 지을 수 있으면
  // 아래에서 선택하는 단 한 곳에만 BUILD 병력을 보내지 않는다.
  //
  // 기지 건설은 별도 공사 기간 없이 명령 턴에 즉시 완료되지만, 이동은 그
  // 턴의 BUILD 단계 뒤에 처리된다. 따라서 h hop 떨어진 병력은 h턴 뒤에야
  // 건설 명령을 낼 수 있다. 상대가 같은 턴에 도착하는 경우는 "이미 건설
  // 완료"가 아니므로 제외하지 않고, 상대 완공 턴이 내 도착 턴보다 엄밀히
  // 빠를 때만 해당 후보를 버린다. 이동비와 건설비 300, 현재 상대 골드와
  // 관측 순수입도 날짜별로 반영한다.
  //
  // 상대 현재 병력 위치+이동 방향으로 "어느 빈 거점에 언제 도착하는지" 예측.
  // MOVING 상태로 방향이 잡히는 상대 유닛에 대해, prev_region→region이 빈
  // 거점 t로 가는 최단경로의 다음 칸(P.nxt)과 일치하면 t로 향한다고 보고,
  // 지금 위치에서 t까지 남은 hop을 예상 도착 턴으로 둔다(여럿이 같은 t를
  // 노리면 가장 이른 도착). 상대 위치는 이번 결정 동안 바뀌지 않으니 루프
  // 시작 전에 한 번만 계산한다.
  const int PRECLAIM_NEVER = std::numeric_limits<int>::max() / 4;
  std::vector<int> oppArrivalEta(N, std::numeric_limits<int>::max());
  std::vector<int> oppPreclaimTurn(N, PRECLAIM_NEVER);
  std::vector<int> oppPreclaimOrigin(N, -1);
  // 지금부터 매 턴 시작 골드를 따라가며, 이 병력이 출발비를 낼 수 있는
  // 최초 턴에 출발하고 목적지 도착 뒤 건설비를 낼 수 있는 최초 턴을 찾는다.
  auto earliestOpponentBuildTurn = [&](int travel, int moveCost) {
    if (travel >= 9999) return PRECLAIM_NEVER;
    long long projectedGold = std::max(0, S.opp_gold);
    int dispatchDay = -1;
    int horizon = std::max(0, MAX_TURN - turn);
    for (int day = 0; day <= horizon; ++day) {
      if (dispatchDay == -1 && projectedGold >= moveCost) {
        projectedGold -= moveCost;
        dispatchDay = day;
      }
      if (dispatchDay != -1 && day >= dispatchDay + travel &&
          projectedGold >= BASE_LEVELS[1].cost)
        return day;
      projectedGold = std::max<long long>(
          0, projectedGold + oppNetIncome);
    }
    return PRECLAIM_NEVER;
  };
  // oppIncomingHps[t]: 빈 거점 t를 향해 "이동 중인"(MOVING) 상대 병력의 체력
  // 목록. 집결(STATIONARY)해 있는 병력은 여기 안 들어온다 — 실제로 그 거점을
  // 향해 출발한 병력만 짓고 난 뒤의 즉각적 위협으로 본다.
  std::vector<std::vector<int>> oppIncomingHps(N);
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side) continue;
    if (w.state != WState::MOVING) continue;
    if (w.prev_region < 0 || w.prev_region == w.region) continue;
    if (myCnt[w.region] > 0) continue; // 현재 교전 중이면 예정대로 전진 불가
    for (int t : M.strongholds) {
      if (P.nxt[w.prev_region][t] != w.region) continue;
      int remaining = hops(w.region, t);
      oppArrivalEta[t] = std::min(oppArrivalEta[t], remaining);
      oppIncomingHps[t].push_back(w.hp);
      int buildTurn = earliestOpponentBuildTurn(remaining, 0);
      if (buildTurn < oppPreclaimTurn[t]) {
        oppPreclaimTurn[t] = buildTurn;
        oppPreclaimOrigin[t] = w.region;
      }
    }
  }

  // 상대 병력/골드는 모든 후보 거점에 동시에 복제되어 쓰일 수 없다. 따라서
  // "상대가 먼저 완공할 수 있다"는 이유로 제외하는 거점은 이번 판단 전체에서
  // 딱 하나만 고른다. 상대 예상 완공 턴이 가장 빠른 곳을 우선하고, 동률이면
  // 기존 우리 확장 우선순위(전략 우선도 -> 내 도착 턴 -> 전선 거리 -> 실제
  // 거리)를 따라 우리가 먼저 노릴 곳을 상대도 막는다고 본다.
  //
  // 이미 아군 병력이 확보하러 가고 있는 곳(home+incoming >= 1)과 상대 병력이
  // 현재 서 있는 곳은 이 "빈 거점 선점" 한 자리에서 제외한다. 전자는 이미
  // 파병이 확정됐고, 후자는 평화적 빈 거점 확보가 아니라 기존 회수/공세 전투가
  // 처리해야 하기 때문이다.
  int preclaimHqIdle = 0;
  for (const Warrior *w : idle)
    if (w->region == M.my_hq) ++preclaimHqIdle;
  bool preclaimCanDispatchNow =
      preclaimHqIdle > 0 && homeCnt[M.my_hq] > need[M.my_hq];
  int excludedPreclaimTarget = -1;
  int excludedOppBuildTurn = PRECLAIM_NEVER;
  int excludedPriority = std::numeric_limits<int>::min();
  int excludedSwing = std::numeric_limits<int>::min();
  int excludedMyArrival = std::numeric_limits<int>::max();
  double excludedFrontD = std::numeric_limits<double>::infinity();
  double excludedDist = std::numeric_limits<double>::infinity();
  for (int t : M.strongholds) {
    if (bld[t] != nullptr || !strategicWanted[t]) continue;
    if (oppCnt[t] > 0) continue;
    if (homeCnt[t] + incoming[t] >= 1) continue;
    if (t != territoryReclaimTarget && !worth_building_base(turn, 0)) continue;

    int myArrival = hops(M.my_hq, t) + (preclaimCanDispatchNow ? 0 : 1);
    if (oppPreclaimTurn[t] >= myArrival) continue;
    double frontD = frontlineDist(t);
    double dist = P.dist[M.my_hq][t];

    bool better = false;
    if (oppPreclaimTurn[t] != excludedOppBuildTurn)
      better = oppPreclaimTurn[t] < excludedOppBuildTurn;
    else if (strategicPriority[t] != excludedPriority)
      better = strategicPriority[t] > excludedPriority;
    else if (strategicSwing[t] != excludedSwing)
      better = strategicSwing[t] > excludedSwing;
    else if (myArrival != excludedMyArrival)
      better = myArrival < excludedMyArrival;
    else if (frontD != excludedFrontD)
      better = frontD < excludedFrontD;
    else if (dist != excludedDist)
      better = dist < excludedDist;
    else
      better = excludedPreclaimTarget == -1 || t < excludedPreclaimTarget;

    if (!better) continue;
    excludedPreclaimTarget = t;
    excludedOppBuildTurn = oppPreclaimTurn[t];
    excludedPriority = strategicPriority[t];
    excludedSwing = strategicSwing[t];
    excludedMyArrival = myArrival;
    excludedFrontD = frontD;
    excludedDist = dist;
  }
  if (excludedPreclaimTarget != -1) {
    dbg::note(turn, "EXPAND_PRECLAIM_SINGLE R" +
                        std::to_string(excludedPreclaimTarget) +
                        " my_arrival=" + std::to_string(excludedMyArrival) +
                        " opp_build=" + std::to_string(excludedOppBuildTurn) +
                        " from=R" +
                        std::to_string(oppPreclaimOrigin[excludedPreclaimTarget]));
  }

#if ANTICIPATE_MANDATORY_PRECLAIM
  // 누적 이동 경로와 일치하는 빈 거점 후보가 하나로 확정된 상대 병력만
  // 대상으로 한다. 단순히 '바로 다음 거점'일 필요는 없으며 중간 거점을
  // 통과해도 지금까지의 실제 경로 교집합이 하나라면 먼 목표도 확정된다.
  int detectedAnticipatedMandatory = -1;
  int detectedAnticipatedMyArrival = std::numeric_limits<int>::max();
  int detectedAnticipatedOppBuild = PRECLAIM_NEVER;
  int detectedAnticipatedEnemyOrigin = -1;
  int detectedAnticipatedPriority = std::numeric_limits<int>::min();
  for (const auto &[enemyNum, target] : confirmedEnemyPathTargets) {
    if (!mandatoryTerritory[target] || bld[target] != nullptr ||
        myCnt[target] > 0 || oppCnt[target] > 0 ||
        homeCnt[target] + incoming[target] >= 1)
      continue;
    const Warrior *enemyMover = nullptr;
    for (const auto &w : S.warriors)
      if (w.id.side != M.my_side && w.id.num == enemyNum &&
          w.state == WState::MOVING) {
        enemyMover = &w;
        break;
      }
    if (enemyMover == nullptr) continue;
    int enemyArrival = hops(enemyMover->region, target);
    int enemyBuild = earliestOpponentBuildTurn(enemyArrival, 0);
    if (enemyArrival >= 9999 || enemyBuild >= PRECLAIM_NEVER)
      continue;
    // 도착 뒤 머물며 돈을 더 모을 수도 있다는 추측은 제외한다. 도착한
    // 바로 그 턴에 건설 가능한 경우만 확정 선점으로 취급한다.
    if (enemyBuild > enemyArrival) continue;

    int actualMyArrival = hops(M.my_hq, target) +
                          (preclaimCanDispatchNow ? 0 : 1);
    std::vector<int> usableAtSource(N, 0);
    for (const Warrior *own : idle) {
      int source = own->region;
      int available = std::max(0, homeCnt[source] - need[source]);
      if (usableAtSource[source] >= available) continue;
      ++usableAtSource[source];
      actualMyArrival = std::min(actualMyArrival,
                                 hops(source, target));
    }
    if (enemyBuild >= actualMyArrival) continue;

    bool better = enemyBuild < detectedAnticipatedOppBuild ||
                  (enemyBuild == detectedAnticipatedOppBuild &&
                   strategicPriority[target] >
                       detectedAnticipatedPriority) ||
                  (enemyBuild == detectedAnticipatedOppBuild &&
                   strategicPriority[target] ==
                       detectedAnticipatedPriority &&
                   target < detectedAnticipatedMandatory);
    if (!better) continue;
    detectedAnticipatedMandatory = target;
    detectedAnticipatedMyArrival = actualMyArrival;
    detectedAnticipatedOppBuild = enemyBuild;
    detectedAnticipatedEnemyOrigin = enemyMover->region;
    detectedAnticipatedPriority = strategicPriority[target];
  }

  // 매 턴 이동 방향을 다시 봤는데 더 이상 같은 거점의 선점이 확정되지
  // 않으면 사전 잠금을 푼다. 실제 적 점유/건설로 이미 일반 필수 탈환으로
  // 승격된 상태는 anticipated 표식이 위에서 사라졌으므로 여기 오지 않는다.
  if (anticipatedMandatoryTarget != -1 &&
      anticipatedMandatoryTarget != detectedAnticipatedMandatory) {
    int cancelled = anticipatedMandatoryTarget;
    dbg::note(turn, "RECLAIM_PRECLAIM_CANCEL target=R" +
                        std::to_string(cancelled) +
                        " predicted_build=T" +
                        std::to_string(anticipatedMandatoryBuildTurn));
    if (territoryReclaimTarget == cancelled &&
        !reclaimAssault.launched) {
      territoryReclaimTarget = -1;
      territoryReclaimIsExpansion = false;
      reclaimAssault.reset(-1, turn);
    }
    anticipatedMandatoryTarget = -1;
    anticipatedMandatoryBuildTurn = -1;
  }

  if (detectedAnticipatedMandatory != -1 &&
      (territoryReclaimTarget == -1 ||
       territoryReclaimIsExpansion ||
       territoryReclaimTarget == detectedAnticipatedMandatory)) {
    int target = detectedAnticipatedMandatory;
    bool firstDetection = anticipatedMandatoryTarget != target;
    if (territoryReclaimTarget != target) {
      // 아직 출격하지 않은 선택 공세 병력은 버리지 않고 새 필수 거점의
      // 집결 전력으로 넘긴다. 선발대가 이미 출발한 작전은 위 조건에서
      // 들어오더라도 실제 이동을 되돌릴 수 없으므로 committed만 보존한다.
      std::vector<int> committed = std::move(reclaimAssault.committed);
      territoryReclaimTarget = target;
      territoryReclaimIsExpansion = false;
      reclaimAssault.reset(target, turn);
      reclaimAssault.committed = std::move(committed);
    }
    anticipatedMandatoryTarget = target;
    anticipatedMandatoryBuildTurn = turn + detectedAnticipatedOppBuild;
    mandatoryReclaimActiveNow = true;
    strongholdDone = true;
    if (firstDetection) {
      dbg::note(turn, "RECLAIM_PRECLAIM_TRIGGER target=R" +
                          std::to_string(target) + " my_arrival=" +
                          std::to_string(detectedAnticipatedMyArrival) +
                          " opp_build=" +
                          std::to_string(detectedAnticipatedOppBuild) +
                          " build_abs=T" +
                          std::to_string(anticipatedMandatoryBuildTurn) +
                          " enemy_from=R" +
                          std::to_string(detectedAnticipatedEnemyOrigin));
    }
  }
#endif

  auto opponentPreclaimsBefore = [&](int t, int myArrivalTurn) {
    if (t != excludedPreclaimTarget) return false;
    if (oppPreclaimTurn[t] >= myArrivalTurn) return false;
    dbg::note(turn, "EXPAND_SKIP_ENEMY_FIRST_SINGLE R" + std::to_string(t) +
                        " my_arrival=" + std::to_string(myArrivalTurn) +
                        " opp_build=" + std::to_string(oppPreclaimTurn[t]) +
                        " from=R" + std::to_string(oppPreclaimOrigin[t]));
    return true;
  };

  // 빈 거점 t를 짓기로 할 때, 짓고 나서 이걸 지켜낼 수 있는지 판단한다.
  // t로 이동 중인 상대 병력(oppIncomingHps)을 상대로 레벨1 기지(체력/터렛
  // 포함)를 지키는 데 필요한 최소 수비 인원을 전투 시뮬(min_regional_defenders)
  // 로 구하고, 상대가 도착(eta)하기 전까지 "현재 골드 + 예상 수입"으로 그
  // 인원을 새로 생산+투입(훈련비+이동비)할 수 있는지 본다. 지금 그 거점으로
  // 오는 상대가 없으면(목록이 비면) 방어 수요 0으로 보고 통과시킨다 — 평화
  // 확장은 막지 않는다. simGold/simIncome은 판단 시점의 가용 골드/순수입
  // 추정치(제자리 파병은 available_gold/projIncome, 훈련 시뮬은 그 차감분).
  auto defensibleAfterBuild = [&](int t, int simGold, int simIncome) -> bool {
    const std::vector<int> &atk = oppIncomingHps[t];
    if (atk.empty()) return true;
    int defNeed = min_regional_defenders(BASE_LEVELS[1].hp, BASE_LEVELS[1].turret,
                                         atk, curWarriorHp);
    // 이미 그 거점에 있거나 오는 중인 아군 + 지금 보낼 빌더 1명은 방어에
    // 합류하므로, 새로 생산해야 할 인원은 그만큼 뺀다.
    int already = homeCnt[t] + incoming[t] + 1;
    int toProduce = std::max(0, defNeed - already);
    if (toProduce == 0) return true;
    int eta = oppArrivalEta[t]; // atk가 비어있지 않으면 유한
    long long budget = (long long)simGold + (long long)simIncome * eta;
    long long cost = (long long)(BASE_LEVELS[1].cost + MOVE_COST) +
                     (long long)toProduce * (TRAIN_COST + MOVE_COST);
    return budget >= cost;
  };
  while (!strongholdDone) {
    std::vector<int> needing;
    for (int t : M.strongholds) {
      if (bld[t] != nullptr) continue;
      if (!strategicWanted[t]) continue;
      // 상대 병력이 이미 서 있는 빈 땅에 BUILD 목적 병력을 보내지 않는다.
      // 이런 필수 영토는 아래 회수 전투 시뮬레이션에서 먼저 적을 제거한다.
      if (oppCnt[t] > 0) continue;
      if (homeCnt[t] + incoming[t] >= oppCnt[t] + 1) continue;
      needing.push_back(t);
    }
    if (needing.empty()) break;

    // 1) 사령부에 남는(need 초과) 유휴 병력이 있으면, 다른 건물이 아니라
    //    항상 사령부에서 출발시킨다. "내가 실제로 지을 수 있는 턴"이
    //    가장 빠른 거점을 고른다. 단순 이동 hops(h)만 보면 이동은 빨라도
    //    건설비를 모을 때까지 계속 기다려야 하는 조합을 놓칠 수 있다.
    //    그래서 도착(h턴) 시점까지 쌓일 순수입으로도 건설비가 모자라면,
    //    그 부족분을 순수입으로 채우는 데 드는 추가 턴까지 더해 "실제
    //    완공 턴"을 추정한다. 같은 턴에 지을 수 있다면(동률), 두 사령부를
    //    잇는 축의 수직이등분선(전선)에 더 가까운, 경합이 더 급한 거점을
    //    우선한다.
    int hqRegion = M.my_hq;
    int hqIdleCnt = 0;
    auto reservedForMandatoryReclaim = [&](const Warrior *w) {
      return mandatoryReclaimActiveNow &&
             (reclaimAssault.isCommitted(w->id.num) ||
              w->purpose == WPurpose::ATTACK);
    };
    for (const Warrior *w : idle)
      if (w->region == hqRegion && !reservedForMandatoryReclaim(w))
        ++hqIdleCnt;

    // 사령부에서 지금 파병한다면 어느 빈 거점을 고를지 정하는 로직.
    // 2)번(훈련 여부 판단)에서도 "훈련한 신병이 도착하면 실제로 이 로직이
    // 파병하는가"를 need[hqRegion]과의 대소 비교로 손수 근사하지 않고,
    // 이 로직 자체를 그대로(파라미터만 바꿔) 재실행해 시뮬레이션한다.
    // delay: 실제 이동을 시작하기까지 미리 흘러가야 하는 턴 수. 1)번(지금
    // 있는 유휴 병력을 바로 보냄)은 0이지만, 2)번은 "지금 훈련해도 신병은
    // 다음 턴에야 사령부에 등장"하므로 1을 넘겨서, 그 1턴만큼 worth_building_base의
    // 잔여 턴 계산과 골드 적립 계산에 반영되게 한다.
    // simGold/simIncome: 이 판단 시점에 실제로 가용한 골드/순수입 추정치.
    // 1)번은 지금의 available_gold/projIncome을 그대로 넘기면 되지만, 2)번은
    // "신병을 훈련하기로 결정하면" 이번 턴 훈련비(TRAIN_COST)가 즉시 빠지고
    // 다음 턴부터 그 신병의 유지비(UPKEEP_PER_WARRIOR)만큼 순수입도 줄어드는
    // 걸 먼저 반영한 값을 넘겨야, "생산 이후" 상태를 정확히 시뮬레이션한다.
    // (실제 available_gold/projIncome 변수 자체는 hqBestT가 확정된 뒤에야
    // 차감되므로, 여기선 그 차감분을 미리 반영한 임시값만 파라미터로 받는다.)
    auto findHqDispatchTarget = [&](int delay, int simGold, int simIncome) {
      int bT = -1, bH = std::numeric_limits<int>::max();
      int bPriority = -1, bSwing = std::numeric_limits<int>::min();
      int bTurn = std::numeric_limits<int>::max();
      double bFrontD = std::numeric_limits<double>::infinity();
      double bD = std::numeric_limits<double>::infinity();
      for (int t : needing) {
        double frontD = frontlineDist(t);
        int h = hops(hqRegion, t);
        int steps = h + delay; // 출발 전 지연까지 포함한, 지금부터 도착까지의 실제 턴 수
        if (t != territoryReclaimTarget && !worth_building_base(turn, steps)) continue;
        if (opponentPreclaimsBefore(t, steps)) continue;
        // 지어봤자 못 지키는 거점(상대가 도착 전까지 수비 인원을 못 채우는
        // 거점)은 후보에서 제외한다.
        if (!defensibleAfterBuild(t, simGold, simIncome)) continue;

        int totalCost = BASE_LEVELS[1].cost + MOVE_COST;
        int shortfall = totalCost - simIncome * steps - simGold;
        int myTurn = steps;
        if (shortfall > 0) {
          myTurn = (simIncome > 0)
              ? steps + (shortfall + simIncome - 1) / simIncome
              : std::numeric_limits<int>::max() / 2; // 수입으로는 영영 못 모음
        }

        if (strategicPriority[t] < bPriority) continue;
        if (strategicPriority[t] == bPriority &&
            strategicSwing[t] < bSwing) continue;
        if (strategicPriority[t] > bPriority ||
            strategicSwing[t] > bSwing) {
          bTurn = std::numeric_limits<int>::max();
          bFrontD = std::numeric_limits<double>::infinity();
          bD = std::numeric_limits<double>::infinity();
        }
        if (myTurn > bTurn) continue;
        if (myTurn == bTurn && frontD > bFrontD) continue;
        if (myTurn == bTurn && frontD == bFrontD &&
            P.dist[hqRegion][t] >= bD) continue;
        bPriority = strategicPriority[t]; bSwing = strategicSwing[t];
        bTurn = myTurn; bFrontD = frontD; bD = P.dist[hqRegion][t];
        bT = t; bH = h;
      }
      // bTurn(완공 턴)까지 함께 돌려준다: 훈련 판단(branch 2)에서 신병이
      // 도착 후 얼마나 놀며 골드를 기다려야 하는지(idle 대기)를 재기 위함.
      return std::tuple<int, int, int>{bT, bH, bTurn};
    };

    int bestT = -1, bestH = std::numeric_limits<int>::max();
    if (hqIdleCnt > need[hqRegion])
      std::tie(bestT, bestH, std::ignore) =
          findHqDispatchTarget(0, available_gold, projIncome);

    if (bestT != -1) {
      int base_cost = BASE_LEVELS[1].cost + MOVE_COST - (projIncome * bestH);
      int req_gold = std::max(MOVE_COST, base_cost);
      if (available_gold < req_gold) { strongholdDone = true; break; }

      auto it = idle.end();
      for (auto cand = idle.begin(); cand != idle.end(); ++cand) {
        if ((*cand)->region != hqRegion) continue;
        if (reservedForMandatoryReclaim(*cand)) continue;
        if (it == idle.end() || (*cand)->hp > (*it)->hp) it = cand;
      }
      const Warrior *w = *it;
      a.moves.push_back({w->id, bestT, WPurpose::BUILD});
      dbg::move(turn, w->id, w->region, bestT, WPurpose::BUILD,
                "빈 거점 확보 파병 (hop=" + std::to_string(bestH) + ")");
      gold -= MOVE_COST;
      available_gold -= MOVE_COST;
      --homeCnt[hqRegion];
      ++incoming[bestT];

      // 미래 건설비를 미리 차감해, 같은 턴에 여러 거점으로 예산이
      // 중복 배정되는 것을 막는다.
      int future_cost = std::max(0, BASE_LEVELS[1].cost - (projIncome * bestH));
      available_gold -= future_cost;

      idle.erase(it);
      continue;
    }

    // 2) 남는 병력이 아무 데도 없다 -> 본부에서 새로 훈련해서 채울 몫으로
    // 잡아둔다. "훈련한 신병이 도착하면 실제로 1)번 로직이 파병하는가"를
    // need 비교로 손수 근사하지 않고, 1)번과 똑같은 findHqDispatchTarget()을
    // 다음 턴 예상 hqIdleCnt(현재 hqIdleCnt + 이번 턴에 이미 예약해둔
    // 훈련분 + 이번 신병)로, 그리고 신병이 실제로 등장하는 데 걸리는 1턴
    // 지연(delay=1)까지 반영해 그대로 실행해 판단한다. 이렇게 하면 1)번이
    // 실제로 파병할지 여부와 목표 선정 기준(myTurn/전선 우선순위)까지
    // 완전히 동일한 기준으로 검증된다 — 통과 못 하면(=사령부 자체
    // need[hqRegion]을 채우는 데 흡수돼버리거나, 1턴 지연까지 감안하면
    // 더 이상 수지가 안 맞으면) 훈련비만 쓰고 계속 사령부에 눌러앉아
    // 유지비만 축내는 유휴 인원이 되므로, 애초에 훈련을 걸지 않는다.
    int projectedHqIdleCnt = hqIdleCnt + strongholdTrainNeed + 1;
    int hqBestT = -1, hqBestH = 0, hqBestTurn = 0;
    if (projectedHqIdleCnt > need[hqRegion])
      std::tie(hqBestT, hqBestH, hqBestTurn) =
          findHqDispatchTarget(1, available_gold - TRAIN_COST,
                               projIncome - UPKEEP_PER_WARRIOR);
    if (hqBestT == -1 || strongholdTrainCapLeft <= 0) { strongholdDone = true; break; }
    if (available_gold < TRAIN_COST) { strongholdDone = true; break; }
    // idle 대기 게이트(K=0): 신병이 도착하자마자 지을 수 있는(=도착-병목)
    // 목표일 때만 훈련한다. 골드-병목이라 도착 후 놀며 골드를 기다려야 하면
    // (idleWait>0), 그 신병의 유휴 유지비가 오히려 골드 적립을 늦춰 건설을
    // 더 미룬다. 그러니 이번 턴엔 뽑지 않고 골드가 찰 때까지 기다렸다가(다음
    // 턴 재판단) 도착-병목이 되는 순간에 뽑아 idle 없이 곧장 건설로 잇는다.
    // findHqDispatchTarget(delay=1) 기준 도착 턴은 hqBestH+1이고, 완공 턴
    // (hqBestTurn)이 그보다 크면 그 차이가 곧 도착 후 놀아야 하는 턴 수다.
    int arriveTurn = hqBestH + 1;
    if (hqBestTurn - arriveTurn > 0) { strongholdDone = true; break; }

    ++strongholdTrainNeed;
    --strongholdTrainCapLeft;
    gold -= TRAIN_COST;
    available_gold -= TRAIN_COST;
    train_reserved += TRAIN_COST;
    // 방금 결정한 신병도 다음 턴부터 유지비를 무니, 이후 반복의 수입 추정치에
    // 반영한다.
    projIncome -= UPKEEP_PER_WARRIOR;
    // 실제 신병은 다음 턴에야 본부에 등장하므로 지금 당장 이동시킬 수는
    // 없다. 같은 거점을 이번 턴 반복에서 또 잡지 않도록 incoming만 미리
    // 올려둔다 — 다음 턴 신병이 idle로 등장하면 이 로직이 처음부터 다시
    // 실제 파병을 결정한다.
    ++incoming[hqBestT];
  }

  // 방어 공백(need를 못 채우는 거점)이 있어도 총공세를 끄지 않는다. 방어에
  // 필요한 인원은 위 best_help 재배치가 이미 유휴 잉여를 needy 거점으로
  // 돌려보내며 최대한 처리했고, 그러고도 남는 부족분은 아래 total_offensive
  // 분기의 훈련량(trainWant)에 missing_workers/baseline_military로 반영해
  // 공격과 방어 훈련이 같은 턴에 함께 나가게 한다. 공격 후보(attackCandidates)도
  // effNeed로 각 거점의 방어 수요를 먼저 예약한 뒤 남는 순잉여만 뽑으므로,
  // 방어에 꼭 필요한 병력이 공격에 쓸려가지 않는다. (예전에는 방어 공백이
  // 있으면 공세 자체를 껐는데, 그러면 변두리 거점 하나만 살짝 모자라도 이길
  // 때조차 공세를 멈추고 웅크리는 문제가 있었다.)

  // 총공세 판단: 수급이 밀리는데 먹을 빈 거점도 없으면, 뚫을 수 있다는
  // 확신 여부와 무관하게 남는 유휴 병력을 모아 가장 가까운 상대 거점으로
  // 돌격시켜 압박한다(구체적인 파견 수량 계산은 아래 currentTarget 분기
  // 참고).
  int currentTarget = -1;
  bool total_offensive = false;
  int neededExtra = 0; // total_offensive가 참일 때, 지금 새로 보내야 할 정확한 인원수
  std::vector<int> effNeed = need; // 총공세 파병 시 실제로 적용할 유효 need (상대 진짜 HQ가 목표면 본부 예약분을 0으로 낮춤)
  std::vector<const Warrior *> attackCandidates; // 총공세 후보(체력 내림차순), 실제 파병은 앞에서 neededExtra명만 사용
  int stagingPoint = -1; // 목표와 가장 가까운 아군 거점(집결지)
  int extraToTrain = -1; // 유휴 병력으로도 부족해 추가로 훈련해야 하는 인원(-1이면 훈련해도 답 없음/해당 없음)
  bool territoryReclaimAttack = false;
  bool territoryReinforcement = false;
  bool reclaimLaunchedThisTurn = false;
  std::vector<int> committedIds;
  std::vector<int> targetFixedIds;
  bool reclaimAssemblyInTransit = false;
  // 아직 아무도 안 지은 빈 거점이 있고 그걸 점거하는 데 병력이 더
  // 필요하다면(homeCnt+incoming이 oppCnt+1에 못 미치면), 총공세보다 빈
  // 거점 확보를 항상 우선한다. 공짜 거점을 놔두고 상대 본진을 치러 가는
  // 건 낭비다 — 위 stronghold-first 파병 패스가 이미 가용 자원(유휴
  // 병력/훈련 한도/골드) 내에서 최대한 처리했으므로, 여기서 여전히
  // 남아있다는 건 자원이 부족해서 이번 턴엔 더 손댈 수 없다는 뜻이다.
  bool hasCapturableEmptyStronghold = false;
  int remainingHqIdle = 0;
  for (const Warrior *w : idle)
    if (w->region == M.my_hq) ++remainingHqIdle;
  bool canDispatchHqBuilderNow =
      remainingHqIdle > 0 && homeCnt[M.my_hq] > need[M.my_hq];
  for (int t : M.strongholds) {
    if (bld[t] != nullptr) continue;
    if (!strategicWanted[t]) continue;
    if (t != territoryReclaimTarget && !worth_building_base(turn, 0)) continue;
    int fastestMyArrival = hops(M.my_hq, t) +
                           (canDispatchHqBuilderNow ? 0 : 1);
    // 상대가 먼저 완공할 빈 거점은 "먹을 수 있는 공짜 거점"이 아니다.
    // 이곳 때문에 일반 공세까지 막아 세우지 말고, 실제 상대 기지가 생기면
    // 다음 턴부터 기존 필수 탈환/공세 로직이 전투 목표로 처리하게 둔다.
    if (opponentPreclaimsBefore(t, fastestMyArrival)) continue;
    if (homeCnt[t] + incoming[t] < oppCnt[t] + 1) {
      hasCapturableEmptyStronghold = true;
      break;
    }
  }
  // 총공세 게이트: 골드 수급(순수입, 위에서 이미 계산해 둔 myIncomeAhead)
  // 비교로 정한다. 내 수입이 상대보다 적으면 공격으로 상대 거점을 빼앗아
  // 수입 격차를 직접 좁히고, 그렇지 않으면 병력을 소모하지 않고 돈을 모아
  // 사령부/거점을 올리는 쪽에 집중한다. 다만 내가 수급에서 앞서 있어도,
  // 내 사령부가 이미 만렙이거나 상대보다 최소 한 레벨 더 높으면(=병력
  // 체력/포탑에서 확실한 우위) 그 우위를 썩히지 말고 공격한다.
  bool hqOffensiveAdvantage =
      (myHqLevel >= HQ_MAX_LEVEL) || (myHqLevel > oppHqLevel);
  // 공격 조건은 오리지널 전략을 그대로 유지한다. 거점 선별 실험은
  // 공격 개시 여부에 관여하지 않는다.
  bool canOffensive = (!myIncomeAhead || hqOffensiveAdvantage) &&
                      !hasCapturableEmptyStronghold;

  // 상대 HQ 직접 공세는 일반 공세의 HQ 만렙/수입 게이트를 우회한다.
  // 거점 70% 이상과 실제 동원 병력 HP 우위는 안전장치로 유지하되, 지금
  // 동원 가능한 병력이 집결하고 이동해 남은 턴 안에 현재 HQ를 실제로
  // 파괴하는 경우만 결전을 허용한다. 미래 생산은 성공 판정에 넣지 않는다.
  bool directHqFeasible = false;
  bool directHqLockStartedThisTurn = false;
  int directHqCaptureDay = -1;
  int directHqForce = 0;
  int directHqStaging = -1;
  if (territoryReclaimTarget == -1 && !M.strongholds.empty()) {
    int myStrongCnt = 0;
    for (int t : M.strongholds)
      if (bld[t] != nullptr && bld[t]->side == M.my_side) ++myStrongCnt;
    bool directHqDominance =
        (double)myStrongCnt / (double)M.strongholds.size() >= 0.7 &&
        myTotalHp > oppTotalHp;

    const Building *enemyHq = bld[M.opp_hq];
    if (directHqDominance && enemyHq != nullptr &&
        enemyHq->side != M.my_side && enemyHq->type == BType::HQ) {
      int stagingH = std::numeric_limits<int>::max();
      double stagingD = std::numeric_limits<double>::infinity();
      for (const auto &b : S.buildings) {
        if (b.side != M.my_side) continue;
        int h = hops(b.region, M.opp_hq);
        if (h > stagingH) continue;
        if (h == stagingH && P.dist[b.region][M.opp_hq] >= stagingD) continue;
        stagingH = h;
        stagingD = P.dist[b.region][M.opp_hq];
        directHqStaging = b.region;
      }

      if (directHqStaging != -1 && stagingH < 9999) {
        // 실제 HQ 결전과 동일하게 각 거점의 방어 수요를 남긴다. 내 HQ는
        // 위협 대비 초과 예약만 풀고 노동 인구(work_cap)는 보존한다.
        std::vector<int> directNeed = need;
        if (bld[M.my_hq] != nullptr)
          directNeed[M.my_hq] = bld[M.my_hq]->work_cap();

        std::vector<const Warrior *> directCandidates;
        std::vector<int> keptDirect(N, 0);
        for (const Warrior *w : idle) {
          int r = w->region;
          if (r == M.opp_hq) continue;
          if (w->purpose == WPurpose::BUILD && bld[r] == nullptr) continue;
          if (keptDirect[r] < directNeed[r]) {
            ++keptDirect[r];
            continue;
          }
          directCandidates.push_back(w);
        }
        std::sort(directCandidates.begin(), directCandidates.end(),
                  [&](const Warrior *x, const Warrior *y) {
                    int hx = hops(x->region, directHqStaging);
                    int hy = hops(y->region, directHqStaging);
                    if (hx != hy) return hx < hy;
                    return x->hp > y->hp;
                  });

        std::vector<int> fixedHps;
        for (const auto &w : S.warriors)
          if (w.id.side == M.my_side && w.region == M.opp_hq)
            fixedHps.push_back(w.hp);
        std::vector<int> candidateHps;
        candidateHps.reserve(directCandidates.size());
        for (const Warrior *w : directCandidates) candidateHps.push_back(w->hp);

        std::vector<CW> hqGarrison;
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side && w.region == M.opp_hq)
            hqGarrison.push_back({w.hp, w.id.num});
        int myWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;
        int myTotalWarriors = 0;
        for (int c : myCnt) myTotalWarriors += c;
        int hqSafetyMargin =
            std::max(ATTACK_SAFETY_MARGIN, myTotalWarriors / 3);
        AttackPlan directPlan = plan_attack_force(
            enemyHq->hp, HQ_LEVELS[enemyHq->level].turret, hqGarrison,
            fixedHps, candidateHps, myWarriorHp, 0, hqSafetyMargin, true);

        int directAttackGold =
            std::max(0, gold + train_reserved - reserved_build);
        if (directPlan.extraToTrain == 0 && directPlan.sendCount > 0 &&
            directPlan.sendCount <= (int)directCandidates.size()) {
          int lastPlayableDay = MAX_TURN - turn - 1;
          if (lastPlayableDay >= 0) {
            // HQ는 목표 그 자체가 생산지라 공격대가 집결·이동하는 동안
            // 상대가 새 수비병을 바로 뽑을 수 있다. 현재 골드·순수입·훈련
            // 슬롯으로 매 턴 가능한 최대 수비 생산을 시간축에 넣고, 현재
            // 병력 중 몇 명을 보내야 그 경우까지 실제 파괴되는지 찾는다.
            for (int sendCount = directPlan.sendCount;
                 sendCount <= (int)directCandidates.size(); ++sendCount) {
              if (directAttackGold < sendCount * MOVE_COST) break;

              int gather = 0;
              std::vector<StreamArrival> atkArrivals, defArrivals;
              int arrivalNum = 0;
              for (int hp : fixedHps)
                atkArrivals.push_back({0, hp, arrivalNum++});
              for (int i = 0; i < sendCount; ++i)
                gather = std::max(
                    gather,
                    hops(directCandidates[i]->region, directHqStaging));
              int chargeArrival = gather + std::max(0, stagingH - 1);
              for (int i = 0; i < sendCount; ++i)
                atkArrivals.push_back(
                    {chargeArrival, directCandidates[i]->hp, arrivalNum++});
              for (const auto &w : S.warriors)
                if (w.id.side != M.my_side && w.region == M.opp_hq)
                  defArrivals.push_back({0, w.hp, w.id.num});

              long long projectedOppGold = std::max(0, S.opp_gold);
              long long projectedOppNetIncome = oppNetIncome;
              int nextDefNum = 6000000;
              int oppTrainCap = HQ_LEVELS[oppHqLevel].train_cap;
              for (int day = 0; day <= lastPlayableDay; ++day) {
                for (int slot = 0;
                     slot < oppTrainCap && projectedOppGold >= TRAIN_COST;
                     ++slot) {
                  projectedOppGold -= TRAIN_COST;
                  projectedOppNetIncome -= UPKEEP_PER_WARRIOR;
                  defArrivals.push_back(
                      {day, HQ_LEVELS[oppHqLevel].warrior_hp,
                       nextDefNum++});
                }
                projectedOppGold = std::max<long long>(
                    0, projectedOppGold + projectedOppNetIncome);
              }

              StreamCombatForecast forecast = simulate_reinforcement_stream(
                  enemyHq->hp, HQ_LEVELS[enemyHq->level].turret, true,
                  std::move(atkArrivals), std::move(defArrivals),
                  lastPlayableDay);
              if (forecast.captured) {
                directHqFeasible = true;
                directHqCaptureDay = forecast.captureDay;
                directHqForce = sendCount;
                break;
              }
            }
          }
        }
      }
    }
  }
  if (directHqFeasible)
    dbg::note(turn, "DIRECT_HQ_FEASIBLE target=R" +
                        std::to_string(M.opp_hq) +
                        " staging=R" + std::to_string(directHqStaging) +
                        " force=" + std::to_string(directHqForce) +
                        " capture_day=" +
                        std::to_string(directHqCaptureDay) +
                        " last_day=" +
                        std::to_string(MAX_TURN - turn - 1));
  if (directHqFeasible && !directHqAssault) {
    directHqAssault = true;
    directHqLockStartedThisTurn = true;
    dbg::note(turn, "DIRECT_HQ_LOCK target=R" +
                        std::to_string(M.opp_hq));
  }
  // 필수 영토가 적에게 넘어갔다면 일반 공격 게이트와 무관하게 그곳을
  // 최우선 목표로 잠근다. 적 기지가 있으면 공성전, 빈 땅에 적 병력만
  // 있으면 야전으로 계산한다. 적을 제거한 뒤에는 위 stronghold-first가
  // 같은 잠금 목표에 건설자를 보내 아군 기지를 완공한다.
  if (territoryReclaimTarget != -1) {
    const Building *rb = bld[territoryReclaimTarget];
    bool enemyBuilding = (rb != nullptr && rb->side != M.my_side);
    bool enemyOccupation = (rb == nullptr && oppCnt[territoryReclaimTarget] > 0);
    bool holdingForIncomingFieldBattle =
        reclaimAssault.postBreachHold && baseRaceHoldThreat;
#if ANTICIPATE_MANDATORY_PRECLAIM
    bool preparingForPredictedBuild =
        anticipatedMandatoryTarget == territoryReclaimTarget;
#else
    bool preparingForPredictedBuild = false;
#endif
    if (!raidReengageCooling &&
        (enemyBuilding || enemyOccupation || holdingForIncomingFieldBattle ||
         preparingForPredictedBuild)) {
      currentTarget = territoryReclaimTarget;
      territoryReclaimAttack = true;
      dbg::note(turn, "RECLAIM_ATTACK target=R" +
                          std::to_string(currentTarget) +
                          (enemyBuilding
                               ? " type=SIEGE"
                               : (preparingForPredictedBuild
                                      ? " type=PRECLAIM_PREP"
                               : (holdingForIncomingFieldBattle
                                      ? " type=FIELD_HOLD"
                                      : " type=FIELD"))));
    }
  }

  // 필수 탈환/선택 공세 모두 같은 턴 앞부분에서 계산된 방어 이동은
  // 보존한다. 공격 명령을 만들며 Actions를 초기화하더라도 방어 병력을
  // 내부 후보에서만 제거한 채 실제 이동은 취소하는 유령 재배치가 생기지
  // 않게 한다. 새 거점 건설과 일반 확장 명령만 취소한다.
  bool optionalTerritoryPush =
      territoryReclaimAttack && territoryReclaimIsExpansion;
  if (territoryReclaimAttack) {
    std::vector<MoveOrder> preservedDefenseMoves;
    std::vector<int> preservedHqUpgrades;
    int preservedHqUpgradeCost = 0;
    for (const auto &m : a.moves)
      if (m.purpose == WPurpose::MOVE &&
          !reclaimAssault.isCommitted(m.id.num))
        preservedDefenseMoves.push_back(m);
    // 필수 탈환이든 선택 공세든, 이번 턴 보유 골드로 이미 가능하다고 계산된
    // HQ 업그레이드는 취소하지 않는다. 미래 업그레이드 비용을 예약하는
    // 규칙이 아니라 지금 즉시 실행할 명령만 보존하며, 그 비용은 아래 공격
    // 이동·훈련 예산에서 먼저 뺀다.
    for (int r : a.upgrades) {
      if (r != M.my_hq || bld[r] == nullptr ||
          bld[r]->side != M.my_side || bld[r]->type != BType::HQ ||
          bld[r]->level >= HQ_MAX_LEVEL)
        continue;
      preservedHqUpgrades.push_back(r);
      preservedHqUpgradeCost += HQ_LEVELS[bld[r]->level + 1].upgrade_cost;
    }
    a = Actions{};
    a.moves = std::move(preservedDefenseMoves);
    a.upgrades = std::move(preservedHqUpgrades);
    gold = std::max(0, S.gold - preservedHqUpgradeCost);
    train_reserved = 0;
    reserved_build = 0;
    strongholdTrainNeed = 0;
    dbg::note(turn,
              std::string(optionalTerritoryPush
                              ? "PUSH_ATTACK_WITH_DEFENSE keep_moves="
                              : "RECLAIM_ATTACK_WITH_DEFENSE keep_moves=") +
                  std::to_string(a.moves.size()) +
                  " keep_hq_upgrade=" +
                  std::to_string(a.upgrades.size()) +
                  " gold=" + std::to_string(gold));
  }

  if (territoryReclaimAttack ||
      (territoryReclaimTarget == -1 &&
       (canOffensive || directHqAssault))) {
    // 회수 작전이 아닐 때의 일반 공격 목표 선정과 개시 조건은 오리지널
    // 전략을 그대로 유지한다.
    if (!territoryReclaimAttack) {
    // 위에서 현재 전력의 집결·이동·공성까지 남은 턴 안에 끝난다고 확인된
    // 경우에만 변두리 거점 스코어링을 건너뛰고 상대 HQ를 직접 노린다.
    if (directHqAssault) {
      currentTarget = M.opp_hq;
    } else {
      // 목표 선정(대응 시간 포함): 각 (집결지 s = 아군 건물, 목표 t = 상대
      // 거점, HQ 제외)에 대해 현재 수비대를 점령하는 데 걸리는 시간과,
      // 공격 경로로 목표가 드러난 뒤 상대의 첫 대응 병력이 도착하는 시간을
      // 함께 계산한다. 기본 소요 턴 = ① s에 y명이 실제로 모이는 데 걸리는 턴
      // (동원 가능한 유휴 병력을 s까지의 hop이 가까운 순으로 y명 골랐을 때
      // 그중 가장 늦게 도착하는 y번째의 hop) + ② s→t 이동 hop + ③ 그 y명을
      // (신병이 아니라 실제 체력 그대로) t의 포탑/상주 병력에 부딪혀 무너뜨리는
      // 데 걸리는 공성 일수. y는 1..동원가능수를 모두 훑어 이 총합이 가장
      // 작아지는 조합을 찾는다(더 많이 모으면 집결은 늦지만 공성이 빨라지는
      // 트레이드오프를 정확히 반영). 동률이면 y가 더 적은 쪽을 택한다.
      //
      // 대응 병력이 점령 전에 도착할 것으로 보이면, 겹치는 턴 수만큼 목표
      // 점수에 위험 페널티를 준다. 상대 예비대를 확정 수비대로 전투에 넣어
      // 공격 자체를 막지는 않는다. 실제 파병 인원/집결지는 여기 결과가 아니라
      // 아래 stagingPoint 계산과 plan_attack_force가 현재 수비/안전마진을
      // 반영해 다시 정한다.

      // 동원 가능한 유휴 병력(일자리/방어 need를 채우고 남는 잉여)의 위치·체력.
      struct MobW { int region, hp; };
      std::vector<MobW> pool;
      {
        std::vector<int> keptTmp(N, 0);
        for (const Warrior *w : idle) {
          int r = w->region;
          if (w->purpose == WPurpose::BUILD && bld[r] == nullptr) continue;
          if (keptTmp[r] < need[r]) { ++keptTmp[r]; continue; }
          pool.push_back({r, w->hp});
        }
      }

      // 집결지 s별로, 동원 병력을 s까지의 hop 오름차순(동률이면 체력 내림차순)
      // 으로 정렬해 둔다. y명을 앞에서부터 취하면 그게 "가장 빨리 모이는 y명"
      // 이고, y번째 원소의 hop이 곧 집결 소요 턴이다. s에 이미 있는 병력은
      // hop 0이라 맨 앞에 온다.
      std::vector<std::pair<int, std::vector<std::pair<int,int>>>> ordByStaging;
      for (const auto &s : S.buildings) {
        if (s.side != M.my_side) continue;
        std::vector<std::pair<int,int>> ord; // (hop to s, hp)
        ord.reserve(pool.size());
        for (const auto &mw : pool) ord.push_back({hops(mw.region, s.region), mw.hp});
        std::sort(ord.begin(), ord.end(), [](const std::pair<int,int>&A,
                                              const std::pair<int,int>&B) {
          return A.first != B.first ? A.first < B.first : A.second > B.second;
        });
        ordByStaging.push_back({s.region, std::move(ord)});
      }

      // aF(공격 병력, 실제 체력)로 (bldHp,turret,garrison) 거점을 무너뜨리는 데
      // 걸리는 일수. 못 뚫으면(공격측 전멸) NEVER.
      auto siegeDays = [&](std::vector<CW> aF, int bldHp, int turret,
                           std::vector<CW> defF) -> int {
        int hp = bldHp, dummy = -1, day = 0;
        while (!aF.empty() && hp > 0) {
          combatDay(aF, 0, dummy, defF, turret, hp);
          ++day;
        }
        return hp <= 0 ? day : NEVER;
      };

      // 공격대의 최종 목적지는 공개되지 않고 매 턴 실제 이동한 한 칸만
      // 보인다. 현재 상대 건물 후보 중 관측 경로와 일치하는 후보를 지워,
      // 목표가 하나로 좁혀지는 시점을 출격 후 탐지 오프셋으로 사용한다.
      auto targetDetectionOffset = [&](int staging, int target) -> int {
        int travel = hops(staging, target);
        if (travel >= 9999) return 9999;
        std::vector<int> plausible;
        for (const auto &candidate : S.buildings)
          if (candidate.side != M.my_side)
            plausible.push_back(candidate.region);
        if (std::find(plausible.begin(), plausible.end(), target) ==
            plausible.end())
          plausible.push_back(target);

        int observed = staging;
        for (int step = 0; step < travel; ++step) {
          int next = P.nxt[observed][target];
          if (next < 0) break;
          plausible.erase(
              std::remove_if(plausible.begin(), plausible.end(),
                             [&](int candidate) {
                               return P.nxt[observed][candidate] != next;
                             }),
              plausible.end());
          if (plausible.size() == 1) return step;
          observed = next;
        }
        // 끝까지 갈림길이 남으면 실제 거점에 진입한 순간에는 확정된다.
        return std::max(0, travel - 1);
      };

      // 첫 '가능 대응' 도착 시점만 목표 위험도로 사용한다. 이미 목표 쪽으로
      // 이동 중인 병력은 현재 시점부터의 실제 ETA를 쓰고, 정지 예비대는
      // 공격 목표 탐지 다음 턴부터 이동한다고 본다. 상대 건물의 노동 인구는
      // 예비대에서 제외한다. 아직 생산하지 않은 미래 병력은 상대가 실제로
      // 생산·방어를 선택할지 알 수 없으므로 여기서 확정 대응으로 세지 않는다.
      auto earliestResponseTurn = [&](int target, int staging, int gather,
                                      int detectionOffset) -> int {
        const int INF = 1000000000;
        int earliest = INF;
        std::vector<int> stationarySeen(N, 0);
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side || w.region == target) continue;
          if (w.state == WState::MOVING) {
            bool toward = w.prev_region >= 0 &&
                          P.nxt[w.prev_region][target] == w.region;
            if (!toward) continue;
            int h = hops(w.region, target);
            if (h < 9999) earliest = std::min(earliest, std::max(0, h - 1));
            continue;
          }

          int keep = 0;
          if (bld[w.region] != nullptr &&
              bld[w.region]->side != M.my_side)
            keep = bld[w.region]->work_cap();
          int seen = stationarySeen[w.region]++;
          if (seen < keep) continue;
          int h = hops(w.region, target);
          if (h < 9999)
            earliest = std::min(earliest, gather + detectionOffset + h);
        }

        return earliest;
      };

      int bestScore = std::numeric_limits<int>::max();
      int bestTurns = std::numeric_limits<int>::max();
      int bestY = std::numeric_limits<int>::max();
      int bestDetection = -1;
      int bestResponse = -1;
      int bestSlack = 9999;
      int bestPenalty = 0;
      for (const auto &b : S.buildings) {
        if (b.side == M.my_side) continue;
        if (b.type == BType::HQ) continue; // 사령부는 이 단계에서 목표로 삼지 않음
        int turret = (b.type == BType::HQ) ? HQ_LEVELS[b.level].turret
                                           : BASE_LEVELS[b.level].turret;
        std::vector<CW> garrison;
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side && w.region == b.region)
            garrison.push_back({w.hp, w.id.num});

        for (const auto &so : ordByStaging) {
          int travel = hops(so.first, b.region);
          if (travel >= 9999) continue;
          const auto &ord = so.second;
          std::vector<CW> aF;
          aF.reserve(ord.size());
          for (int y = 1; y <= (int)ord.size(); ++y) {
            aF.push_back({ord[y - 1].second, y - 1});
            int gather = ord[y - 1].first; // y명 중 가장 늦게 도착하는 인원의 hop
            // 공성은 최소 1일, 대응 위험 페널티는 0 이상이다. 이 하한부터
            // 현재 최선 이상이면 더 큰 y로 개선할 수 없다.
            if (gather + travel >= bestScore) break;
            int sd = siegeDays(aF, b.hp, turret, garrison);
            if (sd == NEVER) continue; // 이 y로는 아직 못 뚫음 -> 더 모아본다
            int total = gather + travel + sd;
            int detectionOffset = targetDetectionOffset(so.first, b.region);
            int response = earliestResponseTurn(
                b.region, so.first, gather, detectionOffset);
            int slack = response >= 1000000000 ? 9999 : response - total;
            // 같은 턴 도착도 전투 전에 합류할 수 있으므로 위험으로 표시한다.
            // 다만 대응 병력 수와 실제 방어 선택은 아직 불확실하므로 일찍
            // 도착하는 턴 수를 누적하지 않고 목표 점수에 1점만 더한다.
            int penalty = (slack != 9999 && slack <= 0) ? 1 : 0;
            int score = total + penalty;
            if (score < bestScore ||
                (score == bestScore && total < bestTurns) ||
                (score == bestScore && total == bestTurns && y < bestY)) {
              bestScore = score;
              bestTurns = total;
              bestY = y;
              bestDetection = gather + detectionOffset;
              bestResponse = response >= 1000000000 ? -1 : response;
              bestSlack = slack;
              bestPenalty = penalty;
              currentTarget = b.region;
            }
          }
        }
      }

      if (currentTarget != -1)
        dbg::note(turn, "ATTACK_TARGET_RESPONSE target=R" +
                            std::to_string(currentTarget) +
                            " capture=" + std::to_string(bestTurns) +
                            " detect=" + std::to_string(bestDetection) +
                            " response=" + std::to_string(bestResponse) +
                            " slack=" + std::to_string(bestSlack) +
                            " penalty=" + std::to_string(bestPenalty) +
                            " score=" + std::to_string(bestScore) +
                            " force=" + std::to_string(bestY));

      if (currentTarget == -1) {
        // 폴백: 지금 동원 가능한 유휴 병력만으로는 어떤 상대 거점도 못 뚫는다
        // (pool이 비었거나 전부 전멸). 기존 방식대로 내 사령부에서 hop이 가장
        // 가까운 상대 건물(HQ 포함)을 목표로 잡고, 필요한 인원은 아래
        // plan_attack_force/훈련이 채운다.
        int bestH = std::numeric_limits<int>::max();
        double bestD = std::numeric_limits<double>::infinity();
        for (const auto &b : S.buildings) {
          if (b.side == M.my_side) continue;
          int d = hops(M.my_hq, b.region);
          if (d == 9999) continue;
          if (d > bestH) continue;
          if (d == bestH && P.dist[M.my_hq][b.region] >= bestD) continue;
          bestH = d; bestD = P.dist[M.my_hq][b.region]; currentTarget = b.region;
        }
      }
    }
    }
    if (currentTarget != -1) {
      // 공격을 "뚫을 수 있다는 확신이 설 때만" 내보내면, 상대 거점
      // 레벨이 조금만 높아도(hp/turret이 세지면) 확신이 안 서서 공격
      // 자체가 영영 안 나가는 문제가 있었다(수급 1000골드 차로 밀리는데도
      // 공격 명령이 안 나감). 그렇다고 확신 없이 유휴 병력을 무작정 다
      // 보내면 필요 이상으로 병력을 낭비하게 된다. 그래서 그 거점의
      // 포탑과 지금 상주 중인 병력(hp, 마릿수)만 보고 몇 명을 보내야
      // 건물을 무너뜨릴 수 있는지 직접 시뮬레이션한다.
      //
      // 단, 공격 목표가 상대의 진짜 사령부(HQ)라면 얘기가 다르다: 그건
      // 승부를 끝낼 수 있는 결전이므로, 내 본부의 방어 예약분(need[my_hq]가
      // work_cap을 넘는 초과분, 즉 위협 대비 병력)까지는 투입한다. 다만
      // 일자리(work_cap)만큼은 그대로 남겨서 사령부가 노동 가능 인구
      // 밑으로 떨어지지 않게 한다 — 결전 중에도 수입이 완전히 끊기면 안
      // 된다.
      effNeed = need;
      if (territoryReclaimAttack && !optionalTerritoryPush) {
        // 공격 중에도 노동 인구만은 남겨 다음 증원 생산의 수입원을
        // 유지한다. 위협 방어분·건설 대기분 등 나머지는 모두 동원한다.
        std::fill(effNeed.begin(), effNeed.end(), 0);
        for (const auto &b : S.buildings)
          if (b.side == M.my_side)
            effNeed[b.region] = b.work_cap();
      } else if (currentTarget == M.opp_hq)
        effNeed[M.my_hq] = bld[M.my_hq]->work_cap();

      // 공격 집결지는 기존 최고판과 동일하게 공격 목표에 가장 가까운
      // 아군 건물로 잡는다. ALL_BASES_RALLY_SIM은 상대 정지 집결군에
      // 대응하는 방어 위치만 바꾸며 공격 작전에는 관여하지 않는다.
      if (stagingPoint == -1) {
#if DYNAMIC_OPTIONAL_PUSH_FIXED_RALLY
        if (territoryReclaimAttack && territoryReclaimIsExpansion &&
            !reclaimAssault.launched && optionalPushFixedRally != -1)
          stagingPoint = optionalPushFixedRally;
#endif
      }
      if (stagingPoint == -1) {
        int stagingH = std::numeric_limits<int>::max();
        double stagingD = std::numeric_limits<double>::infinity();
        for (const auto &b : S.buildings) {
          if (b.side != M.my_side) continue;
          int h = hops(b.region, currentTarget);
          if (h > stagingH) continue;
          if (h == stagingH &&
              P.dist[b.region][currentTarget] >= stagingD)
            continue;
          stagingH = h;
          stagingD = P.dist[b.region][currentTarget];
          stagingPoint = b.region;
        }
      }
#if DIRECT_ACTIVE_FIELD_RECLAIM
      // 이미 싸우는 빈 필수 거점에는 별도 집결지를 두지 않는다. 공격 계획의
      // 목적지와 실제 이동 목적지를 모두 전투 지역 자체로 맞춘다.
      if (activeMandatoryFieldBattleNow && territoryReclaimAttack)
        stagingPoint = currentTarget;
#endif

      // 이미 지난 턴들에 이 작전을 위해 파병되어 지금 집결지나 목표로
      // 이동 중인(아직 유휴 상태가 아닌) 병력. 이미 결정되어 오고 있는
      // 몫이니 무조건 계산에 포함시켜야 한다 — 이걸 무시하면 매 턴 이미
      // 오고 있는 병력이 없는 셈 치고 또 훈련/파병을 지시하게 되어
      // 필요 인원(surplus)을 과하게 책정하게 된다.
      std::vector<int> committedHps;
      // 목표 지역에서 이미 싸우는 병력은 출격 전력에 고정 포함한다.
      // 반면 집결지에 도착한 committed 병력은 아래 fresh 후보로 남겨 실제
      // 동시 출격 인원에 포함한다.
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side) continue;
        // 목표 지역에 이미 들어가 싸우고 있는 아군은 야전/공성 시뮬의
        // 고정 전력이다. 이들을 빼면 매 턴 같은 수만큼 또 생산하게 된다.
        if (w.state == WState::STATIONARY && w.region == currentTarget) {
          committedHps.push_back(w.hp);
          committedIds.push_back(w.id.num);
          targetFixedIds.push_back(w.id.num);
          continue;
        }
        if (w.state != WState::MOVING) continue;
        // 이번 잠금 작전이 직접 지정한 병력만 고정 전력으로 센다. 목적만
        // ATTACK인 다른 일반 공세 병력을 섞으면 목표 선택과 실행이 다시
        // 달라질 수 있다.
        bool directHqCommitted =
            directHqAssault && currentTarget == M.opp_hq &&
            w.purpose == WPurpose::ATTACK;
        bool belongsToThisAssault = directHqCommitted ||
            (cautiousMandatoryRetry()
                 ? (w.purpose == WPurpose::ATTACK)
                 : reclaimAssault.isCommitted(w.id.num));
#if DIRECT_ACTIVE_FIELD_RECLAIM
        belongsToThisAssault = belongsToThisAssault ||
            (activeMandatoryFieldBattleNow &&
             w.purpose == WPurpose::ATTACK);
#endif
        if (belongsToThisAssault) {
          committedHps.push_back(w.hp);
          committedIds.push_back(w.id.num);
          if (territoryReclaimAttack && !reclaimAssault.launched)
            reclaimAssemblyInTransit = true;
        }
      }

      // 실제로 보낼 수 있는 유휴 병력 후보(일자리/방어 수요를 채우고 남는
      // 잉여)를 실제 체력 그대로 모은다. 다들 똑같이 myWarriorHp짜리
      // 신병이라고 가정하지 않는다 — 이미 전투를 겪어 체력이 깎인 유닛도
      // 섞여 있을 수 있기 때문이다. 체력이 높은 순으로 정렬해서, 필요한
      // 최소 인원만 추릴 때 더 튼튼한 쪽부터 쓴다.
      std::vector<int> kept0(N, 0);
      for (const Warrior *w : idle) {
        int r = w->region;
        if (r == currentTarget) continue;
        // 작전에 배정된 병력은 집결지에 도착해 stationary가 되어도 방어
        // 예약을 다시 적용하지 않고 항상 공격 후보로 유지한다.
        bool directHqReserved =
            directHqAssault && currentTarget == M.opp_hq &&
            w->purpose == WPurpose::ATTACK;
        bool reservedForAssault = directHqReserved ||
            (cautiousMandatoryRetry()
                 ? (w->purpose == WPurpose::ATTACK)
                 : reclaimAssault.isCommitted(w->id.num));
#if DIRECT_ACTIVE_FIELD_RECLAIM
        reservedForAssault = reservedForAssault ||
            (activeMandatoryFieldBattleNow &&
             w->purpose == WPurpose::ATTACK);
#endif
        if (reservedForAssault) {
          attackCandidates.push_back(w);
          continue;
        }
        if (!territoryReclaimAttack && w->purpose == WPurpose::BUILD &&
            bld[r] == nullptr)
          continue;
        if (kept0[r] < effNeed[r]) { ++kept0[r]; continue; }
        attackCandidates.push_back(w);
      }
      // 체력 순으로만 정렬하면, 이미 집결지에 도착한 병력이 체력이 낮다는
      // 이유만으로 상위 neededExtra명(candidates)에서 밀려날 수 있다. 그러면
      // readyAtStaging이 실제 집결 인원보다 적게 잡혀 언제까지고 돌격
      // (readyToCharge)을 못 하고 계속 더 모으라는 명령만 나가는 문제가
      // 생긴다. 그래서 집결지까지 거리(가까운 쪽 우선)를 1순위로, 체력을
      // 2순위로 정렬해 이미 도착한 병력이 항상 먼저 뽑히게 한다.
      std::sort(attackCandidates.begin(), attackCandidates.end(),
               [&](const Warrior *x, const Warrior *y) {
                 int hx = hops(x->region, stagingPoint), hy = hops(y->region, stagingPoint);
                 if (hx != hy) return hx < hy;
                 return x->hp > y->hp;
               });

      const Building *tb = bld[currentTarget];
      bool targetHasBuilding = (tb != nullptr);
      int targetSimulationHp = tb != nullptr ? tb->hp : 0;
      int trg_turret = 0;
      if (tb != nullptr)
        trg_turret = (tb->type == BType::HQ) ? HQ_LEVELS[tb->level].turret
                                              : BASE_LEVELS[tb->level].turret;
#if ANTICIPATE_MANDATORY_PRECLAIM
      bool anticipatedTargetSimulation =
          anticipatedMandatoryTarget == currentTarget && tb == nullptr;
      if (anticipatedTargetSimulation) {
        // 준비 인원을 빈 땅 기준으로 계산하면 상대가 예정대로 L1 기지를
        // 지은 순간 다시 필요 인원이 늘어난다. 처음부터 그 건설 결과의
        // HP와 포탑을 목표로 계산해 준비량이 뒤늦게 흔들리지 않게 한다.
        targetHasBuilding = true;
        targetSimulationHp = BASE_LEVELS[1].hp;
        trg_turret = BASE_LEVELS[1].turret;
        dbg::note(turn, "RECLAIM_PRECLAIM_SIM target=R" +
                            std::to_string(currentTarget) +
                            " build_abs=T" +
                            std::to_string(anticipatedMandatoryBuildTurn) +
                            " hp=" +
                            std::to_string(targetSimulationHp) +
                            " turret=" + std::to_string(trg_turret));
      }
#else
      bool anticipatedTargetSimulation = false;
#endif
#ifdef UNIFIED_RECLAIM_STREAM
      bool predictedEmptyFortify = tb == nullptr &&
          opponent_can_fortify_empty_target_now(S, M, currentTarget);
      if (predictedEmptyFortify) {
        targetHasBuilding = true;
        targetSimulationHp = BASE_LEVELS[1].hp;
        trg_turret = BASE_LEVELS[1].turret;
        dbg::note(turn, "RECLAIM_FORTIFY predicted=BUILD R" +
                            std::to_string(currentTarget) +
                            " cost=" +
                            std::to_string(BASE_LEVELS[1].cost) +
                            " income=" +
                            std::to_string(WORK_INCOME));
      }
#endif
      std::vector<CW> garrison;
      int movingGarrison = 0;
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side) continue;
        bool atTarget = w.region == currentTarget;
        bool movingToward = false;
#if ANTICIPATE_MANDATORY_PRECLAIM
        movingToward = anticipatedTargetSimulation && !atTarget &&
                       w.state == WState::MOVING &&
                       w.prev_region >= 0 &&
                       w.prev_region != w.region &&
                       myCnt[w.region] == 0 &&
                       P.nxt[w.prev_region][currentTarget] == w.region;
#endif
#if INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM
        movingToward = movingToward ||
                       (!atTarget && w.state == WState::MOVING &&
                       w.prev_region >= 0 && w.prev_region != w.region &&
                       myCnt[w.region] == 0 &&
                       P.nxt[w.prev_region][currentTarget] == w.region);
#endif
        if (atTarget || movingToward) {
          garrison.push_back({w.hp, w.id.num});
          if (movingToward) ++movingGarrison;
        }
      }
#if INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM
      if (movingGarrison > 0)
        dbg::note(turn, "RECLAIM_INITIAL_MOVING_DEF count=" +
                            std::to_string(movingGarrison) + " target=R" +
                            std::to_string(currentTarget));
#endif
      int myWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;

      std::vector<int> freshHpsDesc;
      freshHpsDesc.reserve(attackCandidates.size());
      for (const Warrior *w : attackCandidates) freshHpsDesc.push_back(w->hp);

      int trainCap = std::max<size_t>(1, my_hq_train_cap(S, M) * (size_t)(MAX_TURN - turn));
      // 목표가 상대의 진짜 사령부면 결전이라 예측이 틀렸을 때 되돌릴 수
      // 없다. 그래서 평소 마진(ATTACK_SAFETY_MARGIN=1명)보다 훨씬 두텁게,
      // 지금 내가 보유한 전체 병력의 1/3만큼을 여유로 잡는다. 다만 병력
      // 총수가 적을 때는 1/3이 1명보다 작아질 수 있으므로, 최소한 평소
      // 마진(1명)보다는 못하지 않게 max로 하한을 둔다.
      int myTotalWarriors = 0;
      for (int c : myCnt) myTotalWarriors += c;
      bool directHqWaveInFlight = false;
      if (currentTarget == M.opp_hq) {
        for (const auto &w : S.warriors) {
          if (w.id.side != M.my_side) continue;
          if (w.region == M.opp_hq ||
              (w.state == WState::MOVING &&
               w.purpose == WPurpose::ATTACK)) {
            directHqWaveInFlight = true;
            break;
          }
        }
      }
      // 필수 영토 회수는 한 번의 완벽한 결전을 기다리는 작전이 아니라,
      // 현재 보이는 수비를 이길 최소 전력에 1명만 더해 신속히 치고
      // 실패/재점유 시 같은 목표에 계속 증원하는 작전이다. 일반 공세의
      // 일반 공세의 1명 마진과 상대 HQ 결전 마진은 별도로 적용한다.
      int mandatoryReclaimMargin =
#ifdef UNIFIED_RECLAIM_STREAM
          UNIFIED_RECLAIM_SAFETY_MARGIN;
#else
          MANDATORY_RECLAIM_INITIAL_MARGIN;
#endif
#if MANDATORY_MARGIN_TWO_WHEN_BASE_AHEAD
      // 경제적으로 앞서 기지 한 곳을 더 유지하고 있을 때는 필수 거점
      // 탈환을 최소 전력+1로 아슬아슬하게 보내지 않고 +2까지 확보한다.
      // HQ는 양쪽에 기본 한 채씩 있으므로 비교에서 제외하고, 실제 BASE
      // 개수만 센다. 선택 공세에는 이 동적 마진을 적용하지 않는다.
      if (territoryReclaimAttack && !territoryReclaimIsExpansion) {
        int myBaseCount = 0, oppBaseCount = 0;
        for (const auto &base : S.buildings) {
          if (base.type != BType::BASE) continue;
          if (base.side == M.my_side)
            ++myBaseCount;
          else
            ++oppBaseCount;
        }
        if (myBaseCount > oppBaseCount)
          mandatoryReclaimMargin = std::max(
              mandatoryReclaimMargin, MANDATORY_RECLAIM_AHEAD_MARGIN);
      }
#endif
      int safetyMargin = territoryReclaimAttack
          ? mandatoryReclaimMargin
          : ((currentTarget == M.opp_hq)
              ? (directHqWaveInFlight
                     ? 0
                     : std::max(ATTACK_SAFETY_MARGIN,
                                myTotalWarriors / 3))
              : ATTACK_SAFETY_MARGIN);
      AttackPlan plan = plan_attack_force(targetSimulationHp,
                                         trg_turret, garrison,
                                         committedHps, freshHpsDesc,
                                         myWarriorHp, trainCap, safetyMargin,
                                         targetHasBuilding);
      // 확정 파괴 검증이 이번 턴 처음 잠금을 건 경우에는, 미래 HQ 생산까지
      // 이긴 최소 인원을 그대로 첫 집결 규모로 사용한다. 아래 일반 계획은
      // 현재 주둔군만 보므로 그보다 적은 병력을 고를 수 있다.
      if (directHqLockStartedThisTurn && currentTarget == M.opp_hq) {
        plan.sendCount = std::max(
            plan.sendCount,
            std::min(directHqForce, (int)attackCandidates.size()));
        plan.extraToTrain = 0;
      }
      // 상대 영토로의 선택 공세는 필수 영토 탈환과 달리, 아직 출발도
      // 하기 전에 최신 수비가 남은 턴의 생산 한도로 뚫을 수 없게 됐다면
      // 고집할 이유가 없다. 잠금을 풀고 평시 운영으로 돌아가 다음 턴에
      // 다시 공략 가능한 목표를 찾는다. 이미 선발대가 출발한 뒤라면 기존
      // 연속 증원 규칙을 유지한다.
      bool abortImpossiblePush = territoryReclaimIsExpansion &&
                                 !reclaimAssault.launched &&
                                 plan.extraToTrain < 0;
      if (abortImpossiblePush) {
        dbg::note(turn, "PUSH_ABORT impossible target=R" +
                            std::to_string(currentTarget));
        optionalPushCooldownUntil = std::max(optionalPushCooldownUntil, turn + 15);
        territoryReclaimTarget = -1;
        territoryReclaimIsExpansion = false;
        reclaimAssault.reset(-1, turn);
        territoryReclaimAttack = false;
        currentTarget = -1;
        neededExtra = 0;
        extraToTrain = -1;
        total_offensive = false;
      } else {
      // 선발대가 출발한 뒤에는 현재 교전/이동 병력, 지금 보낼 수 있는
      // 잉여, 현재 골드와 유지비로 매 턴 한 명씩 생산 가능한 후속 병력을
      // 실제 도착일에 합류시켜 끝까지 점령 가능한지 다시 검사한다. 상대는
      // 현재 목표 수비대와 이미 목표 방향으로 움직이는 것이 관측된 병력만
      // 포함한다. 아직 선택하지 않은 미래 생산/전 맵 예비대까지 확정 수비로
      // 당겨 쓰면 이전 시간축판처럼 과보수적으로 변하기 때문이다.
      territoryReinforcement =
          territoryReclaimAttack && reclaimAssault.launched &&
          !reclaimAssault.spearhead.empty();
      if (territoryReinforcement) {
        int streamAtTarget = 0;
        int streamInFlight = 0;
        int streamIdle = 0;
        int streamFuture = 0;
        int streamObservedDefense = 0;
        int streamFutureDefense = 0;
        int targetDefendersNow = (int)garrison.size();
        if (reclaimAssault.lastTargetDefenders >= 0 &&
            targetDefendersNow > reclaimAssault.lastTargetDefenders)
          reclaimAssault.defenseResponseSeen = true;
        reclaimAssault.lastTargetDefenders = targetDefendersNow;
#ifdef UNIFIED_RECLAIM_STREAM
        UnifiedReclaimStreamStats unifiedStats;
        StreamCombatForecast forecast = forecast_unified_reclaim_stream(
            S, M, P, turn, currentTarget, reclaimAssault.committed,
            attackCandidates, -1, 0, myWarriorHp,
            HQ_LEVELS[oppHqLevel].warrior_hp,
            current_net_income, oppNetIncome, true, &unifiedStats);
        streamAtTarget = unifiedStats.atTarget;
        streamInFlight = unifiedStats.inFlight;
        streamIdle = unifiedStats.fresh;
        streamFuture = unifiedStats.futureAttackers;
        streamObservedDefense = unifiedStats.targetDefenders +
                                unifiedStats.movingDefenders +
                                unifiedStats.reserveDefenders;
        streamFutureDefense = unifiedStats.futureDefenders;
#else
        auto reinforcementForecast = [&]() -> StreamCombatForecast {
          int horizon = std::max(0, MAX_TURN - turn - 1);
          std::vector<StreamArrival> atkArrivals, defArrivals;
          std::vector<int> includedIds;
          int nextAtkNum = 3000000;

          // 이미 목표에서 싸우는 병력은 0일 전력이다.
          for (const auto &w : S.warriors) {
            if (w.id.side != M.my_side || w.region != currentTarget) continue;
            atkArrivals.push_back({0, w.hp, w.id.num});
            includedIds.push_back(w.id.num);
            ++streamAtTarget;
          }

          // 이미 명령을 받아 목표/경유지로 이동 중인 작전 병력은 새 명령과
          // 무관하게 파이프라인에 남는다. 다른 지역에서 적과 교전 중이면
          // 그 지역을 벗어날 수 없으므로 도착 전력으로 미리 세지 않는다.
          for (const auto &w : S.warriors) {
            if (w.id.side != M.my_side || w.region == currentTarget ||
                w.state != WState::MOVING ||
                !reclaimAssault.isCommitted(w.id.num) ||
                oppCnt[w.region] > 0)
              continue;
            int h = hops(w.region, currentTarget);
            if (h >= 9999) continue;
            atkArrivals.push_back({std::max(0, h - 1), w.hp, w.id.num});
            includedIds.push_back(w.id.num);
            ++streamInFlight;
          }

          // 현재 방어/노동 예약을 채우고도 남아 이번 턴 실제 파견 대상이
          // 되는 유휴 병력은 명령 직후 첫 칸을 움직이므로 h-1일에 도착한다.
          for (const Warrior *w : attackCandidates) {
            if (std::find(includedIds.begin(), includedIds.end(), w->id.num) !=
                includedIds.end())
              continue;
            int h = hops(w->region, currentTarget);
            if (h >= 9999) continue;
            atkArrivals.push_back({std::max(0, h - 1), w->hp, w->id.num});
            includedIds.push_back(w->id.num);
            ++streamIdle;
          }

          for (const auto &w : S.warriors) {
            if (w.id.side == M.my_side) continue;
            if (w.region == currentTarget) {
              defArrivals.push_back({0, w.hp, w.id.num});
              ++streamObservedDefense;
              continue;
            }
            if (w.state != WState::MOVING || w.prev_region < 0) continue;
            bool toward = P.nxt[w.prev_region][currentTarget] == w.region;
            if (!toward) continue;
            reclaimAssault.defenseResponseSeen = true;
            int h = hops(w.region, currentTarget);
            if (h >= 9999) continue;
            defArrivals.push_back({std::max(0, h - 1), w.hp, w.id.num});
            ++streamObservedDefense;
          }

          // 공격 목표 쪽 이동 또는 목표 수비대 증가가 실제로 한 번이라도
          // 관측된 뒤에는 상대가 방어를 시작했다고 본다. 그 전에는 미래
          // 행동을 만들지 않지만, 시작이 확인된 뒤에는 상대도 현재 추정
          // 골드/순수입/유지비로 다음 3턴 동안 한 명씩 생산해 같은 거점을
          // 지키는 경우를 계산한다. 실측상 탐지 뒤 3턴 이내 출발 병력이 평균
          // 약 2.92명이었고, 그 이후는 다음 턴의 새 관측으로 다시 계산한다.
          // 탐지 시점을 추측하는 대신 실제 반응을 스위치로 쓰고, 한 번
          // 반응했다는 이유로 남은 게임 내내 올인한다고 가정하지 않는다.
          if (reclaimAssault.defenseResponseSeen) {
            long long projectedOppGold = std::max(0, S.opp_gold);
            long long projectedOppNetIncome = oppNetIncome;
            int oppHqTravel = hops(M.opp_hq, currentTarget);
            bool freeDefenseMove = tb != nullptr && tb->side != M.my_side;
            int defenderCost = TRAIN_COST +
                               (freeDefenseMove ? 0 : MOVE_COST);
            int nextDefNum = 4000000;
            const int responseLookahead =
                std::min(REINFORCE_RESPONSE_LOOKAHEAD, horizon + 1);
            for (int day = 0; day < responseLookahead; ++day) {
              if (oppHqTravel < 9999 && projectedOppGold >= defenderCost) {
                projectedOppGold -= defenderCost;
                projectedOppNetIncome -= UPKEEP_PER_WARRIOR;
                int arrival = day + oppHqTravel;
                if (arrival <= horizon) {
                  defArrivals.push_back(
                      {arrival, HQ_LEVELS[oppHqLevel].warrior_hp,
                       nextDefNum++});
                  ++streamFutureDefense;
                }
              }
              projectedOppGold = std::max<long long>(
                  0, projectedOppGold + projectedOppNetIncome);
            }
          }

          // 현재 연속 증원 정책은 HQ 슬롯이 남아도 공격용으로는 매 턴 한
          // 명만 요구한다. 그 정책 그대로 훈련비, 마지막 적 지역 이동비,
          // 새 병력 유지비를 차감해 실제 생산 가능한 날만 추가한다.
          long long projectedGold = std::max(0, gold);
          long long projectedNetIncome = current_net_income;
          int hqTravel = hops(M.my_hq, currentTarget);
          for (int day = 0; day <= horizon; ++day) {
            if (hqTravel < 9999 &&
                projectedGold >= TRAIN_COST + MOVE_COST) {
              projectedGold -= TRAIN_COST + MOVE_COST;
              projectedNetIncome -= UPKEEP_PER_WARRIOR;
              int arrival = day + hqTravel;
              if (arrival <= horizon) {
                atkArrivals.push_back(
                    {arrival, myWarriorHp, nextAtkNum++});
                ++streamFuture;
              }
            }
            projectedGold = std::max<long long>(
                0, projectedGold + projectedNetIncome);
          }

          return simulate_reinforcement_stream(
              tb != nullptr ? tb->hp : 0, trg_turret, targetHasBuilding,
              std::move(atkArrivals), std::move(defArrivals), horizon,
              reclaimAssault.postBreachHold);
        };

        StreamCombatForecast forecast = reinforcementForecast();
#endif
        bool rawRolloutCaptured = forecast.captured;
        if (forecast.captured &&
            forecast.attackerSurvivors < REINFORCE_CAPTURE_SURVIVOR_MARGIN)
          forecast.captured = false;
        dbg::note(turn, "REINFORCE_ROLLOUT result=" +
                            std::string(forecast.captured
                                            ? "CAPTURE"
                                            : "FAIL") +
                            " day=" + std::to_string(forecast.captureDay) +
                            " at_target=" + std::to_string(streamAtTarget) +
                            " in_flight=" + std::to_string(streamInFlight) +
                            " idle=" + std::to_string(streamIdle) +
                            " future=" + std::to_string(streamFuture) +
                            " defenders=" +
                            std::to_string(streamObservedDefense) +
                            " future_def=" +
                            std::to_string(streamFutureDefense) +
                            " survivors=" +
                            std::to_string(forecast.attackerSurvivors) +
                            " survivor_margin=" +
                            std::to_string(REINFORCE_CAPTURE_SURVIVOR_MARGIN) +
                            " response_turns=" +
                            std::to_string(REINFORCE_RESPONSE_LOOKAHEAD) +
                            " raw_capture=" +
                            std::to_string(rawRolloutCaptured ? 1 : 0) +
#ifdef UNIFIED_RECLAIM_STREAM
                            " reserve_def=" +
                            std::to_string(unifiedStats.reserveDefenders) +
                            " fortify=" +
                            std::to_string(unifiedStats.predictedFortify ? 1 : 0) +
                            " fortify_income=" +
                            std::to_string(unifiedStats.fortifyIncome) +
                            " horizon=" +
                            std::to_string(unifiedStats.horizon) +
#endif
                            " response_seen=" +
                            std::to_string(
                                reclaimAssault.defenseResponseSeen ? 1 : 0));

        if (forecast.captured) {
          neededExtra = (int)attackCandidates.size();
          extraToTrain = 1;
          total_offensive = true;
        } else if (territoryReclaimIsExpansion) {
          // 선택 공세는 더 부어도 못 먹는다는 최신 계산이면 추가 손실을
          // 중단하고 목표를 해제한다. 이미 교전 중인 병력은 후퇴할 수 없지만
          // 아직 도착하지 않은 병력과 이후 골드는 다른 작전에 남길 수 있다.
          dbg::note(turn, "PUSH_ABORT reinforcement_rollout_failed target=R" +
                              std::to_string(currentTarget));
          optionalPushCooldownUntil = std::max(optionalPushCooldownUntil,
                                               turn + 15);
          territoryReclaimTarget = -1;
          territoryReclaimIsExpansion = false;
          reclaimAssault.reset(-1, turn);
          territoryReclaimAttack = false;
          territoryReinforcement = false;
          currentTarget = -1;
          neededExtra = 0;
          extraToTrain = -1;
          total_offensive = false;
        } else {
          // 필수 거점은 포기하지 않는다. 다만 각개 증원은 멈추고 작전 목표와
          // 기존 committed 병력을 유지한 채 ASSEMBLE로 되돌아가 다음 파동을
          // 동기화한다. 이번 턴은 한 명 생산만 시도하고 새 이동은 내지 않는다.
          reclaimAssault.launched = false;
          reclaimAssault.spearhead.clear();
          ++reclaimAssault.failedWaves;
          reclaimAssault.assemblyDeadline = turn + 12;
          territoryReinforcement = false;
          neededExtra = 0;
          extraToTrain = 1;
          total_offensive = true;
          dbg::note(turn, "RECLAIM_STREAM regroup target=R" +
                              std::to_string(currentTarget) + " failed=" +
                              std::to_string(reclaimAssault.failedWaves));
        }
      } else {
        neededExtra = plan.sendCount;
        extraToTrain = plan.extraToTrain;
        // 일반 공격의 동작은 유지하되, 필수 영토 회수만큼은 현재 보낼
        // 유휴 병력이 0명이어도 시뮬이 요구한 신병 생산부터 시작한다.
        total_offensive = territoryReclaimAttack || (neededExtra > 0);
      }
      if (territoryReclaimAttack)
        dbg::note(turn, "RECLAIM_PLAN fixed=" +
                            std::to_string(committedHps.size()) +
                            " idle=" + std::to_string(freshHpsDesc.size()) +
                            " send=" + std::to_string(plan.sendCount) +
                            " train=" + std::to_string(plan.extraToTrain) +
                            " margin=" + std::to_string(safetyMargin) +
                            " phase=" +
                            (territoryReinforcement ? "REINFORCE" : "ASSEMBLE"));
      }
    }
  }

  if (total_offensive) {
    // 건설 대기 중인 유닛 몫의 건설비는 총공세용 이동/훈련에 쓰지 않고
    // 남겨둔다(reserved_build는 위에서 이미 계산해 둠). train_reserved는
    // 위 방어/확장 재배치 단계에서 strongholdTrainNeed 등을 위해 미리
    // 차감해둔 훈련비인데, 그 수요는 trainWant(아래)에 이미 반영되므로
    // 여기서 다시 더해줘야 이중으로 깎이지 않는다(안 더하면 그 돈이
    // 예약됐다는 이유만으로 정작 그 훈련조차 예산 부족으로 취소되는
    // 모순이 생긴다).
    // 회수 공격 중에는 건설비를 미리 남기지 않는다. 적이 살아 있는 동안
    // 300골드를 묶으면 선발대 출발 직후의 핵심 증원이 끊긴다. 건설비는
    // 적 제거 후 reclaimReadyToBuild 단계에서만 사용한다.
    int attackGold = territoryReclaimAttack
        ? gold
        : std::max(0, gold + train_reserved - reserved_build);
    // attackCandidates는 집결지까지 가까운 순, 동률이면 체력순이다.
    std::vector<const Warrior *> candidates;
    if (territoryReinforcement) {
      // 작전 개시 뒤에는 방어/일자리 예약을 통과한 모든 잉여가 증원이다.
      candidates = attackCandidates;
    } else {
      candidates.assign(
          attackCandidates.begin(),
          attackCandidates.begin() +
              std::min<size_t>(neededExtra, attackCandidates.size()));
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Warrior *x, const Warrior *y) {
      int hx = hops(x->region, currentTarget), hy = hops(y->region, currentTarget);
      if (hx != hy) return hx < hy;
      return P.dist[x->region][currentTarget] < P.dist[y->region][currentTarget];
    });

    // 필요한 인원이 한꺼번에 도착해야 plan_attack_force의 가정이 실제로
    // 맞는다. 예전처럼 각자 자기 위치에서 그때그때 가장 가까운 경유지로만
    // 보내면, 출발지가 제각각인 병력들이 서로 다른 날짜에 목표에 도착해
    // 상대에게 각개격파당한다. 그래서 목표와 가장 가까운 아군 거점
    // 하나(stagingPoint, 위 계획 단계에서 이미 구해 둠)에 병력을 먼저
    // 모으고, 이미 집결지에 모여 있는 인원만 실제로 목표를 향해 마지막
    // 발걸음을 내딛게 한다.

    // 여기서 필요한 건 "총력전"이 아니라 시뮬레이션으로 계산해 둔 정확히
    // neededExtra명이다. 그보다 많이 보내면 방어/경제에 쓸 병력을 불필요하게
    // 낭비하게 되므로, 딱 그만큼만 집결/돌격시킨다.
    //
    // 집결지에 이미 있는 인원이 neededExtra에 못 미치면 그 인원만 먼저
    // 돌격시키지 않는다 — 그러면 매 턴 한두 명씩 도착하는 족족 목표로
    // 흘려보내는 셈이라 동시 도착 가정이 다시 깨지고, 각개격파로 병력만
    // 낭비된다. 그러니 집결지에 실제로 neededExtra명이 다 모였을 때만
    // 한꺼번에 돌격시키고, 그 전까지는 전부 집결만 시킨다.
    int readyAtStaging = 0;
    if (stagingPoint != -1) {
      const auto &readyPool = cautiousMandatoryRetry()
                                  ? candidates
                                  : (territoryReclaimAttack
                                         ? attackCandidates
                                         : candidates);
      for (const Warrior *w : readyPool)
        if (w->region == stagingPoint) ++readyAtStaging;
    }

    // 출격 여부는 "이동 중인 병력까지 언젠가 합류한다"는 계획값이 아니라
    // 지금 집결지에 실제로 모인 병력만 다시 시뮬레이션해 결정한다. 이들이
    // 현재 수비를 이기면 다른 증원이 이동 중이어도 즉시 출격하고, 이동
    // 중 병력은 이후 REINFORCE 파이프라인으로 자연스럽게 합류한다.
    int initialChargeCount = neededExtra;
    bool readyToCharge = false;
    bool synchronizedMandatoryRetry =
        territoryReclaimAttack && !territoryReclaimIsExpansion &&
        reclaimAssault.failedWaves > 0;
    if (territoryReclaimAttack && !territoryReinforcement &&
        stagingPoint != -1 && !synchronizedMandatoryRetry) {
      const Building *chargeTarget = bld[currentTarget];
      bool chargeHasBuilding = chargeTarget != nullptr;
      int chargeBuildingHp = chargeTarget != nullptr ? chargeTarget->hp : 0;
      int chargeTurret = 0;
      if (chargeTarget != nullptr)
        chargeTurret = chargeTarget->type == BType::HQ
            ? HQ_LEVELS[chargeTarget->level].turret
            : BASE_LEVELS[chargeTarget->level].turret;
#if ANTICIPATE_MANDATORY_PRECLAIM
      bool chargeAnticipatesBuild =
          anticipatedMandatoryTarget == currentTarget &&
          chargeTarget == nullptr;
      if (chargeAnticipatesBuild) {
        chargeHasBuilding = true;
        chargeBuildingHp = BASE_LEVELS[1].hp;
        chargeTurret = BASE_LEVELS[1].turret;
      }
#else
      bool chargeAnticipatesBuild = false;
#endif
#ifdef UNIFIED_RECLAIM_STREAM
      bool chargePredictsFortify = chargeTarget == nullptr &&
          opponent_can_fortify_empty_target_now(S, M, currentTarget);
      if (chargePredictsFortify) {
        chargeHasBuilding = true;
        chargeBuildingHp = BASE_LEVELS[1].hp;
        chargeTurret = BASE_LEVELS[1].turret;
      }
#endif
      std::vector<CW> chargeGarrison;
      int movingChargeGarrison = 0;
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side) continue;
        bool atTarget = w.region == currentTarget;
        bool movingToward = false;
#if ANTICIPATE_MANDATORY_PRECLAIM
        movingToward = chargeAnticipatesBuild && !atTarget &&
                       w.state == WState::MOVING &&
                       w.prev_region >= 0 &&
                       w.prev_region != w.region &&
                       myCnt[w.region] == 0 &&
                       P.nxt[w.prev_region][currentTarget] == w.region;
#endif
#if INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM
        movingToward = movingToward ||
                       (!atTarget && w.state == WState::MOVING &&
                       w.prev_region >= 0 && w.prev_region != w.region &&
                       myCnt[w.region] == 0 &&
                       P.nxt[w.prev_region][currentTarget] == w.region);
#endif
        if (atTarget || movingToward) {
          chargeGarrison.push_back({w.hp, w.id.num});
          if (movingToward) ++movingChargeGarrison;
        }
      }
#if INCLUDE_MOVING_DEFENDERS_IN_INITIAL_RECLAIM
      if (movingChargeGarrison > 0)
        dbg::note(turn, "RECLAIM_LAUNCH_MOVING_DEF count=" +
                            std::to_string(movingChargeGarrison) +
                            " target=R" + std::to_string(currentTarget));
#endif
      int chargeWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;
      std::vector<int> fixedAtTarget;
      for (const auto &w : S.warriors)
        if (w.id.side == M.my_side && w.state == WState::STATIONARY &&
            w.region == currentTarget)
          fixedAtTarget.push_back(w.hp);
      std::vector<int> stagedHps;
      std::vector<const Warrior *> stagedCandidates;
      for (const Warrior *w : attackCandidates)
        if (w->region == stagingPoint) {
          stagedHps.push_back(w->hp);
          stagedCandidates.push_back(w);
        }
      int launchSafetyMargin =
#ifdef UNIFIED_RECLAIM_STREAM
          UNIFIED_RECLAIM_SAFETY_MARGIN;
#else
          MANDATORY_RECLAIM_INITIAL_MARGIN;
#endif
      int effectiveLaunchSafetyMargin = launchSafetyMargin;
      AttackPlan launchPlan = plan_attack_force(
          chargeBuildingHp, chargeTurret,
          chargeGarrison, fixedAtTarget, stagedHps, chargeWarriorHp, 0,
          launchSafetyMargin,
          chargeHasBuilding);
#if MANDATORY_MARGIN_TWO_UNMATCHED_ONLY
      // BASE 우위에서 쓰는 +2 마진은 실제 출격 직전에도 유지하되, 그 한
      // 명을 더 기다리는 동안 상대도 HQ에서 새 수비병을 더 만들어 목표에
      // 투입할 수 있다면 실질적인 머릿수 우위가 늘지 않는다. 먼저 +1
      // 파동이 지금 출격 가능한지 확인하고, +2 파동이 아직 준비되지 않은
      // 경우에만 양쪽 시간축을 비교한다.
      bool launchMarginTwoEligible = false;
      if (territoryReclaimAttack && !territoryReclaimIsExpansion) {
        int myBaseCount = 0, oppBaseCount = 0;
        for (const auto &base : S.buildings) {
          if (base.type != BType::BASE) continue;
          if (base.side == M.my_side)
            ++myBaseCount;
          else
            ++oppBaseCount;
        }
        launchMarginTwoEligible = myBaseCount > oppBaseCount;
      }

      if (launchMarginTwoEligible) {
        AttackPlan marginTwoLaunchPlan = plan_attack_force(
            chargeBuildingHp, chargeTurret,
            chargeGarrison, fixedAtTarget, stagedHps, chargeWarriorHp, 0,
            std::max(launchSafetyMargin, MANDATORY_RECLAIM_AHEAD_MARGIN),
            chargeHasBuilding);
        int plusOneCount = launchPlan.sendCount +
                           std::max(0, launchPlan.extraToTrain);
        int plusTwoCount = marginTwoLaunchPlan.sendCount +
                           std::max(0, marginTwoLaunchPlan.extraToTrain);
        bool plusOneReady = launchPlan.sendCount > 0 &&
                            launchPlan.extraToTrain == 0;
        bool plusTwoReady = marginTwoLaunchPlan.sendCount > 0 &&
                            marginTwoLaunchPlan.extraToTrain == 0;

        if (plusTwoReady) {
          // 추가 한 명이 이미 집결해 있다면 지연이 전혀 없으므로 그대로
          // +2 파동을 출격시킨다.
          launchPlan = marginTwoLaunchPlan;
          effectiveLaunchSafetyMargin = std::max(
              effectiveLaunchSafetyMargin, MANDATORY_RECLAIM_AHEAD_MARGIN);
          dbg::note(turn, "RECLAIM_MARGIN2_TIMING action=MARGIN2_NOW plus1=" +
                              std::to_string(plusOneCount) + " plus2=" +
                              std::to_string(plusTwoCount));
        } else if (plusOneReady) {
          const int horizon = std::max(0, MAX_TURN - turn - 1);
          int ownExtraWait = std::numeric_limits<int>::max();

          // 아직 집결지 밖에 있는 실제 공격 후보 중 가장 빨리 합류할 한
          // 명. 이미 이 작전에 커밋되어 이동 중인 병력도 빠뜨리지 않는다.
          for (const Warrior *w : attackCandidates) {
            if (w->region == stagingPoint) continue;
            int h = hops(w->region, stagingPoint);
            if (h < 9999) ownExtraWait = std::min(ownExtraWait, h);
          }
          for (const auto &w : S.warriors) {
            if (w.id.side != M.my_side || w.state != WState::MOVING ||
                !reclaimAssault.isCommitted(w.id.num) ||
                w.region == currentTarget)
              continue;
            int h = hops(w.region, stagingPoint);
            if (h < 9999) ownExtraWait = std::min(ownExtraWait, h);
          }

          // 현 병력으로 한 명을 더 채울 수 없다면, 현재 골드와 순수입으로
          // 신병 한 명을 만들고 HQ에서 집결지까지 보내는 가장 빠른 때를
          // 계산한다. 훈련한 턴에는 이동할 수 없으므로 1턴을 더한다.
          int hqToStaging = hops(M.my_hq, stagingPoint);
          if (hqToStaging < 9999) {
            long long projectedGold = std::max(0, attackGold);
            for (int day = 0; day <= horizon; ++day) {
              if (projectedGold >= TRAIN_COST) {
                ownExtraWait = std::min(
                    ownExtraWait, day + 1 + hqToStaging);
                break;
              }
              projectedGold = std::max<long long>(
                  0, projectedGold + current_net_income);
            }
          }

          int chargeTravel = hops(stagingPoint, currentTarget);
          int immediateCaptureDay = NEVER;
          int delayedCaptureDay = NEVER;
          if (chargeTravel < 9999 && ownExtraWait <= horizon) {
            int chargeArrival = std::max(0, chargeTravel - 1);
            auto captureDayForWave = [&](int sendCount, int wait) {
              std::vector<StreamArrival> atkArrivals;
              std::vector<StreamArrival> defArrivals;
              int nextNum = 7000000;
              for (int hp : fixedAtTarget)
                atkArrivals.push_back({0, hp, nextNum++});
              for (int i = 0; i < sendCount; ++i) {
                int hp = i < (int)stagedHps.size()
                             ? stagedHps[i]
                             : chargeWarriorHp;
                atkArrivals.push_back(
                    {wait + chargeArrival, hp, nextNum++});
              }
              for (const auto &w : chargeGarrison)
                defArrivals.push_back({0, w.hp, w.num});
              StreamCombatForecast forecast = simulate_reinforcement_stream(
                  chargeBuildingHp, chargeTurret, chargeHasBuilding,
                  std::move(atkArrivals), std::move(defArrivals), horizon);
              return forecast.captured ? forecast.captureDay : NEVER;
            };
            immediateCaptureDay = captureDayForWave(plusOneCount, 0);
            delayedCaptureDay = captureDayForWave(
                plusTwoCount, ownExtraWait);
          }

          // 주어진 완료일까지 상대 HQ에서 새로 생산해 목표에 도착시킬 수
          // 있는 최대 병력 수. +2 쪽 완료일까지의 수가 +1 쪽보다 늘면,
          // 기다린 한 명을 상대도 그대로 맞춰낼 수 있다고 본다.
          auto projectedOpponentArrivalsBy = [&](int deadline) {
            if (deadline == NEVER || deadline < 0) return 0;
            int oppTravel = hops(M.opp_hq, currentTarget);
            if (oppTravel >= 9999) return 0;
            int arrivalOffset = 1 + std::max(0, oppTravel - 1);
            int latestTrainDay = deadline - arrivalOffset;
            if (latestTrainDay < 0) return 0;

            bool freeDefenseMove =
                chargeTarget != nullptr &&
                chargeTarget->side != M.my_side;
            int perUnitCost = TRAIN_COST +
                              (freeDefenseMove ? 0 : MOVE_COST);
            long long projectedGold = std::max(0, S.opp_gold);
            long long projectedIncome = oppNetIncome;
            int produced = 0;
            int cap = std::max(1, HQ_LEVELS[oppHqLevel].train_cap);
            for (int day = 0; day <= latestTrainDay; ++day) {
              for (int k = 0;
                   k < cap && projectedGold >= perUnitCost; ++k) {
                projectedGold -= perUnitCost;
                projectedIncome -= UPKEEP_PER_WARRIOR;
                ++produced;
              }
              projectedGold = std::max<long long>(
                  0, projectedGold + projectedIncome);
            }
            return produced;
          };

          int oppByPlusOne = projectedOpponentArrivalsBy(
              immediateCaptureDay);
          int oppByPlusTwo = projectedOpponentArrivalsBy(
              delayedCaptureDay);
          bool waitIsUsable = ownExtraWait <= horizon &&
                              delayedCaptureDay != NEVER;
          bool enemyMatchesExtra = !waitIsUsable ||
                                   oppByPlusTwo > oppByPlusOne;

          if (!enemyMatchesExtra) {
            // 상대 생산 증가 없이 우리만 한 명 늘어나는 경우에만 +2를
            // 기다린다. 실제 이동/훈련 지시는 앞에서 계산한 +2 집결 계획을
            // 그대로 사용하고, 여기서는 조기 +1 출격만 막는다.
            launchPlan = marginTwoLaunchPlan;
            effectiveLaunchSafetyMargin = std::max(
                effectiveLaunchSafetyMargin,
                MANDATORY_RECLAIM_AHEAD_MARGIN);
            initialChargeCount = plusTwoCount;
            neededExtra = std::max(neededExtra, plusTwoCount);
          }
          dbg::note(turn, "RECLAIM_MARGIN2_TIMING action=" +
                              std::string(enemyMatchesExtra
                                              ? "PLUS1_NOW"
                                              : "WAIT_MARGIN2") +
                              " plus1=" + std::to_string(plusOneCount) +
                              " plus2=" + std::to_string(plusTwoCount) +
                              " wait=" +
                              (ownExtraWait == std::numeric_limits<int>::max()
                                   ? std::string("INF")
                                   : std::to_string(ownExtraWait)) +
                              " cap1=" +
                              std::to_string(immediateCaptureDay) +
                              " cap2=" +
                              std::to_string(delayedCaptureDay) +
                              " opp1=" + std::to_string(oppByPlusOne) +
                              " opp2=" + std::to_string(oppByPlusTwo));
        }
      }
#endif
#if MANDATORY_MARGIN_TWO_AT_LAUNCH && !MANDATORY_MARGIN_TWO_UNMATCHED_ONLY
      // 대조 실험용: BASE 수가 앞서면 상대의 추가 생산 가능 여부와
      // 무관하게 출격 직전까지 항상 +2 마진을 유지한다.
      if (territoryReclaimAttack && !territoryReclaimIsExpansion) {
        int myBaseCount = 0, oppBaseCount = 0;
        for (const auto &base : S.buildings) {
          if (base.type != BType::BASE) continue;
          if (base.side == M.my_side)
            ++myBaseCount;
          else
            ++oppBaseCount;
        }
        if (myBaseCount > oppBaseCount) {
          effectiveLaunchSafetyMargin = std::max(
              effectiveLaunchSafetyMargin, MANDATORY_RECLAIM_AHEAD_MARGIN);
          launchPlan = plan_attack_force(
              chargeBuildingHp, chargeTurret,
              chargeGarrison, fixedAtTarget, stagedHps, chargeWarriorHp, 0,
              std::max(launchSafetyMargin, MANDATORY_RECLAIM_AHEAD_MARGIN),
              chargeHasBuilding);
        }
      }
#endif
#if SNAPSHOT_MANDATORY_MOVERS_AT_LAUNCH && !defined(UNIFIED_RECLAIM_STREAM)
      // 필수 탈환의 정적 계획이 실제로 출격 가능한 최초 순간에만, 지금
      // 관측되는 목표 방향 이동군을 실제 도착일에 넣어 필요 파동을 한 번
      // 보정한다. 이후 새 생산/이동을 매 턴 다시 얹으면 필요 인원이 계속
      // 올라 출격이 무한히 밀리므로, 이 스냅샷의 결과는 해당 파동이
      // 출발하거나 작전 상태가 reset될 때까지 고정한다.
      bool usedMandatoryMoverSnapshot = false;
      if (territoryReclaimAttack && !territoryReclaimIsExpansion &&
          currentTarget != M.opp_hq) {
        usedMandatoryMoverSnapshot = true;
        // 한 명을 더 기다리는 동안 상대도 계속 증원하면 오히려 불리하다.
        // 정적 파동이 이미 준비된 순간에는 집결지 건물의 노동 예약 인원도
        // 추가 파동에 필요한 만큼만 즉시 빌릴 수 있게 한다. 다른 거점의
        // 노동자는 건드리지 않아 전역 경제를 무너뜨리지 않는다.
        std::vector<const Warrior *> snapshotCandidates = attackCandidates;
        for (const auto &w : S.warriors) {
          if (w.id.side != M.my_side || w.hp <= 0 ||
              w.state != WState::STATIONARY ||
              w.region != stagingPoint)
            continue;
          bool alreadyIncluded = false;
          for (const Warrior *candidate : snapshotCandidates)
            if (candidate->id.num == w.id.num) {
              alreadyIncluded = true;
              break;
            }
          if (!alreadyIncluded) snapshotCandidates.push_back(&w);
        }
        std::sort(snapshotCandidates.begin(), snapshotCandidates.end(),
                  [&](const Warrior *x, const Warrior *y) {
                    bool xs = x->region == stagingPoint;
                    bool ys = y->region == stagingPoint;
                    if (xs != ys) return xs > ys;
                    if (x->hp != y->hp) return x->hp > y->hp;
                    return x->id.num < y->id.num;
                  });
        std::vector<const Warrior *> snapshotStagedCandidates;
        for (const Warrior *w : snapshotCandidates)
          if (w->region == stagingPoint)
            snapshotStagedCandidates.push_back(w);
        int borrowedStagingWorkers = std::max(
            0, (int)snapshotStagedCandidates.size() -
                   (int)stagedCandidates.size());
        int staticRequired = launchPlan.sendCount +
                             std::max(0, launchPlan.extraToTrain);
        bool staticReadyNow = launchPlan.sendCount > 0 &&
                              launchPlan.extraToTrain == 0 &&
                              (int)stagedCandidates.size() >= staticRequired &&
                              attackGold >= staticRequired * MOVE_COST;

        bool refreshMandatoryLaunchSnapshot =
            reclaimAssault.preciseLaunchRequired < 0 && staticReadyNow;
#if SNAPSHOT_MANDATORY_NEAREST_RESERVE_AT_LAUNCH
        // 이전 스냅샷이 요구한 인원이 실제로 다 모였을 때는 곧바로
        // 출발시키기 전에 현재 예비대 위치로 한 번 더 계산한다. 필요
        // 인원이 아직 모이지 않은 매 턴에는 다시 계산하지 않으므로 목표가
        // 계속 흔들리지 않으면서도, 오래된 스냅샷으로 출격하지 않는다.
        if (reclaimAssault.preciseLaunchRequired >= 0 &&
            (int)snapshotStagedCandidates.size() >=
                reclaimAssault.preciseLaunchRequired &&
            attackGold >= reclaimAssault.preciseLaunchRequired * MOVE_COST) {
          refreshMandatoryLaunchSnapshot = true;
        }
#endif
        if (refreshMandatoryLaunchSnapshot) {
          std::vector<StreamArrival> observedDefenderArrivals;
          int observedTargetDefenders = 0;
          int observedMovingDefenders = 0;
          int observedReserveDefenders = 0;
          int observedReserveRegion = -1;
          int observedReserveArrival = -1;
          int latestObservedDefenderArrival = 0;
          for (const auto &w : S.warriors) {
            if (w.id.side == M.my_side) continue;
            if (w.region == currentTarget) {
              observedDefenderArrivals.push_back({0, w.hp, w.id.num});
              ++observedTargetDefenders;
              continue;
            }
            if (w.state != WState::MOVING || w.prev_region < 0 ||
                w.prev_region == w.region || myCnt[w.region] > 0 ||
                P.nxt[w.prev_region][currentTarget] != w.region)
              continue;
            int h = hops(w.region, currentTarget);
            if (h >= 9999) continue;
            int arrival = std::max(0, h - 1);
            observedDefenderArrivals.push_back(
                {arrival, w.hp, w.id.num});
            ++observedMovingDefenders;
            latestObservedDefenderArrival = std::max(
                latestObservedDefenderArrival, arrival);
          }

#if SNAPSHOT_MANDATORY_NEAREST_RESERVE_AT_LAUNCH
          // 실제 출격 명령을 본 다음 턴부터 반응할 수 있는 상대 정지
          // 예비대도 빠뜨리지 않는다. 모든 병력을 순간이동시키지 않고,
          // 상대 건물 중 목표까지 가장 가까운 한 지역만 실제 hop만큼 늦게
          // 합류시킨다. 출격 턴에는 아직 움직이지 않으므로 reserveH-1이
          // 아니라 reserveH가 올바른 도착일이다.
          int reserveH = std::numeric_limits<int>::max();
          double reserveD = std::numeric_limits<double>::infinity();
          for (const auto &base : S.buildings) {
            if (base.side == M.my_side || base.region == currentTarget)
              continue;
            bool hasStationary = false;
            for (const auto &w : S.warriors) {
              if (w.id.side != M.my_side && w.region == base.region &&
                  w.state == WState::STATIONARY) {
                hasStationary = true;
                break;
              }
            }
            if (!hasStationary) continue;
            int h = hops(base.region, currentTarget);
            double d = P.dist[base.region][currentTarget];
            if (h < reserveH || (h == reserveH && d < reserveD)) {
              observedReserveRegion = base.region;
              reserveH = h;
              reserveD = d;
            }
          }
          if (observedReserveRegion != -1 && reserveH < 9999) {
            observedReserveArrival = reserveH;
            for (const auto &w : S.warriors) {
              if (w.id.side == M.my_side ||
                  w.region != observedReserveRegion ||
                  w.state != WState::STATIONARY)
                continue;
              observedDefenderArrivals.push_back(
                  {observedReserveArrival, w.hp, w.id.num});
              ++observedReserveDefenders;
            }
            latestObservedDefenderArrival = std::max(
                latestObservedDefenderArrival, observedReserveArrival);
          }
#endif

          std::sort(snapshotStagedCandidates.begin(),
                    snapshotStagedCandidates.end(),
                    [](const Warrior *x, const Warrior *y) {
                      if (x->hp != y->hp) return x->hp > y->hp;
                      return x->id.num < y->id.num;
                    });
          int chargeTravel = hops(stagingPoint, currentTarget);
          int chargeArrival = chargeTravel >= 9999
                                  ? 9999
                                  : std::max(0, chargeTravel - 1);
          int horizon = std::max(0, MAX_TURN - turn - 1);
          auto forecastSnapshotCount = [&](int chargeCount) {
            std::vector<StreamArrival> attackerArrivals;
            int nextNum = 7300000;
            for (const auto &w : S.warriors)
              if (w.id.side == M.my_side && w.region == currentTarget)
                attackerArrivals.push_back({0, w.hp, w.id.num});
            for (int i = 0; i < chargeCount && chargeArrival < 9999; ++i) {
              int hp = i < (int)snapshotStagedCandidates.size()
                           ? snapshotStagedCandidates[i]->hp
                           : chargeWarriorHp;
              int num = i < (int)snapshotStagedCandidates.size()
                            ? snapshotStagedCandidates[i]->id.num
                            : nextNum++;
              attackerArrivals.push_back({chargeArrival, hp, num});
            }
            return simulate_reinforcement_stream(
                chargeBuildingHp, chargeTurret, chargeHasBuilding,
                std::move(attackerArrivals), observedDefenderArrivals,
                horizon, true);
          };

          int minimumSecureCount = -1;
          if (chargeArrival < 9999) {
            for (int chargeCount = 1; chargeCount <= 40; ++chargeCount) {
              StreamCombatForecast trial =
                  forecastSnapshotCount(chargeCount);
              if (!trial.captured || trial.attackerSurvivors < 1) continue;
              minimumSecureCount = chargeCount;
              break;
            }
          }
          int requiredCount = minimumSecureCount >= 0
                                  ? minimumSecureCount +
                                        effectiveLaunchSafetyMargin
                                  : std::max(staticRequired,
                                             (int)snapshotStagedCandidates.size() + 1);
          reclaimAssault.preciseLaunchRequired =
              std::min(40, requiredCount);
          reclaimAssault.preciseLaunchSnapshotTurn = turn;
          StreamCombatForecast requiredForecast = forecastSnapshotCount(
              reclaimAssault.preciseLaunchRequired);
          dbg::note(turn, "RECLAIM_MOVER_SNAPSHOT target=R" +
                              std::to_string(currentTarget) +
                              " static=" + std::to_string(staticRequired) +
                              " minimum=" +
                              std::to_string(minimumSecureCount) +
                              " margin=" +
                              std::to_string(effectiveLaunchSafetyMargin) +
                              " required=" +
                              std::to_string(
                                  reclaimAssault.preciseLaunchRequired) +
                              " borrowed=" +
                              std::to_string(borrowedStagingWorkers) +
                              " target_def=" +
                              std::to_string(observedTargetDefenders) +
                              " moving_def=" +
                              std::to_string(observedMovingDefenders) +
                              " reserve_def=" +
                              std::to_string(observedReserveDefenders) +
                              " reserve_r=" +
                              std::to_string(observedReserveRegion) +
                              " reserve_arrival=" +
                              std::to_string(observedReserveArrival) +
                              " last_arrival=" +
                              std::to_string(latestObservedDefenderArrival) +
                              " secure_day=" +
                              std::to_string(requiredForecast.captureDay) +
                              " survivors=" +
                              std::to_string(requiredForecast.attackerSurvivors));
        }

        if (reclaimAssault.preciseLaunchRequired >= 0) {
          int requiredCount = reclaimAssault.preciseLaunchRequired;
          initialChargeCount = requiredCount;
          neededExtra = requiredCount;
          extraToTrain = std::max(
              0, requiredCount - (int)snapshotCandidates.size());
          candidates.assign(
              snapshotCandidates.begin(),
              snapshotCandidates.begin() +
                  std::min(requiredCount, (int)snapshotCandidates.size()));
          readyAtStaging = (int)snapshotStagedCandidates.size();
          readyToCharge = readyAtStaging >= requiredCount &&
                          extraToTrain == 0 &&
                          attackGold >= requiredCount * MOVE_COST;
          if (readyToCharge) {
            std::sort(snapshotStagedCandidates.begin(),
                      snapshotStagedCandidates.end(),
                      [](const Warrior *x, const Warrior *y) {
                        if (x->hp != y->hp) return x->hp > y->hp;
                        return x->id.num < y->id.num;
                      });
            candidates.assign(snapshotStagedCandidates.begin(),
                              snapshotStagedCandidates.begin() + requiredCount);
          }
          dbg::note(turn, "RECLAIM_MOVER_SNAPSHOT_STATUS target=R" +
                              std::to_string(currentTarget) +
                              " snapshot_turn=" +
                              std::to_string(
                                  reclaimAssault.preciseLaunchSnapshotTurn) +
                              " ready=" +
                              std::to_string(readyAtStaging) + "/" +
                              std::to_string(requiredCount) +
                              " train=" + std::to_string(extraToTrain));
        }
      }
#endif
#if PRECISE_MANDATORY_RECLAIM_AT_LAUNCH && !defined(UNIFIED_RECLAIM_STREAM)
      // 필수 거점은 목표 자체를 바꾸지 않되, 첫 파동이 실제로 출발하기
      // 직전에 현재 주둔군과 이미 목표 방향으로 움직이는 상대 병력을 실제
      // 도착일에 합류시켜 최소 파동을 다시 계산한다. 상대 정지 예비대나
      // 미래 생산은 확정 수비로 당겨 넣지 않고 출격 후 롤아웃에 맡긴다.
      bool usedPreciseMandatoryGate = false;
      if (territoryReclaimAttack && !territoryReclaimIsExpansion &&
          currentTarget != M.opp_hq) {
        usedPreciseMandatoryGate = true;
        std::vector<StreamArrival> observedDefenderArrivals;
        int observedTargetDefenders = 0;
        int observedMovingDefenders = 0;
        int latestObservedDefenderArrival = 0;
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side) continue;
          if (w.region == currentTarget) {
            observedDefenderArrivals.push_back({0, w.hp, w.id.num});
            ++observedTargetDefenders;
            continue;
          }
          if (w.state != WState::MOVING || w.prev_region < 0 ||
              w.prev_region == w.region ||
              P.nxt[w.prev_region][currentTarget] != w.region)
            continue;
          int h = hops(w.region, currentTarget);
          if (h >= 9999) continue;
          int arrival = std::max(0, h - 1);
          observedDefenderArrivals.push_back(
              {arrival, w.hp, w.id.num});
          ++observedMovingDefenders;
          latestObservedDefenderArrival = std::max(
              latestObservedDefenderArrival, arrival);
        }

        std::sort(stagedCandidates.begin(), stagedCandidates.end(),
                  [](const Warrior *x, const Warrior *y) {
                    if (x->hp != y->hp) return x->hp > y->hp;
                    return x->id.num < y->id.num;
                  });
        int chargeTravel = hops(stagingPoint, currentTarget);
        int chargeArrival = chargeTravel >= 9999
                                ? 9999
                                : std::max(0, chargeTravel - 1);
        int preciseHorizon = std::max(0, MAX_TURN - turn - 1);
        auto forecastMandatoryCount = [&](int chargeCount) {
          std::vector<StreamArrival> attackerArrivals;
          int nextNum = 7200000;
          for (const auto &w : S.warriors)
            if (w.id.side == M.my_side &&
                w.region == currentTarget)
              attackerArrivals.push_back({0, w.hp, w.id.num});
          for (int i = 0; i < chargeCount && chargeArrival < 9999; ++i) {
            int hp = i < (int)stagedCandidates.size()
                         ? stagedCandidates[i]->hp
                         : chargeWarriorHp;
            int num = i < (int)stagedCandidates.size()
                          ? stagedCandidates[i]->id.num
                          : nextNum++;
            attackerArrivals.push_back({chargeArrival, hp, num});
          }
          return simulate_reinforcement_stream(
              chargeBuildingHp, chargeTurret, chargeHasBuilding,
              std::move(attackerArrivals), observedDefenderArrivals,
              preciseHorizon, true);
        };

        int minimumSecureCount = -1;
        StreamCombatForecast minimumForecast;
        if (chargeArrival < 9999) {
          for (int chargeCount = 1; chargeCount <= 40; ++chargeCount) {
            StreamCombatForecast trial =
                forecastMandatoryCount(chargeCount);
            if (!trial.captured || trial.attackerSurvivors < 1) continue;
            minimumSecureCount = chargeCount;
            minimumForecast = trial;
            break;
          }
        }

        int requiredCount;
        if (minimumSecureCount >= 0) {
          requiredCount = std::min(
              40, minimumSecureCount + effectiveLaunchSafetyMargin);
        } else {
          int oldRequired = launchPlan.sendCount +
                            std::max(0, launchPlan.extraToTrain);
          requiredCount = std::min(
              40, std::max(oldRequired,
                           (int)stagedCandidates.size() + 1));
        }
        StreamCombatForecast requiredForecast =
            forecastMandatoryCount(requiredCount);
        initialChargeCount = requiredCount;
        neededExtra = requiredCount;
        extraToTrain = std::max(
            0, requiredCount - (int)attackCandidates.size());
        candidates.assign(
            attackCandidates.begin(),
            attackCandidates.begin() +
                std::min(requiredCount, (int)attackCandidates.size()));
        readyToCharge = minimumSecureCount >= 0 &&
                        requiredForecast.captured &&
                        requiredForecast.attackerSurvivors >= 1 &&
                        (int)stagedCandidates.size() >= requiredCount &&
                        extraToTrain == 0 &&
                        attackGold >= requiredCount * MOVE_COST;
        if (readyToCharge) {
          candidates.assign(
              stagedCandidates.begin(),
              stagedCandidates.begin() + requiredCount);
        }
        dbg::note(turn, "RECLAIM_PRECISE_GATE result=" +
                            std::string(requiredForecast.captured
                                            ? "SECURE"
                                            : "FAIL") +
                            " target=R" +
                            std::to_string(currentTarget) +
                            " minimum=" +
                            std::to_string(minimumSecureCount) +
                            " margin=" +
                            std::to_string(effectiveLaunchSafetyMargin) +
                            " required=" +
                            std::to_string(requiredCount) +
                            " ready=" +
                            std::to_string(stagedCandidates.size()) + "/" +
                            std::to_string(requiredCount) +
                            " target_def=" +
                            std::to_string(observedTargetDefenders) +
                            " moving_def=" +
                            std::to_string(observedMovingDefenders) +
                            " last_arrival=" +
                            std::to_string(latestObservedDefenderArrival) +
                            " secure_day=" +
                            std::to_string(requiredForecast.captureDay) +
                            " survivors=" +
                            std::to_string(requiredForecast.attackerSurvivors));
      }
#endif
#if BASE_REINFORCEMENT_RACE_LOOKUP
      // 실제 병력별 현재 HP로, 상대의 가장 빠른 증원이 합류하기 전에 BASE를
      // 부술 수 있는 파동을 찾는다. 적 주둔군 훈련비+기지 투자비에서 우리
      // 전사자 훈련비와 출격 이동비를 뺀 즉시 가치가 양수일 때만 단기 타격
      // 작전으로 분류한다. 이득이 아니면 기존의 지속 점령 공세를 그대로 쓴다.
      bool baseRaceLookupApplied = false;
      int baseRaceRequiredCount = -1;
      int baseRaceCombatTurns = -1;
      int baseRaceSurvivors = 0;
      int baseRaceOwnArrival = -1;
      int baseRaceEnemyArrival = std::numeric_limits<int>::max();
      int baseRaceImmediateNet = 0;
      std::vector<const Warrior *> baseRaceCandidates = stagedCandidates;
#if BASE_RACE_BORROW_STAGING_WORKER
      // 평상시 공격 후보는 거점의 work_cap만큼 노동자를 남긴다. 하지만
      // 이미 집결지에 있는 그 한 명까지 보태야 실제 이동 중인 증원보다
      // 먼저 적 BASE를 깨고 양수 교환을 만들 수 있다면, 새 병력을 기다리는
      // 것보다 지금 함께 보내는 편이 낫다. 다른 거점 노동자나 두 명 이상은
      // 빌리지 않아 이 예외가 전역 경제를 무너뜨리지 않게 한다.
      const Warrior *borrowedStagingWorker = nullptr;
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side || w.hp <= 0 ||
            w.state != WState::STATIONARY || w.region != stagingPoint)
          continue;
        bool alreadyIncluded = false;
        for (const Warrior *candidate : baseRaceCandidates)
          if (candidate->id.num == w.id.num) {
            alreadyIncluded = true;
            break;
          }
        if (alreadyIncluded) continue;
        if (borrowedStagingWorker == nullptr ||
            w.hp > borrowedStagingWorker->hp ||
            (w.hp == borrowedStagingWorker->hp &&
             w.id.num < borrowedStagingWorker->id.num))
          borrowedStagingWorker = &w;
      }
      if (borrowedStagingWorker != nullptr)
        baseRaceCandidates.push_back(borrowedStagingWorker);
      std::sort(baseRaceCandidates.begin(), baseRaceCandidates.end(),
                [](const Warrior *x, const Warrior *y) {
                  if (x->hp != y->hp) return x->hp > y->hp;
                  return x->id.num < y->id.num;
                });
#endif
      std::vector<int> baseRaceHps;
      baseRaceHps.reserve(baseRaceCandidates.size());
      for (const Warrior *w : baseRaceCandidates)
        baseRaceHps.push_back(w->hp);

      bool eligibleBaseRace = territoryReclaimAttack &&
                              chargeTarget != nullptr &&
                              chargeTarget->side != M.my_side &&
                              chargeTarget->type == BType::BASE &&
                              chargeTarget->level >= 1 &&
                              chargeTarget->level <= 3 &&
                              fixedAtTarget.empty() &&
                              !baseRaceHps.empty();

      if (eligibleBaseRace) {
        int ownTravel = hops(stagingPoint, currentTarget);
        if (ownTravel < 9999)
          baseRaceOwnArrival = std::max(0, ownTravel - 1);

        // 이미 목표 방향으로 이동 중인 상대 병력은 현재 위치에서 남은
        // hop만큼 뒤에 합류한다. 명령 당일 한 칸을 이미 이동했으므로 h-1.
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side || w.region == currentTarget ||
              w.state != WState::MOVING || w.prev_region < 0 ||
              w.prev_region == w.region ||
              P.nxt[w.prev_region][currentTarget] != w.region)
            continue;
          int h = hops(w.region, currentTarget);
          if (h < 9999)
            baseRaceEnemyArrival = std::min(
                baseRaceEnemyArrival, std::max(0, h - 1));
        }

        // 상대 건물의 정지 병력 중 work_cap을 넘는 전투 예비대는 출격을
        // 본 다음 턴 반응한다고 본다. 반응 1턴과 첫 이동이 상쇄되어 현재
        // 건물에서 목표까지의 h가 그대로 합류일이다.
        for (const auto &eb : S.buildings) {
          if (eb.side == M.my_side || eb.region == currentTarget) continue;
          std::vector<const Warrior *> stationed;
          for (const auto &w : S.warriors)
            if (w.id.side != M.my_side &&
                w.region == eb.region &&
                w.state == WState::STATIONARY)
              stationed.push_back(&w);
          if ((int)stationed.size() <= eb.work_cap()) continue;
          int h = hops(eb.region, currentTarget);
          if (h < 9999)
            baseRaceEnemyArrival = std::min(baseRaceEnemyArrival, h);
        }

        // 건물이 없는 지역의 정지 야전 병력도 같은 방식으로 다음 턴
        // 반응한다. 현재 아군과 교전 중인 병력은 빠져나올 수 없으므로 제외.
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side || w.region == currentTarget ||
              w.state != WState::STATIONARY ||
              bld[w.region] != nullptr || myCnt[w.region] > 0)
            continue;
          int h = hops(w.region, currentTarget);
          if (h < 9999)
            baseRaceEnemyArrival = std::min(baseRaceEnemyArrival, h);
        }

#if BASE_RACE_INCLUDE_REACTIVE_HQ_TRAIN
        // 출격 시점에 아직 존재하지 않는 병력도, 상대가 현재 보유 골드나
        // 이후 순수입으로 HQ에서 즉시 생산해 최단 경로로 대응할 수 있으면
        // 치고 빠지기의 실제 마감 시각이 된다. 현재 턴에 훈련한 신병은
        // 다음 턴부터 이동할 수 있으므로 HQ->목표 h hop이 그대로 도착
        // 오프셋이다. 한 명만 합류해도 고립 전투 전제가 깨지므로 가장 빠른
        // 첫 생산만 계산한다.
        int reactiveTrainWait = -1;
        long long projectedOppGold = std::max(0, S.opp_gold);
        int remainingRaceTurns = std::max(0, MAX_TURN - turn);
        for (int wait = 0; wait <= remainingRaceTurns; ++wait) {
          if (projectedOppGold >= TRAIN_COST) {
            reactiveTrainWait = wait;
            break;
          }
          projectedOppGold = std::max<long long>(
              0, projectedOppGold + oppNetIncome);
        }
        int oppHqTravel = hops(M.opp_hq, currentTarget);
        if (reactiveTrainWait >= 0 && oppHqTravel < 9999)
          baseRaceEnemyArrival = std::min(
              baseRaceEnemyArrival, reactiveTrainWait + oppHqTravel);
#endif

        // destroy absolute day = ownArrival + combatTurns - 1. 상대와 같은 날
        // 도착하는 경우는 증원 전 파괴가 아니므로 허용하지 않는다. 증원이
        // 전혀 없으면 남은 경기 시간까지만 전투 창으로 사용한다.
        if (baseRaceOwnArrival >= 0 &&
            (baseRaceEnemyArrival == std::numeric_limits<int>::max() ||
             baseRaceEnemyArrival > baseRaceOwnArrival)) {
          int combatWindow =
              baseRaceEnemyArrival == std::numeric_limits<int>::max()
                  ? std::max(0, MAX_TURN - turn - baseRaceOwnArrival)
                  : baseRaceEnemyArrival - baseRaceOwnArrival;
          int buildingInvestment = 0;
          for (int level = 1; level <= chargeTarget->level; ++level)
            buildingInvestment += BASE_LEVELS[level].cost;
          std::vector<int> selectedHps;
          for (int sendCount = 1;
               sendCount <= (int)baseRaceHps.size(); ++sendCount) {
            selectedHps.push_back(baseRaceHps[sendCount - 1]);
            IsolatedBaseCombatOutcome outcome =
                simulate_isolated_base_combat(
                    chargeBuildingHp, chargeTurret, selectedHps,
                    chargeGarrison, combatWindow);
            if (!outcome.destroyed || outcome.attackerSurvivors < 1)
              continue;
            int ownLosses = sendCount - outcome.attackerSurvivors;
            int immediateNet =
                (int)chargeGarrison.size() * TRAIN_COST +
                buildingInvestment - ownLosses * TRAIN_COST -
                sendCount * MOVE_COST;
            if (immediateNet <= 0) continue;
            bool better = !baseRaceLookupApplied ||
                          immediateNet > baseRaceImmediateNet ||
                          (immediateNet == baseRaceImmediateNet &&
                           sendCount < baseRaceRequiredCount);
            if (!better) continue;
            baseRaceLookupApplied = true;
            baseRaceRequiredCount = sendCount;
            baseRaceCombatTurns = outcome.combatTurns;
            baseRaceSurvivors = outcome.attackerSurvivors;
            baseRaceImmediateNet = immediateNet;
          }
        }
      }
#endif
#ifdef UNIFIED_RECLAIM_STREAM
      bool usedUnifiedGate = currentTarget != M.opp_hq;
      if (usedUnifiedGate) {
        // 정적 최소치(+1)부터 시작해, 현재 집결 인원과 같은 위치에 추가
        // 병력이 함께 모였다고 가정했을 때 통합 시간축에서 이기는 최소
        // 파동을 찾는다. 출격 전에는 양쪽 모두 미래 생산을 넣지 않고 현재
        // 존재하는 주둔군/이동군/노동 초과 예비대만 같은 입력 생성기로 본다.
        int startCount = std::max(1, launchPlan.sendCount);
        int maxCount = std::min(
            40, std::max(startCount,
                         (int)attackCandidates.size() + 12));
        int requiredCount = -1;
        StreamCombatForecast gateForecast;
        UnifiedReclaimStreamStats gateStats;
        for (int chargeCount = startCount; chargeCount <= maxCount;
             ++chargeCount) {
          int realCount = std::min(chargeCount,
                                   (int)stagedCandidates.size());
          std::vector<const Warrior *> selected(
              stagedCandidates.begin(), stagedCandidates.begin() + realCount);
          int hypothetical = chargeCount - realCount;
          UnifiedReclaimStreamStats trialStats;
          StreamCombatForecast trial = forecast_unified_reclaim_stream(
              S, M, P, turn, currentTarget, reclaimAssault.committed,
              selected, stagingPoint, hypothetical, chargeWarriorHp,
              HQ_LEVELS[oppHqLevel].warrior_hp,
              current_net_income, oppNetIncome, false, &trialStats);
          gateForecast = trial;
          gateStats = trialStats;
          if (!trial.captured) continue;
          requiredCount = chargeCount;
          break;
        }

        if (requiredCount < 0) {
          // 현재 탐색 상한으로도 답이 없으면 작은 파동을 내보내지 않는다.
          // 다음 한 명을 더 모으도록 해 다음 턴 새 관측과 함께 재평가한다.
          requiredCount = std::min(
              40, std::max(startCount, (int)stagedCandidates.size() + 1));
        }
        initialChargeCount = requiredCount;
        neededExtra = requiredCount;
        extraToTrain = std::max(
            0, requiredCount - (int)attackCandidates.size());
        candidates.assign(
            attackCandidates.begin(),
            attackCandidates.begin() +
                std::min(requiredCount, (int)attackCandidates.size()));
        readyToCharge = gateForecast.captured &&
                        (int)stagedCandidates.size() >= requiredCount &&
                        extraToTrain == 0 &&
                        attackGold >= requiredCount * MOVE_COST;
        dbg::note(turn, "RECLAIM_UNIFIED_GATE result=" +
                            std::string(gateForecast.captured
                                            ? "CAPTURE"
                                            : "FAIL") +
                            " day=" +
                            std::to_string(gateForecast.captureDay) +
                            " ready=" +
                            std::to_string(stagedCandidates.size()) + "/" +
                            std::to_string(requiredCount) +
                            " target_def=" +
                            std::to_string(gateStats.targetDefenders) +
                            " moving_def=" +
                            std::to_string(gateStats.movingDefenders) +
                            " reserve_def=" +
                            std::to_string(gateStats.reserveDefenders) +
                            " fortify=" +
                            std::to_string(gateStats.predictedFortify ? 1 : 0) +
                            " fortify_income=" +
                            std::to_string(gateStats.fortifyIncome) +
                            " horizon=" +
                            std::to_string(gateStats.horizon));
      }
      if (!usedUnifiedGate && launchPlan.extraToTrain == 0 &&
          launchPlan.sendCount > 0) {
#else
      if (
#if SNAPSHOT_MANDATORY_MOVERS_AT_LAUNCH
          !usedMandatoryMoverSnapshot &&
#endif
#if PRECISE_MANDATORY_RECLAIM_AT_LAUNCH
          !usedPreciseMandatoryGate &&
#endif
          launchPlan.extraToTrain == 0 && launchPlan.sendCount > 0) {
#endif
        initialChargeCount = launchPlan.sendCount;
        readyToCharge = attackGold >= initialChargeCount * MOVE_COST;
        if (readyToCharge) {
          candidates.clear();
          for (const Warrior *w : attackCandidates) {
            if (w->region != stagingPoint) continue;
            candidates.push_back(w);
            if ((int)candidates.size() >= initialChargeCount) break;
          }
        }
      }
#if BASE_REINFORCEMENT_RACE_LOOKUP
      if (baseRaceLookupApplied) {
        // 고정 +1/+2 마진 대신 "실제 증원 도착 전 파괴 + 1명 이상 생존"을
        // 만족하는 최소 파동으로 최종 출격 인원을 덮어쓴다.
        reclaimAssault.baseRaceStrike = true;
        reclaimAssault.postBreachHold = false;
        reclaimAssault.baseRaceImmediateNet = baseRaceImmediateNet;
        initialChargeCount = baseRaceRequiredCount;
        neededExtra = baseRaceRequiredCount;
        extraToTrain = 0;
        readyToCharge =
            attackGold >= baseRaceRequiredCount * MOVE_COST &&
            (int)baseRaceCandidates.size() >= baseRaceRequiredCount;
        candidates.assign(
            baseRaceCandidates.begin(),
            baseRaceCandidates.begin() + baseRaceRequiredCount);
        dbg::note(turn, "RECLAIM_BASE_RACE action=LAUNCH target=R" +
                            std::to_string(currentTarget) + " send=" +
                            std::to_string(baseRaceRequiredCount) +
                            " combat_turns=" +
                            std::to_string(baseRaceCombatTurns) +
                            " own_arrival=" +
                            std::to_string(baseRaceOwnArrival) +
                            " enemy_arrival=" +
                            std::to_string(baseRaceEnemyArrival) +
                            " survivors=" +
                            std::to_string(baseRaceSurvivors) +
                            " immediate_net=" +
                            std::to_string(baseRaceImmediateNet) +
                            " borrowed_worker=" +
                            std::to_string(
                                (int)baseRaceCandidates.size() -
                                (int)stagedCandidates.size()));
      } else if (eligibleBaseRace) {
        dbg::note(turn, "RECLAIM_BASE_RACE action=FALLBACK target=R" +
                            std::to_string(currentTarget) +
                            " own_arrival=" +
                            std::to_string(baseRaceOwnArrival) +
                            " enemy_arrival=" +
                            (baseRaceEnemyArrival ==
                                     std::numeric_limits<int>::max()
                                 ? std::string("INF")
                                 : std::to_string(baseRaceEnemyArrival)));
      }
#endif
    } else if (synchronizedMandatoryRetry) {
      // 첫 공격이 실패했다는 것은 현재 스냅샷 기반 최소 병력이 상대의
      // 증원/도착 시차를 과소평가했다는 실제 증거다. 같은 작은 파동을 즉시
      // 반복하지 않고, 이번 계획 인원과 이동 중 증원이 전부 집결한 뒤
      // 한꺼번에 재출격한다. 목표는 그대로 유지하며 영구 제외하지 않는다.
      bool canPayRetry = attackGold >= neededExtra * MOVE_COST;
      readyToCharge = stagingPoint != -1 &&
                      readyAtStaging >= neededExtra &&
                      extraToTrain == 0 && canPayRetry &&
                      !reclaimAssemblyInTransit;
      dbg::note(turn, "RECLAIM_RETRY_SYNC ready=" +
                          std::to_string(readyAtStaging) + "/" +
                          std::to_string(neededExtra) + " train=" +
                          std::to_string(extraToTrain) + " transit=" +
                          std::to_string(reclaimAssemblyInTransit ? 1 : 0));
    } else if (!territoryReclaimAttack) {
      readyToCharge = stagingPoint != -1 && readyAtStaging >= neededExtra;
    }

#if PRECISE_OPTIONAL_PUSH_AT_LAUNCH
    // 실제 출격 가능한 순간에는 단순 현재 주둔군 정적 전투가 아니라,
    // 양쪽 병력의 실제 위치와 도착일을 넣은 시간축 전투로 모든 목표를
    // 비교한다. 상대가 다른 아군 거점을 향해 이동 중이면 그 병력은 목표
    // 수비에 순간이동시키지 않는다. 반대로 상대 건물로 복귀 중인 병력은
    // 도착 후 일자리 인원을 채우고 남는 실제 전투 병력만 반응시킨다.
    bool preciseOptionalPushLaunch = (readyToCharge
#if PROFITABLE_PRE_REINFORCEMENT_RAID
                                      || attackGold >= MOVE_COST
#endif
                                      ) &&
                                     territoryReclaimAttack &&
                                     territoryReclaimIsExpansion &&
                                     !territoryReinforcement &&
                                     !reclaimAssault.launched;
    bool preciseGeneralLaunch = false;
#if PRECISE_ALL_OFFENSE_AT_LAUNCH
    // 일반 총공세도 병력이 실제 집결한 마지막 순간에는 선택 PUSH와 똑같은
    // 시간축 검증을 거친다. 목표 선정 단계의 response/slack 점수는 후보를
    // 고르는 휴리스틱일 뿐이며, 그것만으로 병력을 출발시키지 않는다.
    // 상대 HQ 결전은 위 DIRECT_HQ_FEASIBLE에서 이미 별도 정밀 검증한다.
    preciseGeneralLaunch = readyToCharge && !territoryReclaimAttack &&
                           currentTarget != M.opp_hq;
#endif
    if (preciseOptionalPushLaunch || preciseGeneralLaunch) {
      bool preciseGateWasReady = readyToCharge;
      struct MovingEnemyEndpoint {
        const Warrior *warrior = nullptr;
        int endpoint = -1;
        int arrivalDay = 0;
      };

      // 상대 이동 명령의 최종 목적지는 공개되지 않으므로, 직전->현재
      // 이동 방향과 일치하는 건물 중 현재 위치에서 가장 먼저 만나는 곳을
      // 관측 가능한 목적지로 복원한다. 내 건물로 향하는 병력은 공세 중인
      // 병력이므로 다른 상대 거점 수비대로 복제하지 않는다.
      std::vector<MovingEnemyEndpoint> movingEnemyEndpoints;
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side || w.state != WState::MOVING ||
            w.prev_region < 0 || w.prev_region == w.region)
          continue;
        int endpoint = -1;
        int bestH = std::numeric_limits<int>::max();
        double bestD = std::numeric_limits<double>::infinity();
        for (const auto &eb : S.buildings) {
          if (P.nxt[w.prev_region][eb.region] != w.region) continue;
          int h = hops(w.region, eb.region);
          double d = P.dist[w.region][eb.region];
          if (h < bestH || (h == bestH && d < bestD)) {
            endpoint = eb.region;
            bestH = h;
            bestD = d;
          }
        }
        if (endpoint == -1 || bestH >= 9999) continue;
        movingEnemyEndpoints.push_back(
            {&w, endpoint, std::max(0, bestH - 1)});
      }

      std::vector<const Warrior *> stagedLaunchers;
      for (const Warrior *w : attackCandidates)
        if (w->region == stagingPoint) stagedLaunchers.push_back(w);
      std::sort(stagedLaunchers.begin(), stagedLaunchers.end(),
                [](const Warrior *x, const Warrior *y) {
                  if (x->hp != y->hp) return x->hp > y->hp;
                  return x->id.num < y->id.num;
                });

      int preciseBestTarget = -1;
      int preciseBestCaptureDay = std::numeric_limits<int>::max();
      int preciseBestSend = std::numeric_limits<int>::max();
      int preciseBestSurvivors = -1;
      int preciseBestDefenders = 0;
      double preciseBestDistance = std::numeric_limits<double>::infinity();
      int profitableRaidTarget = -1;
      int profitableRaidSend = -1;
      int profitableRaidCombatTurns = -1;
      int profitableRaidOwnArrival = -1;
      int profitableRaidEnemyArrival = std::numeric_limits<int>::max();
      int profitableRaidSurvivors = 0;
      int profitableRaidImmediateNet = 0;
      int profitableRaidDefenderLosses = 0;
      int profitableRaidBuildingHpRemaining = 0;
      bool profitableRaidDestroyed = false;
      int preciseHorizon = std::max(
          0, std::min(40, MAX_TURN - turn - 1));

      for (int t : M.strongholds) {
        const Building *preciseTargetBuilding = bld[t];
        bool enemyBuilding = preciseTargetBuilding != nullptr &&
                             preciseTargetBuilding->side != M.my_side;
        bool enemyOccupation = preciseTargetBuilding == nullptr &&
                               oppCnt[t] > 0;
        if (!enemyBuilding && !enemyOccupation) continue;

#if BLOCK_OPTIONAL_PUSH_ROUTE_COLLISION
        // 목표 전투 시간축만 보면, 상대가 다른 아군 거점을 공격하러 가는
        // 병력은 목표 수비대에서 제외하는 것이 맞다. 하지만 그 공격 경로와
        // 우리 출격 경로가 같은 날 같은 중간 지역을 지나면 목표에 도착하기
        // 전에 야전 전투가 발생한다. 관측 중인 양쪽 명령을 그대로 한 칸씩
        // 진행시켜 목표 도착 전 확정 충돌이 하나라도 있으면 이 선택 공세
        // 후보를 이번 턴에는 제외한다. 반대 방향 간선 교환은 전투가 없으므로
        // 같은 이동 후 지역에 선 경우만 충돌로 센다.
        int routeCollisionDay = -1;
        int routeCollisionRegion = -1;
        int routeCollisionEnemies = 0;
        int myRoutePos = stagingPoint;
        std::vector<int> enemyRoutePositions;
        enemyRoutePositions.reserve(movingEnemyEndpoints.size());
        for (const auto &me : movingEnemyEndpoints)
          enemyRoutePositions.push_back(me.warrior->region);
        for (int day = 0;
             day <= preciseHorizon && myRoutePos != t; ++day) {
          int nextMy = P.nxt[myRoutePos][t];
          if (nextMy < 0 || nextMy == myRoutePos) break;
          myRoutePos = nextMy;
          for (int i = 0; i < (int)movingEnemyEndpoints.size(); ++i) {
            const auto &me = movingEnemyEndpoints[i];
            if (myCnt[enemyRoutePositions[i]] > 0) continue;
            if (enemyRoutePositions[i] != me.endpoint) {
              int nextEnemy = P.nxt[enemyRoutePositions[i]][me.endpoint];
              if (nextEnemy >= 0)
                enemyRoutePositions[i] = nextEnemy;
            }
          }
          if (myRoutePos == t) break; // 목표 전투는 아래 시간축이 처리한다.
          int colliders = 0;
          for (int i = 0; i < (int)movingEnemyEndpoints.size(); ++i)
            if (enemyRoutePositions[i] == myRoutePos)
              ++colliders;
          if (colliders > 0) {
            routeCollisionDay = day;
            routeCollisionRegion = myRoutePos;
            routeCollisionEnemies = colliders;
            break;
          }
        }
        if (routeCollisionDay >= 0) {
          dbg::note(turn, "PUSH_ROUTE_COLLISION_BLOCK target=R" +
                              std::to_string(t) + " day=" +
                              std::to_string(routeCollisionDay) +
                              " region=R" +
                              std::to_string(routeCollisionRegion) +
                              " enemies=" +
                              std::to_string(routeCollisionEnemies));
          continue;
        }
#endif

        bool targetHasBuilding = enemyBuilding;
        int targetHp = enemyBuilding ? preciseTargetBuilding->hp : 0;
        int targetTurret = 0;
        if (enemyBuilding)
          targetTurret = preciseTargetBuilding->type == BType::HQ
              ? HQ_LEVELS[preciseTargetBuilding->level].turret
              : BASE_LEVELS[preciseTargetBuilding->level].turret;

        std::vector<StreamArrival> defenderArrivals;
        std::vector<int> includedDefenders;
        int earliestExternalDefenderArrival =
            std::numeric_limits<int>::max();
        auto addPreciseDefender = [&](int day, const Warrior &w) {
          if (day > preciseHorizon ||
              std::find(includedDefenders.begin(), includedDefenders.end(),
                        w.id.num) != includedDefenders.end())
            return;
          defenderArrivals.push_back({std::max(0, day), w.hp, w.id.num});
          includedDefenders.push_back(w.id.num);
        };

        // 목표에 있는 병력은 일자리 여부와 무관하게 전부 0일 수비대다.
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side && w.region == t)
            addPreciseDefender(0, w);

        // 이미 이 목표 방향으로 이동 중인 병력은 현재 위치에서 남은 실제
        // hop만큼 뒤에 합류한다. 명령 당일 한 칸을 이미 진행하므로 h-1일.
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side || w.region == t ||
              w.state != WState::MOVING || w.prev_region < 0 ||
              P.nxt[w.prev_region][t] != w.region)
            continue;
          int h = hops(w.region, t);
          if (h < 9999) {
            int arrival = std::max(0, h - 1);
            addPreciseDefender(arrival, w);
            earliestExternalDefenderArrival = std::min(
                earliestExternalDefenderArrival, arrival);
          }
        }

        // 상대 건물별 정지 병력은 work_cap까지 노동자로 남기고, 그 초과분만
        // 출격을 본 다음 턴부터 반응 가능한 전투 예비대로 넣는다. 각자의
        // 현재 위치에서 목표까지 h hop이면 반응 지연 1턴과 명령 당일 첫
        // 이동이 상쇄되어 정확히 h일 뒤 도착한다.
        std::vector<int> stationaryAtRegion(N, 0);
        for (const auto &eb : S.buildings) {
          if (eb.side == M.my_side) continue;
          std::vector<const Warrior *> stationed;
          for (const auto &w : S.warriors)
            if (w.id.side != M.my_side && w.region == eb.region &&
                w.state == WState::STATIONARY)
              stationed.push_back(&w);
          std::sort(stationed.begin(), stationed.end(),
                    [](const Warrior *x, const Warrior *y) {
                      return x->id.num < y->id.num;
                    });
          stationaryAtRegion[eb.region] = (int)stationed.size();
          if (eb.region == t) continue;
          for (int i = eb.work_cap(); i < (int)stationed.size(); ++i) {
            int h = hops(eb.region, t);
            if (h < 9999) {
              addPreciseDefender(h, *stationed[i]);
              earliestExternalDefenderArrival = std::min(
                  earliestExternalDefenderArrival, h);
            }
          }
        }

        // 건물이 아닌 곳에 정지한 야전 병력도 실제 위치에서 반응한다.
        // 현재 아군과 교전 중이면 빠져나올 수 없으므로 제외한다.
        for (const auto &w : S.warriors) {
          if (w.id.side == M.my_side || w.state != WState::STATIONARY ||
              w.region == t || bld[w.region] != nullptr ||
              myCnt[w.region] > 0)
            continue;
          int h = hops(w.region, t);
          if (h < 9999) {
            addPreciseDefender(h, w);
            earliestExternalDefenderArrival = std::min(
                earliestExternalDefenderArrival, h);
          }
        }

        // 상대 소유 건물로 이동 중인 병력은 그 건물 도착 시점에 노동 빈칸을
        // 먼저 채운다. 남는 병력만 다음 턴 새 목표를 받아 실제 위치에서
        // 반응한다. 내 건물을 향하는 공세 병력은 여기 들어오지 않는다.
        for (const auto &eb : S.buildings) {
          if (eb.side == M.my_side || eb.region == t) continue;
          std::vector<MovingEnemyEndpoint> arrivals;
          for (const auto &me : movingEnemyEndpoints)
            if (me.endpoint == eb.region) arrivals.push_back(me);
          std::sort(arrivals.begin(), arrivals.end(),
                    [](const MovingEnemyEndpoint &x,
                       const MovingEnemyEndpoint &y) {
                      if (x.arrivalDay != y.arrivalDay)
                        return x.arrivalDay < y.arrivalDay;
                      return x.warrior->id.num < y.warrior->id.num;
                    });
          int workerSlots = std::max(
              0, eb.work_cap() - stationaryAtRegion[eb.region]);
          for (int i = workerSlots; i < (int)arrivals.size(); ++i) {
            const auto &me = arrivals[i];
            int h = hops(eb.region, t);
            if (h >= 9999) continue;
            int responseArrival = me.arrivalDay + 1 + std::max(0, h - 1);
            addPreciseDefender(responseArrival, *me.warrior);
            earliestExternalDefenderArrival = std::min(
                earliestExternalDefenderArrival, responseArrival);
          }
        }

        std::sort(defenderArrivals.begin(), defenderArrivals.end(),
                   [](const StreamArrival &x, const StreamArrival &y) {
                    if (x.day != y.day) return x.day < y.day;
                    return x.num < y.num;
                   });

#if PROFITABLE_PRE_REINFORCEMENT_RAID
        // 완전 점령을 끝까지 유지할 수 없더라도, 실제 주둔군과 BASE를 가장
        // 빠른 외부 증원보다 먼저 제거하고 빠질 수 있으며 즉시 교환 가치가
        // 양수라면 별도의 단기 수익 공세 후보로 잡는다. HQ 업그레이드 상태나
        // 수입 우열과 무관하게 매 출격 판단에서 검사한다.
        if (enemyBuilding && preciseTargetBuilding->type == BType::BASE &&
            myCnt[t] == 0) {
          int ownTravel = hops(stagingPoint, t);
          int ownArrival = ownTravel >= 9999
                               ? std::numeric_limits<int>::max()
                               : std::max(0, ownTravel - 1);
          if (ownArrival != std::numeric_limits<int>::max() &&
              (earliestExternalDefenderArrival ==
                   std::numeric_limits<int>::max() ||
               earliestExternalDefenderArrival > ownArrival)) {
            int combatWindow =
                earliestExternalDefenderArrival ==
                        std::numeric_limits<int>::max()
                    ? std::max(0, MAX_TURN - turn - ownArrival)
                    : earliestExternalDefenderArrival - ownArrival;
            std::vector<CW> isolatedGarrison;
            for (const auto &w : S.warriors)
              if (w.id.side != M.my_side && w.region == t)
                isolatedGarrison.push_back({w.hp, w.id.num});
            int buildingInvestment = 0;
            for (int level = 1; level <= preciseTargetBuilding->level; ++level)
              buildingInvestment += BASE_LEVELS[level].cost;
            std::vector<int> selectedHps;
            for (int sendCount = 1;
                 sendCount <= (int)stagedLaunchers.size(); ++sendCount) {
              if (attackGold < sendCount * MOVE_COST) break;
              selectedHps.push_back(stagedLaunchers[sendCount - 1]->hp);
              IsolatedBaseCombatOutcome outcome =
                  simulate_isolated_base_combat(
                      preciseTargetBuilding->hp, targetTurret, selectedHps,
                      isolatedGarrison, combatWindow);
              bool destroyedRaid = outcome.destroyed &&
                                   outcome.attackerSurvivors >= 1;
              bool failedTrade =
#if PROFITABLE_FAILED_GARRISON_TRADE
                  !outcome.destroyed && outcome.resolved &&
                  outcome.attackerSurvivors == 0;
#else
                  false;
#endif
              if (!destroyedRaid && !failedTrade) continue;
              int ownLosses = sendCount - outcome.attackerSurvivors;
              int defenderLosses =
                  (int)isolatedGarrison.size() - outcome.defenderSurvivors;
              int immediateNet =
                  defenderLosses * TRAIN_COST +
                  (outcome.destroyed ? buildingInvestment : 0) -
                  ownLosses * TRAIN_COST -
                  sendCount * MOVE_COST;
              if (immediateNet <= 0) continue;
              bool better = profitableRaidTarget == -1 ||
                            (outcome.destroyed &&
                             !profitableRaidDestroyed) ||
                            (outcome.destroyed == profitableRaidDestroyed &&
                             immediateNet > profitableRaidImmediateNet) ||
                            (outcome.destroyed == profitableRaidDestroyed &&
                             immediateNet == profitableRaidImmediateNet &&
                             outcome.combatTurns <
                                 profitableRaidCombatTurns) ||
                            (outcome.destroyed == profitableRaidDestroyed &&
                             immediateNet == profitableRaidImmediateNet &&
                             outcome.combatTurns ==
                                 profitableRaidCombatTurns &&
                             sendCount < profitableRaidSend);
              if (!better) continue;
              profitableRaidTarget = t;
              profitableRaidSend = sendCount;
              profitableRaidCombatTurns = outcome.combatTurns;
              profitableRaidOwnArrival = ownArrival;
              profitableRaidEnemyArrival =
                  earliestExternalDefenderArrival;
              profitableRaidSurvivors = outcome.attackerSurvivors;
              profitableRaidImmediateNet = immediateNet;
              profitableRaidDefenderLosses = defenderLosses;
              profitableRaidBuildingHpRemaining =
                  outcome.buildingHpRemaining;
              profitableRaidDestroyed = outcome.destroyed;
            }
          }
        }
#endif

        // 현재 집결지에 실제로 서 있는 병력을 1명씩 늘려 보며 이 시간축을
        // 이기는 최소 파동을 찾는다. 미래 아군 생산은 첫 파동의 승리로
        // 미리 당겨 쓰지 않는다. 그것까지 넣으면 R58처럼 다른 필수 거점이
        // 먼저 무너져 증원 작전이 취소되는 판을 또 낙관하게 된다.
        for (int sendCount = 1;
             sendCount <= (int)stagedLaunchers.size(); ++sendCount) {
          if (attackGold < sendCount * MOVE_COST) break;
          std::vector<StreamArrival> attackerArrivals;
          bool reachable = true;
          for (int i = 0; i < sendCount; ++i) {
            const Warrior *w = stagedLaunchers[i];
            int h = hops(w->region, t);
            if (h >= 9999) {
              reachable = false;
              break;
            }
            attackerArrivals.push_back(
                {std::max(0, h - 1), w->hp, w->id.num});
          }
          if (!reachable) continue;
          StreamCombatForecast forecast = simulate_reinforcement_stream(
              targetHp, targetTurret, targetHasBuilding,
              std::move(attackerArrivals), defenderArrivals,
              preciseHorizon);
          if (!forecast.captured) continue;

          double targetDistance = P.dist[stagingPoint][t];
          bool better = false;
          if (forecast.captureDay != preciseBestCaptureDay)
            better = forecast.captureDay < preciseBestCaptureDay;
          else if (sendCount != preciseBestSend)
            better = sendCount < preciseBestSend;
          else if (forecast.attackerSurvivors != preciseBestSurvivors)
            better = forecast.attackerSurvivors > preciseBestSurvivors;
          else
            better = targetDistance < preciseBestDistance;
          if (!better) break;
          preciseBestTarget = t;
          preciseBestCaptureDay = forecast.captureDay;
          preciseBestSend = sendCount;
          preciseBestSurvivors = forecast.attackerSurvivors;
          preciseBestDefenders = (int)defenderArrivals.size();
          preciseBestDistance = targetDistance;
          break;
        }
      }

      dbg::note(turn, std::string(preciseGeneralLaunch
                                      ? "OFFENSE_PRECISE_PRELAUNCH old=R"
                                      : "PUSH_PRECISE_PRELAUNCH old=R") +
                          std::to_string(currentTarget) + " best=R" +
                          std::to_string(preciseBestTarget) + " capture=" +
                          std::to_string(preciseBestCaptureDay) + " send=" +
                          std::to_string(preciseBestSend) + " defenders=" +
                          std::to_string(preciseBestDefenders) +
                          " survivors=" +
                          std::to_string(preciseBestSurvivors));

      bool profitableRaidSelected = false;
#if PROFITABLE_PRE_REINFORCEMENT_RAID
      if (!preciseGeneralLaunch && preciseBestTarget == -1 &&
          profitableRaidTarget != -1) {
        int oldTarget = currentTarget;
        if (profitableRaidTarget != currentTarget) {
          std::vector<int> committed = std::move(reclaimAssault.committed);
          territoryReclaimTarget = profitableRaidTarget;
          territoryReclaimIsExpansion = true;
          reclaimAssault.reset(profitableRaidTarget, turn);
          reclaimAssault.committed = std::move(committed);
          currentTarget = profitableRaidTarget;
        }
        reclaimAssault.baseRaceStrike = true;
        reclaimAssault.failedGarrisonTrade = !profitableRaidDestroyed;
        reclaimAssault.postBreachHold = false;
        reclaimAssault.baseRaceImmediateNet = profitableRaidImmediateNet;
        initialChargeCount = profitableRaidSend;
        neededExtra = profitableRaidSend;
        extraToTrain = 0;
        candidates.assign(stagedLaunchers.begin(),
                          stagedLaunchers.begin() + profitableRaidSend);
        readyToCharge = true;
        profitableRaidSelected = true;
        dbg::note(turn, "PUSH_PROFITABLE_RAID from=R" +
                            std::to_string(stagingPoint) + " old=R" +
                            std::to_string(oldTarget) + " target=R" +
                            std::to_string(profitableRaidTarget) + " send=" +
                            std::to_string(profitableRaidSend) +
                            " combat_turns=" +
                            std::to_string(profitableRaidCombatTurns) +
                            " own_arrival=" +
                            std::to_string(profitableRaidOwnArrival) +
                            " enemy_arrival=" +
                            (profitableRaidEnemyArrival ==
                                     std::numeric_limits<int>::max()
                                 ? std::string("INF")
                                 : std::to_string(
                                       profitableRaidEnemyArrival)) +
                            " survivors=" +
                            std::to_string(profitableRaidSurvivors) +
                            " result=" +
                            std::string(profitableRaidDestroyed
                                            ? "DESTROYED"
                                            : "FAILED_TRADE") +
                            " defender_losses=" +
                            std::to_string(profitableRaidDefenderLosses) +
                            " building_hp=" +
                            std::to_string(
                                profitableRaidBuildingHpRemaining) +
                            " immediate_net=" +
                            std::to_string(profitableRaidImmediateNet));
      }
#endif

      if (!profitableRaidSelected && preciseBestTarget != -1 &&
          preciseBestTarget != currentTarget) {
        int oldTarget = currentTarget;
        if (!preciseGeneralLaunch) {
          std::vector<int> committed = std::move(reclaimAssault.committed);
          territoryReclaimTarget = preciseBestTarget;
          territoryReclaimIsExpansion = true;
          reclaimAssault.reset(preciseBestTarget, turn);
          reclaimAssault.committed = std::move(committed);
        }
        // 위 시간축은 현재 집결지에 실제로 서 있는 이 병력들이 지금 새
        // 목표로 직행한다고 계산했다. 여기서 return해 다음 턴의 일반
        // 집결 로직으로 넘기면 새 목표 최근접 거점으로 다시 우회해, 판단과
        // 실행의 도착일이 달라진다. 따라서 같은 턴, 시뮬레이션에 사용한
        // 최소 파동을 현재 집결지에서 곧바로 새 목표로 출격시킨다. 아래
        // 공통 돌격 코드가 이 값을 그대로 사용하도록 지역 변수도 함께
        // 동기화한다.
        currentTarget = preciseBestTarget;
        initialChargeCount = preciseBestSend;
        neededExtra = preciseBestSend;
        candidates.assign(stagedLaunchers.begin(),
                          stagedLaunchers.begin() + preciseBestSend);
        targetFixedIds.clear();
        readyToCharge = true;
        dbg::note(turn, std::string(preciseGeneralLaunch
                                        ? "OFFENSE_PRECISE_RETARGET_DIRECT from=R"
                                        : "PUSH_PRECISE_RETARGET_DIRECT from=R") +
                            std::to_string(oldTarget) + " to=R" +
                            std::to_string(preciseBestTarget) + " send=" +
                            std::to_string(preciseBestSend) + " committed=" +
                            std::to_string(reclaimAssault.committed.size()));
      }

      if (!profitableRaidSelected && preciseBestTarget == currentTarget) {
        initialChargeCount = preciseBestSend;
        neededExtra = preciseBestSend;
        candidates.assign(stagedLaunchers.begin(),
                           stagedLaunchers.begin() + preciseBestSend);
        readyToCharge = true;
      } else if (!profitableRaidSelected && preciseBestTarget == -1 &&
                 preciseGateWasReady) {
        // 지금 실제 집결 병력으로 어느 목표도 확보하지 못한다. 작은 파동을
        // 버리지 않고 한 명을 더 모은 뒤 다음 출격 직전에 다시 전 거점을
        // 계산한다. 일반 공세도 여기서는 같은 정밀 재판단 규칙을 쓴다.
        readyToCharge = false;
        neededExtra = std::min(40, (int)stagedLaunchers.size() + 1);
        initialChargeCount = neededExtra;
        extraToTrain = std::max(
            0, neededExtra - (int)attackCandidates.size());
        candidates.assign(
            attackCandidates.begin(),
            attackCandidates.begin() +
                std::min(neededExtra, (int)attackCandidates.size()));
        dbg::note(turn, std::string(preciseGeneralLaunch
                                        ? "OFFENSE_PRECISE_WAIT ready="
                                        : "PUSH_PRECISE_WAIT ready=") +
                            std::to_string(stagedLaunchers.size()) +
                            " next=" + std::to_string(neededExtra));
      }
    }
#elif RESELECT_OPTIONAL_PUSH_AT_LAUNCH
    // 선택 공세는 집결하는 동안에는 최초 목표를 그대로 유지한다. 다만
    // 실제 첫 파동을 내보낼 수 있게 된 바로 그 턴에는 최신 수비/병력으로
    // 전체 상대 거점을 한 번만 다시 비교한다. 목표가 바뀌면 오래된
    // 집결지에서 잘못 출격하지 않고 이번 턴의 공세 행동을 끝낸 뒤, 다음
    // 턴부터 새 목표로 재집결한다. 필수 거점 탈환과 출격 후 증원에는
    // 적용하지 않는다.
    if (readyToCharge && territoryReclaimAttack &&
        territoryReclaimIsExpansion && !territoryReinforcement &&
        !reclaimAssault.launched) {
      std::vector<const Warrior *> launchPushPool;
      std::vector<int> launchKeptWorkers(N, 0);
      for (const Warrior *w : idle) {
        int keep = need[w->region];
        if (reclaimAssault.isCommitted(w->id.num)) {
          launchPushPool.push_back(w);
          continue;
        }
        if (launchKeptWorkers[w->region] < keep) {
          ++launchKeptWorkers[w->region];
          continue;
        }
        launchPushPool.push_back(w);
      }

      int launchBestTarget = -1;
      int launchBestReadyTurns = std::numeric_limits<int>::max();
      int launchBestForce = std::numeric_limits<int>::max();
      double launchBestFrontDist = std::numeric_limits<double>::infinity();
      int launchPushTrainCap =
          std::max(1, my_hq_train_cap(S, M) * (MAX_TURN - turn));

      for (int t : M.strongholds) {
        const Building *launchTb = bld[t];
        bool enemyBuilding =
            launchTb != nullptr && launchTb->side != M.my_side;
        bool enemyOccupation = launchTb == nullptr && oppCnt[t] > 0;
        if (!enemyBuilding && !enemyOccupation) continue;

        bool simulatedEnemyBuilding = enemyBuilding;
        int simulatedEnemyHp = enemyBuilding ? launchTb->hp : 0;
#ifdef UNIFIED_RECLAIM_STREAM
        bool predictedEmptyFortify = !enemyBuilding && enemyOccupation &&
            opponent_can_fortify_empty_target_now(S, M, t);
        if (predictedEmptyFortify) {
          simulatedEnemyBuilding = true;
          simulatedEnemyHp = BASE_LEVELS[1].hp;
        }
#endif

        int launchTurret = 0;
        if (simulatedEnemyBuilding)
#ifdef UNIFIED_RECLAIM_STREAM
          launchTurret = predictedEmptyFortify
              ? BASE_LEVELS[1].turret
              : ((launchTb->type == BType::HQ)
                     ? HQ_LEVELS[launchTb->level].turret
                     : BASE_LEVELS[launchTb->level].turret);
#else
          launchTurret = (launchTb->type == BType::HQ)
              ? HQ_LEVELS[launchTb->level].turret
              : BASE_LEVELS[launchTb->level].turret;
#endif

        std::vector<CW> launchGarrison;
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side && w.region == t)
            launchGarrison.push_back({w.hp, w.id.num});

        std::vector<int> launchFixedHps;
        for (const auto &w : S.warriors) {
          if (w.id.side != M.my_side) continue;
          if ((w.state == WState::STATIONARY && w.region == t) ||
              (w.state == WState::MOVING &&
               w.purpose == WPurpose::ATTACK))
            launchFixedHps.push_back(w.hp);
        }
        std::vector<int> launchFreshHps;
        for (const Warrior *w : launchPushPool)
          if (w->region != t) launchFreshHps.push_back(w->hp);
        std::sort(launchFreshHps.begin(), launchFreshHps.end(),
                  std::greater<int>());

        AttackPlan launchChoicePlan = plan_attack_force(
            simulatedEnemyHp, launchTurret, launchGarrison,
            launchFixedHps, launchFreshHps, myWarriorHp,
            launchPushTrainCap, 1, simulatedEnemyBuilding);
        if (launchChoicePlan.extraToTrain < 0) continue;

        int launchFrontH = std::numeric_limits<int>::max();
        double launchFrontD = std::numeric_limits<double>::infinity();
        for (const auto &myB : S.buildings) {
          if (myB.side != M.my_side) continue;
          int h = hops(myB.region, t);
          double d = P.dist[myB.region][t];
          if (h < launchFrontH ||
              (h == launchFrontH && d < launchFrontD)) {
            launchFrontH = h;
            launchFrontD = d;
          }
        }
        if (launchFrontH >= 9999) continue;
        int perTurn = std::max(1, my_hq_train_cap(S, M));
        int trainTurns =
            (launchChoicePlan.extraToTrain + perTurn - 1) / perTurn;
        int readyTurns = launchFrontH + trainTurns;
        int force = launchChoicePlan.sendCount +
                    launchChoicePlan.extraToTrain;
        if (readyTurns > launchBestReadyTurns) continue;
        if (readyTurns == launchBestReadyTurns && force > launchBestForce)
          continue;
        if (readyTurns == launchBestReadyTurns && force == launchBestForce &&
            launchFrontD >= launchBestFrontDist)
          continue;
        launchBestTarget = t;
        launchBestReadyTurns = readyTurns;
        launchBestForce = force;
        launchBestFrontDist = launchFrontD;
      }

      dbg::note(turn, "PUSH_PRELAUNCH_CHECK old=R" +
                          std::to_string(currentTarget) + " best=R" +
                          std::to_string(launchBestTarget) + " ready=" +
                          std::to_string(launchBestReadyTurns) + " force=" +
                          std::to_string(launchBestForce));
      if (launchBestTarget != -1 && launchBestTarget != currentTarget) {
        int oldTarget = currentTarget;
        std::vector<int> committed = std::move(reclaimAssault.committed);
        territoryReclaimTarget = launchBestTarget;
        territoryReclaimIsExpansion = true;
        reclaimAssault.reset(launchBestTarget, turn);
        reclaimAssault.committed = std::move(committed);
        dbg::note(turn, "PUSH_PRELAUNCH_RETARGET from=R" +
                            std::to_string(oldTarget) + " to=R" +
                            std::to_string(launchBestTarget) + " committed=" +
                            std::to_string(reclaimAssault.committed.size()));
        return a;
      }
    }
#endif

#if ANTICIPATE_MANDATORY_PRECLAIM
    // 사전 감지 단계의 목적은 병력 생산과 집결을 앞당기는 것이다. 상대가
    // 아직 점유하지 않은 빈 땅으로 먼저 돌격시키면 기존 BUILD 확장과 다를
    // 바가 없고, 경로 오판 때 작전 병력이 묶인다. 실제 적 도착이 관측되어
    // 위에서 일반 필수 탈환으로 승격될 때까지 마지막 출격만 막는다.
    if (anticipatedMandatoryTarget == currentTarget) {
      if (readyToCharge)
        dbg::note(turn, "RECLAIM_PRECLAIM_READY_HOLD target=R" +
                            std::to_string(currentTarget) + " ready=" +
                            std::to_string(readyAtStaging) + " required=" +
                            std::to_string(initialChargeCount) +
                            " predicted_build=T" +
                            std::to_string(anticipatedMandatoryBuildTurn));
      readyToCharge = false;
    }
#endif

    // 집결이 아직 안 끝났으면, 집결지 밖 인원은 딱 부족한 만큼만
    // (neededExtra - readyAtStaging) 더 불러모은다 — 이미 집결지에 있는
    // 인원 위에 추가로 neededExtra명을 또 채우면 총 인원이 넘쳐버린다.
    int gatherTarget = std::max(0, neededExtra - readyAtStaging);

    int sent = 0;
    if (territoryReinforcement) {
      // 후속 병력은 다시 집결시키지 않는다. 각자 같은 잠금 목표를 향해
      // 즉시 파이프라인에 들어가며, 안전한 아군 건물 경유는 기존
      // pickWaypoint를 그대로 사용한다.
      for (const Warrior *w : candidates) {
        // BASE를 깬 뒤 이미 야전 전투를 피할 수 없게 된 HOLD 상태에서는
        // 경유지에 멈출 시간이 없다. 새 증원은 실제 전투 지역으로 곧장
        // 명령해 이동 경로와 롤아웃의 도착 시간을 일치시킨다.
        int step = reclaimAssault.postBreachHold
                       ? currentTarget
                       : pickWaypoint(w->region, currentTarget);
        bool stepIsMyBuilding =
            (bld[step] != nullptr && bld[step]->side == M.my_side);
        int cost = stepIsMyBuilding ? 0 : MOVE_COST;
        if (attackGold < cost) break;
        if (territoryReclaimAttack && !cautiousMandatoryRetry())
          reclaimAssault.commit(w->id.num);
        a.moves.push_back({w->id, step, WPurpose::ATTACK});
        dbg::move(turn, w->id, w->region, step, WPurpose::ATTACK,
                  std::string(reclaimAssault.postBreachHold
                                  ? "파괴 후 야전 HOLD 직행 ->R"
                                  : "회수전 연속 증원 ->R") +
                      std::to_string(currentTarget) + " (경유지 R" +
                      std::to_string(step) + ")");
        attackGold -= cost;
        ++sent;
      }
    } else {
      for (const Warrior *w : candidates) {
        bool atStaging = (stagingPoint != -1 && w->region == stagingPoint);
        if (readyToCharge) {
          if (!atStaging) continue; // 이미 다 모였으니 추가 집결은 불필요
          if (sent >= initialChargeCount) break;
          if (attackGold < MOVE_COST) break;
          if (territoryReclaimAttack && !cautiousMandatoryRetry())
            reclaimAssault.commit(w->id.num);
          a.moves.push_back({w->id, currentTarget, WPurpose::ATTACK});
          dbg::move(turn, w->id, w->region, currentTarget, WPurpose::ATTACK,
                    "총공세 돌격 (집결완료 " +
                        std::to_string(initialChargeCount) + "명)");
          if (territoryReclaimAttack)
            reclaimAssault.spearhead.push_back(w->id.num);
          attackGold -= MOVE_COST;
          ++sent;
        } else {
          if (atStaging) continue; // 아직 집결 중: 이미 도착한 인원은 그대로 대기
          if (sent >= gatherTarget) break;
          int dest = (stagingPoint != -1) ? stagingPoint : currentTarget;
          // 집결 이동도 목적지까지 한 번에 못 박지 않고, pickWaypoint가
          // 계산한 경유지(아군 건물을 징검다리 삼아 손해 5턴 이내에서 가장
          // 이르게 멈추는 지점)까지만 보낸다 — 도착하면 idle로 풀려 다음
          // 턴에 최신 계획으로 재판단한다. 총공세가 계속 켜져 있는 한
          // stagingPoint/currentTarget은 그대로 유지되므로 대개는 곧장
          // 이어서 다음 구간으로 나아가게 된다.
          int step =
#if DIRECT_ACTIVE_FIELD_RECLAIM
              activeMandatoryFieldBattleNow ? currentTarget :
#endif
              pickWaypoint(w->region, dest);
          bool stepIsMyBuilding =
              (bld[step] != nullptr && bld[step]->side == M.my_side);
          int cost = stepIsMyBuilding ? 0 : MOVE_COST;
          if (attackGold < cost) break;
          if (territoryReclaimAttack && !cautiousMandatoryRetry())
            reclaimAssault.commit(w->id.num);
          a.moves.push_back({w->id, step, WPurpose::ATTACK});
          dbg::move(turn, w->id, w->region, step, WPurpose::ATTACK,
                    "총공세 집결 ->R" + std::to_string(dest) + " (경유지 R" +
                        std::to_string(step) + ")");
          attackGold -= cost;
          ++sent;
        }
      }
      if (territoryReclaimAttack && readyToCharge &&
          sent == initialChargeCount && sent > 0) {
        // 이미 목표에서 싸우고 있던 고정 전력도 이번 선발대의 일부다.
        // (집결 중인 병력은 위 readyToCharge 게이트 때문에 이 시점에는 없다.)
        const std::vector<int> &initialFixedIds =
            cautiousMandatoryRetry() ? committedIds : targetFixedIds;
        for (int num : initialFixedIds)
          if (std::find(reclaimAssault.spearhead.begin(),
                        reclaimAssault.spearhead.end(), num) ==
              reclaimAssault.spearhead.end())
            reclaimAssault.spearhead.push_back(num);
        reclaimAssault.launched = true;
        reclaimLaunchedThisTurn = true;
        dbg::note(turn, "RECLAIM_WAVE launched=" + std::to_string(sent) +
                            " target=R" + std::to_string(currentTarget) +
                            " mode=" +
                            (territoryReclaimIsExpansion
                                 ? std::string("OPTIONAL")
                                 : std::string("MANDATORY")));
      }
    }
    // 예전엔 여기서 무조건 훈련 가능한 만큼 최대로 뽑았는데, 그러면
    // 이미 충분한 상황(extraToTrain<=0)에서도 총공세가 켜져 있는 내내
    // 계속 병력을 과잉 생산하게 된다. 계획 단계에서 계산해 둔 추가
    // 필요 인원(extraToTrain)만큼만 훈련한다. 사령부 레벨이 상대에게
    // 밀리고 있으면 이마저도 멈추고 골드를 사령부 업그레이드에 전부
    // 남겨둔다. gold_lead_military(사령부 만렙 + 골드 우위일 때 남는
    // 돈을 병력으로 돌리는 목표)와 strongholdTrainNeed(위에서 이미 방어/
    // 확장 재배치 단계에서 잡아둔 빈 거점용 훈련 몫)는 total_offensive
    // 중이라고 꺼지면 안 되므로 여기서도 그대로 반영한다.
    // 공세 중에도 방어 훈련을 함께 낸다: 방어 공백(missing_workers)과 전체
    // HP 열세 보충(baseline_military)은 total_offensive라고 꺼지면 안 된다.
    // 예전엔 공세가 켜지면 이 분기가 곧장 return해서 아래 최종 want(방어
    // 훈련)를 못 타고 extraToTrain(공격용)만 뽑았는데, 그 사이 위협받는
    // 거점이 병력을 충원받지 못한 채 방치됐다. hqBehind(사령부 레벨 열세)면
    // 최종 want와 동일하게 이 유지 수요는 모두 멈추고 골드를 아낀다.
    int maintainWant = (hqBehind && !territoryReclaimAttack) ? 0
        : std::max({missing_workers, baseline_military,
                    offensiveReserveWant,
                    extraToTrain > 0 ? extraToTrain : 0});
    int trainWant = 0;
    if (territoryReclaimAttack) {
      // ASSEMBLE에서는 계산된 부족분을 채우고, 최초 돌격을 명령한 바로
      // 그 턴부터는 다음 턴에 도착할 증원 1명을 예약한다. REINFORCE 동안도
      // 매 턴 한 명을 최우선 생산한다(실제 생산량은 HQ cap/골드로 제한).
      int attackTrainWant =
          (territoryReinforcement || reclaimLaunchedThisTurn)
              ? 1
              : std::max(0, extraToTrain);
      // 선택 공세에서는 공격 병력보다 기존 거점의 결원/전력 열세 보충을
      // 낮게 취급하지 않는다. 생산 슬롯이 하나뿐이면 이 수요를 만족한
      // 신병이 다음 턴 방어 재배치에 먼저 배정되고, 남는 병력이 공격
      // 파이프라인으로 들어간다.
      trainWant = optionalTerritoryPush
                      ? std::max({attackTrainWant, missing_workers,
                                  baseline_military,
                                  offensiveReserveWant})
                      : attackTrainWant;
    } else {
      trainWant =
          std::max({maintainWant, gold_lead_military, strongholdTrainNeed});
    }
    a.train_n = std::max(0, std::min({my_hq_train_cap(S, M), attackGold / TRAIN_COST, trainWant}));
    dbg::note(turn, "TRAIN " + std::to_string(a.train_n) + " (공세중; want=" +
                        std::to_string(trainWant) + " extraToTrain=" +
                        std::to_string(extraToTrain) + ")");
    return a;
  }


  // 업그레이드는 수급 비교와 무관하게 계속한다: 공격(idle 병력 소모)과
  // 업그레이드(골드 소모)는 서로 다른 자원을 쓰므로 배타적일 필요가 없다.
  // 기지/사령부 업그레이드 자체가 work_cap을 늘려 수급을 개선하는 수단이라,
  // 수급이 밀릴 때 오히려 이쪽도 같이 돌아가야 격차를 더 빨리 좁힌다.
  // 예전엔 여기서 turn/MAX_TURN 비율로 예산을 잠갔다 풀었다 했는데, 그
  // 인위적인 턴 비율 대신 아래 upgradeCandidates 정렬 자체(사령부가 항상
  // 거리 0이라 최우선)에 맡긴다: 예산이 되면 사령부부터 사고, 안 되면
  // 남는 돈이 자연히 거점으로 흘러간다.
  int upgrade_budget = std::max(0, gold - reserved_build);
  // 훈련 예산은 이미 위(best_help 직후)에서 buildNowCandidates보다 먼저
  // gold에서 떼어 train_reserved에 넣어뒀으므로, 여기 upgrade_budget은
  // 자연히 그 나머지만 쓰게 된다 — 별도의 hqMaxed 전용 잠금은 더 이상
  // 필요 없다.

  // HQ와 기지를 합쳐서 "본부로부터 가까운 순"으로 정렬한 뒤 그 순서대로
  // 예산을 쓴다. HQ는 본부 자신(거리 0)이라 항상 가장 먼저 시도된다.
  std::vector<const Building *> upgradeCandidates;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    // 사령부는 위에서 공격 판단 전에 이미 처리했으니 여기서 또 큐에
    // 넣지 않는다(중복 UPGRADE 명령 방지)
    if (b.type == BType::HQ && hqUpgradedThisTurn) continue;
    if (b.type == BType::HQ && b.level >= HQ_MAX_LEVEL) continue;
    if (b.type == BType::BASE && b.level >= BASE_MAX_LEVEL) continue;
    upgradeCandidates.push_back(&b);
  }
  std::sort(upgradeCandidates.begin(), upgradeCandidates.end(),
           [&](const Building *x, const Building *y) {
             return hops(M.my_hq, x->region) < hops(M.my_hq, y->region);
           });

  // (예전엔 여기서 "사령부가 지금 당장 안 되면 추가로 더 저축" 잠금이 하나
  // 더 있었는데, 위 첫 번째 잠금(lateness 비례로 예산을 풀어줌)과 정확히
  // 반대 방향(1-lateness 비례로 예산을 다시 잠금)으로 작동해서 둘을 곱하면
  // 게임 내내 예산이 거의 항상 눌려 있는 문제가 있었다. 그래서 기지
  // 업그레이드가 사실상 전혀 안 되고 있었다. 이 이중 잠금을 제거하고 위
  // 첫 번째 잠금 하나로만 초반 확장/후반 사령부 우선순위를 조절한다.)

  // 거점 업글을 미루고 사령부 업글 자금을 모으는 건 다음 두 경우로만
  // 한정한다: (1) 내 사령부 레벨이 상대보다 낮아 따라잡는 게 급한 경우,
  // (2) 내 순수입이 상대보다 많아 거점을 더 늘리지 않아도 격차가 벌어지지
  // 않는(=저축 여유가 있는) 경우. 둘 다 아니면(사령부 레벨이 같거나
  // 앞서 있고, 수입도 상대보다 적거나 같으면) 거점 업글을 미룰 이유가
  // 없으므로 바로 진행한다.
  bool waitForHqUpgrade =
      (myHqLevel < oppHqLevel) || (current_net_income > oppNetIncome);

  // 신규 거점 건설(300골드, 일자리+1)이 기존 거점 레벨업(600/1000골드)보다
  // 일자리당 훨씬 싸다. 그러니 아직 아무도 안 지은 빈 거점이 남아 있는
  // 동안은 거점 레벨업에 돈을 쓰지 않고, 그 골드를 새 거점 확보(위
  // buildNowCandidates/stronghold-first 파병 로직)에 먼저 돌린다.
  bool hasUnbuiltStronghold = false;
  for (int t : M.strongholds) {
    if (bld[t] != nullptr) continue;
    if (!strategicWanted[t]) continue;
    if (t != territoryReclaimTarget && !worth_building_base(turn, 0)) continue;
    hasUnbuiltStronghold = true;
    break;
  }
  for (const Building *b : upgradeCandidates) {
    if (b->type == BType::BASE && waitForHqUpgrade) continue;
    if (b->type == BType::BASE && hasUnbuiltStronghold) continue;
    if (b->type == BType::BASE && !worth_upgrading_base(turn, b->level, myHqLevel)) continue;
    int c = (b->type == BType::HQ) ? HQ_LEVELS[b->level + 1].upgrade_cost
                                    : BASE_LEVELS[b->level + 1].cost;
    if (myCnt[b->region] > 0 && oppCnt[b->region] == 0 && upgrade_budget >= c) {
      a.upgrades.push_back(b->region);
      upgrade_budget -= c;
      gold -= c;
      dbg::note(turn, "UPGRADE R" + std::to_string(b->region) +
                          (b->type == BType::HQ ? " (HQ)" : " (기지)") + " L" +
                          std::to_string(b->level) + "->" +
                          std::to_string(b->level + 1) + " (cost=" +
                          std::to_string(c) + ")");
    }
  }

  // missing_workers/baseline_military/cap은 buildNowCandidates 이전에 이미
  // 계산해 훈련 예산(train_reserved)에 반영해 뒀으므로 여기서는 그 값을
  // 그대로 재사용한다.
  gold += train_reserved;

  int want = std::max({missing_workers, baseline_military,
                       strongholdTrainNeed, gold_lead_military,
                       offensiveReserveWant});
  // 예측 방어 수요(결원보충/HP열세보충)는 빈 거점 건설비 예약(reserved_build)
  // 보다 우선한다: 건설비 예약은 "비상용"이라, 그 예약(빚) 때문에 방어 수요를
  // 훈련으로 못 채우게 되면 예약을 무시하고 방어부터 채운다. 방어를 넘는
  // 여유 수요(빈 거점/골드우위)만 예약을 지킨 예산으로 뽑는다.
  int defenseWant = std::max(missing_workers, baseline_military);
  int budgetAfterReserve = std::max(0, gold - reserved_build);
  int nNormal = std::min({cap, budgetAfterReserve / TRAIN_COST, want});
  int nEmergency = std::min({cap, gold / TRAIN_COST, defenseWant});

  // 사령부 레벨이 상대에게 밀리고 있으면 병력 생산도 멈추고 골드를
  // 사령부 업그레이드에 전부 남겨둔다(거점 업그레이드는 이미 waitForHqUpgrade가
  // 막아 두었다).
  a.train_n = (hqBehind || reclaimReadyToBuild)
      ? 0 : std::max({0, nNormal, nEmergency});
  dbg::note(turn, "TRAIN " + std::to_string(a.train_n) + " (평시; want=" +
                      std::to_string(want) + " nNormal=" +
                      std::to_string(nNormal) + " nEmergency=" +
                      std::to_string(nEmergency) +
                      (hqBehind ? " HQ열세->0" : "") +
                      (reclaimReadyToBuild ? " 회수건설저축->0" : "") + ")");

  return a;
}
int main() {
  GameMap M;
  GameState S;
  parse_init(M, S);
  Paths P = calculate_paths(M);
  dbg::init(M.my_side);

  int turn;
  while (read_turn_start(turn)) {
    dbg::turn_header(turn, S, M);
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }
  return 0;
}
