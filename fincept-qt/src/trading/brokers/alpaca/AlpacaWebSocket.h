#pragma once
// Alpaca Data API v2 WebSocket client for real-time US equity market data.
//
// Protocol: wss://stream.data.alpaca.markets/v2/{feed}
//   - JSON-based auth: {"action":"auth","key":"...","secret":"..."}
//   - Subscribe: {"action":"subscribe","trades":["AAPL"],"quotes":["AAPL"],"bars":["AAPL"]}
//   - Messages arrive as JSON arrays: [{"T":"t",...},{"T":"q",...}]
//   - Must authenticate within 10s of connect
//   - Connection limit: 1 per endpoint per user
//
// Market-hours aware: checks AlpacaBroker::get_clock() on init and periodically.
// Connects only when the market is open (or within 15min of open). Disconnects
// gracefully after market close.

#include "trading/TradingTypes.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <atomic>

namespace fincept {
class WebSocketClient;
}

namespace fincept::trading {

class AlpacaWebSocket : public QObject {
    Q_OBJECT
  public:
    explicit AlpacaWebSocket(const QString& api_key, const QString& api_secret,
                             QObject* parent = nullptr);
    ~AlpacaWebSocket() override;

    void open();
    void close();
    bool is_connected() const { return connected_.load(); }

    void subscribe(const QStringList& symbols);
    void unsubscribe(const QStringList& symbols);
    void set_subscriptions(const QStringList& symbols);

  signals:
    void tick_received(const fincept::trading::BrokerQuote& quote);
    void trade_received(const fincept::trading::BrokerTrade& trade);
    void bar_received(const QString& symbol, const fincept::trading::BrokerCandle& candle);
    void connected();
    void disconnected();
    void error_occurred(const QString& error);
    void market_closed();

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& message);
    void on_ws_error(const QString& error);
    void on_heartbeat_timeout();
    void on_clock_check();

  private:
    void send_auth();
    void send_subscribe(const QStringList& symbols);
    void send_unsubscribe(const QStringList& symbols);

    void handle_message(const QJsonObject& msg);
    void handle_trade(const QJsonObject& msg);
    void handle_quote(const QJsonObject& msg);
    void handle_bar(const QJsonObject& msg);
    void handle_success(const QJsonObject& msg);
    void handle_error(const QJsonObject& msg);
    void handle_subscription(const QJsonObject& msg);

    static int64_t parse_rfc3339_to_ms(const QString& ts);

    QString api_key_;
    QString api_secret_;

    fincept::WebSocketClient* ws_ = nullptr;
    QTimer* heartbeat_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;

    QStringList subscribed_symbols_;
    std::atomic<bool> connected_{false};
    bool authenticated_ = false;

    static constexpr int HEARTBEAT_TIMEOUT_MS = 15000;
    static constexpr int CLOCK_CHECK_INTERVAL_MS = 60000;
    static constexpr const char* kWsUrl = "wss://stream.data.alpaca.markets/v2/iex";
    static constexpr const char* kTag = "AlpacaWS";
};

} // namespace fincept::trading
