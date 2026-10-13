#include "services/connectivity/ConnectivityService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <chrono>

namespace fincept::services {

namespace connectivity_service_detail {

constexpr const char* kConnectivityServiceTag = "ConnectivityService";
constexpr const char* kConnectivityHealthTopic = "system:health";
// Whole-request budget. A probe slower than this is reported as a failure —
// the status bar would show it red anyway.
constexpr int kConnectivityProbeTimeoutMs = 5000;
constexpr int kConnectivityHealthyTtlMs = 30 * 1000;
// After a failed probe the reading ages out sooner, so a recovered backend is
// picked up within ~15 s instead of a full healthy-TTL.
constexpr int kConnectivityFailedTtlMs = 15 * 1000;

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
    policy.min_interval_ms = 10 * 1000;
    // Slightly above the request's own timeout so the hub never declares the
    // probe stuck while the reply is still allowed to arrive.
    policy.refresh_timeout_ms = csd::kConnectivityProbeTimeoutMs + 5000;
    hub.set_policy(health_topic(), policy);

    hub_registered_ = true;
    LOG_INFO(csd::kConnectivityServiceTag, "Registered with DataHub (system:health)");
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
        if (ok) {
            hub.publish(health_topic(), QVariant::fromValue(payload));
        } else {
            LOG_WARN(csd::kConnectivityServiceTag, QString("Health probe to %1 failed: %2").arg(endpoint, error));
            hub.publish(health_topic(), QVariant::fromValue(payload),
                        std::chrono::milliseconds(csd::kConnectivityFailedTtlMs));
        }
    });
}

} // namespace fincept::services
