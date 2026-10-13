#include "services/economics/FinceptMacroService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace fincept::services {

namespace fincept_macro_svc_detail {

constexpr const char* kFinceptMacroSvcTag = "FinceptMacro";
constexpr const char* kCachePrefix = "fincept_macro:v1:";
constexpr const char* kCacheCategory = "fincept_macro";
// Last-known-good horizon: a copy older than this is never shown, even offline.
constexpr int kStaleKeepSec = 7 * 24 * 60 * 60;

bool is_metadata_endpoint(const QString& endpoint) {
    return endpoint == QLatin1String("ceic/countries") || endpoint == QLatin1String("ceic/categories") ||
           endpoint == QLatin1String("ceic/series/countries") || endpoint == QLatin1String("ceic/series/indicators");
}

} // namespace fincept_macro_svc_detail



FinceptMacroService& FinceptMacroService::instance() {
    static FinceptMacroService s;
    return s;
}

FinceptMacroService::FinceptMacroService(QObject* parent) : QObject(parent) {}

double FinceptMacroService::credit_cost(const QString& endpoint) {
    // Mirrors the backend defaults (macro.py: METADATA 0.5, everything else 1). Server-side cache hits
    // on some endpoints are free; this is the worst case for one uncached call.
    return fincept_macro_svc_detail::is_metadata_endpoint(endpoint) ? 0.5 : 1.0;
}

int FinceptMacroService::fresh_seconds(const QString& endpoint) {
    if (endpoint == QLatin1String("upcoming-events"))
        return 5 * 60; // the event feed (actuals) moves during the day
    if (endpoint.startsWith(QLatin1String("wgb/")) || endpoint == QLatin1String("economic-calendar"))
        return 30 * 60; // scraped snapshots, refreshed a few times a day upstream
    if (endpoint == QLatin1String("ceic/series"))
        return 6 * 60 * 60; // monthly / quarterly / annual series
    return 24 * 60 * 60; // catalogue / metadata
}

QString FinceptMacroService::cache_key(const QString& endpoint, const Params& params) {
    Params sorted;
    for (const auto& p : params)
        if (!p.second.isEmpty())
            sorted.append(p);
    std::sort(sorted.begin(), sorted.end(),
              [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) { return a.first < b.first; });
    QStringList parts;
    for (const auto& p : std::as_const(sorted))
        parts << p.first + QLatin1Char('=') + p.second.toLower();
    return QString::fromLatin1(fincept_macro_svc_detail::kCachePrefix) + endpoint + QLatin1Char('?') + parts.join(QLatin1Char('&'));
}

std::optional<FinceptMacroService::CachedEntry> FinceptMacroService::read_cache(const QString& key) {
    const auto raw = fincept::CacheManager::instance().try_get(key);
    if (!raw)
        return std::nullopt;
    const QJsonObject wrapper = QJsonDocument::fromJson(raw->toUtf8()).object();
    CachedEntry e;
    e.data = wrapper.value(QStringLiteral("data")).toObject();
    e.fetched_at = QDateTime::fromString(wrapper.value(QStringLiteral("fetched_at")).toString(), Qt::ISODate);
    if (!e.fetched_at.isValid())
        return std::nullopt;
    return e;
}

std::optional<QJsonObject> FinceptMacroService::cached_payload(const QString& endpoint, const Params& params) const {
    const auto e = read_cache(cache_key(endpoint, params));
    if (!e)
        return std::nullopt;
    return e->data;
}

QJsonArray FinceptMacroService::extract_rows(const QJsonObject& data) {
    // Every /macro payload carries exactly one list (central_bank_rates, series, events, indicators, …)
    // next to scalar counts and a filters_applied object.
    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
        if (it.value().isArray())
            return it.value().toArray();
    return {};
}

void FinceptMacroService::fetch(const QString& request_id, const QString& endpoint, const Params& params, bool force) {
    const QString key = cache_key(endpoint, params);

    if (!force) {
        if (const auto cached = read_cache(key)) {
            const qint64 age = cached->fetched_at.secsTo(QDateTime::currentDateTimeUtc());
            if (age >= 0 && age < fresh_seconds(endpoint)) {
                FinceptMacroResult res;
                res.status = FinceptMacroResult::Status::Ok;
                res.request_id = request_id;
                res.endpoint = endpoint;
                res.payload = cached->data;
                res.rows = extract_rows(cached->data);
                res.fetched_at = cached->fetched_at;
                res.from_cache = true;
                LOG_DEBUG(fincept_macro_svc_detail::kFinceptMacroSvcTag, QString("Cache hit (%1 s old): %2").arg(age).arg(endpoint));
                emit result_ready(res);
                return;
            }
        }
    }

    QUrl url(AppConfig::instance().api_base_url() + QStringLiteral("/macro/") + endpoint);
    QUrlQuery query;
    for (const auto& p : params)
        if (!p.second.isEmpty())
            query.addQueryItem(p.first, p.second);
    url.setQuery(query);

    LOG_INFO(fincept_macro_svc_detail::kFinceptMacroSvcTag, QString("GET /macro/%1 (%2 param(s))").arg(endpoint).arg(params.size()));

    QPointer<FinceptMacroService> self = this;
    HttpClient::instance().get(
        url.toString(QUrl::FullyEncoded),
        [self, request_id, endpoint, key](Result<QJsonDocument> result) {
            if (!self)
                return;
            FinceptMacroResult res;
            res.request_id = request_id;
            res.endpoint = endpoint;

            // A failure that has a recent last-good copy serves that copy, flagged stale.
            auto serve_stale_or = [&self, &res, &key]() {
                if (res.status == FinceptMacroResult::Status::Offline || res.status == FinceptMacroResult::Status::Error) {
                    if (const auto cached = read_cache(key)) {
                        if (cached->fetched_at.secsTo(QDateTime::currentDateTimeUtc()) < fincept_macro_svc_detail::kStaleKeepSec) {
                            res.payload = cached->data;
                            res.rows = extract_rows(cached->data);
                            res.fetched_at = cached->fetched_at;
                            res.from_cache = true;
                            res.stale = true;
                        }
                    }
                }
                emit self->result_ready(res);
            };

            if (result.is_err()) {
                const std::string err = result.error();
                res.http_status = HttpClient::status_from_error(err);
                res.error = HttpClient::message_from_error(err);
                if (res.http_status == 401 || res.http_status == 403)
                    res.status = FinceptMacroResult::Status::Unauthorized;
                else if (res.http_status == 402)
                    res.status = FinceptMacroResult::Status::NoCredits;
                else if (res.http_status == 0)
                    res.status = FinceptMacroResult::Status::Offline;
                else
                    res.status = FinceptMacroResult::Status::Error;
                if (res.error.isEmpty() && res.http_status > 0)
                    res.error = QStringLiteral("HTTP %1").arg(res.http_status);
                LOG_WARN(fincept_macro_svc_detail::kFinceptMacroSvcTag, QString("/macro/%1 failed: %2").arg(endpoint, res.error));
                serve_stale_or();
                return;
            }

            const QJsonObject root = result.value().object();
            // APIResponse envelope: {success, message, data}. A 200 with success:false is a failure,
            // not an empty result.
            if (root.contains(QStringLiteral("success")) && !root.value(QStringLiteral("success")).toBool()) {
                res.status = FinceptMacroResult::Status::Error;
                const QJsonValue msg = root.value(QStringLiteral("message"));
                res.error = msg.isString() ? msg.toString() : QStringLiteral("The server reported a failure");
                LOG_WARN(fincept_macro_svc_detail::kFinceptMacroSvcTag, QString("/macro/%1 returned success=false").arg(endpoint));
                serve_stale_or();
                return;
            }

            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            res.status = FinceptMacroResult::Status::Ok;
            res.payload = data;
            res.rows = extract_rows(data);
            res.fetched_at = QDateTime::currentDateTimeUtc();

            const QJsonObject wrapper{{QStringLiteral("fetched_at"), res.fetched_at.toString(Qt::ISODate)},
                                      {QStringLiteral("data"), data}};
            fincept::CacheManager::instance().put(
                key, QVariant(QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact))),
                fincept_macro_svc_detail::kStaleKeepSec, QString::fromLatin1(fincept_macro_svc_detail::kCacheCategory));
            LOG_INFO(fincept_macro_svc_detail::kFinceptMacroSvcTag, QString("/macro/%1: %2 row(s)").arg(endpoint).arg(res.rows.size()));
            emit self->result_ready(res);
        },
        this);
}

} // namespace fincept::services
