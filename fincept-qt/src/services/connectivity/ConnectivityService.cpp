#include "services/connectivity/ConnectivityService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "network/http/HttpClient.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>
#include <chrono>

namespace fincept::services {

namespace connectivity_service_detail {

constexpr const char* kConnectivityServiceTag = "ConnectivityService";
constexpr const char* kConnectivityHealthTopic = "system:health";
// Whole-request budget. A probe slower than this is reported as a failure —
// the status bar would show it red anyway.
constexpr int kConnectivityProbeTimeoutMs = 5000;
// GET /health runs a database round trip on the server. Every visible dashboard
// status bar probed every 30 s (120 an hour per terminal, idle or not); a latency
// lamp does not need that. The connection tile's ⟳ still probes on demand.
constexpr int kConnectivityHealthyTtlMs = 2 * 60 * 1000;
// After a failed probe the reading ages out sooner, so a recovered backend is
// picked up within ~30 s — not every 15 s from every terminal while it is down.
constexpr int kConnectivityFailedTtlMs = 30 * 1000;

} // namespace connectivity_service_detail

namespace csd = connectivity_service_detail;

ConnectivityService& ConnectivityService::instance() {
    static ConnectivityService s;
    return s;
}

ConnectivityService::ConnectivityService(QObject* parent) : QObject(parent) {}

QString ConnectivityService::health_topic() {
    return QString::fromLatin1(csd::kConnectivityHealthTopic);
}

void ConnectivityService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = csd::kConnectivityHealthyTtlMs;
    policy.min_interval_ms = 20 * 1000;
    // Slightly above the request's own timeout so the hub never declares the
    // probe stuck while the reply is still allowed to arrive.
    policy.refresh_timeout_ms = csd::kConnectivityProbeTimeoutMs + 5000;
    hub.set_policy(health_topic(), policy);

    hub_registered_ = true;
    LOG_DEBUG(csd::kConnectivityServiceTag, "Registered with DataHub (system:health)");
}

QStringList ConnectivityService::topic_patterns() const {
    return {health_topic()};
}

void ConnectivityService::refresh(const QStringList& topics) {
    if (!topics.contains(health_topic()))
        return;
    probe();
}

void ConnectivityService::probe() {
    if (probe_in_flight_)
        return; // a manual request() raced the scheduler — one probe is enough
    if (!nam_)
        nam_ = new QNetworkAccessManager(this);

    const QUrl base(AppConfig::instance().api_base_url());
    QUrl url = base;
    url.setPath(base.path().endsWith(QLatin1Char('/')) ? base.path() + QStringLiteral("health")
                                                       : base.path() + QStringLiteral("/health"));
    const QString endpoint = base.host();

    QNetworkRequest req(url);
    req.setTransferTimeout(csd::kConnectivityProbeTimeoutMs);
    // A cached 200 would report a dead backend as healthy.
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QElapsedTimer started;
    started.start();
    probe_in_flight_ = true;
    QNetworkReply* reply = nam_->get(req);

    // `this` is the connection context, so the slot is dropped with the service.
    connect(reply, &QNetworkReply::finished, this, [this, reply, started, endpoint]() {
        reply->deleteLater();
        probe_in_flight_ = false;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transport_ok = reply->error() == QNetworkReply::NoError;
        const bool ok = transport_ok && status >= 200 && status < 300;
        const qint64 elapsed = started.elapsed();
        // The server's "do not probe /health faster than this" (X-Poll-Interval,
        // capped at an hour; 0 when absent — older server, or no response at all).
        // The reading's TTL — which is what schedules the next automatic probe —
        // is max(own TTL, hint). A manual ⟳ still probes on demand.
        const qint64 hint_ms = fincept::HttpClient::poll_interval_ms(reply->rawHeader("X-Poll-Interval"));

        QString error;
        if (!ok) {
            if (reply->error() == QNetworkReply::OperationCanceledError ||
                reply->error() == QNetworkReply::TimeoutError)
                error = QStringLiteral("timeout after %1 ms").arg(csd::kConnectivityProbeTimeoutMs);
            else if (!transport_ok)
                error = reply->errorString();
            else
                error = QStringLiteral("HTTP %1").arg(status);
        }

        QJsonObject payload{
            {"ok", ok},
            {"latency_ms", ok ? static_cast<int>(elapsed) : -1},
            {"http_status", status},
            {"error", error},
            {"checked_at_ms", QDateTime::currentMSecsSinceEpoch()},
            {"endpoint", endpoint},
        };

        auto& hub = fincept::datahub::DataHub::instance();
        // Logged on change only: reachable -> unreachable (WARN, with the reason),
        // later failed probes at DEBUG, unreachable -> reachable (INFO, outage length).
        auto& logger = fincept::Logger::instance();
        const QString state_key = QStringLiteral("backend|") + endpoint;
        if (ok) {
            fincept::LogTransition was;
            if (logger.transition_possible(0) && logger.transition(csd::kConnectivityServiceTag, state_key, 0, &was))
                LOG_INFO(csd::kConnectivityServiceTag,
                         logf("Backend reachable again", {{"endpoint", endpoint},
                                                          {"outage_s", was.ms_in_previous / 1000},
                                                          {"failed_probes", was.observations_in_previous}}));
            if (hint_ms > csd::kConnectivityHealthyTtlMs)
                hub.publish(health_topic(), QVariant::fromValue(payload), std::chrono::milliseconds(hint_ms));
            else
                hub.publish(health_topic(), QVariant::fromValue(payload)); // the policy's TTL, as before
        } else {
            if (logger.transition(csd::kConnectivityServiceTag, state_key, 1))
                LOG_WARN(csd::kConnectivityServiceTag,
                         logf("Backend unreachable", {{"endpoint", endpoint}, {"status", status}, {"error", error}}));
            else
                LOG_DEBUG(csd::kConnectivityServiceTag,
                          logf("Health probe failed", {{"endpoint", endpoint}, {"status", status}, {"error", error}}));
            // A degraded backend (503) still says how often to ask; a dead one
            // sends nothing and keeps the short retry.
            hub.publish(health_topic(), QVariant::fromValue(payload),
                        std::chrono::milliseconds(std::max<qint64>(csd::kConnectivityFailedTtlMs, hint_ms)));
        }
    });
}

} // namespace fincept::services
