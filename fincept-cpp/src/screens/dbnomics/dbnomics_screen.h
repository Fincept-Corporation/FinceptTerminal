#pragma once
// DBnomics Screen — Browse economic data from db.nomics.world
// Port of DBnomicsTab.tsx — provider/dataset/series selection,
// global search, single/comparison views, line charts, data table

#include "dbnomics_types.h"
#include <string>
#include <vector>
#include <map>
#include <future>

namespace fincept::dbnomics {

class DBnomicsScreen {
public:
    void render();

private:
    // --- Data ---
    std::vector<Provider> providers_;
    std::vector<Dataset> datasets_;
    std::vector<Series> series_list_;
    std::vector<SearchResult> search_results_;

    // Current loaded series data
    DataPoint current_data_;
    bool has_current_data_ = false;

    // Single view: multiple series overlaid
    std::vector<DataPoint> single_view_series_;
    ChartType single_chart_type_ = ChartType::Line;

    // Comparison view: slots, each with multiple series
    std::vector<std::vector<DataPoint>> slots_ = {{}};
    std::vector<ChartType> slot_chart_types_ = {ChartType::Line};

    enum class ActiveView { Single, Comparison };
    ActiveView active_view_ = ActiveView::Single;

    // --- Selection ---
    std::string selected_provider_;
    std::string selected_dataset_;
    std::string selected_series_;  // full_id

    // --- Search filters ---
    char provider_search_[128] = {};
    char dataset_search_[128] = {};
    char series_search_[128] = {};
    char global_search_[256] = {};

    // --- Pagination ---
    PaginationState datasets_pagination_;
    PaginationState series_pagination_;
    PaginationState search_pagination_;

    // --- Async ---
    bool loading_ = false;
    std::string status_ = "Ready";
    std::string error_;
    bool initialized_ = false;
    std::future<void> async_task_;

    // --- Data fetching ---
    void load_providers();
    void load_datasets(const std::string& provider_code, int offset = 0, bool append = false);
    void load_series(const std::string& provider, const std::string& dataset,
                     int offset = 0, bool append = false, const std::string& query = "");
    void load_series_data(const std::string& full_id, const std::string& name);
    void execute_global_search(const std::string& query, int offset = 0, bool append = false);

    // --- Actions ---
    void select_provider(const std::string& code);
    void select_dataset(const std::string& code);
    void select_series(const Series& s);
    void add_to_single_view();
    void remove_from_single_view(int index);
    void add_slot();
    void remove_slot(int index);
    void add_to_slot(int slot_index);
    void remove_from_slot(int slot_index, int series_index);
    void clear_all();

    // --- Render sections ---
    void render_top_bar();
    void render_selection_panel();
    void render_global_search();
    void render_provider_list();
    void render_dataset_list();
    void render_series_list();
    void render_action_buttons();
    void render_comparison_slots();
    void render_center_panel();
    void render_view_toggle_bar();
    void render_single_view();
    void render_comparison_view();
    void render_chart(const std::vector<DataPoint>& series_arr, ChartType chart_type);
    void render_data_table(const std::vector<DataPoint>& series_arr);
    void render_status_bar();

    // --- Helpers ---
    int next_color_index() const;
    bool is_async_done() const;
    void check_async();
};

} // namespace fincept::dbnomics
