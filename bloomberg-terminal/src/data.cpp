#include "terminal.hpp"
#include "json_mini.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>

// ── HTTP via curl CLI (no libcurl headers needed) ─────────────────────────
#include <cstdio>
#include <array>

static std::string http_get(const std::string& url) {
    // Build command: curl with browser UA, follow redirects, silent, 8s timeout
    std::string cmd = "curl -fsSL --max-time 8 "
                      "-A \"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36\" "
                      "\"" + url + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::string result;
    std::array<char,4096> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    return result;
}

// ═══════════════════════════════════════════════════════════════════
//  Yahoo Finance fetcher
// ═══════════════════════════════════════════════════════════════════

YahooResult yahoo_fetch(const std::string& symbol) {
    YahooResult r;
    // v8 chart endpoint — also gives quote summary fields
    std::string url = "https://query1.finance.yahoo.com/v8/finance/chart/"
                    + symbol + "?interval=1d&range=1mo";
    std::string body = http_get(url);
    if (body.empty()) return r;

    try {
        auto j = JsonParser::parse(body);
        auto& result = j["chart"]["result"][0];
        auto& meta   = result["meta"];

        r.price       = meta["regularMarketPrice"].as_num();
        r.prev_close  = meta["chartPreviousClose"].as_num();
        r.open        = meta["regularMarketOpen"].is_null() ? r.prev_close : meta["regularMarketOpen"].as_num();
        r.day_high    = meta["regularMarketDayHigh"].as_num();
        r.day_low     = meta["regularMarketDayLow"].as_num();
        r.week52_high = meta["fiftyTwoWeekHigh"].as_num();
        r.week52_low  = meta["fiftyTwoWeekLow"].as_num();
        r.volume      = (long long)meta["regularMarketVolume"].as_num();
        r.market_cap  = meta["marketCap"].as_num() / 1e9; // → billions
        r.name        = meta["longName"].is_null() ? symbol : meta["longName"].as_str();

        // historical candles from timestamps + OHLCV arrays
        auto& ts      = result["timestamp"];
        auto& quotes  = result["indicators"]["quote"][0];
        auto& opens   = quotes["open"];
        auto& highs   = quotes["high"];
        auto& lows    = quotes["low"];
        auto& closes  = quotes["close"];
        auto& vols    = quotes["volume"];
        size_t n = ts.size();
        r.history.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (closes[i].is_null()) continue;
            Candle c;
            c.open   = opens[i].is_null()  ? closes[i].as_num() : opens[i].as_num();
            c.high   = highs[i].is_null()  ? closes[i].as_num() : highs[i].as_num();
            c.low    = lows[i].is_null()   ? closes[i].as_num() : lows[i].as_num();
            c.close  = closes[i].as_num();
            c.volume = vols[i].is_null()   ? 0 : (long long)vols[i].as_num();
            r.history.push_back(c);
        }
        r.ok = (r.price > 0);
    } catch (...) {}
    return r;
}

std::vector<Candle> yahoo_history(const std::string& symbol) {
    auto r = yahoo_fetch(symbol);
    return r.history;
}

// ═══════════════════════════════════════════════════════════════════
//  Market Data — universe definition
// ═══════════════════════════════════════════════════════════════════

struct AssetDef {
    const char* symbol;
    const char* name;
    const char* sector;
    double      init_price;
    double      vol;         // daily volatility for random walk
};

static const AssetDef UNIVERSE[] = {
    // Tech
    {"AAPL",  "Apple Inc",          "Technology",   185.2,  0.0018},
    {"MSFT",  "Microsoft Corp",     "Technology",   415.1,  0.0016},
    {"GOOGL", "Alphabet Inc",       "Technology",   175.8,  0.0019},
    {"NVDA",  "NVIDIA Corp",        "Technology",   875.3,  0.0045},
    {"META",  "Meta Platforms",     "Technology",   520.4,  0.0025},
    {"AMZN",  "Amazon.com",         "Technology",   192.5,  0.0020},
    {"TSLA",  "Tesla Inc",          "Consumer",     175.2,  0.0050},
    {"NFLX",  "Netflix Inc",        "Consumer",     620.1,  0.0028},
    {"DIS",   "Walt Disney Co",     "Consumer",     112.3,  0.0022},
    // Finance
    {"JPM",   "JPMorgan Chase",     "Finance",      198.4,  0.0015},
    {"GS",    "Goldman Sachs",      "Finance",      412.8,  0.0018},
    {"BRK.B", "Berkshire Hathaway", "Finance",      381.5,  0.0012},
    {"V",     "Visa Inc",           "Finance",      278.3,  0.0014},
    // ETFs
    {"SPY",   "S&P 500 ETF",        "ETF",          479.2,  0.0010},
    {"QQQ",   "Nasdaq 100 ETF",     "ETF",          450.1,  0.0013},
    {"GLD",   "Gold ETF",           "Commodity",    185.4,  0.0009},
    // Crypto
    {"BTC",   "Bitcoin USD",        "Crypto",    68400.0,   0.0080},
    {"ETH",   "Ethereum USD",       "Crypto",     3850.0,   0.0090},
    // FX
    {"EUR",   "EUR/USD",            "FX",            1.092, 0.0003},
    {"BABA",  "Alibaba Group",      "Technology",   71.4,   0.0030},
};
static constexpr size_t N_UNIVERSE = sizeof(UNIVERSE)/sizeof(UNIVERSE[0]);

// ═══════════════════════════════════════════════════════════════════
//  MarketData
// ═══════════════════════════════════════════════════════════════════

MarketData::MarketData() : rng_(std::random_device{}()) {

    init_quotes();
}

void MarketData::init_quotes() {
    for (size_t i = 0; i < N_UNIVERSE; ++i) {
        const auto& d = UNIVERSE[i];
        Quote q;
        q.symbol     = d.symbol;
        q.name       = d.name;
        q.sector     = d.sector;
        q.price      = d.init_price;
        q.open       = d.init_price;
        q.prev_close = d.init_price;
        q.high       = d.init_price;
        q.low        = d.init_price;
        q.volume     = (long long)(1e6 + std::uniform_real_distribution<>(0, 9e6)(rng_));
        q.live       = false;
        quotes_[q.symbol] = q;
        // Seed history
        for (int k = 0; k < (int)MAX_HIST; ++k) push_candle(q.symbol, q);
    }
}

void MarketData::random_walk(Quote& q) {
    // Find volatility for this symbol
    double vol = 0.002;
    for (size_t i = 0; i < N_UNIVERSE; ++i)
        if (UNIVERSE[i].symbol == q.symbol) { vol = UNIVERSE[i].vol; break; }

    std::normal_distribution<double> nd(0.0, vol);
    q.price = std::max(0.01, q.price * (1.0 + nd(rng_)));
    q.high  = std::max(q.high, q.price);
    q.low   = std::min(q.low,  q.price);
    q.change     = q.price - q.prev_close;
    q.change_pct = q.prev_close > 0 ? q.change / q.prev_close * 100.0 : 0.0;
    q.volume    += (long long)std::uniform_real_distribution<>(0, 50000)(rng_);
}

void MarketData::push_candle(const std::string& sym, const Quote& q) {
    auto& d = hist_[sym];
    d.push_back({q.open, q.high, q.low, q.price, q.volume});
    if (d.size() > MAX_HIST) d.pop_front();
}

void MarketData::tick() {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [sym, q] : quotes_) {
        random_walk(q);
        push_candle(sym, q);
    }
}

void MarketData::fetch_live(const std::string& symbol) {
    if (fetching_.load()) return;
    fetching_ = true;
    fetch_status_ = "Fetching " + symbol + "…";
    std::thread([this, symbol]() {
        auto r = yahoo_fetch(symbol);
        if (r.ok) {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = quotes_.find(symbol);
            if (it != quotes_.end()) {
                auto& q       = it->second;
                q.price       = r.price;
                q.open        = r.open;
                q.prev_close  = r.prev_close;
                q.high        = r.day_high;
                q.low         = r.day_low;
                q.week52_high = r.week52_high;
                q.week52_low  = r.week52_low;
                q.volume      = r.volume;
                q.market_cap  = r.market_cap;
                if (!r.name.empty()) q.name = r.name;
                q.change      = q.price - q.prev_close;
                q.change_pct  = q.prev_close > 0 ? q.change / q.prev_close * 100.0 : 0;
                q.live        = true;
            }
            if (!r.history.empty()) {
                auto& d = hist_[symbol];
                d.clear();
                for (auto& c : r.history) d.push_back(c);
            }
            fetch_status_ = "✓ Live: " + symbol;
        } else {
            fetch_status_ = "⚠ Failed: " + symbol + " (using sim)";
        }
        fetching_ = false;
    }).detach();
}

void MarketData::fetch_all_live() {
    if (fetching_.load()) return;
    fetching_ = true;
    fetch_status_ = "Fetching all live data…";
    std::thread([this]() {
        for (size_t i = 0; i < N_UNIVERSE; ++i) {
            std::string sym = UNIVERSE[i].symbol;
            fetch_status_ = "Fetching " + sym + " (" + std::to_string(i+1)
                          + "/" + std::to_string(N_UNIVERSE) + ")";
            auto r = yahoo_fetch(sym);
            if (r.ok) {
                std::lock_guard<std::mutex> lk(mu_);
                auto it = quotes_.find(sym);
                if (it != quotes_.end()) {
                    auto& q       = it->second;
                    q.price       = r.price;
                    q.open        = r.open;
                    q.prev_close  = r.prev_close;
                    q.high        = r.day_high;
                    q.low         = r.day_low;
                    q.week52_high = r.week52_high;
                    q.week52_low  = r.week52_low;
                    q.volume      = r.volume;
                    q.market_cap  = r.market_cap;
                    if (!r.name.empty()) q.name = r.name;
                    q.change      = q.price - q.prev_close;
                    q.change_pct  = q.prev_close > 0 ? q.change / q.prev_close * 100.0 : 0;
                    q.live        = true;
                }
                if (!r.history.empty()) {
                    auto& d = hist_[sym];
                    d.clear();
                    for (auto& c : r.history) d.push_back(c);
                }
            }
        }
        fetch_status_ = "✓ All live data loaded";
        fetching_ = false;
    }).detach();
}

const Quote* MarketData::get(const std::string& symbol) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = quotes_.find(symbol);
    return it != quotes_.end() ? &it->second : nullptr;
}

std::vector<Quote> MarketData::all() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Quote> out;
    out.reserve(quotes_.size());
    for (auto& [_, q] : quotes_) out.push_back(q);
    return out;
}

const std::deque<Candle>& MarketData::history(const std::string& symbol) const {
    static const std::deque<Candle> empty;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hist_.find(symbol);
    return it != hist_.end() ? it->second : empty;
}

void MarketData::set_history(const std::string& sym, std::vector<Candle> candles) {
    std::lock_guard<std::mutex> lk(mu_);
    auto& d = hist_[sym];
    d.clear();
    for (auto& c : candles) d.push_back(c);
}

std::vector<Alert*> MarketData::check_alerts(std::vector<Alert>& alerts) {
    std::vector<Alert*> fired;
    for (auto& a : alerts) {
        if (a.fired) continue;
        const Quote* q = get(a.symbol);
        if (!q) continue;
        bool trigger = a.above ? (q->price >= a.target_price)
                               : (q->price <= a.target_price);
        if (trigger) { a.fired = true; fired.push_back(&a); }
    }
    return fired;
}

// ═══════════════════════════════════════════════════════════════════
//  Portfolio
// ═══════════════════════════════════════════════════════════════════

void Portfolio::buy(const std::string& sym, double qty, double price) {
    double cost = qty * price;
    if (cost > cash_) return;
    cash_ -= cost;
    for (auto& p : positions_) {
        if (p.symbol == sym) {
            double total = p.avg_cost * p.qty + cost;
            p.qty += qty;
            p.avg_cost = total / p.qty;
            return;
        }
    }
    positions_.push_back({sym, qty, price, price});
}

void Portfolio::sell(const std::string& sym, double qty, double price) {
    for (auto it = positions_.begin(); it != positions_.end(); ++it) {
        if (it->symbol == sym) {
            double sold = std::min(qty, it->qty);
            cash_ += sold * price;
            it->qty -= sold;
            if (it->qty <= 0.0001) positions_.erase(it);
            return;
        }
    }
}

void Portfolio::update_prices(const MarketData& md) {
    for (auto& p : positions_) {
        const Quote* q = md.get(p.symbol);
        if (q) p.current_price = q->price;
    }
}

void Portfolio::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string sym, sq, sc, sp;
        std::getline(ss, sym, ',');
        std::getline(ss, sq,  ',');
        std::getline(ss, sc,  ',');
        std::getline(ss, sp,  ',');
        if (sym == "CASH") { cash_ = std::stod(sq); continue; }
        positions_.push_back({sym, std::stod(sq), std::stod(sc), std::stod(sp)});
    }
}

void Portfolio::save(const std::string& path) const {
    std::ofstream f(path);
    f << "symbol,qty,avg_cost,last_price\n";
    f << "CASH," << cash_ << ",0,0\n";
    for (auto& p : positions_)
        f << p.symbol << "," << p.qty << "," << p.avg_cost << "," << p.current_price << "\n";
}

double Portfolio::total_value()   const {
    double v = cash_;
    for (auto& p : positions_) v += p.value();
    return v;
}
double Portfolio::total_cost()    const {
    double c = 0;
    for (auto& p : positions_) c += p.avg_cost * p.qty;
    return c;
}
double Portfolio::total_pnl()     const { return total_value() - cash_ - total_cost(); }
double Portfolio::total_pnl_pct() const {
    double cost = total_cost();
    return cost > 0 ? total_pnl() / cost * 100.0 : 0.0;
}

// ═══════════════════════════════════════════════════════════════════
//  Black-Scholes
// ═══════════════════════════════════════════════════════════════════

static double norm_cdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }
static double norm_pdf(double x) { return std::exp(-0.5*x*x) / std::sqrt(2.0*M_PI); }

OptionResult black_scholes(double S, double K, double T, double r, double sigma) {
    OptionResult res{};
    if (T <= 0 || sigma <= 0) return res;
    double d1 = (std::log(S/K) + (r + 0.5*sigma*sigma)*T) / (sigma*std::sqrt(T));
    double d2 = d1 - sigma*std::sqrt(T);
    double Nd1 = norm_cdf(d1), Nd2 = norm_cdf(d2);
    double nd1 = norm_pdf(d1);
    res.call_price = S*Nd1 - K*std::exp(-r*T)*Nd2;
    res.put_price  = K*std::exp(-r*T)*norm_cdf(-d2) - S*norm_cdf(-d1);
    res.call_delta = Nd1;
    res.put_delta  = Nd1 - 1.0;
    res.gamma      = nd1 / (S*sigma*std::sqrt(T));
    res.vega       = S*nd1*std::sqrt(T) / 100.0;
    res.theta      = (-(S*nd1*sigma)/(2*std::sqrt(T)) - r*K*std::exp(-r*T)*Nd2) / 365.0;
    res.rho        = K*T*std::exp(-r*T)*Nd2 / 100.0;
    return res;
}

// ═══════════════════════════════════════════════════════════════════
//  News Feed
// ═══════════════════════════════════════════════════════════════════

static const char* HEADLINES[] = {
    "Fed signals potential rate cut in Q2 amid cooling inflation",
    "AAPL reports record iPhone sales across Asian markets",
    "NVDA surges on massive data-center AI chip demand",
    "ECB holds rates steady; EUR dips vs USD",
    "JPMorgan beats Q4 earnings estimates by 12%",
    "Tesla Cybertruck production ramp exceeds guidance",
    "S&P 500 touches all-time high on strong jobs report",
    "Bitcoin consolidates above $68k after ETF inflows",
    "Goldman Sachs upgrades MSFT to Strong Buy, PT $500",
    "China GDP growth surprises to the upside at 5.3%",
    "Oil prices slide as OPEC output deal faces uncertainty",
    "Meta announces $40B share buyback program",
    "Amazon Web Services revenue grows 17% YoY",
    "US 10-year treasury yield falls 8bps to 4.42%",
    "Alphabet Gemini Ultra beats GPT-4 on key benchmarks",
    "VIX drops to 12.4 — lowest level since January",
    "Berkshire discloses new stake in Japanese trading firms",
    "Crypto exchange volume hits monthly high of $3.2T",
    "Disney+ subscriber count surpasses Netflix in SE Asia",
    "Gold breaks $2400 as geopolitical tensions rise",
    "NVIDIA announces new Blackwell GPU architecture",
    "Apple Vision Pro sales exceed analyst expectations",
    "Microsoft Azure growth accelerates to 31% YoY",
    "Retail sales data beats consensus — consumer strong",
    "CPI comes in at 2.8% — below 3.0% forecast",
};
static const char* SOURCES[] = {
    "Bloomberg","Reuters","WSJ","FT","CNBC","MarketWatch","Barron's","Seeking Alpha"
};

NewsFeed::NewsFeed() : rng_(std::random_device{}()) {

    // just call tick 8 times to seed
    for (int i = 0; i < 8; ++i) { tick_++; tick(); tick_--; }
}

void NewsFeed::tick() {
    ++tick_;
    if (tick_ % 8 != 0) return;

    size_t nh = sizeof(HEADLINES)/sizeof(HEADLINES[0]);
    size_t ns = sizeof(SOURCES)/sizeof(SOURCES[0]);
    std::uniform_int_distribution<size_t> hd(0, nh-1), sd(0, ns-1);
    std::uniform_int_distribution<int>    ud(0, 9);

    std::time_t t = std::time(nullptr);
    char buf[9]; std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));

    NewsItem item;
    item.headline = HEADLINES[hd(rng_)];
    item.source   = SOURCES[sd(rng_)];
    item.time_str = buf;
    item.urgent   = (ud(rng_) == 0);

    items_.push_front(item);
    if (items_.size() > MAX) items_.pop_back();
}

// ═══════════════════════════════════════════════════════════════════
//  Macro Dashboard
// ═══════════════════════════════════════════════════════════════════

MacroDashboard::MacroDashboard() : rng_(std::random_device{}()) {
    indicators_ = {
        {"S&P 500",      "^GSPC",  4780.2,  0, 0, "pts",  "US large-cap equity index"},
        {"NASDAQ",       "^IXIC", 15200.0,  0, 0, "pts",  "US tech-heavy equity index"},
        {"VIX",          "^VIX",     14.2,  0, 0, "pts",  "Volatility fear index"},
        {"US 10Y Yield", "^TNX",      4.42, 0, 0, "%",    "Treasury benchmark rate"},
        {"US 2Y Yield",  "^IRX",      4.88, 0, 0, "%",    "Short-term rate"},
        {"DXY",          "DX-Y.NYB", 104.5, 0, 0, "pts",  "US Dollar index"},
        {"Gold",         "GC=F",    2380.0, 0, 0, "$/oz", "Spot gold futures"},
        {"WTI Oil",      "CL=F",      82.4, 0, 0, "$/bbl","Crude oil futures"},
        {"EUR/USD",      "EURUSD=X",  1.092,0, 0, "rate", "Euro vs US Dollar"},
        {"USD/JPY",      "JPY=X",   151.3,  0, 0, "rate", "Dollar vs Japanese Yen"},
        {"Bitcoin",      "BTC-USD", 68400.0,0, 0, "USD",  "Bitcoin spot price"},
        {"Ethereum",     "ETH-USD",  3850.0,0, 0, "USD",  "Ethereum spot price"},
    };
}

void MacroDashboard::tick() {
    // Simulate small random walk on indicators
    std::normal_distribution<double> nd(0, 0.001);
    for (auto& ind : indicators_) {
        double prev = ind.value;
        ind.value = std::max(0.001, ind.value * (1.0 + nd(rng_)));
        ind.change     = ind.value - prev;
        ind.change_pct = prev > 0 ? ind.change / prev * 100.0 : 0;
    }
}

void MacroDashboard::refresh_live() {
    if (fetching_.load()) return;
    fetching_ = true;
    std::thread([this]() {
        for (auto& ind : indicators_) {
            auto r = yahoo_fetch(ind.symbol);
            if (r.ok && r.price > 0) {
                ind.value      = r.price;
                ind.change     = r.price - r.prev_close;
                ind.change_pct = r.prev_close > 0 ? ind.change / r.prev_close * 100.0 : 0;
            }
        }
        fetching_ = false;
    }).detach();
}

// ═══════════════════════════════════════════════════════════════════
//  Analytics Engine
// ═══════════════════════════════════════════════════════════════════

CorrelationMatrix compute_correlation(
    const std::vector<std::string>& symbols,
    const std::function<const std::deque<Candle>*(const std::string&)>& get_hist,
    int periods)
{
    CorrelationMatrix cm;
    cm.symbols = symbols;
    int N = (int)symbols.size();
    cm.matrix.assign(N, std::vector<double>(N, 0.0));

    // Build returns matrix
    std::vector<std::vector<double>> returns(N);
    for (int i = 0; i < N; ++i) {
        const auto* h = get_hist(symbols[i]);
        if (!h || h->size() < 2) continue;
        int n = std::min(periods + 1, (int)h->size());
        int start = (int)h->size() - n;
        for (int j = start + 1; j < (int)h->size(); ++j) {
            double prev = (*h)[j-1].close;
            double cur  = (*h)[j].close;
            if (prev > 0) returns[i].push_back((cur - prev) / prev);
        }
    }

    // Pearson correlation
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j) { cm.matrix[i][j] = 1.0; continue; }
            const auto& a = returns[i];
            const auto& b = returns[j];
            int n = (int)std::min(a.size(), b.size());
            if (n < 3) { cm.matrix[i][j] = 0.0; continue; }

            double sum_a=0, sum_b=0, sum_ab=0, sum_a2=0, sum_b2=0;
            for (int k = 0; k < n; ++k) {
                sum_a  += a[k]; sum_b  += b[k];
                sum_ab += a[k]*b[k];
                sum_a2 += a[k]*a[k]; sum_b2 += b[k]*b[k];
            }
            double mean_a = sum_a/n, mean_b = sum_b/n;
            double cov=0, var_a=0, var_b=0;
            for (int k = 0; k < n; ++k) {
                cov   += (a[k]-mean_a)*(b[k]-mean_b);
                var_a += (a[k]-mean_a)*(a[k]-mean_a);
                var_b += (b[k]-mean_b)*(b[k]-mean_b);
            }
            double denom = std::sqrt(var_a * var_b);
            cm.matrix[i][j] = denom > 1e-10 ? cov / denom : 0.0;
        }
    }
    return cm;
}

MarketSummary compute_market_summary(const std::vector<Quote>& quotes) {
    MarketSummary ms;
    if (quotes.empty()) return ms;

    std::vector<GainerLoser> all;
    double sum_chg = 0;
    int adv=0, dec=0, unch=0;

    for (const auto& q : quotes) {
        GainerLoser g{q.symbol, q.name, q.price, q.change, q.change_pct, (double)q.volume};
        all.push_back(g);
        sum_chg += q.change_pct;
        if (q.change_pct >  0.05) ++adv;
        else if (q.change_pct < -0.05) ++dec;
        else ++unch;
    }
    ms.avg_change_pct = sum_chg / quotes.size();
    ms.advances  = (double)adv  / quotes.size() * 100.0;
    ms.declines  = (double)dec  / quotes.size() * 100.0;
    ms.unchanged = (double)unch / quotes.size() * 100.0;

    // Gainers
    auto sorted = all;
    std::sort(sorted.begin(), sorted.end(),
        [](const GainerLoser& a, const GainerLoser& b){ return a.change_pct > b.change_pct; });
    for (int i = 0; i < std::min(5,(int)sorted.size()); ++i)
        ms.top_gainers.push_back(sorted[i]);

    // Losers
    std::sort(sorted.begin(), sorted.end(),
        [](const GainerLoser& a, const GainerLoser& b){ return a.change_pct < b.change_pct; });
    for (int i = 0; i < std::min(5,(int)sorted.size()); ++i)
        ms.top_losers.push_back(sorted[i]);

    // Volume
    std::sort(sorted.begin(), sorted.end(),
        [](const GainerLoser& a, const GainerLoser& b){ return a.volume > b.volume; });
    for (int i = 0; i < std::min(5,(int)sorted.size()); ++i)
        ms.top_volume.push_back(sorted[i]);

    return ms;
}

std::vector<SectorPerf> compute_sector_perf(const std::vector<Quote>& quotes) {
    std::map<std::string, std::vector<double>> by_sector;
    for (const auto& q : quotes) {
        std::string s = q.sector.empty() ? "Other" : q.sector;
        by_sector[s].push_back(q.change_pct);
    }
    std::vector<SectorPerf> out;
    for (auto& [sector, vals] : by_sector) {
        SectorPerf sp;
        sp.sector = sector;
        sp.count  = (int)vals.size();
        sp.advances = 0;
        double sum = 0;
        for (double v : vals) { sum += v; if (v > 0) ++sp.advances; }
        sp.avg_change_pct = sum / vals.size();
        out.push_back(sp);
    }
    std::sort(out.begin(), out.end(),
        [](const SectorPerf& a, const SectorPerf& b){
            return a.avg_change_pct > b.avg_change_pct; });
    return out;
}
