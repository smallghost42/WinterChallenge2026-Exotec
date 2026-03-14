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
#include <numeric>
using namespace std;

using Clock = chrono::steady_clock;
using Ms    = chrono::milliseconds;
inline int64_t elapsed(Clock::time_point t0) {
    return chrono::duration_cast<Ms>(Clock::now() - t0).count();
}

// ===================== GRID =====================
constexpr int MAX_W = 50, MAX_H = 30, MAX_CELLS = MAX_W * MAX_H;
int W, H;

struct Coord {
    int x, y;
    constexpr Coord() : x(-1), y(-1) {}
    constexpr Coord(int x, int y) : x(x), y(y) {}
    int  idx()       const { return y * MAX_W + x; }
    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    Coord operator+(const Coord& o) const { return {x + o.x, y + o.y}; }
    Coord operator-(const Coord& o) const { return {x - o.x, y - o.y}; }
    int  manhattan(const Coord& o) const { return abs(x - o.x) + abs(y - o.y); }
    bool inBounds() const { return x >= 0 && x < W && y >= 0 && y < H; }
};

constexpr Coord DIRS[4] = {{0,-1},{0,1},{-1,0},{1,0}};
const string DIR_NAMES[4] = {"UP","DOWN","LEFT","RIGHT"};
constexpr int DIR_UP=0, DIR_DOWN=1, DIR_LEFT=2, DIR_RIGHT=3;
inline int reverseDir(int d) { return d ^ 1; }

// ===================== BITBOARD =====================
constexpr int BW = (MAX_CELLS + 63) / 64;
struct BitBoard {
    uint64_t w[BW] = {};
    void set(int i)       { w[i>>6] |=  1ULL<<(i&63); }
    void clr(int i)       { w[i>>6] &= ~(1ULL<<(i&63)); }
    bool tst(int i) const { return (w[i>>6]>>(i&63))&1; }
    void setC(Coord c)       { set(c.idx()); }
    void clrC(Coord c)       { clr(c.idx()); }
    bool tstC(Coord c) const { return tst(c.idx()); }
    BitBoard operator|(const BitBoard& o) const {
        BitBoard r; for(int i=0;i<BW;i++) r.w[i]=w[i]|o.w[i]; return r;
    }
    BitBoard operator&(const BitBoard& o) const {
        BitBoard r; for(int i=0;i<BW;i++) r.w[i]=w[i]&o.w[i]; return r;
    }
    int popcount() const {
        int c=0; for(int i=0;i<BW;i++) c+=__builtin_popcountll(w[i]); return c;
    }
    void reset() { memset(w,0,sizeof(w)); }
    template<typename F> void forEach(F&& fn) const {
        for(int i=0;i<BW;i++){uint64_t v=w[i];while(v){int bit=__builtin_ctzll(v);
        int idx=(i<<6)|bit;fn(Coord(idx%MAX_W,idx/MAX_W));v&=v-1;}}
    }
};

// ===================== GAME STATE =====================
constexpr int MAX_SNAKES = 8;

struct Snake {
    int id=-1, owner=-1;
    bool alive=false;
    deque<Coord> body;
    int lastDir=DIR_UP;

    Coord head() const { return body.front(); }
    int   len()  const { return (int)body.size(); }

    int facing() const {
        if(body.size()<2) return DIR_UP;
        Coord d=body[0]-body[1];
        for(int i=0;i<4;i++) if(DIRS[i].x==d.x&&DIRS[i].y==d.y) return i;
        return DIR_UP;
    }
    int forbidden() const {
        if(body.size()<2) return -1;
        Coord d=body[1]-body[0];
        for(int i=0;i<4;i++) if(DIRS[i].x==d.x&&DIRS[i].y==d.y) return i;
        return -1;
    }
};

struct State {
    BitBoard walls, apples;
    Snake snakes[MAX_SNAKES];
    int nSnakes=0, turn=0, losses[2]={0,0};

    BitBoard bodyBoard() const {
        BitBoard b;
        for(int i=0;i<nSnakes;i++)
            if(snakes[i].alive)
                for(auto&c:snakes[i].body) if(c.inBounds()) b.setC(c);
        return b;
    }
    int scoreFor(int o) const {
        int s=0; for(int i=0;i<nSnakes;i++)
            if(snakes[i].alive&&snakes[i].owner==o) s+=snakes[i].len();
        return s;
    }
    int aliveCount(int o) const {
        int c=0; for(int i=0;i<nSnakes;i++)
            if(snakes[i].alive&&snakes[i].owner==o) c++;
        return c;
    }
};

// ===================== SIMULATION =====================
void simulate(State& st, const int moves[]) {
    // 1. MOVES
    for(int i=0;i<st.nSnakes;i++){
        Snake&sn=st.snakes[i]; if(!sn.alive) continue;
        int dir=moves[i];
        if(dir<0||dir>3) dir=sn.facing();
        if(dir==sn.forbidden()) dir=sn.facing();
        sn.lastDir=dir;
        Coord nh=sn.head()+DIRS[dir];
        bool eat=nh.inBounds()&&st.apples.tstC(nh);
        if(!eat) sn.body.pop_back();
        sn.body.push_front(nh);
    }
    // 2. EATS
    for(int i=0;i<st.nSnakes;i++){
        Snake&sn=st.snakes[i]; if(!sn.alive) continue;
        if(sn.head().inBounds()&&st.apples.tstC(sn.head()))
            st.apples.clrC(sn.head());
    }
    // 3. BEHEADINGS
    bool behead[MAX_SNAKES]={};
    for(int i=0;i<st.nSnakes;i++){
        Snake&sn=st.snakes[i]; if(!sn.alive) continue;
        Coord h=sn.head(); if(!h.inBounds()) continue;
        if(st.walls.tstC(h)){behead[i]=true;continue;}
        for(int j=0;j<st.nSnakes&&!behead[i];j++){
            if(!st.snakes[j].alive) continue;
            int s=(j==i)?1:0;
            for(int k=s;k<st.snakes[j].len();k++)
                if(st.snakes[j].body[k]==h){behead[i]=true;break;}
        }
    }
    // Head-on collisions
    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive) continue;
        Coord hi=st.snakes[i].head(); if(!hi.inBounds()) continue;
        for(int j=i+1;j<st.nSnakes;j++){
            if(!st.snakes[j].alive) continue;
            if(st.snakes[j].head()==hi) behead[i]=behead[j]=true;
        }
    }
    for(int i=0;i<st.nSnakes;i++){
        if(!behead[i]) continue;
        Snake&sn=st.snakes[i];
        if(sn.len()<=3){st.losses[sn.owner]+=sn.len();sn.alive=false;}
        else{st.losses[sn.owner]++;sn.body.pop_front();}
    }
    // 4. FALLS
    {
        bool fell=true;
        while(fell){
            fell=false;
            bool air[MAX_SNAKES]={},gnd[MAX_SNAKES]={};
            for(int i=0;i<st.nSnakes;i++) if(st.snakes[i].alive) air[i]=true;
            bool got=true;
            while(got){
                got=false;
                for(int i=0;i<st.nSnakes;i++){
                    if(!air[i]) continue;
                    bool g=false;
                    for(auto&c:st.snakes[i].body){
                        if(!c.inBounds()) continue;
                        Coord bl(c.x,c.y+1);
                        if(bl.y>=H){g=true;break;}
                        if(st.walls.tstC(bl)||st.apples.tstC(bl)){g=true;break;}
                        for(int gi=0;gi<st.nSnakes;gi++){
                            if(!gnd[gi]) continue;
                            for(auto&gp:st.snakes[gi].body)
                                if(gp==bl){g=true;break;}
                            if(g) break;
                        }
                        if(g) break;
                    }
                    if(g){gnd[i]=true;air[i]=false;got=true;}
                }
            }
            for(int i=0;i<st.nSnakes;i++){
                if(!air[i]) continue;
                fell=true;
                for(auto&c:st.snakes[i].body) c.y++;
                bool allOut=true;
                for(auto&c:st.snakes[i].body) if(c.y<H){allOut=false;break;}
                if(allOut){st.losses[st.snakes[i].owner]+=st.snakes[i].len();st.snakes[i].alive=false;}
            }
        }
    }
    st.turn++;
}

// ===================== ESCAPE SPACE (flood fill) =====================
int escapeSpace(Coord start, const BitBoard& blocked, int maxCount) {
    if(!start.inBounds()||blocked.tstC(start)) return 0;
    static int vis[MAX_CELLS]; static int gen=0; gen++;
    static Coord q[MAX_CELLS];
    int h=0,t=0; q[t++]=start; vis[start.idx()]=gen; int count=0;
    while(h<t&&count<maxCount){
        Coord c=q[h++]; count++;
        for(int d=0;d<4;d++){
            Coord nc=c+DIRS[d];
            if(!nc.inBounds()||blocked.tstC(nc)||vis[nc.idx()]==gen) continue;
            vis[nc.idx()]=gen; q[t++]=nc;
        }
    }
    return count;
}

// ===================== BEHEADING STREAK DETECTOR =====================
struct BeheadingTracker {
    int prevLen[MAX_SNAKES];
    int streak[MAX_SNAKES];   // consecutive turns of losing length
    int stuckCnt[MAX_SNAKES]; // consecutive turns head didn't move
    Coord lastHead[MAX_SNAKES];

    void init() {
        memset(prevLen, 0, sizeof(prevLen));
        memset(streak, 0, sizeof(streak));
        memset(stuckCnt, 0, sizeof(stuckCnt));
        for(int i=0;i<MAX_SNAKES;i++) lastHead[i]={-1,-1};
    }

    void update(const State& st) {
        for(int i=0;i<st.nSnakes;i++){
            if(!st.snakes[i].alive){streak[i]=0;stuckCnt[i]=0;continue;}
            int curLen=st.snakes[i].len();
            // Beheading streak
            if(prevLen[i]>0 && curLen < prevLen[i])
                streak[i]++;
            else if(curLen >= prevLen[i])
                streak[i]=0;
            prevLen[i]=curLen;
            // Stuck detection
            Coord h=st.snakes[i].head();
            if(h==lastHead[i]) stuckCnt[i]++;
            else stuckCnt[i]=0;
            lastHead[i]=h;
        }
    }

    bool isBeheading(int si) const { return streak[si]>=2; }
    bool isStuck(int si)     const { return stuckCnt[si]>=3; }
} tracker;

// ===================== DANGER MAP =====================
struct DangerMap {
    int maxOppLen[MAX_H][MAX_W];
    void compute(const State& st, int myOwner) {
        memset(maxOppLen,0,sizeof(maxOppLen));
        for(int si=0;si<st.nSnakes;si++){
            if(!st.snakes[si].alive||st.snakes[si].owner==myOwner) continue;
            Coord h=st.snakes[si].head(); if(!h.inBounds()) continue;
            int forb=st.snakes[si].forbidden(), slen=st.snakes[si].len();
            for(int d=0;d<4;d++){
                if(d==forb) continue;
                Coord n1=h+DIRS[d];
                if(!n1.inBounds()||st.walls.tstC(n1)) continue;
                maxOppLen[n1.y][n1.x]=max(maxOppLen[n1.y][n1.x],slen);
                for(int d2=0;d2<4;d2++){
                    if(d2==reverseDir(d)) continue;
                    Coord n2=n1+DIRS[d2];
                    if(!n2.inBounds()||st.walls.tstC(n2)) continue;
                    maxOppLen[n2.y][n2.x]=max(maxOppLen[n2.y][n2.x],slen);
                }
            }
        }
    }
    bool isDangerousFor(Coord c, int myLen) const {
        return c.inBounds()&&maxOppLen[c.y][c.x]>=myLen;
    }
};

// ===================== MOVE SAFETY =====================
int moveSafety(const State& st, int si, int dir, const DangerMap& danger) {
    const Snake&sn=st.snakes[si];
    Coord nh=sn.head()+DIRS[dir];
    if(!nh.inBounds()) return -1;
    if(st.walls.tstC(nh)) return -1;

    // Self-collision
    for(int k=1;k<sn.len();k++){
        if(sn.body[k]==nh){
            if(k==sn.len()-1 && !st.apples.tstC(nh)) continue;
            return -1;
        }
    }
    // Other snake body collision
    for(int j=0;j<st.nSnakes;j++){
        if(j==si||!st.snakes[j].alive) continue;
        for(int k=0;k<st.snakes[j].len();k++){
            if(st.snakes[j].body[k]==nh){
                if(k==st.snakes[j].len()-1) return 0;
                return -1;
            }
        }
    }
    // Head-on collision risk
    for(int j=0;j<st.nSnakes;j++){
        if(!st.snakes[j].alive||j==si) continue;
        Coord oh=st.snakes[j].head(); if(!oh.inBounds()) continue;
        int of=st.snakes[j].forbidden();
        for(int d=0;d<4;d++){
            if(d==of) continue;
            if(oh+DIRS[d]==nh){
                if(st.snakes[j].owner!=sn.owner && st.snakes[j].len()>=sn.len()) return 0;
                if(st.snakes[j].owner==sn.owner) return 0;
            }
        }
    }
    // Gravity support check
    BitBoard solid=st.walls|st.apples|st.bodyBoard();
    Coord bl(nh.x,nh.y+1);
    bool sup=(bl.y>=H)||solid.tstC(bl);
    if(!sup){
        int fy=nh.y;
        while(fy+1<H){Coord fb(nh.x,fy+1);if(st.walls.tstC(fb)||solid.tstC(fb))break;fy++;}
        if(fy+1>=H) return -1;
        return 0;
    }
    // Opponent danger
    if(danger.isDangerousFor(nh,sn.len())) return 0;

    // Escape space check
    {
        BitBoard postBlocked=st.walls|st.bodyBoard();
        postBlocked.setC(nh);
        int space=escapeSpace(nh,postBlocked,sn.len()*3);
        if(space < sn.len()) return 0;
    }
    return 1;
}

int validMovesCount(const State& st, int si, int out[]) {
    const Snake&sn=st.snakes[si]; if(!sn.alive) return 0;
    int f=sn.forbidden(),n=0;
    for(int d=0;d<4;d++){
        if(d==f) continue;
        Coord nh=sn.head()+DIRS[d];
        if(!nh.inBounds()||st.walls.tstC(nh)) continue;
        out[n++]=d;
    }
    if(n==0){for(int d=0;d<4;d++) out[n++]=d;}
    return n;
}

int validMovesSafe(const State& st, int si, const DangerMap& danger, int out[]) {
    const Snake&sn=st.snakes[si]; if(!sn.alive) return 0;
    int safe[4],nS=0,risky[4],nR=0,desp[4],nD=0,f=sn.forbidden();
    for(int d=0;d<4;d++){
        if(d==f) continue;
        int s=moveSafety(st,si,d,danger);
        if(s==1) safe[nS++]=d;
        else if(s==0) risky[nR++]=d;
        else desp[nD++]=d;
    }
    if(nS>0){memcpy(out,safe,nS*sizeof(int));return nS;}
    if(nR>0){memcpy(out,risky,nR*sizeof(int));return nR;}
    if(nD>0){memcpy(out,desp,nD*sizeof(int));return nD;}
    int n=0; for(int d=0;d<4;d++) out[n++]=d; return n;
}

// ===================== VORONOI =====================
struct VoronoiResult {
    int territory[2]={0,0};
    int energyControl[2]={0,0};
    int reachable[MAX_SNAKES]={};
    int closestEnergy[MAX_SNAKES];
};

VoronoiResult computeVoronoi(const State& st) {
    VoronoiResult vr;
    fill(vr.closestEnergy, vr.closestEnergy+MAX_SNAKES, INT_MAX);
    int dist[MAX_H][MAX_W]; int cellSnake[MAX_H][MAX_W];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){dist[y][x]=INT_MAX;cellSnake[y][x]=-1;}

    struct Node{int cost;short x,y;int si;};
    auto cmp=[](const Node&a,const Node&b){return a.cost>b.cost;};
    priority_queue<Node,vector<Node>,decltype(cmp)> pq(cmp);
    BitBoard blocked=st.walls|st.bodyBoard();

    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive) continue;
        Coord h=st.snakes[i].head();
        if(h.inBounds()) pq.push({0,(short)h.x,(short)h.y,i});
    }
    while(!pq.empty()){
        auto nd=pq.top();pq.pop();
        int cx=nd.x,cy=nd.y,cost=nd.cost,si=nd.si;
        if(cost>=dist[cy][cx]) continue;
        dist[cy][cx]=cost; cellSnake[cy][cx]=si;
        int owner=st.snakes[si].owner;
        vr.territory[owner]++; vr.reachable[si]++;
        if(st.apples.tstC({cx,cy})){
            vr.energyControl[owner]++;
            vr.closestEnergy[si]=min(vr.closestEnergy[si],cost);
        }
        if(cost>=40) continue;
        for(int d=0;d<4;d++){
            int nx=cx+DIRS[d].x, ny=cy+DIRS[d].y;
            if(nx<0||nx>=W||ny<0||ny>=H) continue;
            if(blocked.tstC({nx,ny})) continue;
            int nc=cost+1;
            // Gravity: if moving sideways/down, check fall
            if(d!=DIR_UP){
                int fy=ny;
                while(fy+1<H){
                    Coord below(nx,fy+1);
                    if(st.walls.tstC(below)||st.apples.tstC(below)||blocked.tstC(below)) break;
                    fy++; nc++;
                }
                if(fy+1>=H) continue; // death fall
                ny=fy;
            }
            if(nc<dist[ny][nx]) pq.push({nc,(short)nx,(short)ny,si});
        }
    }
    return vr;
}

// ===================== EVALUATION =====================
double evaluate(const State& st, int myOwner, bool fast) {
    int opp=1-myOwner;
    int myA=st.aliveCount(myOwner), oppA=st.aliveCount(opp);
    if(myA==0&&oppA==0) return 0;
    if(myA==0) return -1e6+st.turn;
    if(oppA==0) return 1e6-st.turn;

    int myLen=st.scoreFor(myOwner), oppLen=st.scoreFor(opp);
    double score=0;

    // === PRIMARY: Length differential ===
    score += (myLen - oppLen) * 150.0;
    score += myLen * 15.0;
    score += (myA - oppA) * 80.0;
    score -= st.losses[myOwner] * 250.0;
    score += st.losses[opp] * 250.0;

    // === FRAGILITY ===
    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive) continue;
        double sign=(st.snakes[i].owner==myOwner)?-1.0:1.0;
        if(st.snakes[i].len()<=3) score+=sign*400.0;
        else if(st.snakes[i].len()<=4) score+=sign*120.0;
    }

    BitBoard blocked=st.walls|st.bodyBoard();

    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive||st.snakes[i].owner!=myOwner) continue;
        const Snake&sn=st.snakes[i];
        Coord h=sn.head();
        if(!h.inBounds()){score-=600;continue;}

        // === GRAVITY RISK ===
        bool sup=false;
        for(auto&c:sn.body){
            if(!c.inBounds()) continue;
            Coord bl(c.x,c.y+1);
            if(bl.y>=H||st.walls.tstC(bl)||st.apples.tstC(bl)||blocked.tstC(bl)){sup=true;break;}
        }
        if(!sup){
            bool die=false; int mf=0;
            for(auto&c:sn.body){
                if(!c.inBounds()) continue;
                int fy=c.y;
                while(fy+1<H){Coord b(c.x,fy+1);if(st.walls.tstC(b))break;fy++;}
                if(fy+1>=H) die=true;
                mf=max(mf,fy-c.y);
            }
            if(die) score-=600; else score-=mf*30.0;
        }

        // === AVAILABLE MOVES ===
        int sm=0, forb=sn.forbidden();
        for(int d=0;d<4;d++){
            if(d==forb) continue;
            Coord nh=h+DIRS[d];
            if(!nh.inBounds()||st.walls.tstC(nh)) continue;
            bool hit=false;
            for(int k=1;k<sn.len()-1;k++) if(sn.body[k]==nh){hit=true;break;}
            if(!hit){
                bool bh=false;
                for(int j=0;j<st.nSnakes;j++){
                    if(j==i||!st.snakes[j].alive) continue;
                    for(int k=0;k<st.snakes[j].len()-1;k++)
                        if(st.snakes[j].body[k]==nh){bh=true;break;}
                    if(bh) break;
                }
                if(!bh) sm++;
            }
        }
        if(sm==0) score-=800;      // TRAPPED — critical
        else if(sm==1) score-=200;  // only 1 exit — very dangerous

        // === ESCAPE SPACE ===
        {
            int space=escapeSpace(h,blocked,sn.len()*3);
            if(space < sn.len()) score -= 500.0;
            else if(space < sn.len()*2) score -= 150.0;
            else if(space < sn.len()*3) score -= 30.0;
        }

        // === HEAD PROXIMITY (opponent) ===
        for(int j=0;j<st.nSnakes;j++){
            if(!st.snakes[j].alive||st.snakes[j].owner==myOwner) continue;
            Coord oh=st.snakes[j].head(); if(!oh.inBounds()) continue;
            int dist=h.manhattan(oh);
            if(dist<=2 && sn.len()<=st.snakes[j].len()){
                double pen=(sn.len()<=3)?250:(sn.len()<=4)?120:60;
                score -= (3-dist)*pen;
            }
        }

        // === FRIENDLY PROXIMITY (avoid collisions) ===
        for(int j=0;j<st.nSnakes;j++){
            if(j==i||!st.snakes[j].alive||st.snakes[j].owner!=myOwner) continue;
            if(h.manhattan(st.snakes[j].head())<=1) score-=120;
        }

        // === BEHEADING STREAK PENALTY ===
        if(tracker.isBeheading(i)){
            score -= 600.0; // massive penalty — this snake is dying
            // Extra penalty proportional to streak
            score -= tracker.streak[i] * 200.0;
        }
    }

    // === APPLE PROXIMITY (manhattan for all, fast) ===
    Coord appleList[200]; int nAp=0;
    st.apples.forEach([&](Coord c){if(nAp<200) appleList[nAp++]=c;});

    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive) continue;
        Coord h=st.snakes[i].head(); if(!h.inBounds()) continue;
        int b1=INT_MAX, b2=INT_MAX;
        for(int a=0;a<nAp;a++){
            int md=h.manhattan(appleList[a]);
            if(md<b1){b2=b1;b1=md;} else if(md<b2) b2=md;
        }
        double sign=(st.snakes[i].owner==myOwner)?1.0:-0.4;
        if(b1<INT_MAX) score+=sign*100.0/(1.0+b1);
        if(b2<INT_MAX) score+=sign*25.0/(1.0+b2);
    }

    // === GRAVITY EXPLOIT: apples supporting opponents ===
    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive||st.snakes[i].owner==myOwner) continue;
        for(auto&bc:st.snakes[i].body){
            if(!bc.inBounds()) continue;
            Coord bl(bc.x,bc.y+1);
            if(!bl.inBounds()) continue;
            if(st.apples.tstC(bl)){
                int fallDist=0; Coord fp=bl;
                while(fp.y+1<H){Coord b(fp.x,fp.y+1);if(st.walls.tstC(b))break;fp.y++;fallDist++;}
                double bonus=(fp.y+1>=H)?150.0:fallDist*12.0;
                for(int mi=0;mi<st.nSnakes;mi++){
                    if(!st.snakes[mi].alive||st.snakes[mi].owner!=myOwner) continue;
                    int dist=st.snakes[mi].head().manhattan(bl);
                    if(dist<=10) score+=bonus/(1.0+dist);
                }
            }
        }
    }

    if(fast) return score;

    // === VORONOI (depth 0 only) ===
    VoronoiResult vr=computeVoronoi(st);
    score += (vr.territory[myOwner]-vr.territory[opp])*1.5;
    score += (vr.energyControl[myOwner]-vr.energyControl[opp])*25.0;

    for(int i=0;i<st.nSnakes;i++){
        if(!st.snakes[i].alive||st.snakes[i].owner!=myOwner) continue;
        if(vr.closestEnergy[i]<INT_MAX)
            score+=50.0/(1.0+vr.closestEnergy[i]);
        else score-=40.0;

        double freedom=vr.reachable[i];
        double critLen=st.snakes[i].len()*2.0;
        if(freedom<=critLen) score-=(critLen-freedom+1)*20.0;
        else score+=log(freedom+1)*2.0;
    }

    // === ENDGAME ===
    int tl=200-st.turn;
    if(tl<30 && myLen>oppLen+3) score+=myA*60;
    if(tl<15 && myLen>oppLen) score+=myA*120;

    return score;
}

// ===================== OPPONENT MODEL =====================
int opponentMove(const State& st, int si) {
    const Snake&sn=st.snakes[si]; if(!sn.alive) return sn.facing();
    int mv[4],nm=validMovesCount(st,si,mv);
    if(nm<=1) return nm==1?mv[0]:sn.facing();
    BitBoard blocked=st.walls|st.bodyBoard();
    Coord h=sn.head();
    double bs=-1e9; int bm=mv[0];
    for(int mi=0;mi<nm;mi++){
        int d=mv[mi]; Coord nh=h+DIRS[d]; double ms=0;
        if(!nh.inBounds()){ms=-500;goto dn;}
        if(st.walls.tstC(nh)){ms=-500;goto dn;}
        for(int k=1;k<sn.len()-1;k++) if(sn.body[k]==nh){ms=-400;goto dn;}
        if(blocked.tstC(nh)){
            bool tail=false;
            for(int j=0;j<st.nSnakes;j++)
                if(st.snakes[j].alive&&st.snakes[j].body.back()==nh){tail=true;break;}
            ms+=tail?-20:-300;
        }
        if(st.apples.tstC(nh)) ms+=100;
        {int bd=INT_MAX;st.apples.forEach([&](Coord ac){bd=min(bd,nh.manhattan(ac));});
         if(bd<INT_MAX) ms+=40.0/(1.0+bd);}
        {Coord bl(nh.x,nh.y+1);
         if(bl.y>=H||st.walls.tstC(bl)||blocked.tstC(bl)||st.apples.tstC(bl)) ms+=10;
         else ms-=15;}
        dn:if(ms>bs){bs=ms;bm=d;}
    }
    return bm;
}

// ===================== BEAM SEARCH =====================
struct BeamNode {
    State state;
    int firstMoves[MAX_SNAKES];
    double score;
};

vector<int> beamSearch(const State& initSt, int myOwner, int64_t budgetMs) {
    auto t0=Clock::now();
    int myIdx[MAX_SNAKES],oppIdx[MAX_SNAKES]; int nMy=0,nOpp=0;
    for(int i=0;i<initSt.nSnakes;i++){
        if(!initSt.snakes[i].alive) continue;
        if(initSt.snakes[i].owner==myOwner) myIdx[nMy++]=i;
        else oppIdx[nOpp++]=i;
    }
    if(nMy==0) return {};

    int total=nMy+nOpp;
    int beamW, beamD, comboLim;
    if(total<=3){beamW=300;beamD=8;comboLim=40;}
    else if(total<=4){beamW=250;beamD=7;comboLim=30;}
    else if(total<=6){beamW=180;beamD=6;comboLim=20;}
    else{beamW=100;beamD=5;comboLim=12;}

    DangerMap danger; danger.compute(initSt,myOwner);

    // Generate safe combos
    vector<vector<int>> combos={{}};
    for(int mi=0;mi<nMy;mi++){
        int mv[4],nm=validMovesSafe(initSt,myIdx[mi],danger,mv);
        vector<vector<int>> next; next.reserve(combos.size()*nm);
        for(auto&c:combos) for(int j=0;j<nm;j++){
            auto nc=c; nc.push_back(mv[j]); next.push_back(nc);
        }
        combos=next;
    }

    // Anti-fratricide filter
    {
        vector<vector<int>> filt; filt.reserve(combos.size());
        for(auto&combo:combos){
            bool col=false;
            Coord nh[MAX_SNAKES];
            for(int mi=0;mi<nMy;mi++)
                nh[mi]=initSt.snakes[myIdx[mi]].head()+DIRS[combo[mi]];
            // Head-head collision
            for(int a=0;a<nMy&&!col;a++)
                for(int b=a+1;b<nMy&&!col;b++)
                    if(nh[a]==nh[b]) col=true;
            // Head-body collision
            for(int a=0;a<nMy&&!col;a++)
                for(int b=0;b<nMy&&!col;b++){
                    if(a==b) continue;
                    for(int k=0;k<initSt.snakes[myIdx[b]].len()-1;k++)
                        if(initSt.snakes[myIdx[b]].body[k]==nh[a]){col=true;break;}
                }
            if(!col) filt.push_back(combo);
        }
        if(!filt.empty()) combos=filt;
    }

    // Opponent moves
    int oppMv[MAX_SNAKES];
    for(int oi=0;oi<nOpp;oi++) oppMv[oi]=opponentMove(initSt,oppIdx[oi]);

    // Build initial beam
    bool useFast0 = (int)combos.size() > beamW;
    vector<BeamNode> beam; beam.reserve(combos.size());
    for(auto&combo:combos){
        if(elapsed(t0)>budgetMs-15) break;
        BeamNode nd; nd.state=initSt;
        int am[MAX_SNAKES]={};
        for(int i=0;i<initSt.nSnakes;i++) am[i]=initSt.snakes[i].facing();
        for(int i=0;i<nMy;i++) am[myIdx[i]]=combo[i];
        for(int i=0;i<nOpp;i++) am[oppIdx[i]]=oppMv[i];
        simulate(nd.state,am);
        for(int i=0;i<nMy;i++) nd.firstMoves[myIdx[i]]=combo[i];

        double sN=evaluate(nd.state,myOwner,useFast0);

        // === CRITICAL: death/beheading penalty for first move ===
        for(int i=0;i<nMy;i++){
            int si=myIdx[i];
            if(initSt.snakes[si].alive && !nd.state.snakes[si].alive)
                sN -= 5000;
            else if(initSt.snakes[si].alive && nd.state.snakes[si].alive &&
                    nd.state.snakes[si].len() < initSt.snakes[si].len()){
                sN -= 1500; // beheaded this turn!
                // Extra penalty if already in beheading streak
                if(tracker.isBeheading(si))
                    sN -= 2000; // MUST escape the pattern
            }
        }

        // === STUCK penalty ===
        for(int i=0;i<nMy;i++){
            int si=myIdx[i];
            if(tracker.isStuck(si)){
                if(combo[i]==initSt.snakes[si].facing()) sN-=400;
                sN+=80; // bonus for any move when stuck
            }
        }

        nd.score=sN;
        beam.push_back(nd);
    }

    sort(beam.begin(),beam.end(),[](auto&a,auto&b){return a.score>b.score;});
    if((int)beam.size()>beamW) beam.resize(beamW);

    // Re-evaluate top if we used fast
    if(useFast0 && elapsed(t0)<budgetMs-30){
        int reEval=min((int)beam.size(),30);
        for(int i=0;i<reEval;i++)
            beam[i].score=evaluate(beam[i].state,myOwner,false);
        sort(beam.begin(),beam.end(),[](auto&a,auto&b){return a.score>b.score;});
    }

    int reachedD=1;
    // Beam expansion
    for(int depth=1;depth<beamD;depth++){
        if(elapsed(t0)>budgetMs-10) break;
        int dw=beamW/(1+depth/3);
        vector<BeamNode> nb; nb.reserve(dw*4);
        for(auto&nd:beam){
            if(elapsed(t0)>budgetMs-10) break;
            int cm[MAX_SNAKES],co[MAX_SNAKES]; int cnm=0,cno=0;
            for(int i=0;i<nd.state.nSnakes;i++){
                if(!nd.state.snakes[i].alive) continue;
                if(nd.state.snakes[i].owner==myOwner) cm[cnm++]=i;
                else co[cno++]=i;
            }
            if(cnm==0) continue;

            vector<vector<int>> cc={{}};
            for(int mi=0;mi<cnm;mi++){
                int mv[4]; int nm=validMovesCount(nd.state,cm[mi],mv);
                vector<vector<int>> nx;
                for(auto&c:cc) for(int j=0;j<nm;j++){
                    auto nc=c; nc.push_back(mv[j]); nx.push_back(nc);
                }
                cc=nx;
                if((int)cc.size()>comboLim) break;
            }
            if((int)cc.size()>comboLim) cc.resize(comboLim);

            int cov[MAX_SNAKES];
            for(int i=0;i<cno;i++) cov[i]=opponentMove(nd.state,co[i]);

            for(auto&combo:cc){
                BeamNode ch; ch.state=nd.state;
                memcpy(ch.firstMoves,nd.firstMoves,sizeof(nd.firstMoves));
                int am[MAX_SNAKES]={};
                for(int i=0;i<ch.state.nSnakes;i++) am[i]=ch.state.snakes[i].facing();
                for(int i=0;i<cnm&&i<(int)combo.size();i++) am[cm[i]]=combo[i];
                for(int i=0;i<cno;i++) am[co[i]]=cov[i];
                simulate(ch.state,am);
                ch.score=evaluate(ch.state,myOwner,true);
                nb.push_back(ch);
            }
        }
        if(nb.empty()) break;
        sort(nb.begin(),nb.end(),[](auto&a,auto&b){return a.score>b.score;});
        if((int)nb.size()>dw) nb.resize(dw);
        beam=nb; reachedD=depth+1;
    }

    cerr<<"Beam d="<<reachedD<<"/"<<beamD<<" w="<<beam.size()<<"/"<<beamW
        <<" my="<<nMy<<" opp="<<nOpp<<" best="<<(beam.empty()?0.0:beam[0].score)
        <<" t="<<elapsed(t0)<<"ms"<<endl;

    if(beam.empty()){
        vector<int> r;
        for(int mi=0;mi<nMy;mi++) r.push_back(initSt.snakes[myIdx[mi]].facing());
        return r;
    }
    vector<int> result;
    for(int i=0;i<nMy;i++) result.push_back(beam[0].firstMoves[myIdx[i]]);
    return result;
}

// ===================== I/O =====================
void printMap(const State&st, ostream&os=cerr){
    char grid[MAX_H][MAX_W];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        if(st.walls.tstC({x,y})) grid[y][x]='#';
        else if(st.apples.tstC({x,y})) grid[y][x]='*';
        else grid[y][x]='.';
    }
    for(int i=st.nSnakes-1;i>=0;i--){
        const Snake&sn=st.snakes[i]; if(!sn.alive) continue;
        char bc=(sn.owner==0)?('a'+(sn.id%8)):('A'+(sn.id%8));
        char hc=(sn.id>=0&&sn.id<=9)?('0'+sn.id):'?';
        for(int k=(int)sn.body.size()-1;k>=0;k--){
            Coord c=sn.body[k];
            if(c.x>=0&&c.x<W&&c.y>=0&&c.y<H)
                grid[c.y][c.x]=(k==0)?hc:bc;
        }
    }
    os<<"+"<<string(W,'-')<<"+"<<endl;
    for(int y=0;y<H;y++){os<<"|";for(int x=0;x<W;x++)os<<grid[y][x];os<<"|"<<endl;}
    os<<"+"<<string(W,'-')<<"+"<<endl;
    os<<"Mine:";
    for(int i=0;i<st.nSnakes;i++) if(st.snakes[i].alive&&st.snakes[i].owner==0)
        os<<" "<<st.snakes[i].id<<"("<<st.snakes[i].len()<<")";
    os<<" | Opp:";
    for(int i=0;i<st.nSnakes;i++) if(st.snakes[i].alive&&st.snakes[i].owner==1)
        os<<" "<<st.snakes[i].id<<"("<<st.snakes[i].len()<<")";
    os<<endl;
}

vector<Coord> parseBody(const string&s){
    vector<Coord>r; string c=s;
    for(char&ch:c) if(ch==':'||ch==';') ch=' ';
    istringstream ss(c); string t;
    while(ss>>t){
        auto cm=t.find(',');
        if(cm==string::npos) continue;
        r.push_back({stoi(t.substr(0,cm)),stoi(t.substr(cm+1))});
    }
    return r;
}

int main(){
    ios_base::sync_with_stdio(false); cin.tie(nullptr);
    tracker.init();

    int myId, spp;
    cin>>myId;cin.ignore(); cin>>W;cin.ignore(); cin>>H;cin.ignore();
    BitBoard walls;
    for(int y=0;y<H;y++){
        string row; getline(cin,row);
        for(int x=0;x<(int)row.size()&&x<W;x++)
            if(row[x]=='#') walls.setC({x,y});
    }
    cin>>spp;cin.ignore();
    vector<int> myIds(spp),oppIds(spp);
    for(int i=0;i<spp;i++){cin>>myIds[i];cin.ignore();}
    for(int i=0;i<spp;i++){cin>>oppIds[i];cin.ignore();}

    map<int,int> id2idx,id2owner; int idx=0;
    for(int id:myIds){id2idx[id]=idx;id2owner[id]=0;idx++;}
    for(int id:oppIds){id2idx[id]=idx;id2owner[id]=1;idx++;}
    int totalSnakes=idx, turn=0;

    while(true){
        auto t0=Clock::now(); turn++;
        int nrg; cin>>nrg;cin.ignore();
        BitBoard apples;
        for(int i=0;i<nrg;i++){int x,y;cin>>x>>y;cin.ignore();apples.setC({x,y});}

        int nb; cin>>nb;cin.ignore();
        State st; st.walls=walls; st.apples=apples; st.nSnakes=totalSnakes; st.turn=turn;
        set<int> aliveIds;
        for(int i=0;i<totalSnakes;i++){st.snakes[i].alive=false;st.snakes[i].id=-1;st.snakes[i].owner=-1;}
        for(int i=0;i<nb;i++){
            int sid; string bs; cin>>sid>>bs;cin.ignore();
            if(id2idx.count(sid)){
                int si=id2idx[sid];
                st.snakes[si].id=sid; st.snakes[si].owner=id2owner[sid];
                st.snakes[si].alive=true;
                auto body=parseBody(bs);
                st.snakes[si].body=deque<Coord>(body.begin(),body.end());
                aliveIds.insert(sid);
            }
        }

        tracker.update(st);

        // Log beheading streaks
        for(int i=0;i<totalSnakes;i++){
            if(st.snakes[i].alive && st.snakes[i].owner==0 && tracker.streak[i]>0)
                cerr<<"  WARNING: snake "<<st.snakes[i].id<<" beheading streak="<<tracker.streak[i]<<endl;
        }

        int64_t budget=(turn==1)?800:38;
        vector<int> bestMoves=beamSearch(st,0,budget);

        string out; int mi=0;
        for(int i=0;i<spp;i++){
            int sid=myIds[i];
            if(!aliveIds.count(sid)) continue;
            int dir=(mi<(int)bestMoves.size())?bestMoves[mi]:0; mi++;
            if(dir<0||dir>3) dir=0;
            if(!out.empty()) out+=";";
            out+=to_string(sid)+" "+DIR_NAMES[dir];
        }

        cerr<<"T"<<turn<<" "<<st.scoreFor(0)<<"v"<<st.scoreFor(1)
            <<" E="<<apples.popcount()<<" t="<<elapsed(t0)<<"ms"<<endl;
        printMap(st);
        cout<<(out.empty()?"WAIT":out)<<endl;
    }
}