// src/screens/portfolio/PortfolioScreen.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHideEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>
#include <optional>

namespace fincept::screens {

class PortfolioCommandBar;
class PortfolioStatsRibbon;
class PortfolioStatusBar;
class PortfolioHeatmap;
class PortfolioPerfChart;
class PortfolioSectorPanel;
class PortfolioBlotter;
class PortfolioTxnPanel;
class PortfolioOrderPanel;
class PortfolioDetailWrapper;
class PortfolioFFNView;
class PortfolioAiPanel;
class PortfolioAgentPanel;

class PortfolioScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_portfolios_loaded(QVector<portfolio::Portfolio> portfolios);
    void on_portfolio_selected(const QString& id);
    void on_summary_loaded(portfolio::PortfolioSummary summary);
    void on_summary_error(QString portfolio_id, QString error);
    void on_metrics_computed(portfolio::ComputedMetrics metrics);
    void on_snapshots_loaded(QString portfolio_id, QVector<portfolio::PortfolioSnapshot> snapshots);
    void on_portfolio_created(portfolio::Portfolio portfolio);
    void on_portfolio_deleted(QString id);
    void on_asset_changed(QString portfolio_id);
    void on_create_requested();
    void on_delete_requested(const QString& id);
    void on_detail_view_selected(portfolio::DetailView view);
    void on_refresh_interval_changed(int ms);
    void on_symbol_selected(const QString& symbol);
    void on_buy_requested();
    void on_sell_requested();
    void on_order_panel_close();

  private:
    void build_ui();
    QWidget* build_empty_state();
    QWidget* build_loading_state();
    QWidget* build_main_view();
    void update_content_state();
    void update_main_view_data();
    void request_refresh();
    void load_demo_portfolio();
    const portfolio::HoldingWithQuote* find_holding(const QString& symbol) const;

    // Sub-widgets
    PortfolioCommandBar* command_bar_ = nullptr;
    PortfolioStatsRibbon* stats_ribbon_ = nullptr;
    PortfolioStatusBar* status_bar_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;

    // Content pages
    QWidget* empty_state_ = nullptr;
    QWidget* loading_state_ = nullptr;
    QWidget* main_view_ = nullptr;

    // Main view sub-widgets
    PortfolioHeatmap* heatmap_ = nullptr;
    PortfolioPerfChart* perf_chart_ = nullptr;
    PortfolioSectorPanel* sector_panel_ = nullptr;
    PortfolioBlotter* blotter_ = nullptr;
    PortfolioTxnPanel* txn_panel_ = nullptr;
    PortfolioOrderPanel* order_panel_ = nullptr;
    PortfolioDetailWrapper* detail_wrapper_ = nullptr;
    PortfolioFFNView* ffn_view_ = nullptr;
    PortfolioAiPanel* ai_panel_ = nullptr;
    PortfolioAgentPanel* agent_panel_ = nullptr;

    // State
    QVector<portfolio::Portfolio> portfolios_;
    QString selected_id_;
    QString selected_symbol_;
    portfolio::PortfolioSummary current_summary_;
    portfolio::ComputedMetrics current_metrics_;
    bool summary_loaded_ = false;
    bool order_panel_visible_ = false;
    bool show_ffn_ = false;
    std::optional<portfolio::DetailView> active_detail_;

    // Refresh timer (P3)
    QTimer* refresh_timer_ = nullptr;
    int refresh_interval_ms_ = 60000;
};

} // namespace fincept::screens
