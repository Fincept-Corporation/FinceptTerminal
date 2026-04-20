#pragma once
// ExchangeService — facade over ExchangeSessionManager.
//
// Coordinates the "which exchange is currently active" state for legacy
// screens that haven't been ported to subscribe to DataHub topics directly.
// All data streaming is consumed via the hub — this facade only wraps the
// one-shot REST fetches, configuration, credentials, and WS lifecycle.
//
// Notes:
//  - `set_exchange(id)` only flips which session is "active" for these
//    facade calls. It does NOT stop the previous session's WS process —
//    that session stays warm so switching back is sub-second.
//  - Streaming data (ticker / orderbook / trades / ohlc) is NOT surfaced
//    here any more. Subscribe to `ws:<exchange>:<family>:<pair>` on
//    DataHub instead (see ExchangeSessionManager).

#include "trading/TradingTypes.h"

#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>

namespace fincept::trading {

class ExchangeSession;

class ExchangeService : public QObject {
    Q_OBJECT
  public:
    static ExchangeService& instance();

    /// Back-compat shim — delegates to
    /// ExchangeSessionManager::ensure_registered_with_hub().
    void ensure_registered_with_hub();

    // Configuration
    void set_exchange(const QString& exchange_id);
    QString get_exchange() const;
    void set_credentials(const ExchangeCredentials& creds);

    bool wait_for_daemon_ready(int timeout_ms = 8000);

  signals:
    /// Forwarded from ExchangeDaemonPool::ready() — first time the daemon
    /// finishes its cold-boot. Subsequent daemon restarts also emit this.
    void daemon_ready();

  public:
    // Price feed (service-level; polls the active session's cached data)
    void watch_symbol(const QString& symbol, const QString& portfolio_id);
    void unwatch_symbol(const QString& symbol, const QString& portfolio_id);
    void start_price_feed(int interval_seconds = 5);
    void stop_price_feed();
    bool is_feed_running() const;

    // WebSocket streaming (delegated to current session). Data consumers
    // subscribe to DataHub topics — these entry points are for lifecycle
    // control (start/stop/status) only.
    bool start_ws_stream(const QString& primary_symbol, const QStringList& all_symbols);
    void stop_ws_stream();
    bool is_ws_connected() const;
    /// Process-level check — does the current session already have a running
    /// WS subprocess. Screens use this to skip re-spawning when flipping
    /// back to an already-warm exchange.
    bool is_ws_active() const;
    void set_ws_primary_symbol(const QString& symbol);
    QString get_ws_primary_symbol() const;

    // One-shot data fetches — delegated to current session (synchronous; worker thread only).
    TickerData fetch_ticker(const QString& symbol);
    QVector<TickerData> fetch_tickers(const QStringList& symbols);
    OrderBookData fetch_orderbook(const QString& symbol, int limit = 20);
    QVector<Candle> fetch_ohlcv(const QString& symbol, const QString& timeframe = "1h", int limit = 100);
    QVector<MarketInfo> fetch_markets(const QString& type = "", const QString& query = "");
    QStringList list_exchange_ids();
    QVector<TradeData> fetch_trades(const QString& symbol, int limit = 50);
    FundingRateData fetch_funding_rate(const QString& symbol);
    OpenInterestData fetch_open_interest(const QString& symbol);

    // Authenticated operations — current session, worker thread only.
    QJsonObject fetch_balance();
    QJsonObject place_exchange_order(const QString& symbol, const QString& side, const QString& type, double amount,
                                     double price = 0.0);
    QJsonObject cancel_exchange_order(const QString& order_id, const QString& symbol);
    QJsonObject fetch_positions_live(const QString& symbol = "");
    QJsonObject fetch_open_orders_live(const QString& symbol = "");
    QJsonObject fetch_my_trades(const QString& symbol, int limit = 50);
    QJsonObject fetch_trading_fees(const QString& symbol = "");
    QJsonObject set_leverage(const QString& symbol, int leverage);
    QJsonObject set_margin_mode(const QString& symbol, const QString& mode);

    MarkPriceData fetch_mark_price(const QString& symbol);

    // Cache (reads the active session's cache)
    TickerData get_cached_price(const QString& symbol) const;

    ExchangeService(const ExchangeService&) = delete;
    ExchangeService& operator=(const ExchangeService&) = delete;

  private:
    ExchangeService();
    ~ExchangeService();

    /// Resolve the active session through the manager. Never null.
    ExchangeSession* active_session() const;

    void poll_prices();

    mutable QMutex mutex_;
    QString exchange_id_ = "kraken";

    // Price feed timer (facade-level; iterates active session's cache / does
    // a small fetch_tickers to refresh)
    QTimer* feed_timer_ = nullptr;
    std::atomic<bool> feed_running_{false};
    std::atomic<bool> poll_in_progress_{false};
    int feed_interval_ = 5;
};

} // namespace fincept::trading
