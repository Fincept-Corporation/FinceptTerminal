#include "services/economics/MacroCalendarService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace fincept::services {

namespace {
constexpr const char* kTopic = "econ:fincept:upcoming_events";
constexpr const char* kUrl = "https://api.fincept.in/macro/upcoming-events?limit=25";

// Endpoint-specific API key. The shared HttpClient singleton attaches the
// signed-in user's session key to same-host requests; this endpoint requires
// an explicit user-issued key separate from session auth, so we hand-build
// the request via a private QNetworkAccessManager rather than polluting
// HttpClient's global api_key_ state.
constexpr const char* kApiKey = "fk_user_IYchlC3isi4NG5jtjkrNWX4nF3iPHgsFTT5rZ9TkpTQ";

QJsonArray parse_events(const QJsonDocument& doc) {
    if (doc.isArray())
        return doc.array();
    if (!doc.isObject())
        return {};

    const auto root = doc.object();
    // {"success":true,"data":{"events":[...]}}
    if (root.contains(QStringLiteral("data")) && root.value(QStringLiteral("data")).isObject()) {
        const auto data = root.value(QStringLiteral("data")).toObject();
        if (data.contains(QStringLiteral("events")) && data.value(QStringLiteral("events")).isArray())
            return data.value(QStringLiteral("events")).toArray();
    }
    // {"data":[...]}
    if (root.contains(QStringLiteral("data")) && root.value(QStringLiteral("data")).isArray())
        return root.value(QStringLiteral("data")).toArray();
    // {"events":[...]}
    if (root.contains(QStringLiteral("events")) && root.value(QStringLiteral("events")).isArray())
        return root.value(QStringLiteral("events")).toArray();
    return {};
}
} // namespace

MacroCalendarService& MacroCalendarService::instance() {
    static MacroCalendarService s;
    return s;
}

MacroCalendarService::MacroCalendarService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

void MacroCalendarService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 5 * 60 * 1000;   // 5 min — macro events refresh slowly
    policy.min_interval_ms = 60 * 1000; // 60 s
    policy.refresh_timeout_ms = 30 * 1000;
    hub.set_policy(QString::fromLatin1(kTopic), policy);

    hub_registered_ = true;
    LOG_INFO("MacroCalendarService", "Registered with DataHub (econ:fincept:upcoming_events)");
}

QStringList MacroCalendarService::topic_patterns() const {
    return {QString::fromLatin1(kTopic)};
}

void MacroCalendarService::refresh(const QStringList& topics) {
    // Single-topic producer — the hub may pass the topic list redundantly,
    // but there's only ever one fetch to do.
    if (!topics.contains(QString::fromLatin1(kTopic)))
        return;

    QNetworkRequest req{QUrl(QString::fromLatin1(kUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");
    req.setRawHeader("X-API-Key", QByteArray(kApiKey));

    QPointer<MacroCalendarService> self = this;
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply]() {
        reply->deleteLater();
        if (!self)
            return;
        auto& hub = fincept::datahub::DataHub::instance();

        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString msg = QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
            LOG_WARN("MacroCalendarService", msg);
            hub.publish_error(QString::fromLatin1(kTopic), msg);
            return;
        }

        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &perr);
        if (perr.error != QJsonParseError::NoError) {
            const QString msg = QStringLiteral("Parse error: %1").arg(perr.errorString());
            LOG_WARN("MacroCalendarService", msg);
            hub.publish_error(QString::fromLatin1(kTopic), msg);
            return;
        }
        const QJsonArray events = parse_events(doc);
        hub.publish(QString::fromLatin1(kTopic), QVariant::fromValue(events));
    });
}

} // namespace fincept::services
