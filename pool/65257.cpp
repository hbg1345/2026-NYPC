#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
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
// 이 유닛을 왜 보냈는지: 단순 이동/지원인지, 빈 거점 건설을 위한 파견인지
enum class WPurpose : int { NONE, MOVE, BUILD };

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

// 상대 사령부와 내 사령부를 잇는 직선으로부터 해당 거점까지의 수직 거리.
// 이 값이 작을수록(직선에 가까울수록) 진격로 상에 있는 "중요한" 거점으로 본다.
// 가중치는 게임 초반(turn 0)에는 0이었다가 턴이 진행될수록 선형으로 커진다:
// 초반엔 판도가 덜 정해져 있으니 그냥 가까운 거점부터 확장하고, 후반으로
// 갈수록 진격로 이탈을 더 크게 페널티 주어 결전 경로를 다지게 한다.
constexpr double LINE_IMPORTANCE_MAX_WEIGHT = 1.0;

static double dist_to_hq_line(const GameMap &M, int region) {
  double x1 = (double)M.x[M.my_hq], y1 = (double)M.y[M.my_hq];
  double x2 = (double)M.x[M.opp_hq], y2 = (double)M.y[M.opp_hq];
  double x0 = (double)M.x[region], y0 = (double)M.y[region];
  double dx = x2 - x1, dy = y2 - y1;
  double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-9)
    return 0.0;
  return std::fabs(dx * (y1 - y0) - (x1 - x0) * dy) / len;
}

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

  // 임의의 두 지점 사이 최단 hop 수
  auto hopsBetween = [&](int u, int v) -> int {
    if (u == v) return 0;
    int c = 0, cur = u;
    while (cur != v) {
      int nx = P.nxt[cur][v];
      if (nx < 0) return UNREACH;
      cur = nx;
      ++c;
    }
    return c;
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

// ================= ULTIMATE STRATEGY DECIDE FUNCTION =================
static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  Actions a;
  int gold = S.gold;
  const int N = M.N;

  // 턴 0에는 0, 후반으로 갈수록 선형으로 커지는 진격로 가중치
  double lineWeight = LINE_IMPORTANCE_MAX_WEIGHT * turn / (double)MAX_TURN;

  auto hops = [&](int u, int v) -> int {
    if (u == v) return 0;
    int c = 0;
    while (u != v) {
      int nx = P.nxt[u][v];
      if (nx < 0) return 9999;
      u = nx;
      ++c;
    }
    return c;
  };

  std::vector<int> myCnt(N, 0), oppCnt(N, 0);
  std::vector<const Building *> bld(N, nullptr);
  for (const auto &b : S.buildings) bld[b.region] = &b;
  for (const auto &w : S.warriors) (w.id.side == M.my_side ? myCnt : oppCnt)[w.region]++;
  std::vector<char> isStrong(N, 0);
  for (int s : M.strongholds) isStrong[s] = 1;

  int myBases = 0, oppBases = 0;
  for (const auto &b : S.buildings)
    if (b.type == BType::BASE) (b.side == M.my_side ? myBases : oppBases)++;

  // 사령부 레벨업은 공격 여부 판단보다 먼저 처리한다: 아래에서 총공세
  // 조건(total_offensive)이 성립하면 함수가 그 자리에서 바로 return 해버려서
  // 뒤쪽의 업그레이드 로직을 아예 못 타게 된다. 그러면 공격 가능한 턴마다
  // 사령부 레벨업 기회를 영영 놓치게 되므로, 공격 판단 전에 여유 골드로
  // 사령부부터 올릴 수 있으면 올려둔다.
  bool hqUpgradedThisTurn = false;
  bool hqBehind = false; // 상대 사령부보다 내 사령부 레벨이 낮은 상태
  int oppHqLevel = 1;    // 상대 사령부 레벨 추정치(증원 능력 계산에도 재사용)
  {
    const Building *myHq = nullptr;
    const Building *oppHq = nullptr;
    for (const auto &b : S.buildings) {
      if (b.side == M.my_side && b.type == BType::HQ) myHq = &b;
      if (b.side != M.my_side && b.type == BType::HQ) oppHq = &b;
    }
    oppHqLevel = (oppHq != nullptr) ? oppHq->level : 1;
    if (myHq != nullptr) hqBehind = (myHq->level < oppHqLevel);
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

  // 상대 이동 병력의 예상 목표 예측: 이번 턴에 실제로 한 칸 이동한 상대
  // 유닛에 대해, 그 이동 방향이 내 거점으로 가는 최단 경로의 다음 칸과
  // 일치하면 그 거점을 향한다고 간주한다. (여러 거점과 동시에 일치할 수
  // 있는데, 그 경우 전부 후보로 카운트 - 애매하면 과소평가보다 안전)
  std::vector<int> threat_count(N, 0);
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
    if (bestRegion != -1) ++threat_count[bestRegion];
  }

  // need[r]: r에 상시 배치해두어야 할 최소 병력. 평소엔 일자리(work_cap)만큼
  // 이지만, 예측된 공격이 있으면 터렛으로 못 막는 초과분만큼 방어 병력을
  // 추가로 요구한다. 이렇게 하면 기존 배치/훈련 로직(아래)이 방어 수요도
  // 최소 인원만 자연히 채우게 된다.
  std::vector<int> need(N, 0);
  for (int r = 0; r < N; ++r) {
    if (bld[r] != nullptr && bld[r]->side == M.my_side) {
      int turret = (bld[r]->type == BType::HQ) ? HQ_LEVELS[bld[r]->level].turret
                                                : BASE_LEVELS[bld[r]->level].turret;
      int defense_need = std::max(0, threat_count[r] - turret);
      need[r] = std::max(bld[r]->work_cap(), defense_need);
    }
    // 아직 건물은 없지만 이미 내 병력이 점거해 둔(상대는 없는) 거점: 공격을
    // 마치고 막 점령한 직후라 지금 당장 지을 돈은 없을 수 있다. 그렇다고
    // 바로 다음 공격지로 재배치하지 말고, 최소 1명은 남겨서 돈이 모일
    // 때까지 그 자리를 지키며 기다리게 한다(이 사람이 있어야 나중에
    // UPGRADE로 지을 수 있다).
    if (need[r] == 0 && isStrong[r] && bld[r] == nullptr && myCnt[r] > 0 &&
        oppCnt[r] == 0) {
      need[r] = 1;
    }
  }

  // 이미 내 병력이 점거한 빈 거점은 총공세 판단보다 먼저 건설을 시도한다.
  // total_offensive가 성립하면 이 아래로는 return으로 빠져나가 버리므로,
  // 공격 여부와 무관하게 지금 당장 지을 수 있는 거점은 먼저 지어 둔다.
  for (int r = 0; r < N; ++r) {
    if (!isStrong[r] || bld[r] != nullptr) continue;
    if (myCnt[r] == 0 || oppCnt[r] > 0) continue;
    if (!worth_building_base(turn, 0)) continue; // 남은 턴 수입이 건설비보다 싸면 포기
    if (gold >= BASE_LEVELS[1].cost) {
      a.upgrades.push_back(r);
      gold -= BASE_LEVELS[1].cost;
    }
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
    if (bld[t] == nullptr && buildIncoming[t] && !builtThisTurn[t]) {
      reserved_build += BASE_LEVELS[1].cost;
    }
  }

  std::vector<const Warrior *> idle;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && w.state == WState::STATIONARY) idle.push_back(&w);
  std::sort(idle.begin(), idle.end(), [](const Warrior *x, const Warrior *y) {
    return x->id.num < y->id.num;
  });

  // 각 거점에 이미 배치돼 있거나(homeCnt) 오는 중인(incoming) 병력 수.
  // 총공세 판단(방어 공백 감지)과 그 아래 재배치 로직 모두에 필요해서
  // 여기서 미리 계산해 둔다.
  std::vector<int> homeCnt(N, 0), incoming(N, 0);
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) continue;
    if (w.state == WState::MOVING) ++incoming[w.target];
    else ++homeCnt[w.region];
  }

  // 방어 공백 감지: need[r](일자리+예측된 위협 방어량)를 이미 배치된 인원과
  // 오는 중인 인원으로도 못 채우는 거점이 있으면 방어가 급한 상태로 본다.
  // 예전에는 이 상태에서도 총공세 조건(surplus>=required)이 그대로 참이면
  // 공격을 계속 내보내서, 공격 갔다 경유지에 멈춰 선 유닛들까지 idle이 되자마자
  // 다시 그대로 공격 후보로 쓸려 들어가 버렸다(=회군이 전혀 안 되는 문제).
  // 방어 공백이 있으면 총공세 자체를 끄고 정상 재배치 로직으로 넘겨서,
  // 이미 이동해 있던 유닛들도 다음번에 idle이 되는 즉시 방어로 돌려질 수
  // 있게 한다.
  bool underThreat = false;
  for (int r = 0; r < N; ++r) {
    if (bld[r] != nullptr && bld[r]->side == M.my_side &&
        homeCnt[r] + incoming[r] < need[r]) {
      underThreat = true;
      break;
    }
  }

  // 총공세 판단: 여러 날에 걸친 공성 시뮬레이션(breakDay) 대신, "나한테 가장
  // 가까운 상대 거점을 지금 공격하면 먹을 수 있는가"로 본다. 필요한 병력은
  // 빈 거점 공략과 같은 공식(수비대+1, 그 뒤로는 매일 자동으로 전투가
  // 이어져 결국 건물까지 뚫린다)이고, 동원 가능한 병력은 각 건물의 일자리/
  // 방어 수요(need)를 채우고 남는 유휴 병력의 합이다.
  int currentTarget = -1;
  bool total_offensive = false;
  if (!hqBehind && !underThreat) {
    // 사령부 레벨이 상대보다 낮으면 총공세는 아예 고려하지 않는다: 공격보다
    // 사령부 따라잡기가 먼저다(위쪽 hqUpgradedThisTurn 처리 및 아래
    // upgrade_budget 로직이 사령부 예산을 우선 확보한다).
    // 단순히 내 사령부에서 가장 가까운 상대 거점이 아니라, 두 사령부를 잇는
    // 진격로 위에 있을수록(직선거리가 작을수록) 중요도를 높여서 고른다.
    // 그래야 경로에서 살짝 벗어난 변두리 거점을 먼저 공략하느라 시간을
    // 낭비하지 않는다.
    double bestScore = std::numeric_limits<double>::infinity();
    for (const auto &b : S.buildings) {
      if (b.side == M.my_side) continue;
      int d = hops(M.my_hq, b.region);
      if (d == 9999) continue;
      double score = P.dist[M.my_hq][b.region] +
                     lineWeight * dist_to_hq_line(M, b.region);
      if (score < bestScore) { bestScore = score; currentTarget = b.region; }
    }
    if (currentTarget != -1) {
      int required = oppCnt[currentTarget] + 1;
      // 목표가 상대 진짜 사령부(넥서스)일 때만 증원 가능성을 따로 반영한다:
      // 지금 이 순간의 병력 수(oppCnt)만 보면, 내 공격이 도착하기까지 걸리는
      // 시간 동안 상대가 사령부에서 훈련해 그리로 보낼 수 있는 증원 물량을
      // 놓치게 된다. 일반 기지는 원래 근사(oppCnt+1)를 그대로 쓴다 — 어차피
      // 사령부만큼 오래 버티지도, 증원이 크게 의미 있지도 않기 때문.
      if (currentTarget == M.opp_hq) {
        // 훈련 한도만 무제한으로 채운다고 가정하면 상대 전력을 과대평가하게
        // 되므로, 실제로 그만큼 훈련할 골드가 있는지까지 같이 시뮬레이션한다
        // (assaultOutcome의 수비측 골드 시뮬레이션과 같은 방식: 추정 보유
        // 골드 S.opp_gold에서 시작해 매 턴 상대 수입만큼 쌓고, 훈련비+이동비를
        // 내면서 훈련 한도까지 뽑는다).
        int travelTurns = hops(M.my_hq, currentTarget);
        int distFromOppHQ = hops(M.opp_hq, currentTarget);
        int oppTrainCap = HQ_LEVELS[oppHqLevel].train_cap;
        int reinforceWindow = std::max(0, travelTurns - distFromOppHQ + 1);

        int oppAlive = 0;
        for (const auto &w : S.warriors)
          if (w.id.side != M.my_side) ++oppAlive;
        int oppBaseIncome = 0;
        for (const auto &b : S.buildings)
          if (b.side != M.my_side && b.type == BType::BASE)
            oppBaseIncome += WORK_INCOME * BASE_LEVELS[b.level].work_cap;
        int oppNetIncome = oppBaseIncome + WORK_INCOME * HQ_LEVELS[oppHqLevel].work_cap -
                           oppAlive * UPKEEP_PER_WARRIOR;

        long long oppPool = S.opp_gold;
        int reinforcements = 0;
        for (int d = 0; d < reinforceWindow; ++d) {
          oppPool += oppNetIncome;
          for (int k = 0; k < oppTrainCap && oppPool >= TRAIN_COST + MOVE_COST; ++k) {
            oppPool -= TRAIN_COST + MOVE_COST;
            ++reinforcements;
          }
        }
        required += reinforcements;
      }
      std::vector<int> idleCnt(N, 0);
      for (const Warrior *w : idle) ++idleCnt[w->region];
      int surplus = 0;
      for (int r = 0; r < N; ++r)
        if (bld[r] != nullptr && bld[r]->side == M.my_side)
          surplus += std::max(0, idleCnt[r] - need[r]);
      total_offensive = (surplus >= required);
    }
  }

  if (total_offensive) {
    // 건설 대기 중인 유닛 몫의 건설비는 총공세용 이동/훈련에 쓰지 않고
    // 남겨둔다(reserved_build는 위에서 이미 계산해 둠).
    int attackGold = std::max(0, gold - reserved_build);
    std::vector<int> kept(N, 0);
    std::vector<const Warrior *> candidates;
    for (const Warrior *w : idle) {
      int r = w->region;
      if (r == currentTarget) continue;
      if (w->purpose == WPurpose::BUILD && bld[r] == nullptr) continue; // 건설 대기 중인 유닛은 보존
      if (kept[r] < need[r]) { ++kept[r]; continue; }   // 노동 가능 인구 보존 (HQ 포함)
      candidates.push_back(w);
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Warrior *x, const Warrior *y) {
      return P.dist[x->region][currentTarget] < P.dist[y->region][currentTarget];
    });
    for (const Warrior *w : candidates) {
      // 공격 목표까지 한 번에 못 박지 않는다: 공격하러 간 사이에 본진이
      // 뚫릴 수 있으니, 그쪽으로 가까워지는 내 거점 중 가장 가까운 곳
      // (경유지)까지만 보낸다. 경유지는 아군 건물이라 도착하는 즉시 이동이
      // 끝나고 비용도 무료이며, 다음 턴에 방어가 급해지면(총공세 조건이
      // 깨지면) 이 유닛들이 그대로 방어 로직에 다시 잡혀서 돌아갈 수 있다.
      int dest = currentTarget;
      double waypoint_d = std::numeric_limits<double>::infinity();
      int waypoint = -1;
      for (const auto &b : S.buildings) {
        if (b.side != M.my_side || b.region == w->region) continue;
        if (P.dist[b.region][currentTarget] >= P.dist[w->region][currentTarget]) continue;
        if (P.dist[w->region][b.region] < waypoint_d) { waypoint_d = P.dist[w->region][b.region]; waypoint = b.region; }
      }
      if (waypoint != -1) dest = waypoint;

      int cost = (dest == currentTarget) ? MOVE_COST : 0;
      if (attackGold < cost) break;
      a.moves.push_back({w->id, dest, WPurpose::MOVE});
      attackGold -= cost;
    }
    a.train_n = std::max(0, std::min(my_hq_train_cap(S, M), attackGold / TRAIN_COST));
    return a;
  }


  int current_net_income = 0;
  int alive_w = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    int c = 0;
    for (const auto &w : S.warriors)
      if (w.id.side == M.my_side && w.region == b.region) ++c;
    current_net_income += WORK_INCOME * std::min(c, b.work_cap());
  }
  for (const auto &w : S.warriors) if (w.id.side == M.my_side) ++alive_w;
  current_net_income -= (alive_w * UPKEEP_PER_WARRIOR);

  // 빈 거점 확장은 "아무 유휴 병력"이 아니라 그 거점과 가장 가까운 아군
  // 거점(급양지, feedSrc)에서만 마지막 한 걸음을 보내게 한다. 그래야 사령부의
  // 유휴 인원이 (order상 먼저 처리된다는 이유만으로) 곧장 먼 변경까지
  // 직행하는 일이 없다. feedSrc가 그 확장 때문에 자기 노동 인구보다 부족해지면,
  // 그 부족분은 best_help 로직이 사령부 등에서 최단거리로 보충해 준다
  // (need가 아니라 need+pullNeed 기준으로 판단).
  std::vector<int> feedSrc(N, -1);
  std::vector<int> pullNeed(N, 0);
  if (myBases <= oppBases) {
    for (int t : M.strongholds) {
      if (bld[t] != nullptr) continue;
      int stillNeeded = oppCnt[t] + 1 - (homeCnt[t] + incoming[t]);
      if (stillNeeded <= 0) continue;
      int src = -1;
      double bestD = std::numeric_limits<double>::infinity();
      for (const auto &b : S.buildings) {
        if (b.side != M.my_side || b.region == t) continue;
        if (P.dist[b.region][t] < bestD) { bestD = P.dist[b.region][t]; src = b.region; }
      }
      if (src != -1) {
        feedSrc[t] = src;
        pullNeed[src] += stillNeeded;
      }
    }
  }
  // 어떤 거점이든 급양지(feedSrc)로 지정된 곳이면, 다른 급양지의
  // pullNeed에 이끌려 자기 자리를 비우지 않는다. 그러지 않으면 서로 다른
  // 목표를 먹이는 두 급양지가 "네가 부족해 보이니 내가 간다"를 주고받으며
  // 영원히 왔다 갔다 하는 순환이 생긴다(사령부<->거점 무한 왕복 버그).
  std::vector<char> isFeedSrc(N, 0);
  for (int t : M.strongholds)
    if (feedSrc[t] != -1) isFeedSrc[feedSrc[t]] = 1;

  std::vector<int> kept(N, 0);
  int train_reserved = 0;
  int available_gold = gold - reserved_build; // 다음 유닛 판단 기준이 될 실시간 가용 예산 (기존 부채 차감)

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
    double min_help_d = std::numeric_limits<double>::infinity();
    // 내가 서 있는 곳(r) 자체가 다른 목표의 급양지라면, 여기서 다른 급양지의
    // pullNeed에 이끌려 자리를 비우지 않는다(순수 need[t]만 본다). 그래야
    // 급양지끼리 서로를 끌어당기는 무한 왕복이 생기지 않는다.
    bool ignorePull = isFeedSrc[r];
    for (int t = 0; t < N; ++t) {
      if (t == r) continue;
      // need[t] 외에 pullNeed[t](이 거점을 급양지 삼아 빈 거점으로 계속
      // 보내야 하는 몫)까지 채워야 할 기준으로 삼는다.
      int threshold = need[t] + (ignorePull ? 0 : pullNeed[t]);
      if (bld[t] != nullptr && bld[t]->side == M.my_side &&
          (homeCnt[t] + incoming[t] < threshold)) {
        if (P.dist[r][t] < min_help_d) { min_help_d = P.dist[r][t]; best_help = t; }
      }
    }

    int best_strong = -1;
    double min_strong_d = std::numeric_limits<double>::infinity();
    // 이미 상대보다 거점 수가 앞서 있으면 더 이상 확장하지 않고
    // 골드를 업그레이드(레벨업)로 돌린다
    if (myBases <= oppBases) {
      for (int t : M.strongholds) {
        if (t == r) continue;
        // 빈 거점은 건물이 없어 터렛이 없으므로, 상대 병력이 지키고 있어도
        // 그보다 한 명 더 많이 보내면 이길 수 있다. 다 보낼 때까지
        // (homeCnt+incoming이 oppCnt+1에 도달할 때까지) 계속 후보로 남긴다.
        if (bld[t] == nullptr && (homeCnt[t] + incoming[t] < oppCnt[t] + 1)) {
          // 이 거점으로의 마지막 한 걸음은 지정된 급양지(feedSrc)에서만
          // 보낸다. 다른 곳(예: 사령부)에 있는 유휴 병력은 여기서 곧장
          // 이 거점으로 가지 않고, 대신 위 best_help가 급양지 보충으로
          // 데려간다.
          if (feedSrc[t] != r) continue;
          // 도착까지 걸리는 턴만큼 늦게 지어지는 셈이니, 그 시점부터 게임
          // 종료까지 남은 기간의 예상 수입이 건설비보다 적으면 애초에 후보로
          // 고려하지 않는다(막판에 새 거점을 지어봤자 투자를 회수 못 함).
          int dist_turns = hops(r, t);
          if (!worth_building_base(turn, dist_turns)) continue;
          // 주인 없는(건물 없는) 거점은 진격로 가중치 없이 순수 최단거리
          // 우선으로 고른다. 진격로 가중치는 실제로 상대가 점유한 거점을
          // 공격할 때(total_offensive)만 의미가 있다.
          if (P.dist[r][t] < min_strong_d) { min_strong_d = P.dist[r][t]; best_strong = t; }
        }
      }
    }

    int best_target = -1;
    if (best_help != -1) {
      best_target = best_help;
    } else if (best_strong != -1) {
      int dist_turns = hops(r, best_strong);
      int base_cost = BASE_LEVELS[1].cost + MOVE_COST - (current_net_income * dist_turns);

      if (kept[r] >= need[r]) {
        int req_gold = std::max(MOVE_COST, base_cost);
        if (available_gold >= req_gold) best_target = best_strong;
      } else {
        int req_gold = std::max(TRAIN_COST + MOVE_COST, base_cost + TRAIN_COST);
        if (available_gold >= req_gold) best_target = best_strong;
      }
    }

    if (best_target == -1) {
      if (kept[r] < need[r]) ++kept[r];
      continue;
    }

    // best_help의 목적지는 항상 아군 건물이라 이동 비용이 무료지만,
    // best_strong의 목적지는 아직 건물이 없는 거점이라 실제로 비용이 든다.
    int move_cost_now = (best_target == best_strong) ? MOVE_COST : 0;

    // 목적지가 예측(threat_count) 기반 방어 수요 때문에 골라진 경우, 예측이
    // 틀리면 그 병력을 되돌릴 수 없다는 위험이 있다. 그래서 목적지까지 한
    // 번에 못 박지 않고, 그쪽으로 가까워지는 내 거점 중 출발지에서 가장
    // 가까운 곳(경유지)까지만 보낸다. 경유지는 아군 건물이라 도착하는 즉시
    // 이동이 끝나므로, 다음 턴에 최신 위협 판단으로 다시 갈 곳을 정할 수
    // 있다. 순수 일자리 수요(threat_count==0)나 확장(best_strong)에는 예측
    // 리스크가 없으므로 이 우회를 적용하지 않는다.
    int move_target = best_target;
    if (best_target == best_help && threat_count[best_help] > 0) {
      int waypoint = -1;
      double waypoint_d = std::numeric_limits<double>::infinity();
      for (const auto &b : S.buildings) {
        if (b.side != M.my_side || b.region == r) continue;
        if (P.dist[b.region][best_help] >= P.dist[r][best_help]) continue;
        if (P.dist[r][b.region] < waypoint_d) { waypoint_d = P.dist[r][b.region]; waypoint = b.region; }
      }
      if (waypoint != -1) move_target = waypoint;
    }

    if (available_gold >= move_cost_now) {
      bool needs_replacement = (kept[r] < need[r]);
      WPurpose purpose = (best_target == best_strong) ? WPurpose::BUILD : WPurpose::MOVE;

      a.moves.push_back({w->id, move_target, purpose});
      gold -= move_cost_now;
      available_gold -= move_cost_now;
      --homeCnt[r];
      ++incoming[move_target];

      if (needs_replacement) {
        gold -= TRAIN_COST;
        available_gold -= TRAIN_COST;
        train_reserved += TRAIN_COST;
      }

      // 목적지가 빈 거점인 경우 미래 건설비를 차감하여 중복 파견을 방지함
      if (isStrong[best_target] && bld[best_target] == nullptr) {
        int dist_turns = hops(r, best_target);
        int future_cost = std::max(0, BASE_LEVELS[1].cost - (current_net_income * dist_turns));
        available_gold -= future_cost;
      }
    }
  }

  bool has_empty_strong = false;
  for (int t : M.strongholds) {
    if (bld[t] == nullptr && oppCnt[t] == 0) {
      has_empty_strong = true;
      break;
    }
  }

  int upgrade_budget = std::max(0, gold - reserved_build);

  if (myBases <= oppBases && has_empty_strong && !hqBehind) {
    // 게임 초반엔 거점 확장에 예산을 몰아주려고 업그레이드 예산을 잠그지만,
    // 턴이 진행될수록 이 잠금을 서서히 풀어준다: 남은 턴이 줄어들수록 확장
    // 투자를 회수할 시간이 부족해지는 반면, 마지막 결전은 결국 사령부 레벨
    // (전사 체력/훈련량/포탑)이 좌우하기 때문이다. 업그레이드 후보는 사령부가
    // 항상 거리 0으로 최우선이므로, 풀린 예산은 자연히 사령부부터 채운다.
    // 단, 사령부가 이미 상대보다 뒤처져 있으면(hqBehind) 이 잠금 자체를
    // 걸지 않고 예산 전액을 사령부 추격에 쓸 수 있게 한다.
    double lateness = (double)turn / MAX_TURN;
    upgrade_budget = (int)(upgrade_budget * lateness);
  }

  // 사령부가 이미 만렙이면 기지 업그레이드보다 병력 생산이 우선이다. 다만
  // 기지 업그레이드를 아예 막는 게 아니라, 최대로 훈련할 수 있는 만큼의
  // 비용을 먼저 떼어놓고 그러고도 남는 돈만 기지 업그레이드에 쓰게 한다.
  int myHqLevel = 1;
  for (const auto &b : S.buildings)
    if (b.side == M.my_side && b.type == BType::HQ) myHqLevel = b.level;
  bool hqMaxed = (myHqLevel >= HQ_MAX_LEVEL);
  if (hqMaxed) {
    int train_budget_needed = my_hq_train_cap(S, M) * TRAIN_COST;
    upgrade_budget = std::max(0, upgrade_budget - train_budget_needed);
  }

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

  // 기지 업그레이드는 사령부 레벨업 타이밍을 늦추지 않을 때만 한다: 사령부가
  // 지금 당장 감당되면(upgrade_budget >= hq_cost) 아래 루프에서 사령부부터
  // 사고 남는 돈만 기지에 쓰므로 사령부가 늦어질 일이 없다. 하지만 사령부가
  // 아직 못 미치는 상황이라면, 여기서 기지에 조금이라도 쓰는 순간 그만큼
  // 사령부를 살 수 있는 시점이 뒤로 밀린다. 그래서 예전처럼 lateness 비율만큼만
  // 저축하는 게 아니라, 사령부를 살 수 있을 때까지는 예산을 100% 저축하고
  // 기지 업그레이드는 아예 하지 않는다.
  // (이미 이번 턴에 사령부를 올렸다면 더 저축할 필요가 없으므로 건너뛴다)
  if (!hqUpgradedThisTurn) {
    for (const Building *b : upgradeCandidates) {
      if (b->type != BType::HQ) continue;
      int hq_cost = HQ_LEVELS[b->level + 1].upgrade_cost;
      if (upgrade_budget < hq_cost) {
        upgrade_budget = 0;
      }
      break;
    }
  }

  for (const Building *b : upgradeCandidates) {
    int c = (b->type == BType::HQ) ? HQ_LEVELS[b->level + 1].upgrade_cost
                                    : BASE_LEVELS[b->level + 1].cost;
    if (myCnt[b->region] > 0 && oppCnt[b->region] == 0 && upgrade_budget >= c) {
      a.upgrades.push_back(b->region);
      upgrade_budget -= c;
      gold -= c;
    }
  }

  int missing_workers = 0;
  for (int r = 0; r < N; ++r) {
    if (bld[r] != nullptr && bld[r]->side == M.my_side) {
      int staffed = homeCnt[r] + incoming[r];
      if (staffed < need[r]) {
        missing_workers += (need[r] - staffed); 
      }
    }
  }

  gold += train_reserved;
  // 건설 대기 중인 유닛 몫의 건설비는 훈련에 끌어다 쓰지 않는다.
  gold = std::max(0, gold - reserved_build);

  int cap = static_cast<int>(my_hq_train_cap(S, M));
  int affordable = gold / TRAIN_COST;

  // 건물 일자리/방어 수요만 채우고 끝내지 않는다: 상대 총공세를 지금
  // 전력으로 못 막아낸다면(assaultOutcome 기준) 막아낼 수 있게 되는 최소
  // 병력까지 훈련 목표에 넣는다.
  int missing_military = min_defenders_needed(S, M, P, turn, cap);

  // 평소 유지 병력 규모: 내 거점 수가 상대보다 많으면(수입 우위가 있으면)
  // 상대보다 조금 더 많이 유지하고(공세 전환 여력 확보), 그렇지 않으면
  // 상대 수만큼만 맞춰서 과잉 생산(=낭비되는 유지비)을 피한다.
  int myWarCount = 0, oppWarCount = 0;
  for (const auto &w : S.warriors) (w.id.side == M.my_side ? myWarCount : oppWarCount)++;
  int baseline_target = (myBases > oppBases) ? (oppWarCount + 1) : oppWarCount;
  int baseline_military = std::max(0, baseline_target - myWarCount);

  int want = std::max({missing_workers, missing_military, baseline_military});

  // 사령부가 만렙이면 더 올릴 게 없으니, 남는 골드는 기지 업그레이드 대신
  // 병력 생산에 최우선으로 쏟아붓는다(훈련 가능한 만큼 최대로 뽑는다).
  if (hqMaxed) want = cap;

  a.train_n = std::max(0, std::min({cap, affordable, want}));

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