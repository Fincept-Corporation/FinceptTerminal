#include "polymarket_screen.h"
#include "ui/theme.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <future>

static const char* TAG = "Polymarket";

namespace fincept::polymarket {

using json = nlohmann::json;

// Chart interval definitions
struct IntervalDef {
    const char* label;
    const char* api_param;
    int         fidelity;     // minutes
};

static constexpr IntervalDef INTERVALS[] = {
    {"1H",  "1h",  1},
    {"6H",  "6h",  5},
    {"1D",  "1d",  60},
    {"1W",  "1w",  360},
    {"1M",  "1m",  1440},
    {"ALL", "max", 1440},
};
static constexpr int NUM_INTERVALS = sizeof(INTERVALS) / sizeof(INTERVALS[0]);

// ============================================================================
// Helpers
// ============================================================================

std::string PolymarketScreen::format_volume(double vol) {
    char buf[32];
    if (vol >= 1'000'000.0)
        std::snprintf(buf, sizeof(buf), "$%.1fM", vol / 1'000'000.0);
    else if (vol >= 1'000.0)
        std::snprintf(buf, sizeof(buf), "$%.1fK", vol / 1'000.0);
    else
        std::snprintf(buf, sizeof(buf), "$%.0f", vol);
    return buf;
}

std::string PolymarketScreen::format_price(double price) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", price * 100.0);
    return buf;
}

std::string PolymarketScreen::format_percent(double pct) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%+.1f%%", pct * 100.0);
    return buf;
}

std::string PolymarketScreen::time_ago(const std::string& date_str) {
    if (date_str.empty()) return "\xe2\x80\x94";
    if (date_str.size() >= 10) return date_str.substr(0, 10);
    return date_str;
}

// ============================================================================
// Init
// ============================================================================

void PolymarketScreen::init() {
    if (initialized_) return;
    initialized_ = true;
    LOG_INFO(TAG, "Initializing Polymarket screen");

    // Load tags
    std::string url = std::string(GAMMA_API_BASE) + "/tags";
    pending_tags_ = http::HttpClient::instance().get_async(url);

    load_markets();
}

// ============================================================================
// Data loading — uses get_async() which returns future<HttpResponse>
// ============================================================================

void PolymarketScreen::load_markets() {
    loading_list_ = true;
    error_msg_.clear();

    std::string url = std::string(GAMMA_API_BASE) + "/markets";
    url += "?limit=" + std::to_string(PAGE_SIZE);
    url += "&offset=" + std::to_string(current_page_ * PAGE_SIZE);

    if (!show_closed_) {
        url += "&closed=false&active=true";
    }
    if (tag_filter_[0] != '\0') {
        url += "&tag_id=";
        url += tag_filter_;
    }
    if (sort_by_ == SortBy::volume)        url += "&order=volume&ascending=false";
    else if (sort_by_ == SortBy::liquidity) url += "&order=liquidity&ascending=false";
    else                                    url += "&order=id&ascending=false";

    pending_list_ = http::HttpClient::instance().get_async(url);
}

void PolymarketScreen::load_events() {
    loading_list_ = true;
    error_msg_.clear();

    std::string url = std::string(GAMMA_API_BASE) + "/events";
    url += "?limit=" + std::to_string(PAGE_SIZE);
    url += "&offset=" + std::to_string(current_page_ * PAGE_SIZE);
    if (!show_closed_) url += "&closed=false&active=true";
    url += "&order=id&ascending=false";

    pending_list_ = http::HttpClient::instance().get_async(url);
}

void PolymarketScreen::load_sports() {
    loading_list_ = true;
    error_msg_.clear();

    std::string url = std::string(GAMMA_API_BASE) + "/sports";
    pending_list_ = http::HttpClient::instance().get_async(url);
}

void PolymarketScreen::load_resolved() {
    loading_list_ = true;
    error_msg_.clear();

    std::string url = std::string(GAMMA_API_BASE) + "/events";
    url += "?limit=" + std::to_string(PAGE_SIZE);
    url += "&offset=" + std::to_string(current_page_ * PAGE_SIZE);
    url += "&closed=true&active=false&order=id&ascending=false";

    pending_list_ = http::HttpClient::instance().get_async(url);
}

void PolymarketScreen::load_market_detail(int idx) {
    if (idx < 0 || idx >= static_cast<int>(markets_.size())) return;

    selected_market_idx_ = idx;
    const auto& m = markets_[idx];
    loading_detail_ = true;
    loading_trades_ = true;
    loading_book_   = true;
    loading_chart_  = true;
    recent_trades_.clear();
    yes_price_history_.clear();
    no_price_history_.clear();
    yes_order_book_ = {};
    no_order_book_  = {};
    top_holders_.clear();

    auto& http = http::HttpClient::instance();

    // Load recent trades
    if (!m.clob_token_ids.empty()) {
        std::string url = "https://clob.polymarket.com/trades?token_id="
            + m.clob_token_ids[0] + "&limit=50";
        pending_trades_ = http.get_async(url);
    } else {
        loading_trades_ = false;
    }

    // Load order books
    if (!m.clob_token_ids.empty()) {
        std::string url_yes = "https://clob.polymarket.com/book?token_id="
            + m.clob_token_ids[0];
        pending_book_yes_ = http.get_async(url_yes);

        if (m.clob_token_ids.size() > 1) {
            std::string url_no = "https://clob.polymarket.com/book?token_id="
                + m.clob_token_ids[1];
            pending_book_no_ = http.get_async(url_no);
        }
    } else {
        loading_book_ = false;
    }

    // Load price history
    if (!m.clob_token_ids.empty()) {
        const auto& iv = INTERVALS[chart_interval_idx_];
        std::string url_h = "https://clob.polymarket.com/prices-history?market="
            + m.clob_token_ids[0]
            + "&interval=" + iv.api_param
            + "&fidelity=" + std::to_string(iv.fidelity);
        pending_history_yes_ = http.get_async(url_h);

        if (m.clob_token_ids.size() > 1) {
            std::string url_hn = "https://clob.polymarket.com/prices-history?market="
                + m.clob_token_ids[1]
                + "&interval=" + iv.api_param
                + "&fidelity=" + std::to_string(iv.fidelity);
            pending_history_no_ = http.get_async(url_hn);
        }
    } else {
        loading_chart_ = false;
    }

    loading_detail_ = false;
}

void PolymarketScreen::search_markets() {
    std::string q(search_buf_);
    if (q.empty()) { load_markets(); return; }

    loading_list_ = true;
    error_msg_.clear();

    std::string url = std::string(GAMMA_API_BASE) + "/markets?_q=" + q + "&limit=20";
    pending_list_ = http::HttpClient::instance().get_async(url);
}

void PolymarketScreen::refresh_data() {
    current_page_ = 0;
    switch (view_mode_) {
        case ViewMode::markets:  load_markets(); break;
        case ViewMode::events:   load_events();  break;
        case ViewMode::sports:   load_sports();  break;
        case ViewMode::resolved: load_resolved(); break;
    }
}

// ============================================================================
// JSON parsing — all take the response body string
// ============================================================================

void PolymarketScreen::parse_markets(const std::string& json_str) {
    try {
        json arr = json::parse(json_str);
        if (!arr.is_array()) return;

        std::lock_guard<std::mutex> lock(data_mutex_);
        markets_.clear();
        markets_.reserve(arr.size());

        for (const auto& j : arr) {
            Market m;
            m.id          = j.value("id", "");
            m.question    = j.value("question", "");
            m.description = j.value("description", "");
            m.slug        = j.value("slug", "");
            m.category    = j.value("category", "");
            m.end_date    = j.value("endDate", "");
            m.condition_id = j.value("conditionId", "");
            m.active      = j.value("active", true);
            m.closed      = j.value("closed", false);
            m.archived    = j.value("archived", false);
            m.featured    = j.value("featured", false);

            if (j.contains("volume")) {
                if (j["volume"].is_string()) {
                    try { m.volume = std::stod(j["volume"].get<std::string>()); } catch (...) {}
                } else if (j["volume"].is_number()) {
                    m.volume = j["volume"].get<double>();
                }
            }
            m.volume_24h = j.value("volume24hr", 0.0);
            if (j.contains("liquidity")) {
                if (j["liquidity"].is_string()) {
                    try { m.liquidity = std::stod(j["liquidity"].get<std::string>()); } catch (...) {}
                } else if (j["liquidity"].is_number()) {
                    m.liquidity = j["liquidity"].get<double>();
                }
            }

            m.spread           = j.value("spread", 0.0);
            m.one_day_change   = j.value("oneDayPriceChange", 0.0);
            m.one_week_change  = j.value("oneWeekPriceChange", 0.0);
            m.one_month_change = j.value("oneMonthPriceChange", 0.0);

            if (j.contains("outcomes") && j["outcomes"].is_array()) {
                for (const auto& o : j["outcomes"])
                    m.outcomes.push_back(o.get<std::string>());
            }
            if (j.contains("outcomePrices") && j["outcomePrices"].is_array()) {
                for (const auto& p : j["outcomePrices"]) {
                    try { m.outcome_prices.push_back(std::stod(p.get<std::string>())); }
                    catch (...) { m.outcome_prices.push_back(0.0); }
                }
            }

            if (j.contains("clobTokenIds")) {
                const auto& tok = j["clobTokenIds"];
                if (tok.is_array()) {
                    for (const auto& t : tok)
                        m.clob_token_ids.push_back(t.get<std::string>());
                } else if (tok.is_string()) {
                    try {
                        json tok_arr = json::parse(tok.get<std::string>());
                        if (tok_arr.is_array()) {
                            for (const auto& t : tok_arr)
                                m.clob_token_ids.push_back(t.get<std::string>());
                        }
                    } catch (...) {}
                }
            }

            markets_.push_back(std::move(m));
        }
    } catch (const std::exception& e) {
        LOG_INFO(TAG, "parse_markets error: %s", e.what());
    }
}

void PolymarketScreen::parse_events(const std::string& json_str) {
    try {
        json arr = json::parse(json_str);
        if (!arr.is_array()) return;

        auto& target = (view_mode_ == ViewMode::resolved) ? resolved_events_ : events_;

        std::lock_guard<std::mutex> lock(data_mutex_);
        target.clear();
        target.reserve(arr.size());

        for (const auto& j : arr) {
            Event ev;
            ev.id          = j.value("id", "");
            ev.slug        = j.value("slug", "");
            ev.title       = j.value("title", "");
            ev.description = j.value("description", "");
            ev.start_date  = j.value("startDate", "");
            ev.end_date    = j.value("endDate", "");
            ev.active      = j.value("active", true);
            ev.closed      = j.value("closed", false);
            ev.archived    = j.value("archived", false);
            ev.comment_count = j.value("commentCount", 0);

            if (j.contains("volume")) {
                if (j["volume"].is_string()) {
                    try { ev.volume = std::stod(j["volume"].get<std::string>()); } catch (...) {}
                } else if (j["volume"].is_number()) {
                    ev.volume = j["volume"].get<double>();
                }
            }

            if (j.contains("markets") && j["markets"].is_array()) {
                for (const auto& mj : j["markets"]) {
                    Market m;
                    m.id       = mj.value("id", "");
                    m.question = mj.value("question", mj.value("groupItemTitle", ""));
                    m.active   = mj.value("active", true);
                    m.closed   = mj.value("closed", false);

                    if (mj.contains("outcomePrices") && mj["outcomePrices"].is_array()) {
                        for (const auto& p : mj["outcomePrices"]) {
                            try { m.outcome_prices.push_back(std::stod(p.get<std::string>())); }
                            catch (...) { m.outcome_prices.push_back(0.0); }
                        }
                    }
                    if (mj.contains("outcomes") && mj["outcomes"].is_array()) {
                        for (const auto& o : mj["outcomes"])
                            m.outcomes.push_back(o.get<std::string>());
                    }
                    ev.markets.push_back(std::move(m));
                }
            }
            target.push_back(std::move(ev));
        }
    } catch (const std::exception& e) {
        LOG_INFO(TAG, "parse_events error: %s", e.what());
    }
}

void PolymarketScreen::parse_tags(const std::string& json_str) {
    try {
        json arr = json::parse(json_str);
        if (!arr.is_array()) return;
        std::lock_guard<std::mutex> lock(data_mutex_);
        tags_.clear();
        for (const auto& j : arr) {
            Tag t;
            t.id    = j.value("id", "");
            t.label = j.value("label", "");
            t.slug  = j.value("slug", "");
            tags_.push_back(std::move(t));
        }
    } catch (...) {}
}

void PolymarketScreen::parse_sports(const std::string& json_str) {
    try {
        json arr = json::parse(json_str);
        if (!arr.is_array()) return;
        std::lock_guard<std::mutex> lock(data_mutex_);
        sports_.clear();
        for (const auto& j : arr) {
            Sport s;
            s.id     = j.value("id", 0);
            s.sport  = j.value("sport", "");
            s.image  = j.value("image", "");
            s.tags   = j.value("tags", "");
            s.series = j.value("series", "");
            sports_.push_back(std::move(s));
        }
    } catch (...) {}
}

void PolymarketScreen::parse_trades(const std::string& json_str) {
    try {
        json data = json::parse(json_str);
        json arr = data.is_array() ? data :
                   (data.contains("data") ? data["data"] : json::array());
        if (!arr.is_array()) return;

        recent_trades_.clear();
        for (const auto& j : arr) {
            Trade t;
            t.id       = j.value("id", "");
            t.market   = j.value("market", "");
            t.asset_id = j.value("asset_id", "");
            t.side     = j.value("side", "");
            if (j.contains("price")) {
                if (j["price"].is_string()) {
                    try { t.price = std::stod(j["price"].get<std::string>()); } catch (...) {}
                } else { t.price = j.value("price", 0.0); }
            }
            if (j.contains("size")) {
                if (j["size"].is_string()) {
                    try { t.size = std::stod(j["size"].get<std::string>()); } catch (...) {}
                } else { t.size = j.value("size", 0.0); }
            }
            t.timestamp = j.value("timestamp", static_cast<int64_t>(0));
            recent_trades_.push_back(std::move(t));
        }
    } catch (...) {}
}

void PolymarketScreen::parse_order_book(const std::string& json_str, bool is_yes) {
    try {
        json j = json::parse(json_str);
        OrderBook ob;
        ob.asset_id = j.value("asset_id", "");

        if (j.contains("last_trade_price")) {
            if (j["last_trade_price"].is_string()) {
                try { ob.last_trade_price = std::stod(j["last_trade_price"].get<std::string>()); } catch (...) {}
            } else { ob.last_trade_price = j.value("last_trade_price", 0.0); }
        }

        auto parse_levels = [](const json& levels) {
            std::vector<OrderLevel> result;
            if (!levels.is_array()) return result;
            for (const auto& lv : levels) {
                OrderLevel ol;
                if (lv.contains("price")) {
                    if (lv["price"].is_string()) {
                        try { ol.price = std::stod(lv["price"].get<std::string>()); } catch (...) {}
                    } else { ol.price = lv.value("price", 0.0); }
                }
                if (lv.contains("size")) {
                    if (lv["size"].is_string()) {
                        try { ol.size = std::stod(lv["size"].get<std::string>()); } catch (...) {}
                    } else { ol.size = lv.value("size", 0.0); }
                }
                result.push_back(ol);
            }
            return result;
        };

        if (j.contains("bids")) ob.bids = parse_levels(j["bids"]);
        if (j.contains("asks")) ob.asks = parse_levels(j["asks"]);

        std::sort(ob.bids.begin(), ob.bids.end(),
            [](const OrderLevel& a, const OrderLevel& b) { return a.price > b.price; });
        std::sort(ob.asks.begin(), ob.asks.end(),
            [](const OrderLevel& a, const OrderLevel& b) { return a.price < b.price; });

        if (is_yes) yes_order_book_ = std::move(ob);
        else        no_order_book_  = std::move(ob);
    } catch (...) {}
}

void PolymarketScreen::parse_price_history(const std::string& json_str, bool is_yes) {
    try {
        json j = json::parse(json_str);
        json prices = j.contains("history") ? j["history"] :
                      (j.contains("prices") ? j["prices"] : j);
        if (!prices.is_array()) return;

        auto& target = is_yes ? yes_price_history_ : no_price_history_;
        target.clear();
        target.reserve(prices.size());

        for (const auto& p : prices) {
            PricePoint pp;
            if (p.contains("p"))          pp.price = p["p"].get<double>();
            else if (p.contains("price")) pp.price = p["price"].get<double>();
            if (p.contains("t"))              pp.timestamp = p["t"].get<int64_t>();
            else if (p.contains("timestamp")) pp.timestamp = p["timestamp"].get<int64_t>();
            target.push_back(pp);
        }
    } catch (...) {}
}

// ============================================================================
// Main render
// ============================================================================

void PolymarketScreen::render() {
    init();
    using namespace theme::colors;

    // Poll async results — extract .body from HttpResponse
    if (pending_tags_.valid() &&
        pending_tags_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_tags_.get();
        if (resp.success) parse_tags(resp.body);
    }

    if (pending_list_.valid() &&
        pending_list_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_list_.get();
        loading_list_ = false;
        if (resp.success) {
            if (view_mode_ == ViewMode::markets)       parse_markets(resp.body);
            else if (view_mode_ == ViewMode::events)   parse_events(resp.body);
            else if (view_mode_ == ViewMode::sports)   parse_sports(resp.body);
            else if (view_mode_ == ViewMode::resolved) parse_events(resp.body);
        } else {
            error_msg_ = resp.error.empty() ? "Request failed" : resp.error;
        }
    }

    if (pending_trades_.valid() &&
        pending_trades_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_trades_.get();
        if (resp.success) parse_trades(resp.body);
        loading_trades_ = false;
    }

    if (pending_book_yes_.valid() &&
        pending_book_yes_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_book_yes_.get();
        if (resp.success) parse_order_book(resp.body, true);
        if (!pending_book_no_.valid()) loading_book_ = false;
    }
    if (pending_book_no_.valid() &&
        pending_book_no_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_book_no_.get();
        if (resp.success) parse_order_book(resp.body, false);
        loading_book_ = false;
    }

    if (pending_history_yes_.valid() &&
        pending_history_yes_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_history_yes_.get();
        if (resp.success) parse_price_history(resp.body, true);
        if (!pending_history_no_.valid()) loading_chart_ = false;
    }
    if (pending_history_no_.valid() &&
        pending_history_no_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        http::HttpResponse resp = pending_history_no_.get();
        if (resp.success) parse_price_history(resp.body, false);
        loading_chart_ = false;
    }

    // Layout
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();
    float y = vp->WorkPos.y + tab_h;
    float w = vp->WorkSize.x;
    float h = vp->WorkSize.y - tab_h - footer_h;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARKEST);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##polymarket", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking);

    float avail_w = ImGui::GetContentRegionAvail().x;

    render_top_nav(avail_w);

    if (view_mode_ != ViewMode::sports) {
        render_search_bar(avail_w);
    }

    float remain_h = ImGui::GetContentRegionAvail().y;

    render_market_list(LIST_PANEL_WIDTH, remain_h);
    ImGui::SameLine(0, 0);
    render_detail_panel(avail_w - LIST_PANEL_WIDTH, remain_h);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Top nav
// ============================================================================

void PolymarketScreen::render_top_nav(float w) {
    using namespace theme::colors;

    // Title row
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##pm_title", ImVec2(w, 28), false);
    ImGui::SetCursorPos(ImVec2(12, 6));
    ImGui::TextColored(ACCENT, "POLYMARKET");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "PREDICTION MARKETS");

    float btn_x = w - 90;
    ImGui::SameLine(btn_x);
    ImGui::PushStyleColor(ImGuiCol_Button, loading_list_ ? BG_WIDGET : ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ACCENT.x * 1.2f, ACCENT.y * 1.2f, ACCENT.z * 1.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, loading_list_ ? TEXT_DIM : BG_DARKEST);
    if (ImGui::SmallButton("REFRESH") && !loading_list_) {
        refresh_data();
    }
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // View tabs
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##pm_tabs", ImVec2(w, 26), false);

    struct TabDef { const char* label; ViewMode mode; };
    static constexpr TabDef view_tabs[] = {
        {"MARKETS",  ViewMode::markets},
        {"EVENTS",   ViewMode::events},
        {"SPORTS",   ViewMode::sports},
        {"RESOLVED", ViewMode::resolved},
    };

    ImGui::SetCursorPos(ImVec2(12, 3));
    for (int i = 0; i < 4; i++) {
        bool is_active = (view_mode_ == view_tabs[i].mode);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text,
                view_tabs[i].mode == ViewMode::resolved ? MARKET_GREEN : TEXT_DIM);
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_BG);

        if (ImGui::SmallButton(view_tabs[i].label)) {
            view_mode_ = view_tabs[i].mode;
            current_page_ = 0;
            selected_market_idx_ = -1;
            selected_event_idx_  = -1;
            selected_sport_idx_  = -1;

            switch (view_mode_) {
                case ViewMode::markets:  load_markets(); break;
                case ViewMode::events:   load_events(); break;
                case ViewMode::sports:   load_sports(); break;
                case ViewMode::resolved: load_resolved(); break;
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Search bar
// ============================================================================

void PolymarketScreen::render_search_bar(float w) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##pm_search", ImVec2(w, 30), false);
    ImGui::SetCursorPos(ImVec2(12, 5));

    ImGui::PushItemWidth(w * 0.4f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    bool enter = ImGui::InputTextWithHint("##pm_q", "SEARCH MARKETS...",
        search_buf_, sizeof(search_buf_), ImGuiInputTextFlags_EnterReturnsTrue);
    if (enter) search_markets();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    // Sort
    ImGui::SameLine(0, 8);
    ImGui::PushItemWidth(60);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    const char* sort_labels[] = {"VOL", "LIQ", "RECENT"};
    int sort_idx = static_cast<int>(sort_by_);
    if (ImGui::Combo("##pm_sort", &sort_idx, sort_labels, 3)) {
        sort_by_ = static_cast<SortBy>(sort_idx);
        current_page_ = 0;
        refresh_data();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    // Closed toggle
    ImGui::SameLine(0, 8);
    if (show_closed_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    }
    if (ImGui::SmallButton("CLOSED")) {
        show_closed_ = !show_closed_;
        current_page_ = 0;
        refresh_data();
    }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Market list panel (left)
// ============================================================================

void PolymarketScreen::render_market_list(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##pm_list", ImVec2(w, h), true);

    // Header
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
        ImGui::BeginChild("##pm_list_hdr", ImVec2(w - 2, 22), false);
        ImGui::SetCursorPos(ImVec2(8, 3));

        const char* header_text = "MARKETS";
        int count = 0;
        ImVec4 hdr_color = ACCENT;

        switch (view_mode_) {
            case ViewMode::markets:  header_text = "MARKETS";  count = static_cast<int>(markets_.size()); break;
            case ViewMode::events:   header_text = "EVENTS";   count = static_cast<int>(events_.size()); break;
            case ViewMode::sports:   header_text = "SPORTS";   count = static_cast<int>(sports_.size()); break;
            case ViewMode::resolved: header_text = "RESOLVED"; count = static_cast<int>(resolved_events_.size()); hdr_color = MARKET_GREEN; break;
        }
        ImGui::TextColored(hdr_color, "%s (%d)", header_text, count);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    if (loading_list_) {
        float cy = h * 0.4f;
        ImGui::SetCursorPos(ImVec2(w * 0.3f, cy));
        ImGui::TextColored(TEXT_DIM, "Loading...");
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        return;
    }

    if (!error_msg_.empty()) {
        ImGui::SetCursorPos(ImVec2(8, 30));
        ImGui::TextColored(ERROR_RED, "%s", error_msg_.c_str());
    }

    float item_h = 52.0f;

    if (view_mode_ == ViewMode::markets) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (int i = 0; i < static_cast<int>(markets_.size()); i++) {
            const auto& m = markets_[i];
            bool selected = (i == selected_market_idx_);

            ImGui::PushID(i);
            ImVec2 pos = ImGui::GetCursorScreenPos();

            if (selected) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + item_h),
                    ImGui::ColorConvertFloat4ToU32(ACCENT_BG));
            }

            if (ImGui::InvisibleButton("##mkt", ImVec2(w - 4, item_h))) {
                load_market_detail(i);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + item_h),
                    ImGui::ColorConvertFloat4ToU32(BG_HOVER));
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 4));
            std::string q_trunc = m.question;
            if (q_trunc.size() > 50) q_trunc = q_trunc.substr(0, 47) + "...";
            ImGui::TextColored(selected ? ACCENT : TEXT_PRIMARY, "%s", q_trunc.c_str());

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 24));
            if (!m.outcome_prices.empty()) {
                double yes_price = m.outcome_prices[0];
                ImGui::TextColored(yes_price >= 0.5 ? MARKET_GREEN : MARKET_RED,
                    "YES %s", format_price(yes_price).c_str());
                ImGui::SameLine(0, 12);
            }
            ImGui::TextColored(TEXT_DIM, "Vol %s", format_volume(m.volume).c_str());

            if (std::abs(m.one_day_change) > 0.0001) {
                ImGui::SameLine(0, 12);
                ImGui::TextColored(m.one_day_change > 0 ? MARKET_GREEN : MARKET_RED,
                    "%s", format_percent(m.one_day_change).c_str());
            }

            if (!m.category.empty()) {
                ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 38));
                ImGui::TextColored(TEXT_DIM, "%s", m.category.c_str());
            }

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x + 4, pos.y + item_h - 1),
                ImVec2(pos.x + w - 8, pos.y + item_h - 1),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM));

            ImGui::PopID();
        }
    } else if (view_mode_ == ViewMode::events || view_mode_ == ViewMode::resolved) {
        const auto& evts = (view_mode_ == ViewMode::resolved) ? resolved_events_ : events_;
        std::lock_guard<std::mutex> lock(data_mutex_);

        for (int i = 0; i < static_cast<int>(evts.size()); i++) {
            const auto& ev = evts[i];
            bool selected = (i == selected_event_idx_);

            ImGui::PushID(i + 10000);
            ImVec2 pos = ImGui::GetCursorScreenPos();

            if (selected) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + item_h),
                    ImGui::ColorConvertFloat4ToU32(ACCENT_BG));
            }

            if (ImGui::InvisibleButton("##evt", ImVec2(w - 4, item_h))) {
                selected_event_idx_ = i;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + item_h),
                    ImGui::ColorConvertFloat4ToU32(BG_HOVER));
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 4));
            std::string title = ev.title;
            if (title.size() > 50) title = title.substr(0, 47) + "...";
            ImVec4 tc = view_mode_ == ViewMode::resolved ?
                (ev.closed ? MARKET_GREEN : TEXT_PRIMARY) :
                (selected ? ACCENT : TEXT_PRIMARY);
            ImGui::TextColored(tc, "%s", title.c_str());

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 24));
            ImGui::TextColored(TEXT_DIM, "%d markets", static_cast<int>(ev.markets.size()));
            ImGui::SameLine(0, 12);
            ImGui::TextColored(TEXT_DIM, "Vol %s", format_volume(ev.volume).c_str());

            if (view_mode_ == ViewMode::resolved && ev.closed) {
                ImGui::SameLine(0, 12);
                ImGui::TextColored(MARKET_GREEN, "RESOLVED");
            }

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x + 4, pos.y + item_h - 1),
                ImVec2(pos.x + w - 8, pos.y + item_h - 1),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM));

            ImGui::PopID();
        }
    } else if (view_mode_ == ViewMode::sports) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        float sport_h = 36.0f;

        for (int i = 0; i < static_cast<int>(sports_.size()); i++) {
            const auto& sp = sports_[i];
            bool selected = (i == selected_sport_idx_);

            ImGui::PushID(i + 20000);
            ImVec2 pos = ImGui::GetCursorScreenPos();

            if (selected) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + sport_h),
                    ImGui::ColorConvertFloat4ToU32(ACCENT_BG));
            }

            if (ImGui::InvisibleButton("##spt", ImVec2(w - 4, sport_h))) {
                selected_sport_idx_ = i;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + w - 4, pos.y + sport_h),
                    ImGui::ColorConvertFloat4ToU32(BG_HOVER));
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + 6));
            ImGui::TextColored(selected ? ACCENT : TEXT_PRIMARY, "%s", sp.sport.c_str());

            if (!sp.series.empty()) {
                ImGui::SameLine(0, 12);
                ImGui::TextColored(TEXT_DIM, "%s", sp.series.c_str());
            }

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x + 4, pos.y + sport_h - 1),
                ImVec2(pos.x + w - 8, pos.y + sport_h - 1),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM));

            ImGui::PopID();
        }
    }

    // Pagination
    bool has_full_page = (view_mode_ == ViewMode::markets && static_cast<int>(markets_.size()) >= PAGE_SIZE) ||
                         (view_mode_ == ViewMode::events && static_cast<int>(events_.size()) >= PAGE_SIZE) ||
                         (view_mode_ == ViewMode::resolved && static_cast<int>(resolved_events_.size()) >= PAGE_SIZE);

    if (has_full_page || current_page_ > 0) {
        ImGui::Spacing();
        ImGui::Separator();

        float btn_w = (w - 80) / 2;
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, current_page_ == 0 ? TEXT_DIM : TEXT_PRIMARY);
        if (ImGui::Button("PREV", ImVec2(btn_w, 22)) && current_page_ > 0) {
            current_page_--;
            refresh_data();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        char pg[8];
        std::snprintf(pg, sizeof(pg), "%d", current_page_ + 1);
        ImGui::Button(pg, ImVec2(40, 22));
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        if (ImGui::Button("NEXT", ImVec2(btn_w, 22))) {
            current_page_++;
            refresh_data();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Detail panel (right)
// ============================================================================

void PolymarketScreen::render_detail_panel(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##pm_detail", ImVec2(w, h), false);

    if (view_mode_ == ViewMode::markets && selected_market_idx_ >= 0 &&
        selected_market_idx_ < static_cast<int>(markets_.size())) {
        render_market_detail(w, h);
    } else if ((view_mode_ == ViewMode::events || view_mode_ == ViewMode::resolved) &&
               selected_event_idx_ >= 0) {
        render_event_detail(w, h);
    } else {
        render_no_selection(w, h);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void PolymarketScreen::render_no_selection(float w, float h) {
    using namespace theme::colors;
    ImGui::SetCursorPos(ImVec2(w * 0.25f, h * 0.4f));
    ImGui::TextColored(TEXT_DIM, "Select a market to view details");
    ImGui::SetCursorPos(ImVec2(w * 0.2f, h * 0.4f + 24));
    ImGui::TextColored(ACCENT_DIM, "Price chart, order book, trades, and more");
}

// ============================================================================
// Market detail
// ============================================================================

void PolymarketScreen::render_market_detail(float w, float h) {
    using namespace theme::colors;
    const auto& m = markets_[selected_market_idx_];

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##pm_mkt_hdr", ImVec2(w, 70), false);

    ImGui::SetCursorPos(ImVec2(12, 6));
    ImGui::PushTextWrapPos(w - 20);
    ImGui::TextColored(ACCENT, "%s", m.question.c_str());
    ImGui::PopTextWrapPos();

    ImGui::SetCursorPos(ImVec2(12, 36));
    for (int i = 0; i < static_cast<int>(m.outcomes.size()) && i < static_cast<int>(m.outcome_prices.size()); i++) {
        if (i > 0) ImGui::SameLine(0, 16);
        ImVec4 color = (i == 0) ?
            (m.outcome_prices[i] >= 0.5 ? MARKET_GREEN : MARKET_RED) :
            (m.outcome_prices[i] >= 0.5 ? MARKET_RED : MARKET_GREEN);
        ImGui::TextColored(color, "%s %s",
            m.outcomes[i].c_str(), format_price(m.outcome_prices[i]).c_str());
    }

    ImGui::SameLine(0, 24);
    ImGui::TextColored(TEXT_DIM, "Vol %s", format_volume(m.volume).c_str());
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Liq %s", format_volume(m.liquidity).c_str());

    if (std::abs(m.one_day_change) > 0.0001) {
        ImGui::SameLine(0, 16);
        ImGui::TextColored(m.one_day_change > 0 ? MARKET_GREEN : MARKET_RED,
            "24h %s", format_percent(m.one_day_change).c_str());
    }

    float badge_x = w - 80;
    ImGui::SetCursorPos(ImVec2(badge_x, 8));
    if (m.closed)       ImGui::TextColored(MARKET_RED, "[CLOSED]");
    else if (m.active)  ImGui::TextColored(MARKET_GREEN, "[ACTIVE]");

    ImGui::EndChild();
    ImGui::PopStyleColor();

    float remain_h = h - 70;
    render_price_chart(w, remain_h * 0.4f);

    float panel_w = w / 3;
    float panel_h = remain_h * 0.6f;
    render_order_book(panel_w, panel_h);
    ImGui::SameLine(0, 0);
    render_recent_trades(panel_w, panel_h);
    ImGui::SameLine(0, 0);
    render_top_holders(panel_w, panel_h);
}

// ============================================================================
// Price chart
// ============================================================================

void PolymarketScreen::render_price_chart(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##pm_chart", ImVec2(w, h), true);

    ImGui::SetCursorPos(ImVec2(8, 4));
    ImGui::TextColored(ACCENT, "PRICE HISTORY");

    for (int i = 0; i < NUM_INTERVALS; i++) {
        ImGui::SameLine(0, 4);
        bool active = (i == chart_interval_idx_);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? BG_DARKEST : TEXT_DIM);
        if (ImGui::SmallButton(INTERVALS[i].label)) {
            chart_interval_idx_ = i;
            if (selected_market_idx_ >= 0) load_market_detail(selected_market_idx_);
        }
        ImGui::PopStyleColor(2);
    }

    float chart_y = 28.0f;
    float chart_h = h - chart_y - 4;
    float chart_w = w - 16;

    if (loading_chart_) {
        ImGui::SetCursorPos(ImVec2(w * 0.35f, chart_y + chart_h * 0.4f));
        ImGui::TextColored(TEXT_DIM, "Loading chart...");
    } else if (yes_price_history_.empty()) {
        ImGui::SetCursorPos(ImVec2(w * 0.3f, chart_y + chart_h * 0.4f));
        ImGui::TextColored(TEXT_DIM, "No price history available");
    } else {
        ImVec2 origin = ImGui::GetCursorScreenPos();
        origin.x += 8;
        origin.y = ImGui::GetWindowPos().y + chart_y;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(origin,
            ImVec2(origin.x + chart_w, origin.y + chart_h),
            ImGui::ColorConvertFloat4ToU32(BG_DARKEST));

        for (int i = 0; i <= 4; i++) {
            float gy = origin.y + (chart_h * i / 4.0f);
            dl->AddLine(ImVec2(origin.x, gy), ImVec2(origin.x + chart_w, gy),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM), 0.5f);
            char lbl[8];
            std::snprintf(lbl, sizeof(lbl), "%d%%", 100 - i * 25);
            dl->AddText(ImVec2(origin.x + 2, gy + 1), ImGui::ColorConvertFloat4ToU32(TEXT_DIM), lbl);
        }

        auto draw_line = [&](const std::vector<PricePoint>& pts, ImVec4 color) {
            if (pts.size() < 2) return;
            ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
            for (size_t pi = 1; pi < pts.size(); pi++) {
                float x1 = origin.x + (chart_w * static_cast<float>(pi - 1) / static_cast<float>(pts.size() - 1));
                float y1 = origin.y + chart_h * (1.0f - static_cast<float>(pts[pi-1].price));
                float x2 = origin.x + (chart_w * static_cast<float>(pi) / static_cast<float>(pts.size() - 1));
                float y2 = origin.y + chart_h * (1.0f - static_cast<float>(pts[pi].price));
                dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, 1.5f);
            }
        };

        draw_line(yes_price_history_, MARKET_GREEN);
        draw_line(no_price_history_, MARKET_RED);

        // Legend
        float lx = origin.x + chart_w - 100;
        dl->AddLine(ImVec2(lx, origin.y + 8), ImVec2(lx + 16, origin.y + 8),
            ImGui::ColorConvertFloat4ToU32(MARKET_GREEN), 2.0f);
        dl->AddText(ImVec2(lx + 20, origin.y + 2), ImGui::ColorConvertFloat4ToU32(TEXT_DIM), "YES");
        dl->AddLine(ImVec2(lx, origin.y + 20), ImVec2(lx + 16, origin.y + 20),
            ImGui::ColorConvertFloat4ToU32(MARKET_RED), 2.0f);
        dl->AddText(ImVec2(lx + 20, origin.y + 14), ImGui::ColorConvertFloat4ToU32(TEXT_DIM), "NO");

        // Current price line
        if (!yes_price_history_.empty()) {
            double last = yes_price_history_.back().price;
            float ly = origin.y + chart_h * (1.0f - static_cast<float>(last));
            dl->AddLine(ImVec2(origin.x, ly), ImVec2(origin.x + chart_w, ly),
                ImGui::ColorConvertFloat4ToU32(ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.4f)), 1.0f);
            char plbl[16];
            std::snprintf(plbl, sizeof(plbl), "%.1f%%", last * 100.0);
            dl->AddText(ImVec2(origin.x + chart_w - 40, ly - 14),
                ImGui::ColorConvertFloat4ToU32(ACCENT), plbl);
        }

        ImGui::Dummy(ImVec2(chart_w, chart_h));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Order book
// ============================================================================

void PolymarketScreen::render_order_book(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##pm_ob", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "ORDER BOOK");
    ImGui::Separator();

    if (loading_book_) {
        ImGui::TextColored(TEXT_DIM, "Loading...");
    } else {
        // YES book
        ImGui::TextColored(MARKET_GREEN, "YES");
        ImGui::SameLine(w * 0.5f);
        ImGui::TextColored(TEXT_DIM, "Bid");
        ImGui::SameLine(w * 0.75f);
        ImGui::TextColored(TEXT_DIM, "Ask");

        int show = std::min(MAX_ORDER_LEVELS, static_cast<int>(
            std::max(yes_order_book_.bids.size(), yes_order_book_.asks.size())));

        for (int i = 0; i < show; i++) {
            ImGui::PushID(i);
            if (i < static_cast<int>(yes_order_book_.bids.size())) {
                ImGui::TextColored(MARKET_GREEN, "%.2f", yes_order_book_.bids[i].price);
                ImGui::SameLine(w * 0.25f);
                ImGui::TextColored(TEXT_DIM, "%.0f", yes_order_book_.bids[i].size);
            }
            if (i < static_cast<int>(yes_order_book_.asks.size())) {
                ImGui::SameLine(w * 0.5f);
                ImGui::TextColored(MARKET_RED, "%.2f", yes_order_book_.asks[i].price);
                ImGui::SameLine(w * 0.75f);
                ImGui::TextColored(TEXT_DIM, "%.0f", yes_order_book_.asks[i].size);
            }
            ImGui::PopID();
        }

        if (yes_order_book_.bids.empty() && yes_order_book_.asks.empty())
            ImGui::TextColored(TEXT_DIM, "No order book data");

        ImGui::Spacing();
        ImGui::Separator();

        // NO book
        ImGui::TextColored(MARKET_RED, "NO");
        ImGui::SameLine(w * 0.5f);
        ImGui::TextColored(TEXT_DIM, "Bid");
        ImGui::SameLine(w * 0.75f);
        ImGui::TextColored(TEXT_DIM, "Ask");

        show = std::min(MAX_ORDER_LEVELS, static_cast<int>(
            std::max(no_order_book_.bids.size(), no_order_book_.asks.size())));

        for (int i = 0; i < show; i++) {
            ImGui::PushID(i + 1000);
            if (i < static_cast<int>(no_order_book_.bids.size())) {
                ImGui::TextColored(MARKET_GREEN, "%.2f", no_order_book_.bids[i].price);
                ImGui::SameLine(w * 0.25f);
                ImGui::TextColored(TEXT_DIM, "%.0f", no_order_book_.bids[i].size);
            }
            if (i < static_cast<int>(no_order_book_.asks.size())) {
                ImGui::SameLine(w * 0.5f);
                ImGui::TextColored(MARKET_RED, "%.2f", no_order_book_.asks[i].price);
                ImGui::SameLine(w * 0.75f);
                ImGui::TextColored(TEXT_DIM, "%.0f", no_order_book_.asks[i].size);
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Recent trades
// ============================================================================

void PolymarketScreen::render_recent_trades(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##pm_trades", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "RECENT TRADES");
    ImGui::Separator();

    if (loading_trades_) {
        ImGui::TextColored(TEXT_DIM, "Loading...");
    } else if (recent_trades_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No trades available");
    } else {
        ImGui::TextColored(TEXT_DIM, "Side");
        ImGui::SameLine(w * 0.25f);
        ImGui::TextColored(TEXT_DIM, "Price");
        ImGui::SameLine(w * 0.5f);
        ImGui::TextColored(TEXT_DIM, "Size");
        ImGui::SameLine(w * 0.75f);
        ImGui::TextColored(TEXT_DIM, "Time");
        ImGui::Separator();

        int show_count = std::min(static_cast<int>(recent_trades_.size()), 20);
        for (int i = 0; i < show_count; i++) {
            const auto& t = recent_trades_[i];
            bool is_buy = (t.side == "BUY");

            ImGui::TextColored(is_buy ? MARKET_GREEN : MARKET_RED, "%s", t.side.c_str());
            ImGui::SameLine(w * 0.25f);
            ImGui::TextColored(TEXT_PRIMARY, "%.2f", t.price);
            ImGui::SameLine(w * 0.5f);
            ImGui::TextColored(TEXT_SECONDARY, "%.0f", t.size);
            ImGui::SameLine(w * 0.75f);

            if (t.timestamp > 0) {
                time_t ts = static_cast<time_t>(t.timestamp);
                struct tm* tm_info = localtime(&ts);
                char time_buf[16];
                if (tm_info) std::strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
                else         std::snprintf(time_buf, sizeof(time_buf), "--:--");
                ImGui::TextColored(TEXT_DIM, "%s", time_buf);
            } else {
                ImGui::TextColored(TEXT_DIM, "--:--");
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Top holders / Market info
// ============================================================================

void PolymarketScreen::render_top_holders(float w, float h) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##pm_holders", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "MARKET INFO");
    ImGui::Separator();

    if (selected_market_idx_ >= 0 && selected_market_idx_ < static_cast<int>(markets_.size())) {
        const auto& m = markets_[selected_market_idx_];

        if (!m.description.empty()) {
            ImGui::TextColored(TEXT_DIM, "Description:");
            ImGui::PushTextWrapPos(w - 8);
            std::string desc = m.description;
            if (desc.size() > 300) desc = desc.substr(0, 297) + "...";
            ImGui::TextColored(TEXT_SECONDARY, "%s", desc.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
        }

        if (!m.end_date.empty()) {
            ImGui::TextColored(TEXT_DIM, "End Date:");
            ImGui::SameLine();
            ImGui::TextColored(WARNING, "%s", time_ago(m.end_date).c_str());
        }

        if (m.volume_24h > 0) {
            ImGui::TextColored(TEXT_DIM, "24h Vol:");
            ImGui::SameLine();
            ImGui::TextColored(TEXT_PRIMARY, "%s", format_volume(m.volume_24h).c_str());
        }

        if (m.spread > 0) {
            ImGui::TextColored(TEXT_DIM, "Spread:");
            ImGui::SameLine();
            ImGui::TextColored(TEXT_PRIMARY, "%.2f%%", m.spread * 100.0);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ACCENT, "PRICE CHANGES");
        ImGui::Spacing();

        struct ChangeRow { const char* label; double val; };
        ChangeRow changes[] = {
            {"1 Day",   m.one_day_change},
            {"1 Week",  m.one_week_change},
            {"1 Month", m.one_month_change},
        };

        for (const auto& c : changes) {
            if (std::abs(c.val) > 0.0001) {
                ImGui::TextColored(TEXT_DIM, "%s:", c.label);
                ImGui::SameLine();
                ImGui::TextColored(c.val > 0 ? MARKET_GREEN : MARKET_RED,
                    "%s", format_percent(c.val).c_str());
            }
        }

        if (open_interest_ > 0) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Open Interest:");
            ImGui::SameLine();
            ImGui::TextColored(TEXT_PRIMARY, "%s", format_volume(open_interest_).c_str());
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Event detail
// ============================================================================

void PolymarketScreen::render_event_detail(float w, float h) {
    using namespace theme::colors;

    const auto& evts = (view_mode_ == ViewMode::resolved) ? resolved_events_ : events_;
    if (selected_event_idx_ < 0 || selected_event_idx_ >= static_cast<int>(evts.size())) {
        render_no_selection(w, h);
        return;
    }

    const auto& ev = evts[selected_event_idx_];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##pm_evt_hdr", ImVec2(w, 50), false);

    ImGui::SetCursorPos(ImVec2(12, 6));
    ImGui::PushTextWrapPos(w - 20);
    ImGui::TextColored(ACCENT, "%s", ev.title.c_str());
    ImGui::PopTextWrapPos();

    ImGui::SetCursorPos(ImVec2(12, 30));
    ImGui::TextColored(TEXT_DIM, "%d markets  |  Vol %s",
        static_cast<int>(ev.markets.size()), format_volume(ev.volume).c_str());

    if (ev.closed) {
        ImGui::SameLine(w - 80);
        ImGui::TextColored(MARKET_GREEN, "[RESOLVED]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##pm_evt_mkts", ImVec2(w, h - 50), false);

    if (!ev.description.empty()) {
        ImGui::SetCursorPos(ImVec2(12, 8));
        ImGui::PushTextWrapPos(w - 20);
        std::string desc = ev.description;
        if (desc.size() > 400) desc = desc.substr(0, 397) + "...";
        ImGui::TextColored(TEXT_SECONDARY, "%s", desc.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Separator();
    }

    ImGui::TextColored(ACCENT, "  MARKETS");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(ev.markets.size()); i++) {
        const auto& m = ev.markets[i];
        ImGui::PushID(i);

        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_PRIMARY, "%s", m.question.c_str());

        if (!m.outcome_prices.empty()) {
            ImGui::SameLine(w * 0.6f);
            for (int j = 0; j < static_cast<int>(m.outcomes.size()) && j < static_cast<int>(m.outcome_prices.size()); j++) {
                if (j > 0) ImGui::SameLine(0, 8);
                ImVec4 color = (j == 0) ?
                    (m.outcome_prices[j] >= 0.5 ? MARKET_GREEN : MARKET_RED) : TEXT_DIM;
                ImGui::TextColored(color, "%s %s",
                    m.outcomes[j].c_str(), format_price(m.outcome_prices[j]).c_str());
            }
        }

        ImGui::SameLine(w - 60);
        if (m.closed)       ImGui::TextColored(MARKET_GREEN, "DONE");
        else if (m.active)  ImGui::TextColored(ACCENT, "LIVE");

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::polymarket
