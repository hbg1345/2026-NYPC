#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

constexpr int MAX_TURN = 400;         // maximum turn (days)
constexpr int START_GOLD = 750;       // initial gold
constexpr int START_WARRIORS = 3;     // initial warriors
constexpr int MOVE_COST = 10;         // move cost
constexpr int TRAIN_COST = 120;       // train cost
constexpr int WORK_INCOME = 15;       // income per warrior
constexpr int UPKEEP_PER_WARRIOR = 2; // upkeep per warrior
constexpr int HQ_MAX_LEVEL = 5;       // HQ max level
constexpr int BASE_MAX_LEVEL = 3;     // base max level
constexpr int HQ_HEAL_COST = 1000;    // HQ fix cost
constexpr int BASE_HEAL_COST = 500;   // base fix cost
constexpr int HOP_VISION = 2;         // vision radius shared by all units

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
    {1000, 6, 20, 2, 2, 3}, {2000, 7, 25, 3, 2, 4}, {3000, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {500, 6, 1, 1},
    {550, 12, 1, 2},
    {600, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };
enum class WPurpose : int {
  NONE,
  BUILD,
  DEFEND,
  ATTACK,
  SCOUT,
  HQ_SCOUT,
  OFFENSE_SCOUT,
  VISION_SCOUT
};

inline Side opposite(Side s) {
  return s == Side::LEFT ? Side::RIGHT : Side::LEFT;
}
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) {
  return c == 'A' ? Side::LEFT : Side::RIGHT;
}
inline bool is_scout_purpose(WPurpose purpose) {
  return purpose == WPurpose::SCOUT || purpose == WPurpose::HQ_SCOUT ||
         purpose == WPurpose::OFFENSE_SCOUT ||
         purpose == WPurpose::VISION_SCOUT;
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
  WPurpose purpose = WPurpose::NONE;
};

struct EnemyWarriorIntel {
  WarriorId id;
  int region = 0;
  int hp = 0;
  int last_seen_turn = -1;
  int last_event_turn = 0;
  bool visible = false;
  int previous_region = 0;
  int last_move_turn = -1;
};

struct Building {
  int region = 0;
  Side side = Side::LEFT;
  BType type = BType::HQ;
  int level = 1;
  int hp = 10;
  int last_seen_turn = 0;

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
  int gold = START_GOLD;
  long long opp_gold_upper = START_GOLD;
  int my_countdown = 5;
  int opp_countdown = 5;
  int turn = 0;
  std::vector<Warrior> warriors;
  std::vector<Building> buildings;
  std::vector<int> visible;
  std::vector<int> last_seen_region_turn;
  std::vector<EnemyWarriorIntel> enemy_memory;
  std::vector<int> enemy_region_count_memory;
  std::vector<int> enemy_region_count_turn;
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

struct StrategyState {
  int roaming_patrol_start_turn = -1;
  std::vector<WarriorId> roaming_tracked_ids;
  int offense_probe_start_turn = -1;
  int offense_probe_complete_turn = -1;
  std::vector<int> offense_probe_regions;
  std::vector<int> offense_probe_seen_turn;
  std::vector<std::pair<WarriorId, int>> vision_posts;
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

// Replay-only decision log.  Submission/default builds keep this at zero;
// compile with -DENABLE_DEBUG_LOG=1 to create debug_A.txt or debug_B.txt.
#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif
namespace dbg {
constexpr bool ENABLED = ENABLE_DEBUG_LOG != 0;
static std::ofstream log_file;

static const char *purpose_name(WPurpose purpose) {
  switch (purpose) {
  case WPurpose::BUILD:
    return "BUILD";
  case WPurpose::DEFEND:
    return "DEFEND";
  case WPurpose::ATTACK:
    return "ATTACK";
  case WPurpose::SCOUT:
    return "SCOUT";
  case WPurpose::HQ_SCOUT:
    return "HQ_SCOUT";
  case WPurpose::OFFENSE_SCOUT:
    return "OFFENSE_SCOUT";
  case WPurpose::VISION_SCOUT:
    return "VISION_SCOUT";
  default:
    return "NONE";
  }
}

static void init(Side side) {
  if (!ENABLED)
    return;
  std::string path = std::string("debug_") + side_char(side) + ".txt";
  log_file.open(path, std::ios::out | std::ios::trunc);
}

static void note(int turn, const std::string &message) {
  if (!log_file.is_open())
    return;
  log_file << 'T' << turn << "  " << message << '\n';
  log_file.flush();
}

static void turn_header(int turn, const GameState &S, const GameMap &M) {
  if (!log_file.is_open())
    return;
  int mine = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++mine;
  log_file << "===== TURN " << turn << " | gold=" << S.gold
           << " opp_gold_upper=" << S.opp_gold_upper
           << " my_alive=" << mine
           << " enemy_memory=" << S.enemy_memory.size() << " =====\n";
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.purpose == WPurpose::NONE)
      continue;
    log_file << "  " << format_warrior(w.id) << " R" << w.region
             << " hp=" << w.hp
             << " state="
             << (w.state == WState::STATIONARY ? "STOP" : "MOVING")
             << " target=R" << w.target
             << " purpose=" << purpose_name(w.purpose) << '\n';
  }
  const bool enemy_hq_visible =
      std::find(S.visible.begin(), S.visible.end(), M.opp_hq) !=
      S.visible.end();
  int observed_enemy = 0;
  int observed_enemy_at_hq = 0;
  int remembered_enemy_in_visible_regions = 0;
  int remembered_enemy_at_hq = 0;
  for (const auto &intel : S.enemy_memory) {
    if (intel.visible) {
      ++observed_enemy;
      if (intel.region == M.opp_hq)
        ++observed_enemy_at_hq;
    }
    if (intel.region == M.opp_hq)
      ++remembered_enemy_at_hq;
    if (std::find(S.visible.begin(), S.visible.end(), intel.region) !=
        S.visible.end())
      ++remembered_enemy_in_visible_regions;
  }
  int hq_scout_region = -1;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.purpose == WPurpose::HQ_SCOUT) {
      hq_scout_region = w.region;
      break;
    }
  }
  log_file << 'T' << turn
           << "  VIEW enemy_gold_upper=" << S.opp_gold_upper
           << " enemy_hq_visible=" << (enemy_hq_visible ? 1 : 0)
           << " observed_enemy=" << observed_enemy
           << " observed_enemy_at_hq=" << observed_enemy_at_hq
           << " remembered_enemy=" << S.enemy_memory.size()
           << " remembered_enemy_in_visible_regions="
           << remembered_enemy_in_visible_regions
           << " remembered_enemy_at_hq=" << remembered_enemy_at_hq
           << " hq_scout_region=" << hq_scout_region << '\n';
  for (const auto &intel : S.enemy_memory) {
    log_file << 'T' << turn << "  ENEMY_UNIT id="
             << format_warrior(intel.id) << " region=" << intel.region
             << " hp=" << intel.hp << " visible=" << (intel.visible ? 1 : 0)
             << " last_seen=" << intel.last_seen_turn
             << " last_event=" << intel.last_event_turn << '\n';
  }
  log_file.flush();
}
} // namespace dbg

static int hq_of(const GameMap &M, Side s) {
  return (s == Side::LEFT) ? 0 : M.N - 1;
}

static Building make_base(int region, Side s, int turn = 0) {
  return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp, turn};
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

static std::vector<int> compute_visible(const GameState &S, const GameMap &M) {
  std::vector<char> visible(M.N, false);
  auto add_hops = [&](int start, int radius) {
    std::vector<char> seen(M.N, false);
    std::vector<int> frontier{start};
    seen[start] = true;
    visible[start] = true;
    for (int hop = 0; hop < radius; ++hop) {
      std::vector<int> next;
      for (int region : frontier) {
        for (int neighbor : M.adj[region]) {
          if (!seen[neighbor]) {
            seen[neighbor] = true;
            visible[neighbor] = true;
            next.push_back(neighbor);
          }
        }
      }
      frontier = std::move(next);
    }
  };
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      add_hops(w.region, HOP_VISION);
  for (const auto &b : S.buildings)
    if (b.side == M.my_side)
      add_hops(b.region, HOP_VISION);

  std::vector<int> result;
  for (int region = 0; region < M.N; ++region)
    if (visible[region])
      result.push_back(region);
  return result;
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
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx},
                                 .region = M.my_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx},
                                 .region = M.opp_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.enemy_memory.push_back(
        EnemyWarriorIntel{WarriorId{opp, sfx}, M.opp_hq,
                          HQ_LEVELS[1].warrior_hp, -1, 0, false});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1,
                                 HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ,
                                 1, HQ_LEVELS[1].hp});
  S.visible = compute_visible(S, M);
  S.last_seen_region_turn.assign(M.N, -1);
  S.enemy_region_count_memory.assign(M.N, 0);
  S.enemy_region_count_turn.assign(M.N, -1);
  S.enemy_region_count_memory[M.opp_hq] = START_WARRIORS;
  S.enemy_region_count_turn[M.opp_hq] = 0;
  for (int r : S.visible)
    S.last_seen_region_turn[r] = 0;

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

static const Building *find_building(const GameState &S, int region) {
  for (const auto &b : S.buildings)
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

static EnemyWarriorIntel *find_enemy_intel(GameState &S, WarriorId id) {
  for (auto &w : S.enemy_memory)
    if (w.id == id)
      return &w;
  return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M,
                             const Actions &submitted) {
  const Side enemy_side = opposite(M.my_side);
  bool enemy_hunger_seen = false;
  // Reserve the gold in the same phase order as the engine.  State mutations
  // themselves are applied only from the authoritative result sections below;
  // doing both would advance an existing building two levels for one command.
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) {
      S.gold -= BASE_LEVELS[1].cost;
    } else if (b->level >= max_level(*b)) {
      S.gold -= (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
    } else {
      S.gold -= upgrade_cost(*b);
    }
  }

  const auto &built = submitted.upgrades;

  for (const auto &move : submitted.moves) {
    Building *b = find_building(S, move.target);
    bool own = std::find(built.begin(), built.end(), move.target) != built.end() ||
               (b != nullptr && b->side == M.my_side);
    int cost = own ? 0 : MOVE_COST;
    S.gold -= cost;
    if (Warrior *w = find_warrior(S, move.id)) {
      w->state = WState::MOVING;
      w->target = move.target;
      w->purpose = move.purpose;
    }
  }

  S.gold -= TRAIN_COST * submitted.train_n;

  {
    std::string line = readln();
    if (line == "FINISH")
      std::exit(0);
    auto t = tokens(line);
    assert(!t.empty() && t[0] == "TURN");
    S.turn = std::stoi(t.at(1));
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
      Side owner = parse_side_char(r.at(0)[0]);
      int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (owner == enemy_side) {
        int price = BASE_LEVELS[1].cost;
        if (b != nullptr) {
          if (b->level >= max_level(*b))
            price = b->type == BType::HQ ? HQ_HEAL_COST : BASE_HEAL_COST;
          else
            price = upgrade_cost(*b);
        }
        S.opp_gold_upper = std::max(0LL, S.opp_gold_upper - price);
      }
      if (b == nullptr) {
        S.buildings.push_back(make_base(region, owner, S.turn));
      } else if (b->level >= max_level(*b)) {
        b->hp = b->current_hp();
        b->last_seen_turn = S.turn;
      } else {
        apply_upgrade(*b);
        b->last_seen_turn = S.turn;
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
        int spawn_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, spawn_region);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id,
                                     .region = spawn_region,
                                     .hp = HQ_LEVELS[hq_level].warrior_hp});
        if (id.side == enemy_side) {
          S.opp_gold_upper =
              std::max(0LL, S.opp_gold_upper - TRAIN_COST);
          EnemyWarriorIntel *intel = find_enemy_intel(S, id);
          if (intel == nullptr) {
            S.enemy_memory.push_back(EnemyWarriorIntel{
                id, spawn_region, HQ_LEVELS[hq_level].warrior_hp, -1,
                S.turn, false});
          } else {
            intel->region = spawn_region;
            intel->hp = HQ_LEVELS[hq_level].warrior_hp;
            intel->last_event_turn = S.turn;
          }
        }
      }
    }
  }
  // MOVE
  {
    auto t = tokens(readln()); // "MOVE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(0));
      int region = std::stoi(r.at(1));
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region;
        if (w->state == WState::MOVING && w->region == w->target) {
          w->state = WState::STATIONARY;
          if (w->purpose != WPurpose::BUILD &&
              !is_scout_purpose(w->purpose))
            w->purpose = WPurpose::NONE;
        }
      }
      if (id.side == enemy_side) {
        if (EnemyWarriorIntel *intel = find_enemy_intel(S, id)) {
          intel->previous_region = intel->region;
          intel->region = region;
          intel->last_event_turn = S.turn;
          intel->last_move_turn = S.turn;
        }
      }
    }
  }
  // DAMAGE
  {
    auto t = tokens(readln()); // "DAMAGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<cause> <id> <damage>"
      WarriorId id = parse_warrior(r.at(1));
      int damage = std::stoi(r.at(2));
      if (id.side == enemy_side && r.at(0) == "HUNGER")
        enemy_hunger_seen = true;
      if (Warrior *w = find_warrior(S, id))
        w->hp -= damage;
      if (id.side == enemy_side) {
        if (EnemyWarriorIntel *intel = find_enemy_intel(S, id)) {
          intel->hp -= damage;
          intel->last_event_turn = S.turn;
        }
      }
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [](const Warrior &w) { return w.hp <= 0; }),
                     S.warriors.end());
    S.enemy_memory.erase(
        std::remove_if(S.enemy_memory.begin(), S.enemy_memory.end(),
                       [](const EnemyWarriorIntel &w) { return w.hp <= 0; }),
        S.enemy_memory.end());
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
  S.visible = compute_visible(S, M);
  for (int r : S.visible)
    S.last_seen_region_turn[r] = S.turn;
  // WARRIOR
  {
    for (auto &intel : S.enemy_memory)
      intel.visible = false;
    auto t = tokens(readln()); // "WARRIOR W"
    int w_count = std::stoi(t.at(1));
    std::vector<Warrior> snapshot;
    snapshot.reserve(w_count);
    for (int i = 0; i < w_count; ++i) {
      auto r = tokens(readln()); // "<id> <region> <hp>"
      WarriorId id = parse_warrior(r.at(0));
      Warrior now{.id = id,
                  .region = std::stoi(r.at(1)),
                  .hp = std::stoi(r.at(2))};
      if (Warrior *old = find_warrior(S, id)) {
        now.state = old->state;
        now.target = old->target;
        now.purpose = old->purpose;
        if (now.state == WState::MOVING && now.region == now.target) {
          now.state = WState::STATIONARY;
          if (now.purpose != WPurpose::BUILD &&
              !is_scout_purpose(now.purpose))
            now.purpose = WPurpose::NONE;
        }
      }
      if (id.side == enemy_side) {
        EnemyWarriorIntel *intel = find_enemy_intel(S, id);
        if (intel == nullptr) {
          S.opp_gold_upper =
              std::max(0LL, S.opp_gold_upper - TRAIN_COST);
          S.enemy_memory.push_back(
              EnemyWarriorIntel{id, now.region, now.hp, S.turn, S.turn, true});
        } else {
          // Official finals results report MOVE for our units only.  Recover
          // an enemy's one-edge movement from two consecutive visibility
          // snapshots instead: this is the fog-of-war equivalent of the
          // qualifier model's prev_region -> region tracking.  Do not infer a
          // direction across a visibility gap, since the unit may have taken
          // several edges while unseen.
          const int previous_seen_region = intel->region;
          const bool continuously_seen =
              intel->last_seen_turn == S.turn - 1;
          const bool moved_one_edge =
              previous_seen_region >= 0 && previous_seen_region < M.N &&
              previous_seen_region != now.region &&
              std::binary_search(M.adj[previous_seen_region].begin(),
                                 M.adj[previous_seen_region].end(),
                                 now.region);
          if (continuously_seen && moved_one_edge) {
            intel->previous_region = previous_seen_region;
            intel->last_move_turn = S.turn;
          }
          intel->region = now.region;
          intel->hp = now.hp;
          intel->last_seen_turn = S.turn;
          intel->last_event_turn = S.turn;
          intel->visible = true;
        }
      }
      snapshot.push_back(now);
    }
    for (int region : S.visible) {
      int enemy_count = 0;
      for (const auto &w : snapshot)
        if (w.id.side == enemy_side && w.region == region)
          ++enemy_count;
      S.enemy_region_count_memory[region] = enemy_count;
      S.enemy_region_count_turn[region] = S.turn;
    }
    // Every own warrior is always in our vision.  Enemy warriors, on the other
    // hand, deliberately remain a current-visibility snapshot instead of
    // becoming falsely precise ghosts in fog of war.
    S.warriors = std::move(snapshot);
  }
  // BUILDING
  {
    auto t = tokens(readln()); // "BUILDING B"
    int b_count = std::stoi(t.at(1));
    std::vector<Building> observed;
    observed.reserve(b_count);
    for (int i = 0; i < b_count; ++i) {
      auto r = tokens(readln()); // "<side> <region> <kind> <level> <hp>"
      Side s = parse_side_char(r.at(0)[0]);
      BType bt = (r.at(2) == "HQ") ? BType::HQ : BType::BASE;
      const int region = std::stoi(r.at(1));
      const int level = std::stoi(r.at(3));
      if (s == enemy_side) {
        const Building *old = find_building(S, region);
        int first_unpaid_level = 1;
        if (old != nullptr)
          first_unpaid_level = old->level + 1;
        for (int new_level = first_unpaid_level; new_level <= level;
             ++new_level) {
          const int price = bt == BType::HQ
                                ? HQ_LEVELS[new_level].upgrade_cost
                                : BASE_LEVELS[new_level].cost;
          S.opp_gold_upper = std::max(0LL, S.opp_gold_upper - price);
        }
      }
      observed.push_back(
          Building{region, s, bt, level, std::stoi(r.at(4)), S.turn});
    }

    std::vector<char> is_visible(M.N, false);
    for (int r : S.visible)
      is_visible[r] = true;

    std::vector<Building> merged;
    // Enemy buildings are stationary, so a last-seen record remains useful in
    // fog.  Delete it only when its region is visible and the snapshot proves
    // that the building is gone.  Own buildings are always observed directly.
    for (const auto &old : S.buildings) {
      if (old.side != M.my_side && !is_visible[old.region])
        merged.push_back(old);
    }
    for (const auto &b : observed)
      merged.push_back(b);
    S.buildings = std::move(merged);
  }
  (void)readln(); // "END"

  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int count = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side && w.region == b.region)
        ++count;
    }
    income += WORK_INCOME * std::min(count, b.work_cap());
  }
  S.gold += income;

  int alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++alive;
  S.gold -=
      UPKEEP_PER_WARRIOR * std::min(alive, S.gold / UPKEEP_PER_WARRIOR);

  // Opponent gold is maintained as a conservative upper estimate.  Public
  // construction and training costs are exact; new MOVE command costs are
  // intentionally ignored because their hidden destinations can make them
  // free.  Scouting corrects the remembered worker count, and uncertainty then
  // grows by at most one possible arriving worker per unseen turn.
  int estimated_enemy_workers = 0;
  for (const auto &b : S.buildings) {
    if (b.side != enemy_side)
      continue;
    const int age = S.enemy_region_count_turn[b.region] < 0
                        ? S.turn + 1
                        : S.turn - S.enemy_region_count_turn[b.region];
    const int possible_workers =
        S.enemy_region_count_memory[b.region] + std::max(0, age);
    estimated_enemy_workers += std::min(possible_workers, b.work_cap());
  }
  const int estimated_enemy_alive = (int)S.enemy_memory.size();
  estimated_enemy_workers =
      std::min(estimated_enemy_workers, estimated_enemy_alive);
  S.opp_gold_upper += (long long)WORK_INCOME * estimated_enemy_workers;
  S.opp_gold_upper -= std::min<long long>(
      S.opp_gold_upper,
      (long long)UPKEEP_PER_WARRIOR * estimated_enemy_alive);
  if (enemy_hunger_seen)
    S.opp_gold_upper = std::min(S.opp_gold_upper, 1LL);

}

struct Paths {
  std::vector<std::vector<double>> dist;
  std::vector<std::vector<int>> nxt;
  std::vector<std::vector<int>> hops;
  std::vector<std::vector<int>> vision_hops;
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
  P.hops.assign(M.N, std::vector<int>(M.N, 1000000000));
  P.vision_hops.assign(M.N, std::vector<int>(M.N, 1000000000));

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
  for (int u = 0; u < M.N; ++u) {
    P.hops[u][u] = 0;
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.nxt[u][v] == -1)
        continue;
      int cur = u;
      int count = 0;
      while (cur != v && count <= M.N) {
        cur = P.nxt[cur][v];
        ++count;
      }
      if (cur == v)
        P.hops[u][v] = count;
    }
  }

  // Movement follows the weighted shortest path above, while vision is based
  // on plain graph distance.  Keep both metrics so scouts can stop two graph
  // edges away from an objective without entering a defended region.
  for (int source = 0; source < M.N; ++source) {
    std::vector<int> queue{source};
    P.vision_hops[source][source] = 0;
    for (int head = 0; head < (int)queue.size(); ++head) {
      int u = queue[head];
      for (int v : M.adj[u]) {
        if (P.vision_hops[source][v] != 1000000000)
          continue;
        P.vision_hops[source][v] = P.vision_hops[source][u] + 1;
        queue.push_back(v);
      }
    }
  }
  return P;
}

static void emit_command() { std::cout << "COMMAND\n"; }

static void emit_actions(const Actions &a) {
  for (const auto &move : a.moves) {
    std::cout << "MOVE " << format_warrior(move.id) << ' ' << move.target << '\n';
  }
  for (int r : a.upgrades) {
    std::cout << "UPGRADE " << r << '\n';
  }
  if (a.train_n > 0) {
    std::cout << "TRAIN " << a.train_n << '\n';
  }
}

static void emit_end() { std::cout << "END" << std::endl; }

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////

static int building_turret(const Building &b) {
  return b.type == BType::HQ ? HQ_LEVELS[b.level].turret
                             : BASE_LEVELS[b.level].turret;
}

struct SimWarrior {
  int hp;
  int num;
};

static void deliver_hits(int count, std::vector<SimWarrior> &targets,
                         int &building_hp) {
  for (int hit = 0; hit < count; ++hit) {
    int best = -1;
    for (int i = 0; i < (int)targets.size(); ++i) {
      if (targets[i].hp <= 0)
        continue;
      if (best == -1 || targets[i].hp < targets[best].hp ||
          (targets[i].hp == targets[best].hp &&
           targets[i].num < targets[best].num))
        best = i;
    }
    if (best != -1)
      --targets[best].hp;
    else if (building_hp > 0)
      --building_hp;
  }
}

static bool capture_succeeds(const Building &target,
                             const std::vector<int> &defender_hps,
                             const std::vector<int> &attacker_hps,
                             int max_days) {
  std::vector<SimWarrior> attackers, defenders;
  for (int i = 0; i < (int)attacker_hps.size(); ++i)
    attackers.push_back({attacker_hps[i], 1000000 + i});
  for (int i = 0; i < (int)defender_hps.size(); ++i)
    defenders.push_back({defender_hps[i], i});

  int hp = target.hp;
  int no_attacker_building = -1;
  for (int day = 0; day < max_days && !attackers.empty(); ++day) {
    const int attack_count = (int)attackers.size();
    const int defend_count = (int)defenders.size();
    deliver_hits(building_turret(target), attackers, no_attacker_building);
    deliver_hits(attack_count, defenders, hp);
    deliver_hits(defend_count, attackers, no_attacker_building);
    attackers.erase(
        std::remove_if(attackers.begin(), attackers.end(),
                       [](const SimWarrior &w) { return w.hp <= 0; }),
        attackers.end());
    defenders.erase(
        std::remove_if(defenders.begin(), defenders.end(),
                       [](const SimWarrior &w) { return w.hp <= 0; }),
        defenders.end());
    if (hp <= 0)
      return !attackers.empty();
  }
  return false;
}

static int minimum_attackers(const Building &target,
                             const std::vector<int> &defender_hps,
                             std::vector<int> available_hps,
                             int max_days = 30) {
  std::sort(available_hps.begin(), available_hps.end(), std::greater<int>());
  std::vector<int> chosen;
  for (int hp : available_hps) {
    chosen.push_back(hp);
    if (capture_succeeds(target, defender_hps, chosen, max_days))
      return (int)chosen.size();
  }
  return 1000000000;
}

// Opposite of minimum_attackers(): return the total number of defenders that
// keeps an own building alive against the confirmed incoming wave.  Existing
// defenders retain their current HP; missing slots are filled with warriors
// produced at the current HQ level.
static int minimum_regional_defenders(
    const Building &target, const std::vector<int> &attacker_hps,
    std::vector<int> current_defender_hps, int reinforcement_hp) {
  if (attacker_hps.empty())
    return 0;
  std::sort(current_defender_hps.begin(), current_defender_hps.end(),
            std::greater<int>());
  const int cap = (int)attacker_hps.size() * 2 + 5;
  for (int defender_count = 0; defender_count <= cap; ++defender_count) {
    std::vector<SimWarrior> attackers, defenders;
    for (int i = 0; i < (int)attacker_hps.size(); ++i)
      attackers.push_back({attacker_hps[i], 1000000 + i});
    for (int i = 0; i < defender_count; ++i) {
      const int hp = i < (int)current_defender_hps.size()
                         ? current_defender_hps[i]
                         : reinforcement_hp;
      defenders.push_back({hp, i});
    }

    int building_hp = target.hp;
    int no_attacker_building = -1;
    for (int day = 0; day < 1000 && !attackers.empty() && building_hp > 0;
         ++day) {
      const int attack_count = (int)attackers.size();
      const int defend_count = (int)defenders.size();
      deliver_hits(building_turret(target), attackers, no_attacker_building);
      deliver_hits(attack_count, defenders, building_hp);
      deliver_hits(defend_count, attackers, no_attacker_building);
      attackers.erase(
          std::remove_if(attackers.begin(), attackers.end(),
                         [](const SimWarrior &w) { return w.hp <= 0; }),
          attackers.end());
      defenders.erase(
          std::remove_if(defenders.begin(), defenders.end(),
                         [](const SimWarrior &w) { return w.hp <= 0; }),
          defenders.end());
    }
    if (building_hp > 0 && attackers.empty())
      return defender_count;
  }
  return cap;
}

static int center_stronghold(const GameMap &M) {
  if (M.strongholds.empty())
    return -1;
  const long double cx = ((long double)M.x[0] + M.x[M.N - 1]) / 2.0L;
  const long double cy = ((long double)M.y[0] + M.y[M.N - 1]) / 2.0L;
  int best = -1;
  long double best_d2 = std::numeric_limits<long double>::infinity();
  for (int r : M.strongholds) {
    const long double dx = (long double)M.x[r] - cx;
    const long double dy = (long double)M.y[r] - cy;
    const long double d2 = dx * dx + dy * dy;
    if (d2 < best_d2 || (d2 == best_d2 && r < best)) {
      best_d2 = d2;
      best = r;
    }
  }
  return best;
}

[[maybe_unused]] static Actions decide(const GameState &S, const GameMap &M,
                                       const Paths &P,
                                       StrategyState &strategy, int turn) {
  Actions a;
  int budget = S.gold;
  int force_train_gold = 0;
  int defense_train_gold = 0;
  const Side enemy_side = opposite(M.my_side);
  const int N = M.N;
  const int center = center_stronghold(M);
  std::vector<char> is_strong(N, false), is_visible(N, false);
  for (int r : M.strongholds)
    is_strong[r] = true;
  for (int r : S.visible)
    is_visible[r] = true;

  std::vector<const Building *> building_at(N, nullptr);
  std::vector<int> my_at(N, 0), labor_at(N, 0),
      stationary_labor_at(N, 0), enemy_at(N, 0);
  for (const auto &b : S.buildings)
    building_at[b.region] = &b;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side) {
      ++my_at[w.region];
      if (!is_scout_purpose(w.purpose)) {
        ++labor_at[w.region];
        if (w.state == WState::STATIONARY)
          ++stationary_labor_at[w.region];
      }
    } else {
      ++enemy_at[w.region];
    }
  }

  std::vector<WarriorId> assigned;
  std::vector<WarriorId> reserved_future_scouts;
  std::vector<int> labor_leaving_from(N, 0);
  auto used = [&](WarriorId id) {
    return std::find(assigned.begin(), assigned.end(), id) != assigned.end() ||
           std::find(reserved_future_scouts.begin(),
                     reserved_future_scouts.end(), id) !=
               reserved_future_scouts.end();
  };
  auto target_will_be_ours = [&](int target) {
    const Building *b = building_at[target];
    if (b != nullptr && b->side == M.my_side)
      return true;
    return std::find(a.upgrades.begin(), a.upgrades.end(), target) !=
           a.upgrades.end();
  };
  // Match the qualifier agent's labor invariant at the command boundary:
  // every own building keeps all currently available workers up to work_cap,
  // and only stationary excess labor may be dispatched.  Counting only
  // stationary labor is important because a unit already moving through a
  // workplace will leave it during this turn and cannot fill today's slot.
  auto can_leave_without_losing_labor = [&](const Warrior &w) {
    if (is_scout_purpose(w.purpose))
      return true;
    const Building *home = building_at[w.region];
    if (home == nullptr || home->side != M.my_side)
      return true;
    const int remaining = stationary_labor_at[w.region] -
                          labor_leaving_from[w.region];
    return remaining > home->work_cap();
  };
  auto queue_move = [&](const Warrior &w, int target, WPurpose purpose) {
    if (w.id.side != M.my_side || w.state != WState::STATIONARY || used(w.id) ||
        target < 0 || target >= N || target == w.region ||
        !can_leave_without_losing_labor(w))
      return false;
    const int cost = target_will_be_ours(target) ? 0 : MOVE_COST;
    if (budget < cost)
      return false;
    budget -= cost;
    assigned.push_back(w.id);
    if (!is_scout_purpose(w.purpose))
      ++labor_leaving_from[w.region];
    a.moves.push_back(MoveOrder{w.id, target, purpose});
    return true;
  };
  auto upgrade_price = [&](int region) {
    const Building *b = building_at[region];
    if (b == nullptr)
      return BASE_LEVELS[1].cost;
    if (b->level >= max_level(*b))
      return b->type == BType::HQ ? HQ_HEAL_COST : BASE_HEAL_COST;
    return upgrade_cost(*b);
  };
  auto can_upgrade_region = [&](int region) {
    if (region < 0 || region >= N ||
        labor_at[region] - labor_leaving_from[region] <= 0 ||
        enemy_at[region] > 0)
      return false;
    const Building *b = building_at[region];
    if (b == nullptr)
      return is_strong[region] != 0;
    if (b->side != M.my_side)
      return false;
    return b->level < max_level(*b) || b->hp < b->current_hp();
  };
  auto queue_upgrade = [&](int region, int reserve_after) {
    if (!can_upgrade_region(region) ||
        std::find(a.upgrades.begin(), a.upgrades.end(), region) !=
            a.upgrades.end())
      return false;
    const int cost = upgrade_price(region);
    // A new stronghold is the top development priority.  Once a builder has
    // reached empty land, complete that 500-gold commitment before matching
    // the enemy force.  Existing-building upgrades still respect the force
    // floor inherited from the qualifier model.
    const bool creates_new_base = building_at[region] == nullptr;
    const int required_reserve =
        creates_new_base ? reserve_after
                         : std::max({reserve_after, force_train_gold,
                                     defense_train_gold});
    if (budget - cost < required_reserve)
      return false;
    budget -= cost;
    a.upgrades.push_back(region);
    return true;
  };

  int owned_bases = 0;
  int known_enemy_bases = 0;
  int total_work_cap = 0;
  const Building *seen_enemy_hq = nullptr;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      if (b.type == BType::BASE)
        ++known_enemy_bases;
      else
        seen_enemy_hq = &b;
      continue;
    }
    total_work_cap += b.work_cap();
    if (b.type == BType::BASE)
      ++owned_bases;
  }

  int visible_enemy_army = 0;
  bool enemy_pressure = false;
  for (const auto &w : S.warriors) {
    if (w.id.side != enemy_side)
      continue;
    ++visible_enemy_army;
    if (P.dist[M.my_hq][w.region] <= P.dist[M.opp_hq][w.region])
      enemy_pressure = true;
    for (const auto &b : S.buildings) {
      if (b.side == M.my_side && P.hops[w.region][b.region] <= 2)
        enemy_pressure = true;
    }
  }
  const int enemy_hq_age =
      seen_enemy_hq == nullptr ? turn + 1
                               : std::max(0, turn - seen_enemy_hq->last_seen_turn);
  const int enemy_hq_level = seen_enemy_hq == nullptr ? 1 : seen_enemy_hq->level;
  int my_regular_alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && !is_scout_purpose(w.purpose))
      ++my_regular_alive;
  const bool enemy_tech = seen_enemy_hq != nullptr && enemy_hq_age <= 30 &&
                          enemy_hq_level >= 3;
  const bool enemy_greedy = known_enemy_bases > owned_bases;
  const Building *hq = find_building(S, M.my_hq);
  const int current_train_cap =
      HQ_LEVELS[hq != nullptr ? hq->level : 1].train_cap;

  // The qualifier agent could always compare HQ levels because there was no
  // finals-style fog around that decision.  Enable its HQ-first policy only
  // while the enemy HQ is in the current visibility snapshot; a remembered
  // level from an older turn must not freeze development or training.
  const bool enemy_hq_level_confirmed =
      seen_enemy_hq != nullptr && is_visible[M.opp_hq] &&
      seen_enemy_hq->last_seen_turn == S.turn;
  const bool confirmed_hq_behind =
      enemy_hq_level_confirmed && hq != nullptr &&
      hq->level < seen_enemy_hq->level;
  bool qualifier_hq_upgrade_queued = false;
  if (enemy_hq_level_confirmed && hq != nullptr &&
      hq->side == M.my_side && hq->level < HQ_MAX_LEVEL) {
    // As in the qualifier best model, an affordable HQ level is purchased
    // before attack, expansion, base depth, or training spends the same gold.
    // force_train_gold/defense_train_gold are intentionally still zero here.
    qualifier_hq_upgrade_queued = queue_upgrade(M.my_hq, 0);
  }
  dbg::note(turn, "HQ_TECH confirmed=" +
                      std::string(enemy_hq_level_confirmed ? "1" : "0") +
                      " my_level=" +
                      std::to_string(hq != nullptr ? hq->level : 0) +
                      " enemy_level=" + std::to_string(enemy_hq_level) +
                      " behind=" +
                      std::string(confirmed_hq_behind ? "1" : "0") +
                      " upgrade_queued=" +
                      std::string(qualifier_hq_upgrade_queued ? "1" : "0"));

  // Port the qualifier best model's baseline_military calculation exactly:
  // warriors working at their own building do not count as combat power, and
  // every other warrior contributes its current HP.  Finals fog means the
  // opponent side must use persistent scout-fed memory instead of the current
  // visibility snapshot.  At a workplace, the lowest-HP work_cap warriors are
  // treated as labor just as in the qualifier model.
  std::vector<std::vector<int>> my_workplace_hps(N), enemy_workplace_hps(N);
  int my_combat_hp = 0;
  int known_enemy_combat_hp = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side)
      continue;
    const Building *b = building_at[w.region];
    if (b != nullptr && b->side == M.my_side)
      my_workplace_hps[w.region].push_back(std::max(0, w.hp));
    else
      my_combat_hp += std::max(0, w.hp);
  }
  for (const auto &intel : S.enemy_memory) {
    if (intel.region < 0 || intel.region >= N)
      continue;
    const Building *b = building_at[intel.region];
    if (b != nullptr && b->side == enemy_side)
      enemy_workplace_hps[intel.region].push_back(std::max(0, intel.hp));
    else
      known_enemy_combat_hp += std::max(0, intel.hp);
  }
  for (const auto &b : S.buildings) {
    auto &hps = b.side == M.my_side ? my_workplace_hps[b.region]
                                     : enemy_workplace_hps[b.region];
    std::sort(hps.begin(), hps.end());
    int surplus_hp = 0;
    for (int i = std::min<int>(b.work_cap(), hps.size());
         i < (int)hps.size(); ++i)
      surplus_hp += hps[i];
    if (b.side == M.my_side)
      my_combat_hp += surplus_hp;
    else
      known_enemy_combat_hp += surplus_hp;
  }

  const int my_warrior_hp =
      HQ_LEVELS[hq != nullptr ? hq->level : 1].warrior_hp;
  int my_min_turret = std::numeric_limits<int>::max();
  for (const auto &b : S.buildings)
    if (b.side == M.my_side)
      my_min_turret = std::min(my_min_turret, building_turret(b));
  if (my_min_turret == std::numeric_limits<int>::max())
    my_min_turret = 0;

  const int hp_deficit =
      known_enemy_combat_hp - my_combat_hp - my_min_turret;
  const int force_train_need =
      hp_deficit <= 0 ? 0 : (hp_deficit + my_warrior_hp - 1) / my_warrior_hp;
  const int force_train_reserve =
      TRAIN_COST * std::min(current_train_cap, force_train_need);
  force_train_gold = force_train_reserve;
  dbg::note(turn, "INTEL enemy_memory=" +
                      std::to_string(S.enemy_memory.size()) +
                      " my_regular=" + std::to_string(my_regular_alive) +
                      " enemy_combat_hp=" +
                      std::to_string(known_enemy_combat_hp) +
                      " my_combat_hp=" + std::to_string(my_combat_hp) +
                      " turret_credit=" + std::to_string(my_min_turret) +
                      " hp_deficit=" + std::to_string(hp_deficit) +
                      " force_train_need=" +
                      std::to_string(force_train_need) +
                      " force_reserve=" +
                      std::to_string(force_train_reserve) +
                      " pressure=" + (enemy_pressure ? "1" : "0"));

  // Keep exactly one strategic scout assigned to the enemy HQ.  If the
  // original HQ_SCOUT died, promote the oldest surviving roaming scout instead
  // of leaving the most important observation job permanently vacant.
  const Warrior *hq_sentry = nullptr;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || !is_scout_purpose(w.purpose))
      continue;
    if (hq_sentry == nullptr ||
        (w.purpose == WPurpose::HQ_SCOUT &&
         hq_sentry->purpose != WPurpose::HQ_SCOUT) ||
        (w.purpose == hq_sentry->purpose && w.id.num < hq_sentry->id.num))
      hq_sentry = &w;
  }
  auto is_hq_sentry = [&](const Warrior &w) {
    return hq_sentry != nullptr && w.id == hq_sentry->id;
  };

  // The roaming scout is unlocked only after our HQ level-2 upgrade has
  // actually resolved.  Its patrol/target lock survives across turns so it
  // can follow the same army instead of switching to whichever ghost record
  // happens to score highest on a later turn.
  const bool roaming_scout_enabled =
      hq != nullptr && hq->side == M.my_side && hq->level >= 2;
  if (!roaming_scout_enabled) {
    strategy.roaming_patrol_start_turn = -1;
    strategy.roaming_tracked_ids.clear();
  } else if (strategy.roaming_patrol_start_turn < 0) {
    strategy.roaming_patrol_start_turn = S.turn;
  }

  const bool had_tracked_stack = !strategy.roaming_tracked_ids.empty();
  strategy.roaming_tracked_ids.erase(
      std::remove_if(strategy.roaming_tracked_ids.begin(),
                     strategy.roaming_tracked_ids.end(),
                     [&](WarriorId id) {
                       return std::none_of(
                           S.enemy_memory.begin(), S.enemy_memory.end(),
                           [&](const EnemyWarriorIntel &intel) {
                             return intel.id == id;
                           });
                     }),
      strategy.roaming_tracked_ids.end());
  if (had_tracked_stack && strategy.roaming_tracked_ids.empty())
    strategy.roaming_patrol_start_turn = S.turn;

  auto is_tracked_enemy = [&](WarriorId id) {
    return std::find(strategy.roaming_tracked_ids.begin(),
                     strategy.roaming_tracked_ids.end(),
                     id) != strategy.roaming_tracked_ids.end();
  };
  std::vector<int> tracked_at(N, 0), tracked_hp_at(N, 0);
  for (const auto &intel : S.enemy_memory) {
    if (!is_tracked_enemy(intel.id) || intel.region < 0 || intel.region >= N)
      continue;
    ++tracked_at[intel.region];
    tracked_hp_at[intel.region] += std::max(0, intel.hp);
  }
  int tracked_stack_region = -1;
  for (int region = 0; region < N; ++region) {
    if (tracked_at[region] == 0)
      continue;
    if (tracked_stack_region == -1 ||
        tracked_at[region] > tracked_at[tracked_stack_region] ||
        (tracked_at[region] == tracked_at[tracked_stack_region] &&
         tracked_hp_at[region] > tracked_hp_at[tracked_stack_region]) ||
        (tracked_at[region] == tracked_at[tracked_stack_region] &&
         tracked_hp_at[region] == tracked_hp_at[tracked_stack_region] &&
         region < tracked_stack_region))
      tracked_stack_region = region;
  }
  if (tracked_stack_region == M.opp_hq) {
    // Enemy-HQ occupants are already covered by the permanent sentry.  If the
    // tailed army retreats home, release it and start another base sweep.
    strategy.roaming_tracked_ids.clear();
    strategy.roaming_patrol_start_turn = S.turn;
    tracked_stack_region = -1;
  }
  // Preserve the three opening warriors for the two builders plus HQ labor.
  // Turn 1 trains warrior 4; that dedicated recruit takes the sentry role on
  // turn 2 instead of also converting warrior 3 into a second early scout.
  const bool waiting_for_first_trained_scout =
      turn <= 1 && hq_sentry == nullptr;

  // The first trained warrior becomes a permanent enemy-HQ sentry.  Pick the
  // first region on our weighted route that is exactly inside the shared
  // two-hop vision radius, so it sees every unit in the HQ without entering
  // the HQ or standing needlessly adjacent to its turret.
  int enemy_hq_watch = -1;
  {
    int cur = M.my_hq;
    for (int guard = 0;
         guard <= N && P.vision_hops[cur][M.opp_hq] > HOP_VISION;
         ++guard) {
      int next = P.nxt[cur][M.opp_hq];
      if (next < 0 || next == cur)
        break;
      cur = next;
    }
    if (cur != M.opp_hq &&
        P.vision_hops[cur][M.opp_hq] <= HOP_VISION)
      enemy_hq_watch = cur;
  }

  // 1) Scouts never retreat all the way to an economic building.  When an
  // enemy comes adjacent, sidestep to the safest neighboring region and keep
  // the scouting purpose so the normal intelligence patrol resumes afterward.
  // An HQ sentry first prefers a sidestep that preserves enemy-HQ vision.  It
  // may leave that radius only when every vision-preserving neighbor is bad.
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
        !is_scout_purpose(w.purpose) ||
        (building_at[w.region] != nullptr &&
         building_at[w.region]->side == M.my_side))
      continue;
    bool danger = false;
    for (const auto &enemy : S.warriors) {
      if (enemy.id.side != enemy_side)
        continue;
      // The three original defenders sitting inside the enemy HQ are exactly
      // what the sentry is meant to watch; react only after an enemy sorties.
      if (is_hq_sentry(w) && enemy.region == M.opp_hq)
        continue;
      const int enemy_distance = P.vision_hops[w.region][enemy.region];
      // The roaming scout deliberately tails its locked army from one edge
      // away.  Same-region contact is still dangerous, as are all unrelated
      // enemies.
      if (!is_hq_sentry(w) && w.purpose == WPurpose::SCOUT &&
          enemy.region == tracked_stack_region &&
          is_tracked_enemy(enemy.id) && enemy_distance == 1)
        continue;
      if (enemy_distance <= 1) {
        danger = true;
        break;
      }
    }
    if (!danger)
      continue;

    int evade = -1;
    int best_safety = -1;
    int best_keeps_hq_vision = -1;
    int best_enemy_half = -1;
    int best_staleness = -1;
    for (int candidate : M.adj[w.region]) {
      if (candidate == M.opp_hq || enemy_at[candidate] > 0)
        continue;
      const Building *b = building_at[candidate];
      if (b != nullptr && b->side == enemy_side)
        continue;

      int safety = N + 1;
      for (const auto &intel : S.enemy_memory) {
        if (is_hq_sentry(w) && intel.region == M.opp_hq)
          continue;
        if (!is_hq_sentry(w) && w.purpose == WPurpose::SCOUT &&
            intel.region == tracked_stack_region &&
            is_tracked_enemy(intel.id) &&
            P.vision_hops[candidate][intel.region] == 1)
          continue;
        safety = std::min(safety,
                          P.vision_hops[candidate][intel.region]);
      }
      const int enemy_half =
          P.dist[M.opp_hq][candidate] <= P.dist[M.my_hq][candidate];
      const int keeps_hq_vision =
          is_hq_sentry(w) &&
          P.vision_hops[candidate][M.opp_hq] <= HOP_VISION;
      const int last_seen = S.last_seen_region_turn[candidate];
      const int staleness =
          last_seen < 0 ? MAX_TURN + 1 : turn - last_seen;
      if (keeps_hq_vision > best_keeps_hq_vision ||
          (keeps_hq_vision == best_keeps_hq_vision &&
           safety > best_safety) ||
          (keeps_hq_vision == best_keeps_hq_vision &&
           safety == best_safety && enemy_half > best_enemy_half) ||
          (keeps_hq_vision == best_keeps_hq_vision &&
           safety == best_safety && enemy_half == best_enemy_half &&
           staleness > best_staleness) ||
          (keeps_hq_vision == best_keeps_hq_vision &&
           safety == best_safety && enemy_half == best_enemy_half &&
           staleness == best_staleness &&
           (evade == -1 || candidate < evade))) {
        evade = candidate;
        best_keeps_hq_vision = keeps_hq_vision;
        best_safety = safety;
        best_enemy_half = enemy_half;
        best_staleness = staleness;
      }
    }
    if (evade != -1)
      queue_move(w, evade, w.purpose);
  }

  // Assign an HQ sentry before expansion can consume the newest recruit as a
  // builder.  The common labor guard still keeps the HQ's current work_cap,
  // so a dead sentry causes one replacement to be trained first and that new
  // surplus unit takes the role on the following turn.
  bool hq_scout_ordered_this_turn = false;
  if (hq_sentry == nullptr && enemy_hq_watch >= 0) {
    const Warrior *new_sentry = nullptr;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.region != M.my_hq ||
          w.state != WState::STATIONARY || w.purpose != WPurpose::NONE ||
          !can_leave_without_losing_labor(w))
        continue;
      if (new_sentry == nullptr || w.id.num > new_sentry->id.num)
        new_sentry = &w;
    }
    if (new_sentry != nullptr) {
      // A long destination locks the unit until arrival.  Start with one
      // edge; the patrol planner below will continue in short safe legs.
      const int first_leg = P.nxt[new_sentry->region][enemy_hq_watch];
      hq_scout_ordered_this_turn =
          queue_move(*new_sentry, first_leg, WPurpose::HQ_SCOUT);
    }
  }

  // Reserve newly unlocked scout slots before defense, expansion, workforce,
  // and attack passes can consume every surplus unit.  The reservation is
  // released immediately before scout planning; it changes no command by
  // itself and never removes a filled labor slot.
  const int early_hq_level = hq != nullptr ? hq->level : 1;
  const int early_mobile_scout_limit =
      early_hq_level >= 2
          ? std::min(4, std::max(2, early_hq_level))
          : 0;
  const int early_total_scout_limit = 1 + early_mobile_scout_limit;
  int early_active_scouts = hq_scout_ordered_this_turn ? 1 : 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && is_scout_purpose(w.purpose))
      ++early_active_scouts;
  std::vector<int> scout_reserved_from(N, 0);
  while (early_active_scouts < early_total_scout_limit) {
    const Warrior *best = nullptr;
    int best_free_region = -1;
    int best_enemy_hops = 1000000000;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          used(w.id) || is_scout_purpose(w.purpose))
        continue;
      const bool committed_builder =
          w.purpose == WPurpose::BUILD && w.target >= 0 && w.target < N &&
          building_at[w.target] == nullptr;
      if (committed_builder)
        continue;
      const Building *home = building_at[w.region];
      const int keep = (home != nullptr && home->side == M.my_side)
                           ? home->work_cap()
                           : 0;
      if (home != nullptr && home->side == M.my_side &&
          stationary_labor_at[w.region] - labor_leaving_from[w.region] -
                  scout_reserved_from[w.region] <=
              keep)
        continue;
      const int free_region =
          home == nullptr || home->side != M.my_side ? 1 : 0;
      const int enemy_hops = P.hops[w.region][M.opp_hq];
      if (best == nullptr || free_region > best_free_region ||
          (free_region == best_free_region &&
           enemy_hops < best_enemy_hops) ||
          (free_region == best_free_region &&
           enemy_hops == best_enemy_hops && w.hp > best->hp) ||
          (free_region == best_free_region &&
           enemy_hops == best_enemy_hops && w.hp == best->hp &&
           w.id.num < best->id.num)) {
        best = &w;
        best_free_region = free_region;
        best_enemy_hops = enemy_hops;
      }
    }
    if (best == nullptr)
      break;
    reserved_future_scouts.push_back(best->id);
    ++scout_reserved_from[best->region];
    ++early_active_scouts;
    dbg::note(turn, "SCOUT_RESERVE " + format_warrior(best->id) +
                        " region=R" + std::to_string(best->region) +
                        " total=" + std::to_string(early_active_scouts) +
                        "/" + std::to_string(early_total_scout_limit));
  }

  // Finish every already-dispatched opening base before HQ level 2.  Checking
  // the persistent BUILD intents here prevents HQ 2 from consuming the 500
  // gold of a builder that is already waiting on its target.
  bool unfinished_builder = false;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.purpose == WPurpose::BUILD &&
        building_at[w.target] == nullptr) {
      unfinished_builder = true;
      break;
    }
  }
  if (hq != nullptr && hq->side == M.my_side && hq->level == 1 &&
      owned_bases >= 2 && !unfinished_builder)
    queue_upgrade(M.my_hq, 0);

  // 2) Complete persistent BUILD intents immediately after arrival.  This is
  // the old best agent's most important anti-thrashing invariant.
  int bases_queued_this_turn = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
        w.purpose != WPurpose::BUILD || w.region != w.target)
      continue;
    if (building_at[w.region] == nullptr) {
      const int before = budget;
      // A confirmed HQ level deficit releases every pending construction
      // reserve.  The builder keeps its BUILD intent and waits on the empty
      // stronghold; once HQ parity is restored, construction resumes here.
      if (confirmed_hq_behind) {
        dbg::note(turn, "ARRIVED_BUILDER " + format_warrior(w.id) + " R" +
                            std::to_string(w.region) +
                            " held_for_hq_catchup=1");
        continue;
      }
      // A builder that has reached its target gets the construction money
      // before training.  Requiring 500+120 here caused a permanent deadlock:
      // every accumulated 120 gold was spent on another warrior.
      const int reserve_after = 0;
      const bool queued = queue_upgrade(w.region, reserve_after);
      if (queued)
        ++bases_queued_this_turn;
      dbg::note(turn, "ARRIVED_BUILDER " + format_warrior(w.id) + " R" +
                          std::to_string(w.region) + " budget=" +
                          std::to_string(before) + " cost=" +
                          std::to_string(BASE_LEVELS[1].cost) +
                          " reserve_after=" +
                          std::to_string(reserve_after) +
                          " queued=" + (queued ? "1" : "0"));
    }
  }

  // 3) Reconstruct enemy attack axes from consecutive scout observations.
  // Finals MOVE results contain our units only, so read_turn_result() records
  // an enemy previous->current edge when that unit is visible on consecutive
  // turns.  Attribute each inferred mover to the nearest own building whose
  // weighted shortest route agrees with that observed edge.  A mass sortie
  // watched by the HQ sentry is therefore detected on its first visible step.
  std::vector<std::vector<int>> regional_threat_hps(N);
  std::vector<std::vector<WarriorId>> regional_threat_ids(N);
  std::vector<int> regional_threat_eta(
      N, std::numeric_limits<int>::max());
  auto add_regional_threat = [&](int target, WarriorId id, int hp, int eta) {
    if (target < 0 || target >= N)
      return;
    auto &ids = regional_threat_ids[target];
    if (std::find(ids.begin(), ids.end(), id) != ids.end())
      return;
    ids.push_back(id);
    regional_threat_hps[target].push_back(std::max(0, hp));
    regional_threat_eta[target] =
        std::min(regional_threat_eta[target], std::max(0, eta));
  };

  for (const auto &intel : S.enemy_memory) {
    if (intel.last_move_turn != S.turn || intel.previous_region < 0 ||
        intel.previous_region >= N || intel.region < 0 || intel.region >= N ||
        intel.previous_region == intel.region)
      continue;
    int best_target = -1;
    int best_hops = 1000000000;
    for (const auto &b : S.buildings) {
      if (b.side != M.my_side || b.region == intel.previous_region)
        continue;
      if (P.nxt[intel.previous_region][b.region] != intel.region)
        continue;
      const int h = P.hops[intel.region][b.region];
      if (h < best_hops ||
          (h == best_hops &&
           (best_target == -1 || b.region < best_target))) {
        best_hops = h;
        best_target = b.region;
      }
    }
    if (best_target != -1)
      add_regional_threat(best_target, intel.id, intel.hp, best_hops);
  }

  // Preserve the old one-hop alarm as a fallback for stationary enemies or
  // ambiguous equal-cost routes.  Assign each visible enemy to only its
  // nearest threatened building so one unit is not counted several times.
  for (const auto &w : S.warriors) {
    if (w.id.side != enemy_side)
      continue;
    int best_target = -1;
    int best_hops = 1000000000;
    for (const auto &b : S.buildings) {
      if (b.side != M.my_side)
        continue;
      const int h = P.hops[w.region][b.region];
      if (h > 1)
        continue;
      if (h < best_hops ||
          (h == best_hops &&
           (best_target == -1 || b.region < best_target))) {
        best_hops = h;
        best_target = b.region;
      }
    }
    if (best_target != -1)
      add_regional_threat(best_target, w.id, w.hp, best_hops);
  }

  std::vector<int> defense_need_at(N, 0);
  std::vector<int> existing_defense_incoming(N, 0);
  std::vector<int> existing_leaving_from(N, 0);
  std::vector<std::vector<int>> my_hps_at(N);
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side)
      continue;
    my_hps_at[w.region].push_back(std::max(0, w.hp));
    if (w.state == WState::MOVING && w.target != w.region) {
      ++existing_leaving_from[w.region];
      if (w.target >= 0 && w.target < N) {
        const Building *target = building_at[w.target];
        if (target != nullptr && target->side == M.my_side)
          ++existing_defense_incoming[w.target];
      }
    }
  }

  int raw_defense_shortage = 0;
  int urgent_hq_train = 0;
  bool directed_enemy_threat = false;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int need = b.work_cap();
    if (!regional_threat_hps[b.region].empty()) {
      directed_enemy_threat = true;
      need = std::max(
          need, minimum_regional_defenders(
                    b, regional_threat_hps[b.region], my_hps_at[b.region],
                    my_warrior_hp));
    }
    defense_need_at[b.region] = need;
    const int staying =
        std::max(0, my_at[b.region] - existing_leaving_from[b.region]);
    const int projected = staying + existing_defense_incoming[b.region];
    const int shortage = std::max(0, need - projected);
    raw_defense_shortage += shortage;
    if (b.type == BType::HQ)
      urgent_hq_train = std::max(urgent_hq_train, shortage);
    if (!regional_threat_hps[b.region].empty()) {
      dbg::note(turn, "DEFENSE_THREAT target=R" +
                          std::to_string(b.region) + " attackers=" +
                          std::to_string(regional_threat_hps[b.region].size()) +
                          " eta=" +
                          std::to_string(regional_threat_eta[b.region]) +
                          " need=" + std::to_string(need) + " staying=" +
                          std::to_string(staying) + " incoming=" +
                          std::to_string(existing_defense_incoming[b.region]) +
                          " shortage=" + std::to_string(shortage));
    }
  }
  if (directed_enemy_threat)
    enemy_pressure = true;
  defense_train_gold =
      TRAIN_COST * std::min(current_train_cap, raw_defense_shortage);

  // An imminent attack is the one exception where an existing building may
  // spend on repair/fortification before the reinforcement pass.  Preserve
  // the current-turn training reserve while doing so.  A currently confirmed
  // HQ-level deficit is stricter: only the HQ itself may consume that catch-up
  // fund, matching the qualifier policy that suspends base upgrades.
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || regional_threat_hps[b.region].empty() ||
        regional_threat_eta[b.region] > 1)
      continue;
    if (confirmed_hq_behind && b.type != BType::HQ)
      continue;
    if (b.hp < b.current_hp() || b.level < max_level(b))
      queue_upgrade(b.region, 0);
  }

  // Fill the most urgent regional deficit from the whole map.  Every source
  // retains its own work/defense need; scouts and committed builders are not
  // consumed.  Units that can beat the enemy ETA are preferred, then shortest
  // travel and higher current HP.
  std::vector<int> queued_defense_incoming = existing_defense_incoming;
  while (true) {
    int target = -1;
    int target_eta = std::numeric_limits<int>::max();
    int target_shortage = 0;
    for (const auto &b : S.buildings) {
      if (b.side != M.my_side || regional_threat_hps[b.region].empty())
        continue;
      const int staying =
          std::max(0, my_at[b.region] - existing_leaving_from[b.region]);
      const int shortage = std::max(
          0, defense_need_at[b.region] - staying -
                 queued_defense_incoming[b.region]);
      if (shortage == 0)
        continue;
      const int eta = regional_threat_eta[b.region];
      if (target == -1 || eta < target_eta ||
          (eta == target_eta && shortage > target_shortage) ||
          (eta == target_eta && shortage == target_shortage &&
           b.region < target)) {
        target = b.region;
        target_eta = eta;
        target_shortage = shortage;
      }
    }
    if (target == -1)
      break;

    const Warrior *best = nullptr;
    bool best_timely = false;
    int best_hops = 1000000000;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          used(w.id) || is_scout_purpose(w.purpose) ||
          w.purpose == WPurpose::BUILD || w.region == target ||
          !can_leave_without_losing_labor(w))
        continue;
      const int source = w.region;
      const Building *home = building_at[source];
      const int source_keep =
          (home != nullptr && home->side == M.my_side)
              ? defense_need_at[source]
              : 0;
      const int remaining =
          my_at[source] - existing_leaving_from[source] -
          labor_leaving_from[source];
      if (remaining <= source_keep)
        continue;
      const int h = P.hops[source][target];
      if (h >= 1000000000)
        continue;
      const bool timely = h <= std::max(1, target_eta);
      if (best == nullptr || timely > best_timely ||
          (timely == best_timely && h < best_hops) ||
          (timely == best_timely && h == best_hops && w.hp > best->hp) ||
          (timely == best_timely && h == best_hops && w.hp == best->hp &&
           w.id.num < best->id.num)) {
        best = &w;
        best_timely = timely;
        best_hops = h;
      }
    }
    if (best == nullptr || !queue_move(*best, target, WPurpose::DEFEND))
      break;
    ++queued_defense_incoming[target];
    dbg::note(turn, "DEFENSE_DISPATCH " + format_warrior(best->id) +
                        " R" + std::to_string(best->region) + "->R" +
                        std::to_string(target) + " hops=" +
                        std::to_string(best_hops) + " enemy_eta=" +
                        std::to_string(target_eta) + " timely=" +
                        (best_timely ? "1" : "0"));
  }

  int defense_train_need = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || regional_threat_hps[b.region].empty())
      continue;
    const int staying =
        std::max(0, my_at[b.region] - existing_leaving_from[b.region]);
    defense_train_need += std::max(
        0, defense_need_at[b.region] - staying -
               queued_defense_incoming[b.region]);
  }
  urgent_hq_train = std::max(urgent_hq_train, defense_train_need);
  defense_train_gold =
      TRAIN_COST * std::min(current_train_cap, defense_train_need);
  dbg::note(turn, "DEFENSE_SUMMARY active=" +
                      std::string(directed_enemy_threat ? "1" : "0") +
                      " raw_shortage=" +
                      std::to_string(raw_defense_shortage) +
                      " train_need=" + std::to_string(defense_train_need) +
                      " train_reserve=" +
                      std::to_string(defense_train_gold));

  // Track unresolved builders so the economy phase can fund new level-1 bases
  // before it deepens the buildings we already own.
  std::vector<char> pending_target(N, false);
  int pending_builds = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.purpose != WPurpose::BUILD)
      continue;
    if (building_at[w.target] != nullptr)
      continue;
    if (std::find(a.upgrades.begin(), a.upgrades.end(), w.target) !=
        a.upgrades.end())
      continue;
    if (!pending_target[w.target]) {
      pending_target[w.target] = true;
      ++pending_builds;
    }
  }
  // Construction resolves before movement.  A base queued earlier in this
  // same decision is therefore already secured; mark it reserved so the
  // expansion pass does not dispatch another builder to the stale pre-turn
  // building snapshot.
  for (int region : a.upgrades)
    if (building_at[region] == nullptr)
      pending_target[region] = true;

  // 4) Secure every profitable mandatory stronghold before deepening existing
  // bases.  As in the qualifier best model, mandatory territory means a
  // stronghold whose weighted route takes no more hops from our HQ than from
  // the enemy HQ.  Keep at most three simultaneous BUILD commitments so one
  // wave cannot reserve the whole army, then launch the next wave as soon as
  // one of those bases is completed.
  const int projected_owned_bases = owned_bases + bases_queued_this_turn;
  int max_pending = confirmed_hq_behind ? 0 : (enemy_pressure ? 1 : 3);
  bool expansion_waiting_for_builder = false;
  while (!confirmed_hq_behind && pending_builds < max_pending) {
    int best_target = -1;
    long double best_score = -std::numeric_limits<long double>::infinity();
    for (int r : M.strongholds) {
      if (building_at[r] != nullptr || pending_target[r] || enemy_at[r] > 0)
        continue;
      const int my_hops = P.hops[M.my_hq][r];
      const int enemy_hops = P.hops[M.opp_hq][r];
      if (my_hops >= 1000000000 || enemy_hops >= 1000000000 ||
          my_hops > enemy_hops)
        continue;
      const int useful_days = MAX_TURN - turn - my_hops - 1;
      if ((long long)useful_days * WORK_INCOME < BASE_LEVELS[1].cost + MOVE_COST)
        continue;
      long double score = 20000.0L;
      score += ((long double)P.dist[M.opp_hq][r] - P.dist[M.my_hq][r]) * 0.02L;
      score -= my_hops * 250.0L;
      if (r == center)
        score += 50000.0L;
      if (!is_visible[r])
        score -= 500.0L;
      if (score > best_score || (score == best_score && r < best_target)) {
        best_score = score;
        best_target = r;
      }
    }
    if (best_target == -1)
      break;

    const Warrior *builder = nullptr;
    int best_hops = 1000000000;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          used(w.id) || is_scout_purpose(w.purpose) ||
          !can_leave_without_losing_labor(w))
        continue;
      if (w.purpose == WPurpose::BUILD && building_at[w.region] == nullptr)
        continue;
      const Building *home = building_at[w.region];
      int keep = (home != nullptr && home->side == M.my_side) ? home->work_cap() : 0;
      if (labor_at[w.region] - labor_leaving_from[w.region] <= keep)
        continue;
      int h = P.hops[w.region][best_target];
      if (h < best_hops || (h == best_hops &&
                            (builder == nullptr || w.id.num < builder->id.num))) {
        best_hops = h;
        builder = &w;
      }
    }
    if (builder == nullptr) {
      expansion_waiting_for_builder = true;
      break;
    }
    if (!queue_move(*builder, best_target, WPurpose::BUILD)) {
      expansion_waiting_for_builder = true;
      break;
    }
    pending_target[best_target] = true;
    ++pending_builds;
  }

  // 5) Reserve the next fixed development purchase before training.  The
  // order is two opening bases -> HQ 2 -> third base -> every base to level 3
  // -> HQ 3, 4, 5.  queue_upgrade() spends that reserve with reserve_after=0;
  // the reserve is for later phases, not extra money required by the purchase.
  int reserve = 0;
  if (confirmed_hq_behind && hq != nullptr && hq->level < HQ_MAX_LEVEL) {
    reserve = HQ_LEVELS[hq->level + 1].upgrade_cost;
  } else if (pending_builds > 0 || expansion_waiting_for_builder) {
    reserve = BASE_LEVELS[1].cost;
  } else if (hq != nullptr && hq->side == M.my_side && hq->level == 1 &&
             projected_owned_bases >= 2) {
    reserve = HQ_LEVELS[2].upgrade_cost;
  }
  // Fixed order after expansion: HQ 2, then every currently owned base to
  // level 3, and only then HQ 3 -> 4 -> 5.
  bool base_depth_pending = false;
  for (const auto &b : S.buildings) {
    if (b.side == M.my_side && b.type == BType::BASE &&
        b.level < BASE_MAX_LEVEL) {
      base_depth_pending = true;
      break;
    }
  }
  for (int region : a.upgrades) {
    if (building_at[region] == nullptr) {
      base_depth_pending = true;
      break;
    }
  }
  const bool hq_level_two_ready =
      hq != nullptr && hq->side == M.my_side && hq->level >= 2;
  dbg::note(turn, "ECON owned_bases=" + std::to_string(owned_bases) +
                      " pending_builds=" + std::to_string(pending_builds) +
                      " expansion_waiting=" +
                      (expansion_waiting_for_builder ? "1" : "0") +
                      " base_depth_pending=" +
                      (base_depth_pending ? "1" : "0") +
                      " hq_level=" +
                      std::to_string(hq != nullptr ? hq->level : 0) +
                      " budget=" + std::to_string(budget) +
                      " build_reserve=" + std::to_string(reserve) +
                      " development_reserve=" +
                      std::to_string(reserve));

  std::vector<int> visible_enemy_bases;
  for (const auto &b : S.buildings)
    if (b.side == enemy_side && b.type == BType::BASE &&
        is_visible[b.region] && b.last_seen_turn == S.turn)
      visible_enemy_bases.push_back(b.region);

  // Evaluate every affordable building, not just one.  While enemy bases are
  // currently visible, deepen the own base with the shortest movement route
  // to that front first; economic/defensive value breaks equal-distance ties.
  // Without current enemy-base vision, retain the old value-only ordering.
  // Prospective slots pay their training cost and an eight-turn staffing
  // delay, while current workers produce immediately.
  while (!confirmed_hq_behind && pending_builds == 0 &&
         !expansion_waiting_for_builder) {
    const Building *best = nullptr;
    long double best_value = -1.0L;
    int best_front_hops = 1000000000;
    for (const auto &b : S.buildings) {
      if (b.side != M.my_side || !can_upgrade_region(b.region) ||
          b.level >= max_level(b) ||
          std::find(a.upgrades.begin(), a.upgrades.end(), b.region) !=
              a.upgrades.end())
        continue;
      if (!hq_level_two_ready)
        continue;
      if (base_depth_pending) {
        if (b.type != BType::BASE)
          continue;
      } else {
        // Do not deepen the HQ before the expansion has produced at least one
        // base.  Once all owned bases are level 3, advance the HQ sequentially.
        if (owned_bases == 0 || b.type != BType::HQ)
          continue;
      }
      const int next_cap =
          b.type == BType::HQ ? HQ_LEVELS[b.level + 1].work_cap
                              : BASE_LEVELS[b.level + 1].work_cap;
      const int added_capacity = next_cap - b.work_cap();
      const int immediate_workers =
          std::min(labor_at[b.region], next_cap) -
          std::min(labor_at[b.region], b.work_cap());
      const int future_workers =
          std::max(0, added_capacity - immediate_workers);
      const int remaining = MAX_TURN - turn;
      const int future_work_days = std::max(0, remaining - 8);
      long long economic_gain =
          (long long)remaining * WORK_INCOME * immediate_workers;
      economic_gain +=
          (long long)future_work_days *
              (WORK_INCOME - UPKEEP_PER_WARRIOR) * future_workers -
          (long long)TRAIN_COST * future_workers;

      const int next_hp = b.type == BType::HQ
                              ? HQ_LEVELS[b.level + 1].hp
                              : BASE_LEVELS[b.level + 1].hp;
      const int next_turret = b.type == BType::HQ
                                  ? HQ_LEVELS[b.level + 1].turret
                                  : BASE_LEVELS[b.level + 1].turret;
      const int defense_value =
          (next_hp - b.current_hp()) * 8 +
          (next_turret - building_turret(b)) * 120;
      long double value =
          (long double)(economic_gain + defense_value) /
          std::max(1, upgrade_cost(b));
      if (b.type == BType::HQ) {
        value += 0.5L * (HQ_LEVELS[b.level + 1].train_cap -
                         HQ_LEVELS[b.level].train_cap);
      } else if (turn < 250) {
        // A small strategic premium reflects that distributed bases are also
        // staging/defensive positions, not merely worker slots.
        value += 0.15L;
      }

      int front_hops = 1000000000;
      if (b.type == BType::BASE)
        for (int enemy_region : visible_enemy_bases)
          front_hops =
              std::min(front_hops, P.hops[b.region][enemy_region]);
      const bool frontline_order =
          b.type == BType::BASE && !visible_enemy_bases.empty();
      if (best == nullptr ||
          (frontline_order && front_hops < best_front_hops) ||
          (frontline_order && front_hops == best_front_hops &&
           value > best_value) ||
          (!frontline_order && value > best_value) ||
          (front_hops == best_front_hops && value == best_value &&
           b.region < best->region)) {
        best_front_hops = front_hops;
        best_value = value;
        best = &b;
      }
    }
    if (best == nullptr)
      break;
    if (!queue_upgrade(best->region, 0))
      break;
    if (best->type == BType::BASE && !visible_enemy_bases.empty())
      dbg::note(turn, "BASE_UPGRADE_FRONT target=R" +
                          std::to_string(best->region) + " enemy_bases=" +
                          std::to_string(visible_enemy_bases.size()) +
                          " hops=" + std::to_string(best_front_hops));
  }

  // 6) Turn idle warriors into income before considering any offensive move.
  // Today's upgrades are included because construction resolves before
  // movement.  Never pull a worker out of a filled slot to staff another slot.
  std::vector<int> projected_work_cap_at(N, 0);
  for (const auto &b : S.buildings)
    if (b.side == M.my_side)
      projected_work_cap_at[b.region] = b.work_cap();
  for (int region : a.upgrades) {
    const Building *b = building_at[region];
    if (b == nullptr) {
      projected_work_cap_at[region] = BASE_LEVELS[1].work_cap;
    } else if (b->side == M.my_side && b->level < max_level(*b)) {
      projected_work_cap_at[region] =
          b->type == BType::HQ ? HQ_LEVELS[b->level + 1].work_cap
                               : BASE_LEVELS[b->level + 1].work_cap;
    }
  }

  std::vector<int> projected_labor_at = labor_at;
  for (int region = 0; region < N; ++region)
    projected_labor_at[region] -= labor_leaving_from[region];
  bool worker_reassignment_queued = false;
  while (!enemy_pressure) {
    const Warrior *best_worker = nullptr;
    int best_target = -1;
    int best_deficit = -1;
    int best_hops = 1000000000;
    for (int target = 0; target < N; ++target) {
      const int deficit =
          projected_work_cap_at[target] - projected_labor_at[target];
      if (deficit <= 0)
        continue;
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
            used(w.id) || is_scout_purpose(w.purpose) ||
            !can_leave_without_losing_labor(w))
          continue;
        if (w.purpose == WPurpose::BUILD && building_at[w.region] == nullptr)
          continue;
        if (projected_labor_at[w.region] <=
            projected_work_cap_at[w.region])
          continue;
        const int hops = P.hops[w.region][target];
        if (hops >= 1000000000)
          continue;
        if (deficit > best_deficit ||
            (deficit == best_deficit && hops < best_hops) ||
            (deficit == best_deficit && hops == best_hops &&
             (best_worker == nullptr || w.id.num < best_worker->id.num))) {
          best_worker = &w;
          best_target = target;
          best_deficit = deficit;
          best_hops = hops;
        }
      }
    }
    if (best_worker == nullptr ||
        !queue_move(*best_worker, best_target, WPurpose::NONE))
      break;
    --projected_labor_at[best_worker->region];
    ++projected_labor_at[best_target];
    worker_reassignment_queued = true;
  }

  int projected_total_work_cap_for_attack = 0;
  int currently_staffed_work_cap = 0;
  for (int region = 0; region < N; ++region) {
    projected_total_work_cap_for_attack += projected_work_cap_at[region];
    currently_staffed_work_cap += std::min(
        projected_work_cap_at[region],
        std::max(0, labor_at[region] - labor_leaving_from[region]));
  }
  const int offensive_mobile_reserve = 2 + owned_bases * 2;
  const int attackable_surplus =
      std::max(0, my_regular_alive - projected_total_work_cap_for_attack -
                      pending_builds - offensive_mobile_reserve);
  const bool income_ready_for_attack =
      !enemy_pressure && !worker_reassignment_queued &&
      currently_staffed_work_cap == projected_total_work_cap_for_attack &&
      attackable_surplus > 0;

  // 7) Before a normal offensive, dedicate one additional scout to the
  // nearest cluster of mutually close enemy bases.  Every selected pair must
  // be within four plain graph hops (twice the shared vision radius), and one
  // probe covers at most four bases.  A transitive connected-component rule
  // made long chains of bases on dense maps look like one enormous front.
  // The cluster must be observed after this probe started before remembered
  // garrisons are allowed into the ordinary attack calculation.
  auto choose_offense_probe_cluster = [&]() {
    std::vector<int> enemy_bases;
    for (const auto &b : S.buildings)
      if (b.side == enemy_side && b.type == BType::BASE)
        enemy_bases.push_back(b.region);
    std::sort(enemy_bases.begin(), enemy_bases.end());

    std::vector<int> best_cluster;
    int best_approach = 1000000000;
    int best_first_region = N;
    for (int seed = 0; seed < (int)enemy_bases.size(); ++seed) {
      std::vector<int> candidates;
      for (int region : enemy_bases)
        if (region != enemy_bases[seed] &&
            P.vision_hops[enemy_bases[seed]][region] <= 2 * HOP_VISION)
          candidates.push_back(region);
      std::sort(candidates.begin(), candidates.end(), [&](int lhs, int rhs) {
        const int lhs_hops = P.vision_hops[enemy_bases[seed]][lhs];
        const int rhs_hops = P.vision_hops[enemy_bases[seed]][rhs];
        return lhs_hops != rhs_hops ? lhs_hops < rhs_hops : lhs < rhs;
      });

      std::vector<int> cluster{enemy_bases[seed]};
      for (int candidate : candidates) {
        bool close_to_every_base = true;
        for (int selected : cluster)
          if (P.vision_hops[candidate][selected] > 2 * HOP_VISION) {
            close_to_every_base = false;
            break;
          }
        if (!close_to_every_base)
          continue;
        cluster.push_back(candidate);
        if (cluster.size() == 4)
          break;
      }
      std::sort(cluster.begin(), cluster.end());

      int approach = 1000000000;
      for (int target : cluster)
        for (const auto &home : S.buildings)
          if (home.side == M.my_side)
            approach = std::min(approach, P.hops[home.region][target]);
      const int first_region = cluster.empty() ? N : cluster.front();
      if (approach < best_approach ||
          (approach == best_approach &&
           cluster.size() > best_cluster.size()) ||
          (approach == best_approach &&
           cluster.size() == best_cluster.size() &&
           first_region < best_first_region)) {
        best_approach = approach;
        best_first_region = first_region;
        best_cluster = std::move(cluster);
      }
    }
    return best_cluster;
  };

  const bool offense_scout_exists =
      std::any_of(S.warriors.begin(), S.warriors.end(), [&](const Warrior &w) {
        return w.id.side == M.my_side &&
               w.purpose == WPurpose::OFFENSE_SCOUT;
      });
  const bool offense_probe_was_active =
      strategy.offense_probe_start_turn >= 0 || offense_scout_exists;
  if ((int)strategy.offense_probe_seen_turn.size() != N)
    strategy.offense_probe_seen_turn.assign(N, -1);
  const auto old_probe_size = strategy.offense_probe_regions.size();
  strategy.offense_probe_regions.erase(
      std::remove_if(strategy.offense_probe_regions.begin(),
                     strategy.offense_probe_regions.end(), [&](int region) {
                       const Building *b = building_at[region];
                       return b == nullptr || b->side != enemy_side ||
                              b->type != BType::BASE;
                     }),
      strategy.offense_probe_regions.end());
  if (strategy.offense_probe_regions.size() != old_probe_size) {
    strategy.offense_probe_start_turn = S.turn;
    strategy.offense_probe_complete_turn = -1;
  }
  // Scouting has to precede the strict attack gate.  Waiting for
  // income_ready_for_attack here made the probe circular: the economy could
  // remain busy forever, so no target was ever refreshed and no offensive
  // opportunity could be prepared.  Once HQ 2 unlocks the mobile scouting
  // layer, keep one enemy-base cluster warm even while the army is still
  // accumulating its economic surplus.
  if (strategy.offense_probe_regions.empty() &&
      (roaming_scout_enabled || offense_probe_was_active)) {
    strategy.offense_probe_regions = choose_offense_probe_cluster();
    if (!strategy.offense_probe_regions.empty()) {
      strategy.offense_probe_start_turn = S.turn;
      strategy.offense_probe_complete_turn = -1;
    }
  }

  // Only the dedicated offensive scout can certify a target.  Incidental HQ,
  // building, or roaming-scout vision still updates ordinary intelligence,
  // but cannot bypass this pre-attack sweep.
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || w.purpose != WPurpose::OFFENSE_SCOUT)
      continue;
    for (int region : strategy.offense_probe_regions)
      if (P.vision_hops[w.region][region] <= HOP_VISION &&
          is_visible[region])
        strategy.offense_probe_seen_turn[region] = S.turn;
  }

  bool offense_probe_ready = !strategy.offense_probe_regions.empty();
  for (int region : strategy.offense_probe_regions) {
    const Building *b = building_at[region];
    if (b == nullptr ||
        strategy.offense_probe_seen_turn[region] <
            strategy.offense_probe_start_turn ||
        b->last_seen_turn < strategy.offense_probe_start_turn ||
        S.enemy_region_count_turn[region] <
            strategy.offense_probe_start_turn) {
      offense_probe_ready = false;
      break;
    }
  }
  if (offense_probe_ready && strategy.offense_probe_complete_turn < 0)
    strategy.offense_probe_complete_turn = S.turn;
  // If no attack was possible with the completed snapshot, refresh the front
  // periodically rather than trusting an old garrison indefinitely.
  if (offense_probe_ready &&
      S.turn - strategy.offense_probe_complete_turn > 8) {
    strategy.offense_probe_start_turn = S.turn;
    strategy.offense_probe_complete_turn = -1;
    offense_probe_ready = false;
  }

  std::vector<char> in_offense_probe_cluster(N, false);
  for (int region : strategy.offense_probe_regions)
    in_offense_probe_cluster[region] = true;
  std::vector<std::vector<int>> remembered_enemy_hps(N);
  for (const auto &intel : S.enemy_memory) {
    if (intel.region < 0 || intel.region >= N ||
        (is_visible[intel.region] && !intel.visible))
      continue;
    remembered_enemy_hps[intel.region].push_back(std::max(0, intel.hp));
  }

  dbg::note(turn, "OFFENSE_PROBE active=" +
                      std::string(strategy.offense_probe_regions.empty()
                                      ? "0"
                                      : "1") +
                      " ready=" +
                      std::string(offense_probe_ready ? "1" : "0") +
                      " start=" +
                      std::to_string(strategy.offense_probe_start_turn) +
                      " complete=" +
                      std::to_string(strategy.offense_probe_complete_turn) +
                      " regions=" +
                      std::to_string(strategy.offense_probe_regions.size()));

  // Launch only genuine economic surplus after the probe has covered its
  // complete cluster.  The existing combat margin remains unchanged at two
  // warriors beyond the simulated minimum.
  struct Launch {
    const Building *target = nullptr;
    int source = -1;
    int count = 0;
    int score = std::numeric_limits<int>::min();
    std::vector<const Warrior *> warriors;
  } best_launch;

  for (const auto &target : S.buildings) {
    if (!income_ready_for_attack || !offense_probe_ready)
      break;
    if (target.side != enemy_side || target.type != BType::BASE ||
        !in_offense_probe_cluster[target.region])
      continue;
    if (target.last_seen_turn < strategy.offense_probe_start_turn ||
        S.enemy_region_count_turn[target.region] <
            strategy.offense_probe_start_turn)
      continue;

    for (int source = 0; source < N; ++source) {
      std::vector<const Warrior *> group;
      for (const auto &w : S.warriors) {
        if (w.id.side == M.my_side && w.region == source &&
            w.state == WState::STATIONARY && !used(w.id) &&
            !is_scout_purpose(w.purpose) &&
            !(w.purpose == WPurpose::BUILD && building_at[w.region] == nullptr))
          group.push_back(&w);
      }
      const Building *home = building_at[source];
      int keep = (home != nullptr && home->side == M.my_side)
                     ? std::max(1, home->work_cap())
                     : 0;
      if ((int)group.size() <= keep)
        continue;
      std::sort(group.begin(), group.end(), [](const Warrior *lhs,
                                               const Warrior *rhs) {
        if (lhs->hp != rhs->hp)
          return lhs->hp > rhs->hp;
        return lhs->id.num < rhs->id.num;
      });
      group.resize(group.size() - keep);
      std::vector<int> hps;
      for (const Warrior *w : group)
        hps.push_back(w->hp);
      int need =
          minimum_attackers(target, remembered_enemy_hps[target.region], hps);
      if (need >= 1000000000)
        continue;
      need += 2;
      if ((int)group.size() < need || need > attackable_surplus)
        continue;
      int travel = P.hops[source][target.region];
      if (travel >= 1000000000 || turn + travel + 30 >= MAX_TURN)
        continue;
      int score = 20000 - travel * 500 - need * 30;
      if (enemy_greedy)
        score += 18000;
      if (score > best_launch.score) {
        best_launch = Launch{&target, source, need, score, std::move(group)};
      }
    }
  }
  if (best_launch.target != nullptr) {
    for (int i = 0; i < best_launch.count; ++i)
      queue_move(*best_launch.warriors[i], best_launch.target->region,
                 WPurpose::ATTACK);
    // Keep the dedicated probe scout, but require a new snapshot before the
    // next normal offensive.  If this attack captures a base, the cluster is
    // reselected automatically from the remaining enemy buildings.
    strategy.offense_probe_start_turn = S.turn;
    strategy.offense_probe_complete_turn = -1;
    offense_probe_ready = false;
  }

  // 8) Keep the permanent HQ sentry and roaming army tracker, then add the
  // dedicated pre-offensive probe after HQ 2 unlocks the mobile scout layer.
  int enemy_natural = -1;
  int enemy_natural_hops = 1000000000;
  for (int r : M.strongholds) {
    int h = P.hops[M.opp_hq][r];
    if (h < enemy_natural_hops ||
        (h == enemy_natural_hops && r < enemy_natural)) {
      enemy_natural_hops = h;
      enemy_natural = r;
    }
  }

  struct ScoutObjective {
    int region;
    int cadence;
    int priority;
  };
  std::vector<ScoutObjective> scout_objectives;
  std::vector<int> objective_index(N, -1);
  auto add_objective = [&](int region, int cadence, int priority) {
    if (region < 0 || region >= N)
      return;
    if (objective_index[region] != -1) {
      ScoutObjective &old = scout_objectives[objective_index[region]];
      old.cadence = std::min(old.cadence, cadence);
      old.priority = std::max(old.priority, priority);
      return;
    }
    objective_index[region] = (int)scout_objectives.size();
    scout_objectives.push_back({region, cadence, priority});
  };

  std::vector<int> enemy_likely_at(N, 0), enemy_likely_hp(N, 0);
  for (const auto &intel : S.enemy_memory) {
    if (intel.region < 0 || intel.region >= N)
      continue;
    // If the remembered region is visible now but this unit is absent from
    // the snapshot, its old position has been disproved.  Keep the unit in
    // global force memory, but do not make the roaming scout guard a ghost.
    if (is_visible[intel.region] && !intel.visible)
      continue;
    ++enemy_likely_at[intel.region];
    enemy_likely_hp[intel.region] += std::max(0, intel.hp);
  }

  add_objective(M.opp_hq, 5, 1000000);

  std::vector<int> enemy_patrol_regions;
  std::vector<char> is_enemy_patrol_region(N, false);
  auto add_patrol_region = [&](int region) {
    if (region < 0 || region >= N || region == M.opp_hq ||
        is_enemy_patrol_region[region])
      return;
    is_enemy_patrol_region[region] = true;
    enemy_patrol_regions.push_back(region);
  };
  for (int r : M.strongholds) {
    if (P.dist[M.opp_hq][r] < P.dist[M.my_hq][r])
      add_patrol_region(r);
  }
  // An enemy base discovered outside its nominal half is also part of the
  // patrol, because it can become the true largest staging garrison.
  for (const auto &b : S.buildings)
    if (b.side == enemy_side && b.type == BType::BASE)
      add_patrol_region(b.region);
  if (enemy_patrol_regions.empty())
    add_patrol_region(enemy_natural);
  std::sort(enemy_patrol_regions.begin(), enemy_patrol_regions.end());

  bool patrol_complete = roaming_scout_enabled &&
                         !enemy_patrol_regions.empty();
  for (int region : enemy_patrol_regions) {
    if (S.enemy_region_count_turn[region] <
        strategy.roaming_patrol_start_turn) {
      patrol_complete = false;
      break;
    }
  }

  int strongest_patrol_region = -1;
  if (patrol_complete && strategy.roaming_tracked_ids.empty()) {
    for (int region : enemy_patrol_regions) {
      if (S.enemy_region_count_memory[region] <= 0)
        continue;
      if (strongest_patrol_region == -1 ||
          S.enemy_region_count_memory[region] >
              S.enemy_region_count_memory[strongest_patrol_region] ||
          (S.enemy_region_count_memory[region] ==
               S.enemy_region_count_memory[strongest_patrol_region] &&
           enemy_likely_hp[region] >
               enemy_likely_hp[strongest_patrol_region]) ||
          (S.enemy_region_count_memory[region] ==
               S.enemy_region_count_memory[strongest_patrol_region] &&
           enemy_likely_hp[region] ==
               enemy_likely_hp[strongest_patrol_region] &&
           S.enemy_region_count_turn[region] >
               S.enemy_region_count_turn[strongest_patrol_region]) ||
          (S.enemy_region_count_memory[region] ==
               S.enemy_region_count_memory[strongest_patrol_region] &&
           enemy_likely_hp[region] ==
               enemy_likely_hp[strongest_patrol_region] &&
           S.enemy_region_count_turn[region] ==
               S.enemy_region_count_turn[strongest_patrol_region] &&
           region < strongest_patrol_region))
        strongest_patrol_region = region;
    }

    // Do not lock a stale count.  Travel back until the candidate is visible,
    // then bind the exact visible IDs; those identities are what we follow.
    if (strongest_patrol_region != -1 &&
        is_visible[strongest_patrol_region]) {
      for (const auto &intel : S.enemy_memory) {
        if (!intel.visible || intel.region != strongest_patrol_region)
          continue;
        strategy.roaming_tracked_ids.push_back(intel.id);
      }
      if (!strategy.roaming_tracked_ids.empty()) {
        tracked_stack_region = strongest_patrol_region;
        tracked_at[tracked_stack_region] =
            (int)strategy.roaming_tracked_ids.size();
      }
    }
  }

  // Freeze the exact IDs seen when the largest garrison was selected.  Later
  // reinforcements may merely pass through that region, so absorbing them
  // would let a small departing subgroup drag the scout away from the army it
  // was originally assigned to follow.  If the locked army itself splits,
  // tracked_stack_region still follows its largest surviving subgroup.

  if (roaming_scout_enabled) {
    if (tracked_stack_region != -1) {
      add_objective(tracked_stack_region, 0, 990000);
    } else if (patrol_complete && strongest_patrol_region != -1) {
      add_objective(strongest_patrol_region, 0, 900000);
    } else {
      bool has_unsurveyed = false;
      for (int region : enemy_patrol_regions) {
        if (S.enemy_region_count_turn[region] >=
            strategy.roaming_patrol_start_turn)
          continue;
        has_unsurveyed = true;
        add_objective(region, 0, 600000);
      }
      // If a full sweep found no garrison, keep cycling through the enemy
      // strongholds by staleness until a stack appears.
      if (!has_unsurveyed)
        for (int region : enemy_patrol_regions)
          add_objective(region, 6, 500000);
    }
  }
  dbg::note(turn, "SCOUT_ROAM enabled=" +
                      std::string(roaming_scout_enabled ? "1" : "0") +
                      " patrol_complete=" +
                      std::string(patrol_complete ? "1" : "0") +
                      " candidate=R" +
                      std::to_string(strongest_patrol_region) +
                      " tracked=R" + std::to_string(tracked_stack_region) +
                      " tracked_units=" +
                      std::to_string(strategy.roaming_tracked_ids.size()));

  reserved_future_scouts.clear();

  // Coverage scouts care only about strategically relevant enemy-side
  // strongholds.  Known enemy bases remain relevant even when they lie
  // outside the nominal enemy half; the enemy HQ is handled separately by
  // the permanent HQ sentry.
  std::vector<int> important_enemy_strongholds;
  std::vector<char> is_important_enemy_stronghold(N, false);
  auto add_important_enemy_stronghold = [&](int region) {
    if (region < 0 || region >= N || region == M.opp_hq ||
        is_important_enemy_stronghold[region])
      return;
    is_important_enemy_stronghold[region] = true;
    important_enemy_strongholds.push_back(region);
  };
  for (int region : M.strongholds)
    if (P.dist[M.opp_hq][region] < P.dist[M.my_hq][region])
      add_important_enemy_stronghold(region);
  for (const auto &b : S.buildings)
    if (b.side == enemy_side && b.type == BType::BASE)
      add_important_enemy_stronghold(b.region);
  std::sort(important_enemy_strongholds.begin(),
            important_enemy_strongholds.end());

  std::vector<char> reserved_scout_post(N, false);
  int active_scouts = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || !is_scout_purpose(w.purpose))
      continue;
    ++active_scouts;
    if (w.state == WState::MOVING && w.target >= 0 && w.target < N)
      reserved_scout_post[w.target] = true;
  }
  if (hq_scout_ordered_this_turn)
    ++active_scouts;

  struct ScoutChoice {
    int post = -1;
    int priority = -1;
    long long score = std::numeric_limits<long long>::min();
  };

  // The engine cannot retarget a moving unit.  Scout orders therefore use one
  // safe edge at a time.  A fresh BFS is run every turn so patrols can route
  // around remembered armies/buildings and a tail can react immediately when
  // its tracked stack moves.
  auto scout_region_is_safe = [&](const Warrior &scout, int region) {
    if (region < 0 || region >= N || region == M.opp_hq ||
        enemy_likely_at[region] > 0)
      return false;
    const Building *b = building_at[region];
    if (b != nullptr && b->side == enemy_side)
      return false;
    for (const auto &intel : S.enemy_memory) {
      if (is_hq_sentry(scout) && intel.region == M.opp_hq)
        continue;
      if (intel.region < 0 || intel.region >= N)
        continue;
      const int distance = P.vision_hops[region][intel.region];
      if (!is_hq_sentry(scout) && scout.purpose == WPurpose::SCOUT &&
          intel.region == tracked_stack_region &&
          is_tracked_enemy(intel.id) && distance == 1)
        continue;
      if (distance <= 1)
        return false;
    }
    return true;
  };
  auto safe_scout_leg = [&](const Warrior &scout, int objective,
                            int observation_radius = 0) {
    if (objective < 0 || objective >= N ||
        P.vision_hops[scout.region][objective] <= observation_radius)
      return -1;

    std::vector<int> previous(N, -1);
    std::vector<int> queue{scout.region};
    previous[scout.region] = scout.region;
    int reached = -1;
    for (int head = 0; head < (int)queue.size(); ++head) {
      const int region = queue[head];
      if (region != scout.region &&
          P.vision_hops[region][objective] <= observation_radius) {
        reached = region;
        break;
      }
      for (int next : M.adj[region]) {
        if (previous[next] != -1 || !scout_region_is_safe(scout, next))
          continue;
        previous[next] = region;
        queue.push_back(next);
      }
    }
    if (reached == -1)
      return -1;
    while (previous[reached] != scout.region)
      reached = previous[reached];
    return reached;
  };

  // HQ vision is a hard assignment, not a normal patrol objective.  If the
  // sentry was displaced outside the two-hop radius, move it one edge toward
  // the nearest usable observation post every turn.  Only current occupancy
  // and enemy buildings block this recovery step; stale enemy-memory records
  // must not strand the sentry outside HQ vision forever.
  auto choose_hq_recovery_leg = [&](const Warrior &scout) {
    // Preserve the established safe opening route until the HQ has actually
    // been observed once.  The recovery rule below is specifically for a
    // sentry that was displaced after reaching its assignment.
    if (S.last_seen_region_turn[M.opp_hq] < 0 && enemy_hq_watch >= 0) {
      const int waypoint = safe_scout_leg(scout, enemy_hq_watch);
      if (waypoint >= 0)
        return ScoutChoice{waypoint, 1000000,
                           -(long long)P.hops[scout.region][enemy_hq_watch]};
    }

    ScoutChoice best;
    int best_travel = 1000000000;
    int best_watch_preference = -1;
    for (int post = 0; post < N; ++post) {
      if (post == M.opp_hq || post == scout.region ||
          P.vision_hops[post][M.opp_hq] > HOP_VISION ||
          enemy_at[post] > 0)
        continue;
      const Building *post_building = building_at[post];
      if (post_building != nullptr && post_building->side == enemy_side)
        continue;

      const int travel = P.hops[scout.region][post];
      if (travel <= 0 || travel >= 1000000000)
        continue;
      const int waypoint = P.nxt[scout.region][post];
      if (waypoint < 0 || waypoint == scout.region ||
          waypoint == M.opp_hq || enemy_at[waypoint] > 0)
        continue;
      const Building *waypoint_building = building_at[waypoint];
      if (waypoint_building != nullptr &&
          waypoint_building->side == enemy_side)
        continue;

      const int watch_preference = post == enemy_hq_watch;
      if (travel < best_travel ||
          (travel == best_travel &&
           watch_preference > best_watch_preference) ||
          (travel == best_travel &&
           watch_preference == best_watch_preference &&
           (best.post == -1 || waypoint < best.post))) {
        best_travel = travel;
        best_watch_preference = watch_preference;
        best = {waypoint, 1000000, -(long long)travel};
      }
    }
    return best;
  };

  auto choose_scout_post = [&](const Warrior &scout, bool hq_role) {
    if (hq_role)
      return choose_hq_recovery_leg(scout);

    ScoutChoice best;
    const Building *current_building = building_at[scout.region];
    const bool must_leave_workplace =
        current_building != nullptr && current_building->side == M.my_side;
    for (const auto &objective : scout_objectives) {
      const bool is_hq_objective = objective.region == M.opp_hq;
      if (hq_role != is_hq_objective)
        continue;
      const int observation_radius =
          !hq_role && objective.region == tracked_stack_region
              ? 1
              : HOP_VISION;
      const int waypoint =
          safe_scout_leg(scout, objective.region, observation_radius);
      if (waypoint < 0 || reserved_scout_post[waypoint])
        continue;
      int travel = std::max(
          0, P.hops[scout.region][objective.region] - observation_radius);
      if (travel >= 1000000000)
        continue;
      int last_seen = S.last_seen_region_turn[objective.region];
      int age = last_seen < 0 ? turn + 30 : turn - last_seen;
      int projected_age = age + travel;
      if (!must_leave_workplace && projected_age < objective.cadence)
        continue;
      const long long score =
          (long long)std::min(projected_age, 40) * 4000 -
          (long long)travel * 120;
      if (objective.priority > best.priority ||
          (objective.priority == best.priority && score > best.score) ||
          (objective.priority == best.priority && score == best.score &&
           waypoint < best.post))
        best = {waypoint, objective.priority, score};
    }
    return best;
  };

  auto choose_offense_scout_post = [&](const Warrior &scout) {
    ScoutChoice best;
    for (int region : strategy.offense_probe_regions) {
      const Building *target = building_at[region];
      if (target == nullptr || target->side != enemy_side ||
          target->type != BType::BASE)
        continue;
      if (strategy.offense_probe_seen_turn[region] >=
          strategy.offense_probe_start_turn)
        continue;
      const int waypoint = safe_scout_leg(scout, region, HOP_VISION);
      if (waypoint < 0 || reserved_scout_post[waypoint])
        continue;
      const int travel = std::max(0, P.hops[scout.region][region] - HOP_VISION);
      if (travel >= 1000000000)
        continue;
      const int last_seen = std::min(target->last_seen_turn,
                                     S.enemy_region_count_turn[region]);
      const int age = last_seen < 0 ? turn + 30 : turn - last_seen;
      const long long score =
          (long long)std::min(age, 40) * 4000 - (long long)travel * 120;
      if (best.post == -1 || score > best.score ||
          (score == best.score && waypoint < best.post))
        best = {waypoint, 800000, score};
    }
    return best;
  };

  struct VisionChoice {
    int post = -1;
    int waypoint = -1;
    int new_gain = -1;
    int unique_cover = -1;
    int total_cover = -1;
    int travel = 1000000000;
  };
  std::vector<char> reserved_vision_post(N, false);

  // A coverage route never enters or becomes adjacent to an important enemy
  // stronghold.  Stopping exactly two hops away is enough to reveal it while
  // avoiding immediate same/next-region contact with an unseen garrison.
  auto vision_region_is_safe = [&](const Warrior &scout, int region) {
    if (!scout_region_is_safe(scout, region))
      return false;
    for (int target : important_enemy_strongholds)
      if (P.vision_hops[region][target] < HOP_VISION)
        return false;
    return true;
  };

  // Route safety uses current confirmed enemies.  Applying every stale fog
  // memory record to every intermediate edge can falsely disconnect the map;
  // the final observation post still uses the stricter persistent-memory
  // check above.
  auto vision_route_region_is_safe = [&](int region) {
    if (region < 0 || region >= N || region == M.opp_hq ||
        enemy_likely_at[region] > 0)
      return false;
    const Building *b = building_at[region];
    if (b != nullptr && b->side == enemy_side)
      return false;
    for (const auto &enemy : S.warriors)
      if (enemy.id.side == enemy_side &&
          P.vision_hops[region][enemy.region] <= 1)
        return false;
    for (int target : important_enemy_strongholds)
      if (P.vision_hops[region][target] < HOP_VISION)
        return false;
    return true;
  };

  // Find the safest reachable observation post for one coverage scout.  The
  // primary score is the number of currently invisible, not-yet-claimed
  // important strongholds revealed there.  Subsequent scores spread scouts
  // across all important strongholds before considering travel distance.
  auto choose_vision_scout_post = [&](const Warrior &scout,
                                      const std::vector<char> &claimed,
                                      bool allow_hold,
                                      int forced_post = -1) {
    VisionChoice best;
    std::vector<int> previous(N, -1), travel(N, 1000000000);
    std::vector<int> queue{scout.region};
    previous[scout.region] = scout.region;
    travel[scout.region] = 0;
    for (int head = 0; head < (int)queue.size(); ++head) {
      const int region = queue[head];
      for (int next : M.adj[region]) {
        if (previous[next] != -1 || !vision_route_region_is_safe(next))
          continue;
        previous[next] = region;
        travel[next] = travel[region] + 1;
        queue.push_back(next);
      }
    }

    for (int post = 0; post < N; ++post) {
      if ((forced_post >= 0 && post != forced_post) ||
          previous[post] == -1 || !vision_region_is_safe(scout, post) ||
          reserved_vision_post[post] ||
          (post == scout.region &&
           (!allow_hold || !vision_region_is_safe(scout, post))))
        continue;

      int waypoint = -1;
      if (post != scout.region) {
        waypoint = post;
        while (previous[waypoint] != scout.region)
          waypoint = previous[waypoint];
        if (reserved_scout_post[waypoint])
          continue;
      }

      int new_gain = 0;
      int unique_cover = 0;
      int total_cover = 0;
      for (int target : important_enemy_strongholds) {
        if (P.vision_hops[post][target] > HOP_VISION)
          continue;
        ++total_cover;
        if (!claimed[target]) {
          ++unique_cover;
          if (!is_visible[target])
            ++new_gain;
        }
      }
      if (total_cover == 0)
        continue;

      if (best.post == -1 || new_gain > best.new_gain ||
          (new_gain == best.new_gain &&
           unique_cover > best.unique_cover) ||
          (new_gain == best.new_gain &&
           unique_cover == best.unique_cover &&
           total_cover > best.total_cover) ||
          (new_gain == best.new_gain &&
           unique_cover == best.unique_cover &&
           total_cover == best.total_cover && travel[post] < best.travel) ||
          (new_gain == best.new_gain &&
           unique_cover == best.unique_cover &&
           total_cover == best.total_cover && travel[post] == best.travel &&
           post < best.post)) {
        best = {post, waypoint, new_gain, unique_cover, total_cover,
                travel[post]};
      }
    }
    return best;
  };

  // Persist each coverage scout's final observation post while it advances
  // in safe one-edge legs.  Without this lock, changing fog information can
  // make a scout alternate between two equally valuable distant posts.
  strategy.vision_posts.erase(
      std::remove_if(strategy.vision_posts.begin(),
                     strategy.vision_posts.end(),
                     [&](const std::pair<WarriorId, int> &assignment) {
                       const Warrior *warrior = nullptr;
                       for (const auto &candidate : S.warriors)
                         if (candidate.id == assignment.first) {
                           warrior = &candidate;
                           break;
                         }
                       return warrior == nullptr || is_hq_sentry(*warrior) ||
                              warrior->purpose != WPurpose::VISION_SCOUT;
                     }),
      strategy.vision_posts.end());
  auto vision_post_for = [&](WarriorId id) {
    for (const auto &assignment : strategy.vision_posts)
      if (assignment.first == id)
        return assignment.second;
    return -1;
  };
  auto clear_vision_post = [&](WarriorId id) {
    strategy.vision_posts.erase(
        std::remove_if(strategy.vision_posts.begin(),
                       strategy.vision_posts.end(),
                       [&](const std::pair<WarriorId, int> &assignment) {
                         return assignment.first == id;
                       }),
        strategy.vision_posts.end());
  };
  auto set_vision_post = [&](WarriorId id, int post) {
    clear_vision_post(id);
    strategy.vision_posts.push_back({id, post});
  };

  auto has_current_high_value_watch = [&](const Warrior &scout,
                                           bool hq_role) {
    if (hq_role)
      return P.vision_hops[scout.region][M.opp_hq] <= HOP_VISION;
    return tracked_stack_region != -1 &&
           P.vision_hops[scout.region][tracked_stack_region] <= 1;
  };

  const bool offense_probe_active =
      !strategy.offense_probe_regions.empty();
  const bool offense_scout_required =
      offense_probe_active && (!offense_probe_ready || offense_scout_exists);
  const int current_hq_level = hq != nullptr ? hq->level : 1;
  // The permanent enemy-HQ sentry is separate.  HQ 2, 3, and 4+ support two,
  // three, and four mobile scouts respectively; unused specialized slots are
  // assigned to maximum-coverage vision duty.
  const int mobile_scout_limit =
      roaming_scout_enabled
          ? std::min(4, std::max(2, current_hq_level))
          : 0;
  const int scout_limit = 1 + mobile_scout_limit;

  // Existing specialized scouts keep their roles.  Coverage scouts are
  // deliberately skipped here and planned together below so their fields of
  // view can be de-duplicated.
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || !is_scout_purpose(w.purpose) ||
        w.state != WState::STATIONARY || used(w.id))
      continue;
    const bool hq_role = is_hq_sentry(w);
    ScoutChoice choice;
    WPurpose next_purpose = w.purpose;
    if (hq_role) {
      if (has_current_high_value_watch(w, true))
        continue;
      choice = choose_scout_post(w, true);
      next_purpose = WPurpose::HQ_SCOUT;
    } else if (w.purpose == WPurpose::OFFENSE_SCOUT) {
      if (!offense_scout_required || offense_probe_ready)
        continue;
      choice = choose_offense_scout_post(w);
    } else if (w.purpose == WPurpose::SCOUT) {
      if (!roaming_scout_enabled || has_current_high_value_watch(w, false))
        continue;
      choice = choose_scout_post(w, false);
      next_purpose = WPurpose::SCOUT;
    } else {
      continue;
    }
    if (choice.post != -1 && queue_move(w, choice.post, next_purpose))
      reserved_scout_post[choice.post] = true;
  }

  bool hq_role_filled =
      hq_sentry != nullptr || hq_scout_ordered_this_turn;
  bool roaming_role_filled = false;
  bool offense_role_filled = false;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || !is_scout_purpose(w.purpose) ||
        is_hq_sentry(w))
      continue;
    if (w.purpose == WPurpose::OFFENSE_SCOUT)
      offense_role_filled = true;
    else if (w.purpose == WPurpose::SCOUT)
      roaming_role_filled = true;
  }

  // Fill mandatory roles first.  A stationary coverage scout may be
  // repurposed when a sentry/tracker/probe slot becomes vacant; otherwise a
  // normal surplus warrior is promoted without consuming economic labor.
  auto fill_special_scout_role = [&](WPurpose new_purpose) {
    const Warrior *best_scout = nullptr;
    ScoutChoice best_choice;
    const bool new_hq_role = new_purpose == WPurpose::HQ_SCOUT;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          used(w.id) || is_hq_sentry(w) ||
          (is_scout_purpose(w.purpose) &&
           w.purpose != WPurpose::VISION_SCOUT) ||
          !can_leave_without_losing_labor(w))
        continue;
      const bool committed_builder =
          w.purpose == WPurpose::BUILD && w.target >= 0 && w.target < N &&
          building_at[w.target] == nullptr;
      if (committed_builder)
        continue;
      const Building *home = building_at[w.region];
      int keep = (home != nullptr && home->side == M.my_side)
                     ? home->work_cap()
                     : 0;
      // The first trained scout is still preferred, but neither an HQ-sentry
      // replacement nor a roaming scout may consume a reserved worker.  The
      // common movement guard above has already enforced that invariant.
      if (new_hq_role && waiting_for_first_trained_scout)
        continue;
      if (!new_hq_role &&
          labor_at[w.region] - labor_leaving_from[w.region] <= keep)
        continue;
      ScoutChoice choice =
          new_purpose == WPurpose::OFFENSE_SCOUT
              ? choose_offense_scout_post(w)
              : choose_scout_post(w, new_hq_role);
      if (choice.post == -1)
        continue;
      if (choice.priority > best_choice.priority ||
          (choice.priority == best_choice.priority &&
           choice.score > best_choice.score) ||
          (choice.priority == best_choice.priority &&
           choice.score == best_choice.score &&
           (best_scout == nullptr || w.hp > best_scout->hp ||
            (w.hp == best_scout->hp && w.id.num < best_scout->id.num)))) {
        best_scout = &w;
        best_choice = choice;
      }
    }
    if (best_scout == nullptr ||
        !queue_move(*best_scout, best_choice.post, new_purpose))
      return false;
    reserved_scout_post[best_choice.post] = true;
    if (best_scout->purpose == WPurpose::VISION_SCOUT)
      clear_vision_post(best_scout->id);
    if (!is_scout_purpose(best_scout->purpose))
      ++active_scouts;
    return true;
  };

  if (!hq_role_filled && fill_special_scout_role(WPurpose::HQ_SCOUT))
    hq_role_filled = true;
  if (roaming_scout_enabled && !roaming_role_filled &&
      fill_special_scout_role(WPurpose::SCOUT))
    roaming_role_filled = true;
  if (offense_scout_required && !offense_role_filled &&
      !offense_probe_ready &&
      fill_special_scout_role(WPurpose::OFFENSE_SCOUT))
    offense_role_filled = true;

  const int desired_vision_roles = std::max(
      0, mobile_scout_limit - (roaming_role_filled ? 1 : 0) -
             (offense_scout_required && offense_role_filled ? 1 : 0));

  // Specialized scouts already cover some important strongholds.  Their
  // current and newly ordered one-edge positions seed the coverage set so
  // VISION_SCOUT assignments do not duplicate those observations.
  std::vector<char> planned_important_covered(N, false);
  auto claim_important_from_post = [&](int post) {
    if (post < 0 || post >= N)
      return;
    for (int target : important_enemy_strongholds)
      if (P.vision_hops[post][target] <= HOP_VISION)
        planned_important_covered[target] = true;
  };
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || !is_scout_purpose(w.purpose) ||
        w.purpose == WPurpose::VISION_SCOUT)
      continue;
    claim_important_from_post(w.region);
  }
  for (const auto &move : a.moves)
    if (is_scout_purpose(move.purpose) &&
        move.purpose != WPurpose::VISION_SCOUT)
      claim_important_from_post(move.target);

  auto better_vision_choice = [&](const VisionChoice &candidate,
                                  const Warrior &candidate_scout,
                                  const VisionChoice &best,
                                  const Warrior *best_scout) {
    return best_scout == nullptr || candidate.new_gain > best.new_gain ||
           (candidate.new_gain == best.new_gain &&
            candidate.unique_cover > best.unique_cover) ||
           (candidate.new_gain == best.new_gain &&
            candidate.unique_cover == best.unique_cover &&
            candidate.total_cover > best.total_cover) ||
           (candidate.new_gain == best.new_gain &&
            candidate.unique_cover == best.unique_cover &&
            candidate.total_cover == best.total_cover &&
            candidate.travel < best.travel) ||
           (candidate.new_gain == best.new_gain &&
            candidate.unique_cover == best.unique_cover &&
            candidate.total_cover == best.total_cover &&
            candidate.travel == best.travel &&
            candidate_scout.hp > best_scout->hp) ||
           (candidate.new_gain == best.new_gain &&
            candidate.unique_cover == best.unique_cover &&
            candidate.total_cover == best.total_cover &&
            candidate.travel == best.travel &&
            candidate_scout.hp == best_scout->hp &&
            candidate_scout.id.num < best_scout->id.num);
  };

  auto log_vision_assignment = [&](const Warrior &scout,
                                   const VisionChoice &choice) {
    int nearest_enemy = N + 1;
    for (const auto &intel : S.enemy_memory)
      if (intel.region >= 0 && intel.region < N)
        nearest_enemy =
            std::min(nearest_enemy,
                     P.vision_hops[choice.post][intel.region]);
    dbg::note(turn, "VISION_ASSIGN " + format_warrior(scout.id) +
                        " post=R" + std::to_string(choice.post) +
                        " waypoint=R" + std::to_string(choice.waypoint) +
                        " new_targets=" +
                        std::to_string(choice.new_gain) +
                        " unique_targets=" +
                        std::to_string(choice.unique_cover) +
                        " total_targets=" +
                        std::to_string(choice.total_cover) +
                        " travel=" + std::to_string(choice.travel) +
                        " nearest_enemy=" +
                        std::to_string(nearest_enemy));
  };

  int fixed_vision_roles = 0;
  std::vector<const Warrior *> pending_vision_scouts;
  bool any_unseen_important = false;
  for (int target : important_enemy_strongholds)
    if (!is_visible[target]) {
      any_unseen_important = true;
      break;
    }
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side || is_hq_sentry(w) || used(w.id))
      continue;
    const bool is_vision = w.purpose == WPurpose::VISION_SCOUT;
    const bool stale_offense =
        w.purpose == WPurpose::OFFENSE_SCOUT && !offense_scout_required;
    if (!is_vision && !stale_offense)
      continue;
    if (w.state == WState::MOVING) {
      ++fixed_vision_roles;
      const int locked_post = is_vision ? vision_post_for(w.id) : -1;
      const int planned_post = locked_post >= 0 ? locked_post : w.target;
      claim_important_from_post(planned_post);
      if (planned_post >= 0 && planned_post < N)
        reserved_vision_post[planned_post] = true;
    } else {
      pending_vision_scouts.push_back(&w);
    }
  }

  int vision_slots_left =
      std::max(0, desired_vision_roles - fixed_vision_roles);
  // Re-plan existing coverage scouts jointly, always taking the largest
  // marginal new-stronghold gain first.  This is the standard maximum-
  // coverage allocation and prevents two scouts from selecting the same
  // already-accounted-for targets.
  while (vision_slots_left > 0 && !pending_vision_scouts.empty()) {
    int best_index = -1;
    const Warrior *best_scout = nullptr;
    VisionChoice best_choice;
    for (int i = 0; i < (int)pending_vision_scouts.size(); ++i) {
      const Warrior &candidate_scout = *pending_vision_scouts[i];
      const bool allow_hold =
          candidate_scout.purpose == WPurpose::VISION_SCOUT;
      int locked_post = allow_hold ? vision_post_for(candidate_scout.id) : -1;
      bool locked_covers_unseen = false;
      if (locked_post >= 0 && locked_post < N &&
          vision_region_is_safe(candidate_scout, locked_post)) {
        for (int target : important_enemy_strongholds)
          if (!is_visible[target] &&
              P.vision_hops[locked_post][target] <= HOP_VISION) {
            locked_covers_unseen = true;
            break;
          }
      } else {
        locked_post = -1;
      }
      const bool keep_lock =
          locked_post >= 0 &&
          (locked_covers_unseen || !any_unseen_important);
      VisionChoice choice = choose_vision_scout_post(
          candidate_scout, planned_important_covered, allow_hold,
          keep_lock ? locked_post : -1);
      if (choice.post == -1 && keep_lock)
        choice = choose_vision_scout_post(
            candidate_scout, planned_important_covered, allow_hold);
      if (choice.post == -1 ||
          !better_vision_choice(choice, candidate_scout, best_choice,
                                best_scout))
        continue;
      best_index = i;
      best_scout = &candidate_scout;
      best_choice = choice;
    }
    if (best_scout == nullptr) {
      // These warriors still fill their existing slots even if no safe
      // observation post is currently reachable.
      vision_slots_left =
          std::max(0, vision_slots_left -
                          (int)pending_vision_scouts.size());
      break;
    }

    bool assigned_vision = best_choice.waypoint == -1;
    if (!assigned_vision)
      assigned_vision = queue_move(*best_scout, best_choice.waypoint,
                                   WPurpose::VISION_SCOUT);
    if (assigned_vision) {
      if (best_choice.waypoint >= 0)
        reserved_scout_post[best_choice.waypoint] = true;
      reserved_vision_post[best_choice.post] = true;
      claim_important_from_post(best_choice.post);
      set_vision_post(best_scout->id, best_choice.post);
      log_vision_assignment(*best_scout, best_choice);
    }
    pending_vision_scouts.erase(pending_vision_scouts.begin() + best_index);
    --vision_slots_left;
  }

  // Promote additional surplus warriors until the HQ-level mobile pool is
  // full.  Every promotion is evaluated against the coverage already claimed
  // by earlier assignments, so its score is its true marginal contribution.
  while (vision_slots_left > 0 && active_scouts < scout_limit) {
    const Warrior *best_scout = nullptr;
    VisionChoice best_choice;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.state != WState::STATIONARY ||
          used(w.id) || is_scout_purpose(w.purpose) ||
          !can_leave_without_losing_labor(w))
        continue;
      const bool committed_builder =
          w.purpose == WPurpose::BUILD && w.target >= 0 && w.target < N &&
          building_at[w.target] == nullptr;
      if (committed_builder)
        continue;
      const Building *home = building_at[w.region];
      const int keep = (home != nullptr && home->side == M.my_side)
                           ? home->work_cap()
                           : 0;
      if (labor_at[w.region] - labor_leaving_from[w.region] <= keep)
        continue;
      const VisionChoice choice = choose_vision_scout_post(
          w, planned_important_covered, false);
      if (choice.post == -1 ||
          !better_vision_choice(choice, w, best_choice, best_scout))
        continue;
      best_scout = &w;
      best_choice = choice;
    }
    if (best_scout == nullptr ||
        !queue_move(*best_scout, best_choice.waypoint,
                    WPurpose::VISION_SCOUT))
      break;
    reserved_scout_post[best_choice.waypoint] = true;
    reserved_vision_post[best_choice.post] = true;
    claim_important_from_post(best_choice.post);
    set_vision_post(best_scout->id, best_choice.post);
    log_vision_assignment(*best_scout, best_choice);
    ++active_scouts;
    --vision_slots_left;
  }

  dbg::note(turn, "SCOUT_POOL hq_level=" +
                      std::to_string(current_hq_level) +
                      " mobile_limit=" +
                      std::to_string(mobile_scout_limit) +
                      " total_limit=" + std::to_string(scout_limit) +
                      " roaming=" + (roaming_role_filled ? "1" : "0") +
                      " offense=" + (offense_role_filled ? "1" : "0") +
                      " desired_vision=" +
                      std::to_string(desired_vision_roles) +
                      " active=" + std::to_string(active_scouts));

  const int scout_train_need = std::max(0, scout_limit - active_scouts);

  // 9) Train toward a workforce plus a small mobile reserve.  Newly upgraded
  // HQ capacity is available because construction precedes training.
  int my_alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++my_alive;
  int projected_hq_level = hq != nullptr ? hq->level : 1;
  if (std::find(a.upgrades.begin(), a.upgrades.end(), M.my_hq) !=
          a.upgrades.end() &&
      projected_hq_level < HQ_MAX_LEVEL)
    ++projected_hq_level;
  int train_cap = HQ_LEVELS[projected_hq_level].train_cap;
  int mobile_reserve = std::min(12, 2 + owned_bases * 2);
  if (enemy_pressure)
    mobile_reserve += std::max(4, std::min(8, visible_enemy_army));
  else if (enemy_tech)
    mobile_reserve += 2;
  else if (enemy_greedy)
    mobile_reserve += 1;
  // Count capacity opened by today's upgrades immediately so the training
  // policy actually staffs the cheap base levels it just purchased.
  int projected_total_work_cap = total_work_cap;
  for (int region : a.upgrades) {
    const Building *b = building_at[region];
    if (b == nullptr) {
      projected_total_work_cap += BASE_LEVELS[1].work_cap;
    } else if (b->side == M.my_side && b->level < max_level(*b)) {
      const int next_cap =
          b->type == BType::HQ ? HQ_LEVELS[b->level + 1].work_cap
                               : BASE_LEVELS[b->level + 1].work_cap;
      projected_total_work_cap += next_cap - b->work_cap();
    }
  }

  // Scouts are an additional specialist slot: they do not satisfy workforce,
  // builder, or mobile-reserve demand even if they happen to stand on a base.
  const int economic_force =
      projected_total_work_cap + pending_builds +
      (expansion_waiting_for_builder ? 1 : 0) + mobile_reserve;
  int desired = economic_force + std::max(active_scouts, scout_limit);
  int want = std::max(
      {urgent_hq_train, desired - my_alive, force_train_need,
       scout_train_need});

  // Recompute the next development price after today's purchases.  Without
  // this projection, the economy phase would fail to afford (for example) a
  // 550-gold base upgrade and training would immediately spend every 120 gold
  // on the way there.  Reserve exactly the next step, never step+training.
  int projected_base_count = 0;
  int next_base_upgrade_cost = std::numeric_limits<int>::max();
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || b.type != BType::BASE)
      continue;
    int projected_level = b.level;
    if (std::find(a.upgrades.begin(), a.upgrades.end(), b.region) !=
            a.upgrades.end() &&
        projected_level < BASE_MAX_LEVEL)
      ++projected_level;
    ++projected_base_count;
    if (projected_level < BASE_MAX_LEVEL)
      next_base_upgrade_cost =
          std::min(next_base_upgrade_cost,
                   BASE_LEVELS[projected_level + 1].cost);
  }
  for (int region : a.upgrades) {
    if (building_at[region] != nullptr)
      continue;
    ++projected_base_count;
    next_base_upgrade_cost =
        std::min(next_base_upgrade_cost, BASE_LEVELS[2].cost);
  }

  int next_development_reserve = 0;
  if (pending_builds > 0 || expansion_waiting_for_builder) {
    next_development_reserve = BASE_LEVELS[1].cost;
  } else if (projected_hq_level < 2) {
    next_development_reserve =
        projected_base_count < 2 ? BASE_LEVELS[1].cost
                                 : HQ_LEVELS[2].upgrade_cost;
  } else if (projected_base_count < 3) {
    next_development_reserve = BASE_LEVELS[1].cost;
  } else if (next_base_upgrade_cost != std::numeric_limits<int>::max()) {
    next_development_reserve = next_base_upgrade_cost;
  } else if (projected_hq_level < HQ_MAX_LEVEL) {
    next_development_reserve =
        HQ_LEVELS[projected_hq_level + 1].upgrade_cost;
  }
  if (confirmed_hq_behind && projected_hq_level < HQ_MAX_LEVEL) {
    next_development_reserve =
        std::max(next_development_reserve,
                 HQ_LEVELS[projected_hq_level + 1].upgrade_cost);
  }
  reserve = next_development_reserve;
  // Confirmed combat-HP parity is the qualifier invariant.  Do not let the
  // next development purchase suppress training while that floor is unmet.
  if (defense_train_need > 0 ||
      (force_train_need > 0 && pending_builds == 0 &&
       !expansion_waiting_for_builder))
    reserve = 0;
  if (confirmed_hq_behind && projected_hq_level < HQ_MAX_LEVEL) {
    reserve = std::max(
        reserve, HQ_LEVELS[projected_hq_level + 1].upgrade_cost);
  }
  int affordable = std::max(0, (budget - reserve) / TRAIN_COST);
  a.train_n = std::max(0, std::min({train_cap, want, affordable}));
  // If no existing warrior could take the HQ-sentry role this turn, buy one
  // replacement even while saving for development.  It will be assigned on
  // the next turn and restores continuous enemy-HQ vision as soon as travel
  // permits.
  if (!hq_role_filled && train_cap > 0 && budget >= TRAIN_COST)
    a.train_n = std::max(a.train_n, 1);
  // Qualifier HQ catch-up rule: with a currently confirmed level deficit,
  // keep every remaining coin for HQ technology.  Once vision is lost or the
  // observed levels are no longer behind, normal training resumes.
  if (confirmed_hq_behind)
    a.train_n = 0;
  a.train_n = std::min(a.train_n, train_cap);
  budget -= a.train_n * TRAIN_COST;
  for (const auto &move : a.moves) {
    const Warrior *w = nullptr;
    for (const auto &candidate : S.warriors)
      if (candidate.id == move.id) {
        w = &candidate;
        break;
      }
    dbg::note(turn, "ACTION MOVE " + format_warrior(move.id) + " R" +
                        std::to_string(w != nullptr ? w->region : -1) +
                        "->R" + std::to_string(move.target) + " purpose=" +
                        dbg::purpose_name(move.purpose));
  }
  for (int region : a.upgrades) {
    const Building *b = building_at[region];
    std::string kind = "NEW_BASE";
    std::string level = "1";
    if (b != nullptr) {
      kind = b->type == BType::HQ ? "HQ" : "BASE";
      level = std::to_string(std::min(max_level(*b), b->level + 1));
    }
    dbg::note(turn, "ACTION UPGRADE R" + std::to_string(region) + " " +
                        kind + "->L" + level);
  }
  dbg::note(turn, "ACTION TRAIN " + std::to_string(a.train_n) +
                      " want=" + std::to_string(want) +
                      " scout_need=" +
                      std::to_string(scout_train_need) +
                      " affordable=" + std::to_string(affordable) +
                      " development_reserve=" + std::to_string(reserve) +
                      " confirmed_hq_behind=" +
                      (confirmed_hq_behind ? "1" : "0") +
                      " final_budget=" + std::to_string(budget));
  return a;
}

#ifndef NEXT_VISION_NO_MAIN
int main() {
  GameMap M;
  GameState S;
  StrategyState strategy;
  parse_init(M, S);
  Paths P = calculate_paths(M);
  dbg::init(M.my_side);

  int turn;
  while (read_turn_start(turn)) {
    dbg::turn_header(turn, S, M);
    Actions a = decide(S, M, P, strategy, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }
  return 0;
}
#endif
