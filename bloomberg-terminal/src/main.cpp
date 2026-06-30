#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include "terminal.hpp"
#include "HighResChart.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

// ─── Author contacts — edit here to update the splash screen ─────────────
static const char* CONTACT_INSTAGRAM = "@sqwerzyyy";
static const char* CONTACT_LINKEDIN  = "linkedin.com/in/oleg-avtonomov-93a437380";
static const char* CONTACT_GITHUB    = "github.com/Sqwerzyyy";

// ─── Candle color scheme presets ─────────────────────────────────────────
struct ColorScheme { const char* name; Color up; Color down; };
static const ColorScheme SCHEMES[] = {
    {"Classic",              Color::RGB( 38, 222, 129), Color::RGB(252,  92, 101)},
    {"Colorblind-friendly",  Color::RGB( 52, 152, 219), Color::RGB(230, 126,  34)},
    {"Cyan / Magenta",       Color::RGB(  0, 210, 211), Color::RGB(255,  71, 182)},
};
static constexpr int SCHEME_COUNT = 3;

// ─── Config persistence ───────────────────────────────────────────────────
static std::string config_path() {
    const char* h = std::getenv("HOME");
    return h ? std::string(h) + "/.bloomberg_terminal_config"
             : ".bloomberg_terminal_config";
}
static int load_scheme_index() {
    std::ifstream f(config_path());
    if (!f.is_open()) return -1;
    int idx = 0; f >> idx;
    return (idx >= 0 && idx < SCHEME_COUNT) ? idx : 0;
}
static void save_scheme_index(int idx) {
    std::ofstream f(config_path());
    if (f.is_open()) f << idx << "\n";
}

// ─── Splash screen — info page ────────────────────────────────────────────
static Element splash_info_page() {
    auto C = [](const char* s) {
        return hbox({filler(), text(s) | color(Color::Cyan) | bold, filler()});
    };
    auto Y = [](const char* s) {
        return hbox({filler(), text(s) | color(Color::Yellow) | bold, filler()});
    };

    auto title_box = vbox({
        text(""),
        C(" ██████╗ ██╗   ██╗ █████╗  █████╗ ███╗   ██╗███████╗"),
        C("██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚════██║"),
        C("██║   ██║██║   ██║███████║███████║██╔██╗ ██║    ██╔╝"),
        C("██║   ██║██║   ██║██╔══██║██╔══██║██║╚██╗██║   ██╔╝ "),
        C("╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║  ██╔╝  "),
        C(" ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝╚══════╝"),
        Y("              T  E  R  M  I  N  A  L              "),
        text(""),
    });

    auto contacts = vbox({
        text(" AUTHOR ") | bold | bgcolor(Color::Cyan) | color(Color::Black),
        hbox({text("  Instagram : ") | color(Color::GrayLight),
              text(CONTACT_INSTAGRAM) | bold | color(Color::Cyan)}),
        hbox({text("  LinkedIn  : ") | color(Color::GrayLight),
              text(CONTACT_LINKEDIN)  | color(Color::White)}),
        hbox({text("  GitHub    : ") | color(Color::GrayLight),
              text(CONTACT_GITHUB)    | color(Color::White)}),
    }) | border;

    auto guide = vbox({
        text(" KEYBOARD CONTROLS ") | bold | bgcolor(Color::Yellow) | color(Color::Black),
        hbox({text("  [1] Markets      ") | color(Color::White),
              text("[2] AI Terminal    ") | color(Color::White),
              text("[3] Risk")            | color(Color::White)}),
        hbox({text("  [↑/↓] Navigate watchlist    ") | color(Color::White),
              text("[Tab] Switch sector")            | color(Color::White)}),
        hbox({text("  [A] Quick AI analysis        ") | color(Color::White),
              text("[+/-] Adjust risk sigma")         | color(Color::White)}),
        text("  [Q] Quit") | color(Color::White),
    }) | border;

    return vbox({
        title_box,
        hbox({contacts | flex, separator(), guide | flex}),
        separator(),
        hbox({filler(),
              text("  Press any key to continue...") | color(Color::GrayDark) | bold,
              filler()}),
        text(""),
    });
}

// ─── Splash screen — color selection page ─────────────────────────────────
static Element splash_color_page(int sel) {
    Elements rows;
    for (int i = 0; i < SCHEME_COUNT; ++i) {
        bool is_sel = (i == sel);
        auto row = hbox({
            text(is_sel ? " ► " : "   ") | bold | color(Color::Yellow),
            text(std::string(1, char('1' + i)) + ". ") | color(Color::GrayLight),
            text(SCHEMES[i].name) | color(Color::White) | (is_sel ? bold : nothing),
            text("     "),
            text("█████") | color(SCHEMES[i].up)   | bold,
            text("  "),
            text("█████") | color(SCHEMES[i].down) | bold,
            filler(),
        });
        if (is_sel) row = row | bgcolor(Color::GrayDark);
        rows.push_back(row);
    }

    return vbox({
        text(""),
        hbox({filler(),
              text(" SELECT CANDLE COLOR SCHEME ") | bold | bgcolor(Color::Yellow) | color(Color::Black),
              filler()}),
        text(""),
        hbox({filler(),
              vbox(std::move(rows)) | border | size(WIDTH, EQUAL, 54),
              filler()}),
        text(""),
        hbox({filler(),
              text("  [↑/↓] select   [Enter] confirm") | color(Color::GrayDark) | bold,
              filler()}),
        text(""),
    }) | flex;
}

static std::string fmt2(double v) {
    std::ostringstream s; s << std::fixed << std::setprecision(2) << v; return s.str();
}
static std::string fmtpct(double v) {
    std::ostringstream s; s << std::fixed << std::setprecision(2);
    if (v >= 0) s << "+";
    s << v << "%";
    return s.str();
}
static std::string fmtvol(long long v) {
    if (v >= 1'000'000'000) return std::to_string(v/1'000'000'000)+"B";
    if (v >= 1'000'000)     return std::to_string(v/1'000'000)+"M";
    if (v >= 1'000)         return std::to_string(v/1'000)+"K";
    return std::to_string(v);
}
static Color pct_color(double v) { return v >= 0 ? Color::Green : Color::Red; }

static Color sector_color(const std::string& s) {
    if (s == "Technology")  return Color::Cyan;
    if (s == "Finance")     return Color::Blue;
    if (s == "Crypto")      return Color::Yellow;
    if (s == "Commodities") return Color::Green;
    if (s == "RWA")         return Color::Magenta;
    return Color::White;
}

struct AppState {
    MarketData     md;
    NewsFeed       news;
    MacroDashboard macro;
    AIAnalyst      ai;
    MarketSummary  summary;
    std::vector<SectorPerf> sectors;

    std::string  chart_sym    = "AAPL";
    int          watch_sel    = 0;
    int          sector_sel   = 0;
    int          news_scroll  = 0;
    int          chat_scroll  = 0;
    double       mc_sigma     = 0.25;
    int          mc_sims      = 1000;
    int          mc_days      = 30;

    std::atomic<bool> running{true};
    std::mutex        mu;

    Color candle_up_color   = Color::RGB( 38, 222, 129);
    Color candle_down_color = Color::RGB(252,  92, 101);
};

Element render_markets(AppState& st) {
    std::lock_guard<std::mutex> lk(st.mu);

    auto sector_list = st.md.sectors();
    std::string active_sector = sector_list[std::min(st.sector_sel, (int)sector_list.size()-1)];
    auto quotes = active_sector == "All" ? st.md.all() : st.md.by_sector(active_sector);
    std::sort(quotes.begin(), quotes.end(),
        [](const Quote& a, const Quote& b){ return a.symbol < b.symbol; });

    Elements sector_tabs;
    for (int i = 0; i < (int)sector_list.size(); ++i) {
        bool active = (i == st.sector_sel);
        auto label = text(" " + sector_list[i] + " ");
        if (active)
            sector_tabs.push_back(label | bold | bgcolor(sector_color(sector_list[i])) | color(Color::Black));
        else
            sector_tabs.push_back(label | color(sector_color(sector_list[i])));
        sector_tabs.push_back(text(" "));
    }

    Elements watch_rows;
    watch_rows.push_back(
        hbox({
            text("SYM   ") | bold | color(Color::Cyan),
            text("  PRICE   ") | bold | color(Color::Cyan),
            text("CHG%   ") | bold | color(Color::Cyan),
            text("SPARK  ") | bold | color(Color::Cyan),
        }) | bgcolor(Color::GrayDark)
    );
    int vis_start = std::max(0, st.watch_sel - 12);
    for (int i = vis_start; i < (int)quotes.size() && i < vis_start + 26; ++i) {
        const auto& q = quotes[i];
        bool sel = (i == st.watch_sel);
        auto spark = render_sparkline(st.md.history(q.symbol), 7);
        Color sc = sector_color(q.sector);
        auto row = hbox({
            text(q.symbol.substr(0,6)) | color(sc) | (sel ? bold : nothing),
            text(" "),
            text(fmt2(q.price)) | color(Color::White) | (sel ? bold : nothing),
            text(" "),
            text(fmtpct(q.change_pct)) | color(pct_color(q.change_pct)) | bold,
            text(" "),
            text(spark) | color(pct_color(q.change_pct)),
        });
        if (sel) row = row | bgcolor(Color::GrayDark) | inverted;
        watch_rows.push_back(row);
    }
    watch_rows.push_back(
        text(std::to_string(quotes.size()) + " assets  [tab] sector") | color(Color::GrayDark)
    );

    const Quote* cq = st.md.get(st.chart_sym);
    const auto&  ch = st.md.history(st.chart_sym);
    auto cd = build_chart_data(st.chart_sym, ch, cq);

    Element chart_title;
    if (cq) {
        Color tc = pct_color(cq->change_pct);
        chart_title = hbox({
            text(" " + st.chart_sym + " ") | bold | bgcolor(Color::Cyan) | color(Color::Black),
            text("  $" + fmt2(cq->price)) | bold | color(tc),
            text("  " + fmtpct(cq->change_pct)) | bold | color(tc),
            text("  " + cq->name.substr(0, 22)) | color(Color::GrayLight),
            text("  [" + cq->sector + "]") | color(sector_color(cq->sector)),
        });
    } else {
        chart_title = text(" " + st.chart_sym + " ") | bold | color(Color::Cyan);
    }
    Color line_color = cq ? pct_color(cq->change_pct) : Color::Cyan;
    Element chart_panel = vbox({
        chart_title | size(HEIGHT, EQUAL, 1),
        separator(),
        candle_chart_element(cd, st.candle_up_color, st.candle_down_color) | flex,
    }) | border | flex;

    Elements stats;
    stats.push_back(text(" QUOTE ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    if (cq) {
        auto sr = [](const std::string& k, const std::string& v, Color c = Color::White) {
            return hbox({ text(k) | color(Color::GrayLight), filler(), text(v) | bold | color(c) });
        };
        stats.push_back(sr("Open   ", "$" + fmt2(cq->open)));
        stats.push_back(sr("High   ", "$" + fmt2(cq->high), Color::Green));
        stats.push_back(sr("Low    ", "$" + fmt2(cq->low),  Color::Red));
        stats.push_back(sr("Volume ", fmtvol(cq->volume)));
        if (cq->market_cap > 0)
            stats.push_back(sr("MktCap ", fmt2(cq->market_cap) + "B"));
        stats.push_back(separator());
        stats.push_back(text(" 52W RANGE ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
        stats.push_back(sr("High  ", "$" + fmt2(cq->week52_high), Color::Green));
        stats.push_back(sr("Low   ", "$" + fmt2(cq->week52_low),  Color::Red));
        double range = cq->week52_high - cq->week52_low;
        double pos = range > 0 ? (cq->price - cq->week52_low) / range : 0.5;
        int bw = 13, bp = (int)(pos * bw);
        std::string bar = "[";
        for (int i = 0; i < bw; ++i) bar += (i == bp ? "o" : "-");
        bar += "]";
        stats.push_back(text(bar) | color(Color::Cyan));
        stats.push_back(separator());
        stats.push_back(text(" RISK ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
        std::vector<double> rets;
        for (size_t i = 1; i < ch.size(); ++i)
            if (ch[i-1].close > 0) rets.push_back((ch[i].close - ch[i-1].close) / ch[i-1].close);
        if (!rets.empty()) {
            std::sort(rets.begin(), rets.end());
            double var95 = rets[(int)(rets.size() * 0.05)];
            double mean = 0; for (double r : rets) mean += r; mean /= rets.size();
            double sd = 0; for (double r : rets) sd += (r-mean)*(r-mean);
            sd = std::sqrt(sd / rets.size());
            stats.push_back(sr("VaR 95%", fmt2(var95 * 100) + "%", Color::Red));
            stats.push_back(sr("Std Dev", fmt2(sd * 100) + "%",    Color::Yellow));
        }
        stats.push_back(separator());
        bool live = cq->live;
        stats.push_back(hbox({
            text(live ? "● LIVE" : "○ ...") | color(live ? Color::Green : Color::GrayDark) | bold,
        }));
        stats.push_back(text("auto-refresh enabled") | color(Color::GrayDark));
    }

    return vbox({
        hbox(sector_tabs) | size(HEIGHT, EQUAL, 1),
        separator(),
        hbox({
            vbox(watch_rows) | border | size(WIDTH, EQUAL, 27),
            separator(),
            chart_panel,
            separator(),
            vbox(stats) | border | size(WIDTH, EQUAL, 22),
        }) | flex,
    });
}

struct AITerminalState {
    std::string input_text;
    int         chat_offset  = 0;
    int         news_offset  = 0;
};

Element render_ai_terminal(AppState& st, const AITerminalState& ait, Element model_row) {
    std::lock_guard<std::mutex> lk(st.mu);
    const auto& news_items = st.news.items();

    Elements news_rows;
    news_rows.push_back(
        hbox({
            text(" TIME    ") | bold | color(Color::Cyan),
            text("SOURCE         ") | bold | color(Color::Cyan),
            text("HEADLINE") | bold | color(Color::Cyan) | flex,
        }) | bgcolor(Color::GrayDark)
    );
    int news_start = std::max(0, ait.news_offset);
    for (int i = news_start; i < (int)news_items.size() && i < news_start + 6; ++i) {
        const auto& item = news_items[i];
        news_rows.push_back(hbox({
            text(" " + item.time_str + " ") | color(Color::GrayLight),
            text(item.source.substr(0,12) + "  ") | color(Color::Yellow),
            text(item.headline.substr(0, 60)) | flex
                | color(item.urgent ? Color::Red : Color::White)
                | (item.urgent ? bold : nothing),
        }));
    }

    Elements chat_rows;
    const auto& history = st.ai.history();
    int h_start = std::max(0, (int)history.size() - 20 + ait.chat_offset);
    h_start = std::max(0, std::min(h_start, (int)history.size() - 1));

    if (history.empty()) {
        chat_rows.push_back(text("") );
        chat_rows.push_back(
            text("  Welcome to AI Command Center") | bold | color(Color::Cyan)
        );
        chat_rows.push_back(text("") );
        chat_rows.push_back(
            paragraph("  Ask anything about markets, sectors, or specific assets. "
                       "Market context is automatically included in every request.")
            | color(Color::GrayLight)
        );
        chat_rows.push_back(text("") );
        chat_rows.push_back(
            text("  Examples:") | color(Color::Yellow)
        );
        chat_rows.push_back(text("  > Analyze impact of recent news on RWA sector") | color(Color::GrayDark));
        chat_rows.push_back(text("  > Give me a summary on NVDA") | color(Color::GrayDark));
        chat_rows.push_back(text("  > Which crypto assets show strongest momentum?") | color(Color::GrayDark));
        chat_rows.push_back(text("  > Compare Tech vs Finance sector today") | color(Color::GrayDark));
        chat_rows.push_back(text("") );
        if (!st.ai.has_key()) {
            chat_rows.push_back(
                text("  ⚠  No AI backend available") | color(Color::Yellow) | bold
            );
            chat_rows.push_back(
                text("     Free option: ollama serve  (then: ollama pull llama3.2)") | color(Color::GrayDark)
            );
            chat_rows.push_back(
                text("     Paid option:  export ANTHROPIC_API_KEY=sk-ant-...") | color(Color::GrayDark)
            );
        } else {
            chat_rows.push_back(
                text("  Backend: " + st.ai.backend_label()) | color(Color::GrayDark)
            );
        }
    }

    for (int i = h_start; i < (int)history.size(); ++i) {
        const auto& msg = history[i];
        bool is_user = (msg.role == ChatMessage::USER);
        if (is_user) {
            chat_rows.push_back(
                hbox({
                    text(" [" + msg.timestamp + "] ") | color(Color::GrayDark),
                    text("YOU: ") | bold | color(Color::Cyan),
                    text(msg.text) | bold | color(Color::White),
                })
            );
        } else {
            chat_rows.push_back(
                hbox({
                    text(" [" + msg.timestamp + "] ") | color(Color::GrayDark),
                    text(st.ai.backend_label() + ": ") | bold | color(Color::Yellow),
                })
            );

            std::istringstream words(msg.text);
            std::string line, word;
            while (words >> word) {
                if (line.size() + word.size() > 75) {
                    chat_rows.push_back(text("         " + line) | color(Color::White));
                    line = word + " ";
                } else {
                    line += word + " ";
                }
            }
            if (!line.empty())
                chat_rows.push_back(text("         " + line) | color(Color::White));
            chat_rows.push_back(text("") );
        }
    }

    if (st.ai.is_fetching()) {
        static int spin = 0; ++spin;
        const char* frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        chat_rows.push_back(
            hbox({
                text("  "),
                text(frames[spin % 10]) | color(Color::Yellow),
                text("  " + st.ai.backend_label() + " is thinking...") | color(Color::Yellow) | bold,
            })
        );
    }

    return vbox({
        model_row        | size(HEIGHT, LESS_THAN, 14),
        vbox(news_rows)  | border | size(HEIGHT, EQUAL, 9),
        vbox(chat_rows)  | border | flex,
    });
}

struct MCResult {
    std::vector<double> percentiles, distribution;
    double mean_final = 0, var_95 = 0, prob_gain = 0;
};
static MCResult run_mc(double S0, double sigma, int days, int sims) {
    MCResult r;
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, sigma / std::sqrt(252.0));
    std::vector<double> finals(sims);
    for (int i = 0; i < sims; ++i) {
        double S = S0; for (int d = 0; d < days; ++d) S *= (1.0 + nd(rng)); finals[i] = S;
    }
    std::sort(finals.begin(), finals.end());
    auto pct = [&](double p){ return finals[(int)(p * (sims-1))]; };
    r.percentiles = {pct(0.05), pct(0.25), pct(0.50), pct(0.75), pct(0.95)};
    r.mean_final = 0; for (double v : finals) r.mean_final += v; r.mean_final /= sims;
    r.var_95 = (finals[(int)(0.05*sims)] - S0) / S0 * 100.0;
    r.prob_gain = (double)std::count_if(finals.begin(), finals.end(),
        [S0](double v){ return v > S0; }) / sims * 100.0;
    int B = 24; double lo = finals.front(), hi = finals.back(), bw = (hi - lo) / B;
    r.distribution.assign(B, 0);
    for (double v : finals) { int b = std::min(B-1, (int)((v-lo)/bw)); r.distribution[b]++; }
    double mb = *std::max_element(r.distribution.begin(), r.distribution.end());
    for (auto& b : r.distribution) b /= mb;
    return r;
}

// ─── Probability distribution — dynamic-height custom node ───────────────
namespace {
class DistributionChartNode : public Node {
public:
    explicit DistributionChartNode(MCResult mc) : mc_(std::move(mc)) {}

    void ComputeRequirement() override {
        requirement_.min_x         = 24;
        requirement_.min_y         = 2;
        requirement_.flex_grow_x   = 1;
        requirement_.flex_grow_y   = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_shrink_y = 1;
    }

    void Render(Screen& screen) override {
        int by0 = box_.y_min, by1 = box_.y_max;
        int bx0 = box_.x_min, bx1 = box_.x_max;
        int total_h = by1 - by0 + 1;
        if (total_h < 1) return;

        static constexpr int LABEL_W = 11;  // "XXXXX.XX |"
        int    B     = (int)mc_.distribution.size();
        double p5    = mc_.percentiles[0];
        double p95   = mc_.percentiles[4];
        double range = p95 - p5;

        for (int row_from_top = 0; row_from_top < total_h; ++row_from_top) {
            int row = total_h - 1 - row_from_top;  // 0 = bottom = lowest price
            int sy  = by0 + row_from_top;
            if (sy > by1) break;

            // Price label (left column)
            double price = (total_h > 1)
                ? p5 + (double)row / (total_h - 1) * range
                : p5;
            std::ostringstream lbl;
            lbl << std::fixed << std::setprecision(2)
                << std::setw(LABEL_W - 2) << price << " |";
            std::string ls = lbl.str();
            if ((int)ls.size() > LABEL_W) ls = ls.substr(0, LABEL_W);
            for (int c = 0; c < (int)ls.size() && bx0 + c <= bx1; ++c) {
                Pixel& px = screen.PixelAt(bx0 + c, sy);
                px.character        = std::string(1, ls[c]);
                px.foreground_color = Color::GrayLight;
            }

            // Color gradient: bottom (low price) = Red, mid = Yellow, top = Green
            double frac = (total_h > 1) ? (double)row / (total_h - 1) : 1.0;
            Color bar_color = frac < 0.25 ? Color::Red
                            : frac < 0.50 ? Color::Yellow
                            :               Color::Green;

            // Bar characters for each distribution bin
            for (int b = 0; b < B; ++b) {
                int sx = bx0 + LABEL_W + b;
                if (sx > bx1) break;
                double h = mc_.distribution[b] * total_h;
                const char* ch;
                if      (h >= row + 1.00) ch = "█";
                else if (h >= row + 0.75) ch = "▇";
                else if (h >= row + 0.50) ch = "▄";
                else if (h >= row + 0.25) ch = "▂";
                else                      ch = " ";
                Pixel& px = screen.PixelAt(sx, sy);
                px.character        = ch;
                px.foreground_color = bar_color;
            }
        }
    }

private:
    MCResult mc_;
};
} // namespace

static Element distribution_chart_element(MCResult mc) {
    return std::make_shared<DistributionChartNode>(std::move(mc));
}

Element render_risk(AppState& st) {
    std::lock_guard<std::mutex> lk(st.mu);
    const Quote* q = st.md.get(st.chart_sym);
    double S0 = q ? q->price : 100.0;
    auto mc = run_mc(S0, st.mc_sigma, st.mc_days, st.mc_sims);

    Elements params;
    params.push_back(text(" PARAMETERS ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    auto pr = [](const std::string& k, const std::string& v, Color c = Color::White) {
        return hbox({ text(k) | color(Color::GrayLight), filler(), text(v) | bold | color(c) });
    };
    params.push_back(pr("Asset   ", st.chart_sym));
    params.push_back(pr("Price   ", "$" + fmt2(S0)));
    params.push_back(pr("Sigma   ", fmt2(st.mc_sigma * 100) + "%", Color::Cyan));
    params.push_back(pr("Horizon ", std::to_string(st.mc_days) + " days  [T]", Color::Cyan));
    params.push_back(pr("Sims    ", std::to_string(st.mc_sims), Color::Cyan));
    params.push_back(separator());
    params.push_back(text(" RISK METRICS ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    params.push_back(pr("VaR 95%  ", fmt2(mc.var_95) + "%", mc.var_95 < 0 ? Color::Red : Color::Green));
    params.push_back(pr("E[Price] ", "$" + fmt2(mc.mean_final)));
    params.push_back(pr("P(gain)  ", fmt2(mc.prob_gain) + "%", mc.prob_gain >= 50 ? Color::Green : Color::Red));
    params.push_back(separator());
    params.push_back(text(" PERCENTILES ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    std::vector<std::pair<std::string,int>> plist = {{"P5",0},{"P25",1},{"P50",2},{"P75",3},{"P95",4}};
    for (auto& [lbl, idx] : plist) {
        double val = mc.percentiles[idx], chg = (val - S0) / S0 * 100.0;
        params.push_back(hbox({
            text(lbl + ":") | color(Color::GrayLight),
            text(" $" + fmt2(val)) | color(Color::White),
            filler(),
            text(fmtpct(chg)) | bold | color(pct_color(chg)),
        }));
    }
    params.push_back(separator());
    params.push_back(text("[+/-] sigma  [T] horizon  [A] AI analysis") | color(Color::GrayDark));

    auto dist_panel = vbox({
        hbox({
            text(" PROBABILITY DISTRIBUTION  ") | bold | color(Color::Yellow),
            text(std::to_string(st.mc_sims) + " simulations") | color(Color::GrayLight),
        }),
        distribution_chart_element(mc) | flex,
        hbox({
            text("            ") | color(Color::GrayLight),
            text("$" + fmt2(mc.percentiles[0])) | color(Color::Red),
            filler(),
            text("$" + fmt2(mc.percentiles[2])) | color(Color::White),
            filler(),
            text("$" + fmt2(mc.percentiles[4])) | color(Color::Green),
        }),
        hbox({
            text("            ") | color(Color::GrayLight),
            text("P5") | color(Color::Red), filler(),
            text("Median") | color(Color::White), filler(),
            text("P95") | color(Color::Green),
        }),
    });

    Elements sect;
    sect.push_back(text(" SECTOR PERFORMANCE ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    double ma = 0.01;
    for (auto& sp : st.sectors) ma = std::max(ma, std::abs(sp.avg_change_pct));
    for (auto& sp : st.sectors) {
        int bl = (int)(std::abs(sp.avg_change_pct) / ma * 12);
        bool pos = sp.avg_change_pct >= 0;
        std::string bar(bl, pos ? '#' : 'x');
        sect.push_back(hbox({
            text(sp.sector.substr(0,11)) | color(sector_color(sp.sector)) | size(WIDTH, EQUAL, 12),
            text(bar) | color(pos ? Color::Green : Color::Red),
            filler(),
            text(fmtpct(sp.avg_change_pct)) | color(pos ? Color::Green : Color::Red) | bold,
        }));
    }
    sect.push_back(separator());
    sect.push_back(text(" QUICK ANALYSIS ") | bold | bgcolor(Color::Yellow) | color(Color::Black));
    if (st.ai.result().ready) {
        sect.push_back(paragraph(st.ai.result().market_summary) | color(Color::White));
        bool bull = st.ai.result().sentiment.find("BULL") != std::string::npos;
        sect.push_back(text(st.ai.result().sentiment) | bold | color(bull ? Color::Green : Color::Red));
    } else {
        sect.push_back(text("[A] run AI market analysis") | color(Color::GrayDark));
    }

    return hbox({
        vbox(params) | border | size(WIDTH, EQUAL, 32),
        separator(),
        dist_panel   | border | flex,
        separator(),
        vbox(sect)   | border | size(WIDTH, EQUAL, 28),
    });
}

int main() {
    // ─── Load color scheme (skip color-selection step if config exists) ──
    int scheme_idx       = load_scheme_index();
    const bool has_config = (scheme_idx >= 0);
    if (!has_config) scheme_idx = 0;

    // ─── Splash screen ───────────────────────────────────────────────────
    {
        bool in_color_sel = false;
        auto splash = ScreenInteractive::Fullscreen();
        auto rend = Renderer([&]() -> Element {
            return in_color_sel ? splash_color_page(scheme_idx)
                                : splash_info_page();
        });
        auto splash_ev = CatchEvent(rend, [&](Event e) -> bool {
            if (e == Event::Custom) return false;
            if (!in_color_sel) {
                if (!has_config) { in_color_sel = true; return true; }
                splash.ExitLoopClosure()();
                return true;
            }
            if (e == Event::ArrowUp)
                { scheme_idx = (scheme_idx + SCHEME_COUNT - 1) % SCHEME_COUNT; return true; }
            if (e == Event::ArrowDown)
                { scheme_idx = (scheme_idx + 1) % SCHEME_COUNT; return true; }
            if (e == Event::Return)
                { save_scheme_index(scheme_idx); splash.ExitLoopClosure()(); return true; }
            return true;
        });
        splash.Loop(splash_ev);
    }

    // ─── Main application ────────────────────────────────────────────────
    auto state = std::make_shared<AppState>();
    state->candle_up_color   = SCHEMES[scheme_idx].up;
    state->candle_down_color = SCHEMES[scheme_idx].down;

    auto screen = ScreenInteractive::Fullscreen();

    int tab_index = 0;
    std::vector<std::string> tab_names = {
        "  1:MARKETS  ",
        "  2:AI TERMINAL  ",
        "  3:RISK  ",
    };
    auto tab_toggle = Toggle(&tab_names, &tab_index);

    AITerminalState ait;

    std::vector<std::string> model_labels;
    for (auto& m : state->ai.available_models()) model_labels.push_back(m.label);
    if (model_labels.empty()) model_labels.push_back("no AI backend available");
    int model_sel = state->ai.selected_model_index();
    int last_model_sel = model_sel;
    auto model_dropdown = Dropdown(&model_labels, &model_sel);

    InputOption input_opt;
    input_opt.on_enter = [&]() {
        if (ait.input_text.empty()) return;
        std::string prompt = ait.input_text;
        ait.input_text.clear();
        std::lock_guard<std::mutex> lk(state->mu);
        state->ai.chat(prompt, state->md.all(), state->summary);
    };
    auto ai_input = Input(&ait.input_text, "Type your question...", input_opt);

    std::thread tick_thread([&]() {
        int t = 0;
        while (state->running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (state->md.is_loaded() && ++t % 30 == 0) {
                auto q = state->md.all();
                std::lock_guard<std::mutex> lk(state->mu);
                state->summary = compute_market_summary(q);
                state->sectors = compute_sector_perf(q);
            }
            if (state->md.is_loaded() && t % 400 == 0 && !state->md.is_fetching())
                state->md.refresh_all_live();
            screen.PostEvent(Event::Custom);
        }
    });

    static int tape_off = 0;

    auto renderer = Renderer(Container::Vertical({tab_toggle, ai_input, model_dropdown}), [&]() {

        if (model_sel != last_model_sel) {
            state->ai.select_model(model_sel);
            last_model_sel = model_sel;
        }

        std::string tape;
        {
            std::lock_guard<std::mutex> lk(state->mu);
            auto q = state->md.all();
            std::sort(q.begin(), q.end(),
                [](const Quote& a, const Quote& b){ return a.symbol < b.symbol; });
            std::ostringstream ss;
            for (auto& x : q)
                ss << "  " << x.symbol << " $" << std::fixed << std::setprecision(2)
                   << x.price << " " << (x.change_pct >= 0 ? "+" : "") << x.change_pct << "%  |";
            std::string full = ss.str(); full += full;
            int len = (int)full.size() / 2;
            if (len > 0) {
                int off = tape_off % len;
                tape = full.substr(off, 150);
                if ((int)tape.size() < 150) tape += full.substr(0, 150 - tape.size());
                ++tape_off;
            }
        }

        Element body;
        if (!state->md.is_loaded()) {
            int dots = (tape_off / 3) % 4;
            std::string spin = std::string("[CONNECTING TO LIVE MARKET DATA")
                             + std::string(dots, '.') + std::string(3 - dots, ' ') + "]";
            body = vbox({
                filler(),
                hbox({ filler(),
                       text(spin) | bold | color(Color::Cyan),
                       filler() }),
                hbox({ filler(),
                       text(state->md.fetch_status()) | color(Color::GrayLight),
                       filler() }),
                filler(),
            }) | flex;
        } else {
            switch (tab_index) {
                case 0: body = render_markets(*state);               break;
                case 1: body = render_ai_terminal(*state, ait, hbox({
                            text(" MODEL ") | bold | bgcolor(Color::Yellow) | color(Color::Black),
                            text(" "),
                            model_dropdown->Render(),
                            filler(),
                        })); break;
                case 2: body = render_risk(*state);                  break;
                default: body = text("?");
            }
        }

        Elements mini;
        {
            std::lock_guard<std::mutex> lk(state->mu);
            for (const char* sym : {"SPY","BTC","ETH","GLD"}) {
                const Quote* q = state->md.get(sym);
                if (!q) continue;
                mini.push_back(hbox({
                    text(std::string(" ") + sym) | color(Color::GrayLight),
                    text(" " + fmtpct(q->change_pct)) | bold | color(pct_color(q->change_pct)),
                }));
            }
        }

        auto header = hbox({
            text(" QUAANZ ") | bold | bgcolor(Color::Cyan) | color(Color::Black),
            tab_toggle->Render() | flex,
            hbox(mini),
            text(state->md.is_fetching() ? " FETCHING... " : " ")
                | color(state->md.is_fetching() ? Color::Yellow : Color::GrayDark),
        });

        Element footer;
        if (tab_index == 1) {
            footer = hbox({
                text(" AI> ") | bold | bgcolor(Color::Cyan) | color(Color::Black),
                ai_input->Render() | flex,
                text(" [Enter] send  [PgUp/PgDn] scroll  [A] quick analysis ") | color(Color::GrayDark),
            }) | size(HEIGHT, EQUAL, 1);
        } else {
            footer = text(tape) | color(Color::GrayLight) | bgcolor(Color::GrayDark)
                     | size(HEIGHT, EQUAL, 1);
        }

        return vbox({
            header,
            separator(),
            body | flex,
            separator(),
            footer,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) {

        if (e == Event::Character('1')) { tab_index = 0; return true; }
        if (e == Event::Character('2')) { tab_index = 1; return true; }
        if (e == Event::Character('3')) { tab_index = 2; return true; }

        if (e == Event::ArrowLeft) {
            tab_index = std::clamp(tab_index - 1, 0, 2);
            return true;
        }
        if (e == Event::ArrowRight) {
            tab_index = std::clamp(tab_index + 1, 0, 2);
            return true;
        }

        if (e == Event::Character('q') || e == Event::Character('Q')) {
            state->running = false;
            screen.ExitLoopClosure()();
            return true;
        }

        if (!state->md.is_loaded()) return true;

        if (tab_index == 0) {
            auto sector_list = state->md.sectors();
            auto quotes = state->md.by_sector(
                sector_list[std::min(state->sector_sel, (int)sector_list.size()-1)]);
            std::sort(quotes.begin(), quotes.end(),
                [](const Quote& a, const Quote& b){ return a.symbol < b.symbol; });
            int n = (int)quotes.size();

            if (e == Event::ArrowUp) {
                if (n > 0) {
                    state->watch_sel = std::min(state->watch_sel, n - 1);
                    state->watch_sel = std::max(0, state->watch_sel - 1);
                    state->chart_sym = quotes[state->watch_sel].symbol;
                }
                return true;
            }
            if (e == Event::ArrowDown) {
                if (n > 0) {
                    state->watch_sel = std::max(0, std::min(state->watch_sel, n - 1));
                    state->watch_sel = std::min(n - 1, state->watch_sel + 1);
                    state->chart_sym = quotes[state->watch_sel].symbol;
                }
                return true;
            }

            if (e == Event::Tab) {
                state->sector_sel = (state->sector_sel + 1) % (int)sector_list.size();
                state->watch_sel  = 0;
                auto new_quotes = state->md.by_sector(sector_list[state->sector_sel]);
                if (!new_quotes.empty()) {
                    std::sort(new_quotes.begin(), new_quotes.end(),
                        [](const Quote& a, const Quote& b){ return a.symbol < b.symbol; });
                    state->chart_sym = new_quotes[0].symbol;
                }
                return true;
            }

            if (e == Event::TabReverse) {
                state->sector_sel = (state->sector_sel + (int)sector_list.size() - 1) % (int)sector_list.size();
                state->watch_sel  = 0;
                return true;
            }
        }

        if (tab_index == 1) {
            if (e == Event::PageUp)   { ait.chat_offset  = std::max(-50, ait.chat_offset  - 5); return true; }
            if (e == Event::PageDown) { ait.chat_offset  = std::min(0,   ait.chat_offset  + 5); return true; }
            if (e == Event::F1)       { ait.news_offset  = std::max(0,   ait.news_offset  - 1); return true; }
            if (e == Event::F2)       { ait.news_offset++;                                        return true; }

            if (model_dropdown->OnEvent(e)) return true;
            return ai_input->OnEvent(e);
        }

        if (e == Event::Character('A') || e == Event::Character('a')) {
            std::lock_guard<std::mutex> lk(state->mu);
            state->ai.analyze(state->md.all(), state->summary);
            return true;
        }

        if (e == Event::Character('+')) { state->mc_sigma = std::min(2.0, state->mc_sigma + 0.05); return true; }
        if (e == Event::Character('-') && tab_index != 1) { state->mc_sigma = std::max(0.05, state->mc_sigma - 0.05); return true; }

        if ((e == Event::Character('T') || e == Event::Character('t')) && tab_index == 2) {
            int& d = state->mc_days;
            if      (d < 30)  d = 30;
            else if (d < 90)  d = 90;
            else if (d < 180) d = 180;
            else              d = 7;
            return true;
        }

        return false;
    });

    state->md.start_auto_fetch();
    screen.Loop(app);
    state->running = false;
    tick_thread.join();
    return 0;
}