#include "terminal.hpp"
#include "json_mini.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <array>
#include <unistd.h>

static std::string http_get(const std::string& url) {
    std::string cmd = "curl -fsSL --max-time 8 "
        "-A \"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36\" "
        "\"" + url + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::string r; std::array<char,4096> buf;
    while (fgets(buf.data(), buf.size(), pipe)) r += buf.data();
    pclose(pipe); return r;
}

YahooResult yahoo_fetch(const std::string& symbol) {
    YahooResult r;
    std::string url = "https://query1.finance.yahoo.com/v8/finance/chart/"
                    + symbol + "?interval=1d&range=1mo";
    std::string body = http_get(url);
    if (body.empty()) return r;
    try {
        auto j = JsonParser::parse(body);
        auto& meta = j["chart"]["result"][0]["meta"];
        r.price        = meta["regularMarketPrice"].as_num();
        r.prev_close   = meta["chartPreviousClose"].as_num();
        r.open         = meta["regularMarketOpen"].is_null() ? r.prev_close : meta["regularMarketOpen"].as_num();
        r.day_high     = meta["regularMarketDayHigh"].as_num();
        r.day_low      = meta["regularMarketDayLow"].as_num();
        r.week52_high  = meta["fiftyTwoWeekHigh"].as_num();
        r.week52_low   = meta["fiftyTwoWeekLow"].as_num();
        r.volume       = (long long)meta["regularMarketVolume"].as_num();
        r.market_cap   = meta["marketCap"].as_num() / 1e9;
        r.name         = meta["longName"].is_null() ? symbol : meta["longName"].as_str();
        auto& result   = j["chart"]["result"][0];
        auto& ts       = result["timestamp"];
        auto& q        = result["indicators"]["quote"][0];
        size_t n       = ts.size();
        r.history.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (q["close"][i].is_null()) continue;
            Candle c;
            c.open   = q["open"][i].is_null()   ? q["close"][i].as_num() : q["open"][i].as_num();
            c.high   = q["high"][i].is_null()   ? q["close"][i].as_num() : q["high"][i].as_num();
            c.low    = q["low"][i].is_null()    ? q["close"][i].as_num() : q["low"][i].as_num();
            c.close  = q["close"][i].as_num();
            c.volume = q["volume"][i].is_null() ? 0 : (long long)q["volume"][i].as_num();
            r.history.push_back(c);
        }
        r.ok = (r.price > 0);
    } catch (...) {}
    return r;
}

std::vector<Candle> yahoo_history(const std::string& s) { return yahoo_fetch(s).history; }

struct AssetDef {
    const char* symbol; const char* name; const char* sector;
};

static const AssetDef UNIVERSE[] = {

    {"AAPL",  "Apple Inc",           "Technology"},
    {"MSFT",  "Microsoft Corp",      "Technology"},
    {"GOOGL", "Alphabet Inc",        "Technology"},
    {"NVDA",  "NVIDIA Corp",         "Technology"},
    {"META",  "Meta Platforms",      "Technology"},
    {"AMZN",  "Amazon.com",          "Technology"},
    {"TSLA",  "Tesla Inc",           "Technology"},
    {"NFLX",  "Netflix Inc",         "Technology"},
    {"AMD",   "Advanced Micro Dev",  "Technology"},
    {"INTC",  "Intel Corp",          "Technology"},
    {"CRM",   "Salesforce Inc",      "Technology"},
    {"ORCL",  "Oracle Corp",         "Technology"},
    {"ADBE",  "Adobe Inc",           "Technology"},
    {"QCOM",  "Qualcomm Inc",        "Technology"},
    {"NOW",   "ServiceNow Inc",      "Technology"},

    {"JPM",   "JPMorgan Chase",      "Finance"},
    {"GS",    "Goldman Sachs",       "Finance"},
    {"MS",    "Morgan Stanley",      "Finance"},
    {"BAC",   "Bank of America",     "Finance"},
    {"BRK.B", "Berkshire Hathaway",  "Finance"},
    {"V",     "Visa Inc",            "Finance"},
    {"MA",    "Mastercard Inc",      "Finance"},
    {"BLK",   "BlackRock Inc",       "Finance"},
    {"AXP",   "American Express",    "Finance"},
    {"C",     "Citigroup Inc",       "Finance"},

    {"BTC",   "Bitcoin USD",         "Crypto"},
    {"ETH",   "Ethereum USD",        "Crypto"},
    {"SOL",   "Solana USD",          "Crypto"},
    {"BNB",   "Binance Coin",        "Crypto"},
    {"XRP",   "Ripple USD",          "Crypto"},
    {"ADA",   "Cardano USD",         "Crypto"},
    {"AVAX",  "Avalanche USD",       "Crypto"},
    {"LINK",  "Chainlink USD",       "Crypto"},
    {"DOT",   "Polkadot USD",        "Crypto"},
    {"MATIC", "Polygon USD",         "Crypto"},

    {"SPY",   "S&P 500 ETF",         "Commodities"},
    {"QQQ",   "Nasdaq 100 ETF",      "Commodities"},
    {"GLD",   "Gold ETF",            "Commodities"},
    {"SLV",   "Silver ETF",          "Commodities"},
    {"USO",   "Oil ETF",             "Commodities"},
    {"UNG",   "Natural Gas ETF",     "Commodities"},
    {"DBA",   "Agri Commodities",    "Commodities"},
    {"PDBC",  "Diversified Cmdty",   "Commodities"},
    {"CPER",  "Copper ETF",          "Commodities"},
    {"WEAT",  "Wheat ETF",           "Commodities"},

    {"VNQ",   "Vanguard REIT ETF",   "RWA"},
    {"STWD",  "Starwood Prop",       "RWA"},
    {"AMT",   "American Tower",      "RWA"},
    {"PLD",   "Prologis Inc",        "RWA"},
    {"EQIX",  "Equinix Inc",         "RWA"},
    {"DLR",   "Digital Realty",      "RWA"},
    {"O",     "Realty Income Corp",  "RWA"},
    {"IRM",   "Iron Mountain",       "RWA"},
    {"MPW",   "Medical Properties",  "RWA"},
    {"WPC",   "W.P. Carey Inc",      "RWA"},
};
static constexpr size_t N_UNIVERSE = sizeof(UNIVERSE)/sizeof(UNIVERSE[0]);

MarketData::MarketData() { init_universe(); }

void MarketData::init_universe() {
    for (size_t i = 0; i < N_UNIVERSE; ++i) {
        const auto& d = UNIVERSE[i];
        Quote q;
        q.symbol = d.symbol; q.name = d.name; q.sector = d.sector;
        quotes_[q.symbol] = q;
        hist_[q.symbol] = {};
    }
}

void MarketData::apply_result(const std::string& sym, const YahooResult& r) {
    auto it = quotes_.find(sym);
    if (it != quotes_.end()) {
        auto& q = it->second;
        q.price = r.price; q.open = r.open; q.prev_close = r.prev_close;
        q.high = r.day_high; q.low = r.day_low;
        q.week52_high = r.week52_high; q.week52_low = r.week52_low;
        q.volume = r.volume; q.market_cap = r.market_cap;
        if (!r.name.empty()) q.name = r.name;
        q.change = q.price - q.prev_close;
        q.change_pct = q.prev_close > 0 ? q.change / q.prev_close * 100.0 : 0;
        q.live = true;
    }
    if (!r.history.empty()) {
        auto& d = hist_[sym]; d.clear();
        for (auto& c : r.history) d.push_back(c);
    }
}

void MarketData::refresh_all_live() {
    if (fetching_.load()) return;
    fetching_ = true;
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this]() {
        for (size_t i = 0; i < N_UNIVERSE; ++i) {
            std::string sym = UNIVERSE[i].symbol;
            {
                std::lock_guard<std::mutex> lk(mu_);
                fetch_status_ = "Fetching " + sym + " (" + std::to_string(i+1)
                              + "/" + std::to_string(N_UNIVERSE) + ")";
            }
            std::string sector = UNIVERSE[i].sector;
            std::string fetch_sym = (sector == "Crypto") ? sym + "-USD" : sym;
            auto r = yahoo_fetch(fetch_sym);
            if (r.ok) {
                std::lock_guard<std::mutex> lk(mu_);
                apply_result(sym, r);
                loaded_ = true;
            }
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            fetch_status_ = "Live market data loaded";
        }
        loaded_ = true;
        fetching_ = false;
    });
}

void MarketData::start_auto_fetch() { refresh_all_live(); }

const Quote* MarketData::get(const std::string& s) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = quotes_.find(s); return it != quotes_.end() ? &it->second : nullptr;
}

std::vector<Quote> MarketData::all() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Quote> out; out.reserve(quotes_.size());
    for (auto& [_, q] : quotes_) out.push_back(q);
    return out;
}

std::vector<Quote> MarketData::by_sector(const std::string& sector) const {
    auto all_q = all();
    if (sector.empty() || sector == "All") return all_q;
    std::vector<Quote> out;
    for (auto& q : all_q) if (q.sector == sector) out.push_back(q);
    return out;
}

std::vector<std::string> MarketData::sectors() const {
    return {"All", "Technology", "Finance", "Crypto", "Commodities", "RWA"};
}

const std::deque<Candle>& MarketData::history(const std::string& symbol) const {
    static const std::deque<Candle> empty;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hist_.find(symbol); return it != hist_.end() ? it->second : empty;
}

void MarketData::set_history(const std::string& sym, std::vector<Candle> candles) {
    std::lock_guard<std::mutex> lk(mu_);
    auto& d = hist_[sym]; d.clear();
    for (auto& c : candles) d.push_back(c);
}

std::vector<Alert*> MarketData::check_alerts(std::vector<Alert>& alerts) {
    std::vector<Alert*> fired;
    for (auto& a : alerts) {
        if (a.fired) continue;
        const Quote* q = get(a.symbol); if (!q) continue;
        bool trigger = a.above ? (q->price >= a.target_price) : (q->price <= a.target_price);
        if (trigger) { a.fired = true; fired.push_back(&a); }
    }
    return fired;
}

static double norm_cdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }
static double norm_pdf(double x) { return std::exp(-0.5*x*x) / std::sqrt(2.0*M_PI); }

OptionResult black_scholes(double S, double K, double T, double r, double sigma) {
    OptionResult res{};
    if (T <= 0 || sigma <= 0) return res;
    double d1 = (std::log(S/K) + (r + 0.5*sigma*sigma)*T) / (sigma*std::sqrt(T));
    double d2 = d1 - sigma*std::sqrt(T);
    double nd1 = norm_pdf(d1);
    res.call_price = S*norm_cdf(d1) - K*std::exp(-r*T)*norm_cdf(d2);
    res.put_price  = K*std::exp(-r*T)*norm_cdf(-d2) - S*norm_cdf(-d1);
    res.call_delta = norm_cdf(d1); res.put_delta = norm_cdf(d1) - 1.0;
    res.gamma = nd1 / (S*sigma*std::sqrt(T));
    res.vega  = S*nd1*std::sqrt(T) / 100.0;
    res.theta = (-(S*nd1*sigma)/(2*std::sqrt(T)) - r*K*std::exp(-r*T)*norm_cdf(d2)) / 365.0;
    res.rho   = K*T*std::exp(-r*T)*norm_cdf(d2) / 100.0;
    return res;
}

static const char* HEADLINES[] = {
    "Fed signals potential rate cut in Q2 amid cooling inflation",
    "NVDA surges on massive data-center AI chip demand",
    "Bitcoin consolidates above $68k after record ETF inflows",
    "Goldman Sachs upgrades MSFT to Strong Buy, PT $500",
    "RWA tokenization market surpasses $10B on-chain",
    "Solana ecosystem TVL hits all-time high of $8.2B",
    "JPMorgan beats Q4 earnings estimates by 12 percent",
    "ECB holds rates steady; EUR dips vs USD",
    "S&P 500 touches all-time high on strong jobs report",
    "Amazon Web Services revenue grows 17 percent YoY",
    "Meta announces $40B share buyback program",
    "BlackRock files for spot Ethereum ETF",
    "Chainlink expands CCIP to 10 new blockchains",
    "US 10-year treasury yield falls 8bps to 4.42 percent",
    "Gold breaks $2400 as geopolitical tensions rise",
    "Prologis reports record industrial REIT occupancy at 97 percent",
    "AAPL reports record iPhone sales in Asian markets",
    "Avalanche DeFi volume surpasses Ethereum L2s combined",
    "Oil prices slide as OPEC output deal faces uncertainty",
    "Realty Income raises dividend for 25th consecutive quarter",
    "Polygon zkEVM processes 1M daily transactions milestone",
    "VIX drops to 12.4 lowest level since January",
    "CPI comes in at 2.8 percent below 3.0 percent forecast",
    "Digital Realty secures $5B hyperscaler data center deal",
    "Crypto exchange volume hits monthly high of $3.2 trillion",
};
static const char* SOURCES[] = {
    "Bloomberg","Reuters","WSJ","FT","CNBC","CoinDesk","Barron's","The Block","DeFi Pulse"
};

NewsFeed::NewsFeed() {
    size_t nh = sizeof(HEADLINES)/sizeof(HEADLINES[0]);
    size_t ns = sizeof(SOURCES)/sizeof(SOURCES[0]);
    std::time_t t = std::time(nullptr);
    char buf[9]; std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    for (size_t i = 0; i < nh; ++i) {
        NewsItem item;
        item.headline = HEADLINES[i];
        item.source   = SOURCES[i % ns];
        item.time_str = buf;
        item.urgent   = (i % 7 == 0);
        items_.push_back(item);
        if (items_.size() > MAX) items_.pop_back();
    }
}

MacroDashboard::MacroDashboard() {
    indicators_.push_back({"S&P 500","^GSPC","pts","US large-cap index",4780.2,0,0});
    indicators_.push_back({"NASDAQ","^IXIC","pts","US tech index",15200.0,0,0});
    indicators_.push_back({"VIX","^VIX","pts","Fear index",14.2,0,0});
    indicators_.push_back({"US 10Y","^TNX","%","Treasury benchmark",4.42,0,0});
    indicators_.push_back({"US 2Y","^IRX","%","Short-term rate",4.88,0,0});
    indicators_.push_back({"DXY","DX-Y.NYB","pts","Dollar index",104.5,0,0});
    indicators_.push_back({"Gold","GC=F","$/oz","Spot gold",2380.0,0,0});
    indicators_.push_back({"WTI","CL=F","$/bbl","Crude oil",82.4,0,0});
    indicators_.push_back({"EUR/USD","EURUSD=X","rate","Euro vs Dollar",1.092,0,0});
    indicators_.push_back({"BTC","BTC-USD","USD","Bitcoin",68400.0,0,0});
    indicators_.push_back({"ETH","ETH-USD","USD","Ethereum",3850.0,0,0});
    indicators_.push_back({"SOL","SOL-USD","USD","Solana",185.4,0,0});
}

void MacroDashboard::refresh_live() {
    if (fetching_.load()) return;
    fetching_ = true;
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this]() {
        std::vector<MacroIndicator> snapshot;
        {
            std::lock_guard<std::mutex> lk(mu_);
            snapshot = indicators_;
        }
        for (auto& ind : snapshot) {
            auto r = yahoo_fetch(ind.symbol);
            if (r.ok && r.price > 0) {
                ind.value = r.price;
                ind.change = r.price - r.prev_close;
                ind.change_pct = r.prev_close > 0 ? ind.change / r.prev_close * 100.0 : 0;
            }
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            indicators_ = snapshot;
        }
        fetching_ = false;
    });
}

std::vector<MacroIndicator> MacroDashboard::indicators() const {
    std::lock_guard<std::mutex> lk(mu_);
    return indicators_;
}

MarketSummary compute_market_summary(const std::vector<Quote>& quotes) {
    MarketSummary ms;
    if (quotes.empty()) return ms;
    std::vector<GainerLoser> all;
    double sum = 0; int adv=0, dec=0, unch=0;
    for (const auto& q : quotes) {
        all.push_back({q.symbol,q.name,q.price,q.change,q.change_pct,(double)q.volume});
        sum += q.change_pct;
        if (q.change_pct > 0.05) ++adv;
        else if (q.change_pct < -0.05) ++dec;
        else ++unch;
    }
    ms.avg_change_pct = sum / quotes.size();
    ms.advances  = (double)adv  / quotes.size() * 100.0;
    ms.declines  = (double)dec  / quotes.size() * 100.0;
    ms.unchanged = (double)unch / quotes.size() * 100.0;
    auto sorted = all;
    std::sort(sorted.begin(),sorted.end(),[](const GainerLoser& a,const GainerLoser& b){return a.change_pct>b.change_pct;});
    for (int i=0;i<std::min(5,(int)sorted.size());++i) ms.top_gainers.push_back(sorted[i]);
    std::sort(sorted.begin(),sorted.end(),[](const GainerLoser& a,const GainerLoser& b){return a.change_pct<b.change_pct;});
    for (int i=0;i<std::min(5,(int)sorted.size());++i) ms.top_losers.push_back(sorted[i]);
    std::sort(sorted.begin(),sorted.end(),[](const GainerLoser& a,const GainerLoser& b){return a.volume>b.volume;});
    for (int i=0;i<std::min(5,(int)sorted.size());++i) ms.top_volume.push_back(sorted[i]);
    return ms;
}

std::vector<SectorPerf> compute_sector_perf(const std::vector<Quote>& quotes) {
    std::map<std::string,std::vector<double>> by_sector;
    std::map<std::string,int> adv_map;
    for (const auto& q : quotes) {
        std::string s = q.sector.empty() ? "Other" : q.sector;
        by_sector[s].push_back(q.change_pct);
        if (q.change_pct > 0) adv_map[s]++;
    }
    std::vector<SectorPerf> out;
    for (auto& [sector, vals] : by_sector) {
        SectorPerf sp; sp.sector = sector; sp.count = (int)vals.size();
        sp.advances = adv_map[sector];
        double sum = 0; for (double v : vals) sum += v;
        sp.avg_change_pct = sum / vals.size();
        out.push_back(sp);
    }
    std::sort(out.begin(),out.end(),[](const SectorPerf& a,const SectorPerf& b){return a.avg_change_pct>b.avg_change_pct;});
    return out;
}

CorrelationMatrix compute_correlation(
    const std::vector<std::string>& symbols,
    const std::function<const std::deque<Candle>*(const std::string&)>& get_hist,
    int periods)
{
    CorrelationMatrix cm; cm.symbols = symbols;
    int N = (int)symbols.size();
    cm.matrix.assign(N, std::vector<double>(N, 0.0));
    std::vector<std::vector<double>> returns(N);
    for (int i = 0; i < N; ++i) {
        const auto* h = get_hist(symbols[i]); if (!h || h->size() < 2) continue;
        int n = std::min(periods+1,(int)h->size()), start = (int)h->size()-n;
        for (int j = start+1; j < (int)h->size(); ++j)
            if ((*h)[j-1].close > 0) returns[i].push_back(((*h)[j].close-(*h)[j-1].close)/(*h)[j-1].close);
    }
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
        if (i == j) { cm.matrix[i][j] = 1.0; continue; }
        const auto& a=returns[i]; const auto& b=returns[j];
        int n=(int)std::min(a.size(),b.size()); if (n<3){cm.matrix[i][j]=0.0;continue;}
        double ma=0,mb=0; for(int k=0;k<n;++k){ma+=a[k];mb+=b[k];} ma/=n;mb/=n;
        double cov=0,va=0,vb=0;
        for(int k=0;k<n;++k){cov+=(a[k]-ma)*(b[k]-mb);va+=(a[k]-ma)*(a[k]-ma);vb+=(b[k]-mb)*(b[k]-mb);}
        double d=std::sqrt(va*vb); cm.matrix[i][j]=d>1e-10?cov/d:0.0;
    }
    return cm;
}

AIAnalyst::AIAnalyst() { load_key_from_env(); refresh_available_models(); }

void AIAnalyst::load_key_from_env() {
    const char* k = std::getenv("ANTHROPIC_API_KEY"); if (k) api_key_ = k;
    const char* h = std::getenv("OLLAMA_HOST");        if (h && *h) ollama_host_  = h;
    const char* m = std::getenv("OLLAMA_MODEL");       if (m && *m) ollama_model_ = m;
}

bool AIAnalyst::ping_ollama() {
    std::string raw = http_get(ollama_host_ + "/api/tags");
    return !raw.empty() && raw.find("models") != std::string::npos;
}

std::vector<std::string> AIAnalyst::list_ollama_models() {
    std::vector<std::string> out;
    std::string raw = http_get(ollama_host_ + "/api/tags");
    if (raw.empty()) return out;
    auto j = JsonParser::parse(raw);
    const auto& arr = j["models"];
    for (size_t i = 0; i < arr.size(); ++i) {
        std::string name = arr[i]["name"].as_str();
        if (!name.empty()) out.push_back(name);
    }
    return out;
}

void AIAnalyst::refresh_available_models() {
    std::vector<ModelOption> opts;
    if (!api_key_.empty())
        opts.push_back({AIBackend::CLAUDE, "claude-haiku-4-5-20251001", "Claude (paid)"});

    for (auto& name : list_ollama_models())
        opts.push_back({AIBackend::OLLAMA, name, "Ollama: " + name + " (free)"});

    std::lock_guard<std::mutex> lk(mu_);
    models_ = std::move(opts);
    if (models_.empty()) {
        backend_ = AIBackend::NONE;
        selected_idx_ = 0;
        return;
    }
    selected_idx_ = 0;
    backend_      = models_[0].backend;
    ollama_model_ = models_[0].model;
}

void AIAnalyst::select_model(int idx) {
    std::lock_guard<std::mutex> lk(mu_);
    if (idx < 0 || idx >= (int)models_.size()) return;
    selected_idx_ = idx;
    backend_      = models_[idx].backend;
    if (models_[idx].backend == AIBackend::OLLAMA) ollama_model_ = models_[idx].model;
}

std::string AIAnalyst::backend_label() const {
    std::lock_guard<std::mutex> lk(mu_);
    if (selected_idx_ >= 0 && selected_idx_ < (int)models_.size())
        return models_[selected_idx_].label;
    return "none";
}

std::string AIAnalyst::now_ts() {
    std::time_t t=std::time(nullptr);
    char buf[16]; std::strftime(buf,sizeof(buf),"%H:%M:%S",std::localtime(&t));
    return buf;
}

std::string AIAnalyst::build_market_context(const std::vector<Quote>& quotes,
                                              const MarketSummary& summary) {
    std::ostringstream ss; ss << std::fixed << std::setprecision(2);
    ss << "MARKET CONTEXT (live data):\n";
    ss << "Breadth: ADV=" << summary.advances << "% DEC=" << summary.declines
       << "% AvgChg=" << summary.avg_change_pct << "%\n";
    ss << "Top gainers: ";
    for (auto& g : summary.top_gainers) ss << g.symbol << "(" << g.change_pct << "%) ";
    ss << "\nTop losers: ";
    for (auto& g : summary.top_losers)  ss << g.symbol << "(" << g.change_pct << "%) ";
    ss << "\nKey prices:\n";
    for (auto& q : quotes) {
        if (q.symbol=="SPY"||q.symbol=="BTC"||q.symbol=="ETH"||q.symbol=="NVDA"||
            q.symbol=="AAPL"||q.symbol=="GLD"||q.symbol=="SOL"||q.symbol=="VNQ")
            ss << q.symbol << ":$" << q.price << "(" << q.change_pct << "%) ";
    }
    return ss.str();
}

static std::string json_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if      (c=='"')  r+="\\\"";
        else if (c=='\\') r+="\\\\";
        else if (c=='\n') r+="\\n";
        else if (c=='\r') r+="\\r";
        else              r+=c;
    }
    return r;
}

static std::string write_tmp_payload(const std::string& payload) {
    char path[] = "/tmp/bt_ai_payload_XXXXXX";
    int fd = mkstemp(path);
    if (fd == -1) return {};
    FILE* f = fdopen(fd, "w");
    if (!f) { close(fd); return {}; }
    fwrite(payload.data(), 1, payload.size(), f);
    fclose(f);
    return path;
}

std::string AIAnalyst::call_claude(const std::string& key,
                                    const std::string& system_prompt,
                                    const std::string& user_msg) {
    std::ostringstream payload;
    payload << "{\"model\":\"claude-haiku-4-5-20251001\","
            << "\"max_tokens\":600,"
            << "\"system\":\"" << json_escape(system_prompt) << "\","
            << "\"messages\":[{\"role\":\"user\",\"content\":\""
            << json_escape(user_msg) << "\"}]}";
    std::string tmpfile = write_tmp_payload(payload.str());
    if (tmpfile.empty()) return {};

    std::string cmd = "curl -fsSL --max-time 20 "
        "-H \"Content-Type: application/json\" "
        "-H \"x-api-key: " + key + "\" "
        "-H \"anthropic-version: 2023-06-01\" "
        "-d @" + tmpfile + " "
        "https://api.anthropic.com/v1/messages 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { std::remove(tmpfile.c_str()); return {}; }
    std::string result; std::array<char,4096> buf;
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    std::remove(tmpfile.c_str());
    return result;
}

std::string AIAnalyst::call_ollama(const std::string& model,
                                    const std::string& system_prompt,
                                    const std::string& user_msg) {
    std::ostringstream payload;
    payload << "{\"model\":\"" << json_escape(model) << "\","
            << "\"stream\":false,"
            << "\"messages\":["
            << "{\"role\":\"system\",\"content\":\"" << json_escape(system_prompt) << "\"},"
            << "{\"role\":\"user\",\"content\":\"" << json_escape(user_msg) << "\"}"
            << "]}";
    std::string tmpfile = write_tmp_payload(payload.str());
    if (tmpfile.empty()) return {};

    std::string cmd = "curl -fsSL --max-time 60 "
        "-H \"Content-Type: application/json\" "
        "-d @" + tmpfile + " "
        + ollama_host_ + "/api/chat 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { std::remove(tmpfile.c_str()); return {}; }
    std::string result; std::array<char,4096> buf;
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    std::remove(tmpfile.c_str());
    return result;
}

std::string AIAnalyst::call_api(AIBackend backend, const std::string& model, const std::string& key,
                                 const std::string& system_prompt, const std::string& user_msg) {
    if (backend == AIBackend::CLAUDE) return call_claude(key, system_prompt, user_msg);
    if (backend == AIBackend::OLLAMA) return call_ollama(model, system_prompt, user_msg);
    return {};
}

std::string AIAnalyst::extract_text(AIBackend backend, const std::string& raw) {
    auto outer = JsonParser::parse(raw);
    if (backend == AIBackend::CLAUDE) {
        const auto& content = outer["content"];
        return content.size() > 0 ? content[0]["text"].as_str() : std::string{};
    }
    if (backend == AIBackend::OLLAMA) {
        return outer["message"]["content"].as_str();
    }
    return {};
}

void AIAnalyst::parse_quick_response(AIBackend backend, const std::string& raw) {
    std::string text = extract_text(backend, raw);
    if (text.empty()) {
        std::lock_guard<std::mutex> lk(mu_);
        if (backend == AIBackend::OLLAMA) {
            result_.market_summary   = "Ollama error. Is `ollama serve` running?";
            result_.portfolio_advice = "ollama pull " + ollama_model_;
        } else {
            result_.market_summary   = "API error. Check ANTHROPIC_API_KEY.";
            result_.portfolio_advice = "export ANTHROPIC_API_KEY=sk-ant-...";
        }
        result_.top_opportunity  = "Then press [A] to retry.";
        result_.sentiment        = "NEUTRAL 5";
        result_.ready = true; return;
    }
    size_t js=text.find('{'), je=text.rfind('}');
    if (js!=std::string::npos && je!=std::string::npos) text=text.substr(js,je-js+1);
    auto j = JsonParser::parse(text);
    std::lock_guard<std::mutex> lk(mu_);
    result_.market_summary   = j["market"].as_str();
    result_.portfolio_advice = j["portfolio"].as_str();
    result_.top_opportunity  = j["opportunity"].as_str();
    result_.sentiment        = j["sentiment"].as_str();
    if (result_.market_summary.empty()) result_.market_summary = text;
    result_.ready = true; result_.timestamp = now_ts();
}

void AIAnalyst::analyze(const std::vector<Quote>& quotes, const MarketSummary& summary) {
    if (fetching_.load()) return;
    fetching_ = true;

    AIBackend   backend_snap;
    std::string model_snap, key_snap;
    {
        std::lock_guard<std::mutex> lk(mu_);
        backend_snap = backend_;
        model_snap   = ollama_model_;
        key_snap     = api_key_;
        result_.fetching = true; result_.ready = false;
        result_.market_summary = "Analyzing...";
    }
    std::string ctx = build_market_context(quotes, summary);
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, ctx, backend_snap, model_snap, key_snap]() {
        std::string sys = "You are a concise financial analyst. Respond ONLY in JSON: "
            "{\"market\":\"<2 sentences>\",\"portfolio\":\"<1 sentence>\","
            "\"opportunity\":\"<1 sentence>\",\"sentiment\":\"<BULLISH|NEUTRAL|BEARISH> <1-10>\"}";
        std::string raw = call_api(backend_snap, model_snap, key_snap, sys, ctx);
        parse_quick_response(backend_snap, raw);
        std::lock_guard<std::mutex> lk(mu_); result_.fetching = false; fetching_ = false;
    });
}

void AIAnalyst::chat(const std::string& user_prompt,
                      const std::vector<Quote>& quotes,
                      const MarketSummary& summary) {
    if (fetching_.load()) return;
    fetching_ = true;

    AIBackend   backend_snap;
    std::string model_snap, key_snap;
    {
        std::lock_guard<std::mutex> lk(mu_);
        backend_snap = backend_;
        model_snap   = ollama_model_;
        key_snap     = api_key_;
        history_.push_back({ChatMessage::USER, user_prompt, now_ts()});
    }
    std::string ctx = build_market_context(quotes, summary);
    std::string full_prompt = user_prompt
        + "\n\nReference market data (use only if relevant):\n" + ctx;
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, full_prompt, backend_snap, model_snap, key_snap]() {
        std::string sys =
            "You are a helpful, friendly assistant integrated into a financial terminal app. "
            "Talk naturally about anything the user brings up — casual conversation, questions, requests for help, whatever. "
            "You also have access to live market data (prices, sectors, gainers/losers) which is provided at the end of each message as reference context. "
            "Use it only when the user's question is actually about markets, specific assets, sectors, or their portfolio. "
            "For everything else, just respond like a normal conversational assistant — no need to mention markets or analysis unless asked. "
            "Be concise. Max 3 paragraphs.\n\n"
            "LANGUAGE RULE: Always reply in the same language the user's message is written in. "
            "Never switch languages partway through a response, even if the reference data below is in English.\n\n"
            "TONE RULE: For short casual messages (greetings, small talk, simple questions), "
            "reply in 1-2 sentences max — sound like a normal person, not a support bot. No bullet points, no disclaimers.\n\n"
            "Example:\n"
            "User: привет\n"
            "You: Привет! Как сам? Что интересного?\n\n"
            "Example:\n"
            "User: hi\n"
            "You: Hey! What's up?";
        std::string raw = call_api(backend_snap, model_snap, key_snap, sys, full_prompt);
        std::string text = extract_text(backend_snap, raw);
        if (text.empty()) {
            text = (backend_snap == AIBackend::OLLAMA)
                ? "Ollama error. Run: ollama serve  (and)  ollama pull " + model_snap
                : "API error. Check ANTHROPIC_API_KEY environment variable.";
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            history_.push_back({ChatMessage::ASSISTANT, text, now_ts()});
        }
        fetching_ = false;
    });
}