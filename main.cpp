#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

const int ROW=20, COL=10, CELL=8;
const int LW=COL*CELL, LH=ROW*CELL;
const int HB_W=36, SB_W=56, TEX_W=HB_W+LW+SB_W, TEX_H=LH;
const int WIN_SCALE=5;
const int VS_PW=36, VS_GAP=16;
const int VS_TEX_W=VS_PW+LW+VS_GAP+LW+VS_PW;
const int VS_P1_FX=VS_PW, VS_P2_FX=VS_PW+LW+VS_GAP;
const float PI=3.14159265f;
const float SPIN_DUR=0.35f;

struct Tetromino { int m[4][4]{}; };
const Tetromino SHAPES[7]={
    {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}},
    {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}},
    {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}},
    {{{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}},
    {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}},
    {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}},
    {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}},
};

struct Pal { sf::Uint8 r,g,b; };
const Pal PAL[8]={
    {0,245,255},{40,90,255},{255,140,40},
    {255,230,60},{80,255,120},{200,100,255},{255,80,100},
    {120,125,140},
};

sf::Color mul(const sf::Color& c, float k) {
    return {(sf::Uint8)std::min(255.f,c.r*k),(sf::Uint8)std::min(255.f,c.g*k),
            (sf::Uint8)std::min(255.f,c.b*k),c.a};
}

sf::Color hueToRGB(float h) {
    h-=std::floor(h);
    float f=h*6.f; int i=(int)f; float p=f-i;
    float r=0,g=0,b=0;
    switch(i%6){
        case 0: r=1;g=p;b=0; break;  case 1: r=1-p;g=1;b=0; break;
        case 2: r=0;g=1;b=p; break;  case 3: r=0;g=1-p;b=1; break;
        case 4: r=p;g=0;b=1; break;  default:r=1;g=0;b=1-p; break;
    }
    return {(sf::Uint8)(r*255),(sf::Uint8)(g*255),(sf::Uint8)(b*255)};
}

sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
    return {(sf::Uint8)(a.r+(b.r-a.r)*t),(sf::Uint8)(a.g+(b.g-a.g)*t),
            (sf::Uint8)(a.b+(b.b-a.b)*t),(sf::Uint8)(a.a+(b.a-a.a)*t)};
}

void drawPixel(sf::RenderTarget& rt, int x, int y, const sf::Color& c) {
    static sf::RectangleShape px; px.setSize({1,1});
    px.setPosition((float)x,(float)y); px.setFillColor(c); rt.draw(px);
}

void drawCellC(sf::RenderTarget& rt, float gx, float gy, const sf::Color& base, sf::Uint8 a=255) {
    sf::Color bc=base; bc.a=a;
    sf::Color hi=mul(bc,1.35f); hi.a=a;
    sf::Color lo=mul(bc,0.45f); lo.a=a;
    sf::Color ol(12,16,28,a);
    static sf::RectangleShape r; r.setOutlineThickness(0);
    r.setSize({(float)CELL,(float)CELL}); r.setPosition(gx,gy);
    r.setFillColor(bc); r.setOutlineThickness(-1.f); r.setOutlineColor(ol); rt.draw(r);
    r.setOutlineThickness(0);
    r.setFillColor(hi);
    r.setSize({(float)(CELL-2),2}); r.setPosition(gx+1,gy+1); rt.draw(r);
    r.setSize({2,(float)(CELL-3)}); r.setPosition(gx+1,gy+3); rt.draw(r);
    r.setFillColor(lo);
    r.setSize({(float)(CELL-3),2}); r.setPosition(gx+2,gy+CELL-3); rt.draw(r);
    r.setSize({2,(float)(CELL-4)}); r.setPosition(gx+CELL-3,gy+3); rt.draw(r);
}

void drawCell(sf::RenderTarget& rt, float gx, float gy, int ci, sf::Uint8 a=255) {
    auto& p=PAL[ci]; drawCellC(rt,gx,gy,sf::Color(p.r,p.g,p.b),a);
}

void drawCellSpin(sf::RenderTarget& rt, float gx, float gy, const sf::Color& base, float angle, int axis) {
    float s=std::cos(angle);
    bool back=s<0; float absS=std::abs(s);
    if (absS<0.06f) return;
    sf::Color c=back?mul(base,0.35f):base;
    sf::Color hi=back?mul(c,0.8f):mul(c,1.35f);
    sf::Color ol(12,16,28);
    static sf::RectangleShape r; r.setOutlineThickness(0);
    if (axis==0) {
        float h=CELL*absS, yoff=(CELL-h)*0.5f;
        r.setSize({(float)CELL,h}); r.setPosition(gx,gy+yoff);
        r.setFillColor(c); r.setOutlineThickness(-1.f); r.setOutlineColor(ol); rt.draw(r);
        r.setOutlineThickness(0);
        if (!back&&h>3) {
            r.setFillColor(hi); r.setSize({(float)(CELL-2),std::max(1.f,2*absS)});
            r.setPosition(gx+1,gy+yoff+1); rt.draw(r);
        }
    } else {
        float w=CELL*absS, xoff=(CELL-w)*0.5f;
        r.setSize({w,(float)CELL}); r.setPosition(gx+xoff,gy);
        r.setFillColor(c); r.setOutlineThickness(-1.f); r.setOutlineColor(ol); rt.draw(r);
        r.setOutlineThickness(0);
        if (!back&&w>3) {
            r.setFillColor(hi); r.setSize({std::max(1.f,2*absS),(float)(CELL-2)});
            r.setPosition(gx+xoff+1,gy+1); rt.draw(r);
        }
    }
}

const unsigned GLYPH[40][5]={
    {7,5,5,5,7},{6,2,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
    {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,7,4,7},
    {7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{7,1,1,5,2},
    {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,5,5,5},{2,5,5,5,2},
    {7,5,7,4,4},{2,5,5,2,1},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
    {5,5,5,5,7},{5,5,5,2,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},
    {7,1,2,4,7},
};
const unsigned GLYPH_PUNCT[][5]={{0,0,2,0,0},{0,2,0,2,0},{2,2,2,0,2}};

int glyphIndex(char c) {
    if (c>='0'&&c<='9') return c-'0';
    if (c>='A'&&c<='Z') return 10+(c-'A');
    switch(c){case '-':return 100;case ':':return 101;case '!':return 102;default:return -1;}
}
void drawGlyph(sf::RenderTarget& rt,int x,int y,int gi,const sf::Color& c,int sc=1) {
    if (gi<0) return;
    const unsigned* rows=(gi>=100)?GLYPH_PUNCT[gi-100]:GLYPH[gi];
    for (int row=0;row<5;row++) { unsigned bits=rows[row];
        for (int col=0;col<3;col++) if (bits&(4>>col))
            for (int sy=0;sy<sc;sy++) for (int sx=0;sx<sc;sx++)
                drawPixel(rt,x+col*sc+sx,y+row*sc+sy,c);
    }
}
void drawText(sf::RenderTarget& rt,int x,int y,const char* s,const sf::Color& c,int sc=2) {
    int cx=x; for (;*s;++s) { if (*s==' '){cx+=4*sc;continue;} drawGlyph(rt,cx,y,glyphIndex(*s),c,sc); cx+=4*sc; }
}
int textWidth(const char* s,int sc=2) { int w=0; for(;*s;++s) w+=4*sc; return w>0?w-sc:0; }
void drawTextCentered(sf::RenderTarget& rt,int cx,int y,const char* s,const sf::Color& c,int sc=2) {
    drawText(rt,cx-textWidth(s,sc)/2,y,s,c,sc);
}
void drawNumber(sf::RenderTarget& rt,int x,int y,int n,const sf::Color& c,int sc=2) {
    char buf[16]; snprintf(buf,sizeof buf,"%d",n); drawText(rt,x,y,buf,c,sc);
}

int textWidthRainbow(const char* s,int sc=1){int n=0;for(const char*p=s;*p;++p)n++;if(!n)return 0;int step=4*sc+4;return (n-1)*step+3*sc;}
void drawTextRainbow(sf::RenderTarget& rt,int x,int y,const char* s,int frame,int sc=1){
    int step=4*sc+4;
    int cx=x;int ci=0;
    for(;*s;++s,ci++){
        if(*s==' '){cx+=step;continue;}
        int gi=glyphIndex(*s);if(gi<0){cx+=step;continue;}
        int wy=y+(int)std::round(std::sin(frame*0.08f+ci*0.55f)*2.5f);
        for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
            if(std::abs(dx)<=1&&std::abs(dy)<=1)continue;
            drawGlyph(rt,cx+dx,wy+dy,gi,sf::Color(0,0,0,200),sc);}
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
            if(dx||dy)drawGlyph(rt,cx+dx,wy+dy,gi,sf::Color(255,255,255),sc);
        drawGlyph(rt,cx,wy,gi,hueToRGB(frame*0.018f+ci*0.13f),sc);
        cx+=step;
    }
}
void drawTextRainbowCentered(sf::RenderTarget& rt,int cx,int y,const char* s,int frame,int sc=1){
    drawTextRainbow(rt,cx-textWidthRainbow(s,sc)/2,y,s,frame,sc);
}

void copyMat(int d[4][4],const int s[4][4]){for(int i=0;i<4;i++)for(int j=0;j<4;j++)d[i][j]=s[i][j];}
void rotateCW(int d[4][4],const int s[4][4]){for(int i=0;i<4;i++)for(int j=0;j<4;j++)d[j][3-i]=s[i][j];}

struct Kick{int dx,dy;};
const Kick KICKS_N[]={{0,0},{-1,0},{1,0},{0,-1},{-1,-1},{1,-1}};
const int KICKS_NN=6;
const Kick KICKS_I[]={{0,0},{-1,0},{1,0},{-2,0},{2,0},{0,-1},{-1,-1},{1,-1},{-2,-1},{2,-1},{0,-2}};
const int KICKS_IN=11;

} // namespace

class ArcadeSfx {
public:
    ArcadeSfx() {
        const unsigned R=44100;
        mkSq(movB_,R,520.f,.04f,.35f); mkSq(rotB_,R,380.f,.05f,.4f);
        mkLock(lokB_,R); mkSlam(slmB_,R);
        mkLine1(lin1B_,R); mkLine2(lin2B_,R); mkLine3(lin3B_,R); mkLine4(lin4B_,R);
        mkOver(ovrB_,R); mkSq(clkB_,R,660.f,.018f,.15f); mkLvlUp(lvlB_,R);
        mkTheme(thmB_,R);
        mov_.setBuffer(movB_); rot_.setBuffer(rotB_); lok_.setBuffer(lokB_);
        slm_.setBuffer(slmB_); lin1_.setBuffer(lin1B_); lin2_.setBuffer(lin2B_);
        lin3_.setBuffer(lin3B_); lin4_.setBuffer(lin4B_); ovr_.setBuffer(ovrB_);
        clk_.setBuffer(clkB_); lvl_.setBuffer(lvlB_); thm_.setBuffer(thmB_);
        thm_.setLoop(true);
        setMuted(false);
    }
    void setMuted(bool m) {
        float v=m?0.f:1.f;
        mov_.setVolume(55*v); rot_.setVolume(60*v); lok_.setVolume(45*v);
        slm_.setVolume(72*v); lin1_.setVolume(65*v); lin2_.setVolume(68*v);
        lin3_.setVolume(72*v); lin4_.setVolume(78*v); ovr_.setVolume(58*v);
        clk_.setVolume(50*v); lvl_.setVolume(75*v); thm_.setVolume(32*v); muted_=m;
    }
    bool muted() const{return muted_;}
    void toggleMute(){setMuted(!muted_);}
    void playMove(){rp(mov_);} void playRot(){rp(rot_);}
    void playLock(){rp(lok_);} void playSlam(){rp(slm_);}
    void playLine(int n=1){
        if(n<=1)rp(lin1_);else if(n==2)rp(lin2_);else if(n==3)rp(lin3_);else rp(lin4_);
    }
    void playOver(){rp(ovr_);}
    void playClick(){rp(clk_);} void playLvlUp(){rp(lvl_);}
    void playTheme(){if(thm_.getStatus()!=sf::Sound::Playing)thm_.play();}
    void pauseTheme(){thm_.pause();}
    void stopTheme(){thm_.stop();}
    void setThemePitch(float p){thm_.setPitch(p);}
private:
    static void rp(sf::Sound& s){s.stop();s.play();}
    static void appSq(std::vector<sf::Int16>& o,unsigned R,float hz,float dur,float vol){
        unsigned n=std::max(1u,(unsigned)(dur*R)),st=(unsigned)o.size(); o.resize(st+n);
        for(unsigned i=0;i<n;i++){float t=i/(float)R;
            o[st+i]=(sf::Int16)((std::sin(6.2831853f*hz*t)>=0?1.f:-1.f)*vol*5000.f*std::exp(-t*18.f));}
    }
    static void mkSq(sf::SoundBuffer& b,unsigned R,float hz,float dur,float vol){
        std::vector<sf::Int16> s;appSq(s,R,hz,dur,vol);b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLock(sf::SoundBuffer& b,unsigned R){mkSq(b,R,180.f,.028f,.22f);}
    static void mkSlam(sf::SoundBuffer& b,unsigned R){
        unsigned n=(unsigned)(.12f*R);std::vector<sf::Int16> s(n);
        std::mt19937 rng(42);std::uniform_int_distribution<int> d(-32767,32767);
        for(unsigned i=0;i<n;i++){float e=std::pow(1.f-i/(float)n,.55f);
            s[i]=(sf::Int16)((std::sin(6.2831853f*55.f*(i/(float)R))*.35f+d(rng)/32768.f*.65f)*e*9000.f);}
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLine1(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;
        for(float f:{392.f,523.25f,659.25f}) appSq(s,R,f,.05f,.25f);
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLine2(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;
        for(float f:{523.25f,659.25f,783.99f,1046.5f}) appSq(s,R,f,.05f,.28f);
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLine3(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;
        for(float f:{659.25f,783.99f,1046.5f,1318.5f,1568.f}) appSq(s,R,f,.045f,.3f);
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLine4(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;
        for(float f:{783.99f,1046.5f,1318.5f,1568.f,2093.f}) appSq(s,R,f,.05f,.32f);
        appSq(s,R,2637.f,.1f,.22f);
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkOver(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;float f=320.f;
        for(int i=0;i<5;i++){appSq(s,R,f,.11f,.32f);f*=.78f;}
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkLvlUp(sf::SoundBuffer& b,unsigned R){
        std::vector<sf::Int16> s;
        for(float f:{523.25f,659.25f,783.99f,1046.5f,1318.5f,1568.f})
            appSq(s,R,f,.07f,.3f);
        appSq(s,R,2093.f,.14f,.22f);
        b.loadFromSamples(s.data(),s.size(),1,R);}
    static void mkTheme(sf::SoundBuffer& b,unsigned R){
        const float BD=60.f/120.f;
        const float LN[]={
            659.25f,493.88f,523.25f,587.33f,523.25f,493.88f,440.f,440.f,523.25f,
            659.25f,587.33f,523.25f,493.88f,493.88f,523.25f,587.33f,659.25f,523.25f,
            440.f,440.f,0.f,
            587.33f,698.46f,880.f,783.99f,698.46f,659.25f,523.25f,659.25f,587.33f,
            523.25f,493.88f,493.88f,523.25f,587.33f,659.25f,523.25f,440.f,440.f,0.f,
            329.63f,261.63f,293.66f,246.94f,261.63f,220.f,207.65f,246.94f,
            329.63f,261.63f,293.66f,246.94f,261.63f,329.63f,440.f,440.f,415.3f,0.f};
        const float LT[]={
            1,0.5f,0.5f,1,0.5f,0.5f,1,0.5f,0.5f,1,0.5f,0.5f,1,0.5f,0.5f,1,1,1,1,1,1,
            1.5f,0.5f,1,0.5f,0.5f,1.5f,0.5f,1,0.5f,0.5f,1,0.5f,0.5f,1,1,1,1,1,1,
            2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,3,1};
        const int LC=58;
        const float BN[]={
            82.41f,164.81f,82.41f,164.81f,82.41f,164.81f,82.41f,164.81f,
            55.f,110.f,55.f,110.f,55.f,110.f,55.f,110.f,
            51.91f,103.83f,51.91f,103.83f,51.91f,103.83f,51.91f,103.83f,
            55.f,110.f,55.f,110.f,55.f,123.47f,130.81f,164.81f,
            73.42f,146.83f,73.42f,146.83f,73.42f,146.83f,73.42f,146.83f,
            65.41f,130.81f,65.41f,130.81f,65.41f,130.81f,65.41f,130.81f,
            61.74f,123.47f,61.74f,123.47f,61.74f,123.47f,61.74f,123.47f,
            55.f,110.f,55.f,110.f,55.f,110.f,55.f,110.f,
            55.f,82.41f,55.f,82.41f,55.f,82.41f,55.f,82.41f,
            51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,
            55.f,82.41f,55.f,82.41f,55.f,82.41f,55.f,82.41f,
            51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,
            55.f,82.41f,55.f,82.41f,55.f,82.41f,55.f,82.41f,
            51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,
            55.f,82.41f,55.f,82.41f,55.f,82.41f,55.f,82.41f,
            51.91f,82.41f,51.91f,82.41f,51.91f,82.41f,51.91f,82.41f};
        const int BC=128;
        float totalBeats=0;for(int i=0;i<LC;i++)totalBeats+=LT[i];
        unsigned N=(unsigned)(totalBeats*BD*R);
        std::vector<sf::Int16> s(N,0);
        unsigned pos=0;
        for(int i=0;i<LC;i++){
            unsigned dur=(unsigned)(LT[i]*BD*R);
            if(LN[i]>1.f){
                for(unsigned j=0;j<dur&&pos+j<N;j++){
                    float t=j/(float)R;
                    float env=1.f;
                    if(j<(unsigned)(R*0.005f))env=j/(R*0.005f);
                    if(dur>200&&j>dur-200)env=(dur-j)/200.f;
                    float ph=std::fmod(t*LN[i],1.f);
                    float v=ph<0.5f?1.f:-1.f;
                    v+=ph<0.25f?0.3f:ph<0.5f?-0.3f:ph<0.75f?-0.3f:0.3f;
                    s[pos+j]+=(sf::Int16)(v*env*3200.f);
                }
            }
            pos+=dur;
        }
        pos=0;
        for(int i=0;i<BC;i++){
            unsigned dur=(unsigned)(0.5f*BD*R);
            if(BN[i]>1.f){
                for(unsigned j=0;j<dur&&pos+j<N;j++){
                    float t=j/(float)R;
                    float env=0.9f;
                    if(j<80)env=0.9f*j/80.f;
                    if(dur>120&&j>dur-120)env=0.9f*(dur-j)/120.f;
                    float v=(std::fmod(t*BN[i],1.f)<0.5f)?1.f:-1.f;
                    s[pos+j]+=(sf::Int16)(v*env*1800.f);
                }
            }
            pos+=dur;
        }
        b.loadFromSamples(s.data(),s.size(),1,R);
    }
    sf::SoundBuffer movB_,rotB_,lokB_,slmB_,lin1B_,lin2B_,lin3B_,lin4B_,ovrB_,clkB_,lvlB_,thmB_;
    sf::Sound mov_,rot_,lok_,slm_,lin1_,lin2_,lin3_,lin4_,ovr_,clk_,lvl_,thm_;
    bool muted_=false;
};

static void applyLetterboxView(sf::RenderWindow& win,float gw,float gh){
    auto ws=win.getSize();if(!ws.x||!ws.y)return;
    float ww=(float)ws.x,wh=(float)ws.y,wa=ww/wh,ga=gw/gh;
    sf::FloatRect vp(0,0,1,1);
    if(wa>ga){float w=ga/wa;vp.left=(1-w)*.5f;vp.width=w;}
    else{float h=wa/ga;vp.top=(1-h)*.5f;vp.height=h;}
    sf::View v(sf::FloatRect(0,0,gw,gh));v.setViewport(vp);win.setView(v);
}

// --- High Score Persistence ---
struct HiScore { int score; int lines; int level; char mode; }; // mode: 'M'=marathon, 'S'=sprint
struct HiScoreTable {
    static constexpr int MAX_ENTRIES=5;
    HiScore entries[MAX_ENTRIES]{};
    int count=0;
    void load(const char* path){
        count=0;
        std::ifstream f(path);
        if(!f)return;
        while(count<MAX_ENTRIES){
            int s,l,lv;char m;
            if(!(f>>s>>l>>lv>>m))break;
            entries[count++]={s,l,lv,m};
        }
    }
    void save(const char* path)const{
        std::ofstream f(path);
        for(int i=0;i<count;i++)
            f<<entries[i].score<<' '<<entries[i].lines<<' '<<entries[i].level<<' '<<entries[i].mode<<'\n';
    }
    int rank(int s)const{for(int i=0;i<count;i++)if(s>entries[i].score)return i;return count<MAX_ENTRIES?count:-1;}
    void insert(HiScore h){
        int r=rank(h.score);if(r<0)return;
        for(int i=std::min(count,MAX_ENTRIES-1);i>r;i--)entries[i]=entries[i-1];
        entries[r]=h;if(count<MAX_ENTRIES)count++;
    }
};

enum class AppState { Menu, Playing, GameOverStats, VsPlaying };
enum class GameMode { Marathon, Sprint };
enum class VsDiff { Easy, Medium, Hard };

struct Game {
    int grid[ROW][COL]{};
    int score=0,level=0,linesTotal=0,combo=-1;
    int piece[4][4]{},px=0,py=0,pieceId=0,nextId=0;
    int holdId=-1; bool holdUsed=false;
    std::vector<int> bag;
    std::mt19937 rng;
    enum Phase{Playing,LineFlash,GameOver,Paused} phase=Playing;
    std::vector<int> flashRows;
    int flashFrames=0;
    static constexpr int FLASH_TOTAL=24;
    ArcadeSfx* sfx=nullptr;
    bool wantQuit=false;
    bool scoreSubmitted=false;
    int hiRank=-1; int nextHiScore=0;

    bool holdLeft=false,holdRight=false,holdDown=false;
    float dasTimer=0,dasRepeat=0; int dasDir=0;
    static constexpr float DAS_DELAY=0.17f,DAS_RATE=0.05f;
    float lockTimer=0; bool lockPending=false;
    static constexpr float LOCK_DELAY=0.5f;
    int lockMoves=0; static constexpr int LOCK_MOVES_MAX=15;

    // Per-cell 3D spin animation
    float cellSpin[ROW][COL]{}; // <0=delay, 0..SPIN_DUR=spinning, 0=idle
    int cellAxis[ROW][COL]{};   // 0=X-axis (vert squish), 1=Y-axis (horiz squish)

    // Level-up rainbow
    float lvlUpTimer=0;
    int prevLevel=0;
    static constexpr float LVL_UP_DUR=1.0f;

    // Line-clear field effect: 0=none, 2=yellow, 3=fire, 4=rainbow
    int clearFxType=0;
    float clearFxTimer=0;
    static constexpr float CLEAR_FX_DUR=1.0f;

    // Rubber-banding visual offset (pixels in low-res space)
    float rubX=0,rubY=0;
    static constexpr float RUB_DECAY=0.65f;
    static constexpr float RUB_MAX=4.f;

    // Hard-drop bloom trail
    float dropTrailTimer=0;
    static constexpr float DROP_TRAIL_DUR=0.3f;
    int trailPx=0,trailStartY=0,trailEndY=0;
    int trailPiece[4][4]{};
    int trailColor=0;

    // Camera shake (vertical)
    float shakeTimer=0;
    static constexpr float SHAKE_DUR=0.18f;

    // T-Spin detection
    bool lastWasRotation=false;
    bool lastWasTSpin=false;
    int tSpinCount=0;

    // Combo / action text overlay
    struct ActionText { char text[24]; float timer; sf::Color color; };
    ActionText actionTexts[4]{}; int actionCount=0;
    void pushAction(const char* txt, sf::Color col){
        if(actionCount<4){
            auto& a=actionTexts[actionCount++];
            snprintf(a.text,sizeof a.text,"%s",txt);
            a.timer=1.5f; a.color=col;
        }
    }

    // Danger zone
    float dangerLevel()const{
        for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++)if(grid[i][j])return 1.f-(float)i/ROW;
        return 0.f;
    }

    // Perfect clear
    int perfectClears=0;

    // Sprint mode
    GameMode gameMode=GameMode::Marathon;
    float sprintTimer=0;
    float sprintBest=0;
    static constexpr int SPRINT_TARGET=40;

    // Stats
    int piecesPlaced=0;
    int maxCombo=0;
    int tetrisCount=0;
    float playTime=0;

    // Background scene manager
    static constexpr int NUM_SCENES=12;
    int bgScene=0, bgNextScene=1;
    float bgSceneTimer=0, bgSceneDur=90.f;
    int bgOrder[NUM_SCENES]{};
    int bgOrderIdx=0;

    int fieldX=HB_W; int texW=TEX_W; bool vsMode=false;
    int garbagePending=0, outGarbage=0;

    void initBgScenes(){
        for(int i=0;i<NUM_SCENES;i++)bgOrder[i]=i;
        std::shuffle(bgOrder,bgOrder+NUM_SCENES,rng);
        bgScene=bgOrder[0]; bgOrderIdx=0;
        bgSceneDur=60.f+(rng()%60);
        bgSceneTimer=0;
    }
    void advanceBgScene(){
        bgOrderIdx++;
        if(bgOrderIdx>=NUM_SCENES){
            std::shuffle(bgOrder,bgOrder+NUM_SCENES,rng);
            bgOrderIdx=0;
        }
        bgScene=bgOrder[bgOrderIdx];
        bgSceneDur=60.f+(rng()%60);
        bgSceneTimer=0;
    }

    Game():rng((unsigned)time(nullptr)){initBgScenes();refillBag();nextId=pullBag();spawn();}

    void refillBag(){bag={0,1,2,3,4,5,6};std::shuffle(bag.begin(),bag.end(),rng);}
    int pullBag(){if(bag.empty())refillBag();int v=bag.back();bag.pop_back();return v;}

    void spawn(){
        pieceId=nextId;nextId=pullBag();
        px=COL/2-2;py=-1;
        copyMat(piece,SHAPES[pieceId].m);
        lockPending=false;lockTimer=0;lockMoves=0;
        holdUsed=false;
        rubX=0;rubY=0;
        if(collides(px,py,piece)){py=0;}
        if(collides(px,py,piece)){phase=GameOver;if(sfx){sfx->playOver();sfx->stopTheme();}}
    }

    bool holdPiece(){
        if(phase!=Playing||holdUsed)return false;
        holdUsed=true;
        if(holdId<0){
            holdId=pieceId;
            pieceId=nextId;nextId=pullBag();
        }else{
            int tmp=holdId;holdId=pieceId;pieceId=tmp;
        }
        px=COL/2-2;py=-1;
        copyMat(piece,SHAPES[pieceId].m);
        lockPending=false;lockTimer=0;lockMoves=0;
        rubX=0;rubY=0;
        return true;
    }

    bool collides(int ox,int oy,const int b[4][4])const{
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(b[i][j]){
            int nx=ox+j,ny=oy+i;
            if(nx<0||nx>=COL||ny>=ROW)return true;
            if(ny>=0&&grid[ny][nx])return true;
        } return false;
    }

    bool rotatePiece(){
        if(pieceId==3)return false;
        int t[4][4];rotateCW(t,piece);
        const Kick* kicks=(pieceId==0)?KICKS_I:KICKS_N;
        int nk=(pieceId==0)?KICKS_IN:KICKS_NN;
        for(int k=0;k<nk;k++){
            int tx=px+kicks[k].dx,ty=py+kicks[k].dy;
            if(!collides(tx,ty,t)){
                bool vis=false;
                for(int i=0;i<4&&!vis;i++)for(int j=0;j<4;j++)
                    if(t[i][j]&&ty+i>=0){vis=true;break;}
                if(!vis)continue;
                copyMat(piece,t);px=tx;py=ty;resetLockDelay();lastWasRotation=true;return true;
            }
        } return false;
    }

    void triggerCellSpins(){
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(piece[i][j]){
            int ny=py+i,nx=px+j;
            if(ny>=0&&ny<ROW&&nx>=0&&nx<COL){
                cellSpin[ny][nx]=-(float)(rng()%150+10)*0.001f;
                cellAxis[ny][nx]=rng()%2;
            }
        }
    }

    bool checkTSpin()const{
        if(pieceId!=5||!lastWasRotation)return false;
        int cx=px+1,cy=py+1;
        int corners=0;
        for(int di:{-1,1})for(int dj:{-1,1}){
            int ni=cy+di,nj=cx+dj;
            if(ni<0||ni>=ROW||nj<0||nj>=COL||grid[ni][nj])corners++;
        }
        return corners>=3;
    }

    void lockDown(){
        lastWasTSpin=checkTSpin();
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(piece[i][j]){
            int nx=px+j,ny=py+i;
            if(ny>=0&&ny<ROW)grid[ny][nx]=pieceId+1;
        }
        piecesPlaced++;
        triggerCellSpins();
        lockPending=false;
        lastWasRotation=false;
        findAndFlash();
    }

    void findAndFlash(){
        flashRows.clear();
        for(int i=0;i<ROW;i++){bool full=true;
            for(int j=0;j<COL;j++)if(!grid[i][j]){full=false;break;}
            if(full)flashRows.push_back(i);}
        if(flashRows.empty()){
            if(lastWasTSpin){pushAction("T-SPIN",sf::Color(200,120,255));
                score+=100*(level+1);tSpinCount++;}
            combo=-1;outGarbage=0;injectGarbage();spawn();return;
        }
        int n=(int)flashRows.size();
        if(sfx)sfx->playLine(n);
        if(n>=2){clearFxType=n;clearFxTimer=CLEAR_FX_DUR;}
        combo++;
        if(combo>maxCombo)maxCombo=combo;

        static const int mult[]={0,100,300,500,800};
        int add=mult[std::min(n,4)]*(level+1);
        if(lastWasTSpin){add=(int)(add*1.5f);tSpinCount++;
            pushAction(n>=2?"T-SPIN DOUBLE!":"T-SPIN SINGLE!",sf::Color(200,120,255));}
        if(combo>0){add+=50*combo*(level+1);
            char cb[24];snprintf(cb,sizeof cb,"%dX COMBO!",combo+1);
            pushAction(cb,sf::Color(255,230,80));}
        if(n==4){tetrisCount++;pushAction("TETRIS!",sf::Color(120,255,200));}

        bool perfect=true;
        for(int i=0;i<ROW&&perfect;i++)for(int j=0;j<COL;j++){
            if(grid[i][j]&&std::find(flashRows.begin(),flashRows.end(),i)==flashRows.end()){perfect=false;break;}}
        if(perfect){add+=1500*(level+1);perfectClears++;
            pushAction("PERFECT CLEAR!",sf::Color(255,255,255));
            lvlUpTimer=LVL_UP_DUR;clearFxType=4;clearFxTimer=CLEAR_FX_DUR;}

        outGarbage=0;
        if(n==2)outGarbage=1;else if(n==3)outGarbage=2;else if(n>=4)outGarbage=4;
        if(lastWasTSpin){outGarbage=n>=2?4:2;}
        if(perfect)outGarbage=10;
        {int cancel=std::min(garbagePending,outGarbage);garbagePending-=cancel;outGarbage-=cancel;}

        score+=add;flashFrames=FLASH_TOTAL;phase=LineFlash;
    }

    void finishLineClear(){
        int tmp[ROW][COL]{};
        int dst=ROW-1;
        for(int src=ROW-1;src>=0;src--){
            if(std::find(flashRows.begin(),flashRows.end(),src)!=flashRows.end())continue;
            for(int j=0;j<COL;j++)tmp[dst][j]=grid[src][j];
            dst--;
        }
        for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++)grid[i][j]=tmp[i][j];
        linesTotal+=(int)flashRows.size();
        int newLvl=std::min(20,linesTotal/10);
        if(newLvl>level){
            lvlUpTimer=LVL_UP_DUR;
            if(sfx)sfx->playLvlUp();
            // Trigger spins on all placed cells for celebration
            for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++)if(grid[i][j]&&cellSpin[i][j]==0){
                cellSpin[i][j]=-(float)(rng()%400+10)*0.001f;
                cellAxis[i][j]=rng()%2;
            }
        }
        level=newLvl;
        // Compact cellSpin arrays to match compacted grid
        float tmpSpin[ROW][COL]{}; int tmpAxis[ROW][COL]{};
        dst=ROW-1;
        for(int src=ROW-1;src>=0;src--){
            if(std::find(flashRows.begin(),flashRows.end(),src)!=flashRows.end())continue;
            for(int j=0;j<COL;j++){tmpSpin[dst][j]=cellSpin[src][j];tmpAxis[dst][j]=cellAxis[src][j];}
            dst--;
        }
        for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++){cellSpin[i][j]=tmpSpin[i][j];cellAxis[i][j]=tmpAxis[i][j];}

        flashRows.clear();
        if(gameMode==GameMode::Sprint&&linesTotal>=SPRINT_TARGET){
            phase=GameOver;
            if(sprintBest<=0||sprintTimer<sprintBest)sprintBest=sprintTimer;
            return;
        }
        injectGarbage();
        phase=Playing;spawn();
    }

    bool tryShift(int dx){
        if(phase!=Playing)return false;
        if(!collides(px+dx,py,piece)){px+=dx;resetLockDelay();lastWasRotation=false;return true;}
        return false;
    }

    void resetLockDelay(){if(lockPending&&lockMoves<LOCK_MOVES_MAX){lockTimer=0;lockMoves++;}}
    bool onGround()const{return collides(px,py+1,piece);}

    void gravityStep(){
        if(phase!=Playing)return;
        if(!collides(px,py+1,piece)){py++;
            if(onGround()){lockPending=true;lockTimer=0;lockMoves=0;}
            else lockPending=false;
        }else{if(!lockPending){lockPending=true;lockTimer=0;lockMoves=0;}}
    }
    void softDropStep(){
        if(phase!=Playing)return;
        if(!collides(px,py+1,piece)){py++;score+=1;
            if(onGround()){lockPending=true;lockTimer=0;lockMoves=0;}
            else lockPending=false;}
    }
    void hardDrop(){
        if(phase!=Playing)return;
        int startY=py;
        while(!collides(px,py+1,piece)){py++;score+=2;}
        if(py>startY){
            trailStartY=startY; trailEndY=py; trailPx=px;
            copyMat(trailPiece,piece); trailColor=pieceId;
            dropTrailTimer=DROP_TRAIL_DUR;
            shakeTimer=SHAKE_DUR;
        }
        if(sfx)sfx->playSlam();
        kickRub(0,-3.5f);
        lockDown();
    }

    float dropDelay()const{return std::max(0.05f,0.85f-level*0.035f);}
    int ghostY()const{int gy=py;while(!collides(px,gy+1,piece))gy++;return gy;}

    void restart(){
        for(auto& r:grid)for(int& c:r)c=0;
        for(auto& r:cellSpin)for(auto& c:r)c=0;
        score=0;level=0;linesTotal=0;combo=-1;prevLevel=0;
        phase=Playing;flashRows.clear();
        holdLeft=holdRight=holdDown=false;lockPending=false;
        rubX=rubY=0;lvlUpTimer=0;clearFxTimer=0;clearFxType=0;dropTrailTimer=0;shakeTimer=0;
        holdId=-1;holdUsed=false;
        lastWasRotation=false;lastWasTSpin=false;tSpinCount=0;
        actionCount=0;perfectClears=0;
        piecesPlaced=0;maxCombo=0;tetrisCount=0;playTime=0;sprintTimer=0;
        scoreSubmitted=false;
        garbagePending=0;outGarbage=0;
        refillBag();nextId=pullBag();spawn();
    }

    void injectGarbage(){
        if(garbagePending<=0)return;
        int n=std::min(garbagePending,ROW-4);garbagePending-=n;
        for(int i=0;i<ROW-n;i++)for(int j=0;j<COL;j++){
            grid[i][j]=grid[i+n][j];cellSpin[i][j]=cellSpin[i+n][j];cellAxis[i][j]=cellAxis[i+n][j];}
        for(int i=ROW-n;i<ROW;i++){int gap=rng()%COL;
            for(int j=0;j<COL;j++){grid[i][j]=(j==gap)?0:8;cellSpin[i][j]=0;cellAxis[i][j]=0;}}
        if(phase==Playing&&collides(px,py,piece)){
            while(py>-3&&collides(px,py,piece))py--;
            if(collides(px,py,piece)){phase=GameOver;if(sfx){sfx->playOver();sfx->stopTheme();}}
        }
    }

    void updateDAS(float dt){
        if(phase!=Playing)return;
        int dir=0;
        if(holdLeft&&!holdRight)dir=-1;
        else if(holdRight&&!holdLeft)dir=1;
        if(dir!=dasDir){dasDir=dir;dasTimer=0;dasRepeat=0;}
        if(dir==0)return;
        dasTimer+=dt;
        if(dasTimer>=DAS_DELAY){dasRepeat+=dt;
            while(dasRepeat>=DAS_RATE){dasRepeat-=DAS_RATE;
                if(tryShift(dir)){kickRub(dir*(-1.2f),0);if(sfx)sfx->playMove();}}}
    }
    void updateLock(float dt){
        if(phase!=Playing||!lockPending)return;
        if(!onGround()){lockPending=false;return;}
        lockTimer+=dt;
        if(lockTimer>=LOCK_DELAY){if(sfx)sfx->playLock();lockDown();}
    }

    void kickRub(float dx,float dy){
        rubX+=dx; rubY+=dy;
        rubX=std::max(-RUB_MAX,std::min(RUB_MAX,rubX));
        rubY=std::max(-RUB_MAX,std::min(RUB_MAX,rubY));
    }

    void updateAnimations(float dt){
        rubX*=std::pow(RUB_DECAY,dt*60.f);
        rubY*=std::pow(RUB_DECAY,dt*60.f);
        if(std::abs(rubX)<0.08f)rubX=0;
        if(std::abs(rubY)<0.08f)rubY=0;

        // Timers
        if(lvlUpTimer>0){lvlUpTimer-=dt;if(lvlUpTimer<0)lvlUpTimer=0;}
        if(clearFxTimer>0){clearFxTimer-=dt;if(clearFxTimer<0)clearFxTimer=0;}
        if(dropTrailTimer>0){dropTrailTimer-=dt;if(dropTrailTimer<0)dropTrailTimer=0;}
        if(shakeTimer>0){shakeTimer-=dt;if(shakeTimer<0)shakeTimer=0;}
        for(int i=0;i<actionCount;){
            actionTexts[i].timer-=dt;
            if(actionTexts[i].timer<=0){
                for(int j=i;j<actionCount-1;j++)actionTexts[j]=actionTexts[j+1];
                actionCount--;
            }else i++;
        }
        if(phase==Playing){playTime+=dt;if(gameMode==GameMode::Sprint)sprintTimer+=dt;}
        bgSceneTimer+=dt;
        if(bgSceneTimer>=bgSceneDur)advanceBgScene();

        // Advance cell spins
        for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++){
            float& sp=cellSpin[i][j];
            if(sp==0)continue;
            sp+=dt;
            if(sp>SPIN_DUR)sp=0;
        }

        // Ambient random spins on placed cells (~3 per second)
        if(rng()%20==0){
            int ri=rng()%ROW,rj=rng()%COL;
            if(grid[ri][rj]&&cellSpin[ri][rj]==0){
                cellSpin[ri][rj]=-(float)(rng()%60+5)*0.001f;
                cellAxis[ri][rj]=rng()%2;
            }
        }
        // Occasional cluster spin (ripple effect, ~every 3s)
        if(rng()%180==0){
            int ri=rng()%ROW,rj=rng()%COL;
            for(int di=-1;di<=1;di++)for(int dj=-1;dj<=1;dj++){
                int ni=ri+di,nj=rj+dj;
                if(ni>=0&&ni<ROW&&nj>=0&&nj<COL&&grid[ni][nj]&&cellSpin[ni][nj]==0){
                    cellSpin[ni][nj]=-(float)(std::abs(di)+std::abs(dj))*0.06f-0.01f;
                    cellAxis[ni][nj]=rng()%2;
                }
            }
        }
    }

    // --- drawing ---

    sf::Color getCellColor(int ci,int row,int col){
        if(ci>=7) return sf::Color(120,125,140);
        auto& p=PAL[ci]; sf::Color c(p.r,p.g,p.b);
        if(lvlUpTimer>0){
            float lt=lvlUpTimer/LVL_UP_DUR;
            float blend=(lt>0.8f)?(1.f-lt)/0.2f:(lt<0.2f)?lt/0.2f:1.f;
            blend*=0.85f;
            float phase=(1.f-lt)*4.f+row*0.12f+col*0.06f;
            sf::Color rc=hueToRGB(phase);
            c=lerpColor(c,rc,blend);
        }
        if(clearFxTimer>0){
            float lt=clearFxTimer/CLEAR_FX_DUR;
            float blend=(lt>0.8f)?(1.f-lt)/0.2f:(lt<0.2f)?lt/0.2f:1.f;
            blend*=0.8f;
            float phase=(1.f-lt)*4.f+row*0.12f+col*0.06f;
            sf::Color fx;
            if(clearFxType==2){
                fx=sf::Color(255,230,60);
            }else if(clearFxType==3){
                float t=0.5f+0.5f*std::sin(phase*1.5f);
                if(t<0.33f)fx=lerpColor(sf::Color(40,80,255),sf::Color(255,230,60),t/0.33f);
                else if(t<0.66f)fx=lerpColor(sf::Color(255,230,60),sf::Color(255,80,30),(t-0.33f)/0.33f);
                else fx=lerpColor(sf::Color(255,80,30),sf::Color(40,80,255),(t-0.66f)/0.34f);
            }else{
                fx=hueToRGB(phase);
            }
            c=lerpColor(c,fx,blend);
        }
        return c;
    }

    void drawMiniPiece(sf::RenderTarget& rt,int ox,int oy,int id,int cs,sf::Uint8 a=255){
        auto& m=SHAPES[id].m;
        int minR=3,maxR=0,minC=3,maxC=0;
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(m[i][j]){
            minR=std::min(minR,i);maxR=std::max(maxR,i);
            minC=std::min(minC,j);maxC=std::max(maxC,j);}
        int pw=(maxC-minC+1)*cs,ph=(maxR-minR+1)*cs;
        int offx=(4*cs-pw)/2,offy=(2*cs-ph)/2;
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(m[i][j]){
            static sf::RectangleShape r;
            r.setSize({(float)(cs-1),(float)(cs-1)});
            r.setPosition((float)(ox+offx+(j-minC)*cs),(float)(oy+offy+(i-minR)*cs));
            auto& p=PAL[id];r.setFillColor(sf::Color(p.r,p.g,p.b,a));
            r.setOutlineThickness(-1.f);r.setOutlineColor(sf::Color(20,24,40,a));
            rt.draw(r);r.setOutlineThickness(0);}
    }

    void drawBackground(sf::RenderTarget& rt,int frame){
        const int HB_W=fieldX; const int TEX_W=texW;
        sf::RectangleShape bg;bg.setSize({(float)TEX_W,(float)TEX_H});
        bg.setFillColor(sf::Color(18,22,38));rt.draw(bg);
        for(int i=0;i<70;i++){int x=(i*47+frame)%TEX_W,y=(i*83+frame/2)%TEX_H;
            drawPixel(rt,x,y,sf::Color(40,48,72,90));}
        for(int i=0;i<24;i++){int x=(i*61-frame)%TEX_W,y=(i*19)%TEX_H;
            if(((frame+i*7)%48)<24)drawPixel(rt,(x+TEX_W)%TEX_W,y,sf::Color(255,200,120,40));}
        if(vsMode)return;

        const float SCX=HB_W+LW/2.f,SCY=LH*0.4f;
        const float ft=frame*0.008f;
        const float TAU=6.2832f;
        float sceneT=bgSceneTimer;
        float sceneFade=1.f;
        if(sceneT<3.f)sceneFade=sceneT/3.f;
        else if(sceneT>bgSceneDur-3.f)sceneFade=(bgSceneDur-sceneT)/3.f;
        sceneFade=std::max(0.f,std::min(1.f,sceneFade));

        struct Cam{float sc,ty,rot,px,py;};
        auto sstep=[](float t)->float{t=std::max(0.f,std::min(1.f,t));return t*t*(3.f-2.f*t);};
        auto getCam=[&](const Cam* shots,int n,float t,float dur)->Cam{
            float total=n*dur;float tm=std::fmod(t,total);
            int ci=(int)(tm/dur);float fr=std::fmod(tm,dur)/dur;
            int ni=(ci+1)%n;
            float bl=fr<0.8f?0.f:sstep((fr-0.8f)/0.2f);
            Cam a=shots[ci],b=shots[ni];
            return{a.sc+(b.sc-a.sc)*bl,a.ty+(b.ty-a.ty)*bl,
                   a.rot+(b.rot-a.rot)*bl,a.px+(b.px-a.px)*bl,a.py+(b.py-a.py)*bl};
        };
        auto w2s=[&](float wx,float wy,const Cam& c)->std::pair<float,float>{
            float cs2=std::cos(c.rot),sn2=std::sin(c.rot);
            float rx=wx*cs2-wy*sn2, ry=(wx*sn2+wy*cs2)*c.ty;
            return{SCX+c.px+rx*c.sc, SCY+c.py+ry*c.sc};
        };
        auto safePx=[&](float fx,float fy,sf::Color c){
            int ix=(int)fx,iy=(int)fy;
            if(ix>=HB_W&&ix<HB_W+LW&&iy>=0&&iy<LH){
                c.a=(sf::Uint8)(c.a*sceneFade);drawPixel(rt,ix,iy,c);}
        };
        static sf::RectangleShape pr;pr.setOutlineThickness(0);
        auto safeRect=[&](float fx,float fy,int sz,sf::Color c){
            c.a=(sf::Uint8)(c.a*sceneFade);
            if(fx+sz<HB_W||fx>=HB_W+LW||fy+sz<0||fy>=LH)return;
            pr.setSize({(float)sz,(float)sz});
            pr.setPosition(std::round(fx),std::round(fy));
            pr.setFillColor(c);rt.draw(pr);
        };
        auto ring=[&](float rad,int pts,const Cam& cam,sf::Color c){
            for(int s=0;s<pts;s++){
                float a=s/(float)pts*TAU;
                auto[sx,sy]=w2s(std::cos(a)*rad,std::sin(a)*rad,cam);
                safePx(sx,sy,c);
            }
        };

        int sid=bgScene;

        // ===== 0: SOLAR SYSTEM =====
        if(sid==0){
            const Cam sh[]={{0.45f,0.70f,0.0f,0,0},{1.1f,0.45f,0.35f,5,-12},
                {0.6f,0.25f,-0.2f,0,6},{0.8f,0.85f,0.9f,-8,4},{1.4f,0.55f,1.6f,-14,-6}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            ring(32,40,cam,sf::Color(40,48,72,22));ring(56,60,cam,sf::Color(40,48,72,18));
            ring(88,80,cam,sf::Color(40,48,72,14));ring(120,100,cam,sf::Color(40,48,72,11));
            for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){
                int md=std::abs(dx)+std::abs(dy);if(md>4)continue;
                auto[sx,sy]=w2s(dx*1.f,dy*1.f,cam);
                safePx(sx,sy,sf::Color(255,210,100,(sf::Uint8)(md<=1?190:md<=2?150:100)));
            }
            for(int c=0;c<14;c++){
                float ca=c*TAU/14.f,cr=4.5f+0.7f*std::sin(ft*14+c*0.8f);
                auto[sx,sy]=w2s(std::cos(ca)*cr,std::sin(ca)*cr,cam);
                safePx(sx,sy,sf::Color(255,200,120,(sf::Uint8)(30+28*std::sin(ft*11+c*1.4f))));
            }
            struct PD{float r,sp,ph;int sz;sf::Color c;};
            const PD pl[]={{32,1.8f,0,2,{255,200,120,200}},{56,1.1f,2.1f,4,{80,200,220,190}},
                {88,0.7f,4.2f,5,{90,130,255,180}},{120,0.4f,1.f,4,{180,120,220,160}}};
            for(auto& p:pl){
                float a=ft*p.sp+p.ph;
                auto[sx,sy]=w2s(std::cos(a)*p.r,std::sin(a)*p.r,cam);
                bool bh=std::sin(a+cam.rot)<0;sf::Color pc=p.c;
                if(bh)pc.a=(sf::Uint8)(pc.a*0.35f);
                int ds=std::max(1,(int)(p.sz*cam.sc));
                if(ds<=1)safePx(sx,sy,pc);
                else{safeRect(sx-ds/2.f,sy-ds/2.f,ds,pc);
                    if(!bh)safePx(sx,sy,sf::Color(255,255,255,(sf::Uint8)(pc.a/3)));}
            }
        }
        // ===== 1: GALAXY =====
        else if(sid==1){
            const Cam sh[]={{0.55f,0.85f,0,0,0},{0.9f,0.35f,0.5f,0,6},
                {1.6f,0.75f,0.2f,4,-8},{0.65f,0.18f,-0.3f,0,0},{0.95f,0.65f,1.1f,-6,4}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float gr=ft*0.05f;
            auto[cx0,cy0]=w2s(0,0,cam);safePx(cx0,cy0,sf::Color(255,240,210,200));
            for(int ri=1;ri<=8;ri++){
                float r=ri*0.75f,br=1.f-r/6.f;if(br<0)continue;
                int pts=std::max(8,(int)(r*cam.sc*8.f));pts=std::min(pts,80);
                sf::Uint8 a=(sf::Uint8)(br*br*160);
                sf::Color cc=lerpColor({255,230,195,a},{210,190,255,a},0.25f+0.2f*std::sin(gr+r*0.35f));
                for(int s=0;s<pts;s++){float ag=s/(float)pts*TAU+ri*0.4f;
                    auto[sx,sy]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);safePx(sx,sy,cc);}
            }
            auto arm=[&](float ao,int n,float mR,sf::Uint8 bA,float sc2){
                for(int i=0;i<n;i++){float t=i/(float)n,r=t*mR;
                    float ag=ao+t*8.5f+gr;float dx2=((i*7+3)%13-6)*sc2,dy2=((i*11+5)%13-6)*sc2;
                    auto[px,py]=w2s(std::cos(ag)*r+dx2,std::sin(ag)*r+dy2,cam);
                    sf::Uint8 a=(sf::Uint8)(bA*(1-0.35f*t));
                    sf::Color sc3=t<0.25f?sf::Color(255,220,180,a):t<0.55f?sf::Color(150,180,255,a):sf::Color(100,140,230,a);
                    safePx(px,py,sc3);}};
            arm(0,90,65,130,2.5f);arm(PI,90,65,130,2.5f);
            arm(PI*0.5f,55,50,75,3);arm(PI*1.5f,55,50,75,3);
            for(int i=0;i<35;i++){float a2=i*2.399f+gr*0.15f,r2=22+(i*37%55);
                auto[px,py]=w2s(std::cos(a2)*r2,std::sin(a2)*r2,cam);
                safePx(px,py,sf::Color(80,100,180,(sf::Uint8)(25+18*std::sin(ft*2.5f+i))));}
        }
        // ===== 2: NEBULA =====
        else if(sid==2){
            const Cam sh[]={{0.7f,0.9f,0,0,0},{1.3f,0.7f,0.4f,6,-5},
                {0.5f,0.5f,-0.3f,-4,8},{1.0f,0.8f,0.9f,3,-3},{0.8f,0.6f,-0.6f,-6,2}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            const sf::Color nebCols[]={{180,60,120},{60,120,200},{80,200,180},{200,130,60}};
            for(int b=0;b<4;b++){
                float bx=(b%2-0.5f)*25.f,by=(b/2-0.5f)*20.f;
                float drift=std::sin(ft*0.15f+b*1.7f)*8.f;
                float driftY=std::cos(ft*0.12f+b*2.1f)*6.f;
                for(int i=0;i<45;i++){
                    float ag=i*2.399f+b*1.57f;
                    float r=2.f+i*0.7f+std::sin(ft*0.3f+i*0.5f)*3.f;
                    float wx=bx+drift+std::cos(ag)*r;
                    float wy=by+driftY+std::sin(ag)*r;
                    auto[sx,sy]=w2s(wx,wy,cam);
                    float fade=std::max(0.f,1.f-i/45.f);
                    sf::Uint8 a=(sf::Uint8)(90*fade+20*std::sin(ft*2+i));
                    sf::Color nc=nebCols[b];nc.a=a;safePx(sx,sy,nc);
                }
            }
            for(int i=0;i<25;i++){
                float sx2=((i*73+11)%80-40)*1.f,sy2=((i*47+7)%60-30)*1.f;
                auto[px,py]=w2s(sx2,sy2,cam);
                sf::Uint8 tw=(sf::Uint8)(100+60*std::sin(ft*4+i*0.9f));
                safePx(px,py,sf::Color(255,255,255,tw));
            }
        }
        // ===== 3: COMET SHOWER =====
        else if(sid==3){
            const Cam sh[]={{0.6f,0.8f,0,0,0},{0.9f,0.6f,0.3f,5,-8},
                {1.2f,0.4f,-0.2f,-3,6},{0.7f,0.9f,0.7f,-5,-4},{1.0f,0.5f,-0.5f,4,3}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            for(int c=0;c<7;c++){
                float speed=15.f+c*8.f;
                float ang=0.6f+c*0.15f;
                float headX=std::fmod(c*37.f+ft*speed*std::cos(ang),200.f)-100.f;
                float headY=std::fmod(c*53.f+ft*speed*std::sin(ang),160.f)-80.f;
                sf::Color hc=c%3==0?sf::Color(200,230,255,200):c%3==1?sf::Color(255,200,130,180):sf::Color(130,200,255,190);
                auto[hx,hy]=w2s(headX,headY,cam);
                safeRect(hx,hy,2,hc);
                safePx(hx,hy,sf::Color(255,255,255,(sf::Uint8)(hc.a/2)));
                int tailLen=12+c*3;
                for(int t=1;t<=tailLen;t++){
                    float tx=headX-std::cos(ang)*t*2.5f;
                    float ty=headY-std::sin(ang)*t*2.5f;
                    auto[px,py]=w2s(tx,ty,cam);
                    float fade=1.f-t/(float)tailLen;
                    safePx(px,py,sf::Color(hc.r,hc.g,hc.b,(sf::Uint8)(hc.a*fade*0.5f)));
                }
            }
            for(int i=0;i<20;i++){
                float sx2=((i*89+3)%80-40)*1.5f,sy2=((i*67+1)%60-30)*1.5f;
                auto[px,py]=w2s(sx2,sy2,cam);
                safePx(px,py,sf::Color(60,70,110,(sf::Uint8)(40+20*std::sin(ft*3+i))));
            }
        }
        // ===== 4: BLACK HOLE =====
        else if(sid==4){
            const Cam sh[]={{0.6f,0.8f,0,0,0},{1.2f,0.5f,0.3f,0,-5},
                {0.8f,0.2f,-0.1f,0,3},{1.5f,0.7f,0.6f,4,-8},{0.7f,0.6f,-0.4f,-3,2}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float spin=ft*0.8f;
            for(int ri=0;ri<40;ri++){
                float r=6.f+ri*1.4f;
                float t=ri/40.f;
                int pts=std::max(30,(int)(r*cam.sc*5.f));pts=std::min(pts,200);
                float heat=std::max(0.f,1.f-t);
                sf::Uint8 a=(sf::Uint8)(50+110*heat*heat);
                sf::Color dc;
                if(t<0.25f)dc=sf::Color(255,(sf::Uint8)(240-t*400),(sf::Uint8)(180-t*500),a);
                else if(t<0.55f)dc=sf::Color(255,(sf::Uint8)(140-t*100),(sf::Uint8)(60),a);
                else dc=sf::Color((sf::Uint8)(160-t*80),(sf::Uint8)(60+t*40),(sf::Uint8)(180+t*60),a);
                float twist=spin*(1.2f+3.f/(r*0.1f+1.f));
                for(int s=0;s<pts;s++){
                    float ag=s/(float)pts*TAU+twist;
                    float wobble=std::sin(ag*2+ft*1.5f)*0.6f;
                    auto[sx,sy]=w2s(std::cos(ag)*(r+wobble),std::sin(ag)*(r+wobble),cam);
                    safePx(sx,sy,dc);
                }
            }
            for(int ri=0;ri<4;ri++){
                float r=ri*1.5f+1.f;
                int pts=std::max(10,(int)(r*cam.sc*8));pts=std::min(pts,60);
                for(int s=0;s<pts;s++){float ag=s/(float)pts*TAU;
                    auto[sx,sy]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);
                    safePx(sx,sy,sf::Color(5,5,15,240));}
            }
            auto[cx2,cy2]=w2s(0,0,cam);safePx(cx2,cy2,sf::Color(0,0,0,255));
            for(int i=0;i<18;i++){
                float sa=i*TAU/18+spin*0.3f;
                float infall=std::fmod(ft*10+i*5.f,60.f);
                float sr=62.f;
                if(infall<sr){
                    float ir=sr-infall;
                    auto[ix,iy]=w2s(std::cos(sa)*ir,std::sin(sa)*ir,cam);
                    safePx(ix,iy,sf::Color(200,180,255,(sf::Uint8)(50*(ir/sr))));
                }
            }
        }
        // ===== 5: ASTEROID BELT =====
        else if(sid==5){
            const Cam sh[]={{0.5f,0.7f,0,0,0},{0.9f,0.4f,0.2f,6,-6},
                {0.6f,0.3f,-0.15f,-4,4},{1.1f,0.8f,0.6f,-2,-5},{0.7f,0.5f,-0.4f,5,3}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float drift=ft*0.15f;
            for(int i=0;i<100;i++){
                float orbitR=25.f+(i*31%50);
                float speed=0.3f/(orbitR*0.02f+0.5f);
                float ag=i*2.399f+drift*speed;
                float scatter=((i*17+3)%9-4)*1.5f;
                float wx=std::cos(ag)*(orbitR+scatter);
                float wy=std::sin(ag)*(orbitR+scatter);
                auto[sx,sy]=w2s(wx,wy,cam);
                int sz=(i%7==0)?3:(i%3==0)?2:1;
                sf::Uint8 a=(sf::Uint8)(80+40*std::sin(ft+i));
                sf::Color ac2=i%4==0?sf::Color(160,140,120,a):i%4==1?sf::Color(120,110,100,a):
                    i%4==2?sf::Color(140,130,110,a):sf::Color(100,95,85,a);
                if(sz==1)safePx(sx,sy,ac2);else safeRect(sx-sz/2.f,sy-sz/2.f,sz,ac2);
            }
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                auto[sx,sy]=w2s(dx*0.8f,dy*0.8f,cam);
                safePx(sx,sy,sf::Color(255,230,180,140));
            }
        }
        // ===== 6: BINARY STAR =====
        else if(sid==6){
            const Cam sh[]={{0.7f,0.8f,0,0,0},{1.3f,0.5f,0.25f,3,-6},
                {0.5f,0.3f,-0.15f,0,5},{0.9f,0.9f,0.5f,-5,-3},{1.1f,0.6f,-0.35f,4,2}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float orb=ft*0.4f;
            float sep=18.f;
            float s1x=std::cos(orb)*sep,s1y=std::sin(orb)*sep;
            float s2x=-s1x,s2y=-s1y;
            auto drawStar=[&](float sx2,float sy2,sf::Color core,sf::Color glow){
                for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
                    int md=std::abs(dx)+std::abs(dy);if(md>3)continue;
                    auto[px,py]=w2s(sx2+dx,sy2+dy,cam);
                    safePx(px,py,md<=1?core:glow);
                }
                for(int c=0;c<10;c++){
                    float ca=c*TAU/10;float cr=3.5f+std::sin(ft*10+c)*0.8f;
                    auto[px,py]=w2s(sx2+std::cos(ca)*cr,sy2+std::sin(ca)*cr,cam);
                    safePx(px,py,sf::Color(glow.r,glow.g,glow.b,(sf::Uint8)(40+20*std::sin(ft*8+c))));
                }
            };
            drawStar(s1x,s1y,sf::Color(255,200,120,200),sf::Color(255,170,80,90));
            drawStar(s2x,s2y,sf::Color(130,180,255,200),sf::Color(80,140,255,90));
            for(int i=0;i<40;i++){
                float t=i/40.f;
                float mx=s1x+(s2x-s1x)*t+std::sin(ft*3+i*0.5f)*4;
                float my=s1y+(s2y-s1y)*t+std::cos(ft*3+i*0.7f)*4;
                auto[px,py]=w2s(mx,my,cam);
                sf::Color mc=lerpColor(sf::Color(255,200,120,60),sf::Color(130,180,255,60),t);
                safePx(px,py,mc);
            }
            for(int i=0;i<20;i++){
                float ag=i*TAU/20+orb*0.1f;float r=35+std::sin(i*1.3f)*10;
                auto[px,py]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);
                sf::Color tc=i%2?sf::Color(255,210,150,35):sf::Color(150,190,255,35);
                safePx(px,py,tc);
            }
        }
        // ===== 7: CONSTELLATION MAP =====
        else if(sid==7){
            const Cam sh[]={{0.5f,0.9f,0,0,0},{0.8f,0.8f,0.15f,10,-8},
                {0.6f,0.85f,-0.1f,-12,5},{0.9f,0.7f,0.3f,5,8},{0.7f,0.95f,-0.2f,-8,-5}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            struct Star{float x,y;};struct Con{Star stars[6];int n;int lines[10];int nl;};
            const Con cons[]={
                {{{-30,-30},{-20,-35},{-10,-25},{-15,-15},{-25,-10},{-5,-10}},6,{0,1,1,2,2,3,3,4,4,5},10},
                {{{20,-20},{30,-25},{35,-15},{25,-10}},4,{0,1,1,2,2,3,3,0},8},
                {{{-35,15},{-25,10},{-15,15},{-5,10},{-15,25}},5,{0,1,1,2,2,3,1,4},8},
                {{{15,15},{25,10},{35,15},{25,25}},4,{0,1,1,2,2,3,3,0},8},
                {{{-20,35},{-10,30},{0,35},{10,30},{20,35}},5,{0,1,1,2,2,3,3,4},8},
            };
            for(auto& cn:cons){
                for(int i=0;i<cn.nl;i+=2){
                    auto& a2=cn.stars[cn.lines[i]];auto& b=cn.stars[cn.lines[i+1]];
                    int steps=20;
                    for(int s=0;s<=steps;s++){
                        float t=s/(float)steps;
                        auto[px,py]=w2s(a2.x+(b.x-a2.x)*t,a2.y+(b.y-a2.y)*t,cam);
                        safePx(px,py,sf::Color(60,80,120,40));
                    }
                }
                for(int i=0;i<cn.n;i++){
                    auto[px,py]=w2s(cn.stars[i].x,cn.stars[i].y,cam);
                    sf::Uint8 tw=(sf::Uint8)(140+60*std::sin(ft*3+cn.stars[i].x*0.2f));
                    safePx(px,py,sf::Color(220,230,255,tw));
                    safePx(px+1,py,sf::Color(220,230,255,(sf::Uint8)(tw/3)));
                    safePx(px,py+1,sf::Color(220,230,255,(sf::Uint8)(tw/3)));
                }
            }
        }
        // ===== 8: SUPERNOVA =====
        else if(sid==8){
            const Cam sh[]={{0.7f,0.8f,0,0,0},{1.4f,0.6f,0.2f,0,-4},
                {0.5f,0.5f,-0.3f,3,5},{1.0f,0.7f,0.5f,-4,-3},{0.8f,0.4f,-0.15f,2,2}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float cycle=std::fmod(sceneT,20.f);
            float phase;
            if(cycle<8.f)phase=0;
            else if(cycle<10.f)phase=(cycle-8.f)/2.f;
            else if(cycle<14.f)phase=1.f;
            else phase=std::max(0.f,1.f-(cycle-14.f)/6.f);
            if(phase<0.3f){
                float bright=0.3f+0.7f*(phase/0.3f);
                for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                    auto[sx,sy]=w2s(dx*1.f,dy*1.f,cam);
                    sf::Uint8 a=(sf::Uint8)(bright*180);
                    safePx(sx,sy,sf::Color(255,(sf::Uint8)(200*bright),(sf::Uint8)(120*bright),a));
                }
                sf::Uint8 pa=(sf::Uint8)(80*bright+40*std::sin(ft*8));
                for(int c=0;c<8;c++){float ca=c*TAU/8;
                    auto[sx,sy]=w2s(std::cos(ca)*3,std::sin(ca)*3,cam);
                    safePx(sx,sy,sf::Color(255,200,120,pa));}
            }
            if(phase>=0.3f){
                float ep=(phase-0.3f)/0.7f;
                float maxR=10+ep*55;
                for(int ri=0;ri<20;ri++){
                    float r=maxR*(ri/20.f);
                    float fade=1.f-ri/20.f;
                    int pts=std::max(12,(int)(r*cam.sc*3));pts=std::min(pts,80);
                    for(int s=0;s<pts;s++){
                        float ag=s/(float)pts*TAU+ri*0.3f;
                        auto[sx,sy]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);
                        sf::Uint8 a=(sf::Uint8)(fade*140*phase);
                        sf::Color ec;
                        if(fade>0.7f)ec=sf::Color(255,255,230,a);
                        else if(fade>0.4f)ec=sf::Color(255,(sf::Uint8)(150+60*fade),80,a);
                        else ec=sf::Color((sf::Uint8)(180*fade+80),60,(sf::Uint8)(180-100*fade),a);
                        safePx(sx,sy,ec);
                    }
                }
                float remnBr=std::max(0.f,1.f-ep*0.5f);
                auto[cx2,cy2]=w2s(0,0,cam);
                safePx(cx2,cy2,sf::Color(200,220,255,(sf::Uint8)(200*remnBr)));
            }
        }
        // ===== 9: RINGED PLANET =====
        else if(sid==9){
            const Cam sh[]={{0.8f,0.7f,0,0,0},{1.4f,0.5f,0.15f,4,-6},
                {0.6f,0.3f,-0.1f,0,4},{1.0f,0.8f,0.4f,-5,-3},{1.2f,0.6f,-0.25f,3,2}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            const sf::Color bands[]={{180,150,120},{200,170,130},{160,130,100},{190,160,125}};
            for(int dy=-8;dy<=8;dy++)for(int dx=-8;dx<=8;dx++){
                float d=std::sqrt((float)(dx*dx+dy*dy));if(d>8.5f)continue;
                auto[sx,sy]=w2s(dx*1.f,dy*1.f,cam);
                int bi=((dy+8)/4)%4;sf::Color bc=bands[bi];
                float shade=0.7f+0.3f*(dx+8)/16.f;
                bc.r=(sf::Uint8)(bc.r*shade);bc.g=(sf::Uint8)(bc.g*shade);bc.b=(sf::Uint8)(bc.b*shade);
                bc.a=(sf::Uint8)(d<7?180:140);safePx(sx,sy,bc);
            }
            for(int ri=-2;ri<=2;ri++){
                float rr=14.f+ri*1.5f;
                int pts=std::max(30,(int)(rr*cam.sc*4));pts=std::min(pts,140);
                sf::Uint8 ra=(sf::Uint8)(ri==0?100:60);
                sf::Color rc2=ri%2?sf::Color(180,160,130,ra):sf::Color(150,135,110,ra);
                for(int s=0;s<pts;s++){float ag=s/(float)pts*TAU;
                    float wx=std::cos(ag)*rr*1.6f,wy=std::sin(ag)*rr*0.35f;
                    if(wy>-2&&wy<2&&std::abs(wx)<8.5f)continue;
                    auto[sx,sy]=w2s(wx,wy,cam);safePx(sx,sy,rc2);}
            }
            float moonAg=ft*0.6f;float moonR=22.f;
            auto[mx,my]=w2s(std::cos(moonAg)*moonR,std::sin(moonAg)*moonR*0.5f,cam);
            safePx(mx,my,sf::Color(200,200,210,170));
            safePx(mx+1,my,sf::Color(200,200,210,100));
        }
        // ===== 10: SPACE JELLYFISH =====
        else if(sid==10){
            const Cam sh[]={{0.7f,0.85f,0,0,0},{1.2f,0.6f,0.2f,3,-5},
                {0.5f,0.7f,-0.3f,-4,6},{0.9f,0.9f,0.5f,-2,-4},{1.0f,0.5f,-0.2f,5,3}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float bob=std::sin(ft*0.3f)*15.f;
            float sway=std::sin(ft*0.2f)*10.f;
            float pulse=0.8f+0.2f*std::sin(ft*2.f);
            for(int ri=0;ri<=6;ri++){
                float r=ri*1.8f*pulse;
                float yOfs=-ri*0.5f;
                int pts=std::max(8,(int)(r*4));
                sf::Uint8 a=(sf::Uint8)(160-ri*18);
                float hue=std::fmod(ft*0.5f+ri*0.3f,TAU);
                sf::Color jc=lerpColor(sf::Color(180,80,220,a),sf::Color(80,200,220,a),
                    0.5f+0.5f*std::sin(hue));
                for(int s=0;s<pts;s++){
                    float ag=s/(float)pts*PI;
                    auto[sx,sy]=w2s(sway+std::cos(ag)*r,bob+yOfs+std::sin(ag)*r*0.6f,cam);
                    safePx(sx,sy,jc);
                }
            }
            for(int t=0;t<8;t++){
                float tx=(t-3.5f)*3.f;
                float wave=std::sin(ft*1.5f+t*0.8f)*5.f;
                for(int s=0;s<20;s++){
                    float ty=5.f+s*2.5f+wave*std::sin(s*0.4f);
                    float sx2=tx+std::sin(ft*1.2f+s*0.3f+t)*3.f;
                    auto[px,py]=w2s(sway+sx2,bob+ty,cam);
                    float fade=1.f-s/20.f;
                    sf::Uint8 a=(sf::Uint8)(100*fade*pulse);
                    float hue2=std::fmod(ft*0.5f+s*0.15f+t*0.4f,TAU);
                    sf::Color tc=lerpColor(sf::Color(180,80,220,a),sf::Color(80,200,220,a),
                        0.5f+0.5f*std::sin(hue2));
                    safePx(px,py,tc);
                }
            }
            for(int i=0;i<5;i++){
                float ox=sway+std::sin(ft*0.8f+i*1.5f)*4.f;
                float oy=bob-3.f+std::cos(ft*0.9f+i*1.2f)*2.f;
                auto[px,py]=w2s(ox,oy,cam);
                sf::Uint8 gla=(sf::Uint8)(120+60*std::sin(ft*3+i));
                safePx(px,py,sf::Color(255,255,255,gla));
            }
        }
        // ===== 11: WORMHOLE =====
        else if(sid==11){
            const Cam sh[]={{0.7f,0.9f,0,0,0},{1.3f,0.6f,0.3f,0,-5},
                {0.5f,0.4f,-0.2f,3,4},{1.0f,0.7f,0.6f,-4,-3},{0.8f,0.5f,-0.4f,2,3}};
            Cam cam=getCam(sh,5,sceneT,12.f);
            float spin2=ft*1.2f;
            for(int ri=1;ri<=16;ri++){
                float r=ri*3.5f;
                float twist=spin2+ri*0.4f;
                int pts=std::max(12,(int)(r*cam.sc*2.5f));pts=std::min(pts,80);
                float depth=ri/16.f;
                sf::Uint8 a=(sf::Uint8)(40+100*(1.f-depth));
                for(int s=0;s<pts;s++){
                    float ag=s/(float)pts*TAU+twist;
                    auto[sx,sy]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);
                    sf::Color wc;
                    if(depth<0.3f)wc=sf::Color(200,180,255,a);
                    else if(depth<0.6f)wc=sf::Color(100,140,230,(sf::Uint8)(a*0.8f));
                    else wc=sf::Color(60,80,160,(sf::Uint8)(a*0.6f));
                    safePx(sx,sy,wc);
                }
            }
            for(int ri=0;ri<=3;ri++){
                float r=ri*1.2f;
                int pts=std::max(6,(int)(r*8));
                sf::Uint8 a=(sf::Uint8)(200-ri*40);
                for(int s=0;s<pts;s++){float ag=s/(float)pts*TAU;
                    auto[sx,sy]=w2s(std::cos(ag)*r,std::sin(ag)*r,cam);
                    safePx(sx,sy,sf::Color(230,220,255,a));}
            }
            auto[cx3,cy3]=w2s(0,0,cam);safePx(cx3,cy3,sf::Color(255,255,255,220));
            for(int i=0;i<12;i++){
                float sa=i*TAU/12+spin2*0.5f;
                float infall=std::fmod(ft*20+i*7.f,70.f);
                float r2=60.f-infall;if(r2<2)continue;
                auto[sx,sy]=w2s(std::cos(sa)*r2,std::sin(sa)*r2,cam);
                sf::Uint8 sa2=(sf::Uint8)(80*(r2/60.f));
                safePx(sx,sy,sf::Color(200,210,255,sa2));
            }
        }
    }
    void drawGrid(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        for(int i=1;i<ROW;i++){sf::RectangleShape ln({(float)LW,1.f});
            ln.setPosition(HB_W,(float)(i*CELL));ln.setFillColor(sf::Color(40,44,64,40));rt.draw(ln);}
        for(int j=1;j<COL;j++){sf::RectangleShape ln({1.f,(float)LH});
            ln.setPosition(HB_W+(float)(j*CELL),0);ln.setFillColor(sf::Color(40,44,64,40));rt.draw(ln);}
    }
    void drawBorder(sf::RenderTarget& rt,int frame){
        const int HB_W=fieldX;
        static sf::RectangleShape b;b.setFillColor(sf::Color::Transparent);
        float dang=dangerLevel();
        if(dang>0.3f&&phase==Playing){
            float pulse=0.5f+0.5f*std::sin(frame*0.15f*(1.f+dang*2.f));
            sf::Uint8 ra=(sf::Uint8)((dang-0.3f)/0.7f*pulse*180);
            b.setOutlineColor(sf::Color(255,60,60,ra));b.setOutlineThickness(2);
            b.setSize({(float)(LW-4),(float)(LH-4)});b.setPosition(HB_W+2,2);rt.draw(b);
        }
        b.setOutlineColor(sf::Color(200,210,255,100));b.setOutlineThickness(2);
        b.setSize({(float)(LW-4),(float)(LH-4)});b.setPosition(HB_W+2,2);rt.draw(b);
        b.setOutlineColor(sf::Color(60,70,110,200));b.setOutlineThickness(1);
        b.setSize({(float)LW,(float)LH});b.setPosition(HB_W,0);rt.draw(b);
    }

    bool isFlashRow(int row)const{
        return phase==LineFlash&&std::find(flashRows.begin(),flashRows.end(),row)!=flashRows.end();}

    void drawField(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        float ft=(FLASH_TOTAL>0)?1.f-flashFrames/(float)FLASH_TOTAL:1.f;
        for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++){
            int id=grid[i][j];if(!id)continue;
            float gx=HB_W+(float)(j*CELL),gy=(float)(i*CELL);
            if(isFlashRow(i)){
                auto& p=PAL[id-1];
                float bright=0,alpha=1.f;
                if(ft<0.4f)bright=ft/0.4f;
                else if(ft<0.7f)bright=1.f;
                else{bright=1.f;alpha=1.f-(ft-0.7f)/0.3f;}
                sf::Uint8 r2=(sf::Uint8)std::min(255.f,p.r+(255-p.r)*bright);
                sf::Uint8 g2=(sf::Uint8)std::min(255.f,p.g+(255-p.g)*bright);
                sf::Uint8 b2=(sf::Uint8)std::min(255.f,p.b+(255-p.b)*bright);
                sf::Uint8 a2=(sf::Uint8)(alpha*255);
                static sf::RectangleShape bl;bl.setOutlineThickness(0);
                bl.setSize({(float)CELL,(float)CELL});bl.setPosition(gx,gy);
                bl.setFillColor(sf::Color(r2,g2,b2,a2));
                bl.setOutlineThickness(-1.f);
                bl.setOutlineColor(sf::Color(255,255,255,(sf::Uint8)(bright*alpha*120)));
                rt.draw(bl);bl.setOutlineThickness(0);
            } else {
                sf::Color col=getCellColor(id-1,i,j);
                float sp=cellSpin[i][j];
                if(sp>0&&sp<=SPIN_DUR){
                    float angle=(sp/SPIN_DUR)*PI;
                    drawCellSpin(rt,gx,gy,col,angle,cellAxis[i][j]);
                } else {
                    drawCellC(rt,gx,gy,col);
                }
            }
        }
    }

    void drawBloom(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        if(phase!=LineFlash||flashRows.empty())return;
        float t=1.f-flashFrames/(float)FLASH_TOTAL;
        float glow=0;
        if(t<0.3f)glow=t/0.3f; else if(t<0.6f)glow=1.f; else glow=1.f-(t-0.6f)/0.4f;
        if(glow<=0)return;
        float spread=std::min(t*2.5f,1.f)*CELL*2.5f;
        static sf::RectangleShape gl;gl.setOutlineThickness(0);
        for(int row:flashRows){
            float cy=row*CELL+CELL*0.5f;
            gl.setSize({(float)LW,(float)CELL});gl.setPosition(HB_W,(float)(row*CELL));
            gl.setFillColor(sf::Color(255,255,255,(sf::Uint8)(glow*180)));
            rt.draw(gl,sf::BlendAdd);
            for(int L=1;L<=3;L++){
                float h=spread*L/3; sf::Uint8 a=(sf::Uint8)(glow*90.f/L);if(a<2)continue;
                gl.setSize({(float)LW,h});gl.setFillColor(sf::Color(200,220,255,a));
                float yA=cy-CELL*0.5f-h;
                if(yA>=0){gl.setPosition(HB_W,yA);rt.draw(gl,sf::BlendAdd);}
                float yB=cy+CELL*0.5f;
                if(yB+h<=(float)LH){gl.setPosition(HB_W,yB);rt.draw(gl,sf::BlendAdd);}
            }
        }
    }

    void drawDropTrail(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        if(dropTrailTimer<=0)return;
        float t=dropTrailTimer/DROP_TRAIL_DUR;
        // t goes 1→0 as trail fades
        auto& p=PAL[trailColor];
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(trailPiece[i][j]){
            int col=trailPx+j;
            // Find the topmost row this column's block occupied
            int topY=trailStartY+i;
            int botY=trailEndY+i;
            if(topY<0)topY=0;
            if(botY<0)continue;
            float x=HB_W+(float)(col*CELL);
            float y0=(float)(topY*CELL);
            float y1=(float)(botY*CELL+CELL);
            float trailH=y1-y0;
            if(trailH<=0)continue;

            // Bright core streak (narrow, centered in cell)
            static sf::RectangleShape streak;streak.setOutlineThickness(0);
            float coreW=std::max(2.f,CELL*0.4f*t);
            float coreX=x+(CELL-coreW)*0.5f;
            sf::Uint8 coreA=(sf::Uint8)(t*220);
            streak.setSize({coreW,trailH});
            streak.setPosition(coreX,y0);
            streak.setFillColor(sf::Color(255,255,255,coreA));
            rt.draw(streak,sf::BlendAdd);

            // Wider soft glow around it
            float glowW=CELL*0.9f*t;
            float glowX=x+(CELL-glowW)*0.5f;
            sf::Uint8 glowA=(sf::Uint8)(t*80);
            streak.setSize({glowW,trailH});
            streak.setPosition(glowX,y0);
            streak.setFillColor(sf::Color(p.r,p.g,p.b,glowA));
            rt.draw(streak,sf::BlendAdd);

            // Hot white flash at landing point
            sf::Uint8 landA=(sf::Uint8)(t*t*255);
            streak.setSize({(float)CELL,(float)CELL});
            streak.setPosition(x,(float)(botY*CELL));
            streak.setFillColor(sf::Color(255,255,255,landA));
            rt.draw(streak,sf::BlendAdd);
        }
    }

    void drawGhost(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        if(phase!=Playing)return;
        int gy=ghostY();if(gy==py)return;
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(piece[i][j]){
            int gx2=px+j,gyy=gy+i;if(gyy<0)continue;
            auto& p=PAL[pieceId];
            static sf::RectangleShape r;
            r.setSize({(float)(CELL-2),(float)(CELL-2)});
            r.setPosition(HB_W+(float)(gx2*CELL+1),(float)(gyy*CELL+1));
            r.setFillColor(sf::Color(p.r,p.g,p.b,30));
            r.setOutlineThickness(1.f);r.setOutlineColor(sf::Color(p.r,p.g,p.b,60));
            rt.draw(r);}
    }

    void drawActive(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        if(phase!=Playing)return;
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(piece[i][j]){
            int nx=px+j,ny=py+i;
            if(ny>=0)drawCell(rt,HB_W+nx*CELL+rubX,ny*CELL+rubY,pieceId);
        }
    }

    void drawHoldBox(sf::RenderTarget& rt){
        int hx=fieldX-HB_W+2; int y=2;
        drawText(rt,hx,y,"HOLD",sf::Color(180,190,220),1);y+=8;
        if(holdId>=0){
            drawMiniPiece(rt,hx,y,holdId,4,holdUsed?90:255);
        }
    }
    void drawSidebar(sf::RenderTarget& rt){
        if(vsMode)return;
        const int sx=fieldX+LW+4; int y=2;
        drawText(rt,sx,y,"PIXEL",sf::Color(255,228,150),2);y+=11;
        drawText(rt,sx,y,"TRIS",sf::Color(120,200,255),2);y+=14;
        auto stat=[&](const char* lab,int val,const sf::Color& nc){
            drawText(rt,sx,y,lab,sf::Color(180,190,220),1);y+=6;
            drawNumber(rt,sx,y,val,nc,2);y+=11;};
        stat("SCORE",score,sf::Color(255,255,255));
        stat("LEVEL",level,sf::Color(160,255,200));
        stat("LINES",linesTotal,sf::Color(255,200,160));
        drawText(rt,sx,y,"NEXT",sf::Color(180,190,220),1);y+=7;
        drawMiniPiece(rt,sx+2,y,nextId,5);y+=14;
        const sf::Color h(80,90,120);
        drawText(rt,sx,y,"ARROWS",h,1);y+=6;
        drawText(rt,sx,y,"UP ROT",h,1);y+=6;
        drawText(rt,sx,y,"SPC DROP",h,1);y+=6;
        drawText(rt,sx,y,"TAB HOLD",h,1);y+=6;
        drawText(rt,sx,y,"ESC MENU",h,1);y+=6;
        drawText(rt,sx,y,"F FULL",h,1);y+=6;
        drawText(rt,sx,y,"M MUTE",h,1);
    }

    int pauseSel=0;
    void drawPausedOverlay(sf::RenderTarget& rt,int frame,float rubY=0){
        const int TEX_W=texW;
        sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
        dim.setFillColor(sf::Color(0,0,0,160));rt.draw(dim);
        const sf::Color cyan(80,220,255),wh(220,230,255);
        int cx=TEX_W/2;
        int cy=LH/2-28;
        drawTextCentered(rt,cx,cy,"PAUSED",sf::Color(255,240,180),3);cy+=22;
        const char* items[]={"RESUME","RESTART","QUIT TO MENU"};
        for(int i=0;i<3;i++){
            bool isSel=(i==pauseSel);
            int iy=cy+(isSel?(int)std::round(rubY):0);
            if(isSel){
                char line[32];snprintf(line,sizeof line,"- %s -",items[i]);
                drawTextRainbowCentered(rt,cx,iy,line,frame,1);
            }else{
                drawTextCentered(rt,cx,cy,items[i],wh,1);
            }
            cy+=10;
        }
        if((frame/30)%2)drawTextCentered(rt,cx,cy+4,"_",cyan,1);
    }
    void drawActionTexts(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        int cy=LH/2-20;
        for(int i=0;i<actionCount;i++){
            auto& a=actionTexts[i];
            float t=a.timer/1.5f;
            sf::Uint8 al=(sf::Uint8)(t>0.7f?255:t/0.7f*255);
            float rise=(1.f-t)*12.f;
            sf::Color c=a.color;c.a=al;
            drawTextCentered(rt,HB_W+LW/2,(int)(cy-rise-i*12),a.text,c,2);
        }
    }
    void drawSprintHud(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        if(gameMode!=GameMode::Sprint)return;
        int remaining=std::max(0,SPRINT_TARGET-linesTotal);
        char buf[32];
        snprintf(buf,sizeof buf,"%d LEFT",remaining);
        drawTextCentered(rt,HB_W+LW/2,2,buf,sf::Color(255,200,80),1);
        int mins=(int)sprintTimer/60,secs=(int)sprintTimer%60,ms=(int)(std::fmod(sprintTimer,1.f)*100);
        snprintf(buf,sizeof buf,"%d:%02d.%02d",mins,secs,ms);
        drawTextCentered(rt,HB_W+LW/2,9,buf,sf::Color(200,220,255),1);
    }
    void drawGameOverOverlay(sf::RenderTarget& rt){
        const int TEX_W=texW;
        sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
        dim.setFillColor(sf::Color(20,10,30,210));rt.draw(dim);
        int cy=12;
        if(gameMode==GameMode::Sprint&&linesTotal>=SPRINT_TARGET){
            drawTextCentered(rt,TEX_W/2,cy,"SPRINT CLEAR!",sf::Color(120,255,200),3);cy+=16;
            char buf[32];int mins=(int)sprintTimer/60,secs=(int)sprintTimer%60,ms=(int)(std::fmod(sprintTimer,1.f)*100);
            snprintf(buf,sizeof buf,"TIME %d:%02d.%02d",mins,secs,ms);
            drawTextCentered(rt,TEX_W/2,cy,buf,sf::Color(255,240,180),2);cy+=14;
        }else{
            drawTextCentered(rt,TEX_W/2,cy,"GAME OVER",sf::Color(255,120,140),3);cy+=16;
        }
        auto stat=[&](const char* lab,int val,sf::Color c){
            char buf[32];snprintf(buf,sizeof buf,"%s %d",lab,val);
            drawTextCentered(rt,TEX_W/2,cy,buf,c,1);cy+=8;};
        stat("SCORE",score,sf::Color(255,255,255));
        stat("LINES",linesTotal,sf::Color(255,200,160));
        stat("LEVEL",level,sf::Color(160,255,200));
        stat("PIECES",piecesPlaced,sf::Color(180,200,255));
        if(tetrisCount)stat("TETRISES",tetrisCount,sf::Color(120,255,200));
        if(tSpinCount)stat("T-SPINS",tSpinCount,sf::Color(200,120,255));
        if(maxCombo>0){char buf[32];snprintf(buf,sizeof buf,"MAX COMBO %dX",maxCombo+1);
            drawTextCentered(rt,TEX_W/2,cy,buf,sf::Color(255,230,80),1);cy+=8;}
        if(perfectClears)stat("PERFECTS",perfectClears,sf::Color(255,255,255));
        int mins=(int)playTime/60,secs=(int)playTime%60;
        char tb[32];snprintf(tb,sizeof tb,"TIME %d:%02d",mins,secs);
        drawTextCentered(rt,TEX_W/2,cy,tb,sf::Color(180,190,220),1);cy+=12;
        if(hiRank>=0&&hiRank<3){
            char rb[32];snprintf(rb,sizeof rb,"NEW #%d SCORE!",hiRank+1);
            drawTextCentered(rt,TEX_W/2,cy,rb,sf::Color(255,230,80),2);cy+=12;
        }else if(nextHiScore>0){
            int diff=nextHiScore-score;
            char nb[48];snprintf(nb,sizeof nb,"%d TO NEXT RANK",diff);
            drawTextCentered(rt,TEX_W/2,cy,nb,sf::Color(180,190,220),1);cy+=8;
        }
        int nextLvlLines=(level+1)*10-linesTotal;
        if(nextLvlLines>0&&nextLvlLines<=5&&!(gameMode==GameMode::Sprint&&linesTotal>=SPRINT_TARGET)){
            char lb[48];snprintf(lb,sizeof lb,"%d MORE FOR LV %d...",nextLvlLines,level+1);
            drawTextCentered(rt,TEX_W/2,cy,lb,sf::Color(160,170,200),1);cy+=10;
        }else cy+=2;
        drawTextCentered(rt,TEX_W/2,cy,"R RESTART",sf::Color(200,220,255),2);cy+=10;
        drawTextCentered(rt,TEX_W/2,cy,"ESC MENU",sf::Color(160,170,200),1);
    }

    void drawLevelUpFlash(sf::RenderTarget& rt){
        const int HB_W=fieldX;
        static sf::RectangleShape gl;gl.setOutlineThickness(0);
        gl.setSize({(float)LW,(float)LH});gl.setPosition(HB_W,0);
        if(lvlUpTimer>0){
            float lt=lvlUpTimer/LVL_UP_DUR;
            float intensity=(lt>0.7f)?(1.f-lt)/0.3f:(lt<0.15f)?lt/0.15f:1.f;
            sf::Color rc=hueToRGB((1.f-lt)*3.f);
            gl.setFillColor(sf::Color(rc.r,rc.g,rc.b,(sf::Uint8)(intensity*40)));
            rt.draw(gl,sf::BlendAdd);
        }
        if(clearFxTimer>0){
            float lt=clearFxTimer/CLEAR_FX_DUR;
            float intensity=(lt>0.7f)?(1.f-lt)/0.3f:(lt<0.15f)?lt/0.15f:1.f;
            sf::Color fc;
            if(clearFxType==2)fc=sf::Color(255,230,60);
            else if(clearFxType==3){
                float t=0.5f+0.5f*std::sin((1.f-lt)*6.f);
                if(t<0.33f)fc=lerpColor(sf::Color(40,80,255),sf::Color(255,230,60),t/0.33f);
                else if(t<0.66f)fc=lerpColor(sf::Color(255,230,60),sf::Color(255,80,30),(t-0.33f)/0.33f);
                else fc=lerpColor(sf::Color(255,80,30),sf::Color(40,80,255),(t-0.66f)/0.34f);
            }else fc=hueToRGB((1.f-lt)*3.f);
            gl.setFillColor(sf::Color(fc.r,fc.g,fc.b,(sf::Uint8)(intensity*35)));
            rt.draw(gl,sf::BlendAdd);
        }
    }
};

struct CpuAI {
    int diff=1; // 0=easy,1=medium,2=hard
    float thinkTimer=0;
    float moveTimer=0;
    int targetX=0,targetRot=0;
    bool decided=false,executing=false,useHold=false;
    int movePhase=0,currentRot=0;
    std::mt19937 rng{(unsigned)time(nullptr)+99};

    float thinkDelay()const{
        switch(diff){case 0:return 0.7f;case 1:return 0.3f;default:return 0.1f;}
    }
    float moveDelay()const{
        switch(diff){case 0:return 0.09f;case 1:return 0.045f;default:return 0.022f;}
    }

    float evalBoard(const int g[ROW][COL]){
        int heights[COL]{};
        for(int j=0;j<COL;j++)for(int i=0;i<ROW;i++)if(g[i][j]){heights[j]=ROW-i;break;}
        int aggH=0;for(int j=0;j<COL;j++)aggH+=heights[j];
        int lines=0;
        for(int i=0;i<ROW;i++){bool full=true;for(int j=0;j<COL;j++)if(!g[i][j]){full=false;break;}if(full)lines++;}
        int holes=0;
        for(int j=0;j<COL;j++){bool f=false;for(int i=0;i<ROW;i++){if(g[i][j])f=true;else if(f)holes++;}}
        int bump=0;
        for(int j=0;j<COL-1;j++)bump+=std::abs(heights[j]-heights[j+1]);
        int coveredHoles=0;
        for(int j=0;j<COL;j++){int ceiling=-1;for(int i=0;i<ROW;i++){
            if(g[i][j]&&ceiling<0)ceiling=i;
            else if(!g[i][j]&&ceiling>=0)coveredHoles+=(i-ceiling);}}
        float extra=0;
        if(diff>=2){
            int wellCol=COL-1;
            int wd=heights[wellCol-1]-heights[wellCol];
            if(wd>0&&heights[wellCol]<=2)extra+=std::min(wd,4)*1.2f;
            else if(wd<0)extra+=wd*0.5f;
            int maxH=0;for(int j2=0;j2<COL;j2++)maxH=std::max(maxH,heights[j2]);
            if(maxH>14)extra-=(maxH-14)*(maxH-14)*1.5f;
            if(lines==4)extra+=12.f;
        }
        float wH,wL,wHo,wB,wCov;
        switch(diff){
            case 0:wH=-0.3f;wL=4.0f;wHo=-2.5f;wB=-0.2f;wCov=-0.5f;break;
            case 1:wH=-0.51f;wL=7.6f;wHo=-5.0f;wB=-0.35f;wCov=-1.5f;break;
            default:wH=-0.51f;wL=7.6f;wHo=-7.5f;wB=-0.35f;wCov=-3.0f;break;
        }
        return wH*aggH+wL*lines+wHo*holes+wB*bump+wCov*coveredHoles+extra;
    }

    float evalPiece(const Game& g,int pid,int& bestX,int& bestRot){
        float best=-1e9f;bestX=g.px;bestRot=0;
        int maxRot=(pid==3)?1:(pid==0)?2:4;
        int orig[4][4];copyMat(orig,SHAPES[pid].m);
        for(int rot=0;rot<maxRot;rot++){
            int tp[4][4];copyMat(tp,orig);
            for(int r=0;r<rot;r++){int t2[4][4];rotateCW(t2,tp);copyMat(tp,t2);}
            for(int x=-2;x<=COL;x++){
                if(g.collides(x,0,tp)&&g.collides(x,-1,tp))continue;
                int y=std::max(0,g.py);
                if(g.collides(x,y,tp)){y=-1;if(g.collides(x,y,tp))continue;}
                while(!g.collides(x,y+1,tp))y++;
                int tmp[ROW][COL];
                for(int i=0;i<ROW;i++)for(int j=0;j<COL;j++)tmp[i][j]=g.grid[i][j];
                bool valid=true;
                for(int i=0;i<4;i++)for(int j=0;j<4;j++)if(tp[i][j]){
                    int ni=y+i,nj=x+j;
                    if(ni<0||ni>=ROW||nj<0||nj>=COL){valid=false;break;}
                    tmp[ni][nj]=pid+1;
                }
                if(!valid)continue;
                float sc=evalBoard(tmp);
                float noise=0;
                if(diff==0)noise=((int)(rng()%1000)-500)*0.008f;
                else if(diff==1)noise=((int)(rng()%200)-100)*0.005f;
                if(sc+noise>best){best=sc+noise;bestX=x;bestRot=rot;}
            }
        }
        return best;
    }

    void decide(Game& g){
        int bx,br;float bs=evalPiece(g,g.pieceId,bx,br);
        useHold=false;
        if(!g.holdUsed){
            float threshold=diff>=2?0.5f:diff>=1?2.0f:999.f;
            if(threshold<100.f){
                int altId=g.holdId>=0?g.holdId:g.nextId;
                int ax,ar;float as2=evalPiece(g,altId,ax,ar);
                if(as2>bs+threshold){bs=as2;bx=ax;br=ar;useHold=true;}
            }
        }
        targetX=bx;targetRot=br;
        decided=true;executing=true;
        movePhase=useHold?-1:0;currentRot=0;moveTimer=0;
    }

    void update(Game& g,float dt){
        if(g.phase!=Game::Playing)return;
        if(!decided){
            thinkTimer+=dt;
            if(thinkTimer>=thinkDelay()){decide(g);thinkTimer=0;}
            return;
        }
        if(!executing)return;
        moveTimer+=dt;
        float md=moveDelay();
        if(moveTimer<md)return;
        moveTimer-=md;
        if(movePhase==-1){g.holdPiece();movePhase=0;return;}
        if(movePhase==0){
            if(currentRot<targetRot){g.rotatePiece();currentRot++;return;}
            movePhase=1;
        }
        if(movePhase==1){
            if(g.px<targetX){g.tryShift(1);return;}
            else if(g.px>targetX){g.tryShift(-1);return;}
            movePhase=2;
        }
        if(movePhase==2){g.hardDrop();decided=false;executing=false;thinkTimer=0;}
    }

    void reset(){decided=false;executing=false;thinkTimer=0;moveTimer=0;currentRot=0;useHold=false;}
};

void drawVsPanel(sf::RenderTarget& rt,Game& g,int px,bool /*isRight*/){
    int y=2;
    drawText(rt,px,y,"HOLD",sf::Color(180,190,220),1);y+=8;
    if(g.holdId>=0)g.drawMiniPiece(rt,px,y,g.holdId,4,g.holdUsed?90:255);
    y+=14;
    drawText(rt,px,y,"NEXT",sf::Color(180,190,220),1);y+=7;
    g.drawMiniPiece(rt,px+2,y,g.nextId,4);y+=14;
    char buf[24];
    drawText(rt,px,y,"SCORE",sf::Color(180,190,220),1);y+=6;
    snprintf(buf,sizeof buf,"%d",g.score);drawText(rt,px,y,buf,sf::Color(255,255,255),1);y+=8;
    drawText(rt,px,y,"LEVEL",sf::Color(180,190,220),1);y+=6;
    snprintf(buf,sizeof buf,"%d",g.level);drawText(rt,px,y,buf,sf::Color(160,255,200),1);y+=8;
    drawText(rt,px,y,"LINES",sf::Color(180,190,220),1);y+=6;
    snprintf(buf,sizeof buf,"%d",g.linesTotal);drawText(rt,px,y,buf,sf::Color(255,200,160),1);y+=10;
    if(g.garbagePending>0){
        snprintf(buf,sizeof buf,"%dG",g.garbagePending);
        drawText(rt,px,y,buf,sf::Color(255,80,80),1);y+=8;
    }
}

void drawVsDivider(sf::RenderTarget& rt,int frame){
    int cx=VS_P1_FX+LW+VS_GAP/2;
    int y=2;
    drawTextCentered(rt,cx,y,"VS",sf::Color(255,80,100),2);y+=14;
    sf::RectangleShape line;
    line.setSize({1.f,(float)LH-4});line.setPosition((float)(VS_P1_FX+LW+VS_GAP/2),2);
    line.setFillColor(sf::Color(80,90,130,(sf::Uint8)(60+30*std::sin(frame*0.05f))));
    rt.draw(line);
}

void drawVsOverlay(sf::RenderTarget& rt,bool playerWon,Game& pg,Game& cg,int /*frame*/){
    sf::RectangleShape dim;dim.setSize({(float)VS_TEX_W,(float)TEX_H});
    dim.setFillColor(sf::Color(20,10,30,210));rt.draw(dim);
    int cy=TEX_H/2-30;
    if(playerWon){
        drawTextCentered(rt,VS_TEX_W/2,cy,"YOU WIN!",sf::Color(120,255,200),3);
    }else{
        drawTextCentered(rt,VS_TEX_W/2,cy,"YOU LOSE!",sf::Color(255,80,100),3);
    }
    cy+=20;
    char buf[48];
    snprintf(buf,sizeof buf,"YOU %d  CPU %d",pg.score,cg.score);
    drawTextCentered(rt,VS_TEX_W/2,cy,buf,sf::Color(220,230,255),1);cy+=10;
    snprintf(buf,sizeof buf,"LINES %d  LINES %d",pg.linesTotal,cg.linesTotal);
    drawTextCentered(rt,VS_TEX_W/2,cy,buf,sf::Color(180,190,220),1);cy+=14;
    drawTextCentered(rt,VS_TEX_W/2,cy,"R RESTART",sf::Color(200,220,255),2);cy+=10;
    drawTextCentered(rt,VS_TEX_W/2,cy,"ESC MENU",sf::Color(160,170,200),1);
}

void drawMenu(sf::RenderTarget& rt,int sel,int frame,const HiScoreTable& hi,Game& bgGame,float rubY=0){
    bgGame.drawBackground(rt,frame);

    sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
    dim.setFillColor(sf::Color(10,12,25,180));rt.draw(dim);

    const sf::Color cyan(80,220,255),wh(220,230,255),dim2(90,100,130),gold(255,230,80);
    int cx=TEX_W/2;
    int y=6;
    drawTextCentered(rt,cx,y,"PIXEL TRIS",gold,3);y+=20;

    const char* items[]={"MARATHON","SPRINT 40L","VS CPU","HIGH SCORES","CONTROLS","QUIT"};
    const int nItems=6;
    for(int i=0;i<nItems;i++){
        bool isSel=(i==sel);
        int iy=y+(isSel?(int)std::round(rubY):0);
        if(isSel){
            char line[48];snprintf(line,sizeof line,"- %s -",items[i]);
            drawTextRainbowCentered(rt,cx,iy,line,frame,1);
        }else{
            drawTextCentered(rt,cx,y,items[i],wh,1);
        }
        y+=9;
    }
    y+=3;

    drawTextCentered(rt,cx,y,"------------",dim2,1);y+=8;

    drawTextCentered(rt,cx,y,"TOP SCORES",cyan,1);y+=7;
    for(int i=0;i<hi.count&&i<3;i++){
        char buf[48];
        snprintf(buf,sizeof buf,"%d. %d L%d %c",i+1,hi.entries[i].score,hi.entries[i].level,hi.entries[i].mode);
        drawTextCentered(rt,cx,y,buf,i==0?gold:dim2,1);y+=7;
    }
    if(hi.count==0)drawTextCentered(rt,cx,y,"NO SCORES YET",dim2,1);
    y+=4;
    drawTextCentered(rt,cx,y,"------------",dim2,1);y+=8;

    if((frame/30)%2)drawTextCentered(rt,cx,y,"_",cyan,1);

    drawTextCentered(rt,cx,LH-8,"F FULLSCREEN  M MUTE",sf::Color(60,70,100),1);
}

void drawVsDiffScreen(sf::RenderTarget& rt,int sel,int frame,Game& bgGame,float rubY=0){
    bgGame.drawBackground(rt,frame);
    sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
    dim.setFillColor(sf::Color(10,12,25,190));rt.draw(dim);
    const sf::Color cyan(80,220,255),dim2(90,100,130),gold(255,230,80);
    const sf::Color diffC[]={sf::Color(120,255,180),sf::Color(255,230,80),sf::Color(255,80,100)};
    const char* desc[]={"RELAXED PACE","SMART PLAY","NO MERCY"};
    int cx=TEX_W/2;
    int y=14;
    drawTextCentered(rt,cx,y,"VS CPU",gold,3);y+=22;
    drawTextCentered(rt,cx,y,"SELECT DIFFICULTY",cyan,1);y+=16;
    const char* items[]={"EASY","MEDIUM","HARD"};
    for(int i=0;i<3;i++){
        bool isSel=(i==sel);
        int iy=y+(isSel?(int)std::round(rubY):0);
        if(isSel){
            char line[32];snprintf(line,sizeof line,"- %s -",items[i]);
            drawTextRainbowCentered(rt,cx,iy,line,frame,2);
            drawTextCentered(rt,cx,iy+14,desc[i],dim2,1);
        }else{
            drawTextCentered(rt,cx,y,items[i],diffC[i],2);
        }
        y+=22;
    }
    y+=2;
    if((frame/30)%2)drawTextCentered(rt,cx,y,"_",cyan,1);
    drawTextCentered(rt,cx,LH-8,"ESC BACK",dim2,1);
}

void drawControlsScreen(sf::RenderTarget& rt,int frame,Game& bgGame){
    bgGame.drawBackground(rt,frame);
    sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
    dim.setFillColor(sf::Color(10,12,25,190));rt.draw(dim);

    const sf::Color cyan(80,220,255),wh(220,230,255),dim2(100,110,140);
    int cx=TEX_W/2;
    int y=10;
    drawTextCentered(rt,cx,y,"CONTROLS",cyan,2);y+=16;

    const char* keys[]={"LEFT RIGHT","DOWN","UP Z X","SPACE","TAB","P","M","F","ESC"};
    const char* acts[]={"MOVE","SOFT DROP","ROTATE","HARD DROP","HOLD","PAUSE","MUTE","FULLSCREEN","QUIT"};
    int colK=8,colA=TEX_W/2+4;
    for(int i=0;i<9;i++){
        drawText(rt,colK,y,keys[i],cyan,1);
        drawText(rt,colA,y,acts[i],wh,1);
        y+=8;
    }
    y+=6;
    drawTextCentered(rt,cx,y,"ESC BACK",dim2,1);
}

void drawHiScoresScreen(sf::RenderTarget& rt,int frame,const HiScoreTable& hi,Game& bgGame){
    bgGame.drawBackground(rt,frame);
    sf::RectangleShape dim;dim.setSize({(float)TEX_W,(float)TEX_H});
    dim.setFillColor(sf::Color(10,12,25,190));rt.draw(dim);

    const sf::Color cyan(80,220,255),gold(255,230,80),wh(220,230,255),dim2(100,110,140);
    int cx=TEX_W/2;
    int y=10;
    drawTextCentered(rt,cx,y,"HIGH SCORES",cyan,2);y+=18;
    for(int i=0;i<hi.count;i++){
        char buf[64];
        const char* modeStr=hi.entries[i].mode=='S'?"SPR":"MAR";
        snprintf(buf,sizeof buf,"%d. %6d  LV%d  %dL  %s",i+1,hi.entries[i].score,
            hi.entries[i].level,hi.entries[i].lines,modeStr);
        drawTextCentered(rt,cx,y,buf,i==0?gold:wh,1);y+=9;
    }
    if(hi.count==0){drawTextCentered(rt,cx,y,"NO SCORES YET",dim2,1);y+=9;}
    y+=6;
    drawTextCentered(rt,cx,y,"ESC BACK",dim2,1);
}

int main(){
    const float soloW=(float)(TEX_W*WIN_SCALE),soloH=(float)(TEX_H*WIN_SCALE);
    const float vsW=(float)(VS_TEX_W*WIN_SCALE),vsH=(float)(TEX_H*WIN_SCALE);
    float curW=soloW,curH=soloH;
    sf::Vector2u winSz((unsigned)curW,(unsigned)curH);
    sf::RenderWindow window(sf::VideoMode(winSz.x,winSz.y),"Pixel Tris");
    window.setFramerateLimit(60);

    sf::RenderTexture tex,vsTex;
    if(!tex.create(TEX_W,TEX_H))return 1;
    tex.setSmooth(false);
    if(!vsTex.create(VS_TEX_W,TEX_H))return 1;
    vsTex.setSmooth(false);

    sf::Sprite upscale(tex.getTexture());
    upscale.setScale((float)WIN_SCALE,(float)WIN_SCALE);
    sf::Sprite vsUpscale(vsTex.getTexture());
    vsUpscale.setScale((float)WIN_SCALE,(float)WIN_SCALE);

    ArcadeSfx audio;
    Game game;game.sfx=&audio;
    Game cpuGame;cpuGame.sfx=nullptr;
    CpuAI cpuAi;
    bool fullscreen=false;
    applyLetterboxView(window,curW,curH);

    const char* hiPath="pixeltris_scores.dat";
    HiScoreTable hiScores;
    hiScores.load(hiPath);

    AppState appState=AppState::Menu;
    int menuSel=0;
    enum class MenuSub{Main,HiScores,Controls,VsDiff} menuSub=MenuSub::Main;
    int vsDiffSel=1;

    auto resizeWin=[&](bool vs){
        curW=vs?vsW:soloW;curH=vs?vsH:soloH;
        winSz={(unsigned)curW,(unsigned)curH};
        if(!fullscreen)window.create(sf::VideoMode(winSz.x,winSz.y),"Pixel Tris",sf::Style::Default);
        else window.create(sf::VideoMode::getDesktopMode(),"Pixel Tris",sf::Style::Fullscreen);
        window.setFramerateLimit(60);applyLetterboxView(window,curW,curH);
    };

    sf::Clock clk;
    float dropAccum=0.f,cpuDropAccum=0.f;
    float menuRubY=0.f;
    static int frame=0;

    while(window.isOpen()){
        float dt=clk.restart().asSeconds();
        if(dt>0.1f)dt=0.1f;
        frame++;

        sf::Event ev;
        while(window.pollEvent(ev)){
            if(ev.type==sf::Event::Closed)window.close();
            if(ev.type==sf::Event::Resized)applyLetterboxView(window,curW,curH);
            if(ev.type==sf::Event::KeyPressed){
                auto k=ev.key.code;
                if(k==sf::Keyboard::F||k==sf::Keyboard::F11){
                    fullscreen=!fullscreen;
                    if(fullscreen)window.create(sf::VideoMode::getDesktopMode(),"Pixel Tris",sf::Style::Fullscreen);
                    else window.create(sf::VideoMode(winSz.x,winSz.y),"Pixel Tris",sf::Style::Default);
                    window.setFramerateLimit(60);applyLetterboxView(window,curW,curH);
                }
                if(k==sf::Keyboard::M){audio.toggleMute();audio.playClick();}

                if(appState==AppState::Menu){
                    if(menuSub==MenuSub::Main){
                        if(k==sf::Keyboard::Up){menuSel=(menuSel+5)%6;audio.playClick();menuRubY=-3.5f;}
                        if(k==sf::Keyboard::Down){menuSel=(menuSel+1)%6;audio.playClick();menuRubY=3.5f;}
                        if(k==sf::Keyboard::Return||k==sf::Keyboard::Space){
                            audio.playClick();
                            if(menuSel==0){game.vsMode=false;game.fieldX=HB_W;game.texW=TEX_W;
                                game.gameMode=GameMode::Marathon;game.restart();
                                appState=AppState::Playing;audio.playTheme();dropAccum=0;}
                            else if(menuSel==1){game.vsMode=false;game.fieldX=HB_W;game.texW=TEX_W;
                                game.gameMode=GameMode::Sprint;game.restart();
                                appState=AppState::Playing;audio.playTheme();dropAccum=0;}
                            else if(menuSel==2){menuSub=MenuSub::VsDiff;vsDiffSel=1;}
                            else if(menuSel==3){menuSub=MenuSub::HiScores;}
                            else if(menuSel==4){menuSub=MenuSub::Controls;}
                            else if(menuSel==5){window.close();}
                        }
                        if(k==sf::Keyboard::Escape)window.close();
                    }else if(menuSub==MenuSub::VsDiff){
                        if(k==sf::Keyboard::Up){vsDiffSel=(vsDiffSel+2)%3;audio.playClick();menuRubY=-3.5f;}
                        if(k==sf::Keyboard::Down){vsDiffSel=(vsDiffSel+1)%3;audio.playClick();menuRubY=3.5f;}
                        if(k==sf::Keyboard::Return||k==sf::Keyboard::Space){
                            audio.playClick();
                            cpuAi.diff=vsDiffSel;cpuAi.reset();
                            game.vsMode=true;game.fieldX=VS_P1_FX;game.texW=VS_TEX_W;
                            game.gameMode=GameMode::Marathon;game.restart();
                            cpuGame.vsMode=true;cpuGame.fieldX=VS_P2_FX;cpuGame.texW=VS_TEX_W;
                            cpuGame.gameMode=GameMode::Marathon;cpuGame.restart();
                            appState=AppState::VsPlaying;
                            resizeWin(true);audio.playTheme();dropAccum=0;cpuDropAccum=0;
                        }
                        if(k==sf::Keyboard::Escape){menuSub=MenuSub::Main;audio.playClick();}
                    }else{
                        if(k==sf::Keyboard::Escape||k==sf::Keyboard::Return){menuSub=MenuSub::Main;audio.playClick();}
                    }
                }
                else if(appState==AppState::Playing){
                    auto goMenu=[&]{audio.stopTheme();audio.setThemePitch(1.f);
                        appState=AppState::Menu;menuSub=MenuSub::Main;audio.playClick();};

                    if(k==sf::Keyboard::Escape){
                        if(game.phase==Game::Playing){
                            game.phase=Game::Paused;game.pauseSel=0;
                            audio.pauseTheme();audio.playClick();
                        }else if(game.phase==Game::Paused){
                            game.phase=Game::Playing;audio.playTheme();audio.playClick();
                        }else if(game.phase==Game::GameOver){goMenu();}
                    }

                    if(game.phase==Game::Paused){
                        if(k==sf::Keyboard::Up){game.pauseSel=(game.pauseSel+2)%3;audio.playClick();menuRubY=-3.5f;}
                        if(k==sf::Keyboard::Down){game.pauseSel=(game.pauseSel+1)%3;audio.playClick();menuRubY=3.5f;}
                        if(k==sf::Keyboard::Return||k==sf::Keyboard::Space){
                            audio.playClick();
                            if(game.pauseSel==0){game.phase=Game::Playing;audio.playTheme();}
                            else if(game.pauseSel==1){game.restart();audio.playTheme();dropAccum=0;}
                            else if(game.pauseSel==2)goMenu();
                        }
                    }

                    if(game.phase==Game::Playing){
                        if(k==sf::Keyboard::Left){
                            game.holdLeft=true;game.dasDir=-1;game.dasTimer=0;game.dasRepeat=0;
                            if(game.tryShift(-1)){audio.playMove();game.kickRub(3.f,0);}
                        }
                        if(k==sf::Keyboard::Right){
                            game.holdRight=true;game.dasDir=1;game.dasTimer=0;game.dasRepeat=0;
                            if(game.tryShift(1)){audio.playMove();game.kickRub(-3.f,0);}
                        }
                        if(k==sf::Keyboard::Down)game.holdDown=true;
                        if(k==sf::Keyboard::Up||k==sf::Keyboard::Z||k==sf::Keyboard::X){
                            if(game.rotatePiece()){audio.playRot();
                                game.kickRub((game.rng()%3-1)*1.5f,(game.rng()%3-1)*1.2f);}
                        }
                        if(k==sf::Keyboard::Space)game.hardDrop();
                        if(k==sf::Keyboard::Tab){if(game.holdPiece())audio.playClick();}
                    }

                    if(k==sf::Keyboard::R&&game.phase==Game::GameOver){
                        game.restart();audio.playTheme();audio.playClick();dropAccum=0;
                    }
                }
                else if(appState==AppState::VsPlaying){
                    auto goMenu=[&]{audio.stopTheme();audio.setThemePitch(1.f);
                        game.vsMode=false;game.fieldX=HB_W;game.texW=TEX_W;
                        resizeWin(false);
                        appState=AppState::Menu;menuSub=MenuSub::Main;audio.playClick();};

                    bool vsOver=game.phase==Game::GameOver||cpuGame.phase==Game::GameOver;

                    if(k==sf::Keyboard::Escape){
                        if(vsOver){goMenu();}
                        else if(game.phase==Game::Playing){
                            game.phase=Game::Paused;game.pauseSel=0;
                            audio.pauseTheme();audio.playClick();
                        }else if(game.phase==Game::Paused){
                            game.phase=Game::Playing;audio.playTheme();audio.playClick();
                        }
                    }

                    if(game.phase==Game::Paused&&!vsOver){
                        if(k==sf::Keyboard::Up){game.pauseSel=(game.pauseSel+2)%3;audio.playClick();menuRubY=-3.5f;}
                        if(k==sf::Keyboard::Down){game.pauseSel=(game.pauseSel+1)%3;audio.playClick();menuRubY=3.5f;}
                        if(k==sf::Keyboard::Return||k==sf::Keyboard::Space){
                            audio.playClick();
                            if(game.pauseSel==0){game.phase=Game::Playing;audio.playTheme();}
                            else if(game.pauseSel==1){
                                game.restart();cpuGame.restart();cpuAi.reset();
                                audio.playTheme();dropAccum=0;cpuDropAccum=0;}
                            else if(game.pauseSel==2)goMenu();
                        }
                    }

                    if(game.phase==Game::Playing&&!vsOver){
                        if(k==sf::Keyboard::Left){
                            game.holdLeft=true;game.dasDir=-1;game.dasTimer=0;game.dasRepeat=0;
                            if(game.tryShift(-1)){audio.playMove();game.kickRub(3.f,0);}
                        }
                        if(k==sf::Keyboard::Right){
                            game.holdRight=true;game.dasDir=1;game.dasTimer=0;game.dasRepeat=0;
                            if(game.tryShift(1)){audio.playMove();game.kickRub(-3.f,0);}
                        }
                        if(k==sf::Keyboard::Down)game.holdDown=true;
                        if(k==sf::Keyboard::Up||k==sf::Keyboard::Z||k==sf::Keyboard::X){
                            if(game.rotatePiece()){audio.playRot();
                                game.kickRub((game.rng()%3-1)*1.5f,(game.rng()%3-1)*1.2f);}
                        }
                        if(k==sf::Keyboard::Space)game.hardDrop();
                        if(k==sf::Keyboard::Tab){if(game.holdPiece())audio.playClick();}
                    }

                    if(vsOver&&k==sf::Keyboard::R){
                        game.restart();cpuGame.restart();cpuAi.reset();
                        audio.playTheme();audio.playClick();dropAccum=0;cpuDropAccum=0;
                    }
                }
            }
            if(ev.type==sf::Event::KeyReleased&&(appState==AppState::Playing||appState==AppState::VsPlaying)){
                if(ev.key.code==sf::Keyboard::Left)game.holdLeft=false;
                if(ev.key.code==sf::Keyboard::Right)game.holdRight=false;
                if(ev.key.code==sf::Keyboard::Down)game.holdDown=false;
            }
        }

        menuRubY*=std::pow(0.65f,dt*60.f);
        if(std::abs(menuRubY)<0.05f)menuRubY=0;

        game.updateAnimations(dt);

        if(appState==AppState::Playing&&game.phase==Game::GameOver&&!game.scoreSubmitted){
            game.scoreSubmitted=true;
            game.hiRank=hiScores.rank(game.score);
            game.nextHiScore=0;
            if(game.hiRank<0&&hiScores.count>0)
                game.nextHiScore=hiScores.entries[hiScores.count-1].score;
            else if(game.hiRank>0)
                game.nextHiScore=hiScores.entries[game.hiRank-1].score;
            hiScores.insert({game.score,game.linesTotal,game.level,
                game.gameMode==GameMode::Sprint?'S':'M'});
            hiScores.save(hiPath);
        }

        if(appState==AppState::Playing){
            float dng=game.dangerLevel();
            audio.setThemePitch(1.0f+std::max(0.f,dng-0.3f)*0.3f);
            if(game.phase==Game::Playing){
                game.updateDAS(dt);
                float dd=game.holdDown?0.18f:game.dropDelay();
                dropAccum+=dt;
                while(dropAccum>=dd){dropAccum-=dd;
                    if(game.holdDown)game.softDropStep();
                    else game.gravityStep();
                    if(game.phase!=Game::Playing)break;}
                game.updateLock(dt);
            }else{dropAccum=0.f;}

            if(game.phase==Game::LineFlash){
                game.flashFrames--;
                if(game.flashFrames<=0)game.finishLineClear();
            }
        }

        if(appState==AppState::VsPlaying){
            bool vsOver=game.phase==Game::GameOver||cpuGame.phase==Game::GameOver;
            cpuGame.updateAnimations(dt);

            if(!vsOver&&game.phase!=Game::Paused){
                if(game.outGarbage>0){cpuGame.garbagePending+=game.outGarbage;game.outGarbage=0;}
                if(cpuGame.outGarbage>0){game.garbagePending+=cpuGame.outGarbage;cpuGame.outGarbage=0;}

                float dng=game.dangerLevel();
                audio.setThemePitch(1.0f+std::max(0.f,dng-0.3f)*0.3f);

                if(game.phase==Game::Playing){
                    game.updateDAS(dt);
                    float dd=game.holdDown?0.18f:game.dropDelay();
                    dropAccum+=dt;
                    while(dropAccum>=dd){dropAccum-=dd;
                        if(game.holdDown)game.softDropStep();
                        else game.gravityStep();
                        if(game.phase!=Game::Playing)break;}
                    game.updateLock(dt);
                }else{dropAccum=0.f;}
                if(game.phase==Game::LineFlash){
                    game.flashFrames--;
                    if(game.flashFrames<=0)game.finishLineClear();
                }

                if(cpuGame.phase==Game::Playing){
                    cpuAi.update(cpuGame,dt);
                    float cdd=cpuGame.dropDelay();
                    cpuDropAccum+=dt;
                    while(cpuDropAccum>=cdd){cpuDropAccum-=cdd;
                        cpuGame.gravityStep();
                        if(cpuGame.phase!=Game::Playing)break;}
                    cpuGame.updateLock(dt);
                }else{cpuDropAccum=0.f;}
                if(cpuGame.phase==Game::LineFlash){
                    cpuGame.flashFrames--;
                    if(cpuGame.flashFrames<=0)cpuGame.finishLineClear();
                }
            }else if(game.phase==Game::Paused){dropAccum=0;cpuDropAccum=0;}

            if(vsOver&&!game.scoreSubmitted){
                game.scoreSubmitted=true;
                audio.stopTheme();audio.setThemePitch(1.f);
                if(cpuGame.phase==Game::GameOver)audio.playLvlUp();
                else audio.playOver();
            }
        }

        // --- Render ---
        if(appState==AppState::VsPlaying){
            vsTex.clear(sf::Color::Transparent);
            {sf::RectangleShape bg;bg.setSize({(float)VS_TEX_W,(float)TEX_H});
             bg.setFillColor(sf::Color(18,22,38));vsTex.draw(bg);}
            for(int i=0;i<100;i++){int x=(i*47+frame)%VS_TEX_W,y2=(i*83+frame/2)%TEX_H;
                drawPixel(vsTex,x,y2,sf::Color(40,48,72,90));}
            for(int i=0;i<36;i++){int x=(i*61-frame)%VS_TEX_W,y2=(i*19)%TEX_H;
                if(((frame+i*7)%48)<24)drawPixel(vsTex,(x+VS_TEX_W)%VS_TEX_W,y2,sf::Color(255,200,120,40));}

            game.drawGrid(vsTex);game.drawBorder(vsTex,frame);game.drawField(vsTex);
            game.drawBloom(vsTex);game.drawDropTrail(vsTex);game.drawLevelUpFlash(vsTex);
            game.drawGhost(vsTex);game.drawActive(vsTex);game.drawActionTexts(vsTex);

            cpuGame.drawGrid(vsTex);cpuGame.drawBorder(vsTex,frame);cpuGame.drawField(vsTex);
            cpuGame.drawBloom(vsTex);cpuGame.drawDropTrail(vsTex);cpuGame.drawLevelUpFlash(vsTex);
            cpuGame.drawGhost(vsTex);cpuGame.drawActive(vsTex);cpuGame.drawActionTexts(vsTex);

            drawVsPanel(vsTex,game,2,false);
            drawVsPanel(vsTex,cpuGame,VS_P2_FX+LW+2,true);
            drawVsDivider(vsTex,frame);
            drawTextCentered(vsTex,VS_P1_FX+LW/2,LH-7,"YOU",sf::Color(120,255,180),1);
            drawTextCentered(vsTex,VS_P2_FX+LW/2,LH-7,"CPU",sf::Color(255,140,140),1);

            if(game.phase==Game::Paused)game.drawPausedOverlay(vsTex,frame,menuRubY);
            bool vsOver=game.phase==Game::GameOver||cpuGame.phase==Game::GameOver;
            if(vsOver)drawVsOverlay(vsTex,cpuGame.phase==Game::GameOver,game,cpuGame,frame);

            vsTex.display();
            window.clear(sf::Color(8,10,18));
            float shakeOff=0;
            if(game.shakeTimer>0){
                float st=game.shakeTimer/Game::SHAKE_DUR;
                shakeOff=std::sin(game.shakeTimer*55.f)*10.f*st*st;
            }
            vsUpscale.setPosition(0,shakeOff);
            window.draw(vsUpscale);vsUpscale.setPosition(0,0);
            for(int y2=0;y2<(int)vsH;y2+=3){
                sf::RectangleShape scan(sf::Vector2f(vsW,1.f));
                scan.setPosition(0,(float)y2);scan.setFillColor(sf::Color(0,0,0,28));
                window.draw(scan);}
            sf::RectangleShape vvg;
            vvg.setSize(sf::Vector2f(vsW,vsH));vvg.setFillColor(sf::Color::Transparent);
            vvg.setOutlineThickness(std::max(vsW,vsH)/8.f);
            vvg.setOutlineColor(sf::Color(0,0,0,70));window.draw(vvg);
        }else{
            tex.clear(sf::Color::Transparent);
            if(appState==AppState::Menu){
                if(menuSub==MenuSub::Main)drawMenu(tex,menuSel,frame,hiScores,game,menuRubY);
                else if(menuSub==MenuSub::HiScores)drawHiScoresScreen(tex,frame,hiScores,game);
                else if(menuSub==MenuSub::VsDiff)drawVsDiffScreen(tex,vsDiffSel,frame,game,menuRubY);
                else drawControlsScreen(tex,frame,game);
            }else{
                game.drawBackground(tex,frame);
                game.drawGrid(tex);game.drawBorder(tex,frame);game.drawField(tex);
                game.drawBloom(tex);game.drawDropTrail(tex);game.drawLevelUpFlash(tex);
                game.drawGhost(tex);game.drawActive(tex);
                game.drawHoldBox(tex);game.drawSidebar(tex);
                game.drawActionTexts(tex);game.drawSprintHud(tex);
                if(game.phase==Game::Paused)game.drawPausedOverlay(tex,frame,menuRubY);
                if(game.phase==Game::GameOver)game.drawGameOverOverlay(tex);
            }
            tex.display();
            window.clear(sf::Color(8,10,18));
            float shakeOff=0;
            if(appState==AppState::Playing&&game.shakeTimer>0){
                float st=game.shakeTimer/Game::SHAKE_DUR;
                shakeOff=std::sin(game.shakeTimer*55.f)*10.f*st*st;
            }
            upscale.setPosition(0,shakeOff);
            window.draw(upscale);upscale.setPosition(0,0);
            for(int y2=0;y2<(int)soloH;y2+=3){
                sf::RectangleShape scan(sf::Vector2f(soloW,1.f));
                scan.setPosition(0,(float)y2);scan.setFillColor(sf::Color(0,0,0,28));
                window.draw(scan);}
            sf::RectangleShape vig;
            vig.setSize(sf::Vector2f(soloW,soloH));vig.setFillColor(sf::Color::Transparent);
            vig.setOutlineThickness(std::max(soloW,soloH)/8.f);
            vig.setOutlineColor(sf::Color(0,0,0,70));window.draw(vig);
        }
        window.display();
    }
    return 0;
}

