#pragma once
// BrokerWebSocketBase — shared infrastructure for broker WebSocket adapters.
//
// WebSocketClient (network/websocket) already provides QWebSocket wrapping and
// auto-reconnect. This base adds the broker-streaming concerns that are common
// across adapters: partial-tick merging (preserve last non-zero OHLC fields),
// subscribe debounce (coalesce rapid subscribe calls into one batch), and a
// data-stall health check (force reconnect if no ticks for N seconds).
//
// Concrete adapters subclass this, hold a WebSocketClient*, and implement the
// broker-specific wire protocol. They emit tick_received / depth_received.

#include "trading/TradingTypes.h"

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QVector>
#include <QDateTime>

namespace fincept::trading {

class BrokerWebSocketBase : public QObject {
    Q_OBJECT
  public:
    explicit BrokerWebSocketBase(QObject* parent = nullptr);
    ~BrokerWebSocketBase() override = default;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool is_connected() const = 0;

    // Token-based subscribe (Zerodha-style integer tokens). Symbol-based
    // adapters expose their own overloads alongside these.
    virtual void subscribe(const QVector<qint64>& tokens) = 0;
    virtual void unsubscribe() = 0;

  signals:
    void tick_received(const fincept::trading::BrokerQuote& quote);
    void depth_received(const fincept::trading::MarketDepth& depth);
    void connected();
    void disconnected();
    void error_occurred(const QString& error);

  protected:
    // --- Partial tick merging ---
    // Many brokers send incremental ticks where unchanged fields arrive as 0.
    // merge_tick caches the last full tick per key and overlays only non-zero
    // fields so downstream consumers always see a complete quote.
    BrokerQuote merge_tick(const QString& key, const BrokerQuote& partial);
    void clear_tick_cache();

    // --- Subscribe debounce ---
    // Collect tokens for `debounce_ms` then flush as a single batch. Subclass
    // implements flush_subscribe_queue() to send the actual wire message.
    void enqueue_subscribe(const QVector<qint64>& tokens);
    virtual void flush_subscribe_queue() {}
    QVector<qint64> take_subscribe_queue();

    // --- Health check (data stall detection) ---
    // Call note_tick() on every inbound tick. start_health_check arms a timer
    // that calls on_data_stall() (default: emit error + nothing) when no tick
    // has arrived within timeout_ms. Subclass overrides on_data_stall() to
    // force a reconnect.
    void note_tick();
    void start_health_check(int interval_ms = 30000, int timeout_ms = 90000);
    void stop_health_check();
    virtual void on_data_stall() {}

  private:
    QHash<QString, BrokerQuote> tick_cache_;

    QTimer subscribe_debounce_;
    QVector<qint64> subscribe_queue_;
    static constexpr int kSubscribeDebounceMs = 500;

    QTimer health_timer_;
    qint64 last_tick_ms_ = 0;
    int health_timeout_ms_ = 90000;
};

} // namespace fincept::trading
