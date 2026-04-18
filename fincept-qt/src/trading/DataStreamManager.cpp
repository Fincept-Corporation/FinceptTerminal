#include "trading/DataStreamManager.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"
#    include "trading/BrokerTopic.h"

#include <QVariant>

namespace fincept::trading {

namespace {
constexpr const char* DSM_TAG = "DataStreamManager";
}

DataStreamManager& DataStreamManager::instance() {
    static DataStreamManager s;
    return s;
}

DataStreamManager::DataStreamManager() = default;

// ── Stream lifecycle ────────────────────────────────────────────────────────

AccountDataStream* DataStreamManager::stream_for(const QString& account_id) {
    auto it = streams_.find(account_id);
    if (it != streams_.end())
        return it.value();
    return nullptr;
}

void DataStreamManager::start_stream(const QString& account_id) {
    if (streams_.contains(account_id)) {
        // Already exists — just resume
        streams_[account_id]->resume();
        return;
    }

    // Verify account exists
    if (!AccountManager::instance().has_account(account_id)) {
        LOG_WARN(DSM_TAG, QString("Cannot start stream: account %1 not found").arg(account_id));
        return;
    }

    auto* stream = new AccountDataStream(account_id, this);
    wire_stream_signals(stream);
    streams_.insert(account_id, stream);
    stream->start();

    LOG_INFO(DSM_TAG, QString("Started data stream for account %1").arg(account_id));
}

void DataStreamManager::stop_stream(const QString& account_id) {
    auto it = streams_.find(account_id);
    if (it == streams_.end())
        return;

    it.value()->stop();
    it.value()->deleteLater();
    streams_.erase(it);

    LOG_INFO(DSM_TAG, QString("Stopped data stream for account %1").arg(account_id));
}

void DataStreamManager::start_all_active() {
    const auto accounts = AccountManager::instance().active_accounts();
    for (const auto& account : accounts) {
        if (!streams_.contains(account.account_id))
            start_stream(account.account_id);
    }
    LOG_INFO(DSM_TAG, QString("Started %1 data streams for active accounts").arg(streams_.size()));
}

void DataStreamManager::stop_all() {
    for (auto it = streams_.begin(); it != streams_.end(); ++it) {
        it.value()->stop();
        it.value()->deleteLater();
    }
    streams_.clear();
    LOG_INFO(DSM_TAG, "All data streams stopped");
}

void DataStreamManager::pause_all() {
    for (auto* stream : streams_)
        stream->pause();
}

void DataStreamManager::resume_all() {
    for (auto* stream : streams_)
        stream->resume();
}

// ── Query ───────────────────────────────────────────────────────────────────

bool DataStreamManager::has_stream(const QString& account_id) const {
    return streams_.contains(account_id);
}

QStringList DataStreamManager::active_stream_ids() const {
    return QStringList(streams_.keys().begin(), streams_.keys().end());
}

// ── Signal wiring ───────────────────────────────────────────────────────────

void DataStreamManager::wire_stream_signals(AccountDataStream* stream) {
    // Forward all per-stream signals to aggregated manager signals
    connect(stream, &AccountDataStream::quote_updated,
            this, &DataStreamManager::quote_updated);
    connect(stream, &AccountDataStream::watchlist_updated,
            this, &DataStreamManager::watchlist_updated);
    connect(stream, &AccountDataStream::positions_updated,
            this, &DataStreamManager::positions_updated);
    connect(stream, &AccountDataStream::holdings_updated,
            this, &DataStreamManager::holdings_updated);
    connect(stream, &AccountDataStream::orders_updated,
            this, &DataStreamManager::orders_updated);
    connect(stream, &AccountDataStream::funds_updated,
            this, &DataStreamManager::funds_updated);
    connect(stream, &AccountDataStream::candles_fetched,
            this, &DataStreamManager::candles_fetched);
    connect(stream, &AccountDataStream::orderbook_fetched,
            this, &DataStreamManager::orderbook_fetched);
    connect(stream, &AccountDataStream::time_sales_fetched,
            this, &DataStreamManager::time_sales_fetched);
    connect(stream, &AccountDataStream::latest_trade_fetched,
            this, &DataStreamManager::latest_trade_fetched);
    connect(stream, &AccountDataStream::calendar_fetched,
            this, &DataStreamManager::calendar_fetched);
    connect(stream, &AccountDataStream::clock_fetched,
            this, &DataStreamManager::clock_fetched);
    connect(stream, &AccountDataStream::connection_state_changed,
            this, &DataStreamManager::connection_state_changed);
    connect(stream, &AccountDataStream::token_expired,
            this, &DataStreamManager::token_expired);

    // Dual-fire: publish the same per-account data onto hub topics so
    // consumers subscribed to broker:<id>:<account>:<channel> see it.
    // Only attach after register_producer() has run — no subscribers
    // otherwise, and publishing does nothing useful.
    if (hub_registered_) {
        connect(stream, &AccountDataStream::positions_updated,
                this, &DataStreamManager::on_positions_for_hub);
        connect(stream, &AccountDataStream::holdings_updated,
                this, &DataStreamManager::on_holdings_for_hub);
        connect(stream, &AccountDataStream::orders_updated,
                this, &DataStreamManager::on_orders_for_hub);
        connect(stream, &AccountDataStream::funds_updated,
                this, &DataStreamManager::on_funds_for_hub);
        connect(stream, &AccountDataStream::quote_updated,
                this, &DataStreamManager::on_quote_for_hub);
    }
}

// ── DataHub producer wiring (Phase 7) ───────────────────────────────────────
//
// DataStreamManager is the single Producer for all broker:* topics.
// Per-broker tick-feed subclasses land in follow-up PRs per the
// Phase 7 plan's migrate-brokers-one-by-one note.

QStringList DataStreamManager::topic_patterns() const {
    return {QStringLiteral("broker:*")};
}

void DataStreamManager::refresh(const QStringList& topics) {
    // AccountDataStream's portfolio_timer already polls positions /
    // orders / funds every 3s while a stream is running — short enough
    // that hub refresh() can stay advisory. Just log + validate.
    // Per-broker BrokerProducer subclasses (follow-up PRs per Phase 7
    // plan) can override and trigger explicit fetches when needed.
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        if (parts.size() < 4) {
            LOG_DEBUG(DSM_TAG, "refresh: ignoring malformed topic: " + topic);
            continue;
        }
        const QString channel = parts[3];
        if (channel == QLatin1String("ticks")) continue;  // push-only
        LOG_DEBUG(DSM_TAG, "refresh advisory (timer-driven): " + topic);
    }
}

int DataStreamManager::max_requests_per_sec() const {
    // Aggregate cap across all brokers; per-broker caps are already
    // enforced by each BrokerInterface::get_* implementation.
    return 5;
}

void DataStreamManager::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // broker:<id>:<account>:positions — TTL 5s
    // broker:<id>:<account>:orders    — TTL 5s
    // broker:<id>:<account>:balance   — TTL 30s
    // broker:<id>:<account>:ticks:*   — push-only, coalesce 100ms
    fincept::datahub::TopicPolicy positions_policy;
    positions_policy.ttl_ms = 5 * 1000;
    positions_policy.min_interval_ms = 3 * 1000;  // portfolio_timer cadence
    hub.set_policy_pattern(QStringLiteral("broker:*:*:positions"), positions_policy);

    fincept::datahub::TopicPolicy orders_policy;
    orders_policy.ttl_ms = 5 * 1000;
    orders_policy.min_interval_ms = 3 * 1000;
    hub.set_policy_pattern(QStringLiteral("broker:*:*:orders"), orders_policy);

    fincept::datahub::TopicPolicy balance_policy;
    balance_policy.ttl_ms = 30 * 1000;
    balance_policy.min_interval_ms = 10 * 1000;
    hub.set_policy_pattern(QStringLiteral("broker:*:*:balance"), balance_policy);

    fincept::datahub::TopicPolicy ticks_policy;
    ticks_policy.push_only = true;
    ticks_policy.coalesce_within_ms = 100;
    hub.set_policy_pattern(QStringLiteral("broker:*:*:ticks:*"), ticks_policy);

    // Also cover holdings + single-symbol quote snapshots.
    fincept::datahub::TopicPolicy holdings_policy;
    holdings_policy.ttl_ms = 30 * 1000;
    holdings_policy.min_interval_ms = 10 * 1000;
    hub.set_policy_pattern(QStringLiteral("broker:*:*:holdings"), holdings_policy);

    fincept::datahub::TopicPolicy quote_policy;
    quote_policy.ttl_ms = 5 * 1000;
    quote_policy.min_interval_ms = 1 * 1000;
    hub.set_policy_pattern(QStringLiteral("broker:*:*:quote:*"), quote_policy);

    hub_registered_ = true;

    // Back-wire any streams that were created before registration.
    for (auto* s : streams_) {
        if (!s) continue;
        connect(s, &AccountDataStream::positions_updated,
                this, &DataStreamManager::on_positions_for_hub);
        connect(s, &AccountDataStream::holdings_updated,
                this, &DataStreamManager::on_holdings_for_hub);
        connect(s, &AccountDataStream::orders_updated,
                this, &DataStreamManager::on_orders_for_hub);
        connect(s, &AccountDataStream::funds_updated,
                this, &DataStreamManager::on_funds_for_hub);
        connect(s, &AccountDataStream::quote_updated,
                this, &DataStreamManager::on_quote_for_hub);
    }

    LOG_INFO(DSM_TAG, "Registered with DataHub (broker:*)");
}

// ── Dual-fire hub publishers ────────────────────────────────────────────────

void DataStreamManager::on_positions_for_hub(const QString& account_id,
                                             const QVector<BrokerPosition>& positions) {
    if (!hub_registered_) return;
    auto* stream = stream_for(account_id);
    if (!stream) return;
    const QString topic = broker_topic(stream->broker_id(), account_id, QStringLiteral("positions"));
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(positions));
}

void DataStreamManager::on_holdings_for_hub(const QString& account_id,
                                            const QVector<BrokerHolding>& holdings) {
    if (!hub_registered_) return;
    auto* stream = stream_for(account_id);
    if (!stream) return;
    const QString topic = broker_topic(stream->broker_id(), account_id, QStringLiteral("holdings"));
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(holdings));
}

void DataStreamManager::on_orders_for_hub(const QString& account_id,
                                          const QVector<BrokerOrderInfo>& orders) {
    if (!hub_registered_) return;
    auto* stream = stream_for(account_id);
    if (!stream) return;
    const QString topic = broker_topic(stream->broker_id(), account_id, QStringLiteral("orders"));
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(orders));
}

void DataStreamManager::on_funds_for_hub(const QString& account_id, const BrokerFunds& funds) {
    if (!hub_registered_) return;
    auto* stream = stream_for(account_id);
    if (!stream) return;
    const QString topic = broker_topic(stream->broker_id(), account_id, QStringLiteral("balance"));
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(funds));
}

void DataStreamManager::on_quote_for_hub(const QString& account_id, const QString& symbol,
                                         const BrokerQuote& quote) {
    if (!hub_registered_) return;
    auto* stream = stream_for(account_id);
    if (!stream) return;
    const QString topic = broker_topic(stream->broker_id(), account_id, QStringLiteral("quote"), symbol);
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(quote));
}

} // namespace fincept::trading
