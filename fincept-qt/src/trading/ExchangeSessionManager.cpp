#include "trading/ExchangeSessionManager.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

#include <QMutexLocker>
#include <QVariant>

namespace fincept::trading {

namespace {
const QString kMgrTag = "ExchangeSessionManager";

// Exchanges whose meta-types are registered with DataHub. Other exchanges
// still work through the session (callbacks + direct consumer reads) — they
// just aren't published on the hub yet. Add here as metatypes land.
bool hub_supported_exchange(const QString& id) {
    return id == QLatin1String("kraken") || id == QLatin1String("hyperliquid");
}
} // namespace

ExchangeSessionManager& ExchangeSessionManager::instance() {
    static ExchangeSessionManager s;
    return s;
}

ExchangeSessionManager::ExchangeSessionManager() = default;

ExchangeSessionManager::~ExchangeSessionManager() {
    // Sessions are owned via Qt parent/child (this). Qt would destroy them
    // automatically at manager destruction, but call their WS teardown now
    // so the Python subprocesses exit cleanly before Qt tears everything down.
    QMutexLocker lock(&mutex_);
    qDeleteAll(sessions_);
    sessions_.clear();
}

ExchangeSession* ExchangeSessionManager::session(const QString& exchange_id) {
    QMutexLocker lock(&mutex_);
    auto it = sessions_.find(exchange_id);
    if (it != sessions_.end())
        return it.value();

    // First touch for this exchange — construct a session with a publisher
    // that routes its fan-out to DataHub via the manager's policies.
    auto* s = new ExchangeSession(exchange_id, build_publisher(), this);
    sessions_.insert(exchange_id, s);
    LOG_INFO(kMgrTag, "Created ExchangeSession for " + exchange_id);
    return s;
}

QStringList ExchangeSessionManager::active_exchange_ids() const {
    QMutexLocker lock(&mutex_);
    QStringList ids;
    ids.reserve(sessions_.size());
    for (auto it = sessions_.constBegin(); it != sessions_.constEnd(); ++it)
        ids << it.key();
    return ids;
}

SessionPublisher ExchangeSessionManager::build_publisher() {
    SessionPublisher p;
    p.publish_ticker = [](const QString& exchange, const QString& pair, const TickerData& t) {
        if (!hub_supported_exchange(exchange) || pair.isEmpty())
            return;
        const QString topic = QStringLiteral("ws:") + exchange + QStringLiteral(":ticker:") + pair;
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(t));
    };
    p.publish_orderbook = [](const QString& exchange, const QString& pair, const OrderBookData& ob) {
        if (!hub_supported_exchange(exchange) || pair.isEmpty())
            return;
        const QString topic = QStringLiteral("ws:") + exchange + QStringLiteral(":orderbook:") + pair;
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(ob));
    };
    p.publish_trade = [](const QString& exchange, const QString& pair, const TradeData& td) {
        if (!hub_supported_exchange(exchange) || pair.isEmpty())
            return;
        const QString topic = QStringLiteral("ws:") + exchange + QStringLiteral(":trades:") + pair;
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(td));
    };
    p.publish_candle = [](const QString& exchange, const QString& pair, const QString& interval, const Candle& c) {
        if (!hub_supported_exchange(exchange) || pair.isEmpty())
            return;
        const QString topic = QStringLiteral("ws:") + exchange + QStringLiteral(":ohlc:") + pair +
                              QLatin1Char(':') + interval;
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(c));
    };
    return p;
}

// ── Producer ────────────────────────────────────────────────────────────────

QStringList ExchangeSessionManager::topic_patterns() const {
    return {"ws:kraken:*", "ws:hyperliquid:*"};
}

void ExchangeSessionManager::refresh(const QStringList& /*topics*/) {
    // push_only — scheduler never calls this. Fan-out happens via
    // ExchangeSession → SessionPublisher → DataHub.
}

int ExchangeSessionManager::max_requests_per_sec() const {
    return 0; // unlimited, push-only
}

void ExchangeSessionManager::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy push_only;
    push_only.push_only = true;
    push_only.ttl_ms = 0;
    push_only.min_interval_ms = 0;

    fincept::datahub::TopicPolicy coalesced_ticker = push_only;
    coalesced_ticker.coalesce_within_ms = 50; // 20 Hz fan-out cap

    hub.set_policy_pattern("ws:kraken:ticker:*", coalesced_ticker);
    hub.set_policy_pattern("ws:hyperliquid:ticker:*", coalesced_ticker);

    hub.set_policy_pattern("ws:kraken:orderbook:*", push_only);
    hub.set_policy_pattern("ws:kraken:trades:*", push_only);
    hub.set_policy_pattern("ws:kraken:ohlc:*", push_only);
    hub.set_policy_pattern("ws:hyperliquid:orderbook:*", push_only);
    hub.set_policy_pattern("ws:hyperliquid:trades:*", push_only);
    hub.set_policy_pattern("ws:hyperliquid:ohlc:*", push_only);

    hub_registered_ = true;
    LOG_INFO(kMgrTag, "Registered with DataHub (ws:kraken:*, ws:hyperliquid:*)");
}

} // namespace fincept::trading
