#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,popcnt")
#include <bits/stdc++.h>
using namespace std;

typedef pair<int, int> P;
typedef chrono::steady_clock SC;
static SC::time_point T0;
inline double ms() { return chrono::duration<double, milli>(SC::now() - T0).count(); }

// Evolvable parameters
struct Params {
    // Survival hard limits
    double DEAD = -1e9;
    double FALL_OUT = -8e8;

    // Space control
    double SPACE_CELL = 15.0;
    double TRAP_TINY = -70000.0;
    double TRAP_SMALL = -20000.0;
    double TRAP_MED = -4000.0;
    double FALL_ROW = -320.0;
    double GROUNDED = 300.0;
    double LAND_FOOD = 6000.0;

    // Opponent pressure
    double KILL_BONUS = 28000.0;
    double DEATH_PEN = -280000.0;
    double CLASH_PEN = -3500.0;
    double UNDER_ENEMY = -50000.0;
    double OPP_CROWD = -1200.0;
    double SIZE_ADV = 180.0;

    // Food/objective
    double EAT_BONUS = 90000.0;
    double FOOD_SCALE = 7500.0;
    double CHAIN_FOOD = 3000.0;
    double FOOD_RACE = 2000.0;

    // Tie-break
    double MOBILITY = 55.0;
    double CENTER_PEN = -3.0;
    double TOP_PEN = -550.0;
    double EDGE_PEN = -110.0;
    double LADDER = 800.0;
    double ALLY_PEN = -110000.0;

    // Fast scorer
    double FAST_SOLID_SUP = 200.0;
    double FAST_BODY_SUP = 800.0;
    double FAST_FALL_PEN = -350.0;
    double FAST_BOT_PEN = -5000.0;
    double FAST_THREAT_PEN = -1600.0;
    double FAST_CROWD_PEN = -400.0;
    double FAST_FOOD_BONUS = 5000.0;
    double FAST_FOOD_PULL = 500.0;
    double FAST_ALLY_PEN = -8000.0;
    double FAST_TOP_PEN = -400.0;
    double FAST_EDGE_PEN = -80.0;
};

static Params P_;

static constexpr int MW = 47, MH = 32;
int W, H;
bool WALL[MW][MH];
mt19937 RNG(chrono::steady_clock::now().time_since_epoch().count());

constexpr int DX[4] = {0, 0, -1, 1};
constexpr int DY[4] = {-1, 1, 0, 0};
const char* DN[4] = {"UP", "DOWN", "LEFT", "RIGHT"};
constexpr int OPP[4] = {1, 0, 3, 2};

inline bool inB(int x, int y) { return (unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H; }

bool bodyOcc[MW][MH];
bool eBodyOcc[MW][MH];
bool fullOcc[MW][MH];
bool fGrid[MW][MH];
bool eThreat[MW][MH];
int bfsDist[MW][MH];

struct Snake {
    int id, team, dir;
    deque<P> body;
    bool alive;
    P head() const { return body.empty() ? P{-1, -1} : body.front(); }
    int sz() const { return (int)body.size(); }
};

struct State {
    vector<Snake> snakes;
    vector<P> foods;

    void buildFG() {
        memset(fGrid, 0, sizeof(fGrid));
        for (auto& f : foods) {
            if (inB(f.first, f.second)) {
                fGrid[f.first][f.second] = true;
            }
        }
    }

    int teamScore(int t) const {
        int s = 0;
        for (auto& sn : snakes) {
            if (sn.alive && sn.team == t) {
                s += sn.sz();
            }
        }
        return s;
    }

    bool over() const {
        bool m = false, o = false;
        for (auto& sn : snakes) {
            if (!sn.alive) {
                continue;
            }
            if (sn.team == 0) {
                m = true;
            } else {
                o = true;
            }
        }
        return !m || !o || foods.empty();
    }
};

static bool loadParamsJson(const string& filename) {
    ifstream f(filename);
    if (!f.is_open()) {
        return false;
    }

    unordered_map<string, double*> k = {
        {"DEAD", &P_.DEAD}, {"FALL_OUT", &P_.FALL_OUT},
        {"SPACE_CELL", &P_.SPACE_CELL}, {"TRAP_TINY", &P_.TRAP_TINY},
        {"TRAP_SMALL", &P_.TRAP_SMALL}, {"TRAP_MED", &P_.TRAP_MED},
        {"FALL_ROW", &P_.FALL_ROW}, {"GROUNDED", &P_.GROUNDED}, {"LAND_FOOD", &P_.LAND_FOOD},
        {"KILL_BONUS", &P_.KILL_BONUS}, {"DEATH_PEN", &P_.DEATH_PEN}, {"CLASH_PEN", &P_.CLASH_PEN},
        {"UNDER_ENEMY", &P_.UNDER_ENEMY}, {"OPP_CROWD", &P_.OPP_CROWD}, {"SIZE_ADV", &P_.SIZE_ADV},
        {"EAT_BONUS", &P_.EAT_BONUS}, {"FOOD_SCALE", &P_.FOOD_SCALE}, {"CHAIN_FOOD", &P_.CHAIN_FOOD}, {"FOOD_RACE", &P_.FOOD_RACE},
        {"MOBILITY", &P_.MOBILITY}, {"CENTER_PEN", &P_.CENTER_PEN}, {"TOP_PEN", &P_.TOP_PEN},
        {"EDGE_PEN", &P_.EDGE_PEN}, {"LADDER", &P_.LADDER}, {"ALLY_PEN", &P_.ALLY_PEN},
        {"FAST_SOLID_SUP", &P_.FAST_SOLID_SUP}, {"FAST_BODY_SUP", &P_.FAST_BODY_SUP},
        {"FAST_FALL_PEN", &P_.FAST_FALL_PEN}, {"FAST_BOT_PEN", &P_.FAST_BOT_PEN},
        {"FAST_THREAT_PEN", &P_.FAST_THREAT_PEN}, {"FAST_CROWD_PEN", &P_.FAST_CROWD_PEN},
        {"FAST_FOOD_BONUS", &P_.FAST_FOOD_BONUS}, {"FAST_FOOD_PULL", &P_.FAST_FOOD_PULL},
        {"FAST_ALLY_PEN", &P_.FAST_ALLY_PEN}, {"FAST_TOP_PEN", &P_.FAST_TOP_PEN}, {"FAST_EDGE_PEN", &P_.FAST_EDGE_PEN},
    };

    string line;
    while (getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == string::npos) {
            continue;
        }
        string key = line.substr(0, colon);
        string val = line.substr(colon + 1);

        auto strip = [](string& s) {
            string out;
            out.reserve(s.size());
            for (char c : s) {
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '"' && c != ',' && c != '{' && c != '}') {
                    out.push_back(c);
                }
            }
            s.swap(out);
        };
        strip(key);
        strip(val);
        if (key.empty() || val.empty()) {
            continue;
        }
        auto it = k.find(key);
        if (it == k.end()) {
            continue;
        }
        try {
            *(it->second) = stod(val);
        } catch (...) {
        }
    }

    return true;
}

static bool loadParamsTxt(const string& filename) {
    ifstream pf(filename);
    if (!pf.is_open()) {
        return false;
    }

    // Keep this layout contiguous and plain doubles for flat-array loading.
    double* fields = reinterpret_cast<double*>(&P_);
    int nFields = (int)(sizeof(Params) / sizeof(double));
    for (int i = 0; i < nFields; i++) {
        double v;
        if (pf >> v) {
            fields[i] = v;
        }
    }
    return true;
}

void buildOcc(const State& gs) {
    memset(bodyOcc, 0, sizeof(bodyOcc));
    memset(eBodyOcc, 0, sizeof(eBodyOcc));
    memset(fullOcc, 0, sizeof(fullOcc));
    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) {
            if (WALL[x][y]) {
                fullOcc[x][y] = true;
            }
        }
    }
    for (auto& sn : gs.snakes) {
        if (!sn.alive) {
            continue;
        }
        for (int k = 0; k < sn.sz() - 1; k++) {
            auto [bx, by] = sn.body[k];
            if (!inB(bx, by)) {
                continue;
            }
            bodyOcc[bx][by] = true;
            fullOcc[bx][by] = true;
            if (sn.team == 1) {
                eBodyOcc[bx][by] = true;
            }
        }
    }
}

void buildBFS(const State& gs) {
    memset(bfsDist, 0x3f, sizeof(bfsDist));
    static P q[MW * MH];
    int qh = 0, qt = 0;
    for (auto& f : gs.foods) {
        if (inB(f.first, f.second)) {
            bfsDist[f.first][f.second] = 0;
            q[qt++] = {f.first, f.second};
        }
    }
    while (qh < qt) {
        auto [cx, cy] = q[qh++];
        int nd = bfsDist[cx][cy] + 1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!inB(nx, ny) || fullOcc[nx][ny] || bfsDist[nx][ny] <= nd) {
                continue;
            }
            bfsDist[nx][ny] = nd;
            q[qt++] = {nx, ny};
        }
    }
}

void buildEThreat(const State& gs) {
    memset(eThreat, 0, sizeof(eThreat));
    for (auto& sn : gs.snakes) {
        if (!sn.alive || sn.team == 0) {
            continue;
        }
        auto [hx, hy] = sn.head();
        for (int d = 0; d < 4; d++) {
            if (d == OPP[sn.dir] && sn.sz() > 1) {
                continue;
            }
            int nx = hx + DX[d], ny = hy + DY[d];
            if (inB(nx, ny)) {
                eThreat[nx][ny] = true;
            }
        }
    }
}

void applyGravity(State& gs) {
    static bool solid[MW][MH];
    int n = (int)gs.snakes.size();
    for (int iter = 0; iter < H + 4; iter++) {
        memset(solid, 0, sizeof(solid));
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                if (WALL[x][y]) {
                    solid[x][y] = true;
                }
            }
        }
        for (auto& f : gs.foods) {
            if (inB(f.first, f.second)) {
                solid[f.first][f.second] = true;
            }
        }

        vector<bool> sup(n, false);
        for (int i = 0; i < n; i++) {
            if (!gs.snakes[i].alive) {
                sup[i] = true;
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < n; i++) {
                if (!sup[i]) {
                    continue;
                }
                for (auto& [bx, by] : gs.snakes[i].body) {
                    if (inB(bx, by)) {
                        solid[bx][by] = true;
                    }
                }
            }
            for (int i = 0; i < n; i++) {
                if (sup[i]) {
                    continue;
                }
                for (auto& [bx, by] : gs.snakes[i].body) {
                    if (!inB(bx, by)) {
                        continue;
                    }
                    int bely = by + 1;
                    if (bely >= H) {
                        continue;
                    }
                    if (solid[bx][bely]) {
                        sup[i] = true;
                        changed = true;
                        break;
                    }
                }
            }
        }

        bool fell = false;
        for (int i = 0; i < n; i++) {
            if (!gs.snakes[i].alive || sup[i]) {
                continue;
            }
            fell = true;
            for (auto& [bx, by] : gs.snakes[i].body) {
                by++;
            }
            bool allOut = true;
            for (auto& [bx, by] : gs.snakes[i].body) {
                if (by < H) {
                    allOut = false;
                    break;
                }
            }
            if (allOut) {
                gs.snakes[i].alive = false;
                gs.snakes[i].body.clear();
            }
        }
        if (!fell) {
            break;
        }
    }
}

int estimateFall(const deque<P>& nb) {
    if (nb.empty()) {
        return 0;
    }
    static bool ownOrig[MW][MH];
    memset(ownOrig, 0, sizeof(ownOrig));
    for (auto& [bx, by] : nb) {
        if (inB(bx, by)) {
            ownOrig[bx][by] = true;
        }
    }
    for (int f = 0; f <= H; f++) {
        bool anyIn = false, sup = false;
        for (auto& [bx, origY] : nb) {
            int ny = origY + f;
            if (ny < H) {
                anyIn = true;
            }
            int bely = ny + 1;
            if (bely >= H) {
                continue;
            }
            bool isSolid = WALL[bx][bely] || fGrid[bx][bely] || bodyOcc[bx][bely];
            if (!isSolid) {
                continue;
            }
            int origBely = bely - f;
            bool isOwn = (origBely >= 0 && origBely < H && ownOrig[bx][origBely]);
            if (!isOwn) {
                sup = true;
                break;
            }
        }
        if (!anyIn) {
            return H;
        }
        if (sup) {
            return f;
        }
    }
    return H;
}

int gravityFloodFill(int sx, int sy, int limit = 128) {
    while (sy + 1 < H && !WALL[sx][sy + 1] && !bodyOcc[sx][sy + 1] && !fGrid[sx][sy + 1]) {
        sy++;
    }
    if (!inB(sx, sy) || (fullOcc[sx][sy] && !fGrid[sx][sy])) {
        return 0;
    }

    static bool gvis[MW][MH];
    memset(gvis, 0, sizeof(gvis));
    static P gbq[MW * MH];
    int gqh = 0, gqt = 0;

    auto tryAdd = [&](int x, int y) {
        while (y + 1 < H && !WALL[x][y + 1] && !bodyOcc[x][y + 1] && !fGrid[x][y + 1]) {
            y++;
        }
        if (!inB(x, y) || gvis[x][y] || WALL[x][y] || (fullOcc[x][y] && !fGrid[x][y])) {
            return;
        }
        gvis[x][y] = true;
        gbq[gqt++] = {x, y};
    };

    tryAdd(sx, sy);
    int cnt = 0;
    while (gqh < gqt && cnt < limit) {
        auto [cx, cy] = gbq[gqh++];
        cnt++;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!inB(nx, ny) || WALL[nx][ny]) {
                continue;
            }
            if (fullOcc[nx][ny] && !fGrid[nx][ny]) {
                continue;
            }
            tryAdd(nx, ny);
        }
    }
    return cnt;
}

int floodFill(int sx, int sy, int limit = 128) {
    if (!inB(sx, sy) || (fullOcc[sx][sy] && !fGrid[sx][sy])) {
        return 0;
    }
    static bool vis[MW][MH];
    memset(vis, 0, sizeof(vis));
    static P bq[MW * MH];
    int qh = 0, qt = 0;
    bq[qt++] = {sx, sy};
    vis[sx][sy] = true;
    int cnt = 0;
    while (qh < qt && cnt < limit) {
        auto [cx, cy] = bq[qh++];
        cnt++;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!inB(nx, ny) || vis[nx][ny] || WALL[nx][ny]) {
                continue;
            }
            if (fullOcc[nx][ny] && !fGrid[nx][ny]) {
                continue;
            }
            vis[nx][ny] = true;
            bq[qt++] = {nx, ny};
        }
    }
    return cnt;
}

inline bool isSurvivable(int d, const Snake& sn) {
    if (d == OPP[sn.dir] && sn.sz() > 1) {
        return false;
    }
    auto [hx, hy] = sn.head();
    int nx = hx + DX[d], ny = hy + DY[d];
    if (!inB(nx, ny) || WALL[nx][ny]) {
        return false;
    }
    if (fullOcc[nx][ny] && !fGrid[nx][ny]) {
        return false;
    }
    for (int k = 0; k < sn.sz() - 1; k++) {
        if (sn.body[k].first == nx && sn.body[k].second == ny) {
            return false;
        }
    }
    return true;
}

double scoreDir(int d, const Snake& sn, const State& gs, const set<P>& allyPlan, bool useGravity = true) {
    if (!isSurvivable(d, sn)) {
        return P_.DEAD;
    }

    auto [hx, hy] = sn.head();
    int nx = hx + DX[d], ny = hy + DY[d];
    bool willEat = fGrid[nx][ny];
    double score = 0;

    if (allyPlan.count({nx, ny})) {
        score += P_.ALLY_PEN;
    }

    int landX = nx, landY = ny;
    if (useGravity) {
        deque<P> newBody;
        newBody.push_front({nx, ny});
        int keep = willEat ? sn.sz() : sn.sz() - 1;
        for (int k = 0; k < keep - 1 && k < sn.sz(); k++) {
            newBody.push_back(sn.body[k]);
        }
        int fd = estimateFall(newBody);
        if (fd >= H) {
            return P_.FALL_OUT;
        }
        landY = ny + fd;
        if (!inB(landX, landY)) {
            return P_.FALL_OUT;
        }
        if (fd > 0) {
            if (!willEat) {
                score += fd * P_.FALL_ROW;
            }
            if (fGrid[landX][landY]) {
                score += P_.LAND_FOOD;
            }
        } else {
            score += P_.GROUNDED;
        }
    }

    int space = gravityFloodFill(landX, landY);
    score += space * P_.SPACE_CELL;
    if (space < 4) {
        score += P_.TRAP_TINY;
    } else if (space < 8) {
        score += P_.TRAP_SMALL;
    } else if (space < 18) {
        score += P_.TRAP_MED;
    }

    int mobility = 0;
    for (int dd = 0; dd < 4; dd++) {
        int fx = landX + DX[dd], fy = landY + DY[dd];
        if (inB(fx, fy) && !fullOcc[fx][fy]) {
            mobility++;
        }
    }

    if (bfsDist[landX][landY] < 0x3f3f3f3f) {
        score += P_.FOOD_SCALE / (1.0 + bfsDist[landX][landY]);
    }

    if (!gs.foods.empty()) {
        int d1 = 9999, d2 = 9999;
        for (auto& f : gs.foods) {
            int dd = abs(landX - f.first) + abs(landY - f.second);
            if (dd < d1) {
                d2 = d1;
                d1 = dd;
            } else if (dd < d2) {
                d2 = dd;
            }
        }
        if (d2 < 9999) {
            score += P_.CHAIN_FOOD / (1.0 + d2);
        }

        for (auto& f : gs.foods) {
            int myDist = abs(landX - f.first) + abs(landY - f.second);
            bool iWin = true;
            for (auto& esn : gs.snakes) {
                if (!esn.alive || esn.team == 0) {
                    continue;
                }
                auto [ex, ey] = esn.head();
                if (abs(ex - f.first) + abs(ey - f.second) <= myDist) {
                    iWin = false;
                    break;
                }
            }
            if (iWin) {
                score += P_.FOOD_RACE / (1.0 + myDist);
            }
        }
    }

    if (eThreat[nx][ny]) {
        for (auto& esn : gs.snakes) {
            if (!esn.alive || esn.team == 0) {
                continue;
            }
            auto [ex, ey] = esn.head();
            if (abs(ex - nx) + abs(ey - ny) > 1) {
                continue;
            }
            if (sn.sz() > esn.sz() + 1) {
                score += P_.KILL_BONUS;
            } else if (esn.sz() > sn.sz() + 1) {
                score += P_.DEATH_PEN;
            } else {
                score += P_.CLASH_PEN;
            }
        }
    } else {
        bool nearEnemy = false;
        for (int dd = 0; dd < 4 && !nearEnemy; dd++) {
            int fx = nx + DX[dd], fy = ny + DY[dd];
            if (inB(fx, fy) && eThreat[fx][fy]) {
                nearEnemy = true;
            }
        }
        if (nearEnemy) {
            score += P_.OPP_CROWD;
        }
    }

    for (int cy = ny - 1; cy >= max(0, ny - 4); cy--) {
        if (eBodyOcc[nx][cy]) {
            score += P_.UNDER_ENEMY;
            break;
        }
    }

    for (auto& esn : gs.snakes) {
        if (!esn.alive || esn.team == 0) {
            continue;
        }
        auto [ex, ey] = esn.head();
        if (abs(ex - nx) + abs(ey - ny) <= 8) {
            score += (sn.sz() - esn.sz()) * P_.SIZE_ADV;
            break;
        }
    }

    if (willEat) {
        score += P_.EAT_BONUS;
    }

    score += mobility * P_.MOBILITY;
    if (ny <= 0) {
        score += P_.TOP_PEN;
    }
    if (nx <= 0 || nx >= W - 1) {
        score += P_.EDGE_PEN;
    }
    score += (abs(landX - W / 2) + abs(landY - H / 2)) * P_.CENTER_PEN;
    if (ny + 1 < H && bodyOcc[nx][ny + 1]) {
        score += P_.LADDER;
    }

    return score;
}

int fastScoreDir(int d, const Snake& sn, const vector<P>& foods, const set<P>& allyPlan) {
    if (d == OPP[sn.dir] && sn.sz() > 1) {
        return -100000;
    }
    auto [hx, hy] = sn.head();
    int nx = hx + DX[d], ny = hy + DY[d];
    if (!inB(nx, ny) || WALL[nx][ny]) {
        return -100000;
    }
    if (fullOcc[nx][ny] && !fGrid[nx][ny]) {
        return -100000;
    }
    for (int k = 0; k < sn.sz() - 1; k++) {
        if (sn.body[k].first == nx && sn.body[k].second == ny) {
            return -100000;
        }
    }

    int score = 0;
    if (ny + 1 < H) {
        if (WALL[nx][ny + 1] || fGrid[nx][ny + 1]) {
            score += (int)P_.FAST_SOLID_SUP;
        } else if (bodyOcc[nx][ny + 1]) {
            score += (int)P_.FAST_BODY_SUP;
        } else {
            score += (int)P_.FAST_FALL_PEN;
        }
    } else {
        score += (int)P_.FAST_BOT_PEN;
    }

    if (eThreat[nx][ny]) {
        score += (int)P_.FAST_THREAT_PEN;
    } else {
        for (int dd = 0; dd < 4; dd++) {
            int fx = nx + DX[dd], fy = ny + DY[dd];
            if (inB(fx, fy) && eThreat[fx][fy]) {
                score += (int)P_.FAST_CROWD_PEN;
                break;
            }
        }
    }

    if (fGrid[nx][ny]) {
        score += (int)P_.FAST_FOOD_BONUS;
    } else {
        int best = 9999;
        for (auto& f : foods) {
            best = min(best, abs(nx - f.first) + abs(ny - f.second));
        }
        score += (int)(P_.FAST_FOOD_PULL / (1 + best));
    }

    if (allyPlan.count({nx, ny})) {
        score += (int)P_.FAST_ALLY_PEN;
    }
    if (ny <= 0) {
        score += (int)P_.FAST_TOP_PEN;
    }
    if (nx <= 0 || nx >= W - 1) {
        score += (int)P_.FAST_EDGE_PEN;
    }
    return score;
}

struct DirScore {
    int dir;
    double sc;
    bool operator<(const DirScore& o) const { return sc > o.sc; }
};

vector<pair<int, int>> coordGreedy(const State& gs, const vector<int>& myIdx, bool fast = false) {
    vector<pair<int, int>> order;
    for (int idx : myIdx) {
        if (!gs.snakes[idx].alive) {
            continue;
        }
        auto [hx, hy] = gs.snakes[idx].head();
        int best = 9999;
        for (auto& f : gs.foods) {
            best = min(best, abs(hx - f.first) + abs(hy - f.second));
        }
        order.push_back({best, idx});
    }
    sort(order.begin(), order.end());

    set<P> allyPlan;
    vector<pair<int, int>> decisions;

    for (auto& [_, idx] : order) {
        auto& sn = gs.snakes[idx];
        if (!sn.alive) {
            continue;
        }
        vector<DirScore> ds;

        for (int d = 0; d < 4; d++) {
            if (d == OPP[sn.dir] && sn.sz() > 1) {
                continue;
            }
            if (!isSurvivable(d, sn)) {
                continue;
            }
            double sc = fast ? (double)fastScoreDir(d, sn, gs.foods, allyPlan)
                             : scoreDir(d, sn, gs, allyPlan, true);
            ds.push_back({d, sc});
        }

        if (ds.empty()) {
            for (int d = 0; d < 4; d++) {
                if (d == OPP[sn.dir] && sn.sz() > 1) {
                    continue;
                }
                double sc = fast ? (double)fastScoreDir(d, sn, gs.foods, allyPlan)
                                 : scoreDir(d, sn, gs, allyPlan, false);
                ds.push_back({d, sc});
            }
        }
        if (ds.empty()) {
            ds.push_back({sn.dir, -1e8});
        }

        sort(ds.begin(), ds.end());
        int bd = ds[0].dir;
        auto [hx, hy] = sn.head();
        allyPlan.insert({hx + DX[bd], hy + DY[bd]});
        decisions.push_back({idx, bd});
    }
    return decisions;
}

State fastStep(State gs, const vector<int>& dirs) {
    int n = (int)gs.snakes.size();
    gs.buildFG();

    vector<bool> eats(n, false);
    for (int i = 0; i < n; i++) {
        if (!gs.snakes[i].alive) {
            continue;
        }
        auto [hx, hy] = gs.snakes[i].head();
        int nx = hx + DX[dirs[i]], ny = hy + DY[dirs[i]];
        if (inB(nx, ny) && fGrid[nx][ny]) {
            eats[i] = true;
        }
    }
    for (int i = 0; i < n; i++) {
        if (!gs.snakes[i].alive) {
            continue;
        }
        auto& sn = gs.snakes[i];
        auto [hx, hy] = sn.head();
        P nh = {hx + DX[dirs[i]], hy + DY[dirs[i]]};
        sn.dir = dirs[i];
        if (!eats[i]) {
            sn.body.pop_back();
        }
        sn.body.push_front(nh);
    }

    {
        set<P> eaten;
        for (int i = 0; i < n; i++) {
            if (!gs.snakes[i].alive) {
                continue;
            }
            auto [hx, hy] = gs.snakes[i].head();
            if (inB(hx, hy) && fGrid[hx][hy]) {
                eaten.insert({hx, hy});
            }
        }
        if (!eaten.empty()) {
            vector<P> nf;
            nf.reserve(gs.foods.size());
            for (auto& f : gs.foods) {
                if (!eaten.count(f)) {
                    nf.push_back(f);
                }
            }
            gs.foods = move(nf);
            gs.buildFG();
        }
    }

    static bool bGrid[MW][MH];
    memset(bGrid, 0, sizeof(bGrid));
    for (int i = 0; i < n; i++) {
        if (!gs.snakes[i].alive) {
            continue;
        }
        for (int k = 1; k < gs.snakes[i].sz(); k++) {
            auto [bx, by] = gs.snakes[i].body[k];
            if (inB(bx, by)) {
                bGrid[bx][by] = true;
            }
        }
    }

    static P nh[16];
    for (int i = 0; i < n; i++) {
        nh[i] = gs.snakes[i].alive ? gs.snakes[i].head() : P{-1, -1};
    }

    for (int i = 0; i < n; i++) {
        if (!gs.snakes[i].alive) {
            continue;
        }
        auto [hx, hy] = nh[i];
        bool die = false;
        if (!inB(hx, hy) || WALL[hx][hy]) {
            die = true;
        } else if (bGrid[hx][hy]) {
            die = true;
        } else {
            for (int j = 0; j < n && !die; j++) {
                if (j == i || !gs.snakes[j].alive) {
                    continue;
                }
                if (nh[j] == P{hx, hy} && gs.snakes[j].sz() >= gs.snakes[i].sz()) {
                    die = true;
                }
            }
        }
        if (die) {
            if (gs.snakes[i].sz() <= 3) {
                gs.snakes[i].alive = false;
                gs.snakes[i].body.clear();
            } else {
                gs.snakes[i].body.pop_front();
            }
        }
    }

    applyGravity(gs);
    return gs;
}

double rollout(State gs, int maxSteps, const vector<int>& myIdx, const vector<int>& oppIdx) {
    for (int step = 0; step < maxSteps && !gs.over(); step++) {
        buildOcc(gs);
        gs.buildFG();
        buildEThreat(gs);

        int ns = (int)gs.snakes.size();
        vector<int> dirs(ns, 0);

        auto dec = coordGreedy(gs, myIdx, true);
        for (auto& [idx, dir] : dec) {
            if ((RNG() & 7) == 0) {
                vector<int> valid;
                for (int dd = 0; dd < 4; dd++) {
                    if (dd == OPP[gs.snakes[idx].dir] && gs.snakes[idx].sz() > 1) {
                        continue;
                    }
                    if (!isSurvivable(dd, gs.snakes[idx])) {
                        continue;
                    }
                    valid.push_back(dd);
                }
                if (!valid.empty()) {
                    dir = valid[RNG() % valid.size()];
                }
            }
            dirs[idx] = dir;
        }

        auto oppDec = coordGreedy(gs, oppIdx, true);
        for (auto& [idx, dir] : oppDec) {
            dirs[idx] = dir;
        }

        gs = fastStep(gs, dirs);
    }

    double my = gs.teamScore(0), op = gs.teamScore(1);
    if (op == 0 && my > 0) {
        return 100000.0;
    }
    if (my == 0 && op > 0) {
        return -100000.0;
    }

    double finalScore = (my - op) * 1000.0;
    for (int idx : myIdx) {
        if (!gs.snakes[idx].alive || gs.foods.empty()) {
            continue;
        }
        auto [hx, hy] = gs.snakes[idx].head();
        int bestDist = 9999;
        for (auto& f : gs.foods) {
            bestDist = min(bestDist, abs(hx - f.first) + abs(hy - f.second));
        }
        finalScore -= bestDist * 2.0;
    }
    return finalScore;
}

struct JointAct {
    vector<int> dirs;
    double total = 0;
    int cnt = 0;
    double avg() const { return cnt ? total / cnt : 0.0; }
};

void jointMCTS(const State& gs, const vector<int>& myAlive, const vector<int>& oppAlive, double budgetMs, map<int, int>& bestDirs) {
    int nMy = (int)myAlive.size();
    if (!nMy) {
        return;
    }

    buildOcc(gs);
    const_cast<State&>(gs).buildFG();
    buildBFS(gs);
    buildEThreat(gs);

    vector<vector<int>> vDirs(nMy);
    for (int i = 0; i < nMy; i++) {
        auto& sn = gs.snakes[myAlive[i]];
        auto [hx, hy] = sn.head();
        for (int d = 0; d < 4; d++) {
            if (!isSurvivable(d, sn)) {
                continue;
            }
            int nx = hx + DX[d], ny = hy + DY[d];
            if (floodFill(nx, ny, 2) < 1) {
                continue;
            }
            vDirs[i].push_back(d);
        }
        if (vDirs[i].empty()) {
            for (int d = 0; d < 4; d++) {
                if (d == OPP[sn.dir] && sn.sz() > 1) {
                    continue;
                }
                int nx = hx + DX[d], ny = hy + DY[d];
                if (inB(nx, ny) && !WALL[nx][ny]) {
                    vDirs[i].push_back(d);
                }
            }
            if (vDirs[i].empty()) {
                vDirs[i].push_back(sn.dir);
            }
        }
    }

    auto greedyDec = coordGreedy(gs, myAlive, false);
    map<int, int> gMap;
    for (auto& [idx, dir] : greedyDec) {
        gMap[idx] = dir;
    }

    long long combos = 1;
    for (int i = 0; i < nMy; i++) {
        combos *= (int)vDirs[i].size();
    }

    vector<JointAct> cands;
    if (combos <= 64) {
        vector<int> combo(nMy, 0);
        function<void(int)> gen = [&](int pos) {
            if (pos == nMy) {
                JointAct ja;
                ja.dirs = combo;
                cands.push_back(ja);
                return;
            }
            for (int d : vDirs[pos]) {
                combo[pos] = d;
                gen(pos + 1);
            }
        };
        gen(0);
    } else {
        JointAct base;
        for (int i = 0; i < nMy; i++) {
            base.dirs.push_back(gMap.count(myAlive[i]) ? gMap[myAlive[i]] : vDirs[i][0]);
        }
        cands.push_back(base);
        for (int i = 0; i < nMy; i++) {
            for (int d : vDirs[i]) {
                JointAct ja = base;
                ja.dirs[i] = d;
                cands.push_back(ja);
            }
        }
        for (int i = 0; i < nMy && (int)cands.size() < 96; i++) {
            for (int j = i + 1; j < nMy && (int)cands.size() < 96; j++) {
                for (int di : vDirs[i]) {
                    for (int dj : vDirs[j]) {
                        if ((int)cands.size() >= 96) {
                            goto doneTwoSnake;
                        }
                        JointAct ja = base;
                        ja.dirs[i] = di;
                        ja.dirs[j] = dj;
                        cands.push_back(ja);
                    }
                }
            }
        }
    doneTwoSnake:
        while ((int)cands.size() < 80) {
            JointAct ja;
            for (int i = 0; i < nMy; i++) {
                ja.dirs.push_back(vDirs[i][RNG() % vDirs[i].size()]);
            }
            cands.push_back(ja);
        }
    }

    sort(cands.begin(), cands.end(), [](auto& a, auto& b) { return a.dirs < b.dirs; });
    cands.erase(unique(cands.begin(), cands.end(), [](auto& a, auto& b) { return a.dirs == b.dirs; }), cands.end());
    int nc = (int)cands.size();

    vector<bool> aCol(nc, false);
    vector<double> hBias(nc, 0.0);
    for (int c = 0; c < nc; c++) {
        set<P> heads, usedPlan;
        double bias = 0;
        bool col = false;
        for (int i = 0; i < nMy; i++) {
            auto& sn = gs.snakes[myAlive[i]];
            auto [hx, hy] = sn.head();
            P nhd = {hx + DX[cands[c].dirs[i]], hy + DY[cands[c].dirs[i]]};
            if (heads.count(nhd)) {
                col = true;
                break;
            }
            heads.insert(nhd);
            bias += scoreDir(cands[c].dirs[i], sn, gs, usedPlan, true);
            usedPlan.insert(nhd);
        }
        aCol[c] = col;
        if (col) {
            bias -= 40000;
        }
        hBias[c] = bias;
    }

    vector<int> candOrder(nc);
    iota(candOrder.begin(), candOrder.end(), 0);
    sort(candOrder.begin(), candOrder.end(), [&](int a, int b) { return hBias[a] > hBias[b]; });

    int ns = (int)gs.snakes.size();
    vector<int> oppDirs(ns, 0);
    {
        auto oppDec = coordGreedy(gs, oppAlive, false);
        for (auto& [idx, dir] : oppDec) {
            oppDirs[idx] = dir;
        }
    }

    double tStart = ms(), deadline = tStart + budgetMs;
    int tot = 0;

    auto evalCand = [&](int c, int depth) -> double {
        vector<int> dirs(ns, 0);
        for (int i = 0; i < nMy; i++) {
            dirs[myAlive[i]] = cands[c].dirs[i];
        }
        for (int idx : oppAlive) {
            dirs[idx] = oppDirs[idx];
        }
        State after = fastStep(gs, dirs);
        after.buildFG();
        double s = rollout(after, depth, myAlive, oppAlive);
        if (aCol[c]) {
            s -= 32000;
        }
        return s;
    };

    for (int ci = 0; ci < nc && ms() < deadline - 2.0; ci++) {
        int c = candOrder[ci];
        cands[c].total += evalCand(c, 6);
        cands[c].cnt++;
        tot++;
    }

    double elapsed = ms() - tStart;
    int ucbDepth = (tot > 0 && elapsed / tot < 1.5) ? 8 : 5;

    while (ms() < deadline - 0.5) {
        double logTot = log((double)(tot + 1));
        int bestC = -1;
        double bestU = -1e18;
        for (int c = 0; c < nc; c++) {
            if (!cands[c].cnt) {
                bestC = c;
                break;
            }
            double u = cands[c].avg() + 1.414 * sqrt(logTot / cands[c].cnt);
            if (aCol[c]) {
                u -= 18000;
            }
            if (u > bestU) {
                bestU = u;
                bestC = c;
            }
        }
        if (bestC < 0) {
            break;
        }
        cands[bestC].total += evalCand(bestC, ucbDepth);
        cands[bestC].cnt++;
        tot++;
    }

    int bestC = 0;
    double bestAvg = -1e18;
    for (int c = 0; c < nc; c++) {
        if (!cands[c].cnt) {
            continue;
        }
        double avg = cands[c].avg() - (aCol[c] ? 32000 : 0);
        if (avg > bestAvg) {
            bestAvg = avg;
            bestC = c;
        }
    }
    for (int i = 0; i < nMy; i++) {
        bestDirs[myAlive[i]] = cands[bestC].dirs[i];
    }

    cerr << "[MCTS] sims=" << tot << " cands=" << nc << " ucbD=" << ucbDepth << " best=" << fixed << setprecision(1)
         << bestAvg << " t=" << ms() << "ms\n";
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // First priority for local/offline tuning: --config <json>
    string configPath;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[i + 1];
            break;
        }
    }
    if (!configPath.empty()) {
        if (loadParamsJson(configPath)) {
            cerr << "[PARAMS] loaded JSON config from " << configPath << "\n";
        }
    } else {
        // Fallback for standalone experimentation.
        if (loadParamsTxt("params.txt")) {
            int nFields = (int)(sizeof(Params) / sizeof(double));
            cerr << "[PARAMS] loaded " << nFields << " weights from params.txt\n";
        }
    }

    int MY_ID;
    cin >> MY_ID;
    cin.ignore();
    cin >> W;
    cin.ignore();
    cin >> H;
    cin.ignore();

    for (int y = 0; y < H; y++) {
        string row;
        getline(cin, row);
        for (int x = 0; x < W && x < (int)row.size(); x++) {
            WALL[x][y] = (row[x] == '#');
        }
    }

    int spp;
    cin >> spp;
    cin.ignore();
    vector<int> myIds, oppIds;
    for (int i = 0; i < spp; i++) {
        int id;
        cin >> id;
        cin.ignore();
        myIds.push_back(id);
    }
    for (int i = 0; i < spp; i++) {
        int id;
        cin >> id;
        cin.ignore();
        oppIds.push_back(id);
    }
    set<int> mySet(myIds.begin(), myIds.end());

    int turn = 0;
    while (true) {
        T0 = SC::now();
        State gs;

        int fc;
        cin >> fc;
        cin.ignore();
        for (int i = 0; i < fc; i++) {
            int x, y;
            cin >> x >> y;
            cin.ignore();
            gs.foods.push_back({x, y});
        }
        gs.buildFG();

        int sc;
        cin >> sc;
        cin.ignore();
        map<int, int> id2idx;
        for (int i = 0; i < sc; i++) {
            int sid;
            string bstr;
            cin >> sid >> bstr;
            cin.ignore();
            Snake sn;
            sn.id = sid;
            sn.team = mySet.count(sid) ? 0 : 1;
            sn.alive = true;
            sn.dir = 0;
            stringstream ss(bstr);
            string part;
            while (getline(ss, part, ':')) {
                int cx, cy;
                sscanf(part.c_str(), "%d,%d", &cx, &cy);
                sn.body.push_back({cx, cy});
            }
            if ((int)sn.body.size() >= 2) {
                int ddx = sn.body[0].first - sn.body[1].first;
                int ddy = sn.body[0].second - sn.body[1].second;
                for (int d = 0; d < 4; d++) {
                    if (DX[d] == ddx && DY[d] == ddy) {
                        sn.dir = d;
                        break;
                    }
                }
            }
            id2idx[sid] = (int)gs.snakes.size();
            gs.snakes.push_back(sn);
        }

        vector<int> myAlive, oppAlive;
        for (int sid : myIds) {
            if (id2idx.count(sid) && gs.snakes[id2idx[sid]].alive) {
                myAlive.push_back(id2idx[sid]);
            }
        }
        for (int sid : oppIds) {
            if (id2idx.count(sid) && gs.snakes[id2idx[sid]].alive) {
                oppAlive.push_back(id2idx[sid]);
            }
        }

        buildOcc(gs);
        buildBFS(gs);
        buildEThreat(gs);

        double budget = (turn == 0) ? 850.0 : 38.0;
        map<int, int> bestDirs;

        if (!myAlive.empty()) {
            if (budget > 10.0) {
                jointMCTS(gs, myAlive, oppAlive, budget, bestDirs);
            } else {
                auto dec = coordGreedy(gs, myAlive, false);
                for (auto& [idx, dir] : dec) {
                    bestDirs[idx] = dir;
                }
            }
        }

        vector<string> acts;
        for (int sid : myIds) {
            if (!id2idx.count(sid)) {
                continue;
            }
            int idx = id2idx[sid];
            if (!gs.snakes[idx].alive) {
                continue;
            }
            int d = bestDirs.count(idx) ? bestDirs[idx] : gs.snakes[idx].dir;
            acts.push_back(to_string(sid) + " " + DN[d]);
        }
        if (acts.empty()) {
            acts.push_back("WAIT");
        }

        string out;
        for (int i = 0; i < (int)acts.size(); i++) {
            if (i) {
                out += ";";
            }
            out += acts[i];
        }
        cout << out << "\n";
        cout.flush();
        turn++;
    }
    return 0;
}
