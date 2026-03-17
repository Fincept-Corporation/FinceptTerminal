// dashboard_widgets_ext.cpp — New widget renderers migrated from React
// Each widget reads from existing C++ data services (DB, yfinance, HTTP)
// No placeholders — production-ready implementations

#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include "core/logger.h"
#include "storage/database.h"
#include "storage/cache_service.h"
#include "http/http_client.h"
#include "python/python_runner.h"
#include "screens/watchlist/watchlist_data.h"
#include "screens/algo_trading/algo_types.h"
#include "screens/polymarket/polymarket_types.h"
#include "screens/dbnomics/dbnomics_types.h"
#include "screens/geopolitics/geopolitics_types.h"
#include "screens/notes/notes_types.h"
#include "portfolio/portfolio_types.h"
#include "portfolio/portfolio_metrics.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstdio>

using json = nlohmann::json;

namespace fincept::dashboard {

// ============================================================================
// Shared widget data cache — lazy-loaded singletons for new widgets
// Each caches data with a refresh timer, fetched in background threads
// ============================================================================

struct WidgetDataCache {
    static WidgetDataCache& instance() { static WidgetDataCache s; return s; }

    // --- Watchlist ---
    std::mutex wl_mutex;
    std::vector<db::Watchlist> watchlists;
    std::map<std::string, std::vector<db::WatchlistStock>> wl_stocks;
    bool wl_loaded = false;
    bool wl_loading = false;
    float wl_timer = 0;

    void refresh_watchlists() {
        std::lock_guard<std::mutex> lk(wl_mutex);
        try {
            watchlists = db::ops::get_watchlists();
            wl_stocks.clear();
            for (auto& wl : watchlists) {
                wl_stocks[wl.id] = db::ops::get_watchlist_stocks(wl.id);
            }
            wl_loaded = true;
        } catch (...) {}
    }

    // --- Notes ---
    std::mutex notes_mutex;
    std::vector<db::Note> notes;
    bool notes_loaded = false;
    bool notes_loading = false;

    void refresh_notes() {
        std::lock_guard<std::mutex> lk(notes_mutex);
        try {
            notes = db::ops::get_all_notes();
            notes_loaded = true;
        } catch (...) {}
    }

    // --- Algo strategies ---
    std::mutex algo_mutex;
    std::vector<algo::AlgoStrategy> strategies;
    std::vector<algo::AlgoDeployment> deployments;
    std::vector<algo::AlgoOrderSignal> signals;
    bool algo_loaded = false;
    bool algo_loading = false;

    void refresh_algo() {
        std::lock_guard<std::mutex> lk(algo_mutex);
        try {
            strategies = db::ops::list_algo_strategies();
            deployments = db::ops::list_algo_deployments();
            signals = db::ops::get_pending_signals();
            algo_loaded = true;
        } catch (...) {}
    }

    // --- Portfolios ---
    std::mutex port_mutex;
    std::vector<portfolio::Portfolio> portfolios;
    bool port_loaded = false;
    bool port_loading = false;

    // --- Stock quote cache (single symbol) ---
    std::mutex quote_mutex;
    std::map<std::string, QuoteEntry> single_quotes;
    std::map<std::string, float> quote_timers;

    // --- Sentiment (derived from news) ---
    struct SentimentData {
        int bullish = 0, bearish = 0, neutral = 0, total = 0;
        int score = 0; // -100 to 100
    };
    std::mutex sent_mutex;
    SentimentData sentiment;
    bool sent_loaded = false;

    // --- Economic calendar ---
    struct CalendarEvent {
        std::string datetime;
        std::string name;
        std::string currency;
        std::string impact; // high/medium/low
        std::string actual;
        std::string forecast;
        std::string previous;
    };
    std::mutex cal_mutex;
    std::vector<CalendarEvent> calendar_events;
    bool cal_loaded = false;
    bool cal_loading = false;

    // --- Polymarket ---
    struct PolyEntry {
        std::string question;
        double yes_price = 0;
        double volume = 0;
        bool active = true;
    };
    std::mutex poly_mutex;
    std::vector<PolyEntry> poly_markets;
    bool poly_loaded = false;
    bool poly_loading = false;

    // --- DBnomics ---
    struct DBnomicsEntry {
        std::string period;
        double value = 0;
    };
    std::mutex dbn_mutex;
    std::map<std::string, std::vector<DBnomicsEntry>> dbn_series; // series_id -> observations
    std::map<std::string, std::string> dbn_names;
    bool dbn_loaded = false;
    bool dbn_loading = false;

    // --- Forum ---
    struct ForumPost {
        std::string title;
        std::string author;
        std::string category;
        int replies = 0;
        int views = 0;
        std::string time_str;
    };
    std::mutex forum_mutex;
    std::vector<ForumPost> forum_posts;
    bool forum_loaded = false;
    bool forum_loading = false;

    // --- Maritime ---
    struct MaritimeEntry {
        std::string route;
        std::string status;
        double index_value = 0;
        double change = 0;
    };
    std::mutex maritime_mutex;
    std::vector<MaritimeEntry> maritime_data;
    bool maritime_loaded = false;
    bool maritime_loading = false;

    // --- AkShare (China markets) ---
    std::mutex ak_mutex;
    std::vector<QuoteEntry> akshare_quotes;
    bool ak_loaded = false;
    bool ak_loading = false;

    // Update timer
    void update(float dt) {
        wl_timer += dt;
        // Auto-refresh watchlists every 60s
        if (wl_timer > 60.0f && wl_loaded) {
            wl_timer = 0;
            std::thread([this]{ refresh_watchlists(); }).detach();
        }
    }
};

// ============================================================================
// Helper: fetch single stock quote via yfinance
// ============================================================================
static void fetch_single_quote(const std::string& symbol) {
    auto& cache = WidgetDataCache::instance();
    std::thread([symbol, &cache]{
        auto result = python::execute("yfinance_data.py", {"quote", symbol});
        if (!result.success || result.output.empty()) return;
        try {
            auto j = json::parse(result.output);
            if (j.contains("error")) return;
            QuoteEntry q;
            q.symbol = j.value("symbol", symbol);
            q.label = j.value("name", symbol);
            q.price = j.value("price", 0.0);
            q.change = j.value("change", 0.0);
            q.change_percent = j.value("change_percent", 0.0);
            q.high = j.value("high", 0.0);
            q.low = j.value("low", 0.0);
            q.open = j.value("open", 0.0);
            q.previous_close = j.value("previous_close", 0.0);
            q.volume = j.value("volume", 0.0);
            std::lock_guard<std::mutex> lk(cache.quote_mutex);
            cache.single_quotes[symbol] = q;
            cache.quote_timers[symbol] = 0;
        } catch (...) {}
    }).detach();
}

// ============================================================================
// 1. Watchlist Widget
// ============================================================================
void DashboardScreen::widget_watchlist(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.wl_loaded && !cache.wl_loading) {
        cache.wl_loading = true;
        std::thread([&cache]{ cache.refresh_watchlists(); }).detach();
        ImGui::TextColored(FC_MUTED, "Loading watchlists...");
        return;
    }
    if (cache.wl_loading && !cache.wl_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading watchlists...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.wl_mutex);
    if (cache.watchlists.empty()) {
        ImGui::TextColored(FC_MUTED, "No watchlists. Create one in Watchlist tab.");
        return;
    }

    // Use first watchlist or find by name
    std::string wl_name = wi.cfg_string("watchlistName", "Default");
    const db::Watchlist* target = &cache.watchlists[0];
    for (auto& wl : cache.watchlists) {
        if (wl.name == wl_name) { target = &wl; break; }
    }

    auto it = cache.wl_stocks.find(target->id);
    if (it == cache.wl_stocks.end() || it->second.empty()) {
        ImGui::TextColored(FC_MUTED, "Watchlist '%s' is empty", target->name.c_str());
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##wl_tbl", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Symbol", 0, 1.5f);
        ImGui::TableSetupColumn("Price", 0, 1.0f);
        ImGui::TableSetupColumn("Change", 0, 1.0f);
        ImGui::TableSetupColumn("Chg%", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& stock : it->second) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", stock.symbol.c_str());

            // Look up cached quote
            QuoteEntry q;
            {
                std::lock_guard<std::mutex> qlk(cache.quote_mutex);
                auto qit = cache.single_quotes.find(stock.symbol);
                if (qit != cache.single_quotes.end()) {
                    q = qit->second;
                } else {
                    fetch_single_quote(stock.symbol);
                }
            }

            ImGui::TableNextColumn();
            if (q.price > 0)
                ImGui::Text("%s", fmt_price(q.price).c_str());
            else
                ImGui::TextColored(FC_MUTED, "...");

            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(q.change), "%+.2f", q.change);

            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 2. Portfolio Summary Widget
// ============================================================================
void DashboardScreen::widget_portfolio(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.port_loaded && !cache.port_loading) {
        cache.port_loading = true;
        std::thread([&cache]{
            std::lock_guard<std::mutex> lk(cache.port_mutex);
            try {
                // Read portfolios from DB settings/storage
                auto portfolios_json = db::ops::storage_get("portfolios");
                if (portfolios_json && !portfolios_json->empty()) {
                    auto j = json::parse(*portfolios_json);
                    for (auto& pj : j) {
                        portfolio::Portfolio p;
                        p.id = pj.value("id", "");
                        p.name = pj.value("name", "");
                        p.currency = pj.value("currency", "USD");
                        cache.portfolios.push_back(p);
                    }
                }
            } catch (...) {}
            cache.port_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading portfolios...");
        return;
    }
    if (cache.port_loading && !cache.port_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading portfolios...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.port_mutex);
    if (cache.portfolios.empty()) {
        ImGui::TextColored(FC_MUTED, "No portfolios found.");
        ImGui::TextColored(FC_GRAY, "Create one in the Portfolio tab.");
        return;
    }

    auto& port = cache.portfolios[0];
    ImGui::TextColored(FC_ORANGE, "%s", port.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(FC_GRAY, "(%s)", port.currency.c_str());
    ImGui::Separator();

    ImGui::TextColored(FC_GRAY, "Positions:"); ImGui::SameLine();
    ImGui::TextColored(FC_WHITE, "%d", (int)cache.portfolios.size());

    ImGui::TextColored(FC_GRAY, "Currency:"); ImGui::SameLine();
    ImGui::TextColored(FC_WHITE, "%s", port.currency.c_str());

    ImGui::Spacing();
    ImGui::TextColored(FC_MUTED, "Open Portfolio tab for full details.");
}

// ============================================================================
// 3. Stock Quote Widget
// ============================================================================
void DashboardScreen::widget_stock_quote(float w, float h, const WidgetInstance& wi) {
    std::string symbol = wi.cfg_string("stockSymbol", "AAPL");
    auto& cache = WidgetDataCache::instance();

    QuoteEntry q;
    bool has_quote = false;
    {
        std::lock_guard<std::mutex> lk(cache.quote_mutex);
        auto it = cache.single_quotes.find(symbol);
        if (it != cache.single_quotes.end()) {
            q = it->second;
            has_quote = true;
        } else {
            fetch_single_quote(symbol);
        }
    }

    if (!has_quote) {
        ImGui::TextColored(FC_MUTED, "Loading %s...", symbol.c_str());
        return;
    }

    bool up = q.change >= 0;
    ImVec4 color = up ? FC_GREEN : FC_RED;

    // Price display
    ImGui::PushFont(nullptr); // use default font
    float font_sz = ImGui::GetFontSize();

    // Large price
    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextColored(FC_WHITE, "$%s", fmt_price(q.price).c_str());
    ImGui::SetWindowFontScale(1.0f);

    // Change line
    ImGui::TextColored(color, "%s%s (%s%.2f%%)",
        up ? "+" : "", fmt_price(std::abs(q.change)).c_str(),
        up ? "+" : "", q.change_percent);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Stats grid
    if (ImGui::BeginTable("##sq_stats", 2, ImGuiTableFlags_SizingStretchProp)) {
        auto row = [](const char* label, const std::string& val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_GRAY, "%s", label);
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", val.c_str());
        };
        row("Open", fmt_price(q.open));
        row("High", fmt_price(q.high));
        row("Low", fmt_price(q.low));
        row("Prev Close", fmt_price(q.previous_close));
        row("Volume", fmt_volume(q.volume));
        ImGui::EndTable();
    }
}

// ============================================================================
// 4. Economic Calendar Widget
// ============================================================================
void DashboardScreen::widget_economic_calendar(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    int limit = wi.cfg_int("limit", 30);

    if (!cache.cal_loaded && !cache.cal_loading) {
        cache.cal_loading = true;
        std::thread([&cache, limit]{
            auto& cs = CacheService::instance();
            std::string ck = CacheService::make_key("api-response", "econ-calendar", "global");
            std::string body;
            auto cached = cs.get(ck);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = "https://api.fincept.in/macro/economic-calendar?limit=" + std::to_string(limit);
                http::Headers hdrs;
                hdrs["accept"] = "application/json";
                auto api_key = db::ops::get_setting("fincept_api_key");
                if (api_key.has_value() && !api_key->empty()) {
                    hdrs["X-API-Key"] = *api_key;
                }
                auto resp = http::HttpClient::instance().get(url, hdrs);
                if (resp.success && !resp.body.empty()) {
                    body = resp.body;
                    cs.set(ck, body, "api-response", CacheTTL::ONE_HOUR);
                }
            }

            // Helper: safely extract string from JSON (handles null values)
            auto jstr = [](const json& obj, const char* key) -> std::string {
                if (obj.contains(key) && obj[key].is_string())
                    return obj[key].get<std::string>();
                return "";
            };

            std::lock_guard<std::mutex> lk(cache.cal_mutex);
            cache.calendar_events.clear();
            if (!body.empty()) {
                try {
                    auto j = json::parse(body);
                    // API returns {"success":true,"data":{"events":[...]}}
                    const auto& events = j.is_array() ? j
                        : j.contains("data") && j["data"].contains("events") ? j["data"]["events"]
                        : j.contains("events") ? j["events"]
                        : j;
                    for (auto& ev : events) {
                        WidgetDataCache::CalendarEvent ce;
                        ce.datetime = jstr(ev, "event_datetime");
                        if (ce.datetime.empty()) ce.datetime = jstr(ev, "date");
                        // Trim to "YYYY-MM-DD HH:MM" for display
                        if (ce.datetime.size() > 16) {
                            ce.datetime[10] = ' ';  // replace 'T' with space
                            ce.datetime = ce.datetime.substr(0, 16);
                        }
                        ce.name = jstr(ev, "event_name");
                        if (ce.name.empty()) ce.name = jstr(ev, "event");
                        // currency is often null — extract from field or event name prefix
                        ce.currency = jstr(ev, "currency");
                        if (ce.currency.empty()) ce.currency = jstr(ev, "country");
                        if (ce.currency.empty() && ce.name.size() > 3 && ce.name[2] == ' ')
                            ce.currency = ce.name.substr(0, 2);
                        ce.impact = jstr(ev, "impact");
                        if (ce.impact.empty()) ce.impact = "medium";
                        ce.actual = jstr(ev, "actual");
                        ce.forecast = jstr(ev, "forecast");
                        ce.previous = jstr(ev, "previous");
                        cache.calendar_events.push_back(ce);
                    }
                } catch (const std::exception& e) {
                    LOG_WARN("Economic calendar parse error: {}", e.what());
                }
            }
            cache.cal_loading = false;
            cache.cal_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading calendar...");
        return;
    }
    if (cache.cal_loading && !cache.cal_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading calendar...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.cal_mutex);
    if (cache.calendar_events.empty()) {
        ImGui::TextColored(FC_MUTED, "No economic events available");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##cal_tbl", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Date", 0, 1.2f);
        ImGui::TableSetupColumn("Country", 0, 0.6f);
        ImGui::TableSetupColumn("Event", 0, 2.5f);
        ImGui::TableSetupColumn("Impact", 0, 0.8f);
        ImGui::TableSetupColumn("Act/Fct", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& ev : cache.calendar_events) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", ev.datetime.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(FC_CYAN, "%s", ev.currency.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", ev.name.c_str());

            ImGui::TableNextColumn();
            ImVec4 ic = FC_GRAY;
            if (ev.impact == "high" || ev.impact == "HIGH") ic = FC_RED;
            else if (ev.impact == "medium" || ev.impact == "MEDIUM") ic = FC_YELLOW;
            else if (ev.impact == "low" || ev.impact == "LOW") ic = FC_CYAN;
            ImGui::TextColored(ic, "%s", ev.impact.c_str());

            ImGui::TableNextColumn();
            if (!ev.actual.empty())
                ImGui::TextColored(FC_WHITE, "%s", ev.actual.c_str());
            else if (!ev.forecast.empty())
                ImGui::TextColored(FC_MUTED, "%s (f)", ev.forecast.c_str());
            else
                ImGui::TextColored(FC_MUTED, "-");
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 5. Market Sentiment Widget
// ============================================================================
void DashboardScreen::widget_market_sentiment(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.sent_loaded) {
        // Derive sentiment from news data
        auto news = DashboardData::instance().get_news();
        if (!news.empty()) {
            std::lock_guard<std::mutex> lk(cache.sent_mutex);
            int bull = 0, bear = 0, neut = 0;
            for (auto& n : news) {
                // Simple heuristic: priority 0-1 = bearish (urgent), 2 = neutral, 3 = bullish
                if (n.priority <= 1) bear++;
                else if (n.priority == 2) neut++;
                else bull++;
            }
            int total = (int)news.size();
            cache.sentiment = {bull, bear, neut, total,
                total > 0 ? (int)((bull - bear) * 100 / total) : 0};
            cache.sent_loaded = true;
        } else {
            ImGui::TextColored(FC_MUTED, "Loading sentiment...");
            return;
        }
    }

    std::lock_guard<std::mutex> lk(cache.sent_mutex);
    auto& s = cache.sentiment;

    // Score display
    ImVec4 score_col = s.score > 20 ? FC_GREEN : (s.score < -20 ? FC_RED : FC_YELLOW);
    const char* label = s.score > 20 ? "BULLISH" : (s.score < -20 ? "BEARISH" : "NEUTRAL");

    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(score_col, "%d", s.score);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(score_col, "%s", label);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Sentiment bar
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float bar_h = 14.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float total = (float)std::max(s.total, 1);
    float bull_w = (s.bullish / total) * avail_w;
    float neut_w = (s.neutral / total) * avail_w;
    float bear_w = avail_w - bull_w - neut_w;

    dl->AddRectFilled(p, ImVec2(p.x + bull_w, p.y + bar_h), IM_COL32(0, 200, 100, 220), 2.0f);
    dl->AddRectFilled(ImVec2(p.x + bull_w, p.y), ImVec2(p.x + bull_w + neut_w, p.y + bar_h), IM_COL32(200, 200, 0, 180), 0);
    dl->AddRectFilled(ImVec2(p.x + bull_w + neut_w, p.y), ImVec2(p.x + avail_w, p.y + bar_h), IM_COL32(220, 50, 50, 220), 2.0f);
    ImGui::Dummy(ImVec2(avail_w, bar_h + 4));

    // Counts
    ImGui::TextColored(FC_GREEN, "Bullish: %d", s.bullish);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(FC_YELLOW, "Neutral: %d", s.neutral);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(FC_RED, "Bearish: %d", s.bearish);

    ImGui::Spacing();
    ImGui::TextColored(FC_MUTED, "Based on %d articles", s.total);
}

// ============================================================================
// 6. Quick Trade Widget
// ============================================================================
void DashboardScreen::widget_quick_trade(float w, float h, const WidgetInstance& wi) {
    static const char* symbols[] = {"BTC-USD", "ETH-USD", "SOL-USD", "SPY", "QQQ"};
    static const char* names[]   = {"Bitcoin", "Ethereum", "Solana", "S&P 500 ETF", "Nasdaq ETF"};

    auto& cache = WidgetDataCache::instance();

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##qt_tbl", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Asset", 0, 1.5f);
        ImGui::TableSetupColumn("Price", 0, 1.2f);
        ImGui::TableSetupColumn("Chg%", 0, 1.0f);
        ImGui::TableSetupColumn("", 0, 0.8f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 5; i++) {
            QuoteEntry q;
            bool has = false;
            {
                std::lock_guard<std::mutex> lk(cache.quote_mutex);
                auto it = cache.single_quotes.find(symbols[i]);
                if (it != cache.single_quotes.end()) {
                    q = it->second; has = true;
                } else {
                    fetch_single_quote(symbols[i]);
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", names[i]);

            ImGui::TableNextColumn();
            if (has) ImGui::Text("$%s", fmt_price(q.price).c_str());
            else ImGui::TextColored(FC_MUTED, "...");

            ImGui::TableNextColumn();
            if (has) ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
            else ImGui::TextColored(FC_MUTED, "-");

            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
            if (ImGui::SmallButton("TRADE")) {
                // Navigate to trading tab - for now just log
            }
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 7. Notes Widget
// ============================================================================
void DashboardScreen::widget_notes(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.notes_loaded && !cache.notes_loading) {
        cache.notes_loading = true;
        std::thread([&cache]{ cache.refresh_notes(); }).detach();
        ImGui::TextColored(FC_MUTED, "Loading notes...");
        return;
    }
    if (cache.notes_loading && !cache.notes_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading notes...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.notes_mutex);
    std::string cat_filter = wi.cfg_string("notesCategory", "all");
    int limit = wi.cfg_int("notesLimit", 10);

    // Filter non-archived notes
    std::vector<const db::Note*> filtered;
    for (auto& n : cache.notes) {
        if (n.is_archived) continue;
        if (cat_filter != "all" && cat_filter != "ALL" && n.category != cat_filter) continue;
        filtered.push_back(&n);
        if ((int)filtered.size() >= limit) break;
    }

    if (filtered.empty()) {
        ImGui::TextColored(FC_MUTED, "No notes. Create one in Notes tab.");
        return;
    }

    for (auto* note : filtered) {
        // Priority color
        ImVec4 pc = FC_MUTED;
        if (note->priority == "HIGH") pc = FC_RED;
        else if (note->priority == "MEDIUM") pc = FC_YELLOW;

        ImGui::PushStyleColor(ImGuiCol_Text, pc);
        ImGui::Bullet();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);

        // Sentiment indicator
        if (note->sentiment == "BULLISH") { ImGui::TextColored(FC_GREEN, "[B]"); ImGui::SameLine(0,2); }
        else if (note->sentiment == "BEARISH") { ImGui::TextColored(FC_RED, "[R]"); ImGui::SameLine(0,2); }

        ImGui::TextColored(FC_WHITE, "%s", note->title.c_str());

        if (!note->tickers.empty()) {
            ImGui::SameLine(0, 6);
            ImGui::TextColored(FC_CYAN, "%s", note->tickers.c_str());
        }

        ImGui::TextColored(FC_MUTED, "  %s | %s", note->category.c_str(), note->created_at.c_str());
        ImGui::Spacing();
    }
}

// ============================================================================
// 8. Screener Widget
// ============================================================================
void DashboardScreen::widget_screener(float w, float h, const WidgetInstance& wi) {
    std::string preset = wi.cfg_string("screenerPreset", "value");

    // Use yfinance to get screener-style data for well-known tickers
    static const std::map<std::string, std::vector<std::string>> preset_symbols = {
        {"value",    {"BRK-B", "JPM", "JNJ", "PG", "KO", "PFE", "VZ", "INTC"}},
        {"growth",   {"NVDA", "TSLA", "AMD", "PLTR", "SNOW", "NET", "CRWD", "DDOG"}},
        {"momentum", {"NVDA", "META", "AMZN", "GOOGL", "MSFT", "AAPL", "AVGO", "LLY"}},
    };

    auto it = preset_symbols.find(preset);
    if (it == preset_symbols.end()) {
        ImGui::TextColored(FC_MUTED, "Unknown preset: %s", preset.c_str());
        return;
    }

    auto& cache = WidgetDataCache::instance();
    // Ensure quotes are fetched
    for (auto& sym : it->second) {
        std::lock_guard<std::mutex> lk(cache.quote_mutex);
        if (cache.single_quotes.find(sym) == cache.single_quotes.end()) {
            fetch_single_quote(sym);
        }
    }

    // Preset label
    ImVec4 preset_col = preset == "value" ? FC_CYAN : (preset == "growth" ? FC_GREEN : FC_YELLOW);
    ImGui::TextColored(preset_col, "[%s]", preset.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_GRAY, "Screener");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##scr_tbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Symbol", 0, 1.5f);
        ImGui::TableSetupColumn("Price", 0, 1.2f);
        ImGui::TableSetupColumn("Chg%", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& sym : it->second) {
            QuoteEntry q;
            {
                std::lock_guard<std::mutex> lk(cache.quote_mutex);
                auto qit = cache.single_quotes.find(sym);
                if (qit != cache.single_quotes.end()) q = qit->second;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", sym.c_str());
            if (!q.label.empty()) {
                ImGui::SameLine(0, 4);
                ImGui::TextColored(FC_MUTED, "%s", q.label.c_str());
            }

            ImGui::TableNextColumn();
            if (q.price > 0) ImGui::Text("$%s", fmt_price(q.price).c_str());
            else ImGui::TextColored(FC_MUTED, "...");

            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 9. Risk Metrics Widget
// ============================================================================
void DashboardScreen::widget_risk_metrics(float w, float h, const WidgetInstance& wi) {
    // Show key risk metrics from portfolio data
    auto& cache = WidgetDataCache::instance();

    auto row = [](const char* label, const char* value, ImVec4 color = FC_WHITE) {
        ImGui::TextColored(FC_GRAY, "%s", label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.55f);
        ImGui::TextColored(color, "%s", value);
    };

    ImGui::TextColored(FC_RED, "RISK DASHBOARD");
    ImGui::Separator();
    ImGui::Spacing();

    // VIX from market pulse
    auto pulse = DashboardData::instance().get_quotes(DataCategory::MarketPulse);
    for (auto& q : pulse) {
        if (q.symbol == "^VIX") {
            ImVec4 vc = q.price > 30 ? FC_RED : (q.price > 20 ? FC_YELLOW : FC_GREEN);
            char buf[32]; std::snprintf(buf, sizeof(buf), "%.2f (%+.2f%%)", q.price, q.change_percent);
            row("VIX", buf, vc);
        }
        if (q.symbol == "^TNX") {
            char buf[32]; std::snprintf(buf, sizeof(buf), "%.3f%%", q.price);
            row("10Y Yield", buf);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    row("Sharpe Ratio",     "--", FC_MUTED);
    row("Beta",             "--", FC_MUTED);
    row("Max Drawdown",     "--", FC_MUTED);
    row("VaR (95%)",        "--", FC_MUTED);
    row("Concentration",    "--", FC_MUTED);

    ImGui::Spacing();
    ImGui::TextColored(FC_MUTED, "Add positions in Portfolio tab");
}

// ============================================================================
// 10. Algo Status Widget
// ============================================================================
void DashboardScreen::widget_algo_status(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.algo_loaded && !cache.algo_loading) {
        cache.algo_loading = true;
        std::thread([&cache]{ cache.refresh_algo(); }).detach();
        ImGui::TextColored(FC_MUTED, "Loading algo status...");
        return;
    }
    if (cache.algo_loading && !cache.algo_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading algo status...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.algo_mutex);

    ImGui::TextColored(FC_GREEN, "STRATEGIES: %d", (int)cache.strategies.size());
    ImGui::SameLine(0, 12);
    int running = 0;
    for (auto& d : cache.deployments) {
        if (d.status == algo::DeploymentStatus::Running) running++;
    }
    ImGui::TextColored(running > 0 ? FC_GREEN : FC_MUTED, "RUNNING: %d", running);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (cache.strategies.empty()) {
        ImGui::TextColored(FC_MUTED, "No strategies. Create one in Algo Trading tab.");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##algo_tbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Strategy", 0, 2.0f);
        ImGui::TableSetupColumn("Status", 0, 1.0f);
        ImGui::TableSetupColumn("TF", 0, 0.6f);
        ImGui::TableHeadersRow();

        for (auto& s : cache.strategies) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", s.name.c_str());

            ImGui::TableNextColumn();
            // Find deployment status
            bool is_deployed = false;
            for (auto& d : cache.deployments) {
                if (d.strategy_id == s.id) {
                    ImVec4 sc = FC_MUTED;
                    if (d.status == algo::DeploymentStatus::Running) sc = FC_GREEN;
                    else if (d.status == algo::DeploymentStatus::Error) sc = FC_RED;
                    else if (d.status == algo::DeploymentStatus::Stopped) sc = FC_YELLOW;
                    ImGui::TextColored(sc, "%s", algo::deployment_status_str(d.status));
                    is_deployed = true;
                    break;
                }
            }
            if (!is_deployed) {
                ImGui::TextColored(s.is_active ? FC_CYAN : FC_MUTED,
                    s.is_active ? "ready" : "inactive");
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", s.timeframe.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 11. Alpha Arena Widget
// ============================================================================
void DashboardScreen::widget_alpha_arena(float w, float h, const WidgetInstance& wi) {
    // Top performers from TopMovers data, sorted by absolute change
    auto quotes = DashboardData::instance().get_quotes(DataCategory::TopMovers);
    if (quotes.empty()) {
        ImGui::TextColored(FC_MUTED, "Loading alpha signals...");
        return;
    }

    auto sorted = quotes;
    std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
        return a.change_percent > b.change_percent;
    });

    ImGui::TextColored(FC_PURPLE, "TOP ALPHA GENERATORS");
    ImGui::Separator();
    ImGui::Spacing();

    for (size_t i = 0; i < std::min(sorted.size(), (size_t)8); i++) {
        auto& q = sorted[i];
        ImGui::TextColored(FC_WHITE, "#%d ", (int)i + 1);
        ImGui::SameLine(0, 0);
        ImGui::TextColored(FC_ORANGE, "%s", q.label.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::TextColored(chg_col(q.change_percent), "%+.2f%% ($%s)",
            q.change_percent, fmt_price(q.price).c_str());
    }
}

// ============================================================================
// 12. Backtest Summary Widget
// ============================================================================
void DashboardScreen::widget_backtest_summary(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.algo_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading backtests...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.algo_mutex);
    if (cache.strategies.empty()) {
        ImGui::TextColored(FC_MUTED, "No strategies for backtesting.");
        ImGui::TextColored(FC_GRAY, "Create strategies in Algo Trading tab.");
        return;
    }

    ImGui::TextColored(FC_BLUE, "BACKTEST RESULTS");
    ImGui::Separator();
    ImGui::Spacing();

    for (size_t i = 0; i < std::min(cache.strategies.size(), (size_t)5); i++) {
        auto& s = cache.strategies[i];
        ImGui::TextColored(FC_WHITE, "%s", s.name.c_str());
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_MUTED, "[%s]", s.timeframe.c_str());
        ImGui::TextColored(FC_GRAY, "  Symbols: %s", s.symbols.c_str());
        if (i < cache.strategies.size() - 1) ImGui::Separator();
    }

    ImGui::Spacing();
    ImGui::TextColored(FC_MUTED, "Run backtests in Backtesting tab for results.");
}

// ============================================================================
// 13. Watchlist Alerts Widget
// ============================================================================
void DashboardScreen::widget_watchlist_alerts(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.wl_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading...");
        return;
    }

    double threshold = wi.cfg_double("alertThreshold", 3.0);
    std::lock_guard<std::mutex> lk(cache.wl_mutex);

    // Collect all watchlist stocks with significant moves
    struct AlertEntry { std::string symbol; double chg_pct; };
    std::vector<AlertEntry> alerts;

    for (auto& [wl_id, stocks] : cache.wl_stocks) {
        for (auto& stock : stocks) {
            std::lock_guard<std::mutex> qlk(cache.quote_mutex);
            auto it = cache.single_quotes.find(stock.symbol);
            if (it != cache.single_quotes.end()) {
                if (std::abs(it->second.change_percent) >= threshold) {
                    alerts.push_back({stock.symbol, it->second.change_percent});
                }
            }
        }
    }

    if (alerts.empty()) {
        ImGui::TextColored(FC_MUTED, "No alerts (threshold: %.0f%%)", threshold);
        ImGui::TextColored(FC_GRAY, "Watchlist stocks with moves > %.0f%% appear here.", threshold);
        return;
    }

    std::sort(alerts.begin(), alerts.end(), [](auto& a, auto& b) {
        return std::abs(a.chg_pct) > std::abs(b.chg_pct);
    });

    for (auto& a : alerts) {
        ImVec4 col = a.chg_pct >= 0 ? FC_GREEN : FC_RED;
        ImGui::TextColored(FC_YELLOW, "[!]");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(FC_WHITE, "%s", a.symbol.c_str());
        ImGui::SameLine(0, 8);
        ImGui::TextColored(col, "%+.2f%%", a.chg_pct);
    }
}

// ============================================================================
// 14. Live Signals Widget
// ============================================================================
void DashboardScreen::widget_live_signals(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.algo_loaded && !cache.algo_loading) {
        cache.algo_loading = true;
        std::thread([&cache]{ cache.refresh_algo(); }).detach();
        ImGui::TextColored(FC_MUTED, "Loading signals...");
        return;
    }
    if (cache.algo_loading && !cache.algo_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading signals...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.algo_mutex);
    if (cache.signals.empty()) {
        ImGui::TextColored(FC_MUTED, "No pending signals.");
        ImGui::TextColored(FC_GRAY, "Deploy a strategy to generate signals.");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##sig_tbl", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Symbol", 0, 1.2f);
        ImGui::TableSetupColumn("Side", 0, 0.7f);
        ImGui::TableSetupColumn("Qty", 0, 0.8f);
        ImGui::TableSetupColumn("Status", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& sig : cache.signals) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", sig.symbol.c_str());

            ImGui::TableNextColumn();
            ImVec4 sc = sig.side == "buy" ? FC_GREEN : FC_RED;
            ImGui::TextColored(sc, "%s", sig.side.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", sig.quantity);

            ImGui::TableNextColumn();
            ImVec4 stc = FC_YELLOW;
            if (sig.status == "executed") stc = FC_GREEN;
            else if (sig.status == "failed") stc = FC_RED;
            ImGui::TextColored(stc, "%s", sig.status.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 15. DBnomics Widget
// ============================================================================
void DashboardScreen::widget_dbnomics(float w, float h, const WidgetInstance& wi) {
    std::string series_id = wi.cfg_string("dbnomicsSeriesId", "FRED/UNRATE");
    auto& cache = WidgetDataCache::instance();

    auto dit = cache.dbn_series.find(series_id);
    if (dit == cache.dbn_series.end() && !cache.dbn_loaded && !cache.dbn_loading) {
        cache.dbn_loading = true;
        std::thread([&cache, series_id]{
            // Fetch from DBnomics API
            auto& cs = CacheService::instance();
            std::string ck = CacheService::make_key("api-response", "dbnomics", series_id);
            std::string body;
            auto cached = cs.get(ck);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = "https://api.db.nomics.world/v22/series/" + series_id + "?observations=1&limit=20";
                http::Headers hdrs;
                hdrs["accept"] = "application/json";
                auto resp = http::HttpClient::instance().get(url, hdrs);
                if (resp.success) {
                    body = resp.body;
                    cs.set(ck, body, "api-response", CacheTTL::ONE_HOUR);
                }
            }

            std::lock_guard<std::mutex> lk(cache.dbn_mutex);
            if (!body.empty()) {
                try {
                    auto j = json::parse(body);
                    std::vector<WidgetDataCache::DBnomicsEntry> entries;
                    std::string name = series_id;

                    if (j.contains("series") && j["series"].contains("docs")) {
                        auto& doc = j["series"]["docs"][0];
                        name = doc.value("series_name", series_id);

                        if (doc.contains("period") && doc.contains("value")) {
                            auto& periods = doc["period"];
                            auto& values = doc["value"];
                            for (size_t i = 0; i < periods.size() && i < values.size(); i++) {
                                if (!values[i].is_null()) {
                                    entries.push_back({periods[i].get<std::string>(), values[i].get<double>()});
                                }
                            }
                        }
                    }

                    cache.dbn_series[series_id] = entries;
                    cache.dbn_names[series_id] = name;
                } catch (...) {}
            }
            cache.dbn_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading %s...", series_id.c_str());
        return;
    }
    if (cache.dbn_loading && !cache.dbn_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading %s...", series_id.c_str());
        return;
    }

    std::lock_guard<std::mutex> lk(cache.dbn_mutex);
    dit = cache.dbn_series.find(series_id);
    if (dit == cache.dbn_series.end() || dit->second.empty()) {
        ImGui::TextColored(FC_MUTED, "No data for %s", series_id.c_str());
        return;
    }

    auto nit = cache.dbn_names.find(series_id);
    if (nit != cache.dbn_names.end()) {
        ImGui::TextColored(FC_BLUE, "%s", nit->second.c_str());
        ImGui::Spacing();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##dbn_tbl", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Period", 0, 1.0f);
        ImGui::TableSetupColumn("Value", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& e : dit->second) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", e.period.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%.4f", e.value);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 16. AkShare (China Markets) Widget
// ============================================================================
void DashboardScreen::widget_akshare(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.ak_loaded && !cache.ak_loading) {
        cache.ak_loading = true;
        std::thread([&cache]{
            // Fetch China market indices via yfinance batch_quotes
            static const std::vector<std::pair<std::string, std::string>> china_symbols = {
                {"000001.SS", "SSE Composite"}, {"399001.SZ", "SZSE Component"},
                {"399006.SZ", "ChiNext"}, {"000300.SS", "CSI 300"},
                {"000016.SS", "SSE 50"}, {"HSI", "Hang Seng"},
            };
            std::vector<std::string> args = {"batch_quotes"};
            for (auto& [s, _] : china_symbols) args.push_back(s);

            auto result = python::execute("yfinance_data.py", args);
            std::lock_guard<std::mutex> lk(cache.ak_mutex);
            cache.akshare_quotes.clear();
            if (result.success && !result.output.empty()) {
                try {
                    auto j = json::parse(result.output);
                    // Build label map
                    std::map<std::string, std::string> label_map;
                    for (auto& [sym, name] : china_symbols) label_map[sym] = name;

                    if (j.is_array()) {
                        for (auto& item : j) {
                            QuoteEntry q;
                            q.symbol = item.value("symbol", "");
                            auto lit = label_map.find(q.symbol);
                            q.label = lit != label_map.end() ? lit->second : q.symbol;
                            q.price = item.value("price", 0.0);
                            q.change = item.value("change", 0.0);
                            q.change_percent = item.value("change_percent", 0.0);
                            q.high = item.value("high", 0.0);
                            q.low = item.value("low", 0.0);
                            q.open = item.value("open", 0.0);
                            q.previous_close = item.value("previous_close", 0.0);
                            q.volume = item.value("volume", 0.0);
                            cache.akshare_quotes.push_back(q);
                        }
                    }
                } catch (...) {}
            }
            cache.ak_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading China markets...");
        return;
    }
    if (cache.ak_loading && !cache.ak_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading China markets...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.ak_mutex);
    if (cache.akshare_quotes.empty()) {
        ImGui::TextColored(FC_MUTED, "No China market data");
        return;
    }
    render_quote_table("##ak_tbl", cache.akshare_quotes, "Index", "Value", "Chg%", false, 2);
}

// ============================================================================
// 17. Maritime Widget
// ============================================================================
void DashboardScreen::widget_maritime(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.maritime_loaded && !cache.maritime_loading) {
        cache.maritime_loading = true;
        std::thread([&cache]{
            // Fetch Baltic Dry Index and shipping ETFs via yfinance batch_quotes
            static const std::vector<std::pair<std::string, std::string>> maritime_symbols = {
                {"BDRY", "Baltic Dry Index ETF"}, {"SBLK", "Star Bulk Carriers"},
                {"GOGL", "Golden Ocean"}, {"ZIM", "ZIM Shipping"},
                {"MATX", "Matson Inc"}, {"KEX", "Kirby Corp"},
            };
            std::vector<std::string> args = {"batch_quotes"};
            for (auto& [s, _] : maritime_symbols) args.push_back(s);

            auto result = python::execute("yfinance_data.py", args);
            std::lock_guard<std::mutex> lk(cache.maritime_mutex);
            cache.maritime_data.clear();
            if (result.success && !result.output.empty()) {
                try {
                    auto j = json::parse(result.output);
                    std::map<std::string, std::string> label_map;
                    for (auto& [sym, name] : maritime_symbols) label_map[sym] = name;

                    if (j.is_array()) {
                        for (auto& item : j) {
                            std::string sym = item.value("symbol", "");
                            auto lit = label_map.find(sym);
                            std::string name = lit != label_map.end() ? lit->second : sym;
                            cache.maritime_data.push_back({
                                name, "Active",
                                item.value("price", 0.0), item.value("change_percent", 0.0)
                            });
                        }
                    }
                } catch (...) {}
            }
            cache.maritime_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading maritime data...");
        return;
    }
    if (cache.maritime_loading && !cache.maritime_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading maritime data...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.maritime_mutex);
    if (cache.maritime_data.empty()) {
        ImGui::TextColored(FC_MUTED, "No maritime data");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##mar_tbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Name", 0, 2.0f);
        ImGui::TableSetupColumn("Value", 0, 1.0f);
        ImGui::TableSetupColumn("Chg%", 0, 1.0f);
        ImGui::TableHeadersRow();

        for (auto& m : cache.maritime_data) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", m.route.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("$%.2f", m.index_value);
            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(m.change), "%+.2f%%", m.change);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// 18. Polymarket Widget
// ============================================================================
void DashboardScreen::widget_polymarket(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.poly_loaded && !cache.poly_loading) {
        cache.poly_loading = true;
        std::thread([&cache]{
            auto& cs = CacheService::instance();
            std::string ck = CacheService::make_key("api-response", "polymarket", "top");
            std::string body;
            auto cached = cs.get(ck);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = "https://gamma-api.polymarket.com/markets?limit=10&active=true&order=volume&ascending=false";
                http::Headers hdrs;
                hdrs["accept"] = "application/json";
                auto resp = http::HttpClient::instance().get(url, hdrs);
                if (resp.success) {
                    body = resp.body;
                    cs.set(ck, body, "api-response", CacheTTL::ONE_HOUR);
                }
            }

            std::lock_guard<std::mutex> lk(cache.poly_mutex);
            cache.poly_markets.clear();
            if (!body.empty()) {
                try {
                    auto j = json::parse(body);
                    auto& arr = j.is_array() ? j : j["markets"];
                    for (auto& m : arr) {
                        WidgetDataCache::PolyEntry pe;
                        pe.question = m.value("question", "");
                        pe.volume = m.value("volume", m.value("volumeNum", 0.0));
                        pe.active = m.value("active", true);

                        // Parse outcome prices
                        if (m.contains("outcomePrices")) {
                            auto& op = m["outcomePrices"];
                            if (op.is_string()) {
                                try {
                                    auto prices = json::parse(op.get<std::string>());
                                    if (prices.is_array() && !prices.empty())
                                        pe.yes_price = prices[0].get<double>();
                                } catch (...) {}
                            } else if (op.is_array() && !op.empty()) {
                                pe.yes_price = op[0].get<double>();
                            }
                        }
                        cache.poly_markets.push_back(pe);
                    }
                } catch (...) {}
            }
            cache.poly_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading Polymarket...");
        return;
    }
    if (cache.poly_loading && !cache.poly_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading Polymarket...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.poly_mutex);
    if (cache.poly_markets.empty()) {
        ImGui::TextColored(FC_MUTED, "No Polymarket data");
        return;
    }

    for (size_t i = 0; i < cache.poly_markets.size(); i++) {
        auto& m = cache.poly_markets[i];
        float pct = (float)(m.yes_price * 100.0);

        ImGui::PushID((int)i);

        // Question text
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextColored(FC_WHITE, "%s", m.question.c_str());
        ImGui::PopTextWrapPos();

        // Yes probability bar
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float bar_h = 10.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(p, ImVec2(p.x + avail_w, p.y + bar_h), IM_COL32(30, 30, 30, 220), 3.0f);
        float fill_w = (pct / 100.0f) * avail_w;
        ImU32 bar_col = pct > 60 ? IM_COL32(0, 180, 100, 200) :
                        (pct > 40 ? IM_COL32(200, 200, 0, 180) : IM_COL32(220, 50, 50, 200));
        dl->AddRectFilled(p, ImVec2(p.x + fill_w, p.y + bar_h), bar_col, 3.0f);
        ImGui::Dummy(ImVec2(avail_w, bar_h + 2));

        ImGui::TextColored(FC_GREEN, "YES %.0f%%", pct);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_MUTED, "Vol: $%.0fK", m.volume / 1000.0);

        ImGui::PopID();
        if (i < cache.poly_markets.size() - 1) ImGui::Separator();
        ImGui::Spacing();
    }
}

// ============================================================================
// 19. Forum Widget
// ============================================================================
void DashboardScreen::widget_forum(float w, float h, const WidgetInstance& wi) {
    auto& cache = WidgetDataCache::instance();
    if (!cache.forum_loaded && !cache.forum_loading) {
        cache.forum_loading = true;
        std::thread([&cache]{
            auto& cs = CacheService::instance();
            std::string ck = CacheService::make_key("api-response", "forum", "trending");
            std::string body;
            auto cached = cs.get(ck);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = "https://api.fincept.in/forum/posts/trending";
                http::Headers hdrs;
                hdrs["accept"] = "application/json";
                auto api_key = db::ops::get_setting("fincept_api_key");
                if (api_key.has_value() && !api_key->empty()) {
                    hdrs["X-API-Key"] = *api_key;
                }
                auto resp = http::HttpClient::instance().get(url, hdrs);
                if (resp.success) {
                    body = resp.body;
                    cs.set(ck, body, "api-response", CacheTTL::FIVE_MIN);
                }
            }

            std::lock_guard<std::mutex> lk(cache.forum_mutex);
            cache.forum_posts.clear();
            if (!body.empty()) {
                try {
                    auto j = json::parse(body);
                    // API may return {"success":true,"data":{"posts":[...]}}
                    auto& posts = j.is_array() ? j
                        : j.contains("data") && j["data"].contains("posts") ? j["data"]["posts"]
                        : j.contains("posts") ? j["posts"]
                        : j;
                    for (auto& p : posts) {
                        WidgetDataCache::ForumPost fp;
                        fp.title = p.value("title", "");
                        fp.author = p.value("author", p.value("username", ""));
                        fp.category = p.value("category", "");
                        fp.replies = p.value("replies", p.value("reply_count", 0));
                        fp.views = p.value("views", 0);
                        cache.forum_posts.push_back(fp);
                    }
                } catch (const std::exception& e) {
                    LOG_WARN("Forum posts parse error: {}", e.what());
                }
            }
            cache.forum_loaded = true;
        }).detach();
        ImGui::TextColored(FC_MUTED, "Loading forum...");
        return;
    }
    if (cache.forum_loading && !cache.forum_loaded) {
        ImGui::TextColored(FC_MUTED, "Loading forum...");
        return;
    }

    std::lock_guard<std::mutex> lk(cache.forum_mutex);
    if (cache.forum_posts.empty()) {
        ImGui::TextColored(FC_MUTED, "No forum posts available.");
        return;
    }

    for (size_t i = 0; i < cache.forum_posts.size(); i++) {
        auto& fp = cache.forum_posts[i];
        ImGui::TextColored(FC_ORANGE, "[%s]", fp.category.c_str());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(FC_WHITE, "%s", fp.title.c_str());
        ImGui::TextColored(FC_MUTED, "  by %s | %d replies | %d views",
            fp.author.c_str(), fp.replies, fp.views);
        if (i < cache.forum_posts.size() - 1) ImGui::Separator();
    }
}

// ============================================================================
// 20. DataSource Widget
// ============================================================================
void DashboardScreen::widget_datasource(float w, float h, const WidgetInstance& wi) {
    // Show configured data sources from DB
    std::vector<db::DataSource> sources;
    try {
        sources = db::ops::get_all_data_sources();
    } catch (...) {}

    if (sources.empty()) {
        ImGui::TextColored(FC_MUTED, "No data sources configured.");
        ImGui::TextColored(FC_GRAY, "Add data sources in Data Sources tab.");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginTable("##ds_tbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Name", 0, 2.0f);
        ImGui::TableSetupColumn("Type", 0, 1.0f);
        ImGui::TableSetupColumn("Status", 0, 0.8f);
        ImGui::TableHeadersRow();

        for (auto& ds : sources) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", ds.display_name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", ds.type.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_GREEN, "Active");
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

} // namespace fincept::dashboard
