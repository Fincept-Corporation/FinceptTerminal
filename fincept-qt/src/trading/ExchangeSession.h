#pragma once
// ExchangeSession — one exchange's live state.
//
// Each session owns the WS subprocess, price cache, symbol remap table,
// primary-symbol marker, watched-symbol set, and credentials for exactly
// ONE exchange. Sessions share one `ExchangeDaemonPool` (for REST calls)
// but never share WS pipes — a Kraken session and a Hyperliquid session
// each run their own `ws_stream.py` process.
//
// ExchangeSessionManager holds the live sessions and is responsible for all
// DataHub publishing; a session hands off fan-out data to the manager via a
// SessionPublisher injected at construction. This keeps sessions free of
// DataHub headers and centralises topic policies in the manager.
//
// Threading:
//  - session is created and destroyed on the main thread
//  - WS I/O is driven by QProcess signals on the main thread
//  - REST fetches (`fetch_*` / `place_*` / `cancel_*`) must be called from a
//    worker thread; they block up to `ExchangeDaemonPool::call` timeout.

#include "trading/TradingTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <functional>

namespace fincept::trading {

/// Thin publisher seam. The manager injects one of these at construction so
/// sessions never pull in DataHub headers themselves.
struct SessionPublisher {
    std::function<void(const QString& exchange, const QString& pair, const TickerData&)> publish_ticker;
    std::function<void(const QString& exchange, const QString& pair, const OrderBookData&)> publish_orderbook;
    std::function<void(const QString& exchange, const QString& pair, const TradeData&)> publish_trade;
    std::function<void(const QString& exchange, const QString& pair, const QString& interval, const Candle&)>
        publish_candle;
};

class ExchangeSession : public QObject {
    Q_OBJECT
  public:
    explicit ExchangeSession(const QString& exchange_id,
                             SessionPublisher publisher,
                             QObject* parent = nullptr);
    ~ExchangeSession() override;

    QString exchange_id() const { return exchange_id_; }

    // ── Credentials ────────────────────────────────────────────────────────
    /// Store credentials for this exchange. Persists to SecureStorage under
    /// `crypto:<exchange>:{api_key,secret,password}` so the next app launch
    /// picks them up automatically. Pass an empty-fields struct to clear.
    void set_credentials(const ExchangeCredentials& creds);
    ExchangeCredentials get_credentials() const;

    /// Load any previously-persisted credentials for this exchange from
    /// SecureStorage and install them on the session. Idempotent — missing
    /// keys leave the current credential fields untouched. Called
    /// automatically from the constructor; exposed for tests.
    void load_stored_credentials();

    // ── WS lifecycle ───────────────────────────────────────────────────────
    /// Returns true if the WS subprocess was spawned; see
    /// ExchangeService::start_ws_stream for full semantics.
    bool start_ws(const QString& primary_symbol, const QStringList& all_symbols);
    void stop_ws();
    bool is_ws_connected() const { return ws_connected_.load(); }
    /// True once a WS subprocess has been spawned and has not yet exited.
    /// Distinct from `is_ws_connected` — the process may be up but the remote
    /// handshake still pending. Used by the session manager to avoid
    /// re-spawning when switching back to a session that's already live.
    bool is_ws_active() const;
    void set_ws_primary_symbol(const QString& symbol);
    QString get_ws_primary_symbol() const;

    // ── Watch management (for paper-trading bookkeeping) ───────────────────
    void watch_symbol(const QString& symbol, const QString& portfolio_id);
    void unwatch_symbol(const QString& symbol, const QString& portfolio_id);
    QHash<QString, QSet<QString>> snapshot_watched() const;

    // ── One-shot REST fetches via the shared daemon pool.
    //     Call from a worker thread only — these block up to ~15s.
    TickerData fetch_ticker(const QString& symbol);
    QVector<TickerData> fetch_tickers(const QStringList& symbols);
    OrderBookData fetch_orderbook(const QString& symbol, int limit = 20);
    QVector<Candle> fetch_ohlcv(const QString& symbol, const QString& timeframe = "1h", int limit = 100);
    QVector<MarketInfo> fetch_markets(const QString& type = "", const QString& query = "");
    QVector<TradeData> fetch_trades(const QString& symbol, int limit = 50);
    FundingRateData fetch_funding_rate(const QString& symbol);
    OpenInterestData fetch_open_interest(const QString& symbol);
    MarkPriceData fetch_mark_price(const QString& symbol);

    // ── Authenticated — need credentials on the daemon.
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

    // ── Cache ──────────────────────────────────────────────────────────────
    TickerData get_cached_price(const QString& symbol) const;
    QHash<QString, TickerData> snapshot_price_cache() const;

    ExchangeSession(const ExchangeSession&) = delete;
    ExchangeSession& operator=(const ExchangeSession&) = delete;

  private:
    void handle_ws_line(const QString& line);
    void drain_ws_buffer();

    /// Apply the exchange→requested symbol remap (e.g. on Hyperliquid,
    /// `BTC/USDC:USDC` → `BTC/USDT`). Returns the input unchanged when no
    /// remap is known. Thread-safe — takes the session mutex internally.
    QString remap_symbol(const QString& exchange_symbol) const;

    QJsonObject daemon_call(const QString& method, const QJsonObject& args, int timeout_ms = 15000);

    static TickerData parse_ticker(const QJsonObject& j);
    static OrderBookData parse_orderbook(const QJsonObject& j);
    static Candle parse_candle(const QJsonObject& j);
    static MarketInfo parse_market(const QJsonObject& j);

    const QString exchange_id_;
    SessionPublisher publisher_;

    ExchangeCredentials credentials_;

    QProcess* ws_process_ = nullptr;
    std::atomic<bool> ws_connected_{false};
    QString ws_primary_symbol_;
    QStringList ws_all_symbols_;
    QHash<QString, QString> ws_symbol_map_;  // exchange_symbol → requested_symbol

    QHash<QString, TickerData> price_cache_;
    QHash<QString, QSet<QString>> watched_;   // symbol → set of portfolio_ids

    mutable QMutex mutex_;
};

} // namespace fincept::trading
