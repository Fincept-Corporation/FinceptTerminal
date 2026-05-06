#include "trading/exchanges/hyperliquid/HyperliquidVenue.h"

#include "core/logging/Logger.h"
#include "trading/exchanges/hyperliquid/HyperliquidSigner.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace fincept::trading::hyperliquid {

using fincept::services::alpha_arena::FillEvent;
using fincept::services::alpha_arena::FundingEvent;
using fincept::services::alpha_arena::IExchangeVenue;
using fincept::services::alpha_arena::LiquidationEvent;
using fincept::services::alpha_arena::OrderAck;
using fincept::services::alpha_arena::OrderRequest;

HyperliquidVenue::HyperliquidVenue(QObject* parent) : QObject(parent) {
    client_ = new HyperliquidClient(this);
    reconcile_timer_ = new QTimer(this);
    reconcile_timer_->setInterval(30 * 1000);
    // Class declares its own connect() method which shadows QObject::connect —
    // qualify here so the lookups don't pick up the venue method.
    QObject::connect(reconcile_timer_, &QTimer::timeout, this, &HyperliquidVenue::reconcile_tick);
    QObject::connect(client_, &HyperliquidClient::ws_message, this, &HyperliquidVenue::on_ws_message);
    QObject::connect(client_, &HyperliquidClient::ws_state_changed, this, &HyperliquidVenue::on_ws_state_changed);
}

HyperliquidVenue::~HyperliquidVenue() = default;

void HyperliquidVenue::set_testnet(bool testnet) { client_->set_testnet(testnet); }
void HyperliquidVenue::set_user_address(const QString& addr) { user_address_ = addr; }

void HyperliquidVenue::connect() {
    state_ = ConnectionState::Connecting;
    client_->connect_ws();
    // Subscribe to BTC/ETH/SOL mid-price stream as a default. The engine can
    // re-subscribe to whatever instruments the active competition needs.
    for (const auto& coin : QStringList{"BTC", "ETH", "SOL", "BNB", "DOGE", "XRP"}) {
        QJsonObject sub;
        sub[QStringLiteral("type")] = QStringLiteral("activeAssetCtx");
        sub[QStringLiteral("coin")] = coin;
        client_->subscribe(sub);
    }
    reconcile_timer_->start();
}

void HyperliquidVenue::on_ws_state_changed(int state) {
    switch (static_cast<QAbstractSocket::SocketState>(state)) {
    case QAbstractSocket::ConnectedState:    state_ = ConnectionState::Connected; break;
    case QAbstractSocket::ConnectingState:   state_ = ConnectionState::Connecting; break;
    case QAbstractSocket::UnconnectedState:  state_ = ConnectionState::Disconnected; break;
    default:                                 state_ = ConnectionState::Degraded; break;
    }
}

void HyperliquidVenue::on_ws_message(QJsonObject msg) {
    // HL message envelope: { "channel": "<ch>", "data": <payload> }
    const QString channel = msg.value(QStringLiteral("channel")).toString();
    const QJsonObject data = msg.value(QStringLiteral("data")).toObject();
    if (channel == QLatin1String("activeAssetCtx")) {
        const QString coin = data.value(QStringLiteral("coin")).toString();
        const auto ctx = data.value(QStringLiteral("ctx")).toObject();
        const double mid = ctx.value(QStringLiteral("midPx")).toString().toDouble();
        if (!coin.isEmpty() && mid > 0.0) {
            marks_.insert(coin, mid);
            if (mark_update_cb_) mark_update_cb_(coin, mid, QDateTime::currentMSecsSinceEpoch());
        }
    } else if (channel == QLatin1String("userEvents")) {
        // Dispatch fills / liquidations off `data` here once we have a user
        // subscription with signed authentication.
    }
}

void HyperliquidVenue::reconcile_tick() {
    if (user_address_.isEmpty()) return;
    QJsonObject body;
    body[QStringLiteral("type")] = QStringLiteral("clearinghouseState");
    body[QStringLiteral("user")] = user_address_;
    client_->info(body, [this](Result<QJsonDocument> r) {
        if (r.is_err()) {
            LOG_WARN("Hyperliquid", QString("reconcile failed: %1").arg(QString::fromStdString(r.error())));
            return;
        }
        // For each remote position, compare against local. Emit position_drift
        // on mismatch. (Full impl pending — engine-side persistence has the
        // local view; we surface it once HyperliquidVenue is wired into the
        // engine in a follow-up patch.)
    });
}

void HyperliquidVenue::place_order(const OrderRequest& req,
                                    std::function<void(OrderAck)> ack_cb) {
    if (!HyperliquidSigner::is_wired()) {
        OrderAck a;
        a.status = QStringLiteral("rejected");
        a.error = QStringLiteral("hl_signer_not_wired");
        if (ack_cb) ack_cb(a);
        return;
    }

    // Action body — Hyperliquid `order` action shape.
    QJsonObject order;
    order[QStringLiteral("a")] = 0;                                // asset id; populated from meta cache (TBD)
    order[QStringLiteral("b")] = (req.side == QLatin1String("buy"));
    order[QStringLiteral("p")] = QStringLiteral("0");              // 0 means market for IOC
    order[QStringLiteral("s")] = QString::number(req.qty, 'f', 8);
    order[QStringLiteral("r")] = req.reduce_only;
    QJsonObject t;
    t[QStringLiteral("limit")] = QJsonObject{{"tif", "Ioc"}};
    order[QStringLiteral("t")] = t;

    QJsonArray orders; orders.append(order);

    QJsonObject action;
    action[QStringLiteral("type")] = QStringLiteral("order");
    action[QStringLiteral("orders")] = orders;
    action[QStringLiteral("grouping")] = QStringLiteral("na");

    // Pull agent priv key from SecureStorage handle. We expect the engine to
    // have stored it under `alpha_arena/agent_key/<comp_id>`; the OrderRouter
    // resolves the handle and passes it here. Until the wallet wiring lands
    // on this codepath we surface a clear error.
    Q_UNUSED(req);
    OrderAck a;
    a.status = QStringLiteral("rejected");
    a.error = QStringLiteral("hl_live_path_not_yet_wired");
    if (ack_cb) ack_cb(a);
}

void HyperliquidVenue::cancel_order(const QString& venue_order_id) {
    Q_UNUSED(venue_order_id);
    // Cancel path mirrors place_order — gated until the signer is wired.
}

double HyperliquidVenue::last_mark(const QString& coin) const {
    auto it = marks_.constFind(coin);
    return (it != marks_.constEnd()) ? it.value() : std::nan("");
}

} // namespace fincept::trading::hyperliquid
