#pragma once
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <atomic>
#include <mutex>
#include <thread>

// ─── Data Structures ───────────────────────────────────────────────────────

struct Quote {
    std::string symbol;
    double      price       = 0;
    double      change      = 0;
    double      change_pct  = 0;
    double      open        = 0;
    double      high        = 0;
    double      low         = 0;
    double      prev_close  = 0;
    long long   volume      = 0;
    double      market_cap  = 0;
    double      pe_ratio    = 0;
    double      week52_high = 0;
    double      week52_low  = 0;
    std::string name;
    std::string sector;
    bool        live        = false;
};

struct NewsItem {
    std::string headline;
    std::string source;
    std::string time_str;
    bool        urgent = false;
};

struct Alert {
    std::string symbol;
    double      target_price;
    bool        above;
    bool        fired = false;
};

struct Position {
    std::string symbol;
    double      qty;
    double      avg_cost;
    double      current_price;

    double pnl()     const { return (current_price - avg_cost) * qty; }
    double pnl_pct() const { return avg_cost > 0 ? (current_price - avg_cost) / avg_cost * 100.0 : 0; }
    double value()   const { return current_price * qty; }
};

struct Candle {
    double    open, high, low, close;
    long long volume = 0;
};

struct MacroIndicator {
    std::string name;
    std::string symbol;
    double      value      = 0;
    double      change     = 0;
    double      change_pct = 0;
    std::string unit;
    std::string desc;
};

// ─── Screener ────────────────────────────────────────────────────────────

struct ScreenerFilter {
    double min_change_pct = -100.0;
    double max_change_pct =  100.0;
    double min_volume     = 0;
    double min_price      = 0;
    double max_price      = 1e9;
    std::string sector    = "";
    enum SortBy { CHANGE_PCT, PRICE, VOLUME, SYMBOL } sort_by = CHANGE_PCT;
    bool sort_desc = true;
};

// ─── Yahoo Finance ────────────────────────────────────────────────────────

struct YahooResult {
    bool        ok          = false;
    double      price       = 0;
    double      prev_close  = 0;
    double      open        = 0;
    double      day_high    = 0;
    double      day_low     = 0;
    double      week52_high = 0;
    double      week52_low  = 0;
    long long   volume      = 0;
    double      market_cap  = 0;
    double      pe_ratio    = 0;
    std::string name;
    std::vector<Candle> history;
};

YahooResult      yahoo_fetch(const std::string& symbol);
std::vector<Candle> yahoo_history(const std::string& symbol);

// ─── Market Data Engine ────────────────────────────────────────────────────

class MarketData {
public:
    MarketData();

    void tick();
    void fetch_live(const std::string& symbol);
    void fetch_all_live();

    const Quote* get(const std::string& symbol) const;
    std::vector<Quote> all() const;

    const std::deque<Candle>& history(const std::string& symbol) const;
    void set_history(const std::string& sym, std::vector<Candle> candles);

    std::vector<Alert*> check_alerts(std::vector<Alert>& alerts);

    bool        is_fetching()  const { return fetching_.load(); }
    std::string fetch_status() const { return fetch_status_; }

private:
    mutable std::mutex           mu_;
    std::map<std::string, Quote> quotes_;
    std::map<std::string, std::deque<Candle>> hist_;
    std::mt19937                 rng_;
    std::atomic<bool>            fetching_{false};
    std::string                  fetch_status_;
    static constexpr size_t      MAX_HIST = 80;

    void init_quotes();
    void random_walk(Quote& q);
    void push_candle(const std::string& sym, const Quote& q);
};

// ─── Portfolio ────────────────────────────────────────────────────────────

class Portfolio {
public:
    void load(const std::string& path);
    void save(const std::string& path) const;
    void buy (const std::string& sym, double qty, double price);
    void sell(const std::string& sym, double qty, double price);
    void update_prices(const MarketData& md);

    double total_value()   const;
    double total_cost()    const;
    double total_pnl()     const;
    double total_pnl_pct() const;

    const std::vector<Position>& positions() const { return positions_; }
    double cash() const { return cash_; }

private:
    std::vector<Position> positions_;
    double cash_ = 100'000.0;
};

// ─── Options Calculator ───────────────────────────────────────────────────

struct OptionResult {
    double call_price, put_price;
    double call_delta, put_delta;
    double gamma, vega, theta, rho;
};

OptionResult black_scholes(double S, double K, double T, double r, double sigma);

// ─── News Feed ───────────────────────────────────────────────────────────

class NewsFeed {
public:
    NewsFeed();
    void tick();
    const std::deque<NewsItem>& items() const { return items_; }
private:
    std::deque<NewsItem> items_;
    std::mt19937         rng_;
    int                  tick_ = 0;
    static constexpr size_t MAX = 40;
};

// ─── Macro Dashboard ─────────────────────────────────────────────────────

class MacroDashboard {
public:
    MacroDashboard();
    void refresh_live();
    void tick();
    const std::vector<MacroIndicator>& indicators() const { return indicators_; }
    bool is_fetching() const { return fetching_.load(); }
private:
    std::vector<MacroIndicator> indicators_;
    std::atomic<bool>           fetching_{false};
    std::mt19937                rng_;
};

// ─── Analytics Engine ────────────────────────────────────────────────────

struct CorrelationMatrix {
    std::vector<std::string>          symbols;
    std::vector<std::vector<double>>  matrix;  // NxN, range [-1, 1]
};

// Compute Pearson correlation from price history
CorrelationMatrix compute_correlation(
    const std::vector<std::string>& symbols,
    const std::function<const std::deque<Candle>*(const std::string&)>& get_hist,
    int periods = 30);

struct GainerLoser {
    std::string symbol;
    std::string name;
    double      price;
    double      change;
    double      change_pct;
    double      volume;
};

struct MarketSummary {
    std::vector<GainerLoser> top_gainers;   // top 5
    std::vector<GainerLoser> top_losers;    // top 5
    std::vector<GainerLoser> top_volume;    // top 5 by volume
    double advances  = 0;  // % of symbols up
    double declines  = 0;
    double unchanged = 0;
    double avg_change_pct = 0;
};

MarketSummary compute_market_summary(const std::vector<Quote>& quotes);

struct SectorPerf {
    std::string sector;
    double      avg_change_pct;
    int         count;
    int         advances;
};

std::vector<SectorPerf> compute_sector_perf(const std::vector<Quote>& quotes);
