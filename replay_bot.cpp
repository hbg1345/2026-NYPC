// ============================================================================
//  로그 재생(replay) 봇
//
//  대회 로그(트랜스크립트)의 COMMAND 블록을 그대로 재생한다. 심판이 이 봇에게
//  배정한 쪽(READY LEFT/RIGHT)의 명령을 매 턴 그대로 출력하므로, "로그 속 그
//  상대"를 고정 상대로 삼아 내 에이전트를 반대편에 붙여 같은 판을 재현할 수 있다.
//
//  [사용법]
//   1) 재현할 판의 로그 전체(파일 내용: 예 "5 (2).txt")를 아래
//      R"LOG( ... )LOG" 사이에 통째로 붙여넣는다.
//   2) 컴파일해서 심판에 상대 봇으로 넣고, 내 에이전트를 반대편에 넣는다.
//      (보통 내 에이전트 = LEFT, 이 봇 = RIGHT — 하지만 배정된 쪽을 자동 인식한다.)
//   3) 내 에이전트를 그대로 두면 로그와 100% 같은 판이 재현된다. 그 상태에서
//      내 에이전트(main.c++)에 std::cerr 디버그를 넣으면, "왜 그 판단을 했는지"를
//      바로 그 판에서 관찰할 수 있다.
//
//  [한계] 이 봇은 게임 상태를 안 본다 — 대본대로 명령만 낸다. 그래서 내
//  에이전트를 바꿔 판이 로그와 달라지면 이 봇의 명령이 현실과 안 맞을 수 있다
//  (이미 죽은 유닛 이동 등). 정확한 재현은 "내 에이전트 그대로"일 때만 보장된다.
//
//  [프로토콜] 핸드셰이크/턴결과 소비는 main.c++(parse_init / read_turn_start /
//  read_turn_result)의 라인 소비 순서를 그대로 따라, 심판과 싱크가 어긋나지
//  않게 한다. 로그의 "보이는 형식"이 아니라 그 검증된 순서를 기준으로 읽는다.
// ============================================================================
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ====================== 여기에 로그 전체를 붙여넣으세요 ======================
// (비워두면 아무 명령도 안 내는 "패스 봇"으로 컴파일·동작한다.)
static const char *kLog = R"LOG(

)LOG";
// ============================================================================

static std::string readln() {
  std::string s;
  if (!std::getline(std::cin, s))
    std::exit(0);
  return s;
}

static std::vector<std::string> toks(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream is(s);
  for (std::string t; is >> t;)
    out.push_back(t);
  return out;
}

// side: 0 = LEFT, 1 = RIGHT. 턴 -> 그 턴에 그 쪽이 낸 COMMAND 라인들.
static std::map<int, std::vector<std::string>> gCmds[2];

// 로그에서 뽑은 맵(검증용). 심판이 준 맵과 다르면 재현이 무의미하므로 경고한다.
static int logN = -1, logK = -1;
static std::string logX, logY;      // 좌표 라인(공백 정규화)
static std::vector<int> logStrong;  // 스트롱홀드(정렬)

// 토큰을 단일 공백으로 이어붙여 정규화(들여쓰기/중복공백 무시 비교용).
static std::string norm(const std::string &s) {
  std::string o;
  for (const auto &t : toks(s)) {
    if (!o.empty()) o += ' ';
    o += t;
  }
  return o;
}

// kLog를 파싱: (1) MAP 블록을 검증용으로, (2) 턴별 COMMAND 블록을 gCmds로.
static void parseLog() {
  std::istringstream in(kLog);
  std::vector<std::string> L; // 공백 제거한 비어있지 않은 줄들
  std::string raw;
  while (std::getline(in, raw)) {
    size_t a = raw.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) continue;
    size_t b = raw.find_last_not_of(" \t\r\n");
    L.push_back(raw.substr(a, b - a + 1));
  }

  // ---- (1) 맵 블록: "MAP" 다음 N K / x / y / STRONGHOLDS 줄 ----
  for (size_t i = 0; i + 4 < L.size(); ++i) {
    if (L[i] != "MAP") continue;
    auto nk = toks(L[i + 1]);
    if (nk.size() >= 2) { logN = std::stoi(nk[0]); logK = std::stoi(nk[1]); }
    logX = norm(L[i + 2]);
    logY = norm(L[i + 3]);
    for (const auto &t : toks(L[i + 4]))
      if (t != "STRONGHOLDS") logStrong.push_back(std::stoi(t));
    std::sort(logStrong.begin(), logStrong.end());
    break;
  }

  // ---- (2) 커맨드 블록 ----
  int curTurn = -1;
  int mode = -1; // -1 없음, 0 LEFT, 1 RIGHT
  for (const auto &line : L) {
    auto tk = toks(line);
    if (tk.empty()) continue;
    // "TURN k"(2토큰)=커맨드 구간 시작. "TURN k RESULT"(3토큰)는 제외.
    if (tk[0] == "TURN" && tk.size() == 2) { curTurn = std::stoi(tk[1]); mode = -1; continue; }
    if (tk[0] == "COMMAND" && tk.size() >= 3) {
      int sd = (tk[1] == "LEFT") ? 0 : (tk[1] == "RIGHT") ? 1 : -1;
      if (sd >= 0) { mode = (tk[2] == "START") ? sd : -1; continue; }
    }
    if (mode >= 0 && curTurn >= 0) gCmds[mode][curTurn].push_back(line);
  }
}

// main.c++ read_turn_result 와 동일한 순서로 턴 결과 블록을 읽어 버린다(상태는 무시).
static void consumeResult() {
  std::string line = readln(); // "TURN ..."
  if (line == "FINISH")
    std::exit(0);
  readln(); // 카운트다운 라인 (형식 무관하게 소비만)
  auto blk = [](bool idsLine) {
    auto t = toks(readln()); // "<NAME> N"
    int n = std::stoi(t.at(1));
    if (idsLine) {
      if (n > 0)
        readln(); // TRAIN: id들이 한 줄
    } else {
      for (int i = 0; i < n; ++i)
        readln();
    }
  };
  blk(false); // UPGRADE N + N줄
  blk(true);  // TRAIN N (+ n>0이면 id 한 줄)
  blk(false); // MOVE N + N줄
  blk(false); // DAMAGE N + N줄
  blk(false); // SIEGE N + N줄
  readln();   // "END"
}

int main() {
  parseLog();

  // ---- 핸드셰이크 / 초기화 (parse_init 과 동일한 라인 소비) ----
  int mySide = 0; // 0 LEFT, 1 RIGHT
  {
    auto t = toks(readln()); // "READY LEFT|RIGHT"
    mySide = (t.size() >= 2 && t[1] == "RIGHT") ? 1 : 0;
  }
  int N = 0;
  {
    auto t = toks(readln()); // "N K"
    N = std::stoi(t.at(0));
    int K = (t.size() >= 2) ? std::stoi(t[1]) : -1;
    if (logN >= 0 && (N != logN || K != logK))
      std::cerr << "[replay] MAP MISMATCH N/K: judge=" << N << "/" << K
                << " log=" << logN << "/" << logK
                << " -- 재현 불가(심판 맵이 로그와 다름)\n";
  }
  {
    std::string x = readln(); // x_0 ... x_{N-1}
    if (logN >= 0 && norm(x) != logX)
      std::cerr << "[replay] MAP MISMATCH: x-coords -- 심판 맵이 로그와 다름\n";
  }
  {
    std::string y = readln(); // y_0 ... y_{N-1}
    if (logN >= 0 && norm(y) != logY)
      std::cerr << "[replay] MAP MISMATCH: y-coords -- 심판 맵이 로그와 다름\n";
  }
  {
    auto t = toks(readln()); // 스트롱홀드(숫자들, 접두어 없음)
    std::vector<int> st;
    for (const auto &s : t) st.push_back(std::stoi(s));
    std::sort(st.begin(), st.end());
    if (logN >= 0 && st != logStrong)
      std::cerr << "[replay] MAP MISMATCH: strongholds -- 심판 맵이 로그와 다름\n";
  }
  for (int i = 0; i < N; ++i)  // 인접 리스트 N줄
    readln();
  std::cout << "OK" << std::endl;

  // ---- 턴 루프 (read_turn_start 와 동일) ----
  while (true) {
    std::string line = readln();
    if (line == "FINISH")
      break;
    auto t = toks(line); // "START ... <turn>"
    int turn = std::stoi(t.at(2));

    std::cout << "COMMAND\n";
    auto it = gCmds[mySide].find(turn);
    if (it != gCmds[mySide].end())
      for (const auto &c : it->second)
        std::cout << c << "\n";
    std::cout << "END" << std::endl;

    consumeResult();
  }
  return 0;
}
