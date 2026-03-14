#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <queue>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <climits>
#include <cstring>

using namespace std;

// ============================================================
// Genome Parameters - tunable via config file or embedded defaults
// ============================================================
struct Params {
    double apple_weight         = 3.0;
    double apple_dist_decay     = 0.15;
    double cluster_bonus        = 0.4;
    double safety_weight        = 5.0;
    double dead_end_penalty     = 8.0;
    double fall_penalty         = 2.0;
    double wall_penalty         = 1.0;
    double space_weight         = 2.0;
    double height_weight        = 0.5;
    double center_weight        = 0.3;
    double opponent_proximity   = 3.0;
    double aggression           = 1.0;
    double survival_priority    = 4.0;
    double length_bonus         = 0.2;
    double body_block_bonus     = 0.5;
};

static Params PARAMS;

// Load parameters from JSON-like config file
bool loadParams(const string& filename) {
    ifstream f(filename);
    if (!f.is_open()) return false;

    string line;
    while (getline(f, line)) {
        // Simple key: value parser (handles JSON-like format)
        size_t colon = line.find(':');
        if (colon == string::npos) continue;

        string key = line.substr(0, colon);
        string val = line.substr(colon + 1);

        // Strip whitespace, quotes, commas, braces
        auto strip = [](string& s) {
            string result;
            for (char c : s) {
                if (c != ' ' && c != '"' && c != ',' && c != '{' && c != '}' &&
                    c != '\t' && c != '\n' && c != '\r')
                    result += c;
            }
            s = result;
        };
        strip(key);
        strip(val);

        if (key.empty() || val.empty()) continue;

        double v = 0;
        try { v = stod(val); } catch (...) { continue; }

        if (key == "apple_weight")       PARAMS.apple_weight = v;
        else if (key == "apple_dist_decay")   PARAMS.apple_dist_decay = v;
        else if (key == "cluster_bonus")      PARAMS.cluster_bonus = v;
        else if (key == "safety_weight")      PARAMS.safety_weight = v;
        else if (key == "dead_end_penalty")   PARAMS.dead_end_penalty = v;
        else if (key == "fall_penalty")       PARAMS.fall_penalty = v;
        else if (key == "wall_penalty")       PARAMS.wall_penalty = v;
        else if (key == "space_weight")       PARAMS.space_weight = v;
        else if (key == "height_weight")      PARAMS.height_weight = v;
        else if (key == "center_weight")      PARAMS.center_weight = v;
        else if (key == "opponent_proximity") PARAMS.opponent_proximity = v;
        else if (key == "aggression")         PARAMS.aggression = v;
        else if (key == "survival_priority")  PARAMS.survival_priority = v;
        else if (key == "length_bonus")       PARAMS.length_bonus = v;
        else if (key == "body_block_bonus")   PARAMS.body_block_bonus = v;
    }
    f.close();
    return true;
}

// ============================================================
// Game Types
// ============================================================
struct Coord {
    int x, y;
    Coord() : x(0), y(0) {}
    Coord(int x, int y) : x(x), y(y) {}
    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    bool operator<(const Coord& o) const { return x < o.x || (x == o.x && y < o.y); }
    Coord operator+(const Coord& o) const { return {x + o.x, y + o.y}; }
    int manhattan(const Coord& o) const { return abs(x - o.x) + abs(y - o.y); }
};

enum Direction { UP = 0, RIGHT_DIR = 1, DOWN = 2, LEFT_DIR = 3, NONE = 4 };

static const Coord DIR_DELTA[] = {
    {0, -1},  // UP
    {1, 0},   // RIGHT
    {0, 1},   // DOWN
    {-1, 0},  // LEFT
    {0, 0}    // NONE
};

static const string DIR_NAME[] = {"UP", "RIGHT", "DOWN", "LEFT", "WAIT"};

Direction oppositeDir(Direction d) {
    switch (d) {
        case UP: return DOWN;
        case DOWN: return UP;
        case LEFT_DIR: return RIGHT_DIR;
        case RIGHT_DIR: return LEFT_DIR;
        default: return NONE;
    }
}

struct Bird {
    int id;
    vector<Coord> body;
    bool alive;
    int owner; // 0 or 1

    Coord head() const { return body[0]; }

    Direction facing() const {
        if (body.size() < 2) return NONE;
        int dx = body[0].x - body[1].x;
        int dy = body[0].y - body[1].y;
        if (dx == 0 && dy == -1) return UP;
        if (dx == 1 && dy == 0) return RIGHT_DIR;
        if (dx == 0 && dy == 1) return DOWN;
        if (dx == -1 && dy == 0) return LEFT_DIR;
        return NONE;
    }
};

struct GameState {
    int myIndex;
    int width, height;
    vector<string> grid;
    vector<int> myBirdIds;
    vector<int> oppBirdIds;
    vector<Coord> apples;
    vector<Bird> birds;
    map<int, Bird*> birdById;

    bool isWall(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return true;
        return grid[y][x] == '#';
    }

    bool isWall(Coord c) const { return isWall(c.x, c.y); }

    bool isInBounds(Coord c) const {
        return c.x >= 0 && c.x < width && c.y >= 0 && c.y < height;
    }

    bool isBodyCell(Coord c, int excludeBirdId = -1) const {
        for (const auto& bird : birds) {
            if (!bird.alive || bird.id == excludeBirdId) continue;
            for (const auto& part : bird.body) {
                if (part == c) return true;
            }
        }
        return false;
    }

    bool isOwnBody(Coord c, int birdId) const {
        for (const auto& bird : birds) {
            if (bird.id != birdId || !bird.alive) continue;
            for (size_t i = 1; i < bird.body.size(); i++) {
                if (bird.body[i] == c) return true;
            }
        }
        return false;
    }

    bool isApple(Coord c) const {
        for (const auto& a : apples) {
            if (a == c) return true;
        }
        return false;
    }

    bool hasSupport(Coord c) const {
        Coord below = {c.x, c.y + 1};
        if (below.y >= height) return false;
        if (isWall(below)) return true;
        if (isApple(below)) return true;
        if (isBodyCell(below)) return true;
        return false;
    }

    bool isCellBlocked(Coord c, int excludeBirdId = -1) const {
        if (!isInBounds(c)) return true;
        if (isWall(c)) return true;
        if (isBodyCell(c, excludeBirdId)) return true;
        return false;
    }

    int countAdjacentWalls(Coord c) const {
        int count = 0;
        for (int d = 0; d < 4; d++) {
            Coord n = c + DIR_DELTA[d];
            if (isWall(n)) count++;
        }
        return count;
    }

    // Flood fill to count reachable empty cells from a position
    int floodFill(Coord start, int maxSteps, int excludeBirdId = -1) const {
        if (isCellBlocked(start, excludeBirdId)) return 0;

        set<pair<int,int>> visited;
        queue<pair<Coord, int>> q;
        q.push({start, 0});
        visited.insert({start.x, start.y});

        int count = 0;
        while (!q.empty()) {
            auto [pos, dist] = q.front();
            q.pop();
            count++;

            if (dist >= maxSteps) continue;

            for (int d = 0; d < 4; d++) {
                Coord next = pos + DIR_DELTA[d];
                if (visited.count({next.x, next.y})) continue;
                if (isCellBlocked(next, excludeBirdId)) continue;
                visited.insert({next.x, next.y});
                q.push({next, dist + 1});
            }
        }
        return count;
    }

    bool isMyBird(int id) const {
        for (int mid : myBirdIds) {
            if (mid == id) return true;
        }
        return false;
    }

    int myTotalLength() const {
        int len = 0;
        for (const auto& b : birds) {
            if (b.alive && isMyBird(b.id)) len += b.body.size();
        }
        return len;
    }

    int oppTotalLength() const {
        int len = 0;
        for (const auto& b : birds) {
            if (b.alive && !isMyBird(b.id)) len += b.body.size();
        }
        return len;
    }
};

// ============================================================
// Move Scoring
// ============================================================
double scoreMove(const GameState& state, const Bird& bird, Direction dir) {
    if (dir == NONE) return -1e18;

    // Can't move backwards
    Direction currentFacing = bird.facing();
    if (currentFacing != NONE && dir == oppositeDir(currentFacing)) return -1e18;

    Coord newHead = bird.head() + DIR_DELTA[dir];

    // Immediate death: wall or out of bounds
    if (!state.isInBounds(newHead) || state.isWall(newHead)) return -1e18;

    // Self-collision (exclude head position since it will move)
    if (state.isOwnBody(newHead, bird.id)) return -1e18;

    // Collision with other birds' bodies
    if (state.isBodyCell(newHead, bird.id)) return -1e18;

    double score = 0.0;

    // --- Apple pursuit ---
    if (!state.apples.empty()) {
        double minDist = 1e9;
        for (const auto& apple : state.apples) {
            double d = newHead.manhattan(apple);
            minDist = min(minDist, d);
        }
        score += PARAMS.apple_weight / (1.0 + minDist * PARAMS.apple_dist_decay);

        // Bonus for eating an apple directly
        if (state.isApple(newHead)) {
            score += PARAMS.apple_weight * 2.0;
        }

        // Apple cluster bonus
        int nearbyApples = 0;
        for (const auto& apple : state.apples) {
            if (newHead.manhattan(apple) <= 5) nearbyApples++;
        }
        score += PARAMS.cluster_bonus * nearbyApples;
    }

    // --- Space evaluation (flood fill) ---
    int reachable = state.floodFill(newHead, 12, bird.id);
    score += PARAMS.space_weight * reachable / 50.0;

    // Dead end penalty
    if (reachable < 4) {
        score -= PARAMS.dead_end_penalty;
    } else if (reachable < 8) {
        score -= PARAMS.dead_end_penalty * 0.5;
    }

    // --- Fall risk ---
    if (!state.hasSupport(newHead) && !state.isWall(Coord{newHead.x, newHead.y + 1})) {
        // Check how far we'd fall
        int fallDist = 0;
        Coord fallPos = newHead;
        while (fallPos.y < state.height && !state.isWall(Coord{fallPos.x, fallPos.y + 1}) &&
               !state.isApple(Coord{fallPos.x, fallPos.y + 1})) {
            fallDist++;
            fallPos.y++;
            if (fallPos.y >= state.height) {
                // Would fall off the map!
                score -= PARAMS.fall_penalty * 10.0;
                break;
            }
        }
        score -= PARAMS.fall_penalty * (1.0 + fallDist * 0.5);
    }

    // --- Height preference (higher = safer from gravity) ---
    score += PARAMS.height_weight * (state.height - newHead.y) / (double)state.height;

    // --- Center preference ---
    double centerX = state.width / 2.0;
    double distFromCenter = abs(newHead.x - centerX) / centerX;
    score += PARAMS.center_weight * (1.0 - distFromCenter);

    // --- Wall proximity penalty ---
    int adjWalls = state.countAdjacentWalls(newHead);
    score -= PARAMS.wall_penalty * adjWalls * 0.5;

    // --- Opponent proximity ---
    for (const auto& obird : state.birds) {
        if (!obird.alive || state.isMyBird(obird.id)) continue;
        double d = newHead.manhattan(obird.head());
        if (d <= 2) {
            score -= PARAMS.opponent_proximity / (1.0 + d);
        }
        // Aggression: bonus for being near opponents when we're longer
        if (PARAMS.aggression > 0 && d <= 3) {
            int myLen = bird.body.size();
            int oppLen = obird.body.size();
            if (myLen > oppLen + 1) {
                score += PARAMS.aggression * (myLen - oppLen) / (1.0 + d);
            }
        }
    }

    // --- Length advantage bonus ---
    int myTotal = state.myTotalLength();
    int oppTotal = state.oppTotalLength();
    if (myTotal > oppTotal) {
        score += PARAMS.length_bonus * (myTotal - oppTotal);
    }

    // --- Survival priority: strongly prefer moves that keep options open ---
    if (reachable > 15) {
        score += PARAMS.survival_priority * 0.5;
    }

    return score;
}

// ============================================================
// I/O Parsing
// ============================================================
void parseGlobalInfo(GameState& state) {
    cin >> state.myIndex;
    cin >> state.width >> state.height;

    state.grid.resize(state.height);
    for (int y = 0; y < state.height; y++) {
        cin >> state.grid[y];
    }

    int birdsPerPlayer;
    cin >> birdsPerPlayer;

    state.myBirdIds.resize(birdsPerPlayer);
    for (int i = 0; i < birdsPerPlayer; i++) {
        cin >> state.myBirdIds[i];
    }

    state.oppBirdIds.resize(birdsPerPlayer);
    for (int i = 0; i < birdsPerPlayer; i++) {
        cin >> state.oppBirdIds[i];
    }
}

void parseFrameInfo(GameState& state) {
    int appleCount;
    cin >> appleCount;

    state.apples.clear();
    for (int i = 0; i < appleCount; i++) {
        int x, y;
        cin >> x >> y;
        state.apples.push_back({x, y});
    }

    int birdCount;
    cin >> birdCount;

    state.birds.clear();
    state.birdById.clear();

    for (int i = 0; i < birdCount; i++) {
        Bird bird;
        bird.alive = true;

        string bodyStr;
        cin >> bird.id >> bodyStr;

        // Parse body: "x1,y1:x2,y2:x3,y3"
        stringstream ss(bodyStr);
        string segment;
        while (getline(ss, segment, ':')) {
            size_t comma = segment.find(',');
            int bx = stoi(segment.substr(0, comma));
            int by = stoi(segment.substr(comma + 1));
            bird.body.push_back({bx, by});
        }

        bird.owner = state.isMyBird(bird.id) ? state.myIndex : (1 - state.myIndex);
        state.birds.push_back(bird);
    }

    for (auto& bird : state.birds) {
        state.birdById[bird.id] = &bird;
    }
}

// ============================================================
// Main
// ============================================================
int main(int argc, char* argv[]) {
    // Check for config file argument
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--config" && i + 1 < argc) {
            loadParams(argv[i + 1]);
            break;
        }
    }

    GameState state;
    parseGlobalInfo(state);

    // Game loop
    while (true) {
        parseFrameInfo(state);

        string output;
        bool first = true;

        for (int birdId : state.myBirdIds) {
            auto it = state.birdById.find(birdId);
            if (it == state.birdById.end()) continue;

            Bird& bird = *(it->second);
            if (!bird.alive) continue;

            double bestScore = -1e18;
            Direction bestDir = NONE;

            for (int d = 0; d < 4; d++) {
                Direction dir = static_cast<Direction>(d);
                double s = scoreMove(state, bird, dir);
                if (s > bestScore) {
                    bestScore = s;
                    bestDir = dir;
                }
            }

            if (bestDir != NONE) {
                if (!first) output += ";";
                output += to_string(birdId) + " " + DIR_NAME[bestDir];
                first = false;
            }
        }

        if (output.empty()) {
            output = "WAIT";
        }

        cout << output << endl;
    }

    return 0;
}
