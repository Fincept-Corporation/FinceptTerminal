#include "trading/DataStreamManager.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"
#    include "trading/BrokerTopic.h"

#include <QDateTime>
#include <QPointer>
#include <QSet>
#include <QVariant>

namespace fincept::trading {

namespace {
constexpr const char* DSM_TAG = "DataStreamManager";

// Brokers whose tokens expire daily at ~3:00 AM IST: every Indian-region broker.
// Derived from each broker's own profile (region == "IN") rather than a
// hand-maintained list, so newly-added Indian brokers (samco, flattrade, paytm,
// tradejini, icicidirect, …) are covered automatically and can't silently drift
// out of the sweep. International brokers (US/EU regions) are excluded by region.
bool is_daily_expiring_broker(const QString& broker_id) {
    auto* b = BrokerRegistry::instance().get(broker_id);
    return b != nullptr && b->profile().region == QLatin1String("IN");
}

// Hourly cadence is enough to catch the 3:00 AM IST window without polling
// the clock aggressively.
constexpr int DSM_EXPIRY_CHECK_MS = 60 * 60 * 1000;
} // namespace

DataStreamManager& DataStreamManager::instance() {
    static DataStreamManager s;
    return s;
}

DataStreamManager::DataStreamManager() {
    // Daily IST token-expiry sweep (Phase 3 §19). This is an infrastructure
    // cadence timer on a long-lived service singleton (like the DataHub
    // scheduler), not a visibility-driven widget timer, so it runs continuously.
    expiry_check_timer_ = new QTimer(this);
    expiry_check_timer_->setInterval(DSM_EXPIRY_CHECK_MS);
    connect(expiry_check_timer_, &QTimer::timeout, this,
            &DataStreamManager::check_indian_token_expiry);
    expiry_check_timer_->start();

    // When an account's token is re-authenticated or silently refreshed, rebuild
    // its live stream so the WebSocket adapter (which captures the token at
    // construction with no setter) reconnects with the fresh token. Without this,
    // a stream started with a now-expired token keeps getting HTTP 401 on
    // resolve/subscribe and never streams ticks. Only rebuild streams that
    // already exist — don't spin up streams for accounts the user hasn't opened.
    connect(&AccountManager::instance(), &AccountManager::credentials_changed,
            this, [this](const QString& account_id) {
        if (!streams_.contains(account_id))
            return;
        LOG_INFO(DSM_TAG, QString("Credentials changed for %1 — rebuilding stream "
                                  "with fresh token").arg(account_id));
        restart_stream(account_id);
    });
}

void DataStreamManager::check_indian_token_expiry() {
    // Convert now → IST (UTC+5:30) without needing tz data.
    const QDateTime ist =
        QDateTime::currentDateTimeUtc().addSecs(5 * 3600 + 30 * 60);
    const int hour = ist.time().hour();
    const int doy = ist.date().dayOfYear();

    // Only act inside the 3:00–3:59 AM IST window, and at most once per IST day.
    if (hour != 3 || doy == last_expiry_check_day_)
        return;
    last_expiry_check_day_ = doy;

    auto& am = AccountManager::instance();
    const auto accounts = am.list_accounts();
    int marked = 0;
    for (const auto& acct : accounts) {
        if (!acct.is_active || acct.trading_mode != "live")
            continue;
        if (!is_daily_expiring_broker(acct.broker_id))
            continue;
        if (am.connection_state(acct.account_id) == ConnectionState::TokenExpired)
            continue;
        am.set_connection_state(acct.account_id, ConnectionState::TokenExpired,
                                QStringLiteral("Daily 3:00 AM IST session expiry"));
        ++marked;
    }
    if (marked > 0)
        LOG_INFO(DSM_TAG, QString("3:00 AM IST sweep: marked %1 Indian account(s) "
                                  "TokenExpired").arg(marked));
}

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

void DataStreamManager::restart_stream(const QString& account_id) {
    // stop_stream() tears down the old AccountDataStream (and its WebSocket, which
    // caches the access token at construction); start_stream() then rebuilds it so
    // ws_init() reloads the latest credentials from AccountManager. If no stream
    // exists yet, stop_stream() is a no-op and start_stream() creates a fresh one.
    stop_stream(account_id);
    start_stream(account_id);
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

void DataStreamManager::refresh_portfolio(const QString& account_id) {
    if (auto* s = stream_for(account_id))
        s->refresh_portfolio_now();
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
    // On-demand / one-shot signals — relayed directly (no hub topic)
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
    connect(stream, &AccountDataStream::connection_state_changed,
            this, [](const QString& account_id, ConnectionState state) {
        AccountManager::instance().set_connection_state(account_id, state);
    });
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

// ── Shared quote feed (Stage 2) ─────────────────────────────────────────────

void DataStreamManager::open_quote_feed(QObject* owner, const QString& consumer_id,
                                        const QString& account_id, const QString& symbol,
                                        std::function<void(const BrokerQuote&)> cb) {
    // This singleton + DataHub live on the main thread; callers (DeploymentRunner)
    // run on the algo engine thread. Marshal all stream/DataHub mutation here.
    QPointer<QObject> guard(owner);
    QMetaObject::invokeMethod(this, [this, guard, consumer_id, account_id, symbol, cb]() {
        if (!guard)
            return;
        ensure_registered_with_hub();
        if (!has_stream(account_id))
            start_stream(account_id);
        auto* stream = stream_for(account_id);
        if (!stream) {
            LOG_WARN(DSM_TAG, QString("open_quote_feed: no stream for account %1").arg(account_id));
            return;
        }
        // Join the account stream as an independent consumer + request fast (3s)
        // polling on non-WS brokers so the feed stays timely for algos.
        stream->subscribe_symbols(consumer_id, {symbol});
        stream->set_active_feed(consumer_id, {symbol});

        // Replace any prior feed registered under this consumer id.
        if (auto old = quote_feeds_.find(consumer_id); old != quote_feeds_.end()) {
            if (old->owner)
                fincept::datahub::DataHub::instance().unsubscribe(old->owner, old->topic);
            quote_feeds_.erase(old);
        }

        const QString topic = broker_topic(stream->broker_id(), account_id,
                                            QStringLiteral("quote"), symbol);
        // DataHub fans out on the MAIN thread; marshal each quote to the owner's
        // (engine) thread before invoking cb so on_tick_data runs there.
        fincept::datahub::DataHub::instance().subscribe<BrokerQuote>(
            guard, topic, [guard, cb](const BrokerQuote& q) {
                QMetaObject::invokeMethod(guard, [guard, cb, q]() {
                    if (guard) cb(q);
                }, Qt::QueuedConnection);
            });
        quote_feeds_.insert(consumer_id, QuoteFeed{account_id, topic, guard});
        LOG_INFO(DSM_TAG, QString("Opened quote feed '%1' for %2 on account %3")
                              .arg(consumer_id, symbol, account_id));
    }, Qt::QueuedConnection);
}

void DataStreamManager::close_quote_feed(const QString& consumer_id, const QString& account_id) {
    QMetaObject::invokeMethod(this, [this, consumer_id, account_id]() {
        if (auto it = quote_feeds_.find(consumer_id); it != quote_feeds_.end()) {
            if (it->owner)
                fincept::datahub::DataHub::instance().unsubscribe(it->owner, it->topic);
            quote_feeds_.erase(it);
        }
        if (auto* stream = stream_for(account_id)) {
            stream->unsubscribe_consumer(consumer_id);
            stream->set_active_feed(consumer_id, {});
        }
    }, Qt::QueuedConnection);
}

} // namespace fincept::trading
