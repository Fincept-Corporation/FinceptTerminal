#pragma once
// Portfolio Screen — Investment tracking + Paper Trading UI
// Bloomberg-style dense layout with command bar, stats, holdings, order panel

#include "portfolio/portfolio_types.h"
#include "portfolio/portfolio_metrics.h"
#include "trading/paper_trading.h"
#include "views/analytics_view.h"
#include "views/performance_view.h"
#include "views/risk_view.h"
#include "views/reports_view.h"
#include "views/planning_view.h"
#include "views/economics_view.h"
#include "views/optimization_view.h"
#include "views/quantstats_view.h"
#include "views/indices_view.h"
#include <vector>
#include <string>
#include <map>

namespace fincept::portfolio {

enum class DetailView {
    None = 0,
    AnalyticsSectors,
    PerformanceRisk,
    Optimization,
    QuantStats,
    ReportsPME,
    CustomIndices,
    RiskManagement,
    Planning,
    Economics
};

class PortfolioScreen {
public:
    void render();

private:
    // State
    std::vector<Portfolio> portfolios_;
    int selected_portfolio_idx_ = -1;
    std::vector<PortfolioAsset> assets_;
    std::vector<Transaction> transactions_;
    PortfolioSummary summary_;
    bool needs_refresh_ = true;
    float refresh_timer_ = 0.0f;

    // Live price cache
    std::map<std::string, std::pair<double, double>> price_cache_; // symbol -> (current, prev_close)
    float price_fetch_timer_ = 0.0f;
    float auto_refresh_interval_ = 60.0f; // seconds
    int refresh_interval_idx_ = 0;

    // Computed metrics
    ComputedMetrics metrics_;
    bool metrics_loaded_ = false;

    // Historical data cache (symbol -> [{date, close}])
    std::map<std::string, std::vector<std::pair<std::string, double>>> historical_cache_;

    void compute_portfolio_metrics();

    // Import/Export state
    bool show_import_modal_ = false;
    char import_path_[512] = {};
    int import_mode_ = 0; // 0=new, 1=merge
    int import_merge_idx_ = 0;
    bool show_edit_txn_modal_ = false;
    std::string edit_txn_id_;
    char edit_txn_qty_[32] = {};
    char edit_txn_price_[32] = {};
    char edit_txn_date_[32] = {};
    char edit_txn_notes_[256] = {};

    void render_import_modal();
    void render_edit_transaction_modal();

    // Detail view state
    DetailView detail_view_ = DetailView::None;
    int detail_sub_tab_ = 0;
    AnalyticsView analytics_view_;
    PerformanceView performance_view_;
    RiskView risk_view_;
    ReportsView reports_view_;
    PlanningView planning_view_;
    EconomicsView economics_view_;
    OptimizationView optimization_view_;
    QuantStatsView quantstats_view_;
    IndicesView indices_view_;

    // UI state
    int sort_column_ = 5; // weight
    bool sort_ascending_ = false;
    std::string selected_symbol_;
    bool show_create_modal_ = false;
    bool show_add_asset_modal_ = false;
    bool show_sell_asset_modal_ = false;
    bool show_delete_confirm_ = false;
    bool show_order_panel_ = false;
    bool is_buy_ = true;

    // Modal form state
    char new_name_[128] = {};
    char new_owner_[128] = {};
    int new_currency_idx_ = 0;
    char add_symbol_[32] = {};
    char add_qty_[32] = {};
    char add_price_[32] = {};
    char sell_symbol_[32] = {};
    char sell_qty_[32] = {};
    char sell_price_[32] = {};

    // Paper Trading state
    bool paper_trading_mode_ = false;
    std::vector<trading::PtPortfolio> pt_portfolios_;
    int pt_selected_idx_ = -1;
    std::vector<trading::PtOrder> pt_orders_;
    std::vector<trading::PtPosition> pt_positions_;
    std::vector<trading::PtTrade> pt_trades_;
    trading::PtStats pt_stats_;
    bool pt_needs_refresh_ = true;
    bool show_pt_create_modal_ = false;
    char pt_name_[128] = {};
    char pt_balance_[32] = "100000";
    char pt_order_symbol_[32] = {};
    char pt_order_qty_[32] = {};
    char pt_order_price_[32] = {};
    int pt_order_type_idx_ = 0;
    bool pt_order_is_buy_ = true;

    // Methods
    void refresh_data();
    void refresh_pt_data();
    void render_command_bar();
    void render_stats_ribbon();
    void render_holdings_table();
    void render_transactions_table();
    void render_status_bar();
    void render_detail_buttons();
    void render_detail_header();

    // Detail views (stub implementations for now, each will get its own file later)
    void render_analytics_view();
    void render_performance_view();
    void render_optimization_view();
    void render_quantstats_view();
    void render_reports_view();
    void render_indices_view();
    void render_risk_view();
    void render_planning_view();
    void render_economics_view();

    // Paper Trading panels
    void render_pt_positions();
    void render_pt_orders();
    void render_pt_trades();
    void render_pt_stats();
    void render_pt_order_entry();

    // Modals
    void render_create_portfolio_modal();
    void render_add_asset_modal();
    void render_sell_asset_modal();
    void render_delete_confirm_modal();
    void render_pt_create_modal();
};

} // namespace fincept::portfolio
