#pragma once
// Screener Screen — FRED Economic Data Screener with Bloomberg Terminal UI.
// Fetch, chart, and browse 800,000+ economic time series from FRED API.

#include "screener_types.h"
#include <string>
#include <vector>
#include <future>
#include <set>

namespace fincept::screener {

class ScreenerScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_header(float w);
    void render_api_key_warning(float w);
    void render_query_panel(float w);
    void render_chart_panel(float x, float y, float w, float h);
    void render_data_tables(float x, float y, float w, float h);
    void render_series_table(const FREDSeries& series, float w);
    void render_footer(float w, float total_h);

    // Browser modal
    void render_browser_modal();
    void render_browser_search_results();
    void render_browser_categories();
    void render_browser_series_list();
    void render_search_result_row(const SearchResult& result);

    // Simple line chart (ImGui DrawList)
    void render_line_chart(const FREDSeries& series, float w, float h, int color_idx);

    // Actions
    void fetch_data();
    void search_series(const std::string& query);
    void load_categories(int category_id);
    void load_category_series(int category_id);
    void check_api_key();
    void toggle_series(const std::string& series_id);
    void export_csv();

    // FRED API helpers
    std::string get_fred_api_key() const;
    FREDSeries fetch_single_series(const std::string& series_id,
                                    const std::string& start_date,
                                    const std::string& end_date,
                                    const std::string& api_key);
    std::vector<SearchResult> search_fred(const std::string& query,
                                           const std::string& api_key, int limit);
    std::vector<Category> fetch_categories(int parent_id, const std::string& api_key);
    std::vector<SearchResult> fetch_category_series(int cat_id,
                                                     const std::string& api_key, int limit);

    // State
    bool initialized_ = false;
    bool api_key_configured_ = false;

    // Form buffers
    char series_ids_buf_[512] = "GDP";
    char start_date_buf_[16] = "2000-01-01";
    char end_date_buf_[16] = {};
    ChartType chart_type_ = ChartType::line;
    bool normalize_data_ = false;

    // Loaded data
    std::vector<FREDSeries> data_;
    bool loading_ = false;
    std::string loading_message_;
    std::string error_;
    std::future<std::vector<FREDSeries>> fetch_future_;

    // Search
    std::vector<SearchResult> search_results_;
    bool search_loading_ = false;
    std::future<std::vector<SearchResult>> search_future_;

    // Browser modal
    bool show_browser_ = false;
    char browser_search_buf_[256] = {};
    std::vector<Category> categories_;
    std::vector<SearchResult> category_series_;
    bool category_loading_ = false;
    bool category_series_loading_ = false;
    int current_category_ = 0;
    std::vector<CategoryPath> category_path_;
    std::future<std::vector<Category>> category_future_;
    std::future<std::vector<SearchResult>> cat_series_future_;

    // Selected series IDs (for quick-add highlighting)
    std::set<std::string> selected_ids_;
    void parse_selected_ids();
};

} // namespace fincept::screener
