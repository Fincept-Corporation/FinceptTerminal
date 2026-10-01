#include "trading/ExchangeSessionManager.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

#include <QMutexLocker>
#include <QPointer>
#include <QTimer>
#include <QVariant>

namespace fincept::trading {

namespace {
const QString kMgrTag = "ExchangeSessionManager";

// Hub-demand tuning. A dashboard adds several tiles in one go, so coalesce the
// resulting topic_active bursts into one stream launch; hiding a dashboard tab and
// coming back should not respawn a Python process, so keep an unneeded hub-owned
// stream alive for a while before stopping it.
constexpr int kEsmHubSyncDebounceMs = 400;
constexpr int kEsmHubIdleStopMs = 20000;
constexpr int kEsmHubHealMs = 30000;

// Split "ws:<exchange>:<family>:<pair>" — the pair may itself contain ':'
// (perps: "BTC/USDC:USDC"), so everything after the third separator is the pair.
bool esm_parse_ws_topic(const QString& topic, QString& exchange, QString& family, QString& pair) {
    if (!topic.startsWith(QLatin1String("ws:")))
        return false;
    const QStringList parts = topic.split(QLatin1Char(':'));
    if (parts.size() < 4)
        return false;
    exchange = parts.at(1);
    family = parts.at(2);
    pair = topic.section(QLatin1Char(':'), 3);
    return !exchange.isEmpty() && !pair.isEmpty();
}

// Hub allow-list — exchanges whose WS fan-out is published on DataHub. Backed
// by the single canonical list (ExchangeSessionManager::supported_exchange_ids).
// The published payload types (TickerData/OrderBookData/TradeData/Candle) are
// type-level metatypes registered once, so any listed exchange "just works".
bool hub_supported_exchange(const QString& id) {
    return ExchangeSessionManager::supported_exchange_ids().contains(id);
}
} // namespace

const QStringList& ExchangeSessionManager::supported_exchange_ids() {
    // Kraken (native C++ WS) + Hyperliquid (perps DEX) first, then major
    // global exchanges streamed via the ccxt.pro daemon. All ids verified
    // present in ccxt.pro with WS support. Use canonical ids (e.g. "gate",
    // not the deprecated "gateio" alias).
    static const QStringList ids = {
        "kraken", "hyperliquid", "binance", "coinbase", "okx",       "bybit", "kucoin",
        "bitget", "gate",        "mexc",    "htx",      "cryptocom", "bingx", "bitfinex",
    };
    return ids;
}

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
        const QString topic =
            QStringLiteral("ws:") + exchange + QStringLiteral(":ohlc:") + pair + QLatin1Char(':') + interval;
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(c));
    };
    return p;
}

// ── Producer ────────────────────────────────────────────────────────────────

QStringList ExchangeSessionManager::topic_patterns() const {
    QStringList patterns;
    for (const auto& id : supported_exchange_ids())
        patterns << (QStringLiteral("ws:") + id + QStringLiteral(":*"));
    return patterns;
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

    // Demand-driven streaming (see the header). Timers are created here, on the
    // thread that registers the manager (main).
    hub_sync_timer_ = new QTimer(this);
    hub_sync_timer_->setSingleShot(true);
    hub_sync_timer_->setInterval(kEsmHubSyncDebounceMs);
    connect(hub_sync_timer_, &QTimer::timeout, this, [this]() {
        const QSet<QString> dirty = hub_dirty_;
        hub_dirty_.clear();
        for (const auto& ex : dirty)
            sync_hub_exchange(ex);
    });
    // Self-heal while any demand exists: a screen that stops the stream, or a
    // subprocess that exhausted its bounded respawn budget, would otherwise leave
    // subscribed tiles dead until the next topic change.
    hub_heal_timer_ = new QTimer(this);
    hub_heal_timer_->setInterval(kEsmHubHealMs);
    connect(hub_heal_timer_, &QTimer::timeout, this, &ExchangeSessionManager::heal_hub_streams);
    connect(&hub, &fincept::datahub::DataHub::topic_active, this, &ExchangeSessionManager::on_hub_topic_active);
    connect(&hub, &fincept::datahub::DataHub::topic_idle, this, &ExchangeSessionManager::on_hub_topic_idle);
    // Catch up on subscriptions made before this registration ran.
    for (const auto& st : hub.stats()) {
        if (st.subscriber_count > 0 && st.topic.startsWith(QLatin1String("ws:")))
            on_hub_topic_active(st.topic);
    }

    fincept::datahub::TopicPolicy push_only;
    push_only.push_only = true;
    push_only.ttl_ms = 0;
    push_only.min_interval_ms = 0;

    fincept::datahub::TopicPolicy coalesced_ticker = push_only;
    coalesced_ticker.coalesce_within_ms = 50; // 20 Hz fan-out cap

    // Install per-exchange policies for every supported exchange: ticker
    // coalesced at 20 Hz, orderbook/trades/ohlc pushed straight through.
    for (const auto& id : supported_exchange_ids()) {
        const QString base = QStringLiteral("ws:") + id + QLatin1Char(':');
        hub.set_policy_pattern(base + QStringLiteral("ticker:*"), coalesced_ticker);
        hub.set_policy_pattern(base + QStringLiteral("orderbook:*"), push_only);
        hub.set_policy_pattern(base + QStringLiteral("trades:*"), push_only);
        hub.set_policy_pattern(base + QStringLiteral("ohlc:*"), push_only);
    }

    hub_registered_ = true;
    LOG_INFO(kMgrTag, QString("Registered with DataHub (%1 exchanges)").arg(supported_exchange_ids().size()));
}

// ── DataHub-driven stream demand ────────────────────────────────────────────

void ExchangeSessionManager::on_hub_topic_active(const QString& topic) {
    QString exchange, family, pair;
    if (!esm_parse_ws_topic(topic, exchange, family, pair) || !hub_supported_exchange(exchange))
        return;
    // trades / orderbook only flow for the stream's PRIMARY pair; ticker for any
    // pair on the list. (ohlc topics are chart-timeframe bound and stay with the
    // Crypto Trading screen.)
    const bool primary_feed = family == QLatin1String("trades") || family == QLatin1String("orderbook");
    if (!primary_feed && family != QLatin1String("ticker"))
        return;

    HubDemand& d = hub_demand_[exchange];
    if (d.topics.contains(topic))
        return;
    d.topics.insert(topic);
    (primary_feed ? d.primary : d.watch).append(pair);
    schedule_hub_sync(exchange);
}

void ExchangeSessionManager::on_hub_topic_idle(const QString& topic) {
    QString exchange, family, pair;
    if (!esm_parse_ws_topic(topic, exchange, family, pair))
        return;
    auto it = hub_demand_.find(exchange);
    if (it == hub_demand_.end() || !it->topics.remove(topic))
        return; // not a topic we counted
    const bool primary_feed = family == QLatin1String("trades") || family == QLatin1String("orderbook");
    (primary_feed ? it->primary : it->watch).removeOne(pair); // one occurrence per topic
    schedule_hub_sync(exchange);
}

void ExchangeSessionManager::schedule_hub_sync(const QString& exchange_id) {
    hub_dirty_.insert(exchange_id);
    if (hub_sync_timer_ && !hub_sync_timer_->isActive())
        hub_sync_timer_->start();
    if (hub_heal_timer_ && !hub_heal_timer_->isActive())
        hub_heal_timer_->start();
}

void ExchangeSessionManager::heal_hub_streams() {
    bool any_demand = false;
    const QStringList exchanges = hub_demand_.keys(); // sync_hub_exchange() may erase entries
    for (const auto& ex : exchanges) {
        if (hub_demand_.value(ex).topics.isEmpty())
            continue;
        any_demand = true;
        sync_hub_exchange(ex);
    }
    if (!any_demand && hub_heal_timer_)
        hub_heal_timer_->stop();
}

void ExchangeSessionManager::sync_hub_exchange(const QString& exchange_id) {
    const HubDemand demand = hub_demand_.value(exchange_id);

    // Primary-feed pairs first (the first one becomes the stream's primary), then
    // the ticker-only pairs, deduplicated.
    QStringList want = demand.primary;
    for (const auto& pair : demand.watch)
        want.append(pair);
    want.removeDuplicates();

    ExchangeSession* s = session(exchange_id);
    s->set_hub_pairs(want);

    if (want.isEmpty()) {
        hub_demand_.remove(exchange_id);
        // Demand is gone. Stop only a stream the hub path started (never one a screen
        // owns), and only after a grace period — dashboards are hidden and shown
        // constantly and a respawn costs a Python process. Re-check when it elapses.
        if (s->is_ws_hub_owned() && s->is_ws_active()) {
            QPointer<ExchangeSessionManager> self = this;
            QTimer::singleShot(kEsmHubIdleStopMs, this, [self, exchange_id]() {
                if (!self)
                    return;
                const auto it = self->hub_demand_.constFind(exchange_id);
                if (it != self->hub_demand_.constEnd() && !it->topics.isEmpty())
                    return; // demand came back
                ExchangeSession* sess = self->session(exchange_id);
                if (sess->is_ws_hub_owned() && sess->is_ws_active()) {
                    LOG_INFO(kMgrTag, QString("No DataHub subscribers left — stopping %1 stream").arg(exchange_id));
                    sess->stop_ws();
                }
            });
        }
        return;
    }

    if (!s->is_ws_active()) {
        const QString primary = !demand.primary.isEmpty() ? demand.primary.first() : want.first();
        QStringList list = want;
        list.removeAll(primary);
        list.prepend(primary);
        if (s->start_ws(primary, list, /*hub_owned=*/true))
            LOG_INFO(kMgrTag, QString("Started %1 stream for DataHub subscribers (%2 pair(s))")
                                  .arg(exchange_id)
                                  .arg(list.size()));
        return;
    }

    // A stream is already running (screen-owned or ours): relaunch only when it is
    // missing a wanted pair, keeping its primary and its ownership.
    const QStringList have = s->ws_symbols();
    // On a screen-owned stream the primary slot (trades / orderbook / ohlc) belongs
    // to the screen and moves with its symbol selection, so only ticker-only pairs
    // justify relaunching it; primary-feed pairs are still folded into its next
    // launch via set_hub_pairs() above.
    const QStringList needed = s->is_ws_hub_owned() ? want : demand.watch;
    QStringList merged = have;
    for (const auto& pair : needed) {
        if (!merged.contains(pair))
            merged.append(pair);
    }
    if (merged.size() == have.size())
        return;
    QString primary = s->get_ws_primary_symbol();
    if (primary.isEmpty())
        primary = merged.first();
    // ws_stream.py takes argv[2] as the primary symbol, and a set_primary command
    // since launch may have changed it without reordering the launch list.
    merged.removeAll(primary);
    merged.prepend(primary);
    LOG_INFO(kMgrTag, QString("Extending %1 stream with %2 DataHub pair(s)")
                          .arg(exchange_id)
                          .arg(merged.size() - have.size()));
    s->start_ws(primary, merged, s->is_ws_hub_owned());
}

} // namespace fincept::trading
