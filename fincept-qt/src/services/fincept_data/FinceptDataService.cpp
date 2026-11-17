// FinceptDataService.cpp — see the header for the policy (billing-aware cache,
// in-flight sharing, Retry-After holds, capability discovery).
#include "services/fincept_data/FinceptDataService.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "services/billing/PlanAccess.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QLocale>
#include <QTime>
#include <QTimer>

#include <algorithm>

namespace fincept::services::fincept_data {

namespace fdata_service_detail {

constexpr const char* kTag = "FinceptData";
constexpr const char* kDatasetsPath = "/data/datasets";

/// A copy is reused at least this long after it arrived, even when the server said its
/// freshness window was nearly over — a tab switch or a re-open must not re-bill.
constexpr qint64 kFreshFloorMs = 30 * 1000;
/// A stale answer (the source is failing) is reused this long: asking again bills
/// again and almost always returns the same stale copy.
constexpr qint64 kStaleFloorMs = 2 * 60 * 1000;
/// 404 / 422 answers are reused this long unless the user refreshes (the server keeps
/// its own "does not exist" for 5 min).
constexpr qint64 kNegativeTtlMs = 5 * 60 * 1000;
/// After a discovery that failed in transit: send calls optimistically, check again after.
constexpr qint64 kDiscoveryRetryMs = 60 * 1000;
/// Client copies kept (options chains are the largest: a few hundred contracts).
constexpr int kMaxCacheEntries = 64;
constexpr int kMaxNegativeEntries = 64;

/// Hold key for the plan-wide request limit (429): every dataset waits.
const QString& plan_hold_key() {
    static const QString k = QStringLiteral("*");
    return k;
}

} // namespace fdata_service_detail

using namespace fdata_service_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Lifetime + account
// ═════════════════════════════════════════════════════════════════════════════

FinceptDataService& FinceptDataService::instance() {
    static FinceptDataService s;
    return s;
}

FinceptDataService::FinceptDataService() {
    // Constructed on first use (a tab / screen asking), so an unused feature costs
    // nothing. The data is public and user-agnostic, but billing is per account, and
    // what a server offers is checked once per sign-in.
    identity_ = current_identity();
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &FinceptDataService::on_auth_state_changed);
}

bool FinceptDataService::signed_in() {
    const auto& auth = auth::AuthManager::instance();
    return auth.is_authenticated() && !auth.session().api_key.isEmpty();
}

QByteArray FinceptDataService::current_identity() {
    if (!signed_in())
        return {};
    return QCryptographicHash::hash(auth::AuthManager::instance().session().api_key.toUtf8(),
                                    QCryptographicHash::Sha256);
}

void FinceptDataService::on_auth_state_changed() {
    // auth_state_changed also fires on every periodic profile refresh: only a
    // different account (or a sign-in / sign-out) resets anything.
    const QByteArray id = current_identity();
    if (id == identity_)
        return;
    identity_ = id;
    reset_for_new_account();
}

void FinceptDataService::reset_for_new_account() {
    ++generation_;
    cache_.clear();
    negative_.clear();
    holds_.clear();
    catalog_ = {};
    discovery_retry_ms_ = 0;
    // In-flight requests keep their waiters: their answers are still delivered (public
    // data), just not kept (generation check in on_reply()).
    set_availability(Availability::Unknown);

    if (identity_.isEmpty()) {
        // Signed out: whoever waited for the check is told so now.
        auto queued = std::move(awaiting_discovery_);
        awaiting_discovery_.clear();
        for (const auto& r : queued)
            deliver_later(r.waiter, local_error(ErrorKind::NotSignedIn));
        return;
    }
    // A new sign-in: re-check what this server offers — only if the feature was used in
    // this run (or someone is waiting), so an unused feature still sends nothing.
    if (!awaiting_discovery_.isEmpty() || discovery_used_)
        start_discovery();
}

// ═════════════════════════════════════════════════════════════════════════════
// Discovery
// ═════════════════════════════════════════════════════════════════════════════

void FinceptDataService::set_availability(Availability a) {
    if (availability_ == a)
        return;
    availability_ = a;
    emit availability_changed();
}

FinceptDataService::DatasetState FinceptDataService::dataset_state(const QString& dataset_id) const {
    if (availability_ == Availability::Unavailable)
        return DatasetState::NotOnServer;
    if (availability_ != Availability::Available)
        return DatasetState::Unknown;
    const auto it = catalog_.datasets.constFind(dataset_id);
    if (it == catalog_.datasets.constEnd())
        return DatasetState::NotOnServer;
    return it->available ? DatasetState::Available : DatasetState::NotConfigured;
}

void FinceptDataService::start_discovery() {
    if (availability_ == Availability::Checking)
        return;
    discovery_used_ = true;
    set_availability(Availability::Checking);
    const quint64 gen = generation_;
    QPointer<FinceptDataService> self = this;
    HttpClient::instance().get(
        QString::fromLatin1(kDatasetsPath),
        [self, gen](Result<QJsonDocument> reply) {
            if (self)
                self->on_discovery(gen, reply);
        },
        this);
}

void FinceptDataService::on_discovery(quint64 generation, const Result<QJsonDocument>& reply) {
    if (generation != generation_)
        return; // the account changed while this ran; reset_for_new_account() decided what to do
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (reply.is_ok()) {
        const DataResult r = parse_envelope(reply.value());
        if (r.ok()) {
            catalog_ = parse_catalog(r.data);
            LOG_INFO(kTag, QString("Data API available: %1 datasets").arg(catalog_.datasets.size()));
            set_availability(Availability::Available);
            release_awaiting();
            return;
        }
        LOG_WARN(kTag, "GET /data/datasets: unexpected answer — continuing without the catalogue");
        discovery_retry_ms_ = now + kDiscoveryRetryMs;
        set_availability(Availability::Unknown);
        release_awaiting();
        return;
    }

    const int status = HttpClient::status_from_error(reply.error());
    if (status == 404) {
        // The production server before its upgrade: no /data routes at all. Remember it
        // for this sign-in; every call now fails locally and calmly.
        LOG_INFO(kTag, "This Fincept server has no /data API yet — data tabs show a notice until it is upgraded");
        set_availability(Availability::Unavailable);
        release_awaiting();
        return;
    }

    discovery_retry_ms_ = now + kDiscoveryRetryMs;
    set_availability(Availability::Unknown);
    if (status == 401) {
        // The key / session is being dealt with by AuthManager; sending the queued calls
        // would only add 401s. They are told to sign in; the next call checks again.
        auto queued = std::move(awaiting_discovery_);
        awaiting_discovery_.clear();
        for (const auto& r : queued)
            deliver_later(r.waiter, local_error(ErrorKind::NotSignedIn));
        return;
    }
    // In transit (offline, 5xx, IP block, plan limit): send the calls anyway — a missing
    // route still shows up as a "Not Found" 404 on the call itself.
    release_awaiting();
}

void FinceptDataService::release_awaiting() {
    auto queued = std::move(awaiting_discovery_);
    awaiting_discovery_.clear();
    for (auto& r : queued)
        dispatch(std::move(r));
}

// ═════════════════════════════════════════════════════════════════════════════
// Requests
// ═════════════════════════════════════════════════════════════════════════════

void FinceptDataService::fetch(const QString& dataset_id, const QString& path, const QueryParams& query,
                               const QObject* context, Callback callback, bool refresh) {
    Request r;
    r.dataset_id = dataset_id;
    r.key = request_key(path, query);
    r.refresh = refresh;
    r.waiter.context = const_cast<QObject*>(context);
    r.waiter.scoped = context != nullptr;
    r.waiter.callback = std::move(callback);
    dispatch(std::move(r));
}

void FinceptDataService::dispatch(Request r) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!signed_in()) {
        deliver_later(r.waiter, local_error(ErrorKind::NotSignedIn));
        return;
    }
    if (availability_ == Availability::Unavailable) {
        deliver_later(r.waiter, local_error(ErrorKind::ServerLacksApi, QStringLiteral("missing_route")));
        return;
    }
    if (availability_ == Availability::Checking ||
        (availability_ == Availability::Unknown && now >= discovery_retry_ms_)) {
        awaiting_discovery_.append(std::move(r));
        start_discovery();
        return;
    }
    switch (dataset_state(r.dataset_id)) {
    case DatasetState::NotOnServer:
        deliver_later(r.waiter, local_error(ErrorKind::DatasetUnavailable, QStringLiteral("not_offered")));
        return;
    case DatasetState::NotConfigured:
        deliver_later(r.waiter, local_error(ErrorKind::DatasetUnavailable, QStringLiteral("not_configured")));
        return;
    case DatasetState::Available:
    case DatasetState::Unknown:
        break;
    }

    // The client copy: free.
    if (const auto it = cache_.constFind(r.key); it != cache_.constEnd()) {
        if (now < (r.refresh ? it->fresh_until_ms : it->keep_until_ms)) {
            DataResult copy = it->result;
            copy.from_client_cache = true;
            deliver_later(r.waiter, copy);
            return;
        }
    }
    if (!r.refresh) {
        if (const auto it = negative_.constFind(r.key); it != negative_.constEnd() && now < it->keep_until_ms) {
            deliver_later(r.waiter, it->result);
            return;
        }
    }

    // Retry-After: not before the source (or the plan limit) allows it.
    if (const Hold* hold = active_hold(r.dataset_id, now)) {
        DataResult held;
        held.error = hold->error;
        held.error.retry_after_s = static_cast<int>((hold->until_ms - now + 999) / 1000);
        held.received_ms = now;
        deliver_later(r.waiter, held);
        return;
    }

    // An identical request in flight: share its answer.
    if (const auto it = inflight_.find(r.key); it != inflight_.end()) {
        it->append(r.waiter);
        return;
    }
    inflight_.insert(r.key, {r.waiter});
    send(r.dataset_id, r.key);
}

void FinceptDataService::send(const QString& dataset_id, const QString& key) {
    const quint64 gen = generation_;
    QPointer<FinceptDataService> self = this;
    HttpClient::instance().get(
        key,
        [self, dataset_id, key, gen](Result<QJsonDocument> reply) {
            if (self)
                self->on_reply(dataset_id, key, gen, reply);
        },
        this);
}

void FinceptDataService::on_reply(const QString& dataset_id, const QString& key, quint64 generation,
                                  const Result<QJsonDocument>& reply) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    DataResult result;
    if (reply.is_ok()) {
        result = parse_envelope(reply.value());
        if (!result.ok() && result.error.message.isEmpty() && reply.value().isObject())
            result.error.message = HttpClient::error_text_from_body(reply.value().object());
    } else {
        // The headers say exactly what the sentence only implies: how long to wait
        // (Retry-After), the kind (X-Data-Error) and close symbol matches
        // (X-Symbol-Suggestions). Read here, inside HttpClient's callback, where
        // error_header() is valid. Absent ones (an older server, a proxy, a
        // transport failure) leave classify_error() to infer from status + wording.
        const std::string& err = reply.error();
        bool retry_ok = false;
        const int retry_after = HttpClient::error_header("Retry-After").trimmed().toInt(&retry_ok);
        result.error = classify_error(HttpClient::status_from_error(err), HttpClient::message_from_error(err),
                                      retry_ok && retry_after > 0 ? retry_after : 0,
                                      QString::fromUtf8(HttpClient::error_header("X-Symbol-Suggestions")),
                                      QString::fromUtf8(HttpClient::error_header("X-Data-Error")));
    }
    result.received_ms = now;

    if (generation == generation_)
        record(dataset_id, key, result, now);
    if (!result.ok() && result.error.kind != ErrorKind::NotFound)
        LOG_DEBUG(kTag, QString("%1: %2 (HTTP %3)").arg(dataset_id, result.error.code).arg(result.error.status));

    const QVector<Waiter> waiters = inflight_.take(key);
    deliver(waiters, result);
}

void FinceptDataService::record(const QString& dataset_id, const QString& key, const DataResult& result,
                                qint64 now) {
    if (result.ok()) {
        CacheEntry e;
        e.result = result;
        const qint64 remaining_s = std::max(0, result.meta.max_age_seconds - result.meta.age_seconds);
        e.fresh_until_ms = now + remaining_s * 1000;
        e.keep_until_ms = std::max(e.fresh_until_ms, now + (result.meta.stale ? kStaleFloorMs : kFreshFloorMs));
        cache_.insert(key, e);
        negative_.remove(key);
        trim_cache();
        return;
    }

    const DataError& err = result.error;
    switch (err.kind) {
    case ErrorKind::NotFound:
    case ErrorKind::Invalid: {
        CacheEntry e;
        e.result = result;
        e.keep_until_ms = now + kNegativeTtlMs;
        if (negative_.size() >= kMaxNegativeEntries)
            negative_.clear();
        negative_.insert(key, e);
        break;
    }
    case ErrorKind::Throttled:
    case ErrorKind::NotConfigured:
    case ErrorKind::RateLimited: {
        const int wait_s = err.retry_after_s > 0 ? err.retry_after_s : default_retry_after_s(err);
        if (wait_s <= 0)
            break;
        Hold& h = holds_[err.kind == ErrorKind::RateLimited ? plan_hold_key() : hold_key(dataset_id)];
        h.until_ms = std::max(h.until_ms, now + static_cast<qint64>(wait_s) * 1000);
        h.error = err;
        break;
    }
    case ErrorKind::ServerLacksApi:
        // The route itself is missing: this server predates the /data API (discovery
        // was skipped or failed in transit). Same outcome as a 404 on discovery.
        if (availability_ != Availability::Available) {
            LOG_INFO(kTag, "This Fincept server has no /data API yet");
            set_availability(Availability::Unavailable);
        }
        break;
    default:
        break;
    }
}

void FinceptDataService::trim_cache() {
    while (cache_.size() > kMaxCacheEntries) {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
            if (it->result.received_ms < oldest->result.received_ms)
                oldest = it;
        cache_.erase(oldest);
    }
}

QString FinceptDataService::hold_key(const QString& dataset_id) const {
    // The server throttles per source (cboe, yahoo, ...): every dataset of that source waits.
    const auto it = catalog_.datasets.constFind(dataset_id);
    if (it != catalog_.datasets.constEnd() && !it->source_id.isEmpty())
        return QStringLiteral("source:") + it->source_id;
    return QStringLiteral("dataset:") + dataset_id;
}

const FinceptDataService::Hold* FinceptDataService::active_hold(const QString& dataset_id, qint64 now) const {
    for (const QString& k : {plan_hold_key(), hold_key(dataset_id)}) {
        const auto it = holds_.constFind(k);
        if (it != holds_.constEnd() && now < it->until_ms)
            return &it.value();
    }
    return nullptr;
}

void FinceptDataService::cancel(const QObject* context) {
    if (!context)
        return;
    for (auto it = inflight_.begin(); it != inflight_.end(); ++it) {
        auto& ws = it.value();
        ws.erase(std::remove_if(ws.begin(), ws.end(),
                                [context](const Waiter& w) { return w.scoped && w.context.data() == context; }),
                 ws.end());
    }
    awaiting_discovery_.erase(std::remove_if(awaiting_discovery_.begin(), awaiting_discovery_.end(),
                                             [context](const Request& r) {
                                                 return r.waiter.scoped && r.waiter.context.data() == context;
                                             }),
                              awaiting_discovery_.end());
}

bool FinceptDataService::is_fresh(const QString& path, const QueryParams& query) const {
    const auto it = cache_.constFind(request_key(path, query));
    return it != cache_.constEnd() && QDateTime::currentMSecsSinceEpoch() < it->fresh_until_ms;
}

void FinceptDataService::deliver(const QVector<Waiter>& waiters, const DataResult& result) {
    for (const auto& w : waiters) {
        if (w.scoped && !w.context)
            continue; // its widget is gone
        if (w.callback)
            w.callback(result);
    }
}

void FinceptDataService::deliver_later(const Waiter& waiter, const DataResult& result) {
    // Never inside the caller's own call: callers set "loading" state after asking.
    QTimer::singleShot(0, this, [waiter, result]() {
        if (waiter.scoped && !waiter.context)
            return;
        if (waiter.callback)
            waiter.callback(result);
    });
}

DataResult FinceptDataService::local_error(ErrorKind kind, const QString& code, const QString& message) {
    DataResult r;
    r.error.kind = kind;
    r.error.code = code;
    r.error.message = message;
    r.received_ms = QDateTime::currentMSecsSinceEpoch();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// Typed requests
// ═════════════════════════════════════════════════════════════════════════════

void FinceptDataService::fetch_option_chain(const QString& symbol, const QString& expiration, int strikes_around_atm,
                                            int page, int page_size, const QObject* context, Callback callback,
                                            bool refresh) {
    QueryParams q{{QStringLiteral("page"), QString::number(std::clamp(page, 1, 1000))},
                  {QStringLiteral("page_size"), QString::number(std::clamp(page_size, 1, 500))}};
    if (strikes_around_atm > 0)
        q.append({QStringLiteral("strikes_around_atm"), QString::number(std::clamp(strikes_around_atm, 1, 100))});
    if (!expiration.isEmpty())
        q.append({QStringLiteral("expiration"), expiration});
    fetch(QStringLiteral("options.chain"),
          QStringLiteral("/data/options/") + path_segment(symbol) + QStringLiteral("/chain"), q, context,
          std::move(callback), refresh);
}

void FinceptDataService::fetch_analyst(const QString& symbol, int upgrades_limit, const QObject* context,
                                       Callback callback, bool refresh) {
    fetch(QStringLiteral("analyst.overview"), QStringLiteral("/data/analyst/") + path_segment(symbol),
          {{QStringLiteral("upgrades_limit"), QString::number(std::clamp(upgrades_limit, 0, 500))}}, context,
          std::move(callback), refresh);
}

void FinceptDataService::fetch_ownership(const QString& symbol, int transactions_limit, const QObject* context,
                                         Callback callback, bool refresh) {
    fetch(QStringLiteral("ownership.overview"), QStringLiteral("/data/ownership/") + path_segment(symbol),
          {{QStringLiteral("transactions_limit"), QString::number(std::clamp(transactions_limit, 0, 200))}}, context,
          std::move(callback), refresh);
}

void FinceptDataService::fetch_calendar(CalendarKind kind, const QDate& start, const QDate& end,
                                        const QString& symbol, const QString& sort, int page, int page_size,
                                        const QObject* context, Callback callback, bool refresh) {
    QueryParams q{{QStringLiteral("start_date"), start.toString(Qt::ISODate)},
                  {QStringLiteral("end_date"), end.toString(Qt::ISODate)},
                  {QStringLiteral("page"), QString::number(std::clamp(page, 1, 500))},
                  {QStringLiteral("page_size"), QString::number(std::clamp(page_size, 1, 100))}};
    if (calendar_supports_symbol(kind) && !symbol.trimmed().isEmpty())
        q.append({QStringLiteral("symbol"), symbol.trimmed().toUpper()});
    if (kind == CalendarKind::Earnings && !sort.isEmpty())
        q.append({QStringLiteral("sort"), sort});
    fetch(calendar_dataset_id(kind), calendar_path(kind), q, context, std::move(callback), refresh);
}

// ═════════════════════════════════════════════════════════════════════════════
// Text
// ═════════════════════════════════════════════════════════════════════════════

QString FinceptDataService::unavailable_text() {
    return tr("This data needs a newer Fincept server — it will appear here once the server is upgraded.");
}

QString FinceptDataService::age_text(int seconds) {
    if (seconds < 60)
        return tr("<1 min");
    if (seconds < 3600)
        return tr("%n min", nullptr, seconds / 60);
    if (seconds < 86400)
        return tr("%n h", nullptr, seconds / 3600);
    return tr("%n d", nullptr, seconds / 86400);
}

QString FinceptDataService::error_text(const DataError& e) {
    const QString server = e.message;
    const QString retry_at = e.retry_after_s > 0
                                 ? QTime::currentTime().addSecs(e.retry_after_s).toString(QStringLiteral("HH:mm:ss"))
                                 : QString();
    switch (e.kind) {
    case ErrorKind::None:
        return {};
    case ErrorKind::ServerLacksApi:
        return unavailable_text();
    case ErrorKind::DatasetUnavailable:
        return e.code == QLatin1String("not_configured")
                   ? tr("This dataset is not available on this Fincept server: its data source is not configured there.")
                   : tr("This Fincept server does not offer this dataset yet — it will appear here once the server is "
                        "upgraded.");
    case ErrorKind::NotSignedIn:
        return tr("Sign in to Fincept to load this data.");
    case ErrorKind::NoCredits: {
        QString text = server.isEmpty() ? tr("Not enough Fincept credits for this request. Nothing was charged.")
                                        : tr("%1 Nothing was charged.").arg(server);
        // On Free, say what Pro changes for this data (empty on Pro / older servers).
        return fincept::billing::PlanAccess::instance().with_upgrade_note(text, QStringLiteral("market_data"));
    }
    case ErrorKind::Forbidden:
        return server.isEmpty() ? tr("The Fincept server refused this request (HTTP 403).") : server;
    case ErrorKind::NotFound:
        return server.isEmpty() ? tr("No data found for this request.") : server;
    case ErrorKind::Invalid:
        return server.isEmpty() ? tr("The server did not accept this request.")
                                : tr("The server did not accept this request: %1").arg(server);
    case ErrorKind::RateLimited:
        return (server.isEmpty() ? tr("Your plan's request limit was reached.") : server) +
               (retry_at.isEmpty() ? QString() : QLatin1Char(' ') + tr("Try again after %1.").arg(retry_at));
    case ErrorKind::Throttled:
        return e.code == QLatin1String("busy")
                   ? tr("The data source is at its request limit on this server. Try again after %1.").arg(retry_at)
                   : tr("The data source is rate-limiting requests. Try again after %1.").arg(retry_at);
    case ErrorKind::NotConfigured:
        return tr("This data source is not configured on this Fincept server.");
    case ErrorKind::Upstream:
        return tr("%1 — try again later.")
            .arg(server.isEmpty() ? tr("The data source is not answering") : server);
    case ErrorKind::Timeout:
        return tr("The data source did not answer in time — try again in a moment.");
    case ErrorKind::Network:
        return server.isEmpty() ? tr("Could not reach the Fincept server.")
                                : tr("Could not reach the Fincept server: %1").arg(server);
    case ErrorKind::BadResponse:
        return server.isEmpty() ? tr("The Fincept server sent an unexpected answer.") : server;
    case ErrorKind::Server:
        return server.isEmpty() ? tr("The Fincept server answered HTTP %1.").arg(e.status)
                                : tr("The Fincept server answered HTTP %1: %2").arg(e.status).arg(server);
    }
    return server;
}

QString FinceptDataService::freshness_text(const DataResult& r, qint64 now_ms) {
    if (!r.ok())
        return {};
    QStringList parts;
    if (r.meta.as_of.isValid()) {
        const QDateTime local = r.meta.as_of.toLocalTime();
        const bool today = local.date() == QDate::currentDate();
        parts << tr("as of %1").arg(local.toString(today ? QStringLiteral("HH:mm") : QStringLiteral("MMM d HH:mm")));
        parts << tr("%1 old").arg(age_text(r.age_seconds_at(now_ms)));
    }
    if (!r.meta.source_name.isEmpty())
        parts << r.meta.source_name;
    if (r.meta.delayed_minutes > 0)
        parts << tr("%1-min delayed").arg(r.meta.delayed_minutes);
    QString text = parts.join(QStringLiteral(" · "));
    if (r.meta.stale) {
        const QString why = r.meta.stale_reason == QLatin1String("throttled") ? tr("the source is rate-limiting")
                            : r.meta.stale_reason == QLatin1String("timeout") ? tr("the source timed out")
                                                                              : tr("the source is not answering");
        text = tr("STALE (%1) — %2").arg(why, text);
    }
    return text;
}

} // namespace fincept::services::fincept_data
