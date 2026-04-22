// src/screens/portfolio/PortfolioScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
#include "screens/portfolio/PortfolioTypes.h"

#include <QHideEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
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
class PortfolioInsightsPanel;

class PortfolioScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit PortfolioScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "portfolio"; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

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
    void refresh_theme();
    QWidget* build_empty_state();
    QWidget* build_loading_state();
    QWidget* build_main_view();
    void update_content_state();
    void update_main_view_data();
    void request_refresh();
    void load_demo_portfolio();
    void reposition_order_panel();
    void animate_order_panel_in();
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
    PortfolioInsightsPanel* insights_panel_ = nullptr;
    QWidget* insights_scrim_ = nullptr;

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

    // Order panel slide-in animation
    QPropertyAnimation* order_panel_anim_ = nullptr;

    // Positions section header — count badge updates with holdings
    QLabel* positions_count_label_ = nullptr;
};

} // namespace fincept::screens
