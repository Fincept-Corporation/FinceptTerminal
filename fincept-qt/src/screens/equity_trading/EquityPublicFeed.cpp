// EquityPublicFeed.cpp — see header. Public (Yahoo Finance, via DataHub) market
// data for the Equity Trading screen when no broker stream can serve it.
#include "screens/equity_trading/EquityPublicFeed.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/markets/MarketDataService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"

#include <QCoreApplication>
#include <QDateTime>

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

namespace {

// File-local helpers carry an eqpf_ prefix: the release build is a unity build.
const char* const eqpf_kTag = "EquityPublicFeed";

// Topic names belong to their producer (MarketDataService); never assembled here.
QString eqpf_quote_topic(const QString& yahoo) {
    return fincept::services::MarketDataService::quote_topic(yahoo);
}

QString eqpf_history_topic(const QString& yahoo, const EquityPublicFeed::HistorySpec& spec) {
    return fincept::services::MarketDataService::history_topic(yahoo, spec.period, spec.interval);
}

// When the price was really taken (epoch ms): Yahoo's answer time, never newer
// than the hub's publish. The Python layer serves cached quotes with their
// original fetch time, and a closed market's quote is not refetched for hours,
// so the publish time alone would make a delayed price look fresher than it is.
qint64 eqpf_quote_time_ms(const QString& yahoo, const fincept::services::QuoteData& q) {
    const qint64 published = datahub::DataHub::instance().last_publish_ms(eqpf_quote_topic(yahoo));
    return fincept::services::MarketDataService::quote_fetched_ms(
        q, published > 0 ? published : QDateTime::currentMSecsSinceEpoch());
}

} // namespace

// ============================================================================
// Source decision
// ============================================================================

EquityPublicFeed::BrokerAssessment EquityPublicFeed::assess_broker_feed(const QString& account_id) {
    BrokerAssessment a;
    if (account_id.isEmpty())
        return a; // NoAccount
    if (trading::is_builtin_paper_account(account_id)) {
        a.status = BrokerStatus::BuiltinPaper; // never a broker stream
        a.account_name = trading::AccountManager::instance().builtin_paper_account().display_name;
        return a;
    }

    auto& am = trading::AccountManager::instance();
    const trading::BrokerAccount account = am.get_account(account_id);
    if (account.account_id.isEmpty())
        return a; // focused id no longer exists

    a.account_name = account.display_name;
    if (auto* broker = trading::BrokerRegistry::instance().get(account.broker_id))
        a.broker_name = broker->profile().display_name;
    if (a.broker_name.isEmpty())
        a.broker_name = account.broker_id;

    // Same "has credentials" test the screen uses to decide whether to fetch at
    // all (on_account_changed skips every stream fetch without an api key).
    const auto creds = am.load_credentials(account_id);
    if (creds.api_key.isEmpty()) {
        a.status = BrokerStatus::NoCredentials;
        return a;
    }

    switch (account.state) {
        case trading::ConnectionState::Connected:
        case trading::ConnectionState::Connecting:
            a.status = BrokerStatus::Usable;
            break;
        case trading::ConnectionState::TokenExpired:
            a.status = BrokerStatus::TokenExpired;
            break;
        case trading::ConnectionState::Error:
            a.status = BrokerStatus::NoMarketData;
            a.detail = account.error_message;
            break;
        case trading::ConnectionState::Disconnected:
            // Token-based accounts load as Connected/Connecting/TokenExpired and are
            // only Disconnected after their socket drops. API-key-only accounts have
            // no session to validate and stay Disconnected after a restart while
            // their stream polls normally — they are usable.
            a.status = creds.access_token.isEmpty() ? BrokerStatus::Usable : BrokerStatus::Disconnected;
            break;
    }
    return a;
}

// Symbol / timeframe mapping (yahoo_symbol, history_spec) lives in
// EquitySymbolMap.h — pure, Qt Core only, unit-tested.

// ============================================================================
// Lifecycle
// ============================================================================

EquityPublicFeed::EquityPublicFeed(QObject* parent) : QObject(parent) {
    // A producer that never answers is timed out by the hub scheduler, which only
    // emits this hub-wide signal (no publish_error), so subscribe_errors() never
    // hears about it. Without this a hung Python worker would leave rows at "--"
    // and the chart on "loading" with no reason given. Every other failure arrives
    // through subscribe_errors() with the producer's own text.
    connect(&datahub::DataHub::instance(), &datahub::DataHub::topic_error, this,
            [this](const QString& topic, const QString& error) {
                if (active_ && error == QLatin1String("refresh_timeout"))
                    on_topic_timeout(topic);
            });
}

EquityPublicFeed::~EquityPublicFeed() {
    stop();
}

void EquityPublicFeed::start() {
    if (active_)
        return;
    active_ = true;
    ++generation_;
    for (auto it = watch_.cbegin(); it != watch_.cend(); ++it)
        subscribe_quote(it.key());
    report_unsupported();
    bind_chart(QString());
    LOG_DEBUG(eqpf_kTag, QString("Public feed started: %1 quote topic(s), chart %2")
                            .arg(quote_subs_.size())
                            .arg(chart_topic_.isEmpty() ? QStringLiteral("(none)") : chart_topic_));
}

void EquityPublicFeed::stop() {
    if (!active_)
        return;
    active_ = false;
    ++generation_;
    datahub::DataHub::instance().unsubscribe(this); // data + error subscriptions alike
    quote_subs_.clear();
    chart_bound_ = false;
    chart_last_px_ = 0.0;
    LOG_DEBUG(eqpf_kTag, "Public feed stopped");
}

// ============================================================================
// Quotes
// ============================================================================

void EquityPublicFeed::set_watch(const QVector<Target>& targets) {
    QHash<QString, QStringList> next;
    QHash<QString, QString> unsupported;
    for (const auto& t : targets) {
        if (t.symbol.isEmpty())
            continue;
        if (t.yahoo.isEmpty()) {
            unsupported.insert(t.symbol, t.unsupported_reason.isEmpty()
                                             ? QCoreApplication::translate("EquityPublicFeed",
                                                                           "%1 has no public market data.")
                                                   .arg(t.symbol)
                                             : t.unsupported_reason);
            continue;
        }
        QStringList& rows = next[t.yahoo];
        if (!rows.contains(t.symbol))
            rows.append(t.symbol);
    }

    const QHash<QString, QStringList> previous = watch_;
    watch_ = next;
    unsupported_ = unsupported;
    if (!active_)
        return; // start() subscribes whatever is current

    for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
        if (!watch_.contains(it.key()))
            unsubscribe_quote(it.key());
    }
    auto& hub = datahub::DataHub::instance();
    for (auto it = watch_.cbegin(); it != watch_.cend(); ++it) {
        if (!quote_subs_.contains(it.key())) {
            subscribe_quote(it.key());
            continue;
        }
        // Already subscribed: rows newly mapped onto this ticker get the cached
        // quote now instead of waiting for the next refresh.
        const QStringList before = previous.value(it.key());
        QStringList added;
        for (const auto& s : it.value())
            if (!before.contains(s))
                added << s;
        if (added.isEmpty())
            continue;
        const QVariant v = hub.peek(eqpf_quote_topic(it.key()));
        if (v.canConvert<services::QuoteData>())
            deliver_quote(it.key(), v.value<services::QuoteData>(), added);
    }
    report_unsupported();
}

void EquityPublicFeed::subscribe_quote(const QString& yahoo) {
    auto& hub = datahub::DataHub::instance();
    const QString topic = eqpf_quote_topic(yahoo);
    const quint64 gen = generation_;
    quote_subs_.insert(yahoo);
    // subscribe() hands over a fresh cached value or cold-starts a fetch; the
    // hub's market:quote:* policy (30 s TTL) drives the refreshes from there.
    hub.subscribe(this, topic, [this, yahoo, gen](const QVariant& v) {
        if (!active_ || gen != generation_ || !v.canConvert<services::QuoteData>())
            return;
        on_quote(yahoo, v.value<services::QuoteData>());
    });
    hub.subscribe_errors(this, topic, [this, yahoo, gen](const QString& error) {
        if (!active_ || gen != generation_)
            return;
        on_quote_error(yahoo, error);
    });
}

void EquityPublicFeed::unsubscribe_quote(const QString& yahoo) {
    auto& hub = datahub::DataHub::instance();
    const QString topic = eqpf_quote_topic(yahoo);
    hub.unsubscribe(this, topic);
    hub.unsubscribe_errors(this, topic);
    quote_subs_.remove(yahoo);
}

void EquityPublicFeed::on_quote(const QString& yahoo, const services::QuoteData& q) {
    deliver_quote(yahoo, q, watch_.value(yahoo));

    if (yahoo != chart_yahoo_ || q.price <= 0.0)
        return;
    // Yahoo is polled whether or not the market trades. Only a price that MOVED is
    // treated as a tick — an unchanged price after the close must not sprout a
    // flat bar in every new interval bucket.
    const bool moved = chart_last_px_ > 0.0 && q.price != chart_last_px_;
    chart_last_px_ = q.price;
    if (!moved)
        return;
    if (sinks_.chart_tick) {
        trading::BrokerQuote bq;
        bq.symbol = chart_symbol_;
        bq.ltp = q.price;
        bq.high = q.high;
        bq.low = q.low;
        bq.change = q.change;
        bq.change_pct = q.change_pct;
        bq.timestamp = eqpf_quote_time_ms(yahoo, q);
        sinks_.chart_tick(bq);
    }
    // Intraday charts: while the market moves, pull fresh history about once per
    // bar so the series stays whole (the hub's 30-minute history TTL alone would
    // leave gaps). request() without force honours the hub's min-interval gate and
    // error backoff, so this can never hammer Yahoo.
    if (chart_bound_ && chart_spec_.intraday && !chart_topic_.isEmpty()) {
        auto& hub = datahub::DataHub::instance();
        const qint64 age = hub.age_ms(chart_topic_);
        if (age < 0 || age >= chart_spec_.bar_ms)
            hub.request(chart_topic_);
    }
}

void EquityPublicFeed::deliver_quote(const QString& yahoo, const services::QuoteData& q, const QStringList& targets) {
    if (!sinks_.quote || targets.isEmpty())
        return;
    // The paper account fills on this price with this age shown: its real one.
    const qint64 taken = eqpf_quote_time_ms(yahoo, q);
    for (const QString& sym : targets) {
        if (sinks_.quote_detail)
            sinks_.quote_detail(sym, q, taken);
        trading::BrokerQuote bq;
        bq.symbol = sym;
        bq.ltp = q.price;
        bq.change = q.change;
        bq.change_pct = q.change_pct;
        bq.high = q.high;
        bq.low = q.low;
        bq.volume = q.volume;
        // Previous close, in the broker convention (change = ltp - close). Yahoo
        // has no bid/ask — those stay 0 and the widgets keep showing "--".
        if (std::isfinite(q.previous_close) && q.previous_close > 0.0)
            bq.close = q.previous_close;
        else if (q.price > 0.0 && q.change != 0.0)
            bq.close = q.price - q.change;
        if (std::isfinite(q.open) && q.open > 0.0)
            bq.open = q.open;
        bq.timestamp = taken;
        sinks_.quote(sym, bq);
    }
}

void EquityPublicFeed::on_quote_error(const QString& yahoo, const QString& error) {
    if (!sinks_.quote_error)
        return;
    const QString reason = error.trimmed().isEmpty()
                               ? QCoreApplication::translate("EquityPublicFeed", "Yahoo Finance returned no quote.")
                               : error.trimmed();
    for (const QString& sym : watch_.value(yahoo))
        sinks_.quote_error(sym, reason);
}

void EquityPublicFeed::replay_quote(const QString& symbol) {
    if (!active_ || symbol.isEmpty())
        return;
    const auto unsupported = unsupported_.constFind(symbol);
    if (unsupported != unsupported_.constEnd()) {
        if (sinks_.quote_error)
            sinks_.quote_error(symbol, unsupported.value());
        return;
    }
    for (auto it = watch_.cbegin(); it != watch_.cend(); ++it) {
        if (!it.value().contains(symbol))
            continue;
        auto& hub = datahub::DataHub::instance();
        const QString topic = eqpf_quote_topic(it.key());
        const QVariant v = hub.peek(topic); // fresh (within TTL) values only
        if (v.canConvert<services::QuoteData>()) {
            deliver_quote(it.key(), v.value<services::QuoteData>(), QStringList{symbol});
        } else {
            const QString error = hub.last_error(topic);
            if (!error.isEmpty() && sinks_.quote_error)
                sinks_.quote_error(symbol, error);
        }
        return;
    }
}

void EquityPublicFeed::report_unsupported() {
    if (!sinks_.quote_error)
        return;
    for (auto it = unsupported_.cbegin(); it != unsupported_.cend(); ++it)
        sinks_.quote_error(it.key(), it.value());
}

// ============================================================================
// Chart history
// ============================================================================

void EquityPublicFeed::set_chart(const Target& target, const QString& timeframe) {
    const QString& symbol = target.symbol;
    const HistorySpec spec = history_spec(timeframe);
    const QString topic = target.yahoo.isEmpty() ? QString() : eqpf_history_topic(target.yahoo, spec);
    const bool unchanged = chart_bound_ && symbol == chart_symbol_ && timeframe == chart_tf_ && topic == chart_topic_;

    const QString previous_topic = chart_bound_ ? chart_topic_ : QString();
    chart_symbol_ = symbol;
    chart_tf_ = timeframe;
    chart_spec_ = spec;
    chart_yahoo_ = target.yahoo;
    chart_unsupported_ = target.yahoo.isEmpty()
                             ? (target.unsupported_reason.isEmpty()
                                    ? QCoreApplication::translate("EquityPublicFeed", "%1 has no public price history.")
                                          .arg(symbol)
                                    : target.unsupported_reason)
                             : QString();
    chart_topic_ = topic;
    if (unchanged || !active_)
        return; // start() binds whatever is current
    bind_chart(previous_topic);
}

void EquityPublicFeed::bind_chart(const QString& previous_topic) {
    auto& hub = datahub::DataHub::instance();
    if (!previous_topic.isEmpty()) {
        hub.unsubscribe(this, previous_topic);
        hub.unsubscribe_errors(this, previous_topic);
    }
    chart_bound_ = true;
    chart_last_px_ = 0.0;
    if (chart_symbol_.isEmpty())
        return;
    if (chart_topic_.isEmpty()) {
        if (sinks_.chart_error)
            sinks_.chart_error(chart_symbol_, chart_tf_, chart_unsupported_);
        return;
    }
    // A fresh cached series is handed over by subscribe() on the next event-loop
    // turn — only announce "loading" when a fetch is actually needed.
    if (!hub.peek(chart_topic_).isValid() && sinks_.chart_loading)
        sinks_.chart_loading(chart_symbol_, chart_tf_);

    const quint64 gen = generation_;
    const QString topic = chart_topic_;
    hub.subscribe(this, topic, [this, gen, topic](const QVariant& v) {
        if (!active_ || gen != generation_ || topic != chart_topic_)
            return;
        on_history(v);
    });
    hub.subscribe_errors(this, topic, [this, gen, topic](const QString& error) {
        if (!active_ || gen != generation_ || topic != chart_topic_)
            return;
        on_chart_error(error);
    });
}

void EquityPublicFeed::on_history(const QVariant& value) {
    if (!value.canConvert<QVector<services::HistoryPoint>>())
        return;
    const auto points = value.value<QVector<services::HistoryPoint>>();

    QVector<trading::BrokerCandle> candles;
    candles.reserve(points.size());
    for (const auto& p : points) {
        if (p.timestamp <= 0 || !(p.close > 0.0))
            continue;
        trading::BrokerCandle c;
        c.timestamp = p.timestamp * 1000; // HistoryPoint: seconds; BrokerCandle: ms
        c.close = p.close;
        c.open = p.open > 0.0 ? p.open : p.close;
        c.high = std::max({p.high, c.open, c.close});
        c.low = p.low > 0.0 ? std::min({p.low, c.open, c.close}) : std::min(c.open, c.close);
        c.volume = static_cast<double>(p.volume);
        if (!candles.isEmpty()) {
            if (c.timestamp == candles.last().timestamp) { // Yahoo repeats the live bar
                candles.last() = c;
                continue;
            }
            if (c.timestamp < candles.last().timestamp)
                continue;
        }
        candles.append(c);
    }

    if (candles.isEmpty()) {
        on_chart_error(QCoreApplication::translate("EquityPublicFeed",
                                                   "Yahoo Finance returned no usable price history for %1 (%2, %3).")
                           .arg(chart_yahoo_, chart_spec_.period, chart_spec_.interval));
        return;
    }
    if (sinks_.chart_candles)
        sinks_.chart_candles(chart_symbol_, chart_tf_, candles);
}

void EquityPublicFeed::on_chart_error(const QString& error) {
    if (!sinks_.chart_error)
        return;
    const QString reason =
        error.trimmed().isEmpty()
            ? QCoreApplication::translate("EquityPublicFeed", "Yahoo Finance returned no price history for %1.")
                  .arg(chart_yahoo_)
            : error.trimmed();
    sinks_.chart_error(chart_symbol_, chart_tf_, reason);
}

void EquityPublicFeed::on_topic_timeout(const QString& topic) {
    const QString reason = QCoreApplication::translate(
        "EquityPublicFeed", "Yahoo Finance did not answer in time - retrying automatically.");
    static const QString kQuote = QStringLiteral("market:quote:");
    if (topic.startsWith(kQuote)) {
        const QString yahoo = topic.mid(kQuote.size());
        if (quote_subs_.contains(yahoo))
            on_quote_error(yahoo, reason);
        return;
    }
    if (chart_bound_ && topic == chart_topic_)
        on_chart_error(reason);
}

} // namespace fincept::screens::equity
