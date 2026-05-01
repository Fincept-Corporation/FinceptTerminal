#pragma once
// Native Kraken WebSocket v2 client.
//
// Replaces the Python ws_stream.py path for `exchange_id == "kraken"`. The
// public market-data endpoint is `wss://ws.kraken.com/v2`. We subscribe to
// four channels per symbol — ticker, book, trade, ohlc(1m) — and emit the
// terminal's existing TradingTypes structs via signals.
//
// Lifecycle (driven by ExchangeSession::start_ws → start()):
//   1. open()        connect to ws.kraken.com/v2
//   2. on connect    send 4 subscribe messages (one per channel)
//   3. for each ack  log success/failure; on per-symbol failure for /USDT
//                    pairs, retry once with the /USD fallback
//   4. on each frame parse, dispatch by `channel`, emit
//   5. heartbeat watchdog: any inbound frame resets a 10s timer; on timeout
//                    we close + reconnect (Kraken sends heartbeats ~1Hz idle)
//   6. on close      WebSocketClient handles exponential backoff reconnect
//
// Threading: lives on the thread that owns it. All Qt signals fire there.
//
// Authenticated channels (executions, balances) are NOT handled here — they
// require a token from REST GetWebSocketsToken on a different endpoint
// (wss://ws-auth.kraken.com/v2). Those stay on the daemon for now.

#include "trading/TradingTypes.h"
#include "trading/exchanges/kraken/KrakenBook.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <atomic>

class QThread;

namespace fincept {
class WebSocketClient;
}

namespace fincept::trading::kraken {

class KrakenWsClient : public QObject {
    Q_OBJECT
  public:
    explicit KrakenWsClient(QObject* parent = nullptr);
    ~KrakenWsClient() override;

    /// Designate the dedicated I/O thread the client will move to once the
    /// initial ccxt symbol resolve finishes. Must be called BEFORE start();
    /// the thread must already be running. Owned by the caller.
    void set_io_thread(QThread* io_thread) { io_thread_ = io_thread; }

    /// Open the connection and subscribe to all symbols. `primary_symbol` is
    /// the one the user is actively viewing (gets ohlc + trades + book);
    /// every symbol in `all_symbols` (including primary) gets ticker.
    /// Idempotent — a second call closes the existing connection first.
    /// MUST be called from the thread that owns PythonRunner (main) — the
    /// resolver runs synchronously there before we cross over to the I/O
    /// thread for the actual WebSocket open.
    void start(const QString& primary_symbol, const QStringList& all_symbols);

    /// Close the connection cleanly. Cancels reconnect attempts.
    void stop();

    /// Swap which symbol is "primary" without tearing down the whole feed.
    void set_primary_symbol(const QString& symbol);

    /// True once the WS handshake is up.
    bool is_connected() const { return connected_.load(); }

  signals:
    void ticker_received(const fincept::trading::TickerData& ticker);
    void orderbook_received(const fincept::trading::OrderBookData& orderbook);
    void trade_received(const fincept::trading::TradeData& trade);
    void candle_received(const QString& symbol, const QString& interval,
                         const fincept::trading::Candle& candle);

    /// Connection-state edge transitions.
    void connection_changed(bool connected);

    /// Forwarded subscribe-ack failures, after USD-fallback retry has been
    /// exhausted. Useful for surfacing "WIF/USDT not on Kraken" to the UI.
    void subscribe_failed(const QString& channel, const QString& symbol,
                          const QString& error);

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& message);
    void on_ws_error(const QString& error);
    void on_heartbeat_timeout();

  private:
    /// Run the ccxt-based resolver script and populate the bidirectional
    /// fincept↔kraken symbol maps. On completion connects the WebSocket and
    /// fires send_initial_subscriptions(). Falls back to a passthrough map
    /// if the script fails (matches the behaviour we had before).
    void resolve_symbols_then_connect();
    void send_initial_subscriptions();
    /// `event_trigger` is one of "trades" (default — fires only on a print) or
    /// "bbo" (fires on every best bid/offer change). The watchlist needs "bbo"
    /// so illiquid pairs (e.g. APT/USD, INJ/USD) tick on quote moves rather
    /// than going minutes between updates.
    void send_subscribe(const QString& channel, const QStringList& kraken_symbols,
                        int interval_minutes = 0,
                        const QString& event_trigger = QString());
    void send_unsubscribe(const QString& channel, const QStringList& kraken_symbols,
                          int interval_minutes = 0);

    void handle_message(const QJsonObject& msg);
    void handle_ticker(const QJsonArray& data);
    void handle_book(const QJsonArray& data, bool is_snapshot);
    void handle_trade(const QJsonArray& data);
    void handle_ohlc(const QJsonArray& data);
    void handle_subscribe_ack(const QJsonObject& msg);
    void handle_status(const QJsonObject& msg);

    void register_symbol(const QString& fincept_symbol, const QString& kraken_symbol);
    QString resolve_to_fincept(const QString& kraken_symbol) const;

    bool maybe_retry_with_usd(const QString& channel, const QString& failed_kraken_symbol,
                              const QString& error);

    /// RFC3339 string (e.g. "2023-09-25T09:04:31.742648Z") → ms since epoch.
    static int64_t parse_iso_to_ms(const QString& iso);

    fincept::WebSocketClient* ws_ = nullptr;
    QTimer* heartbeat_timer_ = nullptr;
    QThread* io_thread_ = nullptr;  // designated I/O thread; not owned

    QString primary_symbol_;
    QStringList all_symbols_;
    QHash<QString, QString> kraken_to_fincept_;
    QHash<QString, QString> fincept_to_kraken_;
    QSet<QString> usd_fallback_attempted_;

    KrakenBook book_;
    std::atomic<bool> connected_{false};

    // Per-symbol message counters flushed to LOG_INFO every 10s. Lets us
    // diagnose "this row is frozen" complaints without per-tick log spam.
    QHash<QString, int> ticker_count_since_log_;
    QTimer* stats_timer_ = nullptr;
    void emit_stats_log();

    static constexpr int HEARTBEAT_TIMEOUT_MS = 10000;
    static constexpr int STATS_LOG_INTERVAL_MS = 10000;
};

} // namespace fincept::trading::kraken
