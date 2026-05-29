#pragma once
// UpstoxWebSocket — Upstox V3 market-data streaming adapter (Protobuf3 over WS).
//
// Port of OpenAlgo's upstox_websocket.py + upstox_adapter.py to the Fincept
// BrokerWebSocketBase contract. Unlike the JSON/binary-struct adapters, Upstox
// delivers ticks as binary Protobuf3 (MarketDataFeedV3.FeedResponse) frames.
//
// Protocol summary:
//   1. The WS URL is dynamic. open() does a REST authorize call:
//        GET https://api.upstox.com/v3/feed/market-data-feed/authorize
//        Authorization: Bearer <access_token>
//      The response carries data.authorized_redirect_uri — the pre-authenticated
//      wss:// endpoint. We do this blocking REST call on a worker thread
//      (QtConcurrent + QPointer guard) and connect on the UI thread.
//   2. Subscribe by sending a *binary* frame whose payload is the JSON bytes:
//        {"guid":"<20 hex chars>","method":"sub",
//         "data":{"instrumentKeys":["NSE_EQ|INE002A01018"],"mode":"ltpc"|"full"}}
//      Upstox expects the JSON as a binary WS frame (not a text frame).
//   3. Ticks arrive as binary Protobuf FeedResponse. `feeds` is a
//        map<string instrumentKey, Feed>. Each Feed carries either an LTPC
//        (mode=ltpc) or a fullFeed.marketFF (mode=full) with OHLC, volume,
//        avg/atp, OI and a 5-level bid/ask depth ladder.
//
// Subscriptions are keyed by Upstox instrument key strings ("SEGMENT|TOKEN").
// The token-based subscribe(QVector<qint64>) override resolves each token to an
// instrument key via InstrumentService (matching UpstoxBroker's mapping); the
// native subscribe(QStringList) takes instrument keys directly.
//
// Protobuf parsing is hand-rolled (no protobuf library is linked into the
// project) — see the anonymous-namespace ProtoReader in the .cpp. Field numbers
// are taken from MarketDataFeedV3.proto.

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept {

class WebSocketClient;

namespace trading {

class UpstoxWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// @param access_token  Upstox V3 OAuth access token (Bearer).
    explicit UpstoxWebSocket(const QString& access_token, QObject* parent = nullptr);
    ~UpstoxWebSocket() override;

    void open() override;
    void close() override;
    bool is_connected() const override;

    /// Native subscribe — instrument keys are "SEGMENT|TOKEN" strings
    /// (e.g. "NSE_EQ|INE002A01018"). `full` mode also yields OHLC + depth;
    /// `ltpc` mode yields only last-traded-price + close. Deduplicates.
    void subscribe(const QStringList& instrument_keys, const QString& mode = QStringLiteral("full"));

    /// Token-based subscribe (BrokerWebSocketBase contract). Tokens are mapped
    /// to instrument keys via InstrumentService; subscribes in "full" mode.
    void subscribe(const QVector<qint64>& tokens) override;

    /// Unsubscribe everything currently subscribed.
    void unsubscribe() override;

    /// Remove a specific set of instrument keys.
    void unsubscribe(const QStringList& instrument_keys);

  protected:
    void on_data_stall() override;

  private slots:
    void on_connected();
    void on_binary_message(const QByteArray& data);
    void on_text_message(const QString& msg);
    void on_disconnected();

  private:
    // Resolve the dynamic WS URL via the REST authorize endpoint (blocking —
    // call on a worker thread). Returns empty on failure.
    QString fetch_ws_url() const;

    // Wire senders. method is "sub" / "unsub". The JSON payload is sent as a
    // binary frame (Upstox requirement).
    void send_subscription(const QStringList& instrument_keys, const QString& method,
                           const QString& mode);
    void resubscribe_all();

    // Protobuf FeedResponse → tick/depth signals.
    void parse_feed_response(const QByteArray& data);
    // Map an Upstox instrument key ("SEGMENT|TOKEN") to a display symbol +
    // canonical exchange via InstrumentService (falls back to the key itself).
    void enrich_symbol(const QString& instrument_key, QString& symbol, QString& exchange) const;

    QString access_token_;
    QString ws_url_;

    WebSocketClient* ws_ = nullptr;
    bool connecting_ = false;

    // instrument_key → requested mode ("ltpc" | "full"). Drives replay on
    // reconnect and groups bulk subscribe messages by mode.
    QHash<QString, QString> subscriptions_;

    static constexpr const char* kAuthEndpoint =
        "https://api.upstox.com/v3/feed/market-data-feed/authorize";
    static constexpr int kSubscribeBatch = 100; // instrument keys per sub frame
};

} // namespace trading
} // namespace fincept
