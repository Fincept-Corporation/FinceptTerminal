#pragma once

#include "services/polymarket/PolymarketTypes.h"

#    include "datahub/Producer.h"

#include <QObject>
#include <QSet>
#include <QTimer>

namespace fincept {
class WebSocketClient;
}

namespace fincept::services::polymarket {

/// Singleton WebSocket client for real-time Polymarket market data.
/// Connects to wss://ws-subscriptions-clob.polymarket.com/ws/market
///
/// Phase 4 — DataHub producer for `polymarket:price:*` and
/// `polymarket:orderbook:*`. The existing `price_updated` /
/// `orderbook_updated` signals are kept alive alongside hub publishes
/// so consumers can migrate incrementally.
class PolymarketWebSocket : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static PolymarketWebSocket& instance();

    void subscribe(const QStringList& token_ids);
    void unsubscribe(const QStringList& token_ids);
    void unsubscribe_all();
    void disconnect();
    bool is_connected() const;

    /// Register with the hub + install polymarket:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;   // no-op: push_only
    int max_requests_per_sec() const override;          // 10 (CLOB REST cap)

  signals:
    void price_updated(const QString& asset_id, double price);
    void orderbook_updated(const QString& asset_id, const OrderBook& book);
    void connection_status_changed(bool connected);

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& msg);
    void on_ws_error(const QString& error);
    void send_ping();

  private:
    PolymarketWebSocket();
    void ensure_connected();
    void send_subscribe(const QStringList& token_ids);

    fincept::WebSocketClient* ws_ = nullptr;
    QTimer* ping_timer_ = nullptr;
    QSet<QString> subscribed_tokens_;
    bool connected_ = false;

    static constexpr const char* WS_URL = "wss://ws-subscriptions-clob.polymarket.com/ws/market";

    void publish_price_to_hub(const QString& asset_id, double price);
    void publish_orderbook_to_hub(const QString& asset_id, const OrderBook& book);
    bool hub_registered_ = false;
};

} // namespace fincept::services::polymarket
