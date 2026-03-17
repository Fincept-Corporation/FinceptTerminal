#pragma once
// Polymarket Prediction Markets Screen — Bloomberg terminal-style UI.
// Layout: top_nav | search_bar | [left: market_list | right: detail_panel]

#include "polymarket_types.h"
#include "http/http_client.h"
#include <string>
#include <vector>
#include <future>
#include <mutex>

namespace fincept::polymarket {

class PolymarketScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_top_nav(float w);
    void render_search_bar(float w);
    void render_market_list(float w, float h);
    void render_detail_panel(float w, float h);

    // Detail sub-panels
    void render_market_detail(float w, float h);
    void render_event_detail(float w, float h);
    void render_no_selection(float w, float h);

    // Market detail sections
    void render_price_chart(float w, float h);
    void render_order_book(float w, float h);
    void render_recent_trades(float w, float h);
    void render_top_holders(float w, float h);

    // Data loading
    void load_markets();
    void load_events();
    void load_sports();
    void load_resolved();
    void load_market_detail(int idx);
    void search_markets();
    void refresh_data();

    // JSON parsing
    void parse_markets(const std::string& json_str);
    void parse_events(const std::string& json_str);
    void parse_tags(const std::string& json_str);
    void parse_sports(const std::string& json_str);
    void parse_trades(const std::string& json_str);
    void parse_order_book(const std::string& json_str, bool is_yes);
    void parse_price_history(const std::string& json_str, bool is_yes);

    // Helpers
    static std::string format_volume(double vol);
    static std::string format_price(double price);
    static std::string format_percent(double pct);
    static std::string time_ago(const std::string& date_str);

    // State
    bool initialized_ = false;

    // View
    ViewMode view_mode_ = ViewMode::markets;
    SortBy   sort_by_   = SortBy::volume;
    bool     show_closed_ = false;
    int      current_page_ = 0;

    // Search
    char search_buf_[256] = {};
    char tag_filter_[64]  = {};

    // Data
    std::vector<Market> markets_;
    std::vector<Event>  events_;
    std::vector<Tag>    tags_;
    std::vector<Sport>  sports_;
    std::vector<Event>  resolved_events_;
    std::mutex          data_mutex_;

    // Selection
    int  selected_market_idx_ = -1;
    int  selected_event_idx_  = -1;
    int  selected_sport_idx_  = -1;

    // Market detail data
    std::vector<Trade>      recent_trades_;
    std::vector<PricePoint> yes_price_history_;
    std::vector<PricePoint> no_price_history_;
    OrderBook               yes_order_book_;
    OrderBook               no_order_book_;
    std::vector<Holder>     top_holders_;
    double                  open_interest_ = 0.0;
    int                     chart_interval_idx_ = 2; // index into intervals array

    // Async
    std::future<http::HttpResponse> pending_list_;
    std::future<http::HttpResponse> pending_detail_;
    std::future<http::HttpResponse> pending_trades_;
    std::future<http::HttpResponse> pending_book_yes_;
    std::future<http::HttpResponse> pending_book_no_;
    std::future<http::HttpResponse> pending_history_yes_;
    std::future<http::HttpResponse> pending_history_no_;
    std::future<http::HttpResponse> pending_tags_;

    bool loading_list_   = false;
    bool loading_detail_ = false;
    bool loading_trades_ = false;
    bool loading_book_   = false;
    bool loading_chart_  = false;

    std::string error_msg_;

    // Scroll state
    float list_scroll_y_   = 0.0f;
    float detail_scroll_y_ = 0.0f;
};

} // namespace fincept::polymarket
