#include "services/markets/MarketDataService.h"

#include "core/logging/Logger.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "python/PythonRunner.h"
#include "python/PythonWorker.h"
#include "services/markets/MarketDataParsing.h"
#include "services/instruments/InstrumentIndex.h"
#include "services/markets/MarketPerformance.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QTimeZone>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace fincept::services {

namespace {

namespace mk = fincept::core::market;

// Yahoo throttles per client for a minute or more; retrying a rate-limited
// topic on the normal 2 s rung only extends the throttle.
constexpr std::chrono::milliseconds mds_kRateLimitRetry{60'000};
// Bounds on the script's own "retry_after_s" (its circuit breaker's reopening).
constexpr qint64 mds_kRetryAfterMinS = 30;
constexpr qint64 mds_kRetryAfterMaxS = 30 * 60;

// A failure as it is reported to the hub: user-readable text plus the retry
// hint for DataHub::publish_error (0 = the hub's normal backoff ladder).
struct MdsFailure {
    QString message;
    std::chrono::milliseconds retry_after{0};
    bool rate_limited = false; ///< Yahoo is throttling (counted in the traffic summary)
};

// Seconds until Yahoo traffic resumes, as the script reports it next to a
// rate_limited code ("retry_after_s"); 0 = not given (older script).
qint64 mds_retry_after_s(const QJsonObject& o) {
    const QJsonValue v = o.value(QLatin1String("retry_after_s"));
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() <= 0)
        return 0;
    return static_cast<qint64>(std::ceil(std::min(v.toDouble(), 86400.0)));
}

bool mds_looks_rate_limited(const QString& code, const QString& text) {
    if (code == QLatin1String("rate_limited"))
        return true;
    // Older scripts / PythonWorker errors carry no code — recognise the
    // yfinance and HTTP wording instead.
    static const char* const kMarkers[] = {"too many requests", "rate limit", "ratelimit", "http error 429",
                                           "http 429",          "status 429", "429 client"};
    const QString lower = text.toLower();
    for (const char* m : kMarkers)
        if (lower.contains(QLatin1String(m)))
            return true;
    return false;
}

// Default text when the script gave no usable reason. `what` names the data
// ("quote", "sparkline data", "price history (1mo, 1d)").
QString mds_default_message(const QString& code, const QString& symbol, const QString& what) {
    if (code == QLatin1String("not_found"))
        return QStringLiteral("%1: symbol not found on Yahoo Finance (it may be delisted, or need an exchange "
                              "suffix such as .NS)")
            .arg(symbol);
    if (code == QLatin1String("network"))
        return QStringLiteral("Could not reach Yahoo Finance for %1 (network error)").arg(symbol);
    return QStringLiteral("No %1 returned for %2 by Yahoo Finance").arg(what, symbol);
}

// Turn a script/worker error (`code` may be empty for older scripts) into
// what subscribers see. Rate limiting always says so; "not_found" is
// negative-cached by the hub for kNegativeCacheRetry. A script message that
// doesn't name `symbol` gets it prefixed — panels show one error line for
// many symbols. `retry_after_s` (mds_retry_after_s) is the script's own
// estimate of when Yahoo traffic resumes; it replaces the fixed minute.
MdsFailure mds_classify(const QString& code, const QString& raw, const QString& symbol, const QString& fallback,
                        qint64 retry_after_s = 0) {
    const QString text = raw.trimmed();
    if (mds_looks_rate_limited(code, text)) {
        MdsFailure f;
        f.rate_limited = true;
        if (retry_after_s <= 0) {
            f.message = QStringLiteral("Yahoo Finance is rate limiting requests (too many requests) - retrying "
                                       "automatically in a minute");
            f.retry_after = mds_kRateLimitRetry;
            return f;
        }
        const qint64 wait_s = std::clamp(retry_after_s, mds_kRetryAfterMinS, mds_kRetryAfterMaxS);
        f.retry_after = std::chrono::milliseconds(wait_s * 1000);
        // A script that sends the hint words its message for the user ("... retry at 14:05").
        f.message = !text.isEmpty()
                        ? text.left(240)
                        : QCoreApplication::translate("MarketDataService",
                                                      "Yahoo Finance is rate limiting requests (too many requests) - "
                                                      "retrying automatically in %1 min")
                              .arg((wait_s + 59) / 60);
        return f;
    }
    // "No data" alone tells the user nothing the fallback doesn't.
    const bool uninformative = text.isEmpty() || text.compare(QLatin1String("no data"), Qt::CaseInsensitive) == 0;
    MdsFailure f;
    if (uninformative)
        f.message = fallback;
    else if (!symbol.isEmpty() && !text.contains(symbol, Qt::CaseInsensitive))
        f.message = symbol + QStringLiteral(": ") + text;
    else
        f.message = text;
    f.message = f.message.left(240);
    if (code == QLatin1String("not_found"))
        f.retry_after = datahub::DataHub::kNegativeCacheRetry;
    return f;
}

void mds_publish_failure(const QString& topic, const MdsFailure& f) {
    datahub::DataHub::instance().publish_error(topic, f.message, f.retry_after);
}

// One quote row of a yfinance_data.py reply (batch_all "quotes", batch_quotes,
// quote) — MarketDataParsing.cpp. Keys an older script omits stay absent.
QuoteData mds_quote_from_reply(const QJsonObject& q) {
    return quote_from_reply_row(q);
}

// The quote cache's JSON shape ("market:<SYM>" in CacheManager) — written by
// both refresh paths, read by fetch_quotes(). Carries every QuoteData field.
void mds_cache_quote(const QuoteData& q, int ttl_sec) {
    fincept::CacheManager::instance().put(
        "market:" + q.symbol,
        QVariant(QString::fromUtf8(QJsonDocument(quote_to_cache_json(q)).toJson(QJsonDocument::Compact))), ttl_sec,
        "market_data");
}

QuoteData mds_quote_from_cache(const QJsonObject& o) {
    return quote_from_cache_json(o);
}

// ── Quote-cache writes, off the GUI thread ───────────────────────────────────
// Both refresh paths used to write one cache row per quote, one autocommit each,
// inside the reply callback on the GUI thread. A write has to take cache.db's
// write lock; while another connection held it (sweeper, a worker's cache put)
// each one waited — 0.3–1.3 s GUI stalls per refresh (ui_stalls.log,
// CacheDatabase::execute under flush_batch / the batch_all callback).
//
// Now the callback only queues the quotes. One pool task at a time drains the
// queue and writes each batch in a single transaction on its own thread's
// cache.db connection (CacheDatabase hands every thread its own; CacheManager::put
// runs on it). The queue keeps the newest quote per symbol, so a late batch can
// never overwrite a newer one, and the rows written are exactly what the old
// inline puts wrote — only where and when changed. A fetch_quotes() cache read
// in the few milliseconds before the write lands sees the previous (still
// TTL-valid) row or a miss, as it would have just before the refresh.
struct MdsQuoteCacheQueue {
    QMutex mutex;
    QHash<QString, QPair<QuoteData, int>> pending; ///< symbol → (newest quote, ttl seconds)
    bool draining = false;                        ///< a drain task is queued or running
};

MdsQuoteCacheQueue& mds_quote_cache_queue() {
    static MdsQuoteCacheQueue q;
    return q;
}

/// Pool thread: write queued quotes, one transaction per batch, until the queue is empty.
void mds_drain_quote_cache() {
    auto& q = mds_quote_cache_queue();
    for (;;) {
        QHash<QString, QPair<QuoteData, int>> batch;
        {
            QMutexLocker lock(&q.mutex);
            if (q.pending.isEmpty()) {
                q.draining = false;
                return;
            }
            batch.swap(q.pending);
        }
        // This thread's own connection (the one CacheManager::put uses here). Closed
        // or unavailable (shutdown): the puts below are no-ops, as they were before.
        QSqlDatabase conn = fincept::CacheDatabase::instance().connection();
        const bool in_tx = conn.isOpen() && conn.transaction();
        for (auto it = batch.cbegin(); it != batch.cend(); ++it)
            mds_cache_quote(it.value().first, it.value().second);
        if (in_tx && !conn.commit())
            conn.rollback();
    }
}

/// GUI thread: queue `quotes` for the cache (see above) and make sure a drain runs.
void mds_cache_quotes_async(const QVector<QuoteData>& quotes, int ttl_sec) {
    if (quotes.isEmpty())
        return;
    auto& q = mds_quote_cache_queue();
    bool start = false;
    {
        QMutexLocker lock(&q.mutex);
        for (const QuoteData& qd : quotes)
            q.pending.insert(qd.symbol, qMakePair(qd, ttl_sec));
        if (!q.draining) {
            q.draining = true;
            start = true;
        }
    }
    if (start)
        (void)QtConcurrent::run(&mds_drain_quote_cache);
}

// Yahoo's minor-unit price currencies (case matters: "GBp" is pence, "GBP"
// pounds). A major-unit symbol on these prices is wrong by 100x.
bool mds_is_minor_unit_currency(const QString& code) {
    return code == QLatin1String("GBp") || code == QLatin1String("GBX") || code == QLatin1String("ZAc") ||
           code == QLatin1String("ZAC") || code == QLatin1String("ILA");
}

// History freshness. A daily / weekly / monthly series keeps the
// `market:history:*` policy TTL (30 min); an intraday one goes stale with its
// next bar, so it is published with a TTL of about one bar, bounded to
// 60 s – 15 min. Returns 0 (= use the policy TTL) for non-intraday intervals.
std::chrono::milliseconds mds_history_ttl(const QString& interval) {
    static const QRegularExpression rx(QStringLiteral("^(\\d+)(m|h)$")); // 1m 5m 15m 60m 90m 1h; not 1mo
    const auto m = rx.match(interval.trimmed().toLower());
    if (!m.hasMatch())
        return std::chrono::milliseconds{0};
    const qint64 bar_s = m.captured(1).toLongLong() * (m.captured(2) == QLatin1String("h") ? 3600 : 60);
    if (bar_s <= 0)
        return std::chrono::milliseconds{0};
    return std::chrono::milliseconds{std::clamp<qint64>(bar_s, 60, 15 * 60) * 1000};
}

// ── Session-aware freshness ──────────────────────────────────────────────────
// Yahoo was asked for every subscribed quote every 30 s whether its market
// traded or not. A value is now published with a TTL from its venue's session
// (core/market/MarketSessions), so the hub does not refetch it before the
// session changes. Every function returns 0 for "use the topic policy's TTL".

constexpr int mds_kQuoteTtlMs = 30'000;          // market:quote:* policy TTL (open market)
constexpr qint64 mds_kExtendedTtlMs = 60'000;    // pre / post market, and a delayed feed catching up
constexpr qint64 mds_kClosedMinTtlMs = 30'000;   // floor of "until the next session change"
constexpr qint64 mds_kBarMinTtlMs = 60'000;      // same, for bars
constexpr qint64 mds_kClosedMaxTtlMs = 12 * 60 * 60'000;
constexpr qint64 mds_kUnknownClosedTtlMs = 15 * 60'000; // venue not in the table, Yahoo says CLOSED
// Yahoo delays most non-US exchanges by 15-20 min: a quote taken at the close
// is still that much behind. Until the delay (plus a margin for the closing
// auction prints) has passed, a closed venue is polled like an extended session.
constexpr int mds_kAssumedDelayMin = 20;
constexpr qint64 mds_kSettleMarginMs = 15 * 60'000;
constexpr qint64 mds_kClockRecheckMs = 60 * 60'000; // holiday overrides may load later

// Traffic summary cadence (MarketDataService::report_traffic).
constexpr int mds_kTrafficReportMs = 10 * 60'000;

qint64 mds_settle_ms(int exchange_delay_min) {
    return qint64(exchange_delay_min >= 0 ? exchange_delay_min : mds_kAssumedDelayMin) * 60'000 + mds_kSettleMarginMs;
}

// The venue a Yahoo symbol trades on, or nullptr when it is not in the table
// (or its time zone did not load — then no session rule may be trusted).
const mk::Venue* mds_venue_of(const QString& symbol, const QString& exchange_hint) {
    const mk::Venue* v = mk::venue_for_symbol(symbol, exchange_hint);
    return v && v->tz.isValid() ? v : nullptr;
}

// One venue's session as the TTL rules need it.
struct MdsVenueClock {
    mk::SessionState state = mk::SessionState::Closed;
    qint64 next_change_ms = 0; ///< when `state` ends; 0 = not within the horizon
    qint64 next_open_ms = 0;   ///< start of the next regular session; 0 = unknown
    qint64 last_end_ms = 0;    ///< end of the latest trading segment within a day (lunch start, close, post close); 0 = none
    qint64 computed_ms = 0;
    qint64 valid_until_ms = 0; ///< recompute from here on
};

MdsVenueClock mds_compute_venue_clock(const mk::Venue& v, qint64 now_ms) {
    MdsVenueClock c;
    c.computed_ms = now_ms;
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(now_ms, QTimeZone::utc());
    const mk::SessionStatus st = mk::session_status(v, now);
    c.state = st.state;
    if (st.next_change_utc.isValid())
        c.next_change_ms = st.next_change_utc.toMSecsSinceEpoch();

    // Where trading last stopped, for the delayed-feed settle window.
    if (st.state == mk::SessionState::Lunch || st.state == mk::SessionState::Post) {
        const mk::DaySchedule ds = mk::day_schedule(v, st.trading_date);
        const QDateTime end = st.state == mk::SessionState::Lunch ? ds.lunch_start_utc : ds.close_utc;
        if (end.isValid() && end <= now)
            c.last_end_ms = end.toMSecsSinceEpoch();
    } else if (st.state == mk::SessionState::Closed || st.state == mk::SessionState::Holiday) {
        const QDate today = mk::trading_date(v, now);
        for (int back = 0; back <= 1; ++back) {
            const mk::DaySchedule ds = mk::day_schedule(v, today.addDays(-back));
            if (ds.trading && ds.post_close_utc.isValid() && ds.post_close_utc <= now)
                c.last_end_ms = std::max(c.last_end_ms, ds.post_close_utc.toMSecsSinceEpoch());
        }
    }

    // The next regular open (daily bars move only then): at most Post -> Closed
    // -> Pre -> Open.
    if (st.state == mk::SessionState::Open) {
        c.next_open_ms = now_ms;
    } else {
        QDateTime t = st.next_change_utc;
        for (int hop = 0; hop < 4 && t.isValid(); ++hop) {
            const mk::SessionStatus at = mk::session_status(v, t);
            if (at.state == mk::SessionState::Open) {
                c.next_open_ms = t.toMSecsSinceEpoch();
                break;
            }
            t = at.next_change_utc;
        }
    }

    c.valid_until_ms = now_ms + mds_kClockRecheckMs;
    if (c.next_change_ms > now_ms)
        c.valid_until_ms = std::min(c.valid_until_ms, c.next_change_ms);
    return c;
}

// mds_compute_venue_clock() cached per venue until the venue's state changes:
// a refresh of 100 quotes on one exchange resolves its schedule once. Bounded
// by the venue table; locked, although every caller is a reply callback on
// the GUI thread.
MdsVenueClock mds_venue_clock(const mk::Venue& v, qint64 now_ms) {
    static QMutex mutex;
    static QHash<QString, MdsVenueClock> cache;
    QMutexLocker lock(&mutex);
    const auto it = cache.constFind(v.id);
    if (it != cache.constEnd() && now_ms >= it->computed_ms && now_ms < it->valid_until_ms)
        return it.value();
    const MdsVenueClock c = mds_compute_venue_clock(v, now_ms);
    cache.insert(v.id, c);
    return c;
}

// TTL of a quote. Open (or Yahoo says REGULAR) / 24x7 / unknown: the policy's
// 30 s. Pre / post market: 60 s. Lunch / closed / holiday: 60 s while a
// delayed feed is still catching up with the session that just ended, then
// until the next session change (30 s .. 12 h). A venue not in the table:
// 15 min when Yahoo says CLOSED.
std::chrono::milliseconds mds_quote_ttl(const QuoteData& q, qint64 now_ms) {
    if (q.market_state == QLatin1String("REGULAR"))
        return std::chrono::milliseconds{0};
    const mk::Venue* v = mds_venue_of(q.symbol, q.exchange);
    if (!v)
        return std::chrono::milliseconds{q.market_state == QLatin1String("CLOSED") ? mds_kUnknownClosedTtlMs : 0};
    if (v->is_24x7())
        return std::chrono::milliseconds{0};
    const MdsVenueClock c = mds_venue_clock(*v, now_ms);
    if (c.state == mk::SessionState::Open)
        return std::chrono::milliseconds{0};
    if (c.state == mk::SessionState::Pre || c.state == mk::SessionState::Post)
        return std::chrono::milliseconds{mds_kExtendedTtlMs};
    if (c.last_end_ms > 0 && now_ms < c.last_end_ms + mds_settle_ms(q.exchange_delay_min))
        return std::chrono::milliseconds{mds_kExtendedTtlMs};
    if (c.next_change_ms <= now_ms)
        return std::chrono::milliseconds{0}; // no known reopening: no saving, no risk
    return std::chrono::milliseconds{std::clamp(c.next_change_ms - now_ms, mds_kClosedMinTtlMs, mds_kClosedMaxTtlMs)};
}

// TTL of a bar series of `symbol` (daily-and-longer history, sparkline) once
// its venue has stopped trading and a delayed feed has caught up: until the
// next regular open for daily bars, until the next session change for
// sparklines (hourly bars, which `extended_hours_move` keeps on the policy TTL
// in pre / post market). 60 s .. 12 h. The exchange hint and feed delay come
// from the symbol's last quote in the hub when there is one.
std::chrono::milliseconds mds_bar_ttl(const QString& symbol, bool extended_hours_move, qint64 now_ms) {
    QString hint;
    int delay_min = -1;
    const QVariant last = datahub::DataHub::instance().peek_raw(MarketDataService::quote_topic(symbol));
    if (last.canConvert<QuoteData>()) {
        const QuoteData q = last.value<QuoteData>();
        if (q.market_state == QLatin1String("REGULAR"))
            return std::chrono::milliseconds{0};
        hint = q.exchange;
        delay_min = q.exchange_delay_min;
    }
    const mk::Venue* v = mds_venue_of(symbol, hint);
    if (!v || v->is_24x7())
        return std::chrono::milliseconds{0};
    const MdsVenueClock c = mds_venue_clock(*v, now_ms);
    if (c.state == mk::SessionState::Open)
        return std::chrono::milliseconds{0};
    if (extended_hours_move && (c.state == mk::SessionState::Pre || c.state == mk::SessionState::Post))
        return std::chrono::milliseconds{0};
    if (c.last_end_ms > 0 && now_ms < c.last_end_ms + mds_settle_ms(delay_min))
        return std::chrono::milliseconds{0};
    const qint64 until = extended_hours_move ? c.next_change_ms : c.next_open_ms;
    if (until <= now_ms)
        return std::chrono::milliseconds{0};
    return std::chrono::milliseconds{std::clamp(until - now_ms, mds_kBarMinTtlMs, mds_kClosedMaxTtlMs)};
}

// Teach the symbol master what a successful quote says about its instrument
// (name, Yahoo exchange code, quoteType, currency), so a symbol a user can see
// quoting can also be found by search. Only when that description changed for
// the symbol: a 120-symbol refresh every 30 s must not queue 120 identical learns.
// GUI thread only (both publish sites run in reply callbacks on it).
void mds_learn_from_quote(const QuoteData& q, const QString& name) {
    if (q.symbol.isEmpty())
        return;
    static QHash<QString, QString> learned_sig; // symbol -> name|exchange|quote_type|currency
    const QString sig = name + QLatin1Char('|') + q.exchange + QLatin1Char('|') + q.quote_type + QLatin1Char('|') +
                        q.currency;
    const auto it = learned_sig.constFind(q.symbol);
    if (it != learned_sig.constEnd() && it.value() == sig)
        return;
    if (learned_sig.size() > 20000) // bounded; a cleared entry only costs one repeat learn
        learned_sig.clear();
    learned_sig.insert(q.symbol, sig);
    InstrumentIndex::instance().learn_from_quote(q.symbol, name, q.exchange, q.quote_type, q.currency);
}

} // namespace

QString MarketDataService::quote_topic(const QString& symbol) {
    return QStringLiteral("market:quote:") + symbol;
}

QString MarketDataService::sparkline_topic(const QString& symbol) {
    return QStringLiteral("market:sparkline:") + symbol;
}

QString MarketDataService::history_topic(const QString& symbol, const QString& period, const QString& interval) {
    return QStringLiteral("market:history:") + symbol + QLatin1Char(':') + period + QLatin1Char(':') + interval;
}

QString MarketDataService::performance_history_topic(const QString& symbol) {
    return history_topic(symbol, QLatin1String(kPerformanceHistoryPeriod), QLatin1String(kPerformanceHistoryInterval));
}

QString MarketDataService::info_topic(const QString& symbol) {
    return QStringLiteral("market:info:") + symbol;
}

MarketDataService& MarketDataService::instance() {
    static MarketDataService s;
    return s;
}

MarketDataService::MarketDataService() {}

// ── DataHub Producer integration ────────────────────────────────────────────

QStringList MarketDataService::topic_patterns() const {
    return {QStringLiteral("market:quote:*"), QStringLiteral("market:sparkline:*"), QStringLiteral("market:history:*"),
            QStringLiteral("market:info:*")};
}

int MarketDataService::max_requests_per_sec() const {
    // Raised from 2 to 10: with the merged batch-all refresh path and the
    // persistent yfinance worker, refreshes complete in <200ms, so gating at
    // 2 req/s starved cold-start (quote + spark + history arrived 500ms apart).
    // 10 req/s still backs off enough for upstream yfinance rate limits while
    // letting dashboard + markets refresh converge in one scheduler tick.
    return 10;
}

void MarketDataService::refresh(const QStringList& topics) {
    // Hub guarantees `topics` all match one of our patterns. We bundle ALL
    // three families into a single `yfinance_data.py batch_all` invocation so
    // one refresh tick spawns one Python process instead of three. History and
    // sparklines are hub-only; fetch_quotes keeps a callback API for one-shot reads.
    static const QString kQuote = QStringLiteral("market:quote:");
    static const QString kSpark = QStringLiteral("market:sparkline:");
    static const QString kHist = QStringLiteral("market:history:");
    static const QString kInfo = QStringLiteral("market:info:");

    const qint64 refresh_t0 = QDateTime::currentMSecsSinceEpoch();

    QStringList quote_syms;
    QStringList spark_syms;
    QStringList info_syms;
    struct HistReq {
        QString topic;
        QString symbol;
        QString period;
        QString interval;
    };
    QVector<HistReq> hist_reqs;
    for (const auto& t : topics) {
        if (t.startsWith(kQuote)) {
            quote_syms.append(t.mid(kQuote.size()));
        } else if (t.startsWith(kSpark)) {
            spark_syms.append(t.mid(kSpark.size()));
        } else if (t.startsWith(kInfo)) {
            info_syms.append(t.mid(kInfo.size()));
        } else if (t.startsWith(kHist)) {
            const QString tail = t.mid(kHist.size());
            const QStringList parts = tail.split(QLatin1Char(':'));
            if (parts.size() == 3)
                hist_reqs.append({t, parts.at(0), parts.at(1), parts.at(2)});
            else
                mds_publish_failure(t, {QStringLiteral("Malformed history topic (expected "
                                                       "market:history:<symbol>:<period>:<interval>)"),
                                        datahub::DataHub::kNegativeCacheRetry});
        }
    }

    // Fundamentals go to their own subprocess(es): slow, and must not hold the
    // serial yfinance daemon that serves the quote refreshes below.
    if (!info_syms.isEmpty())
        refresh_info(info_syms);

    if (quote_syms.isEmpty() && spark_syms.isEmpty() && hist_reqs.isEmpty())
        return;

    // Build the unified batch_all payload.
    QJsonObject payload;
    if (!quote_syms.isEmpty()) {
        QJsonArray arr;
        for (const auto& s : quote_syms)
            arr.append(s);
        payload["quotes"] = arr;
    }
    if (!spark_syms.isEmpty()) {
        QJsonArray arr;
        for (const auto& s : spark_syms)
            arr.append(s);
        payload["sparklines"] = arr;
    }
    if (!hist_reqs.isEmpty()) {
        QJsonArray arr;
        for (const auto& h : hist_reqs) {
            QJsonObject o;
            o["symbol"] = h.symbol;
            o["period"] = h.period;
            o["interval"] = h.interval;
            arr.append(o);
        }
        payload["histories"] = arr;
    }

    if (payload.isEmpty()) {
        return; // Nothing to fetch — hub guarantees this won't happen in practice.
    }

    QPointer<MarketDataService> self = this;
    ++traffic_.batch_all_calls;
    traffic_.quote_symbols += static_cast<quint64>(quote_syms.size());

    // Route via PythonWorker (persistent daemon) instead of PythonRunner so
    // we don't pay the 2–3s yfinance/pandas import cost per refresh tick. See
    // PythonWorker docs — scope is yfinance_data.py only.
    python::PythonWorker::instance().submit(
        "batch_all", payload,
        [self, quote_syms, spark_syms, hist_reqs, refresh_t0](bool ok, QJsonObject root, QString err) {
            if (!self)
                return;

            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - refresh_t0;

            if (!ok) {
                // Notify hub so subscribers can react (clear spinner / show
                // error) and so the scheduler clears the in_flight flag. The
                // hub's error backoff decides when to retry. Without this
                // widgets that depend on these topics would spin forever on
                // producer failure.
                // A failed reply may still carry the script's code / retry hint.
                const MdsFailure f = mds_classify(root.value("code").toString(), err, QString(),
                                                  QStringLiteral("Market data refresh failed"), mds_retry_after_s(root));
                if (f.rate_limited)
                    ++self->traffic_.rate_limited;
                // Logged when the batch starts failing or the reason changes; repeats
                // at DEBUG; the recovery line below counts the failed polls.
                fincept::Logger::instance().transition("MarketData", QStringLiteral("batch_all"), 1);
                if (f.message != self->last_refresh_failure_) {
                    // A first run before the Python setup has finished (or a shutdown in
                    // progress) is a state that ends by itself, not a fault: INFO.
                    const bool expected = err.contains(QLatin1String("python is not set up"), Qt::CaseInsensitive) ||
                                          err.contains(QLatin1String("shutting down"), Qt::CaseInsensitive);
                    const QString line = logf(expected ? "Market data batch waiting" : "Market data batch failed",
                                              {{"quotes", quote_syms.size()},
                                               {"duration_ms", elapsed},
                                               {"error", err.left(200)}});
                    if (expected)
                        LOG_INFO("MarketData", line);
                    else
                        LOG_WARN("MarketData", line);
                    self->last_refresh_failure_ = f.message;
                } else {
                    LOG_DEBUG("MarketData", logf("Market data batch still failing", {{"duration_ms", elapsed}}));
                }
                for (const auto& s : quote_syms)
                    mds_publish_failure(quote_topic(s), f);
                for (const auto& s : spark_syms)
                    mds_publish_failure(QStringLiteral("market:sparkline:") + s, f);
                for (const auto& h : hist_reqs)
                    mds_publish_failure(h.topic, f);
                return;
            }
            {
                fincept::LogTransition was;
                auto& logger = fincept::Logger::instance();
                if (logger.transition_possible(0) && logger.transition("MarketData", QStringLiteral("batch_all"), 0, &was))
                    LOG_INFO("MarketData", logf("Market data batch recovered",
                                                {{"after_failures", was.observations_in_previous},
                                                 {"outage_s", was.ms_in_previous / 1000}}));
            }
            self->last_refresh_failure_.clear();

            // PythonWorker passes through the `result` JSON as-is (or wraps
            // a scalar under "_value"). For batch_all the daemon returns an
            // object so root is already the {quotes,sparklines,histories} map.
            // publish to hub, store in cache. We don't need the flush_batch code
            // path here because no callback chain is waiting on this refresh tick.
            //
            // CRITICAL: every requested topic MUST end with either publish()
            // (success) or publish_error() (failure) so the hub clears its
            // `in_flight` flag. Silently dropping errored/missing symbols
            // pins the topic for 30s (refresh_timeout_ms) and triggers the
            // BaseWidget watchdog to render "No data yet — click refresh to
            // retry" on every dashboard widget that subscribed to one of
            // them. yfinance fails per-symbol frequently (rate limits,
            // closed-market windows like ^N225 outside Asia hours, transient
            // 4xx), so tracking and erroring on misses is the difference
            // between "tile flashes a row missing" and "tile permanently
            // stuck in loading state".
            //
            // Reply contract (newer keys are optional — an older script omits
            // them and the fallbacks below apply):
            //   quotes: [...]  quote_errors: {SYM: {code, message}}
            //   quotes_error / quotes_error_code: the whole quote fetch failed
            //   sparklines: {SYM: [closes]} or {"error": "..."} on total failure
            //   histories: [{symbol, period, interval, points, error?, code?}]
            //   code ∈ not_found | no_data | rate_limited | network | unknown
            //   retry_after_s (next to a rate_limited code, per row / entry or
            //   at the top level): when the script's circuit breaker reopens
            //   timestamp (quote rows): when Yahoo answered for the row
            bool saw_rate_limit = false;
            auto fail = [&saw_rate_limit](const QString& topic, const MdsFailure& f) {
                saw_rate_limit = saw_rate_limit || f.rate_limited;
                mds_publish_failure(topic, f);
            };
            const qint64 root_retry_s = mds_retry_after_s(root);
            const QSet<QString> quote_wanted(quote_syms.cbegin(), quote_syms.cend());
            const QJsonObject quote_errors = root.value("quote_errors").toObject();
            const QString quotes_error = root.value("quotes_error").toString();
            const QString quotes_error_code = root.value("quotes_error_code").toString();
            const QJsonArray quotes_arr = root.value("quotes").toArray();
            QSet<QString> quotes_seen;
            int quotes_ok = 0;
            QVector<QuoteData> to_cache; // written off the GUI thread below
            to_cache.reserve(quotes_arr.size());
            for (const auto& v : quotes_arr) {
                const QJsonObject q = v.toObject();
                const QString sym = q.value("symbol").toString();
                if (sym.isEmpty())
                    continue; // nothing to key it on; requested symbols get an error below
                const QString row_err = q.value("error").toString();
                if (!row_err.isEmpty() || q.value("price").isUndefined()) {
                    // Per-row failure: reported here, unless quote_errors
                    // has a reason for the symbol or the row carries no
                    // message — then it stays unseen and the missing-symbol
                    // pass below reports it.
                    if (!row_err.isEmpty() && !quote_errors.contains(sym) && quote_wanted.contains(sym)) {
                        quotes_seen.insert(sym);
                        fail(quote_topic(sym), mds_classify(q.value("code").toString(), row_err, sym,
                                                            mds_default_message(q.value("code").toString(), sym,
                                                                                QStringLiteral("quote")),
                                                            mds_retry_after_s(q)));
                    }
                    continue;
                }
                quotes_seen.insert(sym);
                const QuoteData qd = mds_quote_from_reply(q);
                to_cache.append(qd); // same cache flush_batch writes
                self->publish_quote_to_hub(qd);
                ++quotes_ok;
            }
            mds_cache_quotes_async(to_cache, kQuoteCacheTtlSec);
            // Requested symbols with no quote: say why. Per-symbol reason from
            // quote_errors first, then the whole-fetch reason, then a plain
            // "no quote returned" (older scripts give neither). A zero-quote
            // reply lands here for every symbol, so nothing is left pinned.
            for (const auto& s : quote_syms) {
                if (quotes_seen.contains(s))
                    continue;
                QString code;
                QString reason;
                qint64 retry_s = 0;
                const QJsonValue qe = quote_errors.value(s);
                if (qe.isObject()) {
                    code = qe.toObject().value("code").toString();
                    reason = qe.toObject().value("message").toString();
                    retry_s = mds_retry_after_s(qe.toObject());
                } else if (qe.isString()) {
                    reason = qe.toString();
                } else if (!quotes_error.isEmpty()) {
                    code = quotes_error_code;
                    reason = quotes_error;
                    retry_s = root_retry_s;
                }
                fail(quote_topic(s),
                     mds_classify(code, reason, s, mds_default_message(code, s, QStringLiteral("quote")), retry_s));
            }

            // Sparklines — {sym: [closes]}, or {"error": "..."} when the whole
            // download failed. That error object must not be read as a symbol
            // map (it used to publish on a bogus "market:sparkline:error").
            const QJsonObject sparks = root.value("sparklines").toObject();
            QSet<QString> sparks_seen;
            int sparks_ok = 0;
            QString spark_total_error;
            if (sparks.value("error").isString())
                spark_total_error = sparks.value("error").toString();
            // Same Yahoo session as the quotes: if those were rate limited, so
            // is an otherwise unexplained sparkline failure.
            QString spark_code = sparks.value("code").toString();
            if (spark_code.isEmpty() && mds_looks_rate_limited(quotes_error_code, quotes_error))
                spark_code = QStringLiteral("rate_limited");
            if (spark_total_error.isEmpty()) {
                for (auto it = sparks.begin(); it != sparks.end(); ++it) {
                    const QJsonArray closes = it.value().toArray();
                    if (closes.isEmpty())
                        continue; // reported with the unseen symbols below
                    sparks_seen.insert(it.key());
                    QVector<double> prices;
                    prices.reserve(closes.size());
                    for (const auto& c : closes)
                        prices.append(c.toDouble());
                    self->publish_sparkline_to_hub(it.key(), prices);
                    ++sparks_ok;
                }
            }
            const qint64 spark_retry_s = sparks.contains(QLatin1String("retry_after_s")) ? mds_retry_after_s(sparks)
                                                                                         : root_retry_s;
            for (const auto& s : spark_syms) {
                if (sparks_seen.contains(s))
                    continue;
                fail(sparkline_topic(s),
                     mds_classify(spark_code, spark_total_error, s,
                                  mds_default_message(spark_code, s, QStringLiteral("sparkline data")), spark_retry_s));
            }

            // Histories — array of {symbol, period, interval, points[], error?, code?}.
            // An empty series is a failure, never a value: publishing an empty
            // vector painted a blank chart with the spinner gone.
            QSet<QString> hist_wanted;
            for (const auto& h : hist_reqs)
                hist_wanted.insert(h.topic);
            const QJsonArray hists = root.value("histories").toArray();
            QSet<QString> hists_seen;
            int hists_ok = 0;
            for (const auto& hv : hists) {
                const QJsonObject h = hv.toObject();
                const QString sym = h.value("symbol").toString();
                const QString per = h.value("period").toString();
                const QString ivl = h.value("interval").toString();
                const QString topic = history_topic(sym, per, ivl);
                if (!hist_wanted.contains(topic))
                    continue; // garbled / unrequested entry; requested topics are covered below
                hists_seen.insert(topic);
                const QJsonArray pts = h.value("points").toArray();
                const QString herr = h.value("error").toString();
                if (!herr.isEmpty() || pts.isEmpty()) {
                    const QString code = h.value("code").toString();
                    const QString what = QStringLiteral("price history (%1, %2)").arg(per, ivl);
                    fail(topic,
                         mds_classify(code, herr, sym, mds_default_message(code, sym, what), mds_retry_after_s(h)));
                    continue;
                }
                QVector<HistoryPoint> points;
                points.reserve(pts.size());
                for (const auto& pv : pts) {
                    const QJsonObject p = pv.toObject();
                    HistoryPoint pt;
                    pt.timestamp = static_cast<qint64>(p["timestamp"].toDouble());
                    pt.open = p["open"].toDouble();
                    pt.high = p["high"].toDouble();
                    pt.low = p["low"].toDouble();
                    pt.close = p["close"].toDouble();
                    pt.volume = static_cast<qint64>(p["volume"].toDouble());
                    points.append(pt);
                }
                self->publish_history_to_hub(sym, per, ivl, points);
                ++hists_ok;
            }
            for (const auto& h : hist_reqs) {
                if (hists_seen.contains(h.topic))
                    continue;
                const QString what = QStringLiteral("price history (%1, %2)").arg(h.period, h.interval);
                fail(h.topic, mds_classify(QString(), QString(), h.symbol, mds_default_message(QString(), h.symbol, what)));
            }
            if (saw_rate_limit)
                ++self->traffic_.rate_limited;

            LOG_DEBUG("MarketData", QString("batch_all OK in %1ms: quotes=%2/%3 sparks=%4/%5 hists=%6/%7")
                                       .arg(elapsed)
                                       .arg(quotes_ok)
                                       .arg(quote_syms.size())
                                       .arg(sparks_ok)
                                       .arg(spark_syms.size())
                                       .arg(hists_ok)
                                       .arg(hist_reqs.size()));
        });
}

void MarketDataService::publish_quote_to_hub(const QuoteData& q) {
    remember_quote_currency(q);
    // The quote row now carries the instrument's name (Yahoo longName / shortName,
    // futures contract month stripped). Keep the in-memory name cache current so
    // resolve_names() answers from it and the Markets screen needs no quote_names
    // subprocess on its cold path. In memory only, like the currency above: the next
    // quote_names write persists it; no settings write per tick.
    const QString name = q.name.trimmed();
    if (!q.symbol.isEmpty() && !name.isEmpty() && name != q.symbol) {
        load_name_cache();
        if (name_cache_.value(q.symbol) != name)
            name_cache_.insert(q.symbol, name);
    }
    // Fresh for as long as its venue's session says (mds_quote_ttl): a closed
    // market's quote is not refetched before the venue's next session change.
    const std::chrono::milliseconds ttl = mds_quote_ttl(q, QDateTime::currentMSecsSinceEpoch());
    if (ttl.count() > 0)
        datahub::DataHub::instance().publish(quote_topic(q.symbol), QVariant::fromValue(q), ttl);
    else
        datahub::DataHub::instance().publish(quote_topic(q.symbol), QVariant::fromValue(q));
    mds_learn_from_quote(q, name.isEmpty() ? name_cache_.value(q.symbol) : name);
}

qint64 MarketDataService::quote_fetched_ms(const QuoteData& q, qint64 published_ms) {
    if (q.fetched_at <= 0)
        return published_ms;
    const qint64 fetched_ms = q.fetched_at * 1000;
    return published_ms > 0 ? std::min(published_ms, fetched_ms) : fetched_ms;
}

bool MarketDataService::quote_is_current(const QString& symbol) {
    return datahub::DataHub::instance().peek(quote_topic(symbol)).isValid();
}

void MarketDataService::remember_quote_currency(const QuoteData& q) {
    // Same exclusions as quote_names: no currency for FX pairs (a rate) or
    // index levels / yields. Unknown quote_type: leave it to quote_names.
    if (q.symbol.isEmpty() || q.currency.isEmpty() || q.quote_type.isEmpty() ||
        q.quote_type == QLatin1String("CURRENCY") || q.quote_type == QLatin1String("INDEX"))
        return;
    load_name_cache();
    // In memory only: quote_names persists the cache when it next resolves a
    // name, and this runs on every quote — no settings write per tick.
    if (currency_cache_.value(q.symbol) != q.currency)
        currency_cache_.insert(q.symbol, q.currency);
}

// ── market:info:* (batched fundamentals) ────────────────────────────────────

void MarketDataService::refresh_info(const QStringList& symbols) {
    if (symbols.isEmpty())
        return;
    // One chunk at a time: the next starts when this one answers, so a long
    // list holds one PythonRunner slot (of three) instead of all of them, and
    // Yahoo sees one burst at a time.
    const QStringList chunk = symbols.mid(0, kInfoBatchMax);
    const QStringList rest = symbols.mid(kInfoBatchMax);
    QPointer<MarketDataService> self = this;
    const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    QStringList args;
    args << QStringLiteral("batch_info");
    args.append(chunk);
    ++traffic_.info_chunks;
    // PythonRunner runs batch_info as a subprocess (it is not one of the
    // actions it routes to the serial daemon) — on purpose, see header.
    python::PythonRunner::instance().run(
        QStringLiteral("yfinance_data.py"), args, [self, chunk, rest, t0](python::PythonResult result) {
            // The service is a process-lifetime singleton; the guard only
            // matters during shutdown, when nobody is subscribed anyway.
            if (!self)
                return;
            // Reply: {"info": {SYM: row}, "info_errors": {SYM: {code, message}}}
            // plus top-level error/code when no symbol succeeded — then
            // PythonRunner reports failure but keeps the JSON in `output`, so
            // the per-symbol reasons are still read from it.
            QJsonObject root;
            const QJsonDocument doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
            if (doc.isObject())
                root = doc.object();
            const QJsonObject infos = root.value(QLatin1String("info")).toObject();
            const QJsonObject errors = root.value(QLatin1String("info_errors")).toObject();
            const QString whole_code = root.value(QLatin1String("code")).toString();
            QString whole_reason = root.value(QLatin1String("error")).toString();
            if (!result.success && whole_reason.isEmpty())
                whole_reason = result.error; // process / interpreter failure
            const qint64 whole_retry_s = mds_retry_after_s(root);

            int ok = 0;
            bool saw_rate_limit = false;
            for (const QString& s : chunk) {
                const QJsonValue row = infos.value(s);
                if (row.isObject()) {
                    datahub::DataHub::instance().publish(info_topic(s),
                                                         QVariant::fromValue(info_from_batch_row(row.toObject(), s)));
                    ++ok;
                    continue;
                }
                QString code = whole_code;
                QString reason = whole_reason;
                qint64 retry_s = whole_retry_s;
                const QJsonObject e = errors.value(s).toObject();
                if (!e.isEmpty()) {
                    code = e.value(QLatin1String("code")).toString();
                    reason = e.value(QLatin1String("message")).toString();
                    retry_s = mds_retry_after_s(e);
                }
                const MdsFailure f =
                    mds_classify(code, reason, s, mds_default_message(code, s, QStringLiteral("fundamentals")), retry_s);
                saw_rate_limit = saw_rate_limit || f.rate_limited;
                mds_publish_failure(info_topic(s), f);
            }
            if (saw_rate_limit)
                ++self->traffic_.rate_limited;
            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - t0;
            if (ok == 0)
                LOG_WARN("MarketData", QString("batch_info failed in %1ms for %2 symbol(s): %3")
                                           .arg(elapsed)
                                           .arg(chunk.size())
                                           .arg(whole_reason.left(200)));
            else
                LOG_DEBUG("MarketData",
                          QString("batch_info OK in %1ms: info=%2/%3").arg(elapsed).arg(ok).arg(chunk.size()));

            if (rest.isEmpty())
                return;
            // Throttled: asking for the next chunk now only extends it. Fail
            // the rest with the same reason; the hub retries in a minute.
            if (ok == 0 && mds_looks_rate_limited(whole_code, whole_reason)) {
                for (const QString& s : rest)
                    mds_publish_failure(info_topic(s),
                                        mds_classify(whole_code, whole_reason, s, QString(), whole_retry_s));
                return;
            }
            self->refresh_info(rest);
        });
}

void MarketDataService::publish_history_to_hub(const QString& symbol, const QString& period, const QString& interval,
                                               const QVector<HistoryPoint>& points) {
    const QString topic = history_topic(symbol, period, interval);
    // One 30-minute TTL for every interval kept a 1-minute chart 29 bars stale;
    // an intraday series now expires with its bar (mds_history_ttl). Daily and
    // longer bars of a venue that has stopped trading stay fresh until it
    // reopens (mds_bar_ttl), otherwise the policy's 30 minutes.
    std::chrono::milliseconds ttl = mds_history_ttl(interval);
    if (ttl.count() == 0)
        ttl = mds_bar_ttl(symbol, /*extended_hours_move=*/false, QDateTime::currentMSecsSinceEpoch());
    if (ttl.count() > 0)
        datahub::DataHub::instance().publish(topic, QVariant::fromValue(points), ttl);
    else
        datahub::DataHub::instance().publish(topic, QVariant::fromValue(points));
}

void MarketDataService::publish_sparkline_to_hub(const QString& symbol, const QVector<double>& points) {
    // Hourly closes: unchanged while the venue is shut (mds_bar_ttl).
    const std::chrono::milliseconds ttl =
        mds_bar_ttl(symbol, /*extended_hours_move=*/true, QDateTime::currentMSecsSinceEpoch());
    if (ttl.count() > 0)
        datahub::DataHub::instance().publish(sparkline_topic(symbol), QVariant::fromValue(points), ttl);
    else
        datahub::DataHub::instance().publish(sparkline_topic(symbol), QVariant::fromValue(points));
}

void MarketDataService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = datahub::DataHub::instance();
    hub.register_producer(this);

    // Quotes: 30s TTL, 2s min interval. Dropped min_interval from 5s → 2s so
    // user-triggered refreshes and initial cold-start paint don't queue behind
    // the gate. 2s still prevents hammering yfinance on scheduler ticks.
    //
    // Phase 8 / decision 9.2: pause_when_inactive=true. Quotes drive
    // visible price tickers; when a frame is minimised the user can't see
    // them and the fan-out cost (table re-paints, format computations) is
    // pure waste. The cached value still updates so the next show reads
    // a fresh price from peek(). The hub also stops FETCHING a quote while
    // every subscriber is a hidden widget or sits in a minimised window
    // (services and stores always count as watching).
    //
    // 30 s is the TTL while the venue trades; publish_quote_to_hub() gives
    // pre / post-market and closed-market quotes a session-aware one.
    datahub::TopicPolicy quote_p;
    quote_p.ttl_ms = mds_kQuoteTtlMs;
    quote_p.min_interval_ms = 2'000;
    quote_p.pause_when_inactive = true;
    hub.set_policy_pattern(QStringLiteral("market:quote:*"), quote_p);

    // Sparklines: 5-day hourly data, changes slowly — cache 10 minutes,
    // refresh at most every 30s. Reduced from 60s so a user flipping between
    // screens doesn't wait a full minute for sparkline refresh after swapping
    // the symbol set.
    //
    // Phase 8 / decision 9.2: pause_when_inactive=true. Sparkline updates
    // re-paint inline charts in tables — substantial work for inactive
    // frames. Same reasoning as quotes.
    datahub::TopicPolicy spark_p;
    spark_p.ttl_ms = 10 * 60'000;
    spark_p.min_interval_ms = 30'000;
    spark_p.pause_when_inactive = true;
    hub.set_policy_pattern(QStringLiteral("market:sparkline:*"), spark_p);

    // History: OHLCV series. One-shot per chart; policies are conservative to
    // avoid re-fetching for every open of the same chart. 30 min TTL, 60s
    // min-interval (was 5 min) so a user flipping periods/intervals on a
    // chart doesn't stare at a stale view for 5 minutes. The 30 min applies to
    // daily and longer bars; intraday series are published with a per-publish
    // TTL of about one bar (publish_history_to_hub / mds_history_ttl).
    datahub::TopicPolicy hist_p;
    hist_p.ttl_ms = 30 * 60'000;
    hist_p.min_interval_ms = 60'000;
    hub.set_policy_pattern(QStringLiteral("market:history:*"), hist_p);

    // Fundamentals (sector, ratios, yield, earnings date): change over days,
    // so 4 h TTL and at most one automatic refetch per 10 min. batch_info
    // subprocesses (interpreter start + ~3 requests per symbol, 6 in
    // parallel, kInfoBatchMax symbols each) run one after another, so the
    // last topics of a long list wait — hence the long refresh timeout. Not
    // paused for inactive frames: a refresh is rare and cheap to fan out.
    datahub::TopicPolicy info_p;
    info_p.ttl_ms = 4 * 60 * 60'000;
    info_p.min_interval_ms = 10 * 60'000;
    info_p.refresh_timeout_ms = 5 * 60'000;
    hub.set_policy_pattern(QStringLiteral("market:info:*"), info_p);

    // Failed topics back off for up to 5 minutes (DataHub::publish_error).
    // When Python becomes usable — first-run install finished, interpreter
    // re-resolved — every failure so far was "Python not ready", so retry
    // them on the next scheduler pass instead of waiting out the backoff.
    connect(&python::PythonRunner::instance(), &python::PythonRunner::python_ready, this, [this]() {
        for (const QString& pattern : topic_patterns())
            datahub::DataHub::instance().clear_backoff(pattern);
    });

    // Request counters for diagnostics (DataHub inspector, MCP topic listing):
    // pushed by report_traffic(), never fetched. Long TTL so the idle sweep
    // keeps the value between the 10-minute reports.
    datahub::TopicPolicy diag_p;
    diag_p.push_only = true;
    diag_p.ttl_ms = 60 * 60'000;
    hub.set_policy(QStringLiteral("diag:market_data"), diag_p);
    if (!traffic_timer_) {
        traffic_timer_ = new QTimer(this);
        traffic_timer_->setInterval(mds_kTrafficReportMs);
        connect(traffic_timer_, &QTimer::timeout, this, &MarketDataService::report_traffic);
        traffic_timer_->start();
    }

    hub_registered_ = true;
    LOG_DEBUG("DataHub", "MarketDataService registered as producer for market:quote:*, market:sparkline:*, "
                        "market:history:*, market:info:*");
}

// ── Batched + Cached fetch_quotes ───────────────────────────────────────────

void MarketDataService::fetch_quotes(const QStringList& symbols, QuoteCallback cb) {
    if (symbols.isEmpty()) {
        cb(true, {});
        return;
    }

    // Answer what is already known, per symbol: a quote the hub holds inside its
    // freshness window (a closed market's until the venue reopens), else a row of
    // the 30 s quote cache. Only the rest is fetched. The cache is a database
    // read on this thread, so it is consulted only while it keeps answering: a
    // cold request costs one read, as before.
    auto& hub = datahub::DataHub::instance();
    QVector<QuoteData> served;
    QStringList fetch;
    QSet<QString> seen;
    bool cache_cold = false;
    for (const auto& sym : symbols) {
        if (seen.contains(sym))
            continue;
        seen.insert(sym);
        const QVariant hv = hub.peek(quote_topic(sym));
        if (hv.canConvert<QuoteData>()) {
            served.append(hv.value<QuoteData>());
            continue;
        }
        if (!cache_cold) {
            const QVariant cv = fincept::CacheManager::instance().get("market:" + sym);
            if (!cv.isNull()) {
                served.append(mds_quote_from_cache(QJsonDocument::fromJson(cv.toString().toUtf8()).object()));
                continue;
            }
            cache_cold = true;
        }
        fetch.append(sym);
    }
    traffic_.served_memory += static_cast<quint64>(served.size());
    if (fetch.isEmpty()) {
        last_fetch_error_.clear();
        cb(true, served);
        return;
    }

    // Queue for batching
    pending_.append({fetch, served, std::move(cb)});

    if (!batch_scheduled_) {
        batch_scheduled_ = true;
        QTimer::singleShot(100, this, &MarketDataService::flush_batch);
    }
}

void MarketDataService::flush_batch() {
    batch_scheduled_ = false;

    if (pending_.isEmpty())
        return;

    // Collect all unique symbols from pending requests
    QSet<QString> all_symbols_set;
    for (const auto& req : pending_) {
        for (const auto& sym : req.fetch) {
            all_symbols_set.insert(sym);
        }
    }
    QStringList all_symbols = all_symbols_set.values();

    // Take ownership of pending callbacks
    auto requests = std::move(pending_);
    pending_.clear();

    LOG_DEBUG("MarketData",
              QString("Batch fetch: %1 unique symbols from %2 requests").arg(all_symbols.size()).arg(requests.size()));
    ++traffic_.batch_quotes_calls;
    traffic_.quote_symbols += static_cast<quint64>(all_symbols.size());
    if (requests.size() > 1)
        traffic_.merged += static_cast<quint64>(requests.size() - 1);

    QStringList args;
    args << "batch_quotes";
    args.append(all_symbols);

    python::PythonRunner::instance().run(
        "yfinance_data.py", args, [this, requests = std::move(requests)](python::PythonResult result) {
            QVector<QuoteData> all_quotes;
            QString batch_failure;     // why the whole batch produced nothing, if known
            QJsonObject quote_errors;  // per-symbol reasons (object-shaped replies only)
            bool saw_rate_limit = false;

            if (result.success) {
                auto doc = QJsonDocument::fromJson(result.output.toUtf8());

                // Rows: a bare array (batch_quotes today), an object carrying
                // the batch_all-style {quotes, quote_errors, quotes_error,
                // quotes_error_code}, or a single quote object.
                QJsonArray rows;
                if (doc.isArray()) {
                    rows = doc.array();
                } else if (doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    if (obj.value("quotes").isArray()) {
                        rows = obj.value("quotes").toArray();
                        quote_errors = obj.value("quote_errors").toObject();
                        const QString qe = obj.value("quotes_error").toString();
                        if (!qe.isEmpty()) {
                            const MdsFailure f = mds_classify(obj.value("quotes_error_code").toString(), qe,
                                                              QString(), qe, mds_retry_after_s(obj));
                            batch_failure = f.message;
                            saw_rate_limit = f.rate_limited;
                        }
                    } else if (obj.contains("symbol")) {
                        rows.append(obj);
                    }
                }
                for (const auto& v : rows) {
                    const QJsonObject q = v.toObject();
                    if (q.contains("error")) {
                        saw_rate_limit = saw_rate_limit || mds_looks_rate_limited(q.value("code").toString(),
                                                                                  q.value("error").toString());
                        continue;
                    }
                    if (q.value("symbol").toString().isEmpty() || q.value("price").isUndefined())
                        continue;
                    const QuoteData quote = mds_quote_from_reply(q);
                    all_quotes.append(quote);
                    publish_quote_to_hub(quote);
                }
                for (auto it = quote_errors.constBegin(); it != quote_errors.constEnd(); ++it) {
                    const QJsonObject e = it.value().toObject();
                    saw_rate_limit = saw_rate_limit || mds_looks_rate_limited(e.value("code").toString(),
                                                                              e.value("message").toString());
                }
                // Cached off the GUI thread (mds_cache_quotes_async). The stale-cache
                // fallback below only runs for requests that got none of these quotes,
                // so it never needed them in the cache.
                mds_cache_quotes_async(all_quotes, kQuoteCacheTtlSec);

                LOG_DEBUG("MarketData", QString("Fetched %1 quotes (cached)").arg(all_quotes.size()));
            } else {
                const MdsFailure f =
                    mds_classify(QString(), result.error, QString(), QStringLiteral("Quote fetch failed"));
                batch_failure = f.message;
                saw_rate_limit = f.rate_limited;
                LOG_WARN("MarketData", "Batch fetch failed: " + result.error.left(300));
            }
            if (saw_rate_limit)
                ++traffic_.rate_limited;

            // Why `req` got nothing: the batch-level reason if there is one,
            // else the first per-symbol reason, else a plain statement.
            auto failure_for = [&batch_failure, &quote_errors](const QStringList& syms) -> QString {
                if (!batch_failure.isEmpty())
                    return batch_failure;
                for (const auto& s : syms) {
                    const QJsonObject e = quote_errors.value(s).toObject();
                    if (!e.isEmpty()) {
                        const QString code = e.value("code").toString();
                        return mds_classify(code, e.value("message").toString(), s,
                                            mds_default_message(code, s, QStringLiteral("quote")),
                                            mds_retry_after_s(e))
                            .message;
                    }
                }
                return syms.size() == 1 ? mds_default_message(QString(), syms.first(), QStringLiteral("quote"))
                                        : QStringLiteral("No quotes returned by Yahoo Finance for %1")
                                              .arg(syms.mid(0, 5).join(QStringLiteral(", ")) +
                                                   (syms.size() > 5 ? QStringLiteral(", ...") : QString()));
            };

            // Fan out results to each waiting callback: what it was served before
            // the batch plus the fetched quotes of the symbols it still needed.
            for (const auto& req : requests) {
                const QSet<QString> wanted(req.fetch.begin(), req.fetch.end());
                QVector<QuoteData> out = req.served;
                bool fetched_any = false;
                for (const auto& q : all_quotes) {
                    if (wanted.contains(q.symbol)) {
                        out.append(q);
                        fetched_any = true;
                    }
                }
                if (fetched_any) {
                    last_fetch_error_.clear();
                    req.cb(true, out);
                    continue;
                }

                // Zero quotes for a non-empty request is a failure, whether
                // Python failed or Yahoo answered with nothing — an empty
                // "success" made callers (PortfolioService) treat nothing as
                // data. Serve whatever the quote cache still holds and report
                // ok only if the answer is non-empty.
                last_fetch_error_ = failure_for(req.fetch);
                LOG_WARN_RL("MarketData", QStringLiteral("fetch_quotes|") + last_fetch_error_.left(120), 60000,
                            logf("No quotes fetched", {{"count", req.fetch.size()}, {"error", last_fetch_error_}}));
                // CacheManager::get() still honours the 30 s cache TTL, so
                // these are recent quotes another caller stored, not old ones.
                for (const auto& sym : req.fetch) {
                    const QVariant cv = fincept::CacheManager::instance().get("market:" + sym);
                    if (!cv.isNull())
                        out.append(mds_quote_from_cache(QJsonDocument::fromJson(cv.toString().toUtf8()).object()));
                }
                req.cb(!out.isEmpty(), out);
            }
        });
}

// ── Info fetch (company fundamentals) ───────────────────────────────────────

void MarketDataService::fetch_info(const QString& symbol, InfoCallback cb) {
    // Use financial_ratios command — it has all ratio fields in one call
    // and falls back gracefully. We run two parallel Python calls and merge:
    // 1) get_info  → name, sector, market cap, 52W range, beta, avg volume
    // 2) financial_ratios → P/E, P/B, dividend yield, ROE, margins, D/E, current ratio

    struct Partial {
        InfoData info;
        bool info_done = false;
        bool ratios_done = false;
        bool info_ok = false;
        bool ratios_ok = false;
    };
    auto shared = std::make_shared<Partial>();

    auto try_complete = [cb, shared, symbol]() {
        if (!shared->info_done || !shared->ratios_done)
            return;
        bool ok = shared->info_ok || shared->ratios_ok;
        if (ok)
            LOG_DEBUG("MarketData", "Fetched info for " + symbol);
        else
            LOG_WARN("MarketData", "Both info+ratios failed for " + symbol);
        cb(ok, shared->info);
    };

    // ── Call 1: get_info ──────────────────────────────────────────────────────
    QStringList args1;
    args1 << "info" << symbol;
    python::PythonRunner::instance().run("yfinance_data.py", args1,
                                         [shared, symbol, try_complete](python::PythonResult result) {
                                             shared->info_done = true;
                                             if (!result.success) {
                                                 LOG_WARN("MarketData", "get_info failed for " + symbol);
                                                 try_complete();
                                                 return;
                                             }
                                             auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                                             if (!doc.isObject() || doc.object().contains("error")) {
                                                 try_complete();
                                                 return;
                                             }
                                             QJsonObject o = doc.object();
                                             shared->info.symbol = o["symbol"].toString(symbol);
                                             shared->info.name = o["company_name"].toString();
                                             shared->info.sector = o["sector"].toString();
                                             shared->info.industry = o["industry"].toString();
                                             shared->info.country = o["country"].toString();
                                             shared->info.currency = o["currency"].toString("USD");
                                             shared->info.market_cap = o["market_cap"].toDouble();
                                             shared->info.beta = o["beta"].toDouble();
                                             shared->info.week52_high = o["fifty_two_week_high"].toDouble();
                                             shared->info.week52_low = o["fifty_two_week_low"].toDouble();
                                             shared->info.avg_volume = o["average_volume"].toDouble();
                                             shared->info.eps = o["revenue_per_share"].toDouble();
                                             shared->info_ok = true;
                                             try_complete();
                                         });

    // ── Call 2: financial_ratios ──────────────────────────────────────────────
    QStringList args2;
    args2 << "financial_ratios" << symbol;
    python::PythonRunner::instance().run("yfinance_data.py", args2,
                                         [shared, symbol, try_complete](python::PythonResult result) {
                                             shared->ratios_done = true;
                                             if (!result.success) {
                                                 LOG_WARN("MarketData", "financial_ratios failed for " + symbol);
                                                 try_complete();
                                                 return;
                                             }
                                             auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                                             if (!doc.isObject() || doc.object().contains("error")) {
                                                 try_complete();
                                                 return;
                                             }
                                             QJsonObject o = doc.object();
                                             shared->info.pe_ratio = o["peRatio"].toDouble();
                                             shared->info.forward_pe = o["forwardPE"].toDouble();
                                             shared->info.price_to_book = o["priceToBook"].toDouble();
                                             // yfinance (pinned 0.2.66) reports dividendYield in percent
                                             // (0.33 = 0.33%). InfoData holds it as a fraction like the
                                             // other ratios below, which is what consumers' ×100 expects.
                                             shared->info.dividend_yield = o["dividendYield"].toDouble() / 100.0;
                                             shared->info.roe = o["returnOnEquity"].toDouble();
                                             shared->info.profit_margin = o["profitMargin"].toDouble();
                                             shared->info.debt_to_equity = o["debtToEquity"].toDouble();
                                             shared->info.current_ratio = o["currentRatio"].toDouble();
                                             shared->info.eps = o["revenuePerShare"].toDouble();
                                             shared->ratios_ok = true;
                                             try_complete();
                                         });
}

// ── Static symbol lists ─────────────────────────────────────────────────────

QStringList MarketDataService::indices_symbols() {
    return {"^GSPC", "^DJI",  "^IXIC", "^RUT",      "^FTSE",  "^GDAXI",
            "^FCHI", "^N225", "^HSI",  "000001.SS", "^BSESN", "^NSEI"};
}

QStringList MarketDataService::forex_symbols() {
    return {"EURUSD=X", "GBPUSD=X", "USDJPY=X", "AUDUSD=X", "USDCAD=X", "USDCHF=X", "NZDUSD=X", "EURCHF=X"};
}

QStringList MarketDataService::crypto_symbols() {
    return {"BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD", "XRP-USD", "ADA-USD", "DOGE-USD", "DOT-USD", "LTC-USD"};
}

QStringList MarketDataService::commodity_symbols() {
    return {"GC=F", "SI=F", "CL=F", "BZ=F", "NG=F", "HG=F", "PL=F", "PA=F"};
}

QStringList MarketDataService::mover_symbols() {
    return {"SMCI", "PLTR", "MSTR", "NVDA", "AMD", "TSLA", "INTC", "PFE", "BA", "NKE", "DIS", "PYPL"};
}

QStringList MarketDataService::global_snapshot_symbols() {
    return {"^VIX", "^TNX", "DX-Y.NYB", "GC=F", "CL=F", "BTC-USD"};
}

QVector<MarketCategory> MarketDataService::default_global_markets() {
    return {
        {"Stock Indices",
         {"^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX", "^FTSE", "^GDAXI", "^N225", "^FCHI", "^HSI", "^AXJO", "^BSESN",
          "^NSEI", "^STOXX50E", "^NYA", "^SOX", "^IBEX", "^AEX"}},
        // USD-base pairs in Yahoo's own form (JPY=X is USD/JPY): Yahoo's quote
        // endpoint does not know USDCHF=X / USDINR=X, so those rows had no
        // currency, session, range or name. Displayed as "USD/JPY" etc. through
        // core::instrument::display_symbol; MarketPanelStore canonicalises too.
        {"Forex",
         {"EURUSD=X", "GBPUSD=X", "JPY=X", "CHF=X", "CAD=X", "AUDUSD=X", "NZDUSD=X", "EURGBP=X", "EURJPY=X",
          "GBPJPY=X", "CNY=X", "INR=X"}},
        // Lumber is LBR=F; Yahoo stopped serving the old LBS=F contract
        // ("possibly delisted; no price data found").
        {"Commodities",
         {"GC=F", "SI=F", "PL=F", "PA=F", "HG=F", "CL=F", "BZ=F", "NG=F", "RB=F", "HO=F", "ZC=F", "ZW=F", "ZS=F",
          "KC=F", "CT=F", "SB=F", "CC=F", "LBR=F"}},
        {"Bonds", {"^TNX", "^TYX", "^IRX", "^FVX", "TLT", "IEF", "SHY", "BND", "AGG", "LQD", "HYG", "JNK"}},
        {"ETFs", {"SPY", "QQQ", "DIA", "EEM", "GLD", "XLK", "XLE", "XLF", "XLV", "VNQ", "IWM", "VTI"}},
        // Uniswap trades on Yahoo as UNI7083-USD; UNI-USD returns no data.
        {"Cryptocurrencies",
         {"BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD", "XRP-USD", "ADA-USD", "DOGE-USD", "LINK-USD", "DOT-USD",
          "AVAX-USD", "UNI7083-USD", "ATOM-USD"}},
    };
}

QVector<RegionalMarket> MarketDataService::default_regional_markets() {
    return {
        {"India",
         {
             {"RELIANCE.NS", "Reliance Industries"},
             {"TCS.NS", "Tata Consultancy"},
             {"HDFCBANK.NS", "HDFC Bank"},
             {"INFY.NS", "Infosys"},
             {"HINDUNILVR.NS", "Hindustan Unilever"},
             {"ICICIBANK.NS", "ICICI Bank"},
             {"SBIN.NS", "State Bank of India"},
             {"BHARTIARTL.NS", "Bharti Airtel"},
             {"ITC.NS", "ITC Limited"},
             {"KOTAKBANK.NS", "Kotak Mahindra Bank"},
             {"LT.NS", "Larsen & Toubro"},
             {"WIPRO.NS", "Wipro Limited"},
         }},
        {"China",
         {
             {"BABA", "Alibaba Group"},
             {"PDD", "PDD Holdings"},
             {"JD", "JD.com"},
             {"BIDU", "Baidu"},
             {"NIO", "NIO Inc"},
             {"LI", "Li Auto"},
             {"XPEV", "XPeng"},
             {"BILI", "Bilibili"},
             {"NTES", "NetEase"},
             {"ZTO", "ZTO Express"},
             {"VNET", "VNET Group"},
             {"TAL", "TAL Education"},
         }},
        {"United States",
         {
             {"AAPL", "Apple Inc"},
             {"MSFT", "Microsoft Corp"},
             {"GOOGL", "Alphabet Inc"},
             {"AMZN", "Amazon.com"},
             {"NVDA", "NVIDIA Corp"},
             {"META", "Meta Platforms"},
             {"TSLA", "Tesla Inc"},
             {"JPM", "JPMorgan Chase"},
             {"V", "Visa Inc"},
             {"WMT", "Walmart Inc"},
             {"UNH", "UnitedHealth Group"},
             {"MA", "Mastercard Inc"},
         }},
    };
}

// ── Display-name resolution ─────────────────────────────────────────────────

void MarketDataService::load_name_cache() {
    if (name_cache_loaded_)
        return;

    // Retry backoff. The loaded flag used to be latched *before* the read, so a
    // single transient DB error marked the cache "loaded but empty" forever and
    // the next persist_name_cache() wrote that empty object over the user's
    // full cache. It is now latched only on success — but this function sits on
    // a hot path (currency_prefix() runs per row), so a failing read must not
    // hammer SQLite or spam the log on every call.
    static qint64 retry_after_ms = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < retry_after_ms)
        return;

    bool read_failed = false;
    auto load = [&read_failed](const char* key, QHash<QString, QString>& cache) {
        auto res = SettingsRepository::instance().get(key);
        if (res.is_err()) {
            LOG_ERROR("MarketData", QString("settings read failed for '%1' — name cache stays unloaded and will not "
                                            "be persisted: %2")
                                        .arg(QString::fromLatin1(key), QString::fromStdString(res.error())));
            read_failed = true;
            return;
        }
        if (res.value().isEmpty())
            return;
        auto doc = QJsonDocument::fromJson(res.value().toUtf8());
        if (!doc.isObject())
            return;
        const QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            cache.insert(it.key(), it.value().toString());
    };
    load("market_symbol_names", name_cache_);
    load("market_symbol_currencies", currency_cache_);

    name_cache_loaded_ = !read_failed;
    retry_after_ms = read_failed ? now + 30000 : 0;
}

void MarketDataService::persist_name_cache() {
    if (!name_cache_loaded_) {
        // Never write a cache we never successfully read — the in-memory hash
        // holds only whatever this run happened to resolve, and set() is an
        // INSERT OR REPLACE.
        LOG_WARN("MarketData", "Skipping name-cache persist — the stored cache was never read successfully");
        return;
    }

    auto dump = [](const QHash<QString, QString>& cache) {
        QJsonObject obj;
        for (auto it = cache.constBegin(); it != cache.constEnd(); ++it)
            obj.insert(it.key(), it.value());
        return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    };
    auto& settings = SettingsRepository::instance();
    settings.set("market_symbol_names", dump(name_cache_), "market_data");
    settings.set("market_symbol_currencies", dump(currency_cache_), "market_data");
}

// ISO 4217 code → display symbol. Falls back to "<CODE> " so an unmapped but
// valid currency still reads sensibly (e.g. "SEK 142.50").
//
// TODO(formatting): this table duplicates the central formatter being built in
// src/ui/formatting/; drop it once WatchlistScreen and MarketPanel use that.
QString MarketDataService::currency_prefix(const QString& symbol) {
    load_name_cache();
    const QString code = currency_cache_.value(symbol);
    if (code.isEmpty())
        return {};
    // Minor units are matched before the upper-casing below: "GBp".toUpper() is
    // "GBP", and "£116.95" for a price of 116.95 pence is wrong by 100x.
    if (mds_is_minor_unit_currency(code))
        return code + QLatin1Char(' ');

    static const QHash<QString, QString> kSymbols = {
        {"USD", "$"},    {"EUR", "€"},  {"GBP", "£"},   {"JPY", "¥"},  {"CNY", "CN¥"}, {"INR", "₹"},  {"HKD", "HK$"},
        {"AUD", "A$"},   {"CAD", "C$"}, {"NZD", "NZ$"}, {"SGD", "S$"}, {"KRW", "₩"},   {"BRL", "R$"}, {"ZAR", "R"},
        {"CHF", "CHF "}, {"RUB", "₽"},  {"TWD", "NT$"}, {"THB", "฿"},  {"IDR", "Rp"},  {"MYR", "RM"}, {"AED", "AED "},
    };
    auto it = kSymbols.find(code.toUpper());
    return it != kSymbols.end() ? it.value() : (code + QLatin1Char(' '));
}

void MarketDataService::resolve_names(const QStringList& symbols, NamesCallback cb) {
    load_name_cache();

    auto subset_for = [this](const QStringList& syms) {
        QHash<QString, QString> m;
        for (const auto& s : syms)
            if (name_cache_.contains(s))
                m.insert(s, name_cache_.value(s));
        return m;
    };

    // Deliver what we already know straight away (may be empty on cold start).
    if (cb)
        cb(subset_for(symbols));

    // Symbols still unnamed that nobody has asked for yet this session. A symbol
    // yfinance cannot name is asked once, not on every panel refresh.
    QStringList waiting_on;
    for (const auto& s : symbols) {
        if (s.isEmpty() || name_cache_.contains(s))
            continue;
        if (names_queued_.contains(s) || names_inflight_.contains(s)) {
            waiting_on.append(s);
        } else if (!names_attempted_.contains(s)) {
            names_queued_.insert(s);
            waiting_on.append(s);
        }
    }
    if (waiting_on.isEmpty())
        return;
    if (cb)
        name_waiters_.append({waiting_on, std::move(cb)});

    if (!names_flush_scheduled_ && !names_queued_.isEmpty()) {
        names_flush_scheduled_ = true;
        // Long enough to gather every panel of a screen that is being built.
        QTimer::singleShot(250, this, &MarketDataService::flush_name_requests);
    }
}

void MarketDataService::flush_name_requests() {
    names_flush_scheduled_ = false;
    if (names_queued_.isEmpty())
        return;

    QStringList batch = names_queued_.values();
    batch.sort();
    names_queued_.clear();
    for (const auto& s : batch) {
        names_inflight_.insert(s);
        names_attempted_.insert(s);
    }

    LOG_DEBUG("MarketData", QString("Resolving display names for %1 symbol(s) in one call").arg(batch.size()));

    QStringList args;
    args << "quote_names" << batch;
    ++traffic_.quote_names_calls;

    QPointer<MarketDataService> self = this;
    python::PythonRunner::instance().run("yfinance_data.py", args, [self, batch](python::PythonResult result) {
        if (!self)
            return;
        if (result.success && !result.output.trimmed().isEmpty()) {
            auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
            if (doc.isObject()) {
                const QJsonObject root = doc.object();
                const QJsonObject names = root.value("names").toObject();
                const QJsonObject currs = root.value("currencies").toObject();
                bool changed = false;
                for (auto it = names.begin(); it != names.end(); ++it) {
                    const QString name = it.value().toString().trimmed();
                    if (!name.isEmpty()) {
                        self->name_cache_.insert(it.key(), name);
                        changed = true;
                    }
                }
                for (auto it = currs.begin(); it != currs.end(); ++it) {
                    const QString code = it.value().toString().trimmed();
                    if (!code.isEmpty()) {
                        self->currency_cache_.insert(it.key(), code);
                        changed = true;
                    }
                }
                if (changed)
                    self->persist_name_cache();
            }
        } else if (!result.success) {
            LOG_WARN("MarketData", "quote_names failed: " + result.error.left(200));
            // A failed call (network down, rate limit) is not "unresolvable":
            // allow these symbols to be asked again on a later refresh.
            for (const auto& s : batch)
                self->names_attempted_.remove(s);
        }
        for (const auto& s : batch)
            self->names_inflight_.remove(s);

        // Re-deliver to every waiter that was waiting on this batch; keep a
        // waiter only while some of its symbols are still queued or in flight.
        const QSet<QString> done(batch.cbegin(), batch.cend());
        QVector<NameWaiter> still_waiting;
        QVector<NameWaiter> to_notify;
        for (auto& w : self->name_waiters_) {
            bool touched = false;
            bool pending = false;
            for (const auto& s : w.symbols) {
                if (done.contains(s))
                    touched = true;
                if (self->names_queued_.contains(s) || self->names_inflight_.contains(s))
                    pending = true;
            }
            if (touched)
                to_notify.append(w);
            if (pending)
                still_waiting.append(std::move(w));
        }
        self->name_waiters_ = std::move(still_waiting);
        for (const auto& w : to_notify) {
            QHash<QString, QString> m;
            for (const auto& s : w.symbols)
                if (self->name_cache_.contains(s))
                    m.insert(s, self->name_cache_.value(s));
            if (w.cb)
                w.cb(m);
        }
    });
}

// ── Yahoo traffic summary ───────────────────────────────────────────────────

void MarketDataService::report_traffic() {
    traffic_.hidden_skips = datahub::DataHub::instance().hidden_skip_count();
    const Traffic total = traffic_;
    const Traffic& was = traffic_reported_;
    Traffic window;
    window.batch_all_calls = total.batch_all_calls - was.batch_all_calls;
    window.quote_symbols = total.quote_symbols - was.quote_symbols;
    window.batch_quotes_calls = total.batch_quotes_calls - was.batch_quotes_calls;
    window.info_chunks = total.info_chunks - was.info_chunks;
    window.quote_names_calls = total.quote_names_calls - was.quote_names_calls;
    window.served_memory = total.served_memory - was.served_memory;
    window.merged = total.merged - was.merged;
    window.rate_limited = total.rate_limited - was.rate_limited;
    window.hidden_skips = total.hidden_skips - was.hidden_skips;
    traffic_reported_ = total;

    // Totals since start, for whoever lists the hub's topics (DataHub
    // inspector, MCP diagnostics).
    QVariantMap diag;
    diag.insert(QStringLiteral("batch_all_calls"), total.batch_all_calls);
    diag.insert(QStringLiteral("quote_symbols_requested"), total.quote_symbols);
    diag.insert(QStringLiteral("batch_quotes_calls"), total.batch_quotes_calls);
    diag.insert(QStringLiteral("batch_info_chunks"), total.info_chunks);
    diag.insert(QStringLiteral("quote_names_calls"), total.quote_names_calls);
    diag.insert(QStringLiteral("served_from_memory"), total.served_memory);
    diag.insert(QStringLiteral("merged_requests"), total.merged);
    diag.insert(QStringLiteral("hidden_skips"), total.hidden_skips);
    diag.insert(QStringLiteral("rate_limited_replies"), total.rate_limited);
    diag.insert(QStringLiteral("updated_ms"), QDateTime::currentMSecsSinceEpoch());
    datahub::DataHub::instance().publish(QStringLiteral("diag:market_data"), diag);

    const quint64 python_calls =
        window.batch_all_calls + window.batch_quotes_calls + window.info_chunks + window.quote_names_calls;
    const quint64 other = window.served_memory + window.merged + window.rate_limited + window.hidden_skips;
    // The daemon's own counters (what reached Yahoo, what its caches answered,
    // its circuit breaker), reset with each read. Asked only of a daemon that
    // is up and was used in this window: the report never starts it and never
    // gives an idle one work.
    auto& worker = python::PythonWorker::instance();
    if (python_calls > 0 && worker.is_ready()) {
        QPointer<MarketDataService> self = this;
        worker.submit(QStringLiteral("yahoo_stats"), QJsonObject{{QStringLiteral("reset"), true}},
                      [self, window](bool ok, QJsonObject result, QString) {
                          if (!self)
                              return;
                          // A script without the action answers with an error: our side alone.
                          const bool have = ok && result.contains(QLatin1String("sent"));
                          LOG_INFO("MarketData", traffic_line(window, have ? &result : nullptr));
                      });
        return;
    }
    if (python_calls + other > 0)
        LOG_INFO("MarketData", traffic_line(window, nullptr));
}

QString MarketDataService::traffic_line(const Traffic& w, const QJsonObject* py) {
    QList<LogField> f;
    f.reserve(22);
    f.emplaceBack("window_s", mds_kTrafficReportMs / 1000);
    f.emplaceBack("batch_all", w.batch_all_calls);
    f.emplaceBack("quote_symbols", w.quote_symbols);
    f.emplaceBack("batch_quotes", w.batch_quotes_calls);
    f.emplaceBack("info_chunks", w.info_chunks);
    f.emplaceBack("quote_names", w.quote_names_calls);
    f.emplaceBack("served_memory", w.served_memory);
    f.emplaceBack("merged", w.merged);
    f.emplaceBack("hidden_skips", w.hidden_skips);
    f.emplaceBack("rate_limited", w.rate_limited);
    if (py) {
        auto num = [py](const char* key) { return static_cast<qint64>(py->value(QLatin1String(key)).toDouble()); };
        QStringList kinds;
        const QJsonObject by_kind = py->value(QLatin1String("sent_by_kind")).toObject();
        for (auto it = by_kind.constBegin(); it != by_kind.constEnd(); ++it)
            kinds << it.key() + QLatin1Char('=') + QString::number(static_cast<qint64>(it.value().toDouble()));
        f.emplaceBack("py_window_s", num("window_s"));
        f.emplaceBack("py_sent", num("sent"));
        f.emplaceBack("py_sent_by_kind", kinds.join(QLatin1Char(',')));
        f.emplaceBack("py_served_memory", num("served_memory"));
        f.emplaceBack("py_served_disk", num("served_disk"));
        f.emplaceBack("py_merged", num("merged"));
        f.emplaceBack("py_waited", num("waited"));
        f.emplaceBack("py_rejected", num("rejected"));
        f.emplaceBack("py_http_429", num("http_429"));
        f.emplaceBack("py_breaker_open", py->value(QLatin1String("breaker_open")).toBool());
        f.emplaceBack("py_all_processes_sent", num("all_processes_sent_total"));
    }
    return logf(QStringLiteral("Yahoo traffic"), f);
}

} // namespace fincept::services
