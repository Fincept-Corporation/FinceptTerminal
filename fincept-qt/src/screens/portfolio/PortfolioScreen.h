// src/screens/portfolio/PortfolioScreen.h
#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/portfolio/PortfolioTypes.h"

#include <QHideEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;

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

class PortfolioScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit PortfolioScreen(QWidget* parent = nullptr);

    /// ISectionLinked — the detail tabs "sectors" "perf_risk" "optimization" "quantstats" "reports"
    /// "indices" "risk" "tax_lots" "income" "planning" "economics", "positions" (the main view),
    /// "new" (the CREATE NEW dialog; nothing is created without its OK) and "portfolio:<id>" (selects
    /// it as the list does). focus = a held symbol: that holding is selected.
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "portfolio"; }

    // IGroupLinked — active symbol = currently selected holding. Receiving
    // a group symbol scrolls/selects that holding if it exists in the
    // current portfolio; symbols not held are silently ignored (no synthetic
    // "phantom" selection).
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

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
    void on_market_analytics(bool ok, const QString& error);
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
    /// Re-applies the font-derived fixed sizes of the screen's own chrome
    /// (empty-state cards, POSITIONS header badges). Defined in PortfolioScreen_Layout.cpp.
    void apply_font_sizes();
    void retranslateUi();
    QWidget* build_empty_state();
    QWidget* build_loading_state();
    QWidget* build_main_view();
    void update_content_state();
    void update_loading_anim(); // run the skeleton pulse only while it is on screen
    void update_main_view_data();
    void request_refresh();
    void load_demo_portfolio();
    void reposition_order_panel();
    void animate_order_panel_in();
    const portfolio::HoldingWithQuote* find_holding(const QString& symbol) const;
    void edit_transaction_by_id(const QString& transaction_id);
    void delete_transaction_by_id(const QString& transaction_id);
    void export_ledger_csv();
    /// Filter the POSITIONS blotter to `sector` (empty = all) and show the filter in its header.
    void apply_sector_filter(const QString& sector);
    /// compute_metrics for the whole history (charts, detail views) and, when it differs,
    /// for the stats ribbon's period.
    void request_metrics();
    /// Full ledger of the selected portfolio into the transaction panel (no row cap).
    void reload_transactions();
    /// Ledger dialogs (PortfolioScreen_Ledger.cpp).
    void open_buy_dialog(const QString& preset_symbol);
    void open_sell_dialog(const QString& symbol);
    void open_dividend_dialog();
    void open_cash_dialog();
    void open_split_dialog();
    void open_settings_dialog();
    /// Market info (sector, country, 52-week range, yield) for the blotter / income views.
    void refresh_allocation();
    /// The FFN view, created (and added to the content stack) on first use.
    PortfolioFFNView* ensure_ffn_view();
    /// Warning box with the technical reason (raw database error) under "Show Details…".
    void show_failure(const QString& title, const QString& message, const QString& details);
    /// The portfolio list could not be read: the start page says so (translated, the raw
    /// reason in a tooltip) and its CREATE / IMPORT / DEMO cards are disabled. Empty = readable.
    void set_storage_failure(const QString& message, const QString& details);

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
    bool asset_refresh_queued_ = false; // coalesces bursts of asset_added/asset_sold into one refresh
    bool order_panel_visible_ = false;
    bool show_ffn_ = false;
    std::optional<portfolio::DetailView> active_detail_;

    // How much snapshot history the charts are fed. The PERF chart offers 5Y / ALL
    // and PERF/RISK offers ALL, but the screen asked for the service default (365
    // days), so even after a backfill wrote years of NAV rows those periods could
    // never show more than one year. (compute_metrics reads its own 1-year window.)
    static constexpr int kSnapshotHistoryDays = 3650;

    // Refresh timer (P3)
    QTimer* refresh_timer_ = nullptr;
    int refresh_interval_ms_ = 60000;

    // Order panel slide-in animation
    QPropertyAnimation* order_panel_anim_ = nullptr;

    // Positions section header — count badge updates with holdings
    QLabel* positions_count_label_ = nullptr;
    QLabel* positions_title_label_ = nullptr;
    QLineEdit* positions_filter_edit_ = nullptr;
    QPushButton* sector_filter_btn_ = nullptr; // "<sector> ✕" chip; visible while a sector filter is active
    QString sector_filter_;                    // active sector filter on the blotter ("" = none)

    // Empty / loading state labels — kept for retranslateUi.
    QLabel* empty_title_label_ = nullptr;
    QLabel* empty_sub_label_ = nullptr;
    struct CtaCardLabels {
        QLabel* title = nullptr;
        QLabel* subtitle = nullptr;
    };
    CtaCardLabels empty_create_card_;
    CtaCardLabels empty_import_card_;
    CtaCardLabels empty_demo_card_;
    QLabel* loading_label_ = nullptr;
    QPropertyAnimation* loading_anim_ = nullptr;
    QLabel* storage_error_label_ = nullptr; ///< start page: database cannot be read (hidden otherwise)
    QString storage_error_message_;
    QString storage_error_details_;

    // Symbol-group link (None when unlinked).
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
