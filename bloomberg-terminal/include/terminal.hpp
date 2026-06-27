#pragma once
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <functional>
#include <random>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>

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

struct Candle {
    double    open, high, low, close;
    long long volume = 0;
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

struct MacroIndicator {
    std::string name, symbol, unit, desc;
    double value = 0, change = 0, change_pct = 0;
};

struct ScreenerFilter {
    double min_change_pct = -100, max_change_pct = 100;
    double min_volume = 0, min_price = 0, max_price = 1e9;
    std::string sector;
    enum SortBy { CHANGE_PCT, PRICE, VOLUME, SYMBOL } sort_by = CHANGE_PCT;
    bool sort_desc = true;
};

struct GainerLoser {
    std::string symbol, name;
    double price, change, change_pct, volume;
};

struct MarketSummary {
    std::vector<GainerLoser> top_gainers, top_losers, top_volume;
    double advances = 0, declines = 0, unchanged = 0, avg_change_pct = 0;
};

struct SectorPerf {
    std::string sector;
    double avg_change_pct = 0;
    int count = 0, advances = 0;
};

struct CorrelationMatrix {
    std::vector<std::string>         symbols;
    std::vector<std::vector<double>> matrix;
};

struct YahooResult {
    bool        ok         = false;
    double      price      = 0, prev_close = 0, open = 0;
    double      day_high   = 0, day_low    = 0;
    double      week52_high= 0, week52_low = 0;
    long long   volume     = 0;
    double      market_cap = 0, pe_ratio   = 0;
    std::string name;
    std::vector<Candle> history;
};

YahooResult         yahoo_fetch(const std::string& symbol);
std::vector<Candle> yahoo_history(const std::string& symbol);

struct ChatMessage {
    enum Role { USER, ASSISTANT } role;
    std::string text;
    std::string timestamp;
};

enum class AIBackend { NONE, CLAUDE, OLLAMA };

struct ModelOption {
    AIBackend   backend;
    std::string model;
    std::string label;
};

class AIAnalyst {
public:
    AIAnalyst();
    ~AIAnalyst() { if (worker_.joinable()) worker_.join(); }
    void set_api_key(const std::string& k) { api_key_ = k; refresh_available_models(); }
    bool has_key() const { return backend_ != AIBackend::NONE; }
    void load_key_from_env();

    AIBackend   backend() const { return backend_; }
    std::string backend_label() const;

    std::vector<ModelOption> available_models() const { return models_; }
    int  selected_model_index() const { return selected_idx_; }
    void select_model(int idx);
    void refresh_available_models();

    void analyze(const std::vector<Quote>& quotes, const MarketSummary& summary);

    void chat(const std::string& user_prompt,
              const std::vector<Quote>& quotes,
              const MarketSummary& summary);

    bool is_fetching() const { return fetching_.load(); }

    const std::vector<ChatMessage>& history() const { return history_; }
    void clear_history() { std::lock_guard<std::mutex> lk(mu_); history_.clear(); }

    struct QuickResult {
        std::string market_summary, portfolio_advice, top_opportunity, sentiment, timestamp;
        bool ready = false, fetching = false;
    };
    const QuickResult& result() const { return result_; }

private:
    AIBackend                 backend_ = AIBackend::NONE;
    std::string               api_key_;
    std::string               ollama_host_  = "http://localhost:11434";
    std::string               ollama_model_ = "llama3.2";
    std::vector<ModelOption>  models_;
    int                       selected_idx_ = 0;
    QuickResult                result_;
    std::vector<ChatMessage> history_;
    std::atomic<bool>        fetching_{false};
    mutable std::mutex       mu_;
    std::thread              worker_;

    bool        ping_ollama();
    std::vector<std::string> list_ollama_models();
    std::string call_api(AIBackend backend, const std::string& model, const std::string& key,
                         const std::string& system_prompt, const std::string& user_msg);
    std::string call_claude(const std::string& key,
                            const std::string& system_prompt, const std::string& user_msg);
    std::string call_ollama(const std::string& model,
                            const std::string& system_prompt, const std::string& user_msg);
    std::string extract_text(AIBackend backend, const std::string& raw);
    void        parse_quick_response(AIBackend backend, const std::string& raw);
    std::string build_market_context(const std::vector<Quote>& quotes,
                                      const MarketSummary& summary);
    std::string now_ts();
};

class MarketData {
public:
    MarketData();
    ~MarketData() { if (worker_.joinable()) worker_.join(); }

    void start_auto_fetch();
    void refresh_all_live();

    const Quote*              get(const std::string& symbol) const;
    std::vector<Quote>        all() const;
    std::vector<Quote>        by_sector(const std::string& sector) const;
    std::vector<std::string>  sectors() const;

    const std::deque<Candle>& history(const std::string& symbol) const;
    void set_history(const std::string& sym, std::vector<Candle> candles);

    std::vector<Alert*> check_alerts(std::vector<Alert>& alerts);

    bool        is_fetching()  const { return fetching_.load(); }
    bool        is_loaded()    const { return loaded_.load(); }
    std::string fetch_status() const { std::lock_guard<std::mutex> lk(mu_); return fetch_status_; }

private:
    mutable std::mutex           mu_;
    std::map<std::string, Quote> quotes_;
    std::map<std::string, std::deque<Candle>> hist_;
    std::atomic<bool>            fetching_{false};
    std::atomic<bool>            loaded_{false};
    std::string                  fetch_status_;
    std::thread                  worker_;
    static constexpr size_t      MAX_HIST = 80;

    void init_universe();
    void apply_result(const std::string& sym, const YahooResult& r);
};

class NewsFeed {
public:
    NewsFeed();
    const std::deque<NewsItem>& items() const { return items_; }
private:
    std::deque<NewsItem> items_;
    static constexpr size_t MAX = 60;
};

class MacroDashboard {
public:
    MacroDashboard();
    ~MacroDashboard() { if (worker_.joinable()) worker_.join(); }
    void refresh_live();
    std::vector<MacroIndicator> indicators() const;
    bool is_fetching() const { return fetching_.load(); }
private:
    mutable std::mutex          mu_;
    std::vector<MacroIndicator> indicators_;
    std::atomic<bool>           fetching_{false};
    std::thread                 worker_;
};

MarketSummary            compute_market_summary(const std::vector<Quote>& quotes);
std::vector<SectorPerf>  compute_sector_perf(const std::vector<Quote>& quotes);

CorrelationMatrix compute_correlation(
    const std::vector<std::string>& symbols,
    const std::function<const std::deque<Candle>*(const std::string&)>& get_hist,
    int periods = 30);

struct OptionResult {
    double call_price, put_price, call_delta, put_delta, gamma, vega, theta, rho;
};
OptionResult black_scholes(double S, double K, double T, double r, double sigma);
