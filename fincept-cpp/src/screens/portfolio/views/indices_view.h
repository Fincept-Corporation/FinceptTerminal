#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>

namespace fincept::portfolio {

struct CustomIndex {
    std::string id;
    std::string name;
    std::string description;
    std::string method; // equal_weighted, price_weighted, market_cap_weighted
    double base_value = 100.0;
    double current_value = 100.0;
    double prev_close = 100.0;
    std::string currency = "USD";
    bool is_active = true;
    std::string created_at;
};

struct IndexConstituent {
    std::string id;
    std::string index_id;
    std::string symbol;
    double weight = 0.0;         // user-set or computed
    double current_price = 0.0;
    double prev_close = 0.0;
    double effective_weight = 0.0;
    double contribution = 0.0;
};

class IndicesView {
public:
    void render(const PortfolioSummary& summary);

private:
    // State
    std::vector<CustomIndex> indices_;
    int selected_idx_ = -1;
    std::vector<IndexConstituent> constituents_;
    bool needs_refresh_ = true;

    // Create modal
    bool show_create_ = false;
    char new_name_[128] = {};
    char new_desc_[256] = {};
    int new_method_idx_ = 0;
    char new_base_[16] = "100";

    // Add constituent
    bool show_add_ = false;
    char add_symbol_[32] = {};
    char add_weight_[16] = {};

    void refresh_data();
    void create_index();
    void delete_index(const std::string& id);
    void add_constituent(const std::string& index_id);
    void remove_constituent(const std::string& id);
    void recalculate_index(const std::string& index_id);
    void ensure_tables();

    void render_index_list();
    void render_index_detail();
    void render_create_modal();
    void render_add_constituent_modal();
};

} // namespace fincept::portfolio
