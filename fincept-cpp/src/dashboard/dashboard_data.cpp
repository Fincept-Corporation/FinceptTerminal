#include "dashboard_data.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>
#include <ctime>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace fincept::dashboard {

// Python paths (same as markets_data.cpp)
static const char* PYTHON_EXE = "C:/ProgramData/miniconda3/python.exe";
static const char* YFINANCE_SCRIPT =
    "C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/"
    "src-tauri/resources/scripts/yfinance_data.py";

DashboardData& DashboardData::instance() {
    static DashboardData s;
    return s;
}

// =============================================================================
// Symbol lists — matching React's widget configurations exactly
// =============================================================================
std::vector<std::string> DashboardData::symbols_for(DataCategory cat) {
    switch (cat) {
        case DataCategory::Indices:
            return {
                "^GSPC", "^DJI", "^IXIC", "^RUT", "^FTSE",
                "^GDAXI", "^FCHI", "^N225", "^HSI", "000001.SS",
                "^BSESN", "^NSEI", "^STOXX50E", "^AXJO"
            };
        case DataCategory::Forex:
            return {
                "EURUSD=X", "GBPUSD=X", "USDJPY=X", "USDCHF=X",
                "USDCAD=X", "AUDUSD=X", "NZDUSD=X", "EURGBP=X",
                "EURJPY=X", "GBPJPY=X", "USDCNY=X", "USDINR=X"
            };
        case DataCategory::Commodities:
            return {
                "GC=F", "SI=F", "PL=F", "HG=F",
                "CL=F", "BZ=F", "NG=F",
                "ZC=F", "ZW=F", "ZS=F"
            };
        case DataCategory::Crypto:
            return {
                "BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD",
                "XRP-USD", "ADA-USD", "DOGE-USD", "LINK-USD",
                "DOT-USD", "AVAX-USD", "UNI-USD", "ATOM-USD"
            };
        case DataCategory::SectorETFs:
            return {
                "XLK", "XLV", "XLF", "XLE", "XLI",
                "XLB", "XLU", "XLRE", "XLC", "XLP", "XLY"
            };
        case DataCategory::TopMovers:
            return {
                "SMCI", "PLTR", "MSTR", "NVDA", "AMD", "TSLA",
                "INTC", "PFE", "BA", "NKE", "DIS", "PYPL"
            };
        case DataCategory::Ticker:
            return {
                "^GSPC", "^DJI", "^IXIC", "^RUT", "^FTSE", "^GDAXI", "^N225", "^HSI",
                "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "TSLA",
                "BTC-USD", "ETH-USD",
                "GC=F", "CL=F",
                "EURUSD=X", "GBPUSD=X", "USDJPY=X"
            };
        case DataCategory::MarketPulse:
            return {
                "^VIX", "^TNX", "DX-Y.NYB", "GC=F", "CL=F", "BTC-USD"
            };
        default:
            return {};
    }
}

std::map<std::string, std::string> DashboardData::labels_for(DataCategory cat) {
    switch (cat) {
        case DataCategory::Indices:
            return {
                {"^GSPC", "S&P 500"}, {"^DJI", "DOW JONES"}, {"^IXIC", "NASDAQ"},
                {"^RUT", "RUSSELL 2000"}, {"^FTSE", "FTSE 100"}, {"^GDAXI", "DAX"},
                {"^FCHI", "CAC 40"}, {"^N225", "NIKKEI 225"}, {"^HSI", "HANG SENG"},
                {"000001.SS", "SHANGHAI"}, {"^BSESN", "SENSEX"}, {"^NSEI", "NIFTY 50"},
                {"^STOXX50E", "EURO STOXX"}, {"^AXJO", "ASX 200"}
            };
        case DataCategory::Forex:
            return {
                {"EURUSD=X", "EUR/USD"}, {"GBPUSD=X", "GBP/USD"}, {"USDJPY=X", "USD/JPY"},
                {"USDCHF=X", "USD/CHF"}, {"USDCAD=X", "USD/CAD"}, {"AUDUSD=X", "AUD/USD"},
                {"NZDUSD=X", "NZD/USD"}, {"EURGBP=X", "EUR/GBP"}, {"EURJPY=X", "EUR/JPY"},
                {"GBPJPY=X", "GBP/JPY"}, {"USDCNY=X", "USD/CNY"}, {"USDINR=X", "USD/INR"}
            };
        case DataCategory::Commodities:
            return {
                {"GC=F", "Gold"}, {"SI=F", "Silver"}, {"PL=F", "Platinum"}, {"HG=F", "Copper"},
                {"CL=F", "WTI Crude"}, {"BZ=F", "Brent Crude"}, {"NG=F", "Natural Gas"},
                {"ZC=F", "Corn"}, {"ZW=F", "Wheat"}, {"ZS=F", "Soybeans"}
            };
        case DataCategory::Crypto:
            return {
                {"BTC-USD", "Bitcoin"}, {"ETH-USD", "Ethereum"}, {"BNB-USD", "BNB"},
                {"SOL-USD", "Solana"}, {"XRP-USD", "XRP"}, {"ADA-USD", "Cardano"},
                {"DOGE-USD", "Dogecoin"}, {"LINK-USD", "Chainlink"},
                {"DOT-USD", "Polkadot"}, {"AVAX-USD", "Avalanche"},
                {"UNI-USD", "Uniswap"}, {"ATOM-USD", "Cosmos"}
            };
        case DataCategory::SectorETFs:
            return {
                {"XLK", "Technology"}, {"XLV", "Healthcare"}, {"XLF", "Financials"},
                {"XLE", "Energy"}, {"XLI", "Industrials"}, {"XLB", "Materials"},
                {"XLU", "Utilities"}, {"XLRE", "Real Estate"}, {"XLC", "Comm. Services"},
                {"XLP", "Consumer Staples"}, {"XLY", "Consumer Disc."}
            };
        case DataCategory::TopMovers:
            return {
                {"SMCI", "Super Micro"}, {"PLTR", "Palantir"}, {"MSTR", "MicroStrategy"},
                {"NVDA", "NVIDIA"}, {"AMD", "AMD"}, {"TSLA", "Tesla"},
                {"INTC", "Intel"}, {"PFE", "Pfizer"}, {"BA", "Boeing"},
                {"NKE", "Nike"}, {"DIS", "Disney"}, {"PYPL", "PayPal"}
            };
        case DataCategory::Ticker:
            return {
                {"^GSPC", "S&P 500"}, {"^DJI", "DOW"}, {"^IXIC", "NASDAQ"},
                {"^RUT", "RUSSELL"}, {"^FTSE", "FTSE"}, {"^GDAXI", "DAX"},
                {"^N225", "NIKKEI"}, {"^HSI", "HANG SENG"},
                {"AAPL", "AAPL"}, {"MSFT", "MSFT"}, {"GOOGL", "GOOGL"},
                {"AMZN", "AMZN"}, {"NVDA", "NVDA"}, {"TSLA", "TSLA"},
                {"BTC-USD", "BTC"}, {"ETH-USD", "ETH"},
                {"GC=F", "GOLD"}, {"CL=F", "CRUDE"},
                {"EURUSD=X", "EUR/USD"}, {"GBPUSD=X", "GBP/USD"}, {"USDJPY=X", "USD/JPY"}
            };
        case DataCategory::MarketPulse:
            return {
                {"^VIX", "VIX"}, {"^TNX", "US 10Y"}, {"DX-Y.NYB", "DXY"},
                {"GC=F", "Gold"}, {"CL=F", "Oil"}, {"BTC-USD", "BTC"}
            };
        default:
            return {};
    }
}

// =============================================================================
// Python subprocess runner (same pattern as markets_data.cpp)
// =============================================================================
std::string DashboardData::run_yfinance(const std::string& args) {
    std::string cmd = std::string("\"") + PYTHON_EXE + "\" \""
                    + YFINANCE_SCRIPT + "\" " + args;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::string cmd_line = cmd;

    BOOL ok = CreateProcessA(
        nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return "";
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 60000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Extract last JSON line
    std::string json_line;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '{' || first == '[') json_line = line;
    }
    return json_line;
#else
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
    pclose(pipe);

    std::string json_line;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '{' || first == '[') json_line = line;
    }
    return json_line;
#endif
}

// =============================================================================
// Parse yfinance JSON array into QuoteEntry vector
// =============================================================================
std::vector<QuoteEntry> DashboardData::parse_quotes(
    const std::string& json_str,
    const std::map<std::string, std::string>& labels)
{
    std::vector<QuoteEntry> result;
    try {
        auto j = json::parse(json_str);
        if (!j.is_array()) return result;
        for (auto& item : j) {
            QuoteEntry q;
            q.symbol = item.value("symbol", "");
            q.price = item.value("price", 0.0);
            q.change = item.value("change", 0.0);
            q.change_percent = item.value("change_percent", 0.0);
            q.high = item.value("high", 0.0);
            q.low = item.value("low", 0.0);
            q.open = item.value("open", 0.0);
            q.previous_close = item.value("previous_close", 0.0);
            q.volume = item.value("volume", 0.0);

            // Apply label
            auto it = labels.find(q.symbol);
            q.label = (it != labels.end()) ? it->second : q.symbol;

            if (!q.symbol.empty() && q.price > 0)
                result.push_back(q);
        }
    } catch (...) {
        // parse error — return empty
    }
    return result;
}

// =============================================================================
// Lifecycle
// =============================================================================
void DashboardData::initialize() {
    if (initialized_) return;
    initialized_ = true;

    // Stagger initial fetches to avoid hammering Python all at once
    // Categories with shorter timers will fire first
    for (int i = 0; i < static_cast<int>(DataCategory::Count); i++) {
        categories_[i].timer = refresh_interval_ + 1.0f; // trigger immediately
    }
    news_timer_ = refresh_interval_ + 1.0f;
}

void DashboardData::update(float dt) {
    if (!initialized_) return;

    // Update timers and trigger fetches
    for (int i = 0; i < static_cast<int>(DataCategory::Count); i++) {
        auto& state = categories_[i];
        state.timer += dt;
        if (state.timer >= refresh_interval_ && !state.loading.load()) {
            state.timer = 0;
            fetch_category(static_cast<DataCategory>(i));
        }
    }

    news_timer_ += dt;
    if (news_timer_ >= refresh_interval_ && !news_loading_.load()) {
        news_timer_ = 0;
        fetch_news_feeds();
    }
}

void DashboardData::refresh_all() {
    for (int i = 0; i < static_cast<int>(DataCategory::Count); i++) {
        categories_[i].timer = refresh_interval_ + 1.0f;
    }
    news_timer_ = refresh_interval_ + 1.0f;
}

void DashboardData::refresh(DataCategory cat) {
    int idx = static_cast<int>(cat);
    if (idx >= 0 && idx < static_cast<int>(DataCategory::Count)) {
        categories_[idx].timer = refresh_interval_ + 1.0f;
    }
}

// =============================================================================
// Data access
// =============================================================================
std::vector<QuoteEntry> DashboardData::get_quotes(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    return categories_[idx].quotes;
}

std::vector<NewsItem> DashboardData::get_news() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return news_;
}

bool DashboardData::is_loading(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return false;
    return categories_[idx].loading.load();
}

bool DashboardData::has_data(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return categories_[idx].has_data;
}

std::string DashboardData::get_error(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return "";
    std::lock_guard<std::mutex> lock(mutex_);
    return categories_[idx].error;
}

float DashboardData::time_since_refresh(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return 999.0f;
    auto elapsed = std::chrono::steady_clock::now() - categories_[idx].last_fetch;
    return (float)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
}

std::string DashboardData::last_refresh_str(DataCategory cat) const {
    int idx = static_cast<int>(cat);
    if (idx < 0 || idx >= static_cast<int>(DataCategory::Count)) return "Never";
    auto tp = categories_[idx].last_fetch;
    if (tp == std::chrono::steady_clock::time_point{}) return "Never";
    float secs = time_since_refresh(cat);
    if (secs < 60) return std::to_string((int)secs) + "s ago";
    if (secs < 3600) return std::to_string((int)(secs / 60)) + "m ago";
    return std::to_string((int)(secs / 3600)) + "h ago";
}

void DashboardData::set_refresh_interval(float seconds) {
    refresh_interval_ = seconds;
}

// =============================================================================
// Background fetch
// =============================================================================
void DashboardData::fetch_category(DataCategory cat) {
    int idx = static_cast<int>(cat);
    auto& state = categories_[idx];
    if (state.loading.load()) return;
    state.loading = true;

    auto syms = symbols_for(cat);
    auto lbls = labels_for(cat);

    std::thread([this, idx, syms, lbls]() {
        // Build command: "batch_quotes SYM1 SYM2 ..."
        std::string args = "batch_quotes";
        for (auto& s : syms) args += " " + s;

        std::string output = run_yfinance(args);
        auto parsed = parse_quotes(output, lbls);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!parsed.empty()) {
                categories_[idx].quotes = std::move(parsed);
                categories_[idx].error.clear();
            } else if (categories_[idx].quotes.empty()) {
                categories_[idx].error = "Failed to fetch data";
            }
            // Keep stale data if fetch fails but we had data before
            categories_[idx].has_data = !categories_[idx].quotes.empty();
            categories_[idx].last_fetch = std::chrono::steady_clock::now();
        }
        categories_[idx].loading = false;
    }).detach();
}

void DashboardData::fetch_news_feeds() {
    if (news_loading_.load()) return;
    news_loading_ = true;

    // For now, news comes from hardcoded headlines with real timestamps
    // In a future iteration, this will call RSS feeds like NewsScreen does
    std::thread([this]() {
        // Placeholder: generate timestamped news items
        // Real implementation would reuse NewsScreen::fetch_news() RSS parsing
        std::vector<NewsItem> items;

        // We'll use the news from the existing news screen infrastructure
        // For the dashboard widget, we provide recent market headlines
        // This will be connected to the real RSS system in the next iteration
        time_t now = time(nullptr);

        const char* headlines[] = {
            "Fed officials signal cautious approach to rate decisions amid mixed economic signals",
            "Tech sector leads market rally as AI spending accelerates across enterprise",
            "Oil prices stabilize following OPEC+ production agreement extension",
            "Treasury yields edge higher on stronger-than-expected employment data",
            "Consumer spending shows resilience despite elevated inflation readings",
            "European Central Bank maintains rates, signals data-dependent approach",
            "Semiconductor stocks surge on robust demand forecasts for AI chips",
            "Cryptocurrency markets gain as institutional adoption continues to expand",
            "Manufacturing PMI crosses expansion threshold for first time in months",
            "Emerging market currencies strengthen as dollar index pulls back",
            "Housing starts decline as mortgage rates remain elevated above 6.5%",
            "Corporate earnings season shows mixed results across financial sector",
        };

        const char* sources[] = {
            "Reuters", "Bloomberg", "CNBC", "FT", "WSJ",
            "MarketWatch", "Reuters", "CoinDesk", "ISM",
            "Reuters", "NAR", "Bloomberg"
        };

        const char* cats[] = {
            "ECONOMIC", "MARKETS", "ENERGY", "MARKETS", "ECONOMIC",
            "ECONOMIC", "MARKETS", "CRYPTO", "ECONOMIC",
            "MARKETS", "ECONOMIC", "MARKETS"
        };

        for (int i = 0; i < 12; i++) {
            NewsItem item;
            item.headline = headlines[i];
            item.source = sources[i];
            item.category = cats[i];
            item.sort_ts = now - i * 900; // 15 min apart
            item.priority = (i < 2) ? 1 : 3;

            struct tm* t = localtime(&item.sort_ts);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%H:%M", t);
            item.time_str = buf;

            items.push_back(item);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            news_ = std::move(items);
            news_has_data_ = true;
        }
        news_loading_ = false;
    }).detach();
}

} // namespace fincept::dashboard
