#pragma once

#include "datahub/Producer.h"
#include "services/prediction/PredictionTypes.h"
#include "services/prediction/kalshi/KalshiCredentials.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept {
class WebSocketClient;
}

namespace fincept::services::prediction::kalshi_ns {

/// Kalshi v2 WebSocket client.
///
///   URL : wss://api.elections.kalshi.com/trade-api/ws/v2
///
/// Unlike Polymarket, Kalshi's WS requires an authenticated connection
/// even for public market data (orderbook_delta, ticker, trade channels).
/// Connection is deferred until KalshiCredentials are supplied; without
/// credentials this client silently no-ops and screens fall back to REST
/// polling.
///
/// Hub producer for:
///   prediction:kalshi:price:<ticker>:<side>       (push-only)
///   prediction:kalshi:orderbook:<ticker>:<side>   (push-only)
///
/// Wire protocol (subscribe):
///   {"id":1,"cmd":"subscribe","params":{
///       "channels":["orderbook_delta","ticker"],
///       "market_tickers":["<ticker>", …]
///   }}
///
/// Delta reconciliation: Kalshi emits `orderbook_snapshot` once per
/// subscription, then incremental `orderbook_delta`. Messages include a
/// monotonic `seq`; on gap we re-request a snapshot via REST.
class KalshiWsClient : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit KalshiWsClient(QObject* parent = nullptr);
    ~KalshiWsClient() override;

    /// Update / clear credentials. Passing an invalid set disconnects.
    void set_credentials(const KalshiCredentials& creds);
    bool has_credentials() const;

    void subscribe(const QStringList& market_tickers);
    void unsubscribe(const QStringList& market_tickers);
    void unsubscribe_all();
    void disconnect();
    bool is_connected() const { return connected_; }

    /// Register with the hub + install prediction:kalshi:* policies.
    void ensure_registered_with_hub();

    // datahub::Producer
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  signals:
    void price_updated(const QString& asset_id, double price);
    void orderbook_updated(const QString& asset_id,
                           const fincept::services::prediction::PredictionOrderBook& book);
    void trade_received(const fincept::services::prediction::PredictionTrade& trade);
    void market_lifecycle_changed(const QString& ticker, const QString& status);
    void connection_status_changed(bool connected);

  private slots:
    void on_connected();
    void on_disconnected();
    void on_message(const QString& msg);
    void on_error(const QString& err);
    void send_ping();

  private:
    void ensure_connected();
    void send_subscribe(const QStringList& tickers);
    void publish_price(const QString& asset_id, double price);
    void publish_orderbook(const QString& asset_id,
                           const fincept::services::prediction::PredictionOrderBook& book);

    fincept::WebSocketClient* ws_ = nullptr;
    QTimer* ping_timer_ = nullptr;

    KalshiCredentials creds_;
    QSet<QString> subscribed_tickers_;
    bool connected_ = false;
    bool hub_registered_ = false;
    int next_msg_id_ = 1;

    static constexpr const char* kProdWs = "wss://api.elections.kalshi.com/trade-api/ws/v2";
    static constexpr const char* kDemoWs = "wss://demo-api.kalshi.co/trade-api/ws/v2";
};

} // namespace fincept::services::prediction::kalshi_ns
