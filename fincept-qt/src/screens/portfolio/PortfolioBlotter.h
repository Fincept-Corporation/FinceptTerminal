// src/screens/portfolio/PortfolioBlotter.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataTypes.h"
#include "services/portfolio/PortfolioAllocation.h"

#include <QHash>
#include <QHideEvent>
#include <QPoint>
#include <QShowEvent>
#include <QStringList>
#include <QWidget>

class QLabel;
class QTableView;

namespace fincept::screens {

class PfBlotterModel;
class PfBlotterProxy;

/// Positions blotter — model/view table with a column chooser.
///
/// Columns (choose with the header's right-click menu or show_column_menu()):
/// symbol / name, quantity, last price and average cost in the instrument's own
/// quote currency at its own precision, currency and spot FX, value in local and
/// base currency, cost, unrealised P&L split into price and FX, day change, trend
/// sparkline, weight and risk-share bars, beta, volatility, component VaR,
/// realised P&L, dividends, yield, sector / industry / asset class / country /
/// exchange, 52-week range bar, market state / as-of and open lots. A pinned
/// totals row closes the table. Unpriced / unconverted holdings are marked and
/// show "—" (never zero). Every column sorts; absent values sort last.
class PortfolioBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioBlotter(QWidget* parent = nullptr);
    ~PortfolioBlotter() override;

    /// Holdings + base currency + totals.
    void set_summary(const portfolio::PortfolioSummary& summary);
    /// Legacy entry point: holdings only (base currency of the last summary).
    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    /// Per-holding beta / volatility / risk share / component VaR.
    void set_analytics(const portfolio::MarketAnalytics& analytics);
    /// Sector / industry / asset class / country / exchange per holding.
    void set_allocation(const portfolio::AllocationReport& report);
    /// market:info fundamentals per symbol (52-week range, dividend yield).
    void set_market_info(const QHash<QString, services::InfoData>& info);

    void set_selected_symbol(const QString& symbol);
    void set_filter(const QString& text);
    /// Show only rows whose symbol is in @p symbols. Empty list = show all.
    void set_sector_filter(const QStringList& symbols);
    void refresh_theme();
    /// Save the visible columns of the filtered, sorted view as CSV.
    void export_csv();
    /// Column chooser at a global position (for a header button).
    void show_column_menu(const QPoint& global_pos);
    /// Tell the blotter which broker account (if any) backs the portfolio so it
    /// can overlay live broker ticks. Safe to call repeatedly and with an empty
    /// id (= unlinked portfolio); the id is remembered across re-subscriptions.
    void hub_resubscribe_broker_quotes(const QString& broker_account_id);

    int visible_count() const;

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  signals:
    void symbol_selected(QString symbol);
    /// "Open <symbol> in <screen_id>" (double-click / context menu). The owner
    /// publishes it on the EventBus (nav.open_symbol).
    void open_symbol_requested(QString screen_id, QString symbol);
    void sort_changed(portfolio::SortColumn col, portfolio::SortDirection dir);
    void edit_transaction_requested(QString symbol);
    void delete_position_requested(QString symbol);
    /// Row menu "Buy more" / "Sell" / "Record dividend" / "Record split" for a symbol.
    void buy_requested(QString symbol);
    void sell_requested(QString symbol);
    void split_requested(QString symbol);

  private:
    void build_ui();
    void apply_styles();
    void retranslateUi();
    void on_context_menu(const QPoint& pos);
    void save_columns() const;
    void restore_columns();
    /// PRICE P&L / FX P&L columns: the user's own choice from the column menu when
    /// there is one, else shown when the FX split setting is on and the book holds a
    /// foreign-currency position.
    void apply_split_columns();
    void save_split_choice() const;
    void update_footer();
    void reselect();
    QString symbol_at_proxy_row(int row) const;

    void hub_resubscribe_sparklines(bool force_refresh);
    void subscribe_broker_quotes();
    void hub_unsubscribe_all();
    void fetch_sparklines();

    QTableView* view_ = nullptr;
    PfBlotterModel* model_ = nullptr;
    PfBlotterProxy* proxy_ = nullptr;
    QLabel* footer_ = nullptr;

    bool hub_active_ = false;
    QStringList sub_symbols_;   // sorted symbol set the hub subscriptions were built for
    QString broker_account_id_; // backing broker account of the current portfolio ("" = none)
    QString selected_symbol_;
    bool columns_restored_ = false;
    QHash<int, bool> split_explicit_; ///< column -> shown, for the split columns the user set by hand
};

} // namespace fincept::screens
