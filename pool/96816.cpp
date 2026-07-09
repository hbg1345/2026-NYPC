#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
          Building *tb = find_building(S, region);
          int cost = (tb != nullptr && tb->side == id.side) ? 0 : MOVE_COST;
          S.opp_gold -= cost;
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
      if (!didMove) w.state = WState::STATIONARY;
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

// 시뮬레이션으로 계산한 최소 필요 인원은 어디까지나 지금 보이는 정보만
// 반영한 값이라 딱 그만큼만 보내면 예측이 살짝만 틀려도 실패할 수 있다.
// 그래서 최소 인원보다 항상 이 마진만큼 더 모아서/훈련해서 보낸다.
constexpr int ATTACK_SAFETY_MARGIN = 5;

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
                                    int safetyMargin = ATTACK_SAFETY_MARGIN) {
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
    while (!aF.empty() && hp > 0) {
      combatDay(aF, 0, aBldHpDummy, defF, turret, hp);
    }
    return hp <= 0;
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
static int min_regional_defenders(int bldHp, int turret,
                                  const std::vector<int> &attackerHps,
                                  int myWarriorHp) {
  auto simulate = [&](int defenders) {
    std::vector<CW> defF;
    defF.reserve(defenders);
    for (int i = 0; i < defenders; ++i) defF.push_back({myWarriorHp, i});
    std::vector<CW> aF;
    aF.reserve(attackerHps.size());
    int num = 0;
    for (int hp : attackerHps) aF.push_back({hp, num++});
    int hp = bldHp;
    int aBldHpDummy = -1; // 공격측은 필드 병력이라 건물이 없음
    while (!aF.empty() && hp > 0) {
      combatDay(defF, turret, hp, aF, 0, aBldHpDummy);
    }
    return hp > 0; // 건물이 버텼으면(=공격측이 먼저 전멸) 방어 성공
  };

  // 상한: 머릿수만 맞춰도(+포탑 보너스) 대개 버티므로 공격 인원수의 2배 +
  // 여유분이면 충분하다. 그래도 안 되면(수비측 체력이 크게 밀리는 극단적
  // 상황) 그 상한을 그대로 반환한다.
  int cap = (int)attackerHps.size() * 2 + 5;
  for (int k = 0; k <= cap; ++k)
    if (simulate(k)) return k;
  return cap;
}

// ================= ULTIMATE STRATEGY DECIDE FUNCTION =================
static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  Actions a;
  int gold = S.gold;
  const int N = M.N;

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
  for (const auto &w : S.warriors) (w.id.side == M.my_side ? myCnt : oppCnt)[w.region]++;
  std::vector<char> isStrong(N, 0);
  for (int s : M.strongholds) isStrong[s] = 1;

  // 지역별로 지금 실제로 그 자리에 있는 상대 유닛들의 체력 목록. need[r]
  // 계산에서 "머릿수 - 포탑"이 아니라 실제 combatDay 시뮬레이션으로 버틸
  // 수 있는 인원을 구하는 데 쓴다.
  std::vector<std::vector<int>> oppHpsAt(N);
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side) oppHpsAt[w.region].push_back(w.hp);

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

    // 이 집결지에서 가장 가까운 내 건물을 "노려질 만한 곳"으로 본다.
    // hops가 같으면(동률), 전선(두 사령부 축의 수직이등분선)에 더 가까운
    // 쪽이 실제로 노려질 확률이 높은 최전선이므로 그쪽을 우선한다.
    int mirrorTarget = -1, mirrorH = std::numeric_limits<int>::max();
    double mirrorFrontD = std::numeric_limits<double>::infinity();
    for (const auto &mb : S.buildings) {
      if (mb.side != M.my_side) continue;
      int h = hops(b.region, mb.region);
      double frontD = frontlineDist(mb.region);
      if (h > mirrorH) continue;
      if (h == mirrorH && frontD >= mirrorFrontD) continue;
      mirrorH = h; mirrorFrontD = frontD; mirrorTarget = mb.region;
    }
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
  std::vector<int> need(N, 0);
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
      int sim_need = attackerHps.empty() ? 0
          : min_regional_defenders(bld[r]->hp, turret, attackerHps, curWarriorHp);
      // 집결 감지분은 시뮬레이션이 아니라 머릿수 1:1로 잡는다. 이동 감지
      // 시뮬레이션 결과와는 둘 중 큰 쪽만 취한다(합산하지 않음). 실제로
      // 움직이기 시작하면 위 threat_hps 쪽으로 잡히고 여기 staging_defense
      // 에는 안 들어오므로 이중 계산되지 않는다.
      int defense_need = std::max(sim_need, staging_defense[r]);
      need[r] = std::max(bld[r]->work_cap(), defense_need);
      // 훈련용 수요에는 집결 예측분(staging_defense)을 빼고 시뮬레이션분만 반영.
      need_train[r] = std::max(bld[r]->work_cap(), sim_need);
    }
    // 아직 건물은 없지만 이미 내 병력이 점거해 둔(상대는 없는) 거점은 여기서
    // need를 강제로 예약해두지 않는다. 지금 당장 지을 돈이 있으면 아래
    // buildNowCandidates에서 바로 지어지고, 돈이 없으면 이 사람은 surplus로
    // 풀려서 아래 stronghold-first 파병 패스나 best_help 로직이 가장
    // 가까운 곳(빈 거점 또는 일손 부족한 아군 거점)으로 자연히 재배치한다.
    // 예전엔 여기서 need[r]=1로 무조건
    // 한 명을 잡아둬서, 건설 자금이 안 모이면 그 병력이 아무 일도 안 하고
    // 무한정 대기하는 문제가 있었다.
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

  // 각 거점에 이미 배치돼 있거나(homeCnt) 오는 중인(incoming) 병력 수.
  // 총공세 판단(방어 공백 감지)과 그 아래 재배치 로직 모두에 필요해서
  // 여기서 미리 계산해 둔다.
  std::vector<int> homeCnt(N, 0), incoming(N, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) continue;
    if (w.state == WState::MOVING) ++incoming[w.target];
    else ++homeCnt[w.region];
  }

  std::vector<int> kept(N, 0);
  int train_reserved = 0;

  // best_help 재배치로 실제 이동 명령을 받은 병력은 이번 턴에 이미 다른
  // 용도로 커밋된 것이므로, 뒤이은 공격 후보 선정(idle 재스캔)에서 다시
  // 뽑히면 같은 워리어에게 명령이 두 번 내려가는 셈이 된다. 그래서 이동
  // 명령을 받은 워리어를 따로 모아뒀다가 루프가 끝난 뒤 idle에서 제거한다.
  std::vector<const Warrior *> dispatchedForHelp;
  for (const Warrior *w : idle) {
    int r = w->region;

    if (w->purpose == WPurpose::BUILD && bld[r] == nullptr) {
      // 건설 예정지에 도착해 건물이 지어지길 기다리는 중 -> 재배치 금지
      continue;
    }

    // 사령부든 거점이든 구분 없이, 자기 일자리/방어 수요(need[r])만큼은
    // 우선 예약해서 남겨두고, 그 이상 남는 인원만 재배치 후보로 삼는다.
    if (kept[r] < need[r]) {
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
      if (kept[r] < need[r]) ++kept[r];
      continue;
    }

    // 목적지가 예측(threat_count) 기반 방어 수요 때문에 골라진 경우, 예측이
    // 틀리면 그 병력을 되돌릴 수 없다는 위험이 있다. 그래서 목적지까지 한
    // 번에 못 박지 않고 pickWaypoint가 계산한 경유지에만 이번 턴 이동시킨다.
    int move_target = best_target;
    if (threat_count[best_help] > 0)
      move_target = pickWaypoint(r, best_help);

    // best_help의 목적지는 항상 아군 건물이라 이동 비용이 무료다.
    bool needs_replacement = (kept[r] < need[r]);
    a.moves.push_back({w->id, move_target, WPurpose::MOVE});
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
    if (!worth_building_base(turn, 0)) continue; // 남은 턴 수입이 건설비보다 싸면 포기
    buildNowCandidates.push_back(r);
  }
  std::sort(buildNowCandidates.begin(), buildNowCandidates.end(), [&](int x, int y) {
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
  bool strongholdDone = false;
  // 이 루프에서 신병을 훈련하기로 결정할 때마다(strongholdTrainNeed 증가)
  // 그 병사의 유지비만큼 다음 턴부터 실제 순수입이 줄어든다. current_net_income을
  // 그대로 쓰면 이미 결정한 훈련이 없는 셈 치고 미래 수입을 낙관적으로
  // 추정하게 되므로, 루프 진행에 따라 갱신되는 예상치를 따로 둔다.
  // current_net_income은 턴 시작 스냅샷이라 이번 턴 확정 건설분을 아직
  // 모른다. 그 수입(confirmedBuildIncome)을 더해, 다른 변화가 없다면 다음
  // 턴 current_net_income이 실제로 갖게 될 값에서 파병 판단을 시작한다.
  int projIncome = current_net_income + confirmedBuildIncome;
  // 상대도 같은 빈 거점을 노릴 수 있다. 내가 아무리 서둘러도 상대가 나보다
  // 먼저 지을 수 있는 거점이라면 경쟁에서 밀려 파병 자체가 헛수고가
  // 되므로, 상대 진영에서 그 거점까지 최속으로 완공할 수 있는 턴을
  // 추정해두고 그보다 늦는 후보는 애초에 걸러낸다. 상대 쪽 상태(건물,
  // 수입, 골드)는 이번 결정 동안 바뀌지 않으므로 루프 시작 전에 한 번만
  // 계산한다. 상대의 정확한 유휴 병력 배치는 알 수 없으니, 상대의 가장
  // 가까운 건물에서 곧장 출발한다고 가정하는 낙관적(=상대에게 유리한)
  // 추정치를 쓴다 — 그래야 실제로는 내가 이길 수 있는 후보를 잘못 걸러내는
  // 일이 없다.
  auto oppFastestBuildTurn = [&](int t) -> int {
    int best = std::numeric_limits<int>::max();
    for (const auto &b : S.buildings) {
      if (b.side == M.my_side || b.region == t) continue;
      int h = hops(b.region, t);
      if (h >= 9999) continue;
      int totalCost = BASE_LEVELS[1].cost + MOVE_COST;
      int shortfall = totalCost - oppNetIncome * h - S.opp_gold;
      int oppTurn = h;
      if (shortfall > 0) {
        oppTurn = (oppNetIncome > 0)
            ? h + (shortfall + oppNetIncome - 1) / oppNetIncome
            : std::numeric_limits<int>::max() / 2;
      }
      best = std::min(best, oppTurn);
    }
    return best;
  };
  std::vector<int> oppBestTurn(N, std::numeric_limits<int>::max());
  for (int t : M.strongholds) oppBestTurn[t] = oppFastestBuildTurn(t);
  while (!strongholdDone) {
    std::vector<int> needing;
    for (int t : M.strongholds) {
      if (bld[t] != nullptr) continue;
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
    for (const Warrior *w : idle)
      if (w->region == hqRegion) ++hqIdleCnt;

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
      int bTurn = std::numeric_limits<int>::max();
      double bFrontD = std::numeric_limits<double>::infinity();
      double bD = std::numeric_limits<double>::infinity();
      for (int t : needing) {
        double frontD = frontlineDist(t);
        int h = hops(hqRegion, t);
        int steps = h + delay; // 출발 전 지연까지 포함한, 지금부터 도착까지의 실제 턴 수
        if (!worth_building_base(turn, steps)) continue;

        int totalCost = BASE_LEVELS[1].cost + MOVE_COST;
        int shortfall = totalCost - simIncome * steps - simGold;
        int myTurn = steps;
        if (shortfall > 0) {
          myTurn = (simIncome > 0)
              ? steps + (shortfall + simIncome - 1) / simIncome
              : std::numeric_limits<int>::max() / 2; // 수입으로는 영영 못 모음
        }

        if (myTurn > bTurn) continue;
        if (myTurn == bTurn && frontD > bFrontD) continue;
        if (myTurn == bTurn && frontD == bFrontD &&
            P.dist[hqRegion][t] >= bD) continue;
        bTurn = myTurn; bFrontD = frontD; bD = P.dist[hqRegion][t];
        bT = t; bH = h;
      }
      return std::pair<int, int>{bT, bH};
    };

    int bestT = -1, bestH = std::numeric_limits<int>::max();
    if (hqIdleCnt > need[hqRegion])
      std::tie(bestT, bestH) = findHqDispatchTarget(0, available_gold, projIncome);

    if (bestT != -1) {
      int base_cost = BASE_LEVELS[1].cost + MOVE_COST - (projIncome * bestH);
      int req_gold = std::max(MOVE_COST, base_cost);
      if (available_gold < req_gold) { strongholdDone = true; break; }

      auto it = idle.end();
      for (auto cand = idle.begin(); cand != idle.end(); ++cand) {
        if ((*cand)->region != hqRegion) continue;
        if (it == idle.end() || (*cand)->hp > (*it)->hp) it = cand;
      }
      const Warrior *w = *it;
      a.moves.push_back({w->id, bestT, WPurpose::BUILD});
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
    int hqBestT = -1;
    if (projectedHqIdleCnt > need[hqRegion])
      hqBestT = findHqDispatchTarget(1, available_gold - TRAIN_COST,
                                     projIncome - UPKEEP_PER_WARRIOR).first;
    if (hqBestT == -1 || strongholdTrainCapLeft <= 0) { strongholdDone = true; break; }
    if (available_gold < TRAIN_COST) { strongholdDone = true; break; }

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
  // 아직 아무도 안 지은 빈 거점이 있고 그걸 점거하는 데 병력이 더
  // 필요하다면(homeCnt+incoming이 oppCnt+1에 못 미치면), 총공세보다 빈
  // 거점 확보를 항상 우선한다. 공짜 거점을 놔두고 상대 본진을 치러 가는
  // 건 낭비다 — 위 stronghold-first 파병 패스가 이미 가용 자원(유휴
  // 병력/훈련 한도/골드) 내에서 최대한 처리했으므로, 여기서 여전히
  // 남아있다는 건 자원이 부족해서 이번 턴엔 더 손댈 수 없다는 뜻이다.
  bool hasCapturableEmptyStronghold = false;
  for (int t : M.strongholds) {
    if (bld[t] != nullptr) continue;
    if (!worth_building_base(turn, 0)) continue; // 남은 턴 수입이 건설비보다 싸면 우선순위에서 제외
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
  bool canOffensive = (!myIncomeAhead || hqOffensiveAdvantage) &&
                      !hasCapturableEmptyStronghold;
  if (canOffensive) {
    // 사령부가 만렙이고 거점을 70% 이상 장악했으며, 병력 체력 총합에서도
    // 상대보다 앞서는 압도적 우위 상황이면, 아래 진격로 스코어링(변두리
    // 거점을 하나씩 잠식하는 절차)을 건너뛰고 곧장 상대 사령부를 총공세
    // 목표로 잡는다. 세 조건 중 하나라도 안 맞으면(예: 거점은 많이
    // 먹었지만 병력에서 밀리면) 무리해서 결전으로 가지 않고 기존 방식대로
    // 진행한다.
    int myStrongCnt = 0;
    for (int t : M.strongholds)
      if (bld[t] != nullptr && bld[t]->side == M.my_side) ++myStrongCnt;
    bool dominant = hqMaxed && !M.strongholds.empty() &&
                    (double)myStrongCnt / (double)M.strongholds.size() >= 0.7 &&
                    myTotalHp > oppTotalHp;

    if (dominant) {
      currentTarget = M.opp_hq;
    } else {
      // 목표 선정(정확 시뮬): 각 (집결지 s = 아군 건물, 목표 t = 상대 거점,
      // HQ 제외)에 대해 실제 소요 턴을 계산해 그 최솟값이 가장 작은 거점을
      // 목표로 잡는다. 소요 턴 = ① s에 y명이 실제로 모이는 데 걸리는 턴
      // (동원 가능한 유휴 병력을 s까지의 hop이 가까운 순으로 y명 골랐을 때
      // 그중 가장 늦게 도착하는 y번째의 hop) + ② s→t 이동 hop + ③ 그 y명을
      // (신병이 아니라 실제 체력 그대로) t의 포탑/상주 병력에 부딪혀 무너뜨리는
      // 데 걸리는 공성 일수. y는 1..동원가능수를 모두 훑어 이 총합이 가장
      // 작아지는 조합을 찾는다(더 많이 모으면 집결은 늦지만 공성이 빨라지는
      // 트레이드오프를 정확히 반영). 동률이면 y가 더 적은 쪽을 택한다.
      //
      // 실제 파병 인원/집결지는 여기 결과가 아니라 아래 stagingPoint 계산과
      // plan_attack_force가 committedHps/안전마진까지 반영해 다시 정한다 —
      // 여기 시뮬은 "어느 거점이 가장 빨리·적게 먹히는가"를 비교하는 용도다.

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

      int bestTurns = std::numeric_limits<int>::max();
      int bestY = std::numeric_limits<int>::max();
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
            // 집결+이동만으로 이미 현재 최선 이상이면(공성은 최소 1일 더),
            // 이 s에서 더 큰 y는 gather가 더 커질 뿐이라 개선 불가 -> 중단.
            if (gather + travel >= bestTurns) break;
            int sd = siegeDays(aF, b.hp, turret, garrison);
            if (sd == NEVER) continue; // 이 y로는 아직 못 뚫음 -> 더 모아본다
            int total = gather + travel + sd;
            if (total < bestTurns || (total == bestTurns && y < bestY)) {
              bestTurns = total; bestY = y; currentTarget = b.region;
            }
          }
        }
      }

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
      if (currentTarget == M.opp_hq)
        effNeed[M.my_hq] = bld[M.my_hq]->work_cap();

      // 목표와 가장 가까운 아군 거점을 집결지로 잡는다. 실제 파병
      // 로직(아래 total_offensive 분기)과 여기 계획 단계 모두 같은
      // 집결지를 기준으로 삼아야 한다.
      int stagingH = std::numeric_limits<int>::max();
      double stagingD = std::numeric_limits<double>::infinity();
      for (const auto &b : S.buildings) {
        if (b.side != M.my_side) continue;
        int h = hops(b.region, currentTarget);
        if (h > stagingH) continue;
        if (h == stagingH && P.dist[b.region][currentTarget] >= stagingD) continue;
        stagingH = h;
        stagingD = P.dist[b.region][currentTarget];
        stagingPoint = b.region;
      }

      // 이미 지난 턴들에 이 작전을 위해 파병되어 지금 집결지나 목표로
      // 이동 중인(아직 유휴 상태가 아닌) 병력. 이미 결정되어 오고 있는
      // 몫이니 무조건 계산에 포함시켜야 한다 — 이걸 무시하면 매 턴 이미
      // 오고 있는 병력이 없는 셈 치고 또 훈련/파병을 지시하게 되어
      // 필요 인원(surplus)을 과하게 책정하게 된다.
      std::vector<int> committedHps;
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side || w.state != WState::MOVING) continue;
        // 목적지가 이번 턴에 계산된 경유지(pickWaypoint)라 currentTarget/
        // stagingPoint와 다를 수 있으므로, 목적지 일치가 아니라 파견
        // 목적(ATTACK 표식)으로 "이미 이 작전에 커밋된 병력"인지 판단한다.
        if (w.purpose == WPurpose::ATTACK)
          committedHps.push_back(w.hp);
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
        if (w->purpose == WPurpose::BUILD && bld[r] == nullptr) continue;
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
      int trg_turret = (tb->type == BType::HQ) ? HQ_LEVELS[tb->level].turret
                                                : BASE_LEVELS[tb->level].turret;
      std::vector<CW> garrison;
      for (const auto &w : S.warriors)
        if (w.id.side != M.my_side && w.region == currentTarget)
          garrison.push_back({w.hp, w.id.num});
      int myWarriorHp = HQ_LEVELS[myHqLevel].warrior_hp;

      std::vector<int> freshHpsDesc;
      freshHpsDesc.reserve(attackCandidates.size());
      for (const Warrior *w : attackCandidates) freshHpsDesc.push_back(w->hp);

      int trainCap = std::max<size_t>(1, my_hq_train_cap(S, M) * (size_t)(MAX_TURN - turn));
      // 목표가 상대의 진짜 사령부면 결전이라 예측이 틀렸을 때 되돌릴 수
      // 없다. 그래서 평소 마진(ATTACK_SAFETY_MARGIN=5명)보다 훨씬 두텁게,
      // 지금 내가 보유한 전체 병력의 1/3만큼을 여유로 잡는다. 다만 병력
      // 총수가 적을 때는 1/3이 5명보다 작아질 수 있으므로, 최소한 평소
      // 마진(5명)보다는 못하지 않게 max로 하한을 둔다.
      int myTotalWarriors = 0;
      for (int c : myCnt) myTotalWarriors += c;
      int safetyMargin = (currentTarget == M.opp_hq)
          ? std::max(ATTACK_SAFETY_MARGIN, myTotalWarriors / 3)
          : ATTACK_SAFETY_MARGIN;
      AttackPlan plan = plan_attack_force(tb->hp, trg_turret, garrison,
                                         committedHps, freshHpsDesc,
                                         myWarriorHp, trainCap, safetyMargin);

      // 이미 오고 있는 병력(committedHps)만으로 충분하면 새로 보낼 필요가
      // 없다(plan.sendCount==0). 부족하면 유휴 병력 중 plan.sendCount명만
      // 추가로 보내고, 그래도 모자란 plan.extraToTrain명은 아래 train_n
      // 계산이 한도 내에서 채운다.
      neededExtra = plan.sendCount;
      extraToTrain = plan.extraToTrain;
      total_offensive = (neededExtra > 0);
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
    int attackGold = std::max(0, gold + train_reserved - reserved_build);
    // attackCandidates는 위에서 이미 체력 내림차순으로 정렬돼 있고,
    // plan_attack_force가 그 순서 그대로 앞에서부터 neededExtra명을 썼을
    // 때 목표를 무너뜨릴 수 있다고 판단한 것이다. 그러니 실제 파병도 딱
    // 그만큼(가장 튼튼한 쪽부터)만 골라 쓴다.
    std::vector<const Warrior *> candidates(
        attackCandidates.begin(),
        attackCandidates.begin() +
            std::min<size_t>(neededExtra, attackCandidates.size()));
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
    if (stagingPoint != -1)
      for (const Warrior *w : candidates)
        if (w->region == stagingPoint) ++readyAtStaging;

    bool readyToCharge = (stagingPoint != -1 && readyAtStaging >= neededExtra);
    // 집결이 아직 안 끝났으면, 집결지 밖 인원은 딱 부족한 만큼만
    // (neededExtra - readyAtStaging) 더 불러모은다 — 이미 집결지에 있는
    // 인원 위에 추가로 neededExtra명을 또 채우면 총 인원이 넘쳐버린다.
    int gatherTarget = std::max(0, neededExtra - readyAtStaging);

    int sent = 0;
    for (const Warrior *w : candidates) {
      bool atStaging = (stagingPoint != -1 && w->region == stagingPoint);
      if (readyToCharge) {
        if (!atStaging) continue; // 이미 다 모였으니 추가 집결은 불필요
        if (sent >= neededExtra) break;
        if (attackGold < MOVE_COST) break;
        a.moves.push_back({w->id, currentTarget, WPurpose::ATTACK});
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
        int step = pickWaypoint(w->region, dest);
        bool stepIsMyBuilding = (bld[step] != nullptr && bld[step]->side == M.my_side);
        int cost = stepIsMyBuilding ? 0 : MOVE_COST;
        if (attackGold < cost) break;
        a.moves.push_back({w->id, step, WPurpose::ATTACK});
        attackGold -= cost;
        ++sent;
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
    int maintainWant = hqBehind ? 0
        : std::max({missing_workers, baseline_military,
                    extraToTrain > 0 ? extraToTrain : 0});
    int trainWant = std::max({maintainWant, gold_lead_military, strongholdTrainNeed});
    a.train_n = std::max(0, std::min({my_hq_train_cap(S, M), attackGold / TRAIN_COST, trainWant}));
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
    if (!worth_building_base(turn, 0)) continue;
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
    }
  }

  // missing_workers/baseline_military/cap은 buildNowCandidates 이전에 이미
  // 계산해 훈련 예산(train_reserved)에 반영해 뒀으므로 여기서는 그 값을
  // 그대로 재사용한다.
  gold += train_reserved;
  // 건설 대기 중인 유닛 몫의 건설비는 훈련에 끌어다 쓰지 않는다.
  gold = std::max(0, gold - reserved_build);

  int affordable = gold / TRAIN_COST;

  int want = std::max({missing_workers, baseline_military,
                       strongholdTrainNeed, gold_lead_military});

  // 사령부 레벨이 상대에게 밀리고 있으면 병력 생산도 멈추고 골드를
  // 사령부 업그레이드에 전부 남겨둔다(거점 업그레이드는 이미 waitForHqUpgrade가
  // 막아 두었다).
  a.train_n = hqBehind ? 0 : std::max(0, std::min({cap, affordable, want}));

  return a;
}
int main() {
  GameMap M;
  GameState S;
  parse_init(M, S);              
  Paths P = calculate_paths(M); 

  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }
  return 0;
}