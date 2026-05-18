#pragma once
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>
#include <functional>
#include <memory>
#include <random>

// ─── Data Structures ───────────────────────────────────────────────────────

struct Quote {
    std::string symbol;
    double      price;
    double      change;
    double      change_pct;
    double      open;
    double      high;
    double      low;
    long long   volume;
    std::string name;
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
    bool        above;   // true = alert when price > target, false = when <
    bool        fired = false;
};

struct Position {
    std::string symbol;
    double      qty;
    double      avg_cost;
    double      current_price;

    double pnl()     const { return (current_price - avg_cost) * qty; }
    double pnl_pct() const { return (current_price - avg_cost) / avg_cost * 100.0; }
    double value()   const { return current_price * qty; }
};

struct Candle {
    double open, high, low, close;
};

// ─── Market Data Engine ────────────────────────────────────────────────────

class MarketData {
public:
    MarketData();

    // Refresh all quotes (simulates live tick)
    void tick();

    // Lookup
    const Quote* get(const std::string& symbol) const;
    std::vector<Quote> all() const;

    // Price history (last N candles for sparkline / chart)
    const std::deque<Candle>& history(const std::string& symbol) const;

    // Check and fire alerts
    std::vector<Alert*> check_alerts(std::vector<Alert>& alerts);

private:
    std::map<std::string, Quote>           quotes_;
    std::map<std::string, std::deque<Candle>> hist_;
    std::mt19937                           rng_;
    static constexpr size_t               MAX_HIST = 60;

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

    double total_value()    const;
    double total_cost()     const;
    double total_pnl()      const;
    double total_pnl_pct()  const;

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
    double gamma, vega, theta;
    double call_iv, put_iv;   // implied vol (same for BSM)
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
    static constexpr size_t MAX = 30;
};
