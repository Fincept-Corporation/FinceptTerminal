#pragma once
// FivePaisaWebSocket — 5Paisa market-data streaming adapter (JSON protocol).
//
// Port of OpenAlgo's fivepaisa_websocket.py + fivepaisa_adapter.py to the
// Fincept BrokerWebSocketBase contract. Unlike Zerodha/Angel One (binary), the
// 5Paisa feed is plain JSON over a standard WebSocket.
//
// Protocol summary:
//   1. WS URL is selected from the "RedirectServer" claim in the JWT access
//      token (A/B/C/default → distinct host). Connection URL:
//        wss://<host>/feeds/api/chat?Value1=<access_token>|<client_code>
//   2. Subscribe by sending a JSON envelope:
//        { "Method":"MarketFeedV3", "Operation":"Subscribe",
//          "ClientCode":"<code>", "MarketFeedData":[ {Exch,ExchType,ScripCode} ] }
//   3. Ticks arrive as a JSON object (or array of objects) carrying Token,
//      LastRate (ltp), OpenRate, High, Low, PClose, TotalQty (volume),
//      BidRate, OffRate (ask), AvgRate, plus 5-level depth under "Details".
//   4. OHLC carryforward: 5Paisa often sends 0 for unchanged fields, so we
//      route every tick through merge_tick() to preserve last non-zero values.
//
// Subscription is keyed by numeric ScripCode (= instrument_token). The Exch /
// ExchType wire codes are resolved per-token from InstrumentService (matching
// FivePaisaBroker's fp_exchange / fp_exchange_type mapping).

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace fincept {

class WebSocketClient;

namespace trading {

class FivePaisaWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// access_token: JWT login token (also encodes the redirect server).
    /// client_code:  demat client code (the third ":::"-delimited part of the
    ///               packed ApiKey, falling back to user_id when absent).
    explicit FivePaisaWebSocket(const QString& access_token, const QString& client_code, QObject* parent = nullptr);

    void open() override;
    void close() override;
    bool is_connected() const override;

    /// Subscribe by numeric scrip code (= instrument_token). Exchange code/type
    /// is resolved per token via InstrumentService. Deduplicates internally.
    void subscribe(const QVector<qint64>& tokens) override;

    /// Unsubscribe everything currently subscribed.
    void unsubscribe() override;

    /// Remove a specific set of scrip codes.
    void unsubscribe(const QVector<qint64>& tokens);

  protected:
    void flush_subscribe_queue() override;
    void on_data_stall() override;

  private slots:
    void on_connected();
    void on_text_message(const QString& msg);
    void on_disconnected();

  private:
    // Resolve the host from the JWT "RedirectServer" claim, then build the full
    // connection URL with the Value1 auth query parameter.
    static QString feed_url_for_token(const QString& access_token);
    static QString decode_redirect_server(const QString& access_token);

    // Map a Fincept exchange string to 5Paisa wire codes (matches FivePaisaBroker).
    static QString fp_exchange(const QString& exchange);      // N / B / M
    static QString fp_exchange_type(const QString& exchange); // C / D / U

    // Parse the Microsoft "/Date(1690000000000)/" format → epoch ms (0 on failure).
    static int64_t parse_fp_time(const QString& s);

    void send_subscribe(const QVector<qint64>& tokens, bool subscribe);
    void resubscribe_all();
    void handle_tick(const QJsonObject& obj);

    QString access_token_;
    QString client_code_;

    WebSocketClient* ws_;
    QSet<qint64> subscribed_tokens_;

    static constexpr const char* kMethodMarketFeed = "MarketFeedV3";
    static constexpr const char* kMethodMarketDepth = "MarketDepthService";
};

} // namespace trading
} // namespace fincept
