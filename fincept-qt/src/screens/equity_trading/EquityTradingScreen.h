#pragma once
// Equity Trading Screen — multi-account coordinator.
// Supports multiple broker accounts simultaneously via DataStreamManager.
// Each account has its own data stream (WS/polling), portfolio, and credentials.

#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/equity_trading/EquityTypes.h"
#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QHash>
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

class EquityTradingScreen : public QWidget, public IGroupLinked, public IStatefulScreen {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

    // IGroupLinked — see WatchlistScreen for the propagation contract.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // IStatefulScreen
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return QStringLiteral("equity_trading"); }

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

    // Instruments ready — reloads watchlist/market data for the active account
    // when InstrumentService finishes a load/download for the matching broker.
    void on_instruments_ready(const QString& broker_id);

    void refresh_candles();
    void update_clock();

    // DataStreamManager signal handlers (on-demand / one-shot — kept as legacy signals)
    void on_stream_candles_fetched(const QString& account_id, const QVector<trading::BrokerCandle>& candles);
    void on_stream_orderbook_fetched(const QString& account_id,
                                     const QVector<QPair<double, double>>& bids,
                                     const QVector<QPair<double, double>>& asks,
                                     double spread, double spread_pct,
                                     const QVector<int>& bid_orders,
                                     const QVector<int>& ask_orders);
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

    // DataHub subscription helpers (D4 migration)
    void hub_subscribe_streaming();
    void hub_unsubscribe_all();
    void hub_subscribe_quotes();
    QString broker_id_for_focused() const;

    // Ensure the broker instrument master for `account_id` is loaded into
    // InstrumentService (from SQLite cache or a fresh download). Market data
    // (quotes/charts/depth) needs the numeric securityId map, which only exists
    // once instruments are loaded. Mirrors the ChainSubTab load pattern.
    void ensure_instruments_loaded(const QString& account_id);

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
    bool hub_active_ = false;
    QHash<QString, trading::BrokerQuote> watchlist_quote_cache_;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
