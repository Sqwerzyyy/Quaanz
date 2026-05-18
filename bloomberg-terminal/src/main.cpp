// bloomberg-terminal v2.0 — MIT License
// Keys: 1-8 panels, ↑↓ navigate, Enter=chart, b/s buy/sell,
//       a=alert, f=fetch live, F=fetch all, q=quit

#include "terminal.hpp"
#include <ncurses.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════
//  Color pairs
// ═══════════════════════════════════════════════════════════════════
enum CP {
    CP_DEFAULT=1, CP_GREEN, CP_RED, CP_YELLOW, CP_CYAN, CP_MAGENTA,
    CP_BG_GREEN, CP_BG_RED, CP_BG_YELLOW,
    CP_HEADER, CP_SELECTED, CP_BORDER, CP_DIM, CP_URGENT,
    CP_BG_CYAN, CP_BG_MAGENTA,
    CP_BG_GREEN_DIM, CP_BG_RED_DIM,
    CP_BRIGHT_WHITE,
};

static void init_colors() {
    start_color(); use_default_colors();
    init_pair(CP_DEFAULT,     COLOR_WHITE,   -1);
    init_pair(CP_GREEN,       COLOR_GREEN,   -1);
    init_pair(CP_RED,         COLOR_RED,     -1);
    init_pair(CP_YELLOW,      COLOR_YELLOW,  -1);
    init_pair(CP_CYAN,        COLOR_CYAN,    -1);
    init_pair(CP_MAGENTA,     COLOR_MAGENTA, -1);
    init_pair(CP_DIM,         8,             -1);
    init_pair(CP_BG_GREEN,    COLOR_BLACK,   COLOR_GREEN);
    init_pair(CP_BG_RED,      COLOR_BLACK,   COLOR_RED);
    init_pair(CP_BG_YELLOW,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(CP_HEADER,      COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_SELECTED,    COLOR_BLACK,   COLOR_WHITE);
    init_pair(CP_BORDER,      COLOR_CYAN,    -1);
    init_pair(CP_URGENT,      COLOR_RED,     -1);
    init_pair(CP_BG_CYAN,     COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_BG_MAGENTA,  COLOR_BLACK,   COLOR_MAGENTA);
    init_pair(CP_BG_GREEN_DIM,COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_BG_RED_DIM,  COLOR_RED,     COLOR_BLACK);
    init_pair(CP_BRIGHT_WHITE,COLOR_WHITE,   -1);
}

// ═══════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════

static std::string fmt(double v, int w=10, int dec=2) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(dec) << std::setw(w) << v;
    return s.str();
}
static std::string fmtv(long long v) {
    if (v>=1'000'000'000) return std::to_string(v/1'000'000'000)+"B";
    if (v>=1'000'000)     return std::to_string(v/1'000'000)+"M";
    if (v>=1'000)         return std::to_string(v/1'000)+"K";
    return std::to_string(v);
}
static std::string now_str() {
    std::time_t t=std::time(nullptr);
    char b[16]; std::strftime(b,sizeof(b),"%H:%M:%S",std::localtime(&t));
    return b;
}
static std::string date_str() {
    std::time_t t=std::time(nullptr);
    char b[20]; std::strftime(b,sizeof(b),"%a %b %d %Y",std::localtime(&t));
    return b;
}

// Sparkline (UTF-8 block chars)
static void sparkline(WINDOW* w, int y, int x, int width,
                      const std::deque<Candle>& hist) {
    if (hist.empty() || width < 2) return;
    size_t n = std::min((size_t)width, hist.size());
    double mn=1e18, mx=-1e18;
    for (size_t i=hist.size()-n; i<hist.size(); ++i) {
        mn=std::min(mn,hist[i].low);
        mx=std::max(mx,hist[i].high);
    }
    if (mx==mn) mx=mn+0.01;
    static const char* bars[8]={"▁","▂","▃","▄","▅","▆","▇","█"};
    for (int i=0; i<width && (size_t)i<n; ++i) {
        const Candle& c=hist[hist.size()-n+i];
        int bar=(int)((c.close-mn)/(mx-mn)*7.0+0.5);
        bar=std::max(0,std::min(7,bar));
        wattron(w, COLOR_PAIR(c.close>=c.open?CP_GREEN:CP_RED));
        mvwaddstr(w, y, x+i, bars[bar]);
        wattroff(w, COLOR_PAIR(c.close>=c.open?CP_GREEN:CP_RED));
    }
}

// Box with title
static void titled_box(WINDOW* w, const char* title) {
    int rows,cols; getmaxyx(w,rows,cols);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w,0,0);
    wattroff(w, COLOR_PAIR(CP_BORDER));
    if (title && title[0]) {
        wattron(w, COLOR_PAIR(CP_CYAN)|A_BOLD);
        int tx=2; mvwprintw(w,0,tx," %s ",title);
        wattroff(w, COLOR_PAIR(CP_CYAN)|A_BOLD);
    }
    (void)rows;(void)cols;
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 1 — Watchlist
// ═══════════════════════════════════════════════════════════════════

static void draw_watchlist(WINDOW* w, const std::vector<Quote>& quotes,
                           const MarketData& md, int sel, int scroll) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"MARKET WATCHLIST");

    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,1,1,' ',cols-2);
    mvwprintw(w,1,2,"%-7s %-18s %10s %8s %7s %9s %9s %7s %6s  SPARK","SYM","NAME","PRICE","CHG","CHG%","HIGH","LOW","VOL","LIVE");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    for (int i=0; i<rows-4 && (scroll+i)<(int)quotes.size(); ++i) {
        const auto& q=quotes[scroll+i];
        int row=i+2;
        bool issel=(scroll+i)==sel;
        if (issel)       wattron(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else if(i%2==0)  wattron(w, A_DIM);

        mvwprintw(w,row,2,"%-7s %-18s %10.2f",
                  q.symbol.c_str(), q.name.substr(0,18).c_str(), q.price);
        if (!issel) { wattroff(w,A_DIM); }

        int cp=(q.change>=0)?CP_GREEN:CP_RED;
        int attr=issel?(A_BOLD|COLOR_PAIR(CP_SELECTED)):COLOR_PAIR(cp);
        if (issel) { wattroff(w, COLOR_PAIR(CP_SELECTED)|A_BOLD); }
        wattron(w, COLOR_PAIR(cp)|(issel?A_BOLD:0));
        wprintw(w," %+8.2f %+6.2f%%", q.change, q.change_pct);
        wattroff(w, COLOR_PAIR(cp)|(issel?A_BOLD:0));

        if (issel) wattron(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else if(i%2==0) wattron(w, A_DIM);
        wprintw(w," %9.2f %9.2f %7s", q.high, q.low, fmtv(q.volume).c_str());

        // Live badge
        if (q.live) {
            wattron(w, COLOR_PAIR(CP_BG_GREEN));
            wprintw(w," LIVE ");
            wattroff(w, COLOR_PAIR(CP_BG_GREEN));
        } else {
            wattron(w, COLOR_PAIR(CP_DIM));
            wprintw(w,"  SIM ");
            wattroff(w, COLOR_PAIR(CP_DIM));
        }
        if (issel) wattroff(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else wattroff(w, A_DIM);

        // Spark
        sparkline(w, row, cols-22, 18, md.history(q.symbol));
    }

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-2,2,"[↑↓] Nav  [Enter] Chart  [b] Buy  [s] Sell  [a] Alert  [f] Fetch Live  [F] Fetch All  [1-8] Panels");
    wattroff(w, COLOR_PAIR(CP_DIM));

}

// ═══════════════════════════════════════════════════════════════════
//  Panel 2 — Portfolio
// ═══════════════════════════════════════════════════════════════════

static void draw_portfolio(WINDOW* w, const Portfolio& port,
                           const MarketData& md) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"PORTFOLIO");

    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,1,1,' ',cols-2);
    mvwprintw(w,1,2,"%-8s %10s %10s %10s %12s %10s %8s  CHART","SYM","QTY","AVG COST","PRICE","VALUE","P&L","P&L%");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    if (port.positions().empty()) {
        wattron(w, COLOR_PAIR(CP_DIM));
        mvwprintw(w,rows/2,cols/2-20,"No positions. Press [b] to buy your first stock.");
        wattroff(w, COLOR_PAIR(CP_DIM));
    }

    int row=2;
    for (const auto& p : port.positions()) {
        if (row>=rows-6) break;
        bool pos=(p.pnl()>=0);
        if (row%2==0) wattron(w, A_DIM);
        mvwprintw(w,row,2,"%-8s %10.2f %10.2f %10.2f %12.2f",
                  p.symbol.c_str(), p.qty, p.avg_cost, p.current_price, p.value());
        if (row%2==0) wattroff(w, A_DIM);

        wattron(w, COLOR_PAIR(pos?CP_GREEN:CP_RED)|A_BOLD);
        wprintw(w," %+10.2f %+7.2f%%", p.pnl(), p.pnl_pct());
        wattroff(w, COLOR_PAIR(pos?CP_GREEN:CP_RED)|A_BOLD);

        // Mini sparkline for position
        const auto& hist=md.history(p.symbol);
        wprintw(w,"  ");
        sparkline(w, row, getcurx(w), 12, hist);
        ++row;
    }

    // Divider
    wattron(w, COLOR_PAIR(CP_BORDER));
    mvwhline(w,row+1,1,ACS_HLINE,cols-2);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    // Summary
    double tv=port.total_value(), tp=port.total_pnl(), tpp=port.total_pnl_pct();
    int cp=(tp>=0)?CP_GREEN:CP_RED;
    wattron(w, A_BOLD);
    mvwprintw(w,row+2,2,"Cash: $%12.2f    Total Value: $%12.2f    ", port.cash(), tv);
    wattroff(w, A_BOLD);
    wattron(w, COLOR_PAIR(cp)|A_BOLD);
    wprintw(w,"P&L: %+.2f  (%+.2f%%)", tp, tpp);
    wattroff(w, COLOR_PAIR(cp)|A_BOLD);

    // Allocation bar
    if (!port.positions().empty() && port.total_value()>0) {
        int row3=row+4;
        mvwprintw(w,row3,2,"ALLOCATION  ");
        int bw=cols-20;
        for (const auto& p : port.positions()) {
            double pct=p.value()/port.total_value();
            int len=std::max(1,(int)(pct*bw));
            bool pos2=(p.pnl()>=0);
            wattron(w, COLOR_PAIR(pos2?CP_BG_GREEN:CP_BG_RED));
            for (int k=0;k<len;++k) waddch(w,' ');
            wattroff(w, COLOR_PAIR(pos2?CP_BG_GREEN:CP_BG_RED));
        }
        // Label
        wmove(w,row3+1,2);
        for (const auto& p : port.positions()) {
            double pct=p.value()/port.total_value()*100;
            wattron(w, COLOR_PAIR(CP_DIM));
            wprintw(w,"%s:%.0f%% ", p.symbol.c_str(), pct);
            wattroff(w, COLOR_PAIR(CP_DIM));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 3 — Chart
// ═══════════════════════════════════════════════════════════════════

static void draw_chart(WINDOW* w, const std::string& sym,
                       const MarketData& md) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w);
    const Quote* q=md.get(sym);
    const auto& hist=md.history(sym);
    titled_box(w,("CHART  " + sym).c_str());

    if (!q || hist.empty()) {
        mvwprintw(w,rows/2,cols/2-10,"No data for %s",sym.c_str());
        return;
    }

    // Title row
    wattron(w, A_BOLD|COLOR_PAIR(CP_CYAN));
    mvwprintw(w,1,2," %s  |  %s  |  $%.2f",
              sym.c_str(), q->name.substr(0,24).c_str(), q->price);
    wattroff(w, A_BOLD|COLOR_PAIR(CP_CYAN));
    int cp=(q->change>=0)?CP_GREEN:CP_RED;
    wattron(w, COLOR_PAIR(cp)|A_BOLD);
    wprintw(w,"   %+.2f  (%+.2f%%)", q->change, q->change_pct);
    wattroff(w, COLOR_PAIR(cp)|A_BOLD);
    // Extra stats
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,2,2," O:%.2f  H:%.2f  L:%.2f  Vol:%s  52W H:%.2f  L:%.2f",
              q->open, q->high, q->low, fmtv(q->volume).c_str(),
              q->week52_high, q->week52_low);
    wattroff(w, COLOR_PAIR(CP_DIM));

    int ch=rows-7, cw=cols-14;
    if (ch<4||cw<10) return;

    size_t n=std::min((size_t)cw, hist.size());
    double mn=1e18, mx=-1e18;
    for (size_t i=hist.size()-n; i<hist.size(); ++i) {
        mn=std::min(mn,hist[i].low);
        mx=std::max(mx,hist[i].high);
    }
    if (mx==mn){mx+=0.1;mn-=0.1;}
    double range=mx-mn;

    // Y axis
    int nlabels=std::min(8,ch);
    for (int r=0; r<nlabels; ++r) {
        double price=mx-r*(mx-mn)/(nlabels-1);
        wattron(w, COLOR_PAIR(CP_DIM));
        mvwprintw(w,r+4,1,"%9.2f│", price);
        wattroff(w, COLOR_PAIR(CP_DIM));
    }
    // Grid lines
    wattron(w, COLOR_PAIR(CP_DIM)|A_DIM);
    for (int r=0; r<ch; ++r) {
        mvwhline(w,r+4,11,ACS_HLINE,1); // left edge tick
    }
    wattroff(w, COLOR_PAIR(CP_DIM)|A_DIM);

    // Candles
    int x0=11;
    for (int i=0; i<cw && (size_t)i<n; ++i) {
        const Candle& c=hist[hist.size()-n+i];
        bool bull=(c.close>=c.open);
        int cp2=bull?CP_GREEN:CP_RED;

        auto priceY=[&](double p)->int{
            int y=(int)((mx-p)/range*(ch-1));
            return std::max(0,std::min(ch-1,y));
        };
        int yh=priceY(c.high), yl=priceY(c.low);
        int yo=priceY(c.open), yc=priceY(c.close);
        int yb1=std::min(yo,yc), yb2=std::max(yo,yc);

        wattron(w, COLOR_PAIR(cp2));
        // Wick
        for (int y=yh; y<=yl; ++y) mvwaddch(w,y+4,x0+i,'|');
        // Body
        for (int y=yb1; y<=yb2; ++y) mvwaddstr(w,y+4,x0+i,bull?"#":"░");
        wattroff(w, COLOR_PAIR(cp2));
    }

    // Volume bars at bottom
    long long maxvol=1;
    for (size_t i=hist.size()-n; i<hist.size(); ++i)
        maxvol=std::max(maxvol,hist[i].volume);
    if (maxvol>0) {
        int vrow=rows-3;
        wattron(w, COLOR_PAIR(CP_DIM));
        mvwprintw(w,vrow-1,1,"  VOLUME");
        wattroff(w, COLOR_PAIR(CP_DIM));
        for (int i=0; i<cw && (size_t)i<n; ++i) {
            const Candle& c=hist[hist.size()-n+i];
            bool bull=(c.close>=c.open);
            double vr=(double)c.volume/maxvol;
            int vh=(int)(vr*2+0.5);
            wattron(w, COLOR_PAIR(bull?CP_GREEN:CP_RED)|A_DIM);
            for (int v=0;v<vh;++v) mvwaddstr(w,vrow-v,x0+i,"▄");
            wattroff(w, COLOR_PAIR(bull?CP_GREEN:CP_RED)|A_DIM);
        }
    }

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-1,x0,"← %d periods", (int)n);
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 4 — Options Calculator
// ═══════════════════════════════════════════════════════════════════

static void draw_options(WINDOW* w, const Quote* q, double custom_sigma) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"OPTIONS CALCULATOR  —  Black-Scholes");
    (void)rows;

    double S = q ? q->price : 100.0;
    double r = 0.05;
    double sigma = custom_sigma;

    // Symbol info
    if (q) {
        wattron(w, A_BOLD|COLOR_PAIR(CP_CYAN));
        mvwprintw(w,1,2," Underlying: %s  —  %s  |  Spot: $%.2f",
                  q->symbol.c_str(), q->name.substr(0,24).c_str(), S);
        wattroff(w, A_BOLD|COLOR_PAIR(CP_CYAN));
    }
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,2,2," Risk-free rate: %.1f%%   Implied Vol: %.0f%%   Model: Black-Scholes (1973)",
              r*100, sigma*100);
    wattroff(w, COLOR_PAIR(CP_DIM));

    // Table header
    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,3,1,' ',cols-2);
    mvwprintw(w,3,2,"%-8s %10s %10s %10s %10s %10s %10s %10s %10s",
              "EXPIRY","CALL","PUT","Δ CALL","Δ PUT","GAMMA","VEGA","THETA","RHO");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    // Strikes: ATM, +10%, -10%
    double strikes[]={S*0.9, S, S*1.1};
    const char* slabels[]={"OTM -10%","ATM","OTM +10%"};
    double Ts[]={7.0/365, 30.0/365, 60.0/365, 90.0/365, 180.0/365, 365.0/365};
    const char* Tlabels[]={"7D","30D","60D","90D","180D","1Y"};

    int row=5;
    for (int si=0; si<3; ++si) {
        double K=strikes[si];
        // Strike label
        wattron(w, COLOR_PAIR(CP_YELLOW)|A_BOLD);
        mvwprintw(w,row,2,"  Strike: $%.2f  (%s)", K, slabels[si]);
        wattroff(w, COLOR_PAIR(CP_YELLOW)|A_BOLD);
        ++row;

        for (int ti=0; ti<6 && row<rows-3; ++ti,++row) {
            auto res=black_scholes(S,K,Ts[ti],r,sigma);
            if (row%2==0) wattron(w,A_DIM);
            mvwprintw(w,row,2,"%-8s", Tlabels[ti]);
            wattron(w, COLOR_PAIR(CP_GREEN));
            wprintw(w," %10.2f", res.call_price);
            wattroff(w, COLOR_PAIR(CP_GREEN));
            wattron(w, COLOR_PAIR(CP_RED));
            wprintw(w," %10.2f", res.put_price);
            wattroff(w, COLOR_PAIR(CP_RED));
            wattron(w, COLOR_PAIR(CP_CYAN));
            wprintw(w," %10.4f %10.4f", res.call_delta, res.put_delta);
            wattroff(w, COLOR_PAIR(CP_CYAN));
            wattron(w, COLOR_PAIR(CP_YELLOW));
            wprintw(w," %10.6f %10.4f %10.4f %10.4f",
                    res.gamma, res.vega, res.theta, res.rho);
            wattroff(w, COLOR_PAIR(CP_YELLOW));
            if (row%2==0) wattroff(w,A_DIM);
        }
        ++row; // spacing
    }

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-2,2,"[+/-] Adjust implied vol  [↑↓] Navigate symbols");
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 5 — News Feed
// ═══════════════════════════════════════════════════════════════════

static void draw_news(WINDOW* w, const NewsFeed& feed) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"MARKET NEWS  —  LIVE FEED");

    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,1,1,' ',cols-2);
    mvwprintw(w,1,2,"%-8s %-14s  %s","TIME","SOURCE","HEADLINE");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    int row=2;
    for (const auto& item : feed.items()) {
        if (row>=rows-2) break;
        if (item.urgent) {
            wattron(w, COLOR_PAIR(CP_BG_RED));
            mvwprintw(w,row,2," %-8s", item.time_str.c_str());
            wattroff(w, COLOR_PAIR(CP_BG_RED));
            wattron(w, COLOR_PAIR(CP_BG_YELLOW));
            wprintw(w," %-14s", item.source.c_str());
            wattroff(w, COLOR_PAIR(CP_BG_YELLOW));
            wattron(w, COLOR_PAIR(CP_RED)|A_BOLD);
        } else {
            if (row%2==0) wattron(w,A_DIM);
            mvwprintw(w,row,2," %-8s", item.time_str.c_str());
            wattron(w, COLOR_PAIR(CP_CYAN));
            wprintw(w," %-14s", item.source.c_str());
            wattroff(w, COLOR_PAIR(CP_CYAN));
        }

        std::string h=item.headline;
        if ((int)h.size()>cols-30) h=h.substr(0,cols-33)+"...";
        wprintw(w,"  %s", h.c_str());

        if (item.urgent) wattroff(w, COLOR_PAIR(CP_RED)|A_BOLD);
        else             wattroff(w, A_DIM);
        ++row;
    }
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-2,2,"News updates automatically every ~8 ticks");
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 6 — Heat Map
// ═══════════════════════════════════════════════════════════════════

static void draw_heatmap(WINDOW* w, const std::vector<Quote>& quotes) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"HEAT MAP  —  % CHANGE TODAY");

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,1,2,"Color intensity = magnitude of move. Size = relative market cap / volume.");
    wattroff(w, COLOR_PAIR(CP_DIM));

    // Sort by abs change desc for visual impact
    auto sorted=quotes;
    std::sort(sorted.begin(),sorted.end(),[](const Quote& a,const Quote& b){
        return std::abs(a.change_pct)>std::abs(b.change_pct);
    });

    // Grid layout: each cell is ~14 wide, 4 tall
    int cellW=14, cellH=4;
    int gridCols=std::max(1,(cols-2)/cellW);
    int gridRows=std::max(1,(rows-4)/cellH);
    int row0=2;

    for (int i=0; i<(int)sorted.size() && i<gridRows*gridCols; ++i) {
        const auto& q=sorted[i];
        int gr=i/gridCols, gc=i%gridCols;
        int y=row0+gr*cellH, x=1+gc*cellW;
        if (y+cellH>=rows) break;

        // Color based on change_pct
        double pct=q.change_pct;
        int bg_cp;
        if      (pct>= 3.0) bg_cp=CP_BG_GREEN;
        else if (pct>= 1.0) bg_cp=CP_BG_GREEN_DIM;
        else if (pct<=-3.0) bg_cp=CP_BG_RED;
        else if (pct<=-1.0) bg_cp=CP_BG_RED_DIM;
        else                bg_cp=CP_DIM;

        // Fill cell background
        wattron(w, COLOR_PAIR(bg_cp)|A_BOLD);
        for (int dy=0; dy<cellH-1; ++dy) {
            mvwhline(w,y+dy,x,' ',cellW-1);
        }
        // Symbol
        mvwprintw(w,y,   x+1, "%-6s", q.symbol.c_str());
        // Price
        mvwprintw(w,y+1, x+1, "%8.2f", q.price);
        // Change pct
        mvwprintw(w,y+2, x+1, "%+.2f%%", pct);
        wattroff(w, COLOR_PAIR(bg_cp)|A_BOLD);

        // Cell border
        wattron(w, COLOR_PAIR(CP_BORDER));
        mvwvline(w,y,x+cellW-1,'|',cellH-1);
        mvwhline(w,y+cellH-1,x,ACS_HLINE,cellW-1);
        wattroff(w, COLOR_PAIR(CP_BORDER));
    }

    // Legend
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-2,2,"Legend: ");
    wattroff(w, COLOR_PAIR(CP_DIM));
    wattron(w, COLOR_PAIR(CP_BG_GREEN)|A_BOLD);  wprintw(w," ≥+3%% "); wattroff(w, COLOR_PAIR(CP_BG_GREEN)|A_BOLD);
    wattron(w, COLOR_PAIR(CP_BG_GREEN_DIM));      wprintw(w," ≥+1%% "); wattroff(w, COLOR_PAIR(CP_BG_GREEN_DIM));
    wattron(w, COLOR_PAIR(CP_DIM));               wprintw(w," ~0%%  "); wattroff(w, COLOR_PAIR(CP_DIM));
    wattron(w, COLOR_PAIR(CP_BG_RED_DIM));        wprintw(w," ≤-1%% "); wattroff(w, COLOR_PAIR(CP_BG_RED_DIM));
    wattron(w, COLOR_PAIR(CP_BG_RED)|A_BOLD);     wprintw(w," ≤-3%% "); wattroff(w, COLOR_PAIR(CP_BG_RED)|A_BOLD);
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 7 — Screener
// ═══════════════════════════════════════════════════════════════════

static std::vector<Quote> apply_screener(const std::vector<Quote>& all,
                                          const ScreenerFilter& f) {
    std::vector<Quote> out;
    for (const auto& q : all) {
        if (q.change_pct < f.min_change_pct) continue;
        if (q.change_pct > f.max_change_pct) continue;
        if ((double)q.volume < f.min_volume)  continue;
        if (q.price < f.min_price)            continue;
        if (q.price > f.max_price)            continue;
        if (!f.sector.empty() && q.sector!=f.sector) continue;
        out.push_back(q);
    }
    using SF=ScreenerFilter;
    std::sort(out.begin(),out.end(),[&](const Quote& a,const Quote& b){
        double va=0,vb=0;
        switch(f.sort_by){
            case SF::CHANGE_PCT: va=a.change_pct; vb=b.change_pct; break;
            case SF::PRICE:      va=a.price;       vb=b.price;      break;
            case SF::VOLUME:     va=(double)a.volume; vb=(double)b.volume; break;
            case SF::SYMBOL:     return f.sort_desc ? a.symbol>b.symbol : a.symbol<b.symbol;
        }
        return f.sort_desc ? va>vb : va<vb;
    });
    return out;
}

static void draw_screener(WINDOW* w, const std::vector<Quote>& all,
                           const ScreenerFilter& f, int sel, int scroll) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"STOCK SCREENER");

    // Filter info bar
    wattron(w, COLOR_PAIR(CP_BG_YELLOW));
    mvwhline(w,1,1,' ',cols-2);
    mvwprintw(w,1,2,"FILTER: CHG %+.0f%% to %+.0f%%  |  Vol≥%s  |  Price $%.0f-$%.0f  |  Sector: %s  |  Sort: %s %s",
              f.min_change_pct, f.max_change_pct,
              fmtv((long long)f.min_volume).c_str(),
              f.min_price, f.max_price,
              f.sector.empty()?"ALL":f.sector.c_str(),
              f.sort_by==ScreenerFilter::CHANGE_PCT?"CHG%":
              f.sort_by==ScreenerFilter::PRICE?"PRICE":
              f.sort_by==ScreenerFilter::VOLUME?"VOL":"SYM",
              f.sort_desc?"▼":"▲");
    wattroff(w, COLOR_PAIR(CP_BG_YELLOW));

    auto results=apply_screener(all,f);

    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,2,1,' ',cols-2);
    mvwprintw(w,2,2,"%-7s %-18s %10s %8s %7s %10s %9s %8s %10s",
              "SYM","NAME","PRICE","CHG","CHG%","52W HIGH","52W LOW","VOL","MKT CAP");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,3,cols-20,"Results: %d/%d", (int)results.size(), (int)all.size());
    wattroff(w, COLOR_PAIR(CP_DIM));

    for (int i=0; i<rows-6 && (scroll+i)<(int)results.size(); ++i) {
        const auto& q=results[scroll+i];
        int row=i+4;
        bool issel=(scroll+i)==sel;
        if (issel)      wattron(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else if(i%2==0) wattron(w, A_DIM);

        mvwprintw(w,row,2,"%-7s %-18s %10.2f",
                  q.symbol.c_str(), q.name.substr(0,18).c_str(), q.price);

        if (!issel) { wattroff(w, A_DIM); }
        wattron(w, COLOR_PAIR(q.change>=0?CP_GREEN:CP_RED)|(issel?A_BOLD:0));
        wprintw(w," %+8.2f %+6.2f%%", q.change, q.change_pct);
        wattroff(w, COLOR_PAIR(q.change>=0?CP_GREEN:CP_RED)|(issel?A_BOLD:0));
        if (issel)      wattron(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else if(i%2==0) wattron(w, A_DIM);
        wprintw(w," %10.2f %9.2f %8s",
                q.week52_high, q.week52_low, fmtv(q.volume).c_str());
        if (q.market_cap>0) wprintw(w," %8.1fB", q.market_cap);
        else                wprintw(w," %9s", "N/A");
        if (issel)      wattroff(w, COLOR_PAIR(CP_SELECTED)|A_BOLD);
        else            wattroff(w, A_DIM);
    }

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,rows-2,2,"[g/G] CHG%%  [p/P] PRICE  [v/V] VOL  [Enter] Chart  [t]=Tech [n]=Finance [c]=Crypto [0]=All");
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 8 — Macro Dashboard
// ═══════════════════════════════════════════════════════════════════

static void draw_macro(WINDOW* w, const MacroDashboard& macro) {
    int rows,cols; getmaxyx(w,rows,cols);
    werase(w); titled_box(w,"MACRO DASHBOARD");

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w,1,2,"Global macro indicators  |  %s  |  [r] Refresh live", date_str().c_str());
    if (macro.is_fetching()) {
        wattron(w, COLOR_PAIR(CP_YELLOW)|A_BOLD);
        wprintw(w,"  ⟳ FETCHING…");
        wattroff(w, COLOR_PAIR(CP_YELLOW)|A_BOLD);
    }
    wattroff(w, COLOR_PAIR(CP_DIM));

    // Dividers: split into columns
    int half=cols/2;

    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwprintw(w,2,2,"%-18s %12s %10s %8s  %-20s","INDICATOR","VALUE","CHG","CHG%","DESCRIPTION");
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);

    int row=3;
    std::string last_group="";
    const auto& inds=macro.indicators();
    for (size_t i=0; i<inds.size() && row<rows-3; ++i,++row) {
        const auto& ind=inds[i];
        bool pos=(ind.change>=0);
        if (row%2==0) wattron(w,A_DIM);
        mvwprintw(w,row,2,"%-18s", ind.name.c_str());
        wattron(w, A_BOLD);
        wprintw(w," %12.4f", ind.value);
        wattroff(w, A_BOLD);
        wattron(w, COLOR_PAIR(pos?CP_GREEN:CP_RED)|(row%2==0?A_DIM:0));
        wprintw(w," %+10.4f %+7.2f%%", ind.change, ind.change_pct);
        wattroff(w, COLOR_PAIR(pos?CP_GREEN:CP_RED));
        wattron(w, COLOR_PAIR(CP_DIM));
        wprintw(w,"  %-30s", ind.desc.substr(0,30).c_str());
        wattroff(w, COLOR_PAIR(CP_DIM));
        if (row%2==0) wattroff(w,A_DIM);
    }

    // Yield curve mini chart
    row+=2;
    if (row<rows-6) {
        wattron(w, COLOR_PAIR(CP_CYAN)|A_BOLD);
        mvwprintw(w,row,2,"YIELD CURVE (simulated)");
        wattroff(w, COLOR_PAIR(CP_CYAN)|A_BOLD);
        ++row;
        double yields[]={4.88,4.70,4.60,4.52,4.42,4.20,4.05};
        const char* tenors[]={"2Y","3Y","5Y","7Y","10Y","20Y","30Y"};
        double ymin=4.0, ymax=5.1;
        for (int ti=0; ti<7 && row<rows-2; ++ti) {
            double y=yields[ti];
            int barlen=(int)((y-ymin)/(ymax-ymin)*(half-28)+1);
            wattron(w, COLOR_PAIR(CP_DIM));
            mvwprintw(w,row,2,"%3s %.2f%% ", tenors[ti], y);
            wattroff(w, COLOR_PAIR(CP_DIM));
            wattron(w, COLOR_PAIR(CP_CYAN));
            for (int k=0;k<barlen;++k) waddstr(w,"█");
            wattroff(w, COLOR_PAIR(CP_CYAN));
            ++row;
        }
    }
    (void)half;(void)last_group;
}

// ═══════════════════════════════════════════════════════════════════
//  Header & Status
// ═══════════════════════════════════════════════════════════════════

static void draw_header(WINDOW* w, int panel, bool fetching,
                        const std::string& fstatus) {
    int cols=getmaxx(w);
    werase(w);
    wattron(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
    mvwhline(w,0,0,' ',cols);
    mvwprintw(w,0,1,"▶ BLOOMBERG TERMINAL  ");
    wattroff(w, A_BOLD);

    struct Tab { int id; const char* label; };
    Tab tabs[]={{1,"MARKET"},{2,"PORTFOLIO"},{3,"CHART"},
                {4,"OPTIONS"},{5,"NEWS"},{6,"HEATMAP"},{7,"SCREENER"},{8,"MACRO"},
                {9,"MOVERS"},{10,"CORREL"},{11,"SECTORS"}};
    int x=23;
    for (auto& t : tabs) {
        if (t.id==panel) {
            wattroff(w, COLOR_PAIR(CP_HEADER));
            wattron(w, A_BOLD|A_REVERSE);
            mvwprintw(w,0,x,"[%d]%s",t.id,t.label);
            wattroff(w, A_BOLD|A_REVERSE);
            wattron(w, COLOR_PAIR(CP_HEADER));
        } else {
            mvwprintw(w,0,x,"[%d]%s",t.id,t.label);
        }
        x+=(int)strlen(t.label)+4;
    }

    // Fetch status
    if (fetching) {
        wattron(w, COLOR_PAIR(CP_BG_YELLOW));
        mvwprintw(w,0,x+2," ⟳ %s ", fstatus.substr(0,30).c_str());
        wattroff(w, COLOR_PAIR(CP_BG_YELLOW));
    } else if (!fstatus.empty()) {
        wattron(w, COLOR_PAIR(CP_BG_GREEN));
        mvwprintw(w,0,x+2," %s ", fstatus.substr(0,30).c_str());
        wattroff(w, COLOR_PAIR(CP_BG_GREEN));
    }

    std::string ts=now_str();
    mvwprintw(w,0,cols-(int)ts.size()-2,"%s",ts.c_str());
    wattroff(w, COLOR_PAIR(CP_HEADER)|A_BOLD);
}

static void draw_status(WINDOW* w, const std::string& msg, bool ok) {
    int cols=getmaxx(w);
    werase(w);
    wattron(w, COLOR_PAIR(ok?CP_BG_GREEN:CP_BG_RED)|A_BOLD);
    mvwhline(w,0,0,' ',cols);
    mvwprintw(w,0,2,"%s",msg.c_str());
    wattroff(w, COLOR_PAIR(ok?CP_BG_GREEN:CP_BG_RED)|A_BOLD);
}

static std::string prompt(WINDOW* sw, const std::string& msg) {
    int cols=getmaxx(sw);
    werase(sw);
    wattron(sw, COLOR_PAIR(CP_BG_YELLOW)|A_BOLD);
    mvwhline(sw,0,0,' ',cols);
    mvwprintw(sw,0,2,"%s",msg.c_str());
    wattroff(sw, COLOR_PAIR(CP_BG_YELLOW)|A_BOLD);
    wrefresh(sw);
    echo(); curs_set(1);
    char buf[64]={};
    mvwgetnstr(sw,0,(int)msg.size()+3,buf,48);
    curs_set(0); noecho();
    return buf;
}

// ═══════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════

// Forward declarations for panels defined after main
static void draw_gainers(WINDOW* w, const MarketSummary& ms);
static void draw_correlation(WINDOW* w, const CorrelationMatrix& cm);
static void draw_sector(WINDOW* w, const std::vector<SectorPerf>& sectors,
                        const std::vector<Quote>& quotes);

int main() {
    initscr(); cbreak(); noecho();
    keypad(stdscr,TRUE); curs_set(0);
    nodelay(stdscr,TRUE); set_escdelay(25);

    if (!has_colors()) { endwin(); puts("Terminal needs color support."); return 1; }
    init_colors();

    int ROWS,COLS; getmaxyx(stdscr,ROWS,COLS);
    if (ROWS<24||COLS<100) {
        endwin();
        puts("Terminal too small. Resize to at least 100x24.");
        return 1;
    }

    WINDOW* wHeader=newwin(1,    COLS,0,     0);
    WINDOW* wMain  =newwin(ROWS-2,COLS,1,   0);
    WINDOW* wStatus=newwin(1,    COLS,ROWS-1,0);

    MarketData    md;
    Portfolio     port;
    NewsFeed      news;
    MacroDashboard macro;
    std::vector<Alert> alerts;

    port.load("portfolio.csv");

    int  panel=1, sel=0, scroll=0;
    int  scr_sel=0, scr_scroll=0;
    std::string chart_sym="AAPL";
    std::string status_msg="Welcome  |  [f] fetch live prices  |  [q] quit";
    bool status_ok=true;
    int  tick=0;
    double custom_sigma=0.25;

    ScreenerFilter sf;

    // Analytics cache (recompute every N ticks)
    MarketSummary    mkt_summary;
    CorrelationMatrix corr_matrix;
    std::vector<SectorPerf> sector_perf;
    int analytics_tick = 0;

    while (true) {
        if (++tick%5==0) {
            md.tick(); news.tick(); macro.tick();
            port.update_prices(md);
            auto fired=md.check_alerts(alerts);
            if (!fired.empty()) {
                status_msg="⚡ ALERT: "+fired[0]->symbol
                    +(fired[0]->above?" ≥ ":" ≤ ")
                    +std::to_string(fired[0]->target_price);
                status_ok=false;
            }
            // Recompute analytics every 20 ticks
            if (++analytics_tick % 20 == 0 || analytics_tick == 1) {
                auto all_q = md.all();
                mkt_summary = compute_market_summary(all_q);
                sector_perf = compute_sector_perf(all_q);
                // Correlation: use core symbols
                std::vector<std::string> cor_syms = {
                    "AAPL","MSFT","GOOGL","NVDA","TSLA",
                    "JPM","GS","SPY","BTC","ETH","GLD"
                };
                corr_matrix = compute_correlation(cor_syms,
                    [&](const std::string& s) -> const std::deque<Candle>* {
                        return &md.history(s);
                    });
            }
        }

        auto quotes=md.all();
        std::sort(quotes.begin(),quotes.end(),[](const Quote& a,const Quote& b){
            return a.symbol<b.symbol;
        });

        // Resize check
        int R2,C2; getmaxyx(stdscr,R2,C2);
        if (R2!=ROWS||C2!=COLS) {
            ROWS=R2; COLS=C2;
            wresize(wHeader,1,COLS);
            wresize(wMain,ROWS-2,COLS);
            wresize(wStatus,1,COLS);
            mvwin(wStatus,ROWS-1,0);
        }

        draw_header(wHeader, panel, md.is_fetching(), md.fetch_status());
        wrefresh(wHeader);

        switch(panel) {
            case 1: draw_watchlist(wMain,quotes,md,sel,scroll);         break;
            case 2: draw_portfolio(wMain,port,md);                       break;
            case 3: draw_chart(wMain,chart_sym,md);                      break;
            case 4: draw_options(wMain,md.get(chart_sym),custom_sigma);  break;
            case 5: draw_news(wMain,news);                               break;
            case 6: draw_heatmap(wMain,quotes);                          break;
            case 7: draw_screener(wMain,quotes,sf,scr_sel,scr_scroll);   break;
            case 8: draw_macro(wMain,macro);                             break;
            case 9: draw_gainers(wMain,mkt_summary);                     break;
            case 10: draw_correlation(wMain,corr_matrix);                break;
            case 11: draw_sector(wMain,sector_perf,quotes);              break;
        }
        wrefresh(wMain);
        draw_status(wStatus,status_msg,status_ok);
        wrefresh(wStatus);

        int ch=getch();
        if (ch=='q'||ch=='Q') break;

        // Panel switch
        if (ch>='1'&&ch<='8') { panel=ch-'0'; continue; }
        if (ch=='9') { panel=9;  continue; }
        if (ch=='0') { panel=10; continue; }
        if (ch=='=') { panel=11; continue; }

        // Navigation
        auto nav=[&](int& s, int& sc, int total, int vis){
            if (ch==KEY_UP&&s>0){ --s; if(s<sc)--sc; }
            if (ch==KEY_DOWN&&s<total-1){ ++s; if(s>=sc+vis)++sc; }
        };
        int vis=ROWS-6;

        switch(ch) {
            case KEY_UP: case KEY_DOWN:
                if (panel==1) nav(sel,scroll,(int)quotes.size(),vis);
                if (panel==7) nav(scr_sel,scr_scroll,
                    (int)apply_screener(quotes,sf).size(),vis);
                break;

            case '\n': case KEY_ENTER:
                if (panel==1&&sel<(int)quotes.size()){
                    chart_sym=quotes[sel].symbol; panel=3;
                    status_msg="Chart: "+chart_sym; status_ok=true;
                }
                if (panel==7){
                    auto res=apply_screener(quotes,sf);
                    if (scr_sel<(int)res.size()){
                        chart_sym=res[scr_sel].symbol; panel=3;
                    }
                }
                break;

            // Options vol
            case '+': custom_sigma=std::min(2.0,custom_sigma+0.05); break;
            case '-': custom_sigma=std::max(0.01,custom_sigma-0.05); break;

            // Screener sort
            case 'g': sf.sort_by=ScreenerFilter::CHANGE_PCT; sf.sort_desc=true;  break;
            case 'G': sf.sort_by=ScreenerFilter::CHANGE_PCT; sf.sort_desc=false; break;
            case 'p': sf.sort_by=ScreenerFilter::PRICE;      sf.sort_desc=true;  break;
            case 'P': sf.sort_by=ScreenerFilter::PRICE;      sf.sort_desc=false; break;
            case 'v': sf.sort_by=ScreenerFilter::VOLUME;     sf.sort_desc=true;  break;
            case 'V': sf.sort_by=ScreenerFilter::VOLUME;     sf.sort_desc=false; break;
            case 't': sf.sector="Technology"; break;
            case 'n': sf.sector="Finance";    break;
            case 'c': sf.sector="Crypto";     break;
            case '`': sf.sector="";           break;  // backtick = all sectors

            // Fetch live
            case 'f': case 'F':
                if (ch=='f'&&sel<(int)quotes.size())
                    md.fetch_live(quotes[sel].symbol);
                else
                    md.fetch_all_live();
                status_msg="Fetching live data from Yahoo Finance…";
                status_ok=true;
                break;

            // Macro refresh
            case 'r': case 'R':
                if (panel==8) { macro.refresh_live(); status_msg="Refreshing macro data…"; }
                else           { status_msg="Refreshed  "+now_str(); }
                status_ok=true;
                break;

            // Buy
            case 'b': case 'B': {
                std::string sym=prompt(wStatus,"Buy symbol: ");
                if(sym.empty()) break;
                std::string sq=prompt(wStatus,"Quantity: ");
                const auto* q=md.get(sym);
                if(!q){status_msg="Symbol not found: "+sym;status_ok=false;break;}
                double qty=sq.empty()?0:std::stod(sq);
                if(qty<=0){status_msg="Invalid quantity";status_ok=false;break;}
                port.buy(sym,qty,q->price);
                port.save("portfolio.csv");
                std::ostringstream ss;
                ss<<"Bought "<<qty<<"x "<<sym<<" @ $"<<std::fixed<<std::setprecision(2)<<q->price;
                status_msg=ss.str(); status_ok=true;
                break;
            }
            // Sell
            case 's': case 'S': {
                std::string sym=prompt(wStatus,"Sell symbol: ");
                if(sym.empty()) break;
                std::string sq=prompt(wStatus,"Quantity: ");
                const auto* q=md.get(sym);
                if(!q){status_msg="Symbol not found: "+sym;status_ok=false;break;}
                double qty=sq.empty()?0:std::stod(sq);
                port.sell(sym,qty,q->price);
                port.save("portfolio.csv");
                std::ostringstream ss;
                ss<<"Sold "<<qty<<"x "<<sym<<" @ $"<<std::fixed<<std::setprecision(2)<<q->price;
                status_msg=ss.str(); status_ok=true;
                break;
            }
            // Alert
            case 'a': case 'A': {
                std::string sym=prompt(wStatus,"Alert symbol: ");
                if(sym.empty()) break;
                std::string sp=prompt(wStatus,"Target price: ");
                std::string sd=prompt(wStatus,"Direction (above/below): ");
                if(sp.empty()) break;
                bool above=(sd=="above"||sd=="a"||sd=="A");
                alerts.push_back({sym,std::stod(sp),above,false});
                status_msg="Alert set: "+sym+(above?" ≥ ":" ≤ ")+sp;
                status_ok=true;
                break;
            }
        }
        napms(100);
    }

    port.save("portfolio.csv");
    delwin(wHeader); delwin(wMain); delwin(wStatus);
    endwin();
    puts("Bloomberg Terminal closed. Portfolio saved to portfolio.csv");
    return 0;
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 9 — Gainers / Losers / Volume
// ═══════════════════════════════════════════════════════════════════

static void draw_gainers(WINDOW* w, const MarketSummary& ms) {
    int rows, cols; getmaxyx(w, rows, cols);
    werase(w); titled_box(w, "MARKET MOVERS");

    // Breadth bar
    int bw = cols - 4;
    int adv_w = (int)(ms.advances  / 100.0 * bw);
    int dec_w = (int)(ms.declines  / 100.0 * bw);
    int unc_w = std::max(0, bw - adv_w - dec_w);

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, 1, 2, "MARKET BREADTH  ADV %.0f%%  UNCH %.0f%%  DEC %.0f%%  |  Avg: %+.2f%%",
              ms.advances, ms.unchanged, ms.declines, ms.avg_change_pct);
    wattroff(w, COLOR_PAIR(CP_DIM));

    mvwprintw(w, 2, 2, "");
    wattron(w, COLOR_PAIR(CP_BG_GREEN));
    for (int i = 0; i < adv_w; ++i) waddch(w, ' ');
    wattroff(w, COLOR_PAIR(CP_BG_GREEN));
    wattron(w, COLOR_PAIR(CP_DIM));
    for (int i = 0; i < unc_w; ++i) waddch(w, ' ');
    wattroff(w, COLOR_PAIR(CP_DIM));
    wattron(w, COLOR_PAIR(CP_BG_RED));
    for (int i = 0; i < dec_w; ++i) waddch(w, ' ');
    wattroff(w, COLOR_PAIR(CP_BG_RED));

    // Three columns
    int cw = (cols - 4) / 3;
    auto draw_col = [&](int x, const char* title,
                         const std::vector<GainerLoser>& items,
                         int cp_val, bool show_vol) {
        wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvwhline(w, 4, x, ' ', cw);
        mvwprintw(w, 4, x + 1, " %-6s %8s %8s", "SYM",
                  show_vol ? "VOLUME" : "PRICE",
                  show_vol ? "CHG%"   : "CHG%");
        mvwprintw(w, 3, x + cw/2 - (int)strlen(title)/2, "%s", title);
        wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

        for (int i = 0; i < (int)items.size() && i < 8; ++i) {
            const auto& g = items[i];
            int row = 5 + i * 2;
            if (row >= rows - 2) break;

            // Badge background
            wattron(w, COLOR_PAIR(g.change_pct >= 0 ? CP_BG_GREEN : CP_BG_RED) | A_BOLD);
            mvwprintw(w, row, x + 1, " %-6s ", g.symbol.c_str());
            wattroff(w, COLOR_PAIR(g.change_pct >= 0 ? CP_BG_GREEN : CP_BG_RED) | A_BOLD);

            wattron(w, A_BOLD);
            if (show_vol)
                wprintw(w, " %8s", fmtv((long long)g.volume).c_str());
            else
                wprintw(w, " %8.2f", g.price);
            wattroff(w, A_BOLD);

            wattron(w, COLOR_PAIR(cp_val) | A_BOLD);
            wprintw(w, " %+7.2f%%", g.change_pct);
            wattroff(w, COLOR_PAIR(cp_val) | A_BOLD);

            // Name small
            wattron(w, COLOR_PAIR(CP_DIM));
            mvwprintw(w, row + 1, x + 2, "%-20s",
                      g.name.substr(0, 20).c_str());
            wattroff(w, COLOR_PAIR(CP_DIM));
        }
        // Column divider
        wattron(w, COLOR_PAIR(CP_BORDER));
        for (int r = 3; r < rows - 1; ++r) mvwaddch(w, r, x + cw - 1, ACS_VLINE);
        wattroff(w, COLOR_PAIR(CP_BORDER));
    };

    draw_col(2,       "🟢  TOP GAINERS",  ms.top_gainers, CP_GREEN, false);
    draw_col(2+cw,    "🔴  TOP LOSERS",   ms.top_losers,  CP_RED,   false);
    draw_col(2+cw*2,  "📊  TOP VOLUME",   ms.top_volume,  CP_CYAN,  true);
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 10 — Correlation Matrix
// ═══════════════════════════════════════════════════════════════════

static void draw_correlation(WINDOW* w, const CorrelationMatrix& cm) {
    int rows, cols; getmaxyx(w, rows, cols);
    werase(w); titled_box(w, "CORRELATION MATRIX  —  30-day Pearson");

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, 1, 2,
      "Color: ██ >+0.7 strong pos  ░░ ~0 neutral  ▓▓ <-0.7 strong neg");
    wattroff(w, COLOR_PAIR(CP_DIM));

    int N = (int)cm.symbols.size();
    if (N == 0) return;

    // Cell size
    int cell = 5;
    int label_w = 7;
    int x0 = label_w + 2;
    int y0 = 3;

    // Column headers
    wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
    for (int j = 0; j < N; ++j) {
        int x = x0 + j * cell;
        if (x + cell >= cols) break;
        std::string s = cm.symbols[j].substr(0, 4);
        mvwprintw(w, y0, x, "%4s ", s.c_str());
    }
    wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

    for (int i = 0; i < N; ++i) {
        int y = y0 + 1 + i;
        if (y >= rows - 2) break;

        // Row label
        wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
        mvwprintw(w, y, 2, "%-6s", cm.symbols[i].substr(0, 6).c_str());
        wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

        for (int j = 0; j < N; ++j) {
            int x = x0 + j * cell;
            if (x + cell >= cols) break;

            double r = cm.matrix[i][j];

            // Color by correlation strength
            int cp;
            const char* block;
            if (i == j) {
                cp = CP_BG_CYAN; block = "SELF";
            } else if (r >= 0.7) {
                cp = CP_BG_GREEN; block = "████";
            } else if (r >= 0.3) {
                cp = CP_GREEN;    block = "▓▓▓▓";
            } else if (r >= -0.3) {
                cp = CP_DIM;      block = "░░░░";
            } else if (r >= -0.7) {
                cp = CP_RED;      block = "▒▒▒▒";
            } else {
                cp = CP_BG_RED;   block = "████";
            }

            wattron(w, COLOR_PAIR(cp) | (i==j ? A_BOLD : 0));
            if (i == j) {
                mvwprintw(w, y, x, "%-4s ", cm.symbols[i].substr(0,4).c_str());
            } else {
                mvwprintw(w, y, x, "%+.2f", r);
            }
            wattroff(w, COLOR_PAIR(cp) | A_BOLD);
            (void)block;
        }
    }

    // Legend
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, rows-2, 2, "Correlation based on %d daily returns. ", 30);
    wattroff(w, COLOR_PAIR(CP_DIM));
    wattron(w, COLOR_PAIR(CP_BG_GREEN) | A_BOLD); wprintw(w, " ≥0.7 "); wattroff(w, COLOR_PAIR(CP_BG_GREEN)|A_BOLD);
    wattron(w, COLOR_PAIR(CP_GREEN));              wprintw(w, " ≥0.3 "); wattroff(w, COLOR_PAIR(CP_GREEN));
    wattron(w, COLOR_PAIR(CP_DIM));                wprintw(w, " ~0   "); wattroff(w, COLOR_PAIR(CP_DIM));
    wattron(w, COLOR_PAIR(CP_RED));                wprintw(w, " ≤-0.3"); wattroff(w, COLOR_PAIR(CP_RED));
    wattron(w, COLOR_PAIR(CP_BG_RED) | A_BOLD);    wprintw(w, " ≤-0.7"); wattroff(w, COLOR_PAIR(CP_BG_RED)|A_BOLD);
}

// ═══════════════════════════════════════════════════════════════════
//  Panel 11 — Sector Pie Chart + Performance
// ═══════════════════════════════════════════════════════════════════

static void draw_sector(WINDOW* w, const std::vector<SectorPerf>& sectors,
                         const std::vector<Quote>& quotes) {
    int rows, cols; getmaxyx(w, rows, cols);
    werase(w); titled_box(w, "SECTOR PERFORMANCE");

    // Left: sector bar chart
    int chart_w = cols / 2 - 4;
    int bar_max  = chart_w - 20;

    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 1, 1, ' ', cols / 2 - 2);
    mvwprintw(w, 1, 2, "%-14s %6s %5s %s", "SECTOR", "AVG%", "ADV", "PERFORMANCE BAR");
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

    // Find max abs for scaling
    double max_abs = 0.01;
    for (const auto& s : sectors)
        max_abs = std::max(max_abs, std::abs(s.avg_change_pct));

    static const int SECTOR_COLORS[] = {
        CP_CYAN, CP_GREEN, CP_YELLOW, CP_MAGENTA, CP_RED, CP_BORDER, CP_DIM
    };

    for (int i = 0; i < (int)sectors.size() && i < rows - 5; ++i) {
        const auto& s = sectors[i];
        int row = 2 + i * 2;
        if (row + 1 >= rows - 2) break;

        bool pos = s.avg_change_pct >= 0;
        int cp_bar = pos ? CP_GREEN : CP_RED;
        int cp_sec = SECTOR_COLORS[i % 7];

        wattron(w, COLOR_PAIR(cp_sec) | A_BOLD);
        mvwprintw(w, row, 2, "%-14s", s.sector.substr(0, 14).c_str());
        wattroff(w, COLOR_PAIR(cp_sec) | A_BOLD);

        wattron(w, COLOR_PAIR(pos ? CP_GREEN : CP_RED) | A_BOLD);
        wprintw(w, " %+6.2f%%", s.avg_change_pct);
        wattroff(w, COLOR_PAIR(pos ? CP_GREEN : CP_RED) | A_BOLD);

        wattron(w, COLOR_PAIR(CP_DIM));
        wprintw(w, " %2d/%2d ", s.advances, s.count);
        wattroff(w, COLOR_PAIR(CP_DIM));

        // Bar
        int barlen = (int)(std::abs(s.avg_change_pct) / max_abs * bar_max);
        wattron(w, COLOR_PAIR(cp_bar) | A_BOLD);
        for (int k = 0; k < barlen; ++k) waddstr(w, pos ? "█" : "▒");
        wattroff(w, COLOR_PAIR(cp_bar) | A_BOLD);

        // Sub-row: symbols in sector
        wattron(w, COLOR_PAIR(CP_DIM));
        mvwprintw(w, row + 1, 4, "");
        for (const auto& q : quotes) {
            if (q.sector == s.sector) {
                wattron(w, COLOR_PAIR(q.change_pct >= 0 ? CP_GREEN : CP_RED));
                wprintw(w, "%s(%+.1f%%) ", q.symbol.c_str(), q.change_pct);
                wattroff(w, COLOR_PAIR(q.change_pct >= 0 ? CP_GREEN : CP_RED));
            }
        }
        wattroff(w, COLOR_PAIR(CP_DIM));
    }

    // Right side: ASCII pie-like donut
    int px = cols / 2 + 4, py = 2;
    int pr = std::min((rows - 6) / 2, (cols / 2 - 8) / 4);
    pr = std::max(3, std::min(pr, 8));

    wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
    mvwprintw(w, py - 1, px + pr * 2 - 6, "ALLOCATION");
    wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

    // Draw ASCII donut
    double total_count = 0;
    for (const auto& s : sectors) total_count += s.count;

    for (int dy = -pr; dy <= pr; ++dy) {
        for (int dx = -pr * 2; dx <= pr * 2; ++dx) {
            double fx = (double)dx / (pr * 2);
            double fy = (double)dy / pr;
            double dist = fx*fx + fy*fy;
            if (dist > 1.0 || dist < 0.36) continue; // donut ring

            double angle = std::atan2(fy, fx) + M_PI; // 0..2PI
            double norm  = angle / (2.0 * M_PI);

            // Find which sector this angle falls into
            double cum = 0;
            int sec_idx = 0;
            for (int si = 0; si < (int)sectors.size(); ++si) {
                cum += (double)sectors[si].count / total_count;
                if (norm <= cum) { sec_idx = si; break; }
            }

            int cp_d = SECTOR_COLORS[sec_idx % 7];
            bool pos = sectors[sec_idx].avg_change_pct >= 0;
            wattron(w, COLOR_PAIR(pos ? cp_d : CP_RED) | A_BOLD);
            mvwprintw(w, py + dy + pr, px + dx + pr*2, "█");
            wattroff(w, COLOR_PAIR(pos ? cp_d : CP_RED) | A_BOLD);
        }
    }

    // Legend next to donut
    int lx = px + pr * 4 + 3;
    int ly = py;
    for (int i = 0; i < (int)sectors.size() && ly + i < rows - 2; ++i) {
        const auto& s = sectors[i];
        wattron(w, COLOR_PAIR(SECTOR_COLORS[i % 7]) | A_BOLD);
        mvwprintw(w, ly + i, lx, "█ ");
        wattroff(w, COLOR_PAIR(SECTOR_COLORS[i % 7]) | A_BOLD);
        wattron(w, COLOR_PAIR(CP_DIM));
        wprintw(w, "%-12s %.0f%%",
                s.sector.substr(0, 12).c_str(),
                (double)s.count / total_count * 100.0);
        wattroff(w, COLOR_PAIR(CP_DIM));
    }

    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, rows - 2, 2, "Performance based on today's simulated returns. [f/F] to load live data.");
    wattroff(w, COLOR_PAIR(CP_DIM));
}
