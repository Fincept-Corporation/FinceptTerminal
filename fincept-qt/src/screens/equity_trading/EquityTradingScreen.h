#pragma once
// Equity Trading Screen — multi-account coordinator.
// Supports multiple broker accounts simultaneously via DataStreamManager.
// Each account has its own data stream (WS/polling), portfolio, and credentials.

#include "core/symbol/IGroupLinked.h"
#include "screens/IStatefulScreen.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/workspace/IWorkspaceParticipant.h"
#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::screens::equity {
class EquityTickerBar;
class EquityWatchlist;
class EquityChart;
class EquityOrderEntry;
class EquityOrderBook;
class EquityBottomPanel;
} // namespace fincept::screens::equity

namespace fincept::screens {

class EquityTradingScreen : public QWidget, public IWorkspaceParticipant, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

    QJsonObject save_state() const override;
    void restore_state(const QJsonObject& state) override;

    // IGroupLinked — see WatchlistScreen for the propagation contract.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    // Multi-account slots
    void on_account_changed(const QString& account_id);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_accounts_clicked(); // opens AccountManagementDialog
    void handle_token_expired(const QString& account_id);
    void on_order_submitted(const trading::UnifiedOrder& order);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);
    void on_import_holdings_requested(const QVector<trading::BrokerHolding>& holdings);

    void refresh_candles();
    void update_clock();

    // DataStreamManager signal handlers
    void on_stream_quote_updated(const QString& account_id, const QString& symbol, const trading::BrokerQuote& quote);
    void on_stream_watchlist_updated(const QString& account_id, const QVector<trading::BrokerQuote>& quotes);
    void on_stream_positions_updated(const QString& account_id, const QVector<trading::BrokerPosition>& positions);
    void on_stream_holdings_updated(const QString& account_id, const QVector<trading::BrokerHolding>& holdings);
    void on_stream_orders_updated(const QString& account_id, const QVector<trading::BrokerOrderInfo>& orders);
    void on_stream_funds_updated(const QString& account_id, const trading::BrokerFunds& funds);
    void on_stream_candles_fetched(const QString& account_id, const QVector<trading::BrokerCandle>& candles);
    void on_stream_orderbook_fetched(const QString& account_id,
                                     const QVector<QPair<double, double>>& bids,
                                     const QVector<QPair<double, double>>& asks,
                                     double spread, double spread_pct);
    void on_stream_time_sales_fetched(const QString& account_id, const QVector<trading::BrokerTrade>& trades);
    void on_stream_latest_trade_fetched(const QString& account_id, const trading::BrokerTrade& trade);
    void on_stream_calendar_fetched(const QString& account_id, const QVector<trading::MarketCalendarDay>& days);
    void on_stream_clock_fetched(const QString& account_id, const trading::MarketClock& clock);

  private:
    void setup_ui();
    void setup_timers();
    void connect_data_stream_signals();
    void init_focused_account();
    void switch_symbol(const QString& symbol);
    void update_account_menu();
    void update_connection_status();
    void async_modify_order(const QString& order_id, double qty, double price);

    // ── Command bar widgets ──
    QPushButton* account_btn_ = nullptr;  // shows focused account name
    QMenu* account_menu_ = nullptr;       // lists all active accounts
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* accounts_btn_ = nullptr; // opens AccountManagementDialog
    QLabel* exchange_label_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* conn_label_ = nullptr;        // aggregate connection status

    // ── Sub-widgets ──
    equity::EquityTickerBar* ticker_bar_ = nullptr;
    equity::EquityWatchlist* watchlist_ = nullptr;
    equity::EquityChart* chart_ = nullptr;
    equity::EquityOrderEntry* order_entry_ = nullptr;
    equity::EquityOrderBook* orderbook_ = nullptr;
    equity::EquityBottomPanel* bottom_panel_ = nullptr;

    // ── Timers (only UI-local timers remain; data timers are in AccountDataStream) ──
    QTimer* clock_timer_ = nullptr;
    QTimer* market_clock_timer_ = nullptr;

    // ── Multi-account state ──
    QString focused_account_id_;          // the account targeted by order entry + chart
    QString selected_symbol_ = "RELIANCE";
    QString selected_exchange_ = "NSE";

    // Paper trading (derived from focused account)
    int fill_cb_id_ = -1; // OrderMatcher fill callback

    // Async guards
    std::atomic<bool> token_expired_shown_{false};

    QStringList watchlist_symbols_;
    double current_price_ = 0.0; // last known LTP for selected symbol

    bool initialized_ = false;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
