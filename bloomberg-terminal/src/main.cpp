// bloomberg-terminal — A Bloomberg-inspired terminal emulator in C++
// MIT License  © 2024
//
// Keys:
//   1-5  Switch panels   Tab  Cycle focus
//   b/s  Buy/Sell stock  a    Add alert
//   q    Quit

#include "terminal.hpp"
#include <ncurses.h>
#include <panel.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>

// ─── Color pairs ──────────────────────────────────────────────────
enum CP {
    CP_DEFAULT = 1,
    CP_GREEN, CP_RED, CP_YELLOW, CP_CYAN, CP_MAGENTA,
    CP_BLACK_ON_GREEN, CP_BLACK_ON_RED, CP_BLACK_ON_YELLOW,
    CP_HEADER, CP_SELECTED, CP_BORDER, CP_DIM, CP_URGENT,
    CP_ORANGE, CP_BRIGHT_GREEN
};

static void init_colors() {
    start_color();
    use_default_colors();
    // Foreground colors
    init_pair(CP_DEFAULT,        COLOR_WHITE,   -1);
    init_pair(CP_GREEN,          COLOR_GREEN,   -1);
    init_pair(CP_RED,            COLOR_RED,     -1);
    init_pair(CP_YELLOW,         COLOR_YELLOW,  -1);
    init_pair(CP_CYAN,           COLOR_CYAN,    -1);
    init_pair(CP_MAGENTA,        COLOR_MAGENTA, -1);
    init_pair(CP_DIM,            8,             -1);  // dark gray
    // Badges
    init_pair(CP_BLACK_ON_GREEN,  COLOR_BLACK, COLOR_GREEN);
    init_pair(CP_BLACK_ON_RED,    COLOR_BLACK, COLOR_RED);
    init_pair(CP_BLACK_ON_YELLOW, COLOR_BLACK, COLOR_YELLOW);
    // UI chrome
    init_pair(CP_HEADER,   COLOR_BLACK,  COLOR_CYAN);
    init_pair(CP_SELECTED, COLOR_BLACK,  COLOR_WHITE);
    init_pair(CP_BORDER,   COLOR_CYAN,   -1);
    init_pair(CP_URGENT,   COLOR_RED,    -1);
    init_pair(CP_ORANGE,   COLOR_YELLOW, -1);
    init_pair(CP_BRIGHT_GREEN, COLOR_GREEN, -1);
}

// ─── Helpers ──────────────────────────────────────────────────────

static std::string format_price(double v, int w = 10) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << std::setw(w) << v;
    return ss.str();
}
static std::string format_pct(double v, int w = 7) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << std::setw(w) << v << '%';
    return ss.str();
}
static std::string format_vol(long long v) {
    if (v >= 1'000'000) return std::to_string(v/1'000'000) + "M";
    if (v >= 1'000)     return std::to_string(v/1'000)     + "K";
    return std::to_string(v);
}
static std::string now_str() {
    std::time_t t = std::time(nullptr);
    char buf[16]; std::strftime(buf,sizeof(buf),"%H:%M:%S",std::localtime(&t));
    return buf;
}

static void color_price(WINDOW* w, double val, bool bold=false) {
    int cp = (val > 0) ? CP_GREEN : (val < 0) ? CP_RED : CP_DEFAULT;
    if (bold) wattron(w, A_BOLD | COLOR_PAIR(cp));
    else       wattron(w, COLOR_PAIR(cp));
}
static void color_end(WINDOW* w, bool bold=false) {
    if (bold) wattroff(w, A_BOLD);
    wattroff(w, A_COLOR);
}

// Draw a sparkline chart inside window at (y,x), width w
static void sparkline(WINDOW* win, int y, int x, int w,
                      const std::deque<Candle>& hist) {
    if (hist.empty() || w < 4) return;
    size_t n = std::min((size_t)w, hist.size());
    double mn = hist.back().low, mx = hist.back().high;
    for (size_t i = hist.size()-n; i < hist.size(); ++i) {
        mn = std::min(mn, hist[i].low);
        mx = std::max(mx, hist[i].high);
    }
    if (mx == mn) mx = mn + 0.01;
    static const char* bars = "▁▂▃▄▅▆▇█";
    for (int i = 0; i < w && (size_t)i < n; ++i) {
        const Candle& c = hist[hist.size()-n+i];
        double normalized = (c.close - mn) / (mx - mn);
        int bar = (int)(normalized * 7.0 + 0.5);
        bar = std::max(0, std::min(7, bar));
        // color: green if close > open, red otherwise
        int cp = (c.close >= c.open) ? CP_GREEN : CP_RED;
        wattron(win, COLOR_PAIR(cp));
        // UTF-8 bar chars are multi-byte; mvwaddstr handles them
        char tmp[5] = {};
        // Each bars entry is 3 bytes (UTF-8 ▁..█)
        memcpy(tmp, bars + bar*3, 3);
        mvwaddstr(win, y, x+i, tmp);
        wattroff(win, COLOR_PAIR(cp));
    }
}

// ─── Watchlist panel ─────────────────────────────────────────────

static void draw_watchlist(WINDOW* w, const std::vector<Quote>& quotes,
                           const MarketData& md, int selected, int scroll) {
    int rows, cols;
    getmaxyx(w, rows, cols);
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER)); box(w, 0, 0); wattroff(w, COLOR_PAIR(CP_BORDER));

    // Header bar
    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 1, 1, ' ', cols-2);
    mvwprintw(w, 1, 2, " %-7s %-22s %10s %8s %8s %8s %8s %7s",
              "SYMBOL","NAME","PRICE","CHG","CHG%","HIGH","LOW","VOL");
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

    for (int i = 0; i < rows-4 && (scroll+i) < (int)quotes.size(); ++i) {
        const auto& q = quotes[scroll+i];
        int row = i + 2;
        if ((scroll+i) == selected) wattron(w, COLOR_PAIR(CP_SELECTED) | A_BOLD);
        else if (i % 2 == 0)        wattron(w, A_DIM);

        mvwprintw(w, row, 2, "%-7s %-22s %10.2f",
                  q.symbol.c_str(),
                  q.name.substr(0, 22).c_str(),
                  q.price);

        if ((scroll+i) != selected) wattroff(w, A_DIM);

        // Colored change
        wattron(w, COLOR_PAIR(q.change >= 0 ? CP_GREEN : CP_RED)
                   | ((scroll+i)==selected ? A_BOLD : 0));
        wprintw(w, " %+8.2f %7.2f%%", q.change, q.change_pct);
        wattroff(w, COLOR_PAIR(q.change >= 0 ? CP_GREEN : CP_RED));

        wattron(w, COLOR_PAIR((scroll+i)==selected ? CP_SELECTED : CP_DEFAULT));
        wprintw(w, " %8.2f %8.2f %7s",
                q.high, q.low, format_vol(q.volume).c_str());

        if ((scroll+i) == selected) wattroff(w, COLOR_PAIR(CP_SELECTED) | A_BOLD);

        // Sparkline in last column
        const auto& hist = md.history(q.symbol);
        sparkline(w, row, cols-32, 28, hist);
    }
    // Footer
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, rows-2, 2, " [↑↓] Navigate  [b] Buy  [s] Sell  [a] Alert  [1-5] Panels");
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ─── Portfolio panel ──────────────────────────────────────────────

static void draw_portfolio(WINDOW* w, const Portfolio& port) {
    int rows, cols;
    getmaxyx(w, rows, cols);
    (void)rows; (void)cols;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER)); box(w, 0, 0); wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 1, 1, ' ', cols-2);
    mvwprintw(w, 1, 2, " %-8s %10s %10s %10s %10s %8s %8s",
              "SYMBOL","QTY","AVG COST","PRICE","VALUE","P&L","P&L%");
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

    int row = 2;
    for (const auto& p : port.positions()) {
        if (row >= rows-4) break;
        mvwprintw(w, row, 2, "%-8s %10.2f %10.2f %10.2f %10.2f",
                  p.symbol.c_str(), p.qty, p.avg_cost,
                  p.current_price, p.value());
        double pnl = p.pnl(), pnlp = p.pnl_pct();
        wattron(w, COLOR_PAIR(pnl >= 0 ? CP_GREEN : CP_RED) | A_BOLD);
        wprintw(w, " %+8.2f %+7.2f%%", pnl, pnlp);
        wattroff(w, COLOR_PAIR(pnl >= 0 ? CP_GREEN : CP_RED) | A_BOLD);
        ++row;
    }
    // Summary bar
    mvwhline(w, row+1, 1, ACS_HLINE, cols-2);
    double tv = port.total_value(), tp = port.total_pnl(), tpp = port.total_pnl_pct();
    wattron(w, A_BOLD);
    mvwprintw(w, row+2, 2, "Cash: $%10.2f   Total Value: $%10.2f   ",
              port.cash(), tv);
    wattroff(w, A_BOLD);
    wattron(w, COLOR_PAIR(tp >= 0 ? CP_GREEN : CP_RED) | A_BOLD);
    wprintw(w, "Total P&L: %+.2f  (%+.2f%%)", tp, tpp);
    wattroff(w, COLOR_PAIR(tp >= 0 ? CP_GREEN : CP_RED) | A_BOLD);
}

// ─── Chart panel ──────────────────────────────────────────────────

static void draw_chart(WINDOW* w, const std::string& sym,
                       const std::deque<Candle>& hist, const Quote* q) {
    int rows, cols;
    getmaxyx(w, rows, cols);
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER)); box(w, 0, 0); wattroff(w, COLOR_PAIR(CP_BORDER));

    if (!q || hist.empty()) {
        mvwprintw(w, rows/2, cols/2-6, "No data for %s", sym.c_str());
        return;
    }

    // Title
    wattron(w, A_BOLD | COLOR_PAIR(CP_CYAN));
    mvwprintw(w, 1, 2, " %s — %s    $%.2f ", sym.c_str(), q->name.c_str(), q->price);
    wattroff(w, A_BOLD | COLOR_PAIR(CP_CYAN));
    wattron(w, COLOR_PAIR(q->change >= 0 ? CP_GREEN : CP_RED) | A_BOLD);
    wprintw(w, "  %+.2f (%+.2f%%)", q->change, q->change_pct);
    wattroff(w, COLOR_PAIR(q->change >= 0 ? CP_GREEN : CP_RED) | A_BOLD);

    // Chart area
    int ch = rows - 6, cw = cols - 12;
    if (ch < 4 || cw < 10) return;

    size_t n = std::min((size_t)cw, hist.size());
    double mn = 1e18, mx = -1e18;
    for (size_t i = hist.size()-n; i < hist.size(); ++i) {
        mn = std::min(mn, hist[i].low);
        mx = std::max(mx, hist[i].high);
    }
    if (mx == mn) { mx += 0.1; mn -= 0.1; }
    double range = mx - mn;

    // Y axis labels
    for (int r = 0; r < ch; ++r) {
        double price = mx - r * range / (ch-1);
        wattron(w, COLOR_PAIR(CP_DIM));
        mvwprintw(w, r+3, 2, "%8.2f│", price);
        wattroff(w, COLOR_PAIR(CP_DIM));
    }

    // Candle rendering (simplified: just close price line)
    int x0 = 11;
    for (int i = 0; i < cw && (size_t)i < n; ++i) {
        const Candle& c = hist[hist.size()-n+i];
        int y_close = (int)((mx - c.close) / range * (ch-1));
        int y_open  = (int)((mx - c.open)  / range * (ch-1));
        y_close = std::max(0, std::min(ch-1, y_close));
        y_open  = std::max(0, std::min(ch-1, y_open));

        int cp = (c.close >= c.open) ? CP_GREEN : CP_RED;
        wattron(w, COLOR_PAIR(cp));
        // Draw vertical line for body
        int y1 = std::min(y_close, y_open);
        int y2 = std::max(y_close, y_open);
        for (int y = y1; y <= y2; ++y)
            mvwaddch(w, y+3, x0+i, (y == y_close) ? '*' : '|');
        wattroff(w, COLOR_PAIR(cp));
    }

    // X axis
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwhline(w, ch+3, x0, ACS_HLINE, cw);
    mvwprintw(w, ch+4, x0, "← %d ticks", (int)n);
    wattroff(w, COLOR_PAIR(CP_DIM));
}

// ─── Options Calculator panel ─────────────────────────────────────

static void draw_options(WINDOW* w, double S) {
    int rows, cols;
    getmaxyx(w, rows, cols);
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER)); box(w, 0, 0); wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, A_BOLD | COLOR_PAIR(CP_CYAN));
    mvwprintw(w, 1, 2, " ⚙  Black-Scholes Options Calculator  (ATM)");
    wattroff(w, A_BOLD | COLOR_PAIR(CP_CYAN));

    // Parameters
    double r = 0.05, vol = 0.25;
    double Ts[] = {7.0/365, 30.0/365, 90.0/365, 180.0/365};
    const char* Tlabels[] = {"7D","30D","90D","180D"};

    // Table header
    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 3, 1, ' ', cols-2);
    mvwprintw(w, 3, 2, " %-6s %10s %10s %10s %10s %10s %10s %10s",
              "EXPIRY","CALL","PUT","Δ CALL","Δ PUT","GAMMA","VEGA","THETA");
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

    for (int i = 0; i < 4; ++i) {
        auto res = black_scholes(S, S, Ts[i], r, vol);
        mvwprintw(w, 5+i*2, 2, " %-6s %10.2f %10.2f",
                  Tlabels[i], res.call_price, res.put_price);
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " %10.4f", res.call_delta);
        wattroff(w, COLOR_PAIR(CP_GREEN));
        wattron(w, COLOR_PAIR(CP_RED));
        wprintw(w, " %10.4f", res.put_delta);
        wattroff(w, COLOR_PAIR(CP_RED));
        wattron(w, COLOR_PAIR(CP_YELLOW));
        wprintw(w, " %10.6f %10.4f %10.4f", res.gamma, res.vega, res.theta);
        wattroff(w, COLOR_PAIR(CP_YELLOW));
    }

    // Parameters display
    wattron(w, COLOR_PAIR(CP_DIM));
    mvwprintw(w, rows-3, 2, " Spot: $%.2f   Strike: ATM   r: %.1f%%   σ: %.0f%%   Model: Black-Scholes (1973)",
              S, r*100, vol*100);
    wattroff(w, COLOR_PAIR(CP_DIM));
    (void)rows; (void)cols;
}

// ─── News Feed panel ──────────────────────────────────────────────

static void draw_news(WINDOW* w, const NewsFeed& feed) {
    int rows, cols;
    getmaxyx(w, rows, cols);
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER)); box(w, 0, 0); wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, A_BOLD | COLOR_PAIR(CP_CYAN));
    mvwprintw(w, 1, 2, " 📰  MARKET NEWS — LIVE FEED");
    wattroff(w, A_BOLD | COLOR_PAIR(CP_CYAN));

    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 2, 1, ' ', cols-2);
    mvwprintw(w, 2, 2, " %-8s %-12s  %s", "TIME","SOURCE","HEADLINE");
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);

    int row = 3;
    for (const auto& item : feed.items()) {
        if (row >= rows-2) break;
        if (item.urgent) wattron(w, COLOR_PAIR(CP_RED) | A_BOLD);
        else if (row % 2 == 0) wattron(w, A_DIM);

        mvwprintw(w, row, 2, " %-8s", item.time_str.c_str());
        if (item.urgent) {
            wattron(w, COLOR_PAIR(CP_BLACK_ON_RED));
            wprintw(w, " %-12s", item.source.c_str());
            wattroff(w, COLOR_PAIR(CP_BLACK_ON_RED));
            wattron(w, COLOR_PAIR(CP_RED) | A_BOLD);
        } else {
            wattron(w, COLOR_PAIR(CP_CYAN));
            wprintw(w, " %-12s", item.source.c_str());
            wattroff(w, COLOR_PAIR(CP_CYAN));
        }
        // Truncate headline to fit
        std::string h = item.headline;
        if ((int)h.size() > cols-28) h = h.substr(0, cols-31) + "...";
        wprintw(w, "  %s", h.c_str());

        if (item.urgent) wattroff(w, COLOR_PAIR(CP_RED) | A_BOLD);
        else             wattroff(w, A_DIM);
        ++row;
    }
}

// ─── Top header bar ───────────────────────────────────────────────

static void draw_header(WINDOW* w, int active_panel) {
    werase(w);
    wattron(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwhline(w, 0, 0, ' ', getmaxx(w));
    mvwprintw(w, 0, 1, "  BLOOMBERG  TERMINAL  ");
    wattroff(w, A_BOLD);

    const char* tabs[] = {"[1] MARKET","[2] PORTFOLIO","[3] CHART","[4] OPTIONS","[5] NEWS"};
    int x = 24;
    for (int i = 0; i < 5; ++i) {
        if (i == active_panel-1) {
            wattroff(w, COLOR_PAIR(CP_HEADER));
            wattron(w, A_BOLD | A_UNDERLINE);
            mvwprintw(w, 0, x, " %s ", tabs[i]);
            wattroff(w, A_BOLD | A_UNDERLINE);
            wattron(w, COLOR_PAIR(CP_HEADER));
        } else {
            mvwprintw(w, 0, x, " %s ", tabs[i]);
        }
        x += (int)strlen(tabs[i]) + 3;
    }

    // Clock on the right
    std::string ts = now_str();
    mvwprintw(w, 0, getmaxx(w) - (int)ts.size() - 2, "%s", ts.c_str());
    wattroff(w, COLOR_PAIR(CP_HEADER) | A_BOLD);
}

// ─── Input prompt ─────────────────────────────────────────────────

static std::string prompt_input(WINDOW* status_win, const std::string& msg) {
    int cols = getmaxx(status_win);
    werase(status_win);
    wattron(status_win, COLOR_PAIR(CP_BLACK_ON_YELLOW) | A_BOLD);
    mvwhline(status_win, 0, 0, ' ', cols);
    mvwprintw(status_win, 0, 1, " %s", msg.c_str());
    wattroff(status_win, COLOR_PAIR(CP_BLACK_ON_YELLOW) | A_BOLD);

    echo();
    curs_set(1);
    char buf[64] = {};
    mvwgetnstr(status_win, 0, (int)msg.size()+3, buf, 32);
    curs_set(0);
    noecho();
    return buf;
}

// ─── Status bar ───────────────────────────────────────────────────

static void draw_status(WINDOW* w, const std::string& msg, bool ok = true) {
    int cols = getmaxx(w);
    werase(w);
    int cp = ok ? CP_BLACK_ON_GREEN : CP_BLACK_ON_RED;
    wattron(w, COLOR_PAIR(cp) | A_BOLD);
    mvwhline(w, 0, 0, ' ', cols);
    mvwprintw(w, 0, 2, "%s", msg.c_str());
    wattroff(w, COLOR_PAIR(cp) | A_BOLD);
}

// ─── Main ─────────────────────────────────────────────────────────

int main() {
    // ncurses init
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);
    set_escdelay(25);

    if (!has_colors()) {
        endwin();
        puts("Your terminal does not support colors. Please use a modern terminal.");
        return 1;
    }
    init_colors();

    int ROWS, COLS;
    getmaxyx(stdscr, ROWS, COLS);
    if (ROWS < 24 || COLS < 100) {
        endwin();
        puts("Terminal too small. Please resize to at least 100x24 and retry.");
        return 1;
    }

    // Windows
    WINDOW* win_header = newwin(1,    COLS, 0,      0);
    WINDOW* win_main   = newwin(ROWS-2, COLS, 1,    0);
    WINDOW* win_status = newwin(1,    COLS, ROWS-1, 0);

    // Data
    MarketData   md;
    Portfolio    portfolio;
    NewsFeed     news;
    std::vector<Alert> alerts;
    portfolio.load("portfolio.csv");

    int  active_panel = 1;
    int  selected     = 0;
    int  scroll       = 0;
    std::string chart_sym = "AAPL";
    std::string status_msg = "Welcome to Bloomberg Terminal  |  Press 'q' to quit";
    bool status_ok = true;
    int  tick_counter = 0;

    while (true) {
        // Tick data every 5 iterations (≈500ms)
        if (++tick_counter % 5 == 0) {
            md.tick();
            news.tick();
            portfolio.update_prices(md);
            // Check alerts
            auto fired = md.check_alerts(alerts);
            if (!fired.empty()) {
                std::ostringstream ss;
                ss << "⚡ ALERT: " << fired[0]->symbol
                   << (fired[0]->above ? " ≥ " : " ≤ ")
                   << fired[0]->target_price;
                status_msg = ss.str();
                status_ok = false;
            }
        }

        auto quotes = md.all();
        // Sort alphabetically
        std::sort(quotes.begin(), quotes.end(),
            [](const Quote& a, const Quote& b){ return a.symbol < b.symbol; });

        // Render
        draw_header(win_header, active_panel);
        wrefresh(win_header);

        switch (active_panel) {
            case 1: draw_watchlist(win_main, quotes, md, selected, scroll); break;
            case 2: draw_portfolio(win_main, portfolio); break;
            case 3: {
                const auto* q = md.get(chart_sym);
                draw_chart(win_main, chart_sym, md.history(chart_sym), q);
                break;
            }
            case 4: {
                const auto* q = md.get(chart_sym);
                draw_options(win_main, q ? q->price : 100.0);
                break;
            }
            case 5: draw_news(win_main, news); break;
        }
        wrefresh(win_main);

        draw_status(win_status, status_msg, status_ok);
        wrefresh(win_status);

        // Input
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        switch (ch) {
            case '1': active_panel = 1; break;
            case '2': active_panel = 2; break;
            case '3': active_panel = 3; break;
            case '4': active_panel = 4; break;
            case '5': active_panel = 5; break;

            case KEY_UP:
                if (selected > 0) { --selected; if (selected < scroll) --scroll; }
                break;
            case KEY_DOWN:
                if (selected < (int)quotes.size()-1) {
                    ++selected;
                    int vis = ROWS-5;
                    if (selected >= scroll+vis) ++scroll;
                }
                break;

            case '\n': case KEY_ENTER:
                if (active_panel == 1 && selected < (int)quotes.size()) {
                    chart_sym = quotes[selected].symbol;
                    active_panel = 3;
                    status_msg = "Chart: " + chart_sym;
                    status_ok = true;
                }
                break;

            case 'b': case 'B': {
                std::string sym = prompt_input(win_status, "Buy symbol: ");
                if (sym.empty()) break;
                std::string sq  = prompt_input(win_status, "Quantity: ");
                const auto* q = md.get(sym);
                if (!q) { status_msg = "Symbol not found: " + sym; status_ok = false; break; }
                double qty = std::stod(sq.empty() ? "0" : sq);
                portfolio.buy(sym, qty, q->price);
                portfolio.save("portfolio.csv");
                status_msg = "Bought " + sq + " x " + sym + " @ $" + std::to_string(q->price);
                status_ok = true;
                break;
            }
            case 's': case 'S': {
                std::string sym = prompt_input(win_status, "Sell symbol: ");
                if (sym.empty()) break;
                std::string sq  = prompt_input(win_status, "Quantity: ");
                const auto* q = md.get(sym);
                if (!q) { status_msg = "Symbol not found: " + sym; status_ok = false; break; }
                double qty = std::stod(sq.empty() ? "0" : sq);
                portfolio.sell(sym, qty, q->price);
                portfolio.save("portfolio.csv");
                status_msg = "Sold " + sq + " x " + sym + " @ $" + std::to_string(q->price);
                status_ok = true;
                break;
            }
            case 'a': case 'A': {
                std::string sym = prompt_input(win_status, "Alert symbol: ");
                if (sym.empty()) break;
                std::string sp  = prompt_input(win_status, "Target price: ");
                std::string sd  = prompt_input(win_status, "Direction (above/below): ");
                bool above = (sd == "above" || sd == "a" || sd == "A");
                alerts.push_back({sym, std::stod(sp.empty()?"0":sp), above, false});
                status_msg = "Alert set: " + sym + (above?" ≥ ":" ≤ ") + sp;
                status_ok = true;
                break;
            }
            case 'r': case 'R':
                chart_sym = "SPY";
                status_msg = "Refreshed  •  " + now_str();
                status_ok = true;
                break;
        }

        napms(100); // ~10 fps
    }

    // Cleanup
    portfolio.save("portfolio.csv");
    delwin(win_header);
    delwin(win_main);
    delwin(win_status);
    endwin();
    puts("Bloomberg Terminal closed. Portfolio saved.");
    return 0;
}
