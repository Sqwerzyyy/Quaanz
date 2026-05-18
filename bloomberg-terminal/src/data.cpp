#include "terminal.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════
//  MarketData
// ═══════════════════════════════════════════════════════════════════

static const std::vector<std::pair<std::string,std::string>> UNIVERSE = {
    {"AAPL","Apple Inc"},   {"MSFT","Microsoft Corp"}, {"GOOGL","Alphabet Inc"},
    {"AMZN","Amazon.com"},  {"NVDA","NVIDIA Corp"},    {"TSLA","Tesla Inc"},
    {"META","Meta Platforms"},{"BRK.B","Berkshire B"},  {"JPM","JPMorgan Chase"},
    {"V",   "Visa Inc"},    {"GS",  "Goldman Sachs"},  {"SPY","S&P 500 ETF"},
    {"QQQ","Nasdaq ETF"},   {"BTC","Bitcoin USD"},     {"ETH","Ethereum USD"},
    {"GLD","Gold ETF"},     {"EUR","EUR/USD"},          {"NFLX","Netflix Inc"},
    {"DIS","Walt Disney"},  {"BABA","Alibaba Group"}
};

static const double INIT_PRICES[] = {
    185.2, 415.1, 175.8, 192.5, 875.3, 175.2,
    520.4,  381.5, 198.4, 278.3, 412.8, 479.2,
    450.1, 68400.0, 3850.0, 185.4, 1.092, 620.1,
    112.3,  71.4
};

MarketData::MarketData() : rng_(std::random_device{}()) {
    init_quotes();
}

void MarketData::init_quotes() {
    for (size_t i = 0; i < UNIVERSE.size(); ++i) {
        Quote q;
        q.symbol     = UNIVERSE[i].first;
        q.name       = UNIVERSE[i].second;
        q.price      = INIT_PRICES[i];
        q.open       = q.price;
        q.high       = q.price;
        q.low        = q.price;
        q.change     = 0.0;
        q.change_pct = 0.0;
        q.volume     = (long long)(1e6 + std::uniform_real_distribution<>(0, 9e6)(rng_));
        quotes_[q.symbol] = q;

        // seed history
        for (int k = 0; k < (int)MAX_HIST; ++k)
            push_candle(q.symbol, q);
    }
}

void MarketData::random_walk(Quote& q) {
    // Volatility varies by asset type
    double vol = 0.002;
    if (q.symbol == "BTC" || q.symbol == "ETH") vol = 0.008;
    else if (q.symbol == "NVDA" || q.symbol == "TSLA") vol = 0.005;
    else if (q.symbol == "EUR") vol = 0.0003;

    std::normal_distribution<double> nd(0.0, vol);
    double pct = nd(rng_);
    double prev = q.price;
    q.price = std::max(0.01, q.price * (1.0 + pct));
    q.high  = std::max(q.high, q.price);
    q.low   = std::min(q.low,  q.price);
    q.change     = q.price - q.open;
    q.change_pct = (q.price - q.open) / q.open * 100.0;
    q.volume    += (long long)(std::uniform_real_distribution<>(0, 50000)(rng_));
    (void)prev;
}

void MarketData::push_candle(const std::string& sym, const Quote& q) {
    auto& d = hist_[sym];
    d.push_back({q.open, q.high, q.low, q.price});
    if (d.size() > MAX_HIST) d.pop_front();
}

void MarketData::tick() {
    for (auto& [sym, q] : quotes_) {
        random_walk(q);
        push_candle(sym, q);
    }
}

const Quote* MarketData::get(const std::string& symbol) const {
    auto it = quotes_.find(symbol);
    return it != quotes_.end() ? &it->second : nullptr;
}

std::vector<Quote> MarketData::all() const {
    std::vector<Quote> out;
    out.reserve(quotes_.size());
    for (auto& [_, q] : quotes_) out.push_back(q);
    return out;
}

const std::deque<Candle>& MarketData::history(const std::string& symbol) const {
    static const std::deque<Candle> empty;
    auto it = hist_.find(symbol);
    return it != hist_.end() ? it->second : empty;
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
    if (cost > cash_) { return; } // insufficient funds (silent for now)
    cash_ -= cost;
    for (auto& p : positions_) {
        if (p.symbol == sym) {
            double total_cost = p.avg_cost * p.qty + cost;
            p.qty += qty;
            p.avg_cost = total_cost / p.qty;
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
    std::getline(f, line); // header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string sym, sq, sc, sp;
        std::getline(ss, sym, ',');
        std::getline(ss, sq, ',');
        std::getline(ss, sc, ',');
        std::getline(ss, sp, ',');
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
//  Black-Scholes Options Pricing
// ═══════════════════════════════════════════════════════════════════

static double norm_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}
static double norm_pdf(double x) {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * M_PI);
}

OptionResult black_scholes(double S, double K, double T, double r, double sigma) {
    OptionResult res{};
    if (T <= 0 || sigma <= 0) return res;
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);
    double Nd1  = norm_cdf(d1),  Nd2 = norm_cdf(d2);
    double Nnd1 = norm_cdf(-d1), Nnd2 = norm_cdf(-d2);
    double nd1  = norm_pdf(d1);

    res.call_price  = S * Nd1  - K * std::exp(-r * T) * Nd2;
    res.put_price   = K * std::exp(-r * T) * Nnd2 - S * Nnd1;
    res.call_delta  = Nd1;
    res.put_delta   = Nd1 - 1.0;
    res.gamma       = nd1 / (S * sigma * std::sqrt(T));
    res.vega        = S * nd1 * std::sqrt(T) / 100.0;
    res.theta       = (-(S * nd1 * sigma) / (2 * std::sqrt(T))
                       - r * K * std::exp(-r * T) * Nd2) / 365.0;
    res.call_iv = res.put_iv = sigma;
    return res;
}

// ═══════════════════════════════════════════════════════════════════
//  News Feed
// ═══════════════════════════════════════════════════════════════════

static const std::vector<std::string> HEADLINES = {
    "Fed signals potential rate cut in Q2 amid cooling inflation",
    "AAPL reports record iPhone sales in Asia markets",
    "NVDA surges on data-center AI chip demand",
    "ECB holds rates steady; euro dips vs dollar",
    "JPMorgan beats Q4 earnings estimates by 12%",
    "Tesla Cybertruck production ramp exceeds guidance",
    "S&P 500 touches all-time high on strong jobs report",
    "Bitcoin consolidates above $68k after ETF inflows",
    "Goldman Sachs upgrades MSFT to Strong Buy",
    "China GDP growth surprises to the upside at 5.3%",
    "Oil prices slide as OPEC output deal faces uncertainty",
    "Meta announces $40B share buyback program",
    "Amazon Web Services revenue grows 17% YoY",
    "US 10-year treasury yield falls 8bps to 4.42%",
    "Alphabet's Gemini Ultra beats GPT-4 on benchmarks",
    "VIX drops to 12.4 — lowest level since January",
    "Berkshire Hathaway discloses new stake in Japanese trading firms",
    "Crypto exchange volume hits monthly high of $3.2T",
    "Disney+ subscriber count surpasses Netflix in Southeast Asia",
    "Gold breaks $2400 as geopolitical tensions rise"
};

static const std::vector<std::string> SOURCES = {
    "Bloomberg","Reuters","WSJ","FT","CNBC","MarketWatch","Barron's"
};

NewsFeed::NewsFeed() : rng_(std::random_device{}()) {
    // seed with a few items
    for (int i = 0; i < 6; ++i) tick();
}

void NewsFeed::tick() {
    ++tick_;
    if (tick_ % 8 != 0) return; // add news every ~8 ticks

    std::uniform_int_distribution<size_t> hd(0, HEADLINES.size()-1);
    std::uniform_int_distribution<size_t> sd(0, SOURCES.size()-1);
    std::uniform_int_distribution<int>    ud(0, 9);

    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    char buf[9]; std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&tt));

    NewsItem item;
    item.headline = HEADLINES[hd(rng_)];
    item.source   = SOURCES[sd(rng_)];
    item.time_str = buf;
    item.urgent   = (ud(rng_) == 0); // 10% chance urgent

    items_.push_front(item);
    if (items_.size() > MAX) items_.pop_back();
}
