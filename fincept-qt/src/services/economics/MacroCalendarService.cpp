#include "services/economics/MacroCalendarService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>

namespace fincept::services {

namespace {
constexpr const char* kTopic = "econ:fincept:upcoming_events";

// Path appended to AppConfig::api_base_url(). Requests go through the shared
// HttpClient, which attaches the signed-in user's session key (X-API-Key /
// X-Session-Token) to same-host api.fincept.in requests — so no endpoint key
// is baked into the binary.
constexpr const char* kPath = "/macro/upcoming-events?limit=25";

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

MacroCalendarService::MacroCalendarService(QObject* parent) : QObject(parent) {}

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

    // Route through the shared HttpClient so the signed-in user's session key
    // is attached automatically (same-host auth), matching the other Fincept
    // data services (Geopolitics/Maritime/Forum). Host comes from AppConfig so
    // staging / self-host overrides apply uniformly.
    const QString url = AppConfig::instance().api_base_url() + QString::fromLatin1(kPath);

    QPointer<MacroCalendarService> self = this;
    HttpClient::instance().get(
        url,
        [self](Result<QJsonDocument> result) {
            if (!self)
                return;
            auto& hub = fincept::datahub::DataHub::instance();
            if (!result.is_ok()) {
                const QString msg = QString::fromStdString(result.error());
                LOG_WARN("MacroCalendarService", msg);
                hub.publish_error(QString::fromLatin1(kTopic), msg);
                return;
            }
            const QJsonArray events = parse_events(result.value());
            hub.publish(QString::fromLatin1(kTopic), QVariant::fromValue(events));
        },
        this);
}

} // namespace fincept::services
