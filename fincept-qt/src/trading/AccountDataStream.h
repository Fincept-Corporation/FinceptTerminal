#pragma once
// AccountDataStream — per-account data streaming service.
// Each active broker account gets one instance. Owns:
//   - WebSocket connection (if broker supports it: AngelOne, Zerodha)
//   - Polling timers (quote, portfolio, watchlist)
//   - Cached data snapshots (positions, holdings, orders, funds, quotes)
// Signals are emitted on the main thread for UI consumption.

#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <atomic>

namespace fincept::trading {

class AccountDataStream : public QObject {
    Q_OBJECT
  public:
    explicit AccountDataStream(const QString& account_id, QObject* parent = nullptr);
    ~AccountDataStream() override;

    QString account_id() const { return account_id_; }
    QString broker_id() const { return broker_id_; }

    // --- Lifecycle ---
    void start();   // Load credentials, open WS / start timers
    void stop();    // Close WS, stop timers
    void pause();   // Stop timers (screen hidden), keep WS alive
    void resume();  // Restart timers (screen shown)
    bool is_running() const { return running_; }

    // --- Symbol management ---
    void subscribe_symbols(const QStringList& symbols);
    void set_selected_symbol(const QString& symbol, const QString& exchange);
    QString selected_symbol() const { return selected_symbol_; }

    // --- On-demand fetches (for chart, orderbook, time & sales) ---
    void fetch_candles(const QString& symbol, const QString& timeframe);
    void fetch_orderbook(const QString& symbol);
    void fetch_time_sales(const QString& symbol);
    void fetch_latest_trade(const QString& symbol);
    void fetch_calendar();
    void fetch_clock();

    // --- Cached data access (main thread only) ---
    QVector<BrokerPosition> cached_positions() const;
    QVector<BrokerHolding> cached_holdings() const;
    QVector<BrokerOrderInfo> cached_orders() const;
    BrokerFunds cached_funds() const;
    BrokerQuote cached_quote(const QString& symbol) const;

  signals:
    void quote_updated(const QString& account_id, const QString& symbol, const BrokerQuote& quote);
    void watchlist_updated(const QString& account_id, const QVector<BrokerQuote>& quotes);
    void positions_updated(const QString& account_id, const QVector<BrokerPosition>& positions);
    void holdings_updated(const QString& account_id, const QVector<BrokerHolding>& holdings);
    void orders_updated(const QString& account_id, const QVector<BrokerOrderInfo>& orders);
    void funds_updated(const QString& account_id, const BrokerFunds& funds);
    void candles_fetched(const QString& account_id, const QVector<BrokerCandle>& candles);
    void orderbook_fetched(const QString& account_id,
                           const QVector<QPair<double, double>>& bids,
                           const QVector<QPair<double, double>>& asks,
                           double spread, double spread_pct);
    void time_sales_fetched(const QString& account_id, const QVector<BrokerTrade>& trades);
    void latest_trade_fetched(const QString& account_id, const BrokerTrade& trade);
    void calendar_fetched(const QString& account_id, const QVector<MarketCalendarDay>& days);
    void clock_fetched(const QString& account_id, const MarketClock& clock);
    void connection_state_changed(const QString& account_id, ConnectionState state);
    void token_expired(const QString& account_id);

  private:
    // --- Async fetch methods (all follow P8: QPointer + QtConcurrent + QueuedConnection) ---
    void async_fetch_quote();
    void async_fetch_positions();
    void async_fetch_holdings();
    void async_fetch_orders();
    void async_fetch_funds();
    void async_fetch_watchlist_quotes();

    // --- Timer callbacks ---
    void on_quote_timer();
    void on_portfolio_timer();
    void on_watchlist_timer();

    // --- WebSocket ---
    void ws_init();
    void ws_teardown();
    bool ws_active() const;
    void ws_resubscribe();

    // --- State ---
    QString account_id_;
    QString broker_id_;
    bool running_ = false;

    // WebSocket — polymorphic, null for brokers without WS
    QObject* ws_ = nullptr;

    // Polling timers
    QTimer* quote_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;

    // Subscriptions
    QString selected_symbol_;
    QString selected_exchange_;
    QStringList watchlist_symbols_;

    // Cached data (main thread only — no mutex needed since all updates are via QueuedConnection)
    QVector<BrokerPosition> positions_;
    QVector<BrokerHolding> holdings_;
    QVector<BrokerOrderInfo> orders_;
    BrokerFunds funds_;
    QHash<QString, BrokerQuote> quote_cache_;

    // Fetch guards
    std::atomic<bool> quote_fetching_{false};
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> portfolio_fetching_{false};
};

} // namespace fincept::trading
