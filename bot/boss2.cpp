#pragma GCC optimize("O3,inline,omit-frame-pointer,unroll-loops")

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <climits>
#include <cmath>
#include <deque>
#include <cstring>
#include <chrono>
#include <cassert>
#include <unordered_set>
#include <unordered_map>

using namespace std;

// =============================================================
// PERFORMANCE PRIMITIVES
// =============================================================
using Clock = chrono::steady_clock;
using Ms = chrono::milliseconds;
inline int64_t elapsed(Clock::time_point t0) {
    return chrono::duration_cast<Ms>(Clock::now() - t0).count();
}

// =============================================================
// GRID & COORDINATE SYSTEM
// =============================================================
constexpr int MAX_W = 50, MAX_H = 30, MAX_CELLS = MAX_W * MAX_H;
int W, H;
bool smallMap = false;   
bool tinyMap = false;    
int totalCells = 0;      

struct Coord {
    int x, y;
    constexpr Coord() : x(-1), y(-1) {}
    constexpr Coord(int x, int y) : x(x), y(y) {}
    inline int idx() const { return y * MAX_W + x; }
    inline bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    inline bool operator!=(const Coord& o) const { return !(*this == o); }
    inline bool operator<(const Coord& o) const { return idx() < o.idx(); }
    inline Coord operator+(const Coord& o) const { return {x + o.x, y + o.y}; }
    inline Coord operator-(const Coord& o) const { return {x - o.x, y - o.y}; }
    inline int manhattan(const Coord& o) const { return abs(x - o.x) + abs(y - o.y); }
    inline bool inBounds() const { return x >= 0 && x < W && y >= 0 && y < H; }
};

struct PH { size_t operator()(const Coord& p) const { return hash<int>()((p.x << 16) ^ p.y); } };
using PS = unordered_set<Coord, PH>;

const Coord DIRS[4] = {{0,-1},{0,1},{-1,0},{1,0}}; // UP DOWN LEFT RIGHT
const string DIR_NAMES[4] = {"UP","DOWN","LEFT","RIGHT"};
constexpr int DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3;

// =============================================================
// BITBOARD
// =============================================================
constexpr int BW = (MAX_CELLS + 63) / 64;
struct BitBoard {
    uint64_t w[BW] = {};
    inline void set(int i)        { w[i >> 6] |= 1ULL << (i & 63); }
    inline void clr(int i)        { w[i >> 6] &= ~(1ULL << (i & 63)); }
    inline bool tst(int i) const  { return (w[i >> 6] >> (i & 63)) & 1; }
    inline void setC(Coord c)     { set(c.idx()); }
    inline void clrC(Coord c)     { clr(c.idx()); }
    inline bool tstC(Coord c) const { return tst(c.idx()); }
    inline BitBoard operator|(const BitBoard& o) const {
        BitBoard r; for (int i = 0; i < BW; i++) r.w[i] = w[i] | o.w[i]; return r;
    }
    inline BitBoard operator&(const BitBoard& o) const {
        BitBoard r; for (int i = 0; i < BW; i++) r.w[i] = w[i] & o.w[i]; return r;
    }
    inline BitBoard operator~() const {
        BitBoard r; for (int i = 0; i < BW; i++) r.w[i] = ~w[i]; return r;
    }
    inline int popcount() const {
        int c = 0; for (int i = 0; i < BW; i++) c += __builtin_popcountll(w[i]); return c;
    }
    inline bool empty() const {
        for (int i = 0; i < BW; i++) if (w[i]) return false; return true;
    }
    inline void reset() { for (int i = 0; i < BW; i++) w[i] = 0; }
};

// =============================================================
// GAME STATE
// =============================================================
constexpr int MAX_SNAKES = 8;
struct Snake {
    int id = -1;
    int owner = -1;
    bool alive = false;
    deque<Coord> body;
    int lastDir = DIR_UP;
    inline Coord head() const { return body.front(); }
    inline Coord tail() const { return body.back(); }
    inline int len() const { return (int)body.size(); }
    inline int facing() const {
        if (body.size() < 2) return DIR_UP;
        Coord d = body[0] - body[1];
        for (int i = 0; i < 4; i++)
            if (DIRS[i].x == d.x && DIRS[i].y == d.y) return i;
        return DIR_UP;
    }
    inline int forbidden() const {
        if (body.size() < 2) return -1;
        Coord d = body[1] - body[0];
        for (int i = 0; i < 4; i++)
            if (DIRS[i].x == d.x && DIRS[i].y == d.y) return i;
        return -1;
    }
};

struct State {
    BitBoard walls;
    BitBoard apples;
    Snake snakes[MAX_SNAKES];
    int nSnakes = 0;
    int turn = 0;
    int losses[2] = {0, 0};
    BitBoard cachedBody;
    bool bodyDirty = true;
    inline void rebuildBodyBoard() {
        cachedBody.reset();
        for (int i = 0; i < nSnakes; i++)
            if (snakes[i].alive)
                for (auto& c : snakes[i].body) cachedBody.setC(c);
        bodyDirty = false;
    }
    inline const BitBoard& bodyBoard() {
        if (bodyDirty) rebuildBodyBoard();
        return cachedBody;
    }
    inline BitBoard bodyBoardConst() const {
        BitBoard b;
        for (int i = 0; i < nSnakes; i++)
            if (snakes[i].alive)
                for (auto& c : snakes[i].body) b.setC(c);
        return b;
    }
    inline int scoreFor(int owner) const {
        int s = 0;
        for (int i = 0; i < nSnakes; i++)
            if (snakes[i].alive && snakes[i].owner == owner) s += snakes[i].len();
        return s;
    }
    inline int aliveCount(int owner) const {
        int c = 0;
        for (int i = 0; i < nSnakes; i++)
            if (snakes[i].alive && snakes[i].owner == owner) c++;
        return c;
    }
};

inline bool cellSupported(int x, int y, const BitBoard& walls, const BitBoard& apples, const BitBoard& blocked) {
    if (y + 1 >= H) return true; 
    Coord below(x, y + 1);
    return walls.tstC(below) || apples.tstC(below) || blocked.tstC(below);
}

void simulate(State& st, const int moves[]) {
    for (int i = 0; i < st.nSnakes; i++) {
        Snake& sn = st.snakes[i];
        if (!sn.alive) continue;
        int dir = moves[i];
        if (dir == sn.forbidden() || dir < 0 || dir > 3) dir = sn.facing();
        sn.lastDir = dir;
        Coord newHead = sn.head() + DIRS[dir];
        bool willEat = newHead.inBounds() && st.apples.tstC(newHead);
        if (!willEat) sn.body.pop_back();
        sn.body.push_front(newHead);
    }
    for (int i = 0; i < st.nSnakes; i++) {
        Snake& sn = st.snakes[i];
        if (!sn.alive) continue;
        if (sn.head().inBounds() && st.apples.tstC(sn.head()))
            st.apples.clrC(sn.head());
    }
    bool toBehead[MAX_SNAKES] = {};
    for (int i = 0; i < st.nSnakes; i++) {
        Snake& sn = st.snakes[i];
        if (!sn.alive) continue;
        Coord h = sn.head();
        if (!h.inBounds()) { toBehead[i] = true; continue; }
        if (st.walls.tstC(h)) { toBehead[i] = true; continue; }
        for (int j = 0; j < st.nSnakes && !toBehead[i]; j++) {
            if (!st.snakes[j].alive) continue;
            if (j == i) {
                for (int k = 1; k < sn.len(); k++)
                    if (sn.body[k] == h) { toBehead[i] = true; break; }
            } else {
                for (auto& p : st.snakes[j].body)
                    if (p == h) { toBehead[i] = true; break; }
            }
        }
    }
    for (int i = 0; i < st.nSnakes; i++) {
        if (!toBehead[i]) continue;
        Snake& sn = st.snakes[i];
        if (sn.len() <= 3) {
            st.losses[sn.owner] += sn.len();
            sn.alive = false;
        } else {
            st.losses[sn.owner]++;
            sn.body.pop_front();
        }
    }
    {
        bool somethingFell = true;
        while (somethingFell) {
            somethingFell = false;
            bool isAirborne[MAX_SNAKES] = {};
            bool isGrounded[MAX_SNAKES] = {};
            for (int i = 0; i < st.nSnakes; i++)
                if (st.snakes[i].alive) isAirborne[i] = true;
            bool gotGrounded = true;
            while (gotGrounded) {
                gotGrounded = false;
                for (int i = 0; i < st.nSnakes; i++) {
                    if (!isAirborne[i]) continue;
                    Snake& sn = st.snakes[i];
                    bool grnd = false;
                    for (auto& c : sn.body) {
                        if (!c.inBounds()) continue;
                        Coord below(c.x, c.y + 1);
                        if (below.y >= H || st.walls.tstC(below) || st.apples.tstC(below)) { grnd = true; break; }
                        for (int gi = 0; gi < st.nSnakes; gi++) {
                            if (!isGrounded[gi]) continue;
                            for (auto& gp : st.snakes[gi].body)
                                if (gp == below) { grnd = true; break; }
                            if (grnd) break;
                        }
                        if (grnd) break;
                    }
                    if (grnd) {
                        isGrounded[i] = true;
                        isAirborne[i] = false;
                        gotGrounded = true;
                    }
                }
            }
            for (int i = 0; i < st.nSnakes; i++) {
                if (!isAirborne[i]) continue;
                somethingFell = true;
                Snake& sn = st.snakes[i];
                for (auto& c : sn.body) c.y++;
                bool allOut = true;
                for (auto& c : sn.body)
                    if (c.y < H) { allOut = false; break; }
                if (allOut) sn.alive = false;
            }
        }
    }
    st.bodyDirty = true;
    st.turn++;
}

// =============================================================
// FLOOD-FILL 1D (Rapide)
// =============================================================
static int ff_visited[MAX_CELLS];
static int ff_token = 0;

int floodFillCount(Coord src, const BitBoard& walls, const BitBoard& blocked) {
    if (!src.inBounds()) return 0;
    ff_token++;
    Coord q[MAX_CELLS];
    int qHead = 0, qTail = 0;
    q[qTail++] = src;
    ff_visited[src.idx()] = ff_token;
    int count = 0;
    while (qHead < qTail) {
        Coord c = q[qHead++];
        count++;
        for (int d = 0; d < 4; d++) {
            Coord n = c + DIRS[d];
            if (!n.inBounds()) continue;
            int nidx = n.idx();
            if (ff_visited[nidx] == ff_token) continue;
            if (walls.tst(nidx) || blocked.tst(nidx)) continue;
            ff_visited[nidx] = ff_token;
            q[qTail++] = n;
        }
    }
    return count;
}

static int ffg_visited[MAX_H][MAX_W];
static int ffg_token = 0;

int floodFillGravity(Coord src, const BitBoard& walls, const BitBoard& apples, const BitBoard& blocked) {
    if (!src.inBounds()) return 0;
    ffg_token++;
    Coord q[MAX_CELLS];
    int qHead = 0, qTail = 0;
    q[qTail++] = src;
    ffg_visited[src.y][src.x] = ffg_token;
    int count = 0;
    while (qHead < qTail) {
        Coord c = q[qHead++];
        count++;
        for (int d = 0; d < 4; d++) {
            int nx = c.x + DIRS[d].x, ny = c.y + DIRS[d].y;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            
            Coord next(nx, ny);
            if (!walls.tstC(next) && !blocked.tstC(next)) {
                if (ffg_visited[ny][nx] != ffg_token) {
                    ffg_visited[ny][nx] = ffg_token;
                    q[qTail++] = next;
                }
            }

            int finalY = ny;
            if (!cellSupported(nx, ny, walls, apples, blocked)) {
                int fy = ny;
                while (fy + 1 < H) {
                    Coord below(nx, fy + 1);
                    if (walls.tstC(below) || apples.tstC(below) || blocked.tstC(below)) break;
                    fy++;
                }
                if (fy + 1 >= H) {
                    Coord bottom(nx, fy);
                    if (fy < H && !walls.tstC(bottom) && !blocked.tstC(bottom)) finalY = fy;
                    else continue;
                } else {
                    finalY = fy;
                }
            }
            if (ffg_visited[finalY][nx] == ffg_token) continue;
            Coord dest(nx, finalY);
            if (walls.tstC(dest) || blocked.tstC(dest)) continue;
            ffg_visited[finalY][nx] = ffg_token;
            q[qTail++] = dest;
        }
    }
    return count;
}

// =============================================================
// VORONOI AVEC BUCKET QUEUE
// =============================================================
struct VoronoiResult {
    int territory[2] = {0, 0};
    int energyControl[2] = {0, 0};
    int reachable[MAX_SNAKES] = {};
    int closestEnergy[MAX_SNAKES];
    struct AppleInfo {
        int bestSi = -1, bestDist = INT_MAX;
        int secondSi = -1, secondDist = INT_MAX;
    };
    AppleInfo appleInfos[200];
    int nApples = 0;
};

struct VBucket {
    vector<uint32_t> data;
    inline void push(int x, int y, int si, int tether) {
        data.push_back((x) | (y << 8) | (si << 16) | (tether << 24));
    }
    inline uint32_t pop(int& x, int& y, int& si, int& tether) {
        uint32_t val = data.back();
        data.pop_back();
        x = val & 0xFF;
        y = (val >> 8) & 0xFF;
        si = (val >> 16) & 0xFF;
        tether = (val >> 24) & 0xFF;
        return val;
    }
    inline bool empty() const { return data.empty(); }
    inline void reset() { data.clear(); }
};
static VBucket vbuckets[4096];

VoronoiResult computeVoronoi(const State& st, const BitBoard& blocked, Coord appleList[], int nApples) {
    VoronoiResult vr;
    vr.nApples = nApples;
    fill(vr.closestEnergy, vr.closestEnergy + MAX_SNAKES, INT_MAX);
    
    static int dist1[MAX_H][MAX_W];
    static int snake1[MAX_H][MAX_W];
    static int dist2[MAX_H][MAX_W];
    static int snake2[MAX_H][MAX_W];
    
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            dist1[y][x] = INT_MAX; snake1[y][x] = -1;
            dist2[y][x] = INT_MAX; snake2[y][x] = -1;
        }
    }
    
    int vmax_bucket = 0;
    int vmin_bucket = 0;
    
    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive) continue;
        Coord h = st.snakes[i].head();
        if (h.inBounds()) vbuckets[0].push(h.x, h.y, i, 0); 
    }
    
    while (vmin_bucket <= vmax_bucket) {
        if (vbuckets[vmin_bucket].empty()) {
            vmin_bucket++;
            continue;
        }
        
        int cx, cy, si, tether;
        vbuckets[vmin_bucket].pop(cx, cy, si, tether);
        int cost = vmin_bucket;
        
        bool dominated = false;
        if (cost < dist1[cy][cx]) {
            if (snake1[cy][cx] != si) {
                dist2[cy][cx] = dist1[cy][cx];
                snake2[cy][cx] = snake1[cy][cx];
            }
            dist1[cy][cx] = cost;
            snake1[cy][cx] = si;
        } else if (snake1[cy][cx] != si && cost < dist2[cy][cx]) {
            dist2[cy][cx] = cost;
            snake2[cy][cx] = si;
        } else {
            dominated = true;
        }
        
        if (dominated) continue;
        
        if (cost == dist1[cy][cx] && snake1[cy][cx] == si) {
            int owner = st.snakes[si].owner;
            vr.territory[owner]++;
            vr.reachable[si]++;
            Coord c(cx, cy);
            if (st.apples.tstC(c)) {
                vr.energyControl[owner]++;
                vr.closestEnergy[si] = min(vr.closestEnergy[si], cost);
            }
        }
        
        for (int d = 0; d < 4; d++) {
            int nx = cx + DIRS[d].x, ny = cy + DIRS[d].y;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            Coord next(nx, ny);
            if (blocked.tstC(next)) continue;

            bool supported = cellSupported(nx, ny, st.walls, st.apples, blocked);
            int nextTether = supported ? 0 : (tether + 1);

            if (nextTether < st.snakes[si].len()) {
                int cDirect = cost + (supported ? 1 : 2); 
                if (cDirect < dist2[ny][nx] || cDirect < dist1[ny][nx]) {
                    int bcost = min(cDirect, 4095);
                    vbuckets[bcost].push(nx, ny, si, nextTether); 
                    if (bcost > vmax_bucket) vmax_bucket = bcost;
                }
            } 
            
            if (!supported) {
                int fy = ny;
                while (fy + 1 < H && !cellSupported(nx, fy + 1, st.walls, st.apples, blocked)) {
                    fy++;
                }
                Coord bottom(nx, fy);
                if (fy < H && !st.walls.tstC(bottom) && !blocked.tstC(bottom)) {
                    int fallDist = fy - ny;
                    int cFall = cost + 1 + fallDist * 2; 
                    if (cFall < dist2[fy][nx] || cFall < dist1[fy][nx]) {
                        int bcost = min(cFall, 4095);
                        vbuckets[bcost].push(nx, fy, si, 0); 
                        if (bcost > vmax_bucket) vmax_bucket = bcost;
                    }
                }
            }
        }
    }
    
    for(int i = 0; i <= vmax_bucket; ++i) vbuckets[i].reset();
    
    for (int a = 0; a < nApples; a++) {
        Coord ap = appleList[a];
        if (!ap.inBounds()) continue;
        vr.appleInfos[a].bestSi = snake1[ap.y][ap.x];
        vr.appleInfos[a].bestDist = dist1[ap.y][ap.x];
        vr.appleInfos[a].secondSi = snake2[ap.y][ap.x];
        vr.appleInfos[a].secondDist = dist2[ap.y][ap.x];
    }
    return vr;
}

double gravityExploitScore(const State& st, int myOwner, const BitBoard& blocked) {
    double score = 0.0;
    int oppOwner = 1 - myOwner;
    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive || st.snakes[i].owner != oppOwner) continue;
        for (auto& c : st.snakes[i].body) {
            Coord below(c.x, c.y + 1);
            if (!below.inBounds()) continue;
            if (st.apples.tstC(below)) {
                int fallDist = 0;
                Coord fp = below;
                while (fp.y + 1 < H) {
                    Coord b(fp.x, fp.y + 1);
                    if (st.walls.tstC(b)) break;
                    fp.y++;
                    fallDist++;
                }
                if (fp.y + 1 >= H && !cellSupported(fp.x, fp.y, st.walls, st.apples, blocked))
                    score += 200.0; 
                else if (fallDist >= 3) score += fallDist * 15.0;
            }
        }
    }
    return score;
}

int validMovesSafe(const State& st, int si, const BitBoard& blocked, int out[]);

double snakeGravityRisk(const Snake& sn, const BitBoard& walls, const BitBoard& apples, const BitBoard& blocked) {
    if (!sn.alive) return 0;
    Coord h = sn.head();
    if (!h.inBounds()) return -300.0;

    bool anySupported = false;
    for (auto& c : sn.body) {
        if (!c.inBounds()) continue;
        if (cellSupported(c.x, c.y, walls, apples, blocked)) {
            anySupported = true; break;
        }
    }
    if (anySupported) return 0;

    int minFall = INT_MAX;
    for (auto& c : sn.body) {
        if (!c.inBounds()) continue;
        int fy = c.y;
        while (fy + 1 < H) {
            Coord b(c.x, fy + 1);
            if (walls.tstC(b)) break;
            fy++;
        }
        int fall = fy - c.y;
        if (fy + 1 >= H && !cellSupported(c.x, fy, walls, apples, blocked)) fall = 100;
        minFall = min(minFall, fall);
    }
    if (minFall >= 100) return -500.0;
    if (minFall >= 3) return -minFall * 30.0;
    return -minFall * 10.0;
}

double headCollisionPenalty(const State& st, int si, const BitBoard& blocked) {
    const Snake& sn = st.snakes[si];
    if (!sn.alive) return 0;
    Coord h = sn.head();
    if (!h.inBounds()) return 0;
    double penalty = 0;
    for (int j = 0; j < st.nSnakes; j++) {
        if (j == si || !st.snakes[j].alive || st.snakes[j].owner == sn.owner) continue;
        Coord oh = st.snakes[j].head();
        if (!oh.inBounds()) continue;
        int hd = h.manhattan(oh);
        int myLen = sn.len(), oppLen = st.snakes[j].len();
        
        if (hd <= 1) {
            if (myLen <= oppLen) penalty -= (smallMap ? 200.0 : 80.0);
            else penalty += (smallMap ? 30.0 : 15.0);
        } else if (hd == 2) {
            if (myLen <= oppLen) penalty -= (smallMap ? 80.0 : 20.0);
            if (smallMap && (h.x == oh.x || h.y == oh.y) && myLen <= oppLen)
                penalty -= 40.0;
        }
    }
    return penalty;
}

double edgePenalty(Coord h) {
    if (!h.inBounds()) return 0;
    double pen = 0;
    int dx = min(h.x, W - 1 - h.x);
    int dy = min(h.y, H - 1 - h.y);
    if (smallMap) {
        if (dx == 0) pen -= 15.0;
        if (dy == 0) pen -= 10.0;
        if (dx == 0 && dy == 0) pen -= 25.0;
    }
    return pen;
}

// =============================================================
// EVALUATION FUNCTION
// =============================================================
double evaluate(const State& st, int myOwner, bool fast = false, const State* initSt = nullptr, double stallUrgency = 1.0, const map<int, deque<Coord>>* hist = nullptr) {
    int opp = 1 - myOwner;
    int myAlive = st.aliveCount(myOwner);
    int oppAlive = st.aliveCount(opp);
    
    if (myAlive == 0 && oppAlive == 0) return 0;
    if (myAlive == 0) return -1e6;
    if (oppAlive == 0 && myAlive > 0) return 1e5 + st.scoreFor(myOwner) * 100.0;
    
    int myLen = st.scoreFor(myOwner);
    int oppLen = st.scoreFor(opp);
    int turnsLeft = 200 - st.turn;
    double score = 0;
    double phase = (turnsLeft > 140) ? 0.0 : ((turnsLeft > 60) ? (140.0 - turnsLeft) / 80.0 : 1.0);
    int lenDiff = myLen - oppLen;
    bool winning = lenDiff > 2;
    bool losing = lenDiff < -2;
    
    double lenWeight = 150.0;
    if (smallMap) lenWeight = 200.0; 
    if (phase > 0.5 && winning) lenWeight = (smallMap ? 300.0 : 250.0);
    if (phase > 0.7 && losing) lenWeight = 100.0;
    score += lenDiff * lenWeight;

    // --- FIX: ANTI-TRAMPOLINE ET ANTI-STALLING GLOBAL ---
    if (initSt != nullptr) {
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            
            // Rebondit sur place
            if (st.snakes[i].head() == initSt->snakes[i].head()) {
                score -= 50000.0; 
            }
            
            // Manger = Bonus
            int lenDelta = st.snakes[i].len() - initSt->snakes[i].len();
            if (lenDelta > 0) {
                score += lenDelta * 15000.0 * stallUrgency; 
            }
            
            // ANTI-STALLING: Interdit de repasser sur une des 6 dernières positions !
            if (hist != nullptr) {
                Coord h = st.snakes[i].head();
                auto it = hist->find(st.snakes[i].id);
                if (it != hist->end()) {
                    for (const auto& past_h : it->second) {
                        if (h == past_h) {
                            score -= 2000.0 * stallUrgency;
                            break;
                        }
                    }
                }
            }
        }
    }

    Coord appleList[200]; int nApples = 0;
    for (int y = 0; y < H && nApples < 200; y++)
        for (int x = 0; x < W && nApples < 200; x++)
            if (st.apples.tstC({x, y})) appleList[nApples++] = {x, y};
            
    double scarcityMult = (nApples <= 2) ? 2.5 : (nApples <= 5) ? 2.0 : (nApples <= 10) ? 1.5 : 1.0;
    if (smallMap && scarcityMult < 1.5) scarcityMult = 1.5;
    double advantageMult = (myAlive > oppAlive) ? 1.0 + 0.3 * (myAlive - oppAlive) : 1.0;
    double aggressionMult = (phase > 0.6 && winning) ? 0.6 : ((phase > 0.5 && losing) ? 1.5 : 1.0);
    
    double growthWeight = 200.0 * scarcityMult * advantageMult * aggressionMult * stallUrgency;
    
    BitBoard bodyBrd = st.bodyBoardConst();
    BitBoard blocked = st.walls | bodyBrd;
    
    int staircases = 0;
    BitBoard myBrd;
    for(int i = 0; i < st.nSnakes; ++i) {
        if(st.snakes[i].alive && st.snakes[i].owner == myOwner) {
            for(auto& c : st.snakes[i].body) myBrd.setC(c);
        }
    }
    for(int i = 0; i < st.nSnakes; ++i) {
        if(st.snakes[i].alive && st.snakes[i].owner == myOwner) {
            for(auto& c : st.snakes[i].body) {
                if(c.y + 1 < H && myBrd.tstC({c.x, c.y + 1})) staircases++;
            }
        }
    }
    score += staircases * 8.0;

    if (fast) {
        bool appleAssigned[200] = {};
        struct SnakeApple { int si; int ai; int dist; };
        SnakeApple candidates[200 * MAX_SNAKES]; int nCand = 0;
        bool snakeAssigned[MAX_SNAKES] = {};
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            Coord h = st.snakes[i].head();
            if (!h.inBounds()) continue;
            for (int a = 0; a < nApples; a++) {
                int md = h.manhattan(appleList[a]);
                candidates[nCand++] = {i, a, md};
            }
        }
        sort(candidates, candidates + nCand, [](const SnakeApple& a, const SnakeApple& b) { return a.dist < b.dist; });
        for (int ci = 0; ci < nCand; ci++) {
            auto& ca = candidates[ci];
            if (snakeAssigned[ca.si] || appleAssigned[ca.ai]) continue;
            snakeAssigned[ca.si] = true;
            appleAssigned[ca.ai] = true;
            score += growthWeight / (1.0 + ca.dist);
        }
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            if (snakeAssigned[i]) continue;
            Coord h = st.snakes[i].head();
            if (!h.inBounds()) continue;
            int bestMD = INT_MAX;
            for (int a = 0; a < nApples; a++) bestMD = min(bestMD, h.manhattan(appleList[a]));
            if (bestMD < INT_MAX) score += growthWeight * 0.7 / (1.0 + bestMD);
        }
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != opp) continue;
            Coord h = st.snakes[i].head();
            if (!h.inBounds()) continue;
            int bestMD = INT_MAX;
            for (int a = 0; a < nApples; a++) bestMD = min(bestMD, h.manhattan(appleList[a]));
            if (bestMD <= 3) score -= 60.0 / (1.0 + bestMD);
        }
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            int safeMoves[4]; int nSafe = validMovesSafe(st, i, blocked, safeMoves);
            if (nSafe == 0) score -= (smallMap ? 500.0 : 300.0);
            else if (nSafe == 1) score -= (smallMap ? 150.0 : 80.0);
            else if (nSafe == 2 && smallMap) score -= 30.0;
        }
        if (smallMap) {
            for (int i = 0; i < st.nSnakes; i++) {
                if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
                Coord h = st.snakes[i].head();
                if (!h.inBounds()) continue;
                int reachable = floodFillCount(h, st.walls, blocked);
                int snLen = st.snakes[i].len();
                if (reachable < snLen) score -= (snLen - reachable + 1) * 180.0;
                else if (reachable < snLen * 2) score -= (snLen * 2 - reachable) * 25.0;
            }
        }
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            Coord h = st.snakes[i].head();
            Coord t = st.snakes[i].tail();
            if (!h.inBounds() || !t.inBounds()) continue;
            int tailDist = h.manhattan(t);
            double tailWeight = (phase > 0.6 && winning) ? 30.0 : ((nApples == 0) ? 25.0 : 5.0);
            if (smallMap) tailWeight *= 1.5; 
            if (tailDist > 0) score += tailWeight / (1.0 + tailDist);
        }
        if (smallMap) {
            for (int i = 0; i < st.nSnakes; i++) {
                if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
                score += headCollisionPenalty(st, i, blocked);
                score += edgePenalty(st.snakes[i].head());
            }
        }
    } else {
        VoronoiResult vr = computeVoronoi(st, blocked, appleList, nApples);
        for (int a = 0; a < nApples; a++) {
            auto& ai = vr.appleInfos[a];
            if (ai.bestSi < 0 || ai.bestDist == INT_MAX) continue;

            int bestOwner = st.snakes[ai.bestSi].owner;
            int secondOwner = (ai.secondSi >= 0) ? st.snakes[ai.secondSi].owner : -1;

            if (bestOwner == myOwner) {
                if (ai.secondSi >= 0 && secondOwner == opp && ai.bestDist == ai.secondDist && st.snakes[ai.secondSi].len() >= st.snakes[ai.bestSi].len()) {
                    score -= growthWeight * 0.5 / (1.0 + ai.bestDist); 
                } else if (ai.secondSi < 0 || ai.secondDist == INT_MAX || secondOwner == myOwner) {
                    score += growthWeight * 1.2 / (1.0 + ai.bestDist);
                } else if (ai.bestDist < ai.secondDist - 1) {
                    score += growthWeight / (1.0 + ai.bestDist);
                } else if (ai.bestDist < ai.secondDist) {
                    score += growthWeight * 0.8 / (1.0 + ai.bestDist);
                } else {
                    score += growthWeight * 0.4 / (1.0 + ai.bestDist);
                }
            } else {
                if (ai.secondSi >= 0 && st.snakes[ai.secondSi].owner == myOwner) {
                    int myDist = ai.secondDist;
                    if (myDist < ai.bestDist + 2) {
                        score += growthWeight * 0.2 / (1.0 + myDist);
                    } else {
                        score -= 20.0 / (1.0 + ai.bestDist);
                    }
                } else {
                    score -= 20.0 / (1.0 + ai.bestDist);
                }
            }
        }

        score += (vr.energyControl[myOwner] - vr.energyControl[opp]) * 40.0;

        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            if (vr.closestEnergy[i] < INT_MAX) {
                score += 80.0 * scarcityMult * stallUrgency / (1.0 + vr.closestEnergy[i]);
            } else {
                int bestMD = INT_MAX;
                Coord h = st.snakes[i].head();
                for (int a = 0; a < nApples; a++) bestMD = min(bestMD, h.manhattan(appleList[a]));
                if (bestMD < INT_MAX) {
                    score += 40.0 * scarcityMult * stallUrgency / (1.0 + bestMD);
                } else {
                    score -= 50.0;
                }
            }
        }

        double territoryWeight = (phase > 0.7 && winning) ? 1.5 : 0.5;
        if (smallMap) {
            territoryWeight = (phase > 0.5) ? 3.0 : 1.5;
            if (tinyMap) territoryWeight *= 1.5; 
            if (winning && phase > 0.5) territoryWeight *= 1.5;
        }
        score += (vr.territory[myOwner] - vr.territory[opp]) * territoryWeight;

        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            Coord h = st.snakes[i].head();
            if (!h.inBounds()) continue;
            
            int reachable = floodFillCount(h, st.walls, blocked);
            int snLen = st.snakes[i].len();
            double trapMultiplier = smallMap ? 2.0 : 1.0;
            if (tinyMap) trapMultiplier = 3.0;
            if (reachable < snLen) score -= (snLen - reachable + 1) * 120.0 * trapMultiplier;
            else if (reachable < snLen * 2) score -= (snLen * 2 - reachable) * 15.0 * trapMultiplier;
            else score += log(reachable + 1) * 1.5;
            
            if (smallMap) {
                int freeCells = totalCells; 
                double spaceRatio = (double)reachable / max(1, freeCells);
                if (spaceRatio > 0.5) score += 50.0; 
                else if (spaceRatio < 0.2) score -= 60.0; 
            }
        }

        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != opp) continue;
            Coord h = st.snakes[i].head();
            if (!h.inBounds()) continue;
            int reachable = floodFillCount(h, st.walls, blocked); 
            int snLen = st.snakes[i].len();
            double trapBonus = smallMap ? 100.0 : 60.0;
            if (reachable < snLen) {
                score += (snLen - reachable + 1) * trapBonus;
            }
        }

        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            Coord h = st.snakes[i].head();
            Coord t = st.snakes[i].tail();
            if (!h.inBounds() || !t.inBounds()) continue;
            int tailDist = h.manhattan(t);
            double tailWeight = (phase > 0.6 && winning) ? 40.0 : ((nApples == 0) ? 35.0 : ((vr.closestEnergy[i] == INT_MAX) ? 30.0 : 5.0));
            if (smallMap) tailWeight *= 1.5;
            if (tailDist > 0) score += tailWeight / (1.0 + tailDist);
        }
        score += gravityExploitScore(st, myOwner, blocked) * 0.5;
        for (int i = 0; i < st.nSnakes; i++) {
            if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
            score += headCollisionPenalty(st, i, blocked);
            score += edgePenalty(st.snakes[i].head());
        }
    }

    double aliveWeight = (phase < 0.3) ? 120.0 : ((phase < 0.7) ? 80.0 : 40.0);
    if (winning && phase > 0.5) aliveWeight *= 1.5;
    if (smallMap) aliveWeight *= 1.3; 
    score += (myAlive - oppAlive) * aliveWeight;

    int myMaxLen = 0, oppMaxLen = 0;
    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive) continue;
        if (st.snakes[i].owner == myOwner) myMaxLen = max(myMaxLen, st.snakes[i].len());
        else oppMaxLen = max(oppMaxLen, st.snakes[i].len());
    }
    if (oppMaxLen > myMaxLen + 2) score -= (oppMaxLen - myMaxLen) * 20.0;

    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
        double risk = snakeGravityRisk(st.snakes[i], st.walls, st.apples, blocked);
        score += risk * (smallMap ? 0.8 : 0.5);
    }

    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
        if (st.snakes[i].len() <= 3) score -= (smallMap ? 250.0 : 150.0);
        else if (st.snakes[i].len() == 4) score -= (smallMap ? 80.0 : 40.0);
    }

    for (int i = 0; i < st.nSnakes; i++) {
        if (!st.snakes[i].alive || st.snakes[i].owner != myOwner) continue;
        int safeMoves[4]; int nSafe = validMovesSafe(st, i, blocked, safeMoves);
        if (nSafe == 0) score -= (smallMap ? 400.0 : 200.0);
        else if (nSafe == 1) score -= (smallMap ? 100.0 : 50.0);
        else if (nSafe == 2 && smallMap) score -= 20.0;
    }

    return score;
}

int validMovesCount(const State& st, int si, int out[]) {
    const Snake& sn = st.snakes[si];
    if (!sn.alive) return 0;
    int forb = sn.forbidden();
    int n = 0;
    for (int d = 0; d < 4; d++) {
        if (d == forb) continue;
        Coord nh = sn.head() + DIRS[d];
        if (!nh.inBounds() || st.walls.tstC(nh)) continue;
        out[n++] = d;
    }
    if (n == 0) { for (int d = 0; d < 4; d++) out[n++] = d; }
    return n;
}

int validMovesSafe(const State& st, int si, const BitBoard& blocked, int out[]) {
    const Snake& sn = st.snakes[si];
    if (!sn.alive) return 0;
    int forb = sn.forbidden();
    BitBoard blk = blocked;
    if (sn.len() >= 2) blk.clrC(sn.body.back());

    int safe[4], nSafe = 0;
    int risky[4], nRisky = 0;
    for (int d = 0; d < 4; d++) {
        if (d == forb) continue;
        Coord nh = sn.head() + DIRS[d];
        if (!nh.inBounds()) { risky[nRisky++] = d; continue; }
        if (st.walls.tstC(nh)) continue;
        if (blk.tstC(nh)) { risky[nRisky++] = d; continue; }
        safe[nSafe++] = d;
    }
    if (nSafe > 0) { for (int i = 0; i < nSafe; i++) out[i] = safe[i]; return nSafe; }
    if (nRisky > 0) { for (int i = 0; i < nRisky; i++) out[i] = risky[i]; return nRisky; }
    int n = 0; for (int d = 0; d < 4; d++) out[n++] = d; return n;
}

int greedyMove(const State& st, int si, const BitBoard* preBlocked = nullptr) {
    BitBoard blocked;
    if (preBlocked) blocked = *preBlocked;
    else blocked = st.walls | st.bodyBoardConst();
    
    int moves[4]; int nm = validMovesSafe(st, si, blocked, moves);
    if (nm == 0) return st.snakes[si].facing();

    Coord h = st.snakes[si].head();
    if (!h.inBounds()) return moves[0];

    int bestMove = moves[0]; double bestScore = -1e9;
    for (int mi = 0; mi < nm; mi++) {
        Coord nh = h + DIRS[moves[mi]];
        double sc = 0;

        if (nh.inBounds() && st.apples.tstC(nh)) sc += 1000;

        int bestMD = INT_MAX;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (st.apples.tstC({x, y})) {
                    int md = (nh.inBounds() ? nh.manhattan({x, y}) : 100);
                    bestMD = min(bestMD, md);
                }
        if (bestMD < INT_MAX) sc -= bestMD * 10;

        if (nh.inBounds()) {
            if (cellSupported(nh.x, nh.y, st.walls, st.apples, blocked)) sc += 5.0;
            if (moves[mi] == DIR_UP) sc -= 2.0;
            if (smallMap) sc += edgePenalty(nh) * 0.3;
        }

        if (nh.inBounds() && blocked.tstC(nh)) sc -= 500;

        for (int j = 0; j < st.nSnakes; j++) {
            if (j == si || !st.snakes[j].alive) continue;
            Coord oh = st.snakes[j].head();
            if (!oh.inBounds() || !nh.inBounds()) continue;
            int hd = nh.manhattan(oh);
            if (hd <= 1 && st.snakes[si].len() <= st.snakes[j].len()) sc -= (smallMap ? 200 : 100);
            if (hd <= 1 && st.snakes[si].len() > st.snakes[j].len() + 1) sc += 30;
        }

        if (smallMap && nh.inBounds()) {
            BitBoard tmpBlk = blocked;
            tmpBlk.setC(h); 
            int reach = floodFillCount(nh, st.walls, tmpBlk);
            if (reach < st.snakes[si].len()) sc -= 300;
            else if (reach < st.snakes[si].len() * 2) sc -= 50;
        }

        if (sc > bestScore) { bestScore = sc; bestMove = moves[mi]; }
    }
    return bestMove;
}

int smartOppMove(const State& st, int si, const BitBoard& blocked) {
    int oppOwner = st.snakes[si].owner;
    int moves[4]; int nm = validMovesSafe(st, si, blocked, moves);
    if (nm <= 1) return (nm == 1) ? moves[0] : st.snakes[si].facing();

    int bestMove = moves[0]; double bestScore = -1e9;
    for (int mi = 0; mi < nm; mi++) {
        State simSt = st;
        int allMoves[MAX_SNAKES] = {};
        for (int i = 0; i < st.nSnakes; i++) allMoves[i] = st.snakes[i].facing();
        allMoves[si] = moves[mi];
        simulate(simSt, allMoves);
        double sc = evaluate(simSt, oppOwner, true);
        if (sc > bestScore) { bestScore = sc; bestMove = moves[mi]; }
    }
    return bestMove;
}

// =============================================================
// BEAM SEARCH
// =============================================================
struct BeamNode {
    State state;
    int firstMoves[MAX_SNAKES];
    double score;
};

static vector<BeamNode> beamA;
static vector<BeamNode> beamB;

vector<int> beamSearch(const State& initSt, int myOwner, int64_t budgetMs, double stallUrgency, const map<int, deque<Coord>>& hist) {
    auto t0 = Clock::now();
    beamA.clear();
    beamB.clear();

    int myIdx[MAX_SNAKES], oppIdx[MAX_SNAKES];
    int nMy = 0, nOpp = 0;
    for (int i = 0; i < initSt.nSnakes; i++) {
        if (!initSt.snakes[i].alive) continue;
        if (initSt.snakes[i].owner == myOwner) myIdx[nMy++] = i;
        else oppIdx[nOpp++] = i;
    }
    if (nMy == 0) return {};

    int totalAlive = nMy + nOpp;
    int beamWidth, beamDepthMax, comboLimit;
    
    if (smallMap) {
        if (tinyMap) {
            beamWidth = 100; beamDepthMax = 8; comboLimit = 20;
        } else {
            beamWidth = 150; beamDepthMax = 7; comboLimit = 24;
        }
    } else if (totalAlive <= 4) {
        beamWidth = 180; beamDepthMax = 6; comboLimit = 27;
    } else if (totalAlive <= 6) {
        beamWidth = 120; beamDepthMax = 5; comboLimit = 18;
    } else {
        beamWidth = 70; beamDepthMax = 4; comboLimit = 12;
    }

    BitBoard initBody = initSt.bodyBoardConst();
    BitBoard initBlocked = initSt.walls | initBody;

    vector<vector<int>> combos = {{}};
    for (int mi = 0; mi < nMy; mi++) {
        int moves[4]; int nm = validMovesSafe(initSt, myIdx[mi], initBlocked, moves);
        vector<vector<int>> next;
        next.reserve(combos.size() * nm);
        for (auto& c : combos)
            for (int j = 0; j < nm; j++) {
                auto nc = c; nc.push_back(moves[j]);
                next.push_back(nc);
            }
        combos = next;
    }

    vector<vector<int>> oppCombos = {{}};
    int oppComboLimit = smallMap ? 12 : 9; 
    for (int oi = 0; oi < nOpp; oi++) {
        int moves[4]; int nm = validMovesSafe(initSt, oppIdx[oi], initBlocked, moves);
        vector<vector<int>> next;
        for (auto& c : oppCombos)
            for (int j = 0; j < nm; j++) {
                auto nc = c; nc.push_back(moves[j]);
                next.push_back(nc);
            }
        oppCombos = next;
        if ((int)oppCombos.size() > oppComboLimit) break;
    }
    if ((int)oppCombos.size() > oppComboLimit) {
        int defaultOppMoves[MAX_SNAKES];
        bool useSmart = (nOpp <= 2 && elapsed(t0) < budgetMs / 3);
        if (smallMap) useSmart = true; 
        for (int oi = 0; oi < nOpp; oi++) {
            if (useSmart)
                defaultOppMoves[oi] = smartOppMove(initSt, oppIdx[oi], initBlocked);
            else
                defaultOppMoves[oi] = greedyMove(initSt, oppIdx[oi], &initBlocked);
        }

        vector<int> defaultCombo;
        for (int oi = 0; oi < nOpp; oi++) defaultCombo.push_back(defaultOppMoves[oi]);

        vector<vector<int>> filtered;
        filtered.push_back(defaultCombo);
        for (auto& oc : oppCombos) {
            if ((int)filtered.size() >= oppComboLimit) break;
            if (oc != defaultCombo) filtered.push_back(oc);
        }
        oppCombos = filtered;
    }

    bool useFullEval = (int)combos.size() * (int)oppCombos.size() <= 300 || smallMap;
    bool useMinimax = (int)oppCombos.size() > 1 && elapsed(t0) < budgetMs - 20;

    for (auto& combo : combos) {
        if (elapsed(t0) > budgetMs - 12) break;

        double worstScore = 1e9;

        if (useMinimax) {
            for (auto& oppCombo : oppCombos) {
                if (elapsed(t0) > budgetMs - 12) break;

                State simSt = initSt;
                int allMoves[MAX_SNAKES] = {};
                for (int i = 0; i < initSt.nSnakes; i++) allMoves[i] = -1;
                for (int i = 0; i < nMy; i++) allMoves[myIdx[i]] = combo[i];
                for (int i = 0; i < nOpp && i < (int)oppCombo.size(); i++)
                    allMoves[oppIdx[i]] = oppCombo[i];

                simulate(simSt, allMoves);
                double sc = evaluate(simSt, myOwner, !useFullEval, &initSt, stallUrgency, &hist);
                worstScore = min(worstScore, sc);
            }
        } else {
            State simSt = initSt;
            int allMoves[MAX_SNAKES] = {};
            for (int i = 0; i < initSt.nSnakes; i++) allMoves[i] = -1;
            for (int i = 0; i < nMy; i++) allMoves[myIdx[i]] = combo[i];
            for (int i = 0; i < nOpp; i++)
                allMoves[oppIdx[i]] = greedyMove(initSt, oppIdx[i], &initBlocked);
            simulate(simSt, allMoves);
            worstScore = evaluate(simSt, myOwner, !useFullEval, &initSt, stallUrgency, &hist);
        }

        BeamNode node;
        node.state = initSt;
        for (int i = 0; i < nMy; i++) node.firstMoves[myIdx[i]] = combo[i];
        node.score = worstScore;
        beamA.push_back(node);
    }

    sort(beamA.begin(), beamA.end(), [](const BeamNode& a, const BeamNode& b) {
        return a.score > b.score;
    });
    if ((int)beamA.size() > beamWidth) beamA.resize(beamWidth);

    {
        int oppMoves[MAX_SNAKES];
        for (int oi = 0; oi < nOpp; oi++)
            oppMoves[oi] = greedyMove(initSt, oppIdx[oi], &initBlocked);

        for (auto& node : beamA) {
            node.state = initSt;
            int allMoves[MAX_SNAKES] = {};
            for (int i = 0; i < initSt.nSnakes; i++) allMoves[i] = -1;
            for (int i = 0; i < nMy; i++) allMoves[myIdx[i]] = node.firstMoves[myIdx[i]];
            for (int i = 0; i < nOpp; i++) allMoves[oppIdx[i]] = oppMoves[i];
            simulate(node.state, allMoves);
        }
    }

    if (!useFullEval && elapsed(t0) < budgetMs - 25) {
        int reEvalCount = min((int)beamA.size(), 25);
        for (int i = 0; i < reEvalCount; i++) {
            beamA[i].score = evaluate(beamA[i].state, myOwner, false, &initSt, stallUrgency, &hist);
        }
        sort(beamA.begin(), beamA.end(), [](const BeamNode& a, const BeamNode& b) {
            return a.score > b.score;
        });
    }

    int reachedDepth = 1;

    for (int depth = 1; depth < beamDepthMax; depth++) {
        if (elapsed(t0) > budgetMs - 8) break; // Hard exit check
        beamB.clear();
        
        bool timeout = false;
        int evalCount = 0;

        for (auto& node : beamA) {
            if (timeout) break;

            BitBoard nodeBody = node.state.bodyBoardConst();
            BitBoard nodeBlocked = node.state.walls | nodeBody;

            int cMyIdx[MAX_SNAKES], cOppIdx[MAX_SNAKES];
            int cNMy = 0, cNOpp = 0;
            for (int i = 0; i < node.state.nSnakes; i++) {
                if (!node.state.snakes[i].alive) continue;
                if (node.state.snakes[i].owner == myOwner) cMyIdx[cNMy++] = i;
                else cOppIdx[cNOpp++] = i;
            }
            if (cNMy == 0) continue;

            vector<vector<int>> cCombos = {{}};
            for (int mi = 0; mi < cNMy; mi++) {
                int mv[4]; int nm = validMovesSafe(node.state, cMyIdx[mi], nodeBlocked, mv);
                vector<vector<int>> next;
                for (auto& c : cCombos)
                    for (int j = 0; j < nm; j++) {
                        auto nc = c; nc.push_back(mv[j]);
                        next.push_back(nc);
                    }
                cCombos = next;
                if ((int)cCombos.size() > comboLimit) break;
            }
            if ((int)cCombos.size() > comboLimit) cCombos.resize(comboLimit);

            int cOppMv[MAX_SNAKES];
            for (int i = 0; i < cNOpp; i++)
                cOppMv[i] = greedyMove(node.state, cOppIdx[i]);

            for (auto& combo : cCombos) {
                // FIX TIMEOUT: Frequent micro-checks
                if ((evalCount++ & 15) == 0 && elapsed(t0) > budgetMs - 5) {
                    timeout = true;
                    break;
                }

                BeamNode child;
                child.state = node.state;
                memcpy(child.firstMoves, node.firstMoves, sizeof(node.firstMoves));

                int allMv[MAX_SNAKES] = {};
                for (int i = 0; i < child.state.nSnakes; i++) allMv[i] = -1;
                for (int i = 0; i < cNMy && i < (int)combo.size(); i++)
                    allMv[cMyIdx[i]] = combo[i];
                for (int i = 0; i < cNOpp; i++)
                    allMv[cOppIdx[i]] = cOppMv[i];

                simulate(child.state, allMv);
                
                child.score = evaluate(child.state, myOwner, true, &initSt, stallUrgency, &hist);
                beamB.push_back(child);
            }
        }

        if (timeout || beamB.empty()) break; 

        sort(beamB.begin(), beamB.end(), [](const BeamNode& a, const BeamNode& b) {
            return a.score > b.score;
        });
        if ((int)beamB.size() > beamWidth) beamB.resize(beamWidth);

        beamA.swap(beamB);
        reachedDepth = depth + 1;
    }

    if (beamA.empty()) {
        vector<int> r;
        for (int i = 0; i < nMy; i++) r.push_back(greedyMove(initSt, myIdx[i]));
        return r;
    }

    if (smallMap && beamA.size() > 1) {
        for (auto& node : beamA) {
            bool trapped = false;
            for (int i = 0; i < initSt.nSnakes; i++) {
                if (!node.state.snakes[i].alive || node.state.snakes[i].owner != myOwner) continue;
                Coord h = node.state.snakes[i].head();
                if (!h.inBounds()) { trapped = true; break; }
                BitBoard nb = node.state.walls | node.state.bodyBoardConst();
                int reach = floodFillCount(h, node.state.walls, nb);
                if (reach < node.state.snakes[i].len()) { trapped = true; break; }
            }
            if (trapped) node.score -= 5000.0;
        }
        sort(beamA.begin(), beamA.end(), [](const BeamNode& a, const BeamNode& b) {
            return a.score > b.score;
        });
    }

    vector<int> result;
    for (int i = 0; i < nMy; i++)
        result.push_back(beamA[0].firstMoves[myIdx[i]]);
    return result;
}

// =============================================================
// I/O PARSING
// =============================================================
vector<Coord> parseBody(const string& s) {
    vector<Coord> r;
    string c = s;
    for (char& ch : c) if (ch == ':' || ch == ';') ch = ' ';
    istringstream ss(c);
    string t;
    while (ss >> t) {
        auto cm = t.find(',');
        if (cm == string::npos) continue;
        r.push_back({stoi(t.substr(0, cm)), stoi(t.substr(cm + 1))});
    }
    return r;
}

// =============================================================
// DEBUG: MAP DISPLAY
// =============================================================
void printMap(const State& st, ostream& os = cerr) {
    char gridPrint[MAX_H][MAX_W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (st.walls.tstC({x, y})) gridPrint[y][x] = '#';
            else if (st.apples.tstC({x, y})) gridPrint[y][x] = '*';
            else gridPrint[y][x] = '.';
        }
    for (int i = st.nSnakes - 1; i >= 0; i--) {
        const Snake& sn = st.snakes[i];
        if (!sn.alive) continue;
        char bodyChar = (sn.owner == 0) ? ('a' + (sn.id % 8)) : ('A' + (sn.id % 8));
        char headChar = (sn.id >= 0 && sn.id <= 9) ? ('0' + sn.id) : '?';
        for (int k = (int)sn.body.size() - 1; k >= 0; k--) {
            Coord c = sn.body[k];
            if (c.x >= 0 && c.x < W && c.y >= 0 && c.y < H)
                gridPrint[c.y][c.x] = (k == 0) ? headChar : bodyChar;
        }
    }
    os << "+" << string(W, '-') << "+" << endl;
    for (int y = 0; y < H; y++) {
        os << "|";
        for (int x = 0; x < W; x++) os << gridPrint[y][x];
        os << "|" << endl;
    }
    os << "+" << string(W, '-') << "+" << endl;
    os << "Mine:";
    for (int i = 0; i < st.nSnakes; i++)
        if (st.snakes[i].alive && st.snakes[i].owner == 0)
            os << " " << st.snakes[i].id << "(" << st.snakes[i].len() << ")";
    os << " | Opp:";
    for (int i = 0; i < st.nSnakes; i++)
        if (st.snakes[i].alive && st.snakes[i].owner == 1)
            os << " " << st.snakes[i].id << "(" << st.snakes[i].len() << ")";
    os << endl;
}

// =============================================================
// MAIN
// =============================================================
int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int myId, spp;
    cin >> myId; cin.ignore();
    cin >> W; cin.ignore();
    cin >> H; cin.ignore();

    totalCells = W * H;
    smallMap = (W <= 15 && H <= 15) || totalCells <= 200;
    tinyMap = (W <= 10 && H <= 10) || totalCells <= 100;

    BitBoard walls;
    for (int y = 0; y < H; y++) {
        string row; getline(cin, row);
        for (int x = 0; x < (int)row.size() && x < W; x++)
            if (row[x] == '#') walls.setC({x, y});
    }

    cin >> spp; cin.ignore();
    vector<int> myIds(spp), oppIds(spp);
    for (int i = 0; i < spp; i++) { cin >> myIds[i]; cin.ignore(); }
    for (int i = 0; i < spp; i++) { cin >> oppIds[i]; cin.ignore(); }

    map<int, int> id2idx, id2owner;
    int idx = 0;
    for (int id : myIds)  { id2idx[id] = idx; id2owner[id] = 0; idx++; }
    for (int id : oppIds) { id2idx[id] = idx; id2owner[id] = 1; idx++; }
    int totalSnakes = idx;

    int turn = 0;
    int previousMyScore = -1;
    int stallCounter = 0;
    
    map<int, deque<Coord>> hist;

    if (smallMap) cerr << "SMALL MAP MODE (" << W << "x" << H << ")" << endl;
    if (tinyMap)  cerr << "TINY MAP MODE (" << W << "x" << H << ")" << endl;

    while (true) {
        auto t0 = Clock::now();
        turn++;

        int nrg; cin >> nrg; cin.ignore();
        BitBoard apples;
        for (int i = 0; i < nrg; i++) {
            int x, y; cin >> x >> y; cin.ignore();
            apples.setC({x, y});
        }

        int nb; cin >> nb; cin.ignore();
        State st;
        st.walls = walls;
        st.apples = apples;
        st.nSnakes = totalSnakes;
        st.turn = turn;
        st.bodyDirty = true; 

        set<int> aliveIds;
        for (int i = 0; i < totalSnakes; i++) {
            st.snakes[i].alive = false;
            st.snakes[i].id = -1;
            st.snakes[i].owner = -1;
        }

        for (int i = 0; i < nb; i++) {
            int sid; string bodyStr;
            cin >> sid >> bodyStr; cin.ignore();
            if (id2idx.count(sid)) {
                int si = id2idx[sid];
                st.snakes[si].id = sid;
                st.snakes[si].owner = id2owner[sid];
                auto body = parseBody(bodyStr);
                if (body.empty()) continue;
                st.snakes[si].alive = true;
                st.snakes[si].body = deque<Coord>(body.begin(), body.end());
                aliveIds.insert(sid);
            }
        }

        // --- TRACKING DE L'HISTORIQUE ---
        for (int i = 0; i < totalSnakes; i++) {
            if (st.snakes[i].alive && st.snakes[i].owner == 0) {
                hist[st.snakes[i].id].push_back(st.snakes[i].head());
                if (hist[st.snakes[i].id].size() > 6) hist[st.snakes[i].id].pop_front();
            } else {
                hist.erase(st.snakes[i].id);
            }
        }

        // --- STALL DETECTOR ---
        int currentMyScore = st.scoreFor(0);
        if (currentMyScore == previousMyScore && turn > 1) {
            stallCounter++;
        } else {
            stallCounter = 0;
            previousMyScore = currentMyScore;
        }

        double stallUrgency = 1.0;
        if (stallCounter >= 20) stallUrgency = 2.0;
        else if (stallCounter >= 10) stallUrgency = 1.6;
        else if (stallCounter >= 5)  stallUrgency = 1.3;

        int64_t budget = (turn == 1) ? 900 : 45;

        cerr << "\n=== T" << turn << " " << W << "x" << H;
        int myS = st.scoreFor(0), opS = st.scoreFor(1);
        int delta = myS - opS;
        cerr << " " << myS << "v" << opS << " (";
        if (delta > 0) cerr << "+"; 
        cerr << delta << ")";
        cerr << " E=" << nrg;
        int turnsLeft = 200 - turn;
        string phase = (turnsLeft > 140) ? "EXPAND" : ((turnsLeft > 60) ? "CONTROL" : "CONSERVE");
        if (delta < -2 && turnsLeft <= 80) phase = "AGGRESS";
        cerr << " " << phase << " | STALL=" << stallCounter << " (x" << stallUrgency << ") ===" << endl;
        
        printMap(st);

        vector<int> bestMoves = beamSearch(st, 0, budget, stallUrgency, hist);

        string out;
        int mi = 0;
        for (int i = 0; i < spp; i++) {
            int sid = myIds[i];
            if (!aliveIds.count(sid)) continue;
            int dir = (mi < (int)bestMoves.size()) ? bestMoves[mi] : 0;
            mi++;
            if (dir < 0 || dir > 3) dir = 0;
            if (!out.empty()) out += ";";
            out += to_string(sid) + " " + DIR_NAMES[dir];
        }

        cerr << "  => " << out << " (" << elapsed(t0) << "ms)" << endl;

        cout << (out.empty() ? "WAIT" : out) << endl;
    }
}
