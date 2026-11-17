// src/services/maritime/MaritimeService.cpp
#include "services/maritime/MaritimeService.h"

#include "auth/AuthManager.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QThreadPool>

#include <algorithm>
#include <chrono>
#include <utility>

namespace fincept::services::maritime {

namespace {
inline void publish_to_hub(const QString& topic, const QVariant& value) {
    fincept::datahub::DataHub::instance().publish(topic, value);
}

// A refusal that waiting does not fix (signed out, no credits, not allowed) backs
// off for hours instead of the hub's 5-minute ladder cap; a sign-in, a balance
// increase or the user's refresh lifts it (MaritimeService::release_hub_refusal).
constexpr std::chrono::milliseconds kMaritimeSvcRefusedRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 24;
constexpr std::chrono::milliseconds kMaritimeSvcIpBlockRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 2;

// marine.py VESSEL_POSITION_CREDIT_COST — the cheapest hub-driven call.
constexpr double kMaritimeSvcCreditCost = 1.0;

// A hub-driven refresh that fails must say so: the hub marks the topic in-flight and
// otherwise waits out refresh_timeout_ms before it can retry. Returns the kind of
// refusal (see above), or None for a transient failure the hub's ladder retries.
inline fincept::auth::RefusalLatch::Kind maritime_svc_publish_error(const QString& topic, const QString& message) {
    using Kind = fincept::auth::RefusalLatch::Kind;
    const int status = HttpClient::status_from_error(message.toStdString());
    auto& hub = fincept::datahub::DataHub::instance();
    if (status == 401) {
        hub.publish_error(topic, QStringLiteral("Sign in to Fincept to load vessel data."), kMaritimeSvcRefusedRetry);
        return Kind::Access;
    }
    if (status == 402) {
        hub.publish_error(topic,
                          QStringLiteral("Out of Fincept credits (1 per vessel update). Top up or upgrade in Plans & "
                                         "Pricing, then refresh — paused until then."),
                          kMaritimeSvcRefusedRetry);
        return Kind::Credits;
    }
    if (status == 403) {
        const bool ip = HttpClient::instance().is_ip_blocked();
        hub.publish_error(topic, HttpClient::message_from_error(message.toStdString()).left(200),
                          ip ? kMaritimeSvcIpBlockRetry : kMaritimeSvcRefusedRetry);
        return ip ? Kind::None : Kind::Access;
    }
    hub.publish_error(topic, message.left(200));
    return Kind::None;
}

// One line per failed request: HTTP 402 (out of credits) and 404 (this server does not
// offer the route) are expected refusals (INFO); anything else is a WARN with its status.
void maritime_svc_log_failure(const char* what, const std::string& error) {
    const int status = HttpClient::status_from_error(error);
    const QString line = logf(QString::fromLatin1(what) + QStringLiteral(" failed"),
                              {{"status", status}, {"error", HttpClient::message_from_error(error).left(200)}});
    if (status == 402 || status == 404)
        LOG_INFO("Maritime", line);
    else
        LOG_WARN("Maritime", line);
}
} // namespace

static QString marine_base() {
    return fincept::AppConfig::instance().api_base_url() + QStringLiteral("/marine");
}
// Every call is billed. Positions used to be re-bought every 60 s per vessel on a
// dashboard tile (60 credits an hour each); AIS fixes don't move a map pin
// meaningfully in that time. The user's REFRESH / ⟳ still fetches at once.
static constexpr int kVesselTtlSec = 10 * 60;  // positions (single / multi): 10 min
static constexpr int kAreaTtlSec = 10 * 60;    // area search (8 credits): 10 min
static constexpr int kHistoryTtlSec = 30 * 60; // voyage history (5 credits): 30 min
/// How long an area result is kept as the "last result" a screen shows on open
/// (with its fetch time) instead of buying it again.
static constexpr int kAreaKeepSec = 14 * 24 * 3600;

static QString maritime_svc_area_suffix(const AreaSearchParams& params) {
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(params.min_lat)
        .arg(params.max_lat)
        .arg(params.min_lng)
        .arg(params.max_lng)
        .arg(params.days_ago)
        .arg(params.limit);
}

static QString maritime_svc_area_key(const AreaSearchParams& params) {
    return QStringLiteral("maritime:area:") + maritime_svc_area_suffix(params);
}

/// Side entry next to each kept area copy: just its fetch time, so "is there a
/// last result, and how old?" never reads (or parses) the copy itself.
static QString maritime_svc_area_meta_key(const AreaSearchParams& params) {
    return QStringLiteral("maritime:area_meta:") + maritime_svc_area_suffix(params);
}

// ── Singleton ────────────────────────────────────────────────────────────────
MaritimeService& MaritimeService::instance() {
    static MaritimeService inst;
    return inst;
}

MaritimeService::MaritimeService(QObject* parent) : QObject(parent) {}

// The API reports numeric fields as strings ("last_pos_latitude": "51.92"). A real JSON
// number made toString() return "" and the vessel silently landed on 0,0 (and got
// skipped by the map), so accept both.
static double maritime_svc_json_num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : v.toString().toDouble();
}

// Credit balances are NUMERIC (fractional); toInt() turned every fractional balance
// into "not reported". Absent / unparsable -> -1.
static double maritime_svc_credits(const QJsonValue& v) {
    if (v.isDouble())
        return v.toDouble();
    bool ok = false;
    const double d = v.toString().trimmed().toDouble(&ok);
    return ok ? d : -1.0;
}

// Serialise a vessel back into the API's own field names for the CacheManager. One
// writer for every cached page: the multi-vessel and history writers each used to keep
// their own subset, so a cached replay lost draught / route dates / progress.
static QJsonObject maritime_svc_vessel_to_json(const VesselData& v) {
    QJsonObject o;
    o["id"] = v.id;
    o["imo"] = v.imo;
    o["name"] = v.name;
    o["last_pos_latitude"] = QString::number(v.latitude, 'f', 8);
    o["last_pos_longitude"] = QString::number(v.longitude, 'f', 8);
    o["last_pos_speed"] = QString::number(v.speed);
    o["last_pos_angle"] = QString::number(v.angle);
    o["route_from_port_name"] = v.from_port;
    o["route_to_port_name"] = v.to_port;
    o["route_from_date"] = v.from_date;
    o["route_to_date"] = v.to_date;
    o["route_progress"] = QString::number(v.route_progress);
    o["current_draught"] = QString::number(v.draught);
    o["last_pos_updated_at"] = v.last_updated;
    o["fetched_at"] = v.fetched_at;
    return o;
}

// ── Parse vessel from JSON ───────────────────────────────────────────────────
VesselData MaritimeService::parse_vessel(const QJsonObject& obj) const {
    VesselData v;
    v.id = obj["id"].toInt();
    v.imo = obj["imo"].toString();
    v.name = obj["name"].toString();
    v.latitude = maritime_svc_json_num(obj["last_pos_latitude"]);
    v.longitude = maritime_svc_json_num(obj["last_pos_longitude"]);
    v.speed = maritime_svc_json_num(obj["last_pos_speed"]);
    v.angle = maritime_svc_json_num(obj["last_pos_angle"]);
    v.from_port = obj["route_from_port_name"].toString();
    v.to_port = obj["route_to_port_name"].toString();
    v.from_date = obj["route_from_date"].toString();
    v.to_date = obj["route_to_date"].toString();
    v.route_progress = maritime_svc_json_num(obj["route_progress"]);
    v.draught = maritime_svc_json_num(obj["current_draught"]);
    v.last_updated = obj["last_pos_updated_at"].toString();
    v.fetched_at = obj["fetched_at"].toString();
    return v;
}

// ── Helper: unwrap {success, data: {...}} envelope ──────────────────────────
static QJsonObject unwrap(const QJsonObject& root) {
    if (root.contains("data") && root["data"].isObject())
        return root["data"].toObject();
    return root;
}

// ── Area search — POST /marine/vessel/area-search (8 credits/call) ──────────
//
// Returns the vessels whose last known position lies inside the bounding box.
// Costly (8 credits). `limit` asks for the most recently updated N (newer
// servers; vessel_count stays the true total in the box, returned_count is the
// slice) — older servers ignore it and return every vessel (10k+ for a large
// box), which is capped client-side below. The response shape is:
//   {data: {search_area:{...}, vessels:[...], vessel_count:N, returned_count:N,
//           credits_used:N, remaining_credits:N}}

bool MaritimeService::emit_cached_area(const QString& cache_key, int max_age_sec) {
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (cached.isNull())
        return false;
    const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
    const qint64 fetched_at = static_cast<qint64>(root["fetched_at_ms"].toDouble(0));
    if (max_age_sec > 0 &&
        (fetched_at <= 0 || QDateTime::currentMSecsSinceEpoch() - fetched_at > qint64(max_age_sec) * 1000))
        return false;
    VesselsPage page;
    const QJsonArray arr = root["vessels"].toArray();
    page.vessels.reserve(arr.size());
    for (const auto& v : arr)
        page.vessels.append(parse_vessel(v.toObject()));
    page.total_count = root["total_count"].toInt(page.vessels.size());
    page.credits_used = root["credits_used"].toDouble(0.0);
    page.remaining_credits = -1; // cache hit: free, and the stored balance is stale
    page.fetched_at_ms = fetched_at;
    page.request_key = cache_key;
    emit vessels_loaded(page);
    return true;
}

QString MaritimeService::area_request_key(const AreaSearchParams& params) {
    return maritime_svc_area_key(params);
}

bool MaritimeService::area_cached_at(const AreaSearchParams& params, qint64* fetched_at_ms) const {
    const QVariant meta = fincept::CacheManager::instance().get(maritime_svc_area_meta_key(params));
    if (meta.isNull())
        return false;
    bool ok = false;
    const qint64 at = meta.toString().toLongLong(&ok);
    if (!ok || at <= 0)
        return false;
    if (fetched_at_ms)
        *fetched_at_ms = at;
    return true;
}

bool MaritimeService::load_area_cached(const AreaSearchParams& params, QObject* context,
                                       std::function<void(VesselsPage)> done) {
    // The text is read here (one cache row); the JSON (up to ~0.7 MB) and the
    // vessels are parsed on a worker.
    const QVariant cached = fincept::CacheManager::instance().get(maritime_svc_area_key(params));
    if (cached.isNull() || !context || !done)
        return false;
    QPointer<QObject> ctx = context;
    QThreadPool::globalInstance()->start([this, ctx, done = std::move(done), text = cached.toString(),
                                          key = maritime_svc_area_key(params)]() {
        const QJsonObject root = QJsonDocument::fromJson(text.toUtf8()).object();
        VesselsPage page;
        const QJsonArray arr = root["vessels"].toArray();
        page.vessels.reserve(arr.size());
        for (const auto& v : arr)
            page.vessels.append(parse_vessel(v.toObject())); // reads only its argument
        page.total_count = root["total_count"].toInt(page.vessels.size());
        page.credits_used = root["credits_used"].toDouble(0.0);
        page.remaining_credits = -1; // a kept copy: free, and its stored balance is stale
        page.fetched_at_ms = static_cast<qint64>(root["fetched_at_ms"].toDouble(0));
        page.request_key = key;
        if (!ctx)
            return;
        QMetaObject::invokeMethod(
            ctx.data(),
            [ctx, done, page = std::move(page)]() {
                if (ctx)
                    done(page);
            },
            Qt::QueuedConnection);
    });
    return true;
}

void MaritimeService::search_vessels_by_area(const AreaSearchParams& params, bool force) {
    QJsonObject body;
    body["min_lat"] = params.min_lat;
    body["max_lat"] = params.max_lat;
    body["min_lng"] = params.min_lng;
    body["max_lng"] = params.max_lng;
    if (params.days_ago > 0)
        body["days_ago"] = params.days_ago;
    if (params.limit > 0)
        body["limit"] = params.limit;

    const QString cache_key = maritime_svc_area_key(params);

    // Client-side safety net: if the API ignores `limit` and returns the full
    // unbounded set, cap the working set before we parse/sort/re-serialize so
    // the UI thread never chews through tens of thousands of vessels.
    const int hard_cap = params.limit > 0 ? params.limit : 0;
    // The copy is kept for kAreaKeepSec (the "last result" a screen shows on
    // open); a search answers from it only while it is younger than kAreaTtlSec.
    if (!force && emit_cached_area(cache_key, kAreaTtlSec))
        return;

    // The deployed API (api.fincept.in, 2026-11) does not know `limit` yet: a
    // world-wide box answers with every vessel (~45k rows, tens of MB) after a
    // query that can run well past the client-wide 20 s. So: a 90 s transfer
    // timeout, the JSON parsed off the GUI thread (post_large), and the
    // selection / parse / cache serialisation below on a worker thread as well.
    QPointer<MaritimeService> self = this;
    HttpClient::instance().post_large(
        marine_base() + "/vessel/area-search", body, 90 * 1000,
        [self, cache_key, hard_cap](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                maritime_svc_log_failure("Area search", result.error());
                emit self->error_occurred("area_search", QString::fromStdString(result.error()));
                return;
            }
            MaritimeService* svc = self.data(); // app-lifetime singleton
            QThreadPool::globalInstance()->start([self, svc, cache_key, hard_cap, doc = result.value()]() {
                const auto data = unwrap(doc.object());
                const auto vessels_arr = data["vessels"].toArray();

                // Parse at most `hard_cap` vessels (0 = unbounded). Servers that
                // honour `limit` already send the most recently updated slice; an
                // older server ignores it and sends every vessel in the box ordered
                // by IMO, so the head of that array would be the lowest IMO numbers.
                // Pick the `hard_cap` most recent fixes by their timestamp alone
                // before paying for the full per-vessel parse.
                const int n_all = static_cast<int>(vessels_arr.size());
                QVector<int> chosen;
                if (hard_cap > 0 && n_all > hard_cap) {
                    QVector<QPair<QString, int>> stamps;
                    stamps.reserve(n_all);
                    for (int i = 0; i < n_all; ++i)
                        stamps.append({vessels_arr[i].toObject().value(QStringLiteral("last_pos_updated_at")).toString(), i});
                    std::nth_element(stamps.begin(), stamps.begin() + (hard_cap - 1), stamps.end(),
                                     [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
                                         return a.first > b.first; // newest (ISO-8601) first
                                     });
                    chosen.reserve(hard_cap);
                    for (int i = 0; i < hard_cap; ++i)
                        chosen.append(stamps[i].second);
                } else {
                    chosen.reserve(n_all);
                    for (int i = 0; i < n_all; ++i)
                        chosen.append(i);
                }
                VesselsPage page;
                page.vessels.reserve(chosen.size());
                for (int i : chosen)
                    page.vessels.append(svc->parse_vessel(vessels_arr[i].toObject())); // reads only its argument

                // Newest position first — last_pos_updated_at is ISO-8601 so plain
                // string compare gives correct lexicographic ordering.
                std::sort(page.vessels.begin(), page.vessels.end(),
                          [](const VesselData& a, const VesselData& b) { return a.last_updated > b.last_updated; });

                page.total_count = data["vessel_count"].toInt(page.vessels.size());
                page.credits_used = data["credits_used"].toDouble(0.0);
                page.remaining_credits = maritime_svc_credits(data["remaining_credits"]);
                page.fetched_at_ms = QDateTime::currentMSecsSinceEpoch();
                page.request_key = cache_key;

                // Cache the (already-sorted) vessels + envelope.
                QJsonArray cached_arr;
                for (const auto& v : page.vessels)
                    cached_arr.append(maritime_svc_vessel_to_json(v));
                QJsonObject cached_root;
                cached_root["vessels"] = cached_arr;
                cached_root["total_count"] = page.total_count;
                cached_root["credits_used"] = page.credits_used;
                cached_root["fetched_at_ms"] = static_cast<double>(page.fetched_at_ms);
                QString cached_json = QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact));

                // Back on the GUI thread: cache write, signal, hub.
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, cache_key, page = std::move(page), cached_json = std::move(cached_json)]() {
                        if (!self)
                            return;
                        fincept::CacheManager::instance().put(cache_key, QVariant(cached_json), kAreaKeepSec, "maritime");
                        // ...and its fetch time on the side (area_cached_at reads only this).
                        QString meta_key = cache_key;
                        meta_key.replace(0, int(qstrlen("maritime:area:")), QStringLiteral("maritime:area_meta:"));
                        fincept::CacheManager::instance().put(meta_key, QVariant(QString::number(page.fetched_at_ms)),
                                                              kAreaKeepSec, "maritime");
                        LOG_DEBUG("Maritime", QString("Area search: %1 vessels (total %2, %3 credits left)")
                                                 .arg(page.vessels.size())
                                                 .arg(page.total_count)
                                                 .arg(page.remaining_credits, 0, 'f', 2));
                        emit self->vessels_loaded(page);
                        if (self->hub_registered_)
                            publish_to_hub(QStringLiteral("maritime:vessels:area"), QVariant::fromValue(page));
                    },
                    Qt::QueuedConnection);
            });
        },
        this);
}

// ── Single vessel position ───────────────────────────────────────────────────
void MaritimeService::get_vessel_position(const QString& imo, bool force) {
    if (imo.trimmed().isEmpty())
        return;

    const QString cache_key = "maritime:vessel:" + imo.trimmed();
    const QVariant cached = force ? QVariant() : fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto vessel = parse_vessel(QJsonDocument::fromJson(cached.toString().toUtf8()).object());
        emit vessel_found(vessel);
        // A hub-driven refresh (dashboard widget) can land on a cache hit; without this
        // the topic stayed empty until the cache entry expired.
        if (hub_registered_)
            publish_to_hub(QStringLiteral("maritime:vessel:") + imo.trimmed(), QVariant::fromValue(vessel));
        return;
    }

    QJsonObject body;
    body["imo"] = imo.trimmed();

    QPointer<MaritimeService> self = this;
    const QString imo_trimmed = imo.trimmed();
    HttpClient::instance().post(
        marine_base() + "/vessel/position", body, [self, cache_key, imo_trimmed](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                maritime_svc_log_failure("Vessel position", result.error());
                emit self->error_occurred("vessel_position", QString::fromStdString(result.error()));
                if (self->hub_registered_) {
                    const QString topic = QStringLiteral("maritime:vessel:") + imo_trimmed;
                    self->note_hub_refusal(topic,
                                           maritime_svc_publish_error(topic, QString::fromStdString(result.error())));
                }
                return;
            }
            auto data = unwrap(result.value().object());
            // A 2xx that carries no vessel object is a failure, not an empty
            // result. Writing the empty object into CacheManager would pin a
            // blank "vessel" for kVesselTtlSec, so every retry inside the TTL
            // is served the failure from cache without touching the network.
            if (!data.value(QStringLiteral("vessel")).isObject()) {
                LOG_ERROR("Maritime", "Vessel position: no vessel in response for IMO " + imo_trimmed);
                emit self->error_occurred("vessel_position", QStringLiteral("No vessel data returned"));
                if (self->hub_registered_)
                    fincept::datahub::DataHub::instance().publish_error(QStringLiteral("maritime:vessel:") + imo_trimmed,
                                                                        QStringLiteral("No vessel data returned"));
                return;
            }
            auto vessel_obj = data["vessel"].toObject();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(vessel_obj).toJson(QJsonDocument::Compact))),
                kVesselTtlSec, "maritime");
            auto vessel = self->parse_vessel(vessel_obj);
            LOG_DEBUG("Maritime", QString("Found vessel: %1 [%2]").arg(vessel.name, vessel.imo));
            emit self->vessel_found(vessel);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("maritime:vessel:") + imo_trimmed, QVariant::fromValue(vessel));
        });
}

// ── Multi vessel positions ───────────────────────────────────────────────────
void MaritimeService::get_multi_vessel_positions(const QStringList& imos) {
    QStringList sorted = imos;
    std::sort(sorted.begin(), sorted.end());
    const QString cache_key = "maritime:multi:" + sorted.join(",");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
        VesselsPage page;
        const QJsonArray arr = root["vessels"].toArray();
        page.vessels.reserve(arr.size());
        for (const auto& v : arr)
            page.vessels.append(parse_vessel(v.toObject()));
        page.total_count = root["found_count"].toInt(page.vessels.size());
        page.found_count = page.total_count;
        for (const auto& nf : root["not_found"].toArray())
            page.not_found.append(nf.toString());
        page.credits_used = root["credits_used"].toDouble(0.0);
        page.remaining_credits = -1; // cache hit: free, and the stored balance is stale
        page.fetched_at_ms = static_cast<qint64>(root["fetched_at_ms"].toDouble(0));
        emit vessels_loaded(page);
        return;
    }

    QJsonObject body;
    QJsonArray arr;
    for (const auto& imo : imos)
        arr.append(imo.trimmed());
    body["imos"] = arr;

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(marine_base() + "/vessel/multi", body, [self, cache_key](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            maritime_svc_log_failure("Multi-vessel lookup", result.error());
            emit self->error_occurred("multi_vessel", QString::fromStdString(result.error()));
            return;
        }
        const auto data = unwrap(result.value().object());
        const auto vessels_arr = data["vessels"].toArray();

        VesselsPage page;
        page.vessels.reserve(vessels_arr.size());
        for (const auto& v : vessels_arr)
            page.vessels.append(self->parse_vessel(v.toObject()));

        std::sort(page.vessels.begin(), page.vessels.end(),
                  [](const VesselData& a, const VesselData& b) { return a.last_updated > b.last_updated; });

        page.found_count = data["found_count"].toInt(page.vessels.size());
        page.total_count = page.found_count;
        for (const auto& nf : data["not_found"].toArray())
            page.not_found.append(nf.toString());
        page.credits_used = data["credits_used"].toDouble(0.0);
        page.remaining_credits = maritime_svc_credits(data["remaining_credits"]);
        page.fetched_at_ms = QDateTime::currentMSecsSinceEpoch();

        // Cache the full envelope so cached replays carry not_found etc.
        QJsonArray cached_arr;
        for (const auto& v : page.vessels)
            cached_arr.append(maritime_svc_vessel_to_json(v));
        QJsonObject cached_root;
        cached_root["vessels"] = cached_arr;
        cached_root["found_count"] = page.found_count;
        cached_root["not_found"] = QJsonArray::fromStringList(page.not_found);
        cached_root["credits_used"] = page.credits_used;
        cached_root["fetched_at_ms"] = static_cast<double>(page.fetched_at_ms);
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact))),
            kVesselTtlSec, "maritime");

        LOG_DEBUG("Maritime", QString("Multi vessel: %1 found, %2 missing (%3 credits left)")
                                 .arg(page.found_count)
                                 .arg(page.not_found.size())
                                 .arg(page.remaining_credits, 0, 'f', 2));

        emit self->vessels_loaded(page);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("maritime:vessels:multi"), QVariant::fromValue(page));
    });
}

// ── Vessel history ───────────────────────────────────────────────────────────
void MaritimeService::get_vessel_history(const QString& imo, bool force) {
    const QString imo_trimmed = imo.trimmed();
    const QString cache_key = "maritime:history:" + imo_trimmed;
    const QVariant cached = force ? QVariant() : fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        // The kept copy holds every fix of the voyage (no row limit): the GUI thread
        // only reads the cache row; the JSON and the vessels are parsed on a worker,
        // as for the area search.
        QPointer<MaritimeService> self = this;
        MaritimeService* svc = this; // app-lifetime singleton
        const quint64 shown_at_read = history_fetch_shown_.value(imo_trimmed);
        QThreadPool::globalInstance()->start([self, svc, imo_trimmed, shown_at_read, text = cached.toString()]() {
            const QJsonObject root = QJsonDocument::fromJson(text.toUtf8()).object();
            VesselHistoryPage page;
            page.imo = root["imo"].toString(imo_trimmed);
            const QJsonArray arr = root["history"].toArray();
            page.history.reserve(arr.size());
            for (const auto& v : arr)
                page.history.append(svc->parse_vessel(v.toObject())); // reads only its argument
            page.total_records = root["total_records"].toInt(page.history.size());
            page.credits_used = root["credits_used"].toDouble(0.0);
            page.remaining_credits = -1; // cache hit: free, and the stored balance is stale

            // Back on the GUI thread: signal, hub.
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, imo_trimmed, shown_at_read, page = std::move(page)]() {
                    if (!self)
                        return;
                    // A freshly fetched history of this vessel was emitted while this copy
                    // was being parsed: the copy is older — never show it over the new one.
                    if (self->history_fetch_shown_.value(imo_trimmed) != shown_at_read)
                        return;
                    emit self->vessel_history_loaded(page);
                    if (self->hub_registered_)
                        publish_to_hub(QStringLiteral("maritime:history:") + imo_trimmed, QVariant::fromValue(page));
                },
                Qt::QueuedConnection);
        });
        return;
    }

    QJsonObject body;
    body["imo"] = imo_trimmed;

    // A voyage history has no row limit (every fix the server keeps for the
    // vessel), so the body is parsed off the GUI thread (post_large, the
    // client-wide timeout) and the parse / sort / cache serialisation below run
    // on a worker as well — as for the area search. The GUI thread only stores
    // the copy and emits.
    QPointer<MaritimeService> self = this;
    const quint64 fetch_seq = ++history_fetch_seq_;
    HttpClient::instance().post_large(
        marine_base() + "/vessel/history", body, /*timeout_ms=*/0,
        [self, cache_key, imo_trimmed, fetch_seq](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                emit self->error_occurred("vessel_history", QString::fromStdString(result.error()));
                if (self->hub_registered_) {
                    const QString topic = QStringLiteral("maritime:history:") + imo_trimmed;
                    self->note_hub_refusal(topic,
                                           maritime_svc_publish_error(topic, QString::fromStdString(result.error())));
                }
                return;
            }
            MaritimeService* svc = self.data(); // app-lifetime singleton
            QThreadPool::globalInstance()->start([self, svc, cache_key, imo_trimmed, fetch_seq,
                                                  doc = result.value()]() {
                const auto data = unwrap(doc.object());
                // API returns either "history" (current) or "positions" (legacy).
                QJsonArray arr = data["history"].toArray();
                if (arr.isEmpty())
                    arr = data["positions"].toArray();

                VesselHistoryPage page;
                page.imo = data["imo"].toString(imo_trimmed);
                page.history.reserve(arr.size());
                for (const auto& v : arr)
                    page.history.append(svc->parse_vessel(v.toObject())); // reads only its argument

                std::sort(page.history.begin(), page.history.end(),
                          [](const VesselData& a, const VesselData& b) { return a.last_updated > b.last_updated; });

                page.total_records = data["total_records"].toInt(page.history.size());
                page.credits_used = data["credits_used"].toDouble(0.0);
                page.remaining_credits = maritime_svc_credits(data["remaining_credits"]);

                QJsonArray cached_arr;
                for (const auto& v : page.history)
                    cached_arr.append(maritime_svc_vessel_to_json(v));
                QJsonObject cached_root;
                cached_root["imo"] = page.imo;
                cached_root["history"] = cached_arr;
                cached_root["total_records"] = page.total_records;
                cached_root["credits_used"] = page.credits_used;
                QString cached_json = QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact));

                // Back on the GUI thread: cache write, signal, hub.
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, cache_key, imo_trimmed, fetch_seq, page = std::move(page),
                     cached_json = std::move(cached_json)]() {
                        if (!self)
                            return;
                        // A newer fetch of this vessel's history answered first (and was
                        // cached): this older answer must not replace it.
                        quint64& shown = self->history_fetch_shown_[imo_trimmed];
                        if (fetch_seq < shown)
                            return;
                        shown = fetch_seq;
                        fincept::CacheManager::instance().put(cache_key, QVariant(cached_json), kHistoryTtlSec,
                                                              "maritime");

                        LOG_DEBUG("Maritime", QString("History [%1]: %2 records (total %3, %4 credits left)")
                                                 .arg(page.imo)
                                                 .arg(page.history.size())
                                                 .arg(page.total_records)
                                                 .arg(page.remaining_credits, 0, 'f', 2));

                        emit self->vessel_history_loaded(page);
                        if (self->hub_registered_)
                            publish_to_hub(QStringLiteral("maritime:history:") + imo_trimmed, QVariant::fromValue(page));
                    },
                    Qt::QueuedConnection);
            });
        },
        this);
}

// ── Health check ─────────────────────────────────────────────────────────────
void MaritimeService::check_health() {
    QPointer<MaritimeService> self = this;
    HttpClient::instance().get(marine_base() + "/health", [self](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("health", QString::fromStdString(result.error()));
            if (self->hub_registered_)
                maritime_svc_publish_error(QStringLiteral("maritime:health"), QString::fromStdString(result.error()));
            return;
        }
        const auto obj = result.value().object();
        emit self->health_loaded(obj);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("maritime:health"), QVariant(obj));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — maritime:*
// ═══════════════════════════════════════════════════════════════════════════════

QStringList MaritimeService::topic_patterns() const {
    return {QStringLiteral("maritime:*")};
}

void MaritimeService::refresh(const QStringList& topics) {
    // Only refresh topics whose key uniquely determines the request. Area /
    // multi-vessel topics depend on user-supplied bbox / IMO list, so the
    // hub doesn't drive their cadence — those calls are user-invoked.
    auto& hub = fincept::datahub::DataHub::instance();
    const bool signed_out = auth::AuthManager::instance().session().api_key.isEmpty();
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        const bool billed = parts.size() >= 3 && (parts[1] == QStringLiteral("vessel") || parts[1] == QStringLiteral("history"));
        if (billed && signed_out) {
            // Account-only, billed data: never ask while signed out (a sign-in lifts this).
            hub.publish_error(topic, QStringLiteral("Sign in to Fincept to load vessel data."), kMaritimeSvcRefusedRetry);
            note_hub_refusal(topic, auth::RefusalLatch::Kind::Access);
            continue;
        }
        // The hub asks again only when its own copy has expired or the user
        // pressed ⟳ — either way the CacheManager copy (same age or older) is
        // not an answer. Without a hub copy (cold start) it is.
        const bool hub_has_copy = hub.peek_raw(topic).isValid();
        if (parts.size() >= 3 && parts[1] == QStringLiteral("vessel")) {
            get_vessel_position(parts[2], hub_has_copy);
        } else if (parts.size() >= 3 && parts[1] == QStringLiteral("history")) {
            get_vessel_history(parts[2], hub_has_copy);
        } else if (topic == QStringLiteral("maritime:health")) {
            check_health();
        }
    }
}

int MaritimeService::max_requests_per_sec() const {
    return 2; // Fincept marine API — conservative
}

void MaritimeService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // Position data: TTL matches kVesselTtlSec (10 min, 1 credit per vessel per
    // refresh); min 60 s between refreshes.
    fincept::datahub::TopicPolicy vessel_policy;
    vessel_policy.ttl_ms = kVesselTtlSec * 1000;
    vessel_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:vessel:*"), vessel_policy);
    hub.set_policy_pattern(QStringLiteral("maritime:vessels:*"), vessel_policy);

    // History: TTL matches kHistoryTtlSec (30 min, 5 credits per refresh).
    fincept::datahub::TopicPolicy history_policy;
    history_policy.ttl_ms = kHistoryTtlSec * 1000;
    history_policy.min_interval_ms = 5 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:history:*"), history_policy);

    // Health: 5 min TTL, non-urgent.
    fincept::datahub::TopicPolicy health_policy;
    health_policy.ttl_ms = 5 * 60 * 1000;
    health_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:health"), health_policy);

    // What lifts a refusal backoff: a new sign-in / key, or credits again.
    auto& am = auth::AuthManager::instance();
    connect(&am, &auth::AuthManager::auth_state_changed, this, [this]() {
        const auto& s = auth::AuthManager::instance().session();
        if (refusal_.release_on_auth(s.authenticated, s.api_key))
            release_hub_refusal("signed in");
    });
    connect(&am, &auth::AuthManager::credit_balance_changed, this, [this](double balance) {
        if (refusal_.release_on_balance(balance, kMaritimeSvcCreditCost))
            release_hub_refusal("credits topped up");
    });

    hub_registered_ = true;
    LOG_DEBUG("MaritimeService", "Registered with DataHub (maritime:*)");
}

void MaritimeService::note_hub_refusal(const QString& topic, auth::RefusalLatch::Kind kind) {
    if (kind == auth::RefusalLatch::Kind::None)
        return;
    auto& am = auth::AuthManager::instance();
    refusal_.refuse(kind, am.session().api_key, am.session().user_info.credit_balance);
    refused_topics_.insert(topic);
    // The header chip may still show credits the server says are gone: re-read
    // the account (see RefusalLatch). Once per refusal, not per retry.
    if (kind == auth::RefusalLatch::Kind::Credits)
        am.refresh_user_data(auth::AuthManager::Refresh::Now);
}

void MaritimeService::release_hub_refusal(const char* why) {
    if (!refusal_.active())
        return;
    refusal_.clear();
    LOG_INFO("MaritimeService", QString("Refused topics released (%1)").arg(QLatin1String(why)));
    auto& hub = fincept::datahub::DataHub::instance();
    hub.clear_backoff(QStringLiteral("maritime:*"));
    // Topics on screen fetch now (an event, not a timer — the min-interval would
    // otherwise hold them back); the rest wait for their next subscriber.
    const QSet<QString> topics = std::exchange(refused_topics_, {});
    for (const QString& t : topics)
        if (!hub.subscribers(t).isEmpty())
            hub.request(t, /*force=*/true);
}

} // namespace fincept::services::maritime
