#include "dashboard_data.h"
#include "python/python_runner.h"
#include "http/http_client.h"
#include "storage/cache_service.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace fincept::dashboard {

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
                "^GSPC", "^DJI", "^IXIC", "^FTSE",
                "^GDAXI", "^N225", "^HSI", "^NSEI"
            };
        case DataCategory::Forex:
            return {
                // G10 Majors
                "EURUSD=X", "GBPUSD=X", "USDJPY=X", "USDCHF=X",
                "USDCAD=X", "AUDUSD=X", "NZDUSD=X", "EURCHF=X",
                // Key crosses
                "EURGBP=X", "EURJPY=X", "GBPJPY=X",
                // Key EM
                "USDCNY=X", "USDINR=X"
            };
        case DataCategory::Commodities:
            return {
                "GC=F", "SI=F", "CL=F", "NG=F", "HG=F"
            };
        case DataCategory::Crypto:
            return {
                "BTC-USD", "ETH-USD", "SOL-USD", "XRP-USD",
                "BNB-USD", "ADA-USD", "DOGE-USD"
            };
        case DataCategory::SectorETFs:
            return {
                "XLK", "XLV", "XLF", "XLE", "XLI", "XLY",
                "XLB", "XLU", "XLRE", "XLC"
            };
        case DataCategory::TopMovers:
            return {
                "NVDA", "TSLA", "AMD", "PLTR", "SMCI", "INTC"
            };
        case DataCategory::Ticker:
            return {};  // reuses Indices + Crypto + Forex data, no separate fetch
        case DataCategory::MarketPulse:
            return {
                "^VIX", "^TNX", "DX-Y.NYB"
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
                {"^FTSE", "FTSE 100"}, {"^GDAXI", "DAX"},
                {"^N225", "NIKKEI 225"}, {"^HSI", "HANG SENG"}, {"^NSEI", "NIFTY 50"}
            };
        case DataCategory::Forex:
            return {
                {"EURUSD=X", "EUR/USD"}, {"GBPUSD=X", "GBP/USD"}, {"USDJPY=X", "USD/JPY"},
                {"USDCHF=X", "USD/CHF"}, {"USDCAD=X", "USD/CAD"}, {"AUDUSD=X", "AUD/USD"},
                {"NZDUSD=X", "NZD/USD"}, {"EURCHF=X", "EUR/CHF"},
                {"EURGBP=X", "EUR/GBP"}, {"EURJPY=X", "EUR/JPY"}, {"GBPJPY=X", "GBP/JPY"},
                {"USDCNY=X", "USD/CNY"}, {"USDINR=X", "USD/INR"}
            };
        case DataCategory::Commodities:
            return {
                {"GC=F", "Gold"}, {"SI=F", "Silver"},
                {"CL=F", "WTI Crude"}, {"NG=F", "Natural Gas"}, {"HG=F", "Copper"}
            };
        case DataCategory::Crypto:
            return {
                {"BTC-USD", "Bitcoin"}, {"ETH-USD", "Ethereum"}, {"SOL-USD", "Solana"},
                {"XRP-USD", "XRP"}, {"BNB-USD", "BNB"}, {"ADA-USD", "Cardano"},
                {"DOGE-USD", "Dogecoin"}
            };
        case DataCategory::SectorETFs:
            return {
                {"XLK", "Technology"}, {"XLV", "Healthcare"}, {"XLF", "Financials"},
                {"XLE", "Energy"}, {"XLI", "Industrials"}, {"XLY", "Consumer Disc."},
                {"XLB", "Materials"}, {"XLU", "Utilities"}, {"XLRE", "Real Estate"},
                {"XLC", "Comm. Services"}
            };
        case DataCategory::TopMovers:
            return {
                {"NVDA", "NVIDIA"}, {"TSLA", "Tesla"}, {"AMD", "AMD"},
                {"PLTR", "Palantir"}, {"SMCI", "Super Micro"}, {"INTC", "Intel"}
            };
        case DataCategory::Ticker:
            return {};  // ticker bar assembles from other categories
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
// Python subprocess runner — uses python_runner module for path resolution
// =============================================================================
std::string DashboardData::run_yfinance(const std::string& args) {
    // Split args into vector
    std::vector<std::string> arg_vec;
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) arg_vec.push_back(token);

    auto result = python::execute("yfinance_data.py", arg_vec);
    if (result.success) return result.output;
    return "";
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

    auto syms = symbols_for(cat);
    if (syms.empty()) {
        state.has_data = true;
        state.last_fetch = std::chrono::steady_clock::now();
        return;
    }

    state.loading = true;
    auto lbls = labels_for(cat);

    std::thread([this, idx, syms, lbls]() {
        std::string args = "batch_quotes";
        for (auto& s : syms) args += " " + s;

        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("market-quotes", "dashboard", std::to_string(idx));

        std::string output;
        auto cached = cache.get(cache_key);
        if (cached && !cached->empty()) {
            output = *cached;
        } else {
            output = run_yfinance(args);
            if (!output.empty()) {
                cache.set(cache_key, output, "market-quotes", CacheTTL::FIVE_MIN);
            }
        }
        auto parsed = parse_quotes(output, lbls);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!parsed.empty()) {
                categories_[idx].quotes = std::move(parsed);
                categories_[idx].error.clear();
            } else if (categories_[idx].quotes.empty()) {
                categories_[idx].error = "Failed to fetch data";
            }
            categories_[idx].has_data = !categories_[idx].quotes.empty();
            categories_[idx].last_fetch = std::chrono::steady_clock::now();
        }
        categories_[idx].loading = false;
    }).detach();
}

void DashboardData::fetch_news_feeds() {
    if (news_loading_.load()) return;
    news_loading_ = true;

    // Fetch real RSS feeds from top financial news sources
    std::thread([this]() {
        struct FeedDef {
            const char* url;
            const char* source;
            const char* category;
            int priority;
        };

        // Tier 1-2 financial RSS feeds (same as NewsScreen)
        FeedDef feeds[] = {
            {"https://feeds.a.dj.com/rss/RSSMarketsMain.xml",   "WSJ",         "MARKETS",     2},
            {"https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "CNBC", "MARKETS", 2},
            {"https://feeds.marketwatch.com/marketwatch/topstories/", "MarketWatch", "MARKETS", 2},
            {"https://finance.yahoo.com/news/rssindex",          "Yahoo Finance","MARKETS",    2},
            {"https://www.investing.com/rss/news.rss",           "Investing.com","MARKETS",    2},
            {"https://www.coindesk.com/arc/outboundfeeds/rss/",  "CoinDesk",    "CRYPTO",      2},
            {"https://www.federalreserve.gov/feeds/press_all.xml","Fed Reserve", "ECONOMIC",    1},
        };

        auto& http = http::HttpClient::instance();
        std::vector<NewsItem> items;

        http::Headers hdrs;
        hdrs["User-Agent"] = "FinceptTerminal/4.0.0";
        hdrs["Accept"] = "application/rss+xml, application/xml, text/xml, */*";

        auto& cache = CacheService::instance();

        for (auto& feed : feeds) {
            std::string feed_cache_key = CacheService::make_key("news", "dashboard-rss", feed.source);
            std::string xml;

            auto feed_cached = cache.get(feed_cache_key);
            if (feed_cached && !feed_cached->empty()) {
                xml = *feed_cached;
            } else {
                auto resp = http.get(feed.url, hdrs);
                if (!resp.success || resp.body.empty()) continue;
                xml = resp.body;
                cache.set(feed_cache_key, xml, "news", CacheTTL::TEN_MIN);
            }
            size_t pos = 0;
            int count = 0;
            while (count < 5 && pos < xml.size()) {  // Max 5 items per feed
                size_t item_start = xml.find("<item>", pos);
                if (item_start == std::string::npos) item_start = xml.find("<entry>", pos);
                if (item_start == std::string::npos) break;

                size_t item_end = xml.find("</item>", item_start);
                if (item_end == std::string::npos) item_end = xml.find("</entry>", item_start);
                if (item_end == std::string::npos) break;

                std::string item_xml = xml.substr(item_start, item_end - item_start + 7);

                // Extract title
                auto extract_tag = [](const std::string& s, const std::string& tag) -> std::string {
                    std::string open = "<" + tag + ">";
                    std::string close = "</" + tag + ">";
                    size_t a = s.find(open);
                    if (a == std::string::npos) return "";
                    a += open.size();
                    // Handle CDATA
                    if (s.compare(a, 9, "<![CDATA[") == 0) a += 9;
                    size_t b = s.find(close, a);
                    if (b == std::string::npos) return "";
                    std::string val = s.substr(a, b - a);
                    // Strip CDATA end
                    if (val.size() >= 3 && val.substr(val.size() - 3) == "]]>")
                        val = val.substr(0, val.size() - 3);
                    return val;
                };

                std::string title = extract_tag(item_xml, "title");
                if (title.empty()) { pos = item_end + 7; continue; }

                // Strip HTML entities
                auto strip_entities = [](std::string& s) {
                    size_t p2 = 0;
                    while ((p2 = s.find("&amp;", p2)) != std::string::npos) s.replace(p2, 5, "&");
                    p2 = 0;
                    while ((p2 = s.find("&lt;", p2)) != std::string::npos) s.replace(p2, 4, "<");
                    p2 = 0;
                    while ((p2 = s.find("&gt;", p2)) != std::string::npos) s.replace(p2, 4, ">");
                    p2 = 0;
                    while ((p2 = s.find("&quot;", p2)) != std::string::npos) s.replace(p2, 6, "\"");
                    p2 = 0;
                    while ((p2 = s.find("&#39;", p2)) != std::string::npos) s.replace(p2, 5, "'");
                    p2 = 0;
                    while ((p2 = s.find("&apos;", p2)) != std::string::npos) s.replace(p2, 6, "'");
                };
                strip_entities(title);

                // Extract pubDate
                std::string date_str = extract_tag(item_xml, "pubDate");
                if (date_str.empty()) date_str = extract_tag(item_xml, "published");
                if (date_str.empty()) date_str = extract_tag(item_xml, "updated");

                NewsItem ni;
                ni.headline = title;
                ni.source = feed.source;
                ni.category = feed.category;
                ni.priority = feed.priority;

                // Parse date to timestamp (use current time if parse fails)
                ni.sort_ts = std::time(nullptr) - count * 60;  // Default: stagger by 1 min
                if (!date_str.empty()) {
                    // Try RFC 2822: "Wed, 15 Mar 2026 14:30:00 GMT"
                    struct tm t = {};
                    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                            "Jul","Aug","Sep","Oct","Nov","Dec"};
                    int day = 0, year = 0, hour = 0, min2 = 0, sec = 0;
                    char mon[8] = {};
                    if (std::sscanf(date_str.c_str(), "%*[^,], %d %3s %d %d:%d:%d",
                                    &day, mon, &year, &hour, &min2, &sec) >= 4) {
                        t.tm_mday = day; t.tm_year = year - 1900;
                        t.tm_hour = hour; t.tm_min = min2; t.tm_sec = sec;
                        for (int m = 0; m < 12; m++) {
                            if (std::strncmp(mon, months[m], 3) == 0) { t.tm_mon = m; break; }
                        }
                        time_t parsed = mktime(&t);
                        if (parsed > 0) ni.sort_ts = parsed;
                    } else if (std::sscanf(date_str.c_str(), "%d-%*d-%*dT%d:%d:%d",
                                           &year, &hour, &min2, &sec) >= 1) {
                        // ISO 8601 fallback — at least get the date
                        int iy = 0, im = 0, id = 0;
                        if (std::sscanf(date_str.c_str(), "%d-%d-%dT%d:%d:%d",
                                        &iy, &im, &id, &hour, &min2, &sec) >= 3) {
                            t.tm_year = iy - 1900; t.tm_mon = im - 1; t.tm_mday = id;
                            t.tm_hour = hour; t.tm_min = min2; t.tm_sec = sec;
                            time_t parsed = mktime(&t);
                            if (parsed > 0) ni.sort_ts = parsed;
                        }
                    }
                }

                time_t ts = static_cast<time_t>(ni.sort_ts);
                struct tm* lt = localtime(&ts);
                char tbuf[32];
                std::strftime(tbuf, sizeof(tbuf), "%H:%M", lt);
                ni.time_str = tbuf;

                items.push_back(ni);
                count++;
                pos = item_end + 7;
            }
        }

        // Sort newest first
        std::sort(items.begin(), items.end(),
            [](const NewsItem& a, const NewsItem& b) { return a.sort_ts > b.sort_ts; });

        // Limit to 30 items for dashboard widget
        if (items.size() > 30) items.resize(30);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            news_ = std::move(items);
            news_has_data_ = true;
        }
        news_loading_ = false;
    }).detach();
}

} // namespace fincept::dashboard
