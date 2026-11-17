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

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

class QThread;

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
    explicit ExchangeSession(const QString& exchange_id, SessionPublisher publisher, QObject* parent = nullptr);
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
    /// A stored field that cannot be read installs NOTHING (no partial key set)
    /// and sets credentials_read_error(); a temporary failure is retried a few
    /// times with back-off. Nothing is written or removed on a read failure.
    void load_stored_credentials();
    /// Plain sentence when the stored keys exist but could not be read (the session
    /// then has no keys — that is not "no API key"); empty otherwise. Any thread.
    QString credentials_read_error() const;

    // ── WS lifecycle ───────────────────────────────────────────────────────
    /// Returns true if the WS subprocess was spawned; see
    /// ExchangeService::start_ws_stream for full semantics.
    /// `hub_owned` marks a stream launched on behalf of DataHub subscribers
    /// (ExchangeSessionManager) rather than by a screen; screens never pass it.
    bool start_ws(const QString& primary_symbol, const QStringList& all_symbols, bool hub_owned = false);
    void stop_ws();
    /// True when the running WS subprocess was launched by the DataHub demand path
    /// (and may therefore be stopped by it once no subscriber remains).
    bool is_ws_hub_owned() const { return ws_hub_owned_; }
    /// Symbols the current / most recent WS launch was asked to stream (a copy).
    /// Main thread only, like start_ws().
    QStringList ws_symbols() const { return ws_all_symbols_; }
    /// Pairs DataHub subscribers need streamed on this exchange. start_ws() folds
    /// them into every launch list, so a screen (re)starting the stream does not
    /// silently drop the tiles that are subscribed to `ws:<exchange>:*` topics.
    /// Main thread only.
    void set_hub_pairs(const QStringList& pairs) { hub_pairs_ = pairs; }
    bool is_ws_connected() const { return ws_connected_.load(); }
    /// True once a WS subprocess has been spawned and has not yet exited.
    /// Distinct from `is_ws_connected` — the process may be up but the remote
    /// handshake still pending. Used by the session manager to avoid
    /// re-spawning when switching back to a session that's already live.
    bool is_ws_active() const;
    /// True between a successful start_ws() and stop_ws(): the stream is meant
    /// to be up, so `!is_ws_active()` while this holds is a dropped stream (a
    /// respawn may be pending). Main thread only.
    bool is_ws_wanted() const { return ws_should_run_; }
    void set_ws_primary_symbol(const QString& symbol);
    /// Re-point only the OHLC stream to a new chart timeframe (no restart).
    void set_ws_timeframe(const QString& timeframe);
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
    /// The market an order for `symbol` would be sent to (the daemon resolves the
    /// app's pair the same way place_order does, e.g. Hyperliquid "BTC/USDT" ->
    /// "BTC/USDC:USDC"), with its fees and min amount / min cost. Empty `symbol`
    /// in the result when there is none or the call failed.
    MarketInfo fetch_market(const QString& symbol);
    QVector<TradeData> fetch_trades(const QString& symbol, int limit = 50);
    FundingRateData fetch_funding_rate(const QString& symbol);
    OpenInterestData fetch_open_interest(const QString& symbol);
    MarkPriceData fetch_mark_price(const QString& symbol);

    // ── Any daemon method on THIS exchange (worker thread only; blocks up to
    //    timeout_ms). The session is chosen by the caller — never a global
    //    "current exchange" — so an order goes to the venue it was confirmed for.
    QJsonObject request(const QString& method, const QJsonObject& args, int timeout_ms = 15000);
    /// Same, with the credentials captured when the user confirmed (an order /
    /// cancel / leverage change must go to the account the dialog named, even
    /// if the keys were edited meanwhile; the daemon refuses an order whose
    /// confirmed account_id no longer matches).
    QJsonObject request_as(const QString& method, const QJsonObject& args, const ExchangeCredentials& creds,
                           int timeout_ms = 15000);
    /// Identity of the stored credentials (ExchangeDaemonPool::credential_fingerprint).
    QString account_id() const;
    /// Non-secret label of the account ("API key ab12…ef90", "wallet 0x1111…1111").
    QString account_label() const;

    // ── Authenticated — need credentials on the daemon.
    QJsonObject fetch_balance();
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

  signals:
    /// The WS stream changed state: subprocess spawned, remote handshake
    /// connected / dropped, process exited, or stopped. No payload — re-read
    /// is_ws_active() / is_ws_connected(). Main thread. Not emitted from the
    /// destructor.
    void ws_state_changed(const QString& exchange_id);
    /// The WS stream is down and will not come back by itself: the subprocess
    /// could not be started, exited for a reason a respawn cannot fix (streaming
    /// library not installed, exchange unknown to it), or used up its respawn
    /// budget. `message` is plain wording for the user. Main thread.
    void ws_failed(const QString& exchange_id, const QString& message);

  private:
    void handle_ws_line(const QString& line);
    void drain_ws_buffer();

    /// QProcess::finished handler for the WS subprocess. Clears the connected
    /// flag (so the polled UI badge drops to OFFLINE) and — if the stream was
    /// still supposed to be up — schedules a bounded, backed-off respawn.
    void handle_ws_finished(int exit_code, QProcess::ExitStatus status);
    /// User wording for why the stream is down, from the last process-level error.
    QString ws_failure_text() const;

    /// Every symbol an update for `exchange_symbol` is published under: the
    /// requested symbols shown from that market (e.g. on Hyperliquid
    /// `BTC/USDC:USDC` → `BTC/USDT`) and the market's own symbol. Thread-safe.
    QStringList publish_symbols(const QString& exchange_symbol) const;

    QJsonObject daemon_call(const QString& method, const QJsonObject& args, int timeout_ms = 15000);

    static TickerData parse_ticker(const QJsonObject& j);
    static OrderBookData parse_orderbook(const QJsonObject& j);
    static Candle parse_candle(const QJsonObject& j);
    static MarketInfo parse_market(const QJsonObject& j);

    const QString exchange_id_;
    SessionPublisher publisher_;

    ExchangeCredentials credentials_;
    QString credentials_read_error_; // guarded by mutex_
    int cred_read_retries_ = 0;      // automatic re-reads after a temporary read failure

    QProcess* ws_process_ = nullptr;
    std::atomic<bool> ws_connected_{false};
    bool ws_hub_owned_ = false;   // current subprocess was started by the DataHub demand path
    QStringList hub_pairs_;       // pairs DataHub subscribers need (folded into every launch)
    bool ws_should_run_ = false;  // true between a successful start_ws() and stop_ws()
    int ws_restart_attempts_ = 0; // consecutive auto-restarts; reset after a healthy run
    QElapsedTimer ws_uptime_;     // since the current ws_process_ was spawned
    QString ws_last_error_;       // last process-level {"type":"error"} line of the current subprocess
    bool ws_error_fatal_ = false; // that error cannot be fixed by a respawn (missing library, unknown exchange)
    QString ws_primary_symbol_;
    QStringList ws_all_symbols_;
    QHash<QString, QStringList> ws_symbol_map_; // exchange_symbol → requested symbols shown from it

    QHash<QString, TickerData> price_cache_;
    QHash<QString, QSet<QString>> watched_; // symbol → set of portfolio_ids

    mutable QMutex mutex_;
};

} // namespace fincept::trading
