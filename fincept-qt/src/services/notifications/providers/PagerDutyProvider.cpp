#include "services/notifications/providers/PagerDutyProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>
#include <QUuid>

namespace fincept::notifications {

void PagerDutyProvider::load_fields(SettingsRepository& r, const QString& cat) {
    routing_key_ = get_str(r, cat + ".routing_key");
}

void PagerDutyProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".routing_key", routing_key_, cat);
}

void PagerDutyProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    const QString severity = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:
                return "warning";
            case NotifLevel::Alert:
                return "error";
            case NotifLevel::Critical:
                return "critical";
            default:
                return "info";
        }
    }();

    QJsonObject custom_details;
    custom_details["message"] = req.message;

    QJsonObject payload;
    payload["summary"] = req.title + ": " + req.message;
    payload["severity"] = severity;
    payload["source"] = "Fincept Terminal";
    payload["timestamp"] = req.timestamp.toUTC().toString(Qt::ISODate);
    payload["custom_details"] = custom_details;

    QJsonObject body;
    body["routing_key"] = routing_key_;
    body["event_action"] = "trigger";
    body["dedup_key"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    body["payload"] = payload;

    HttpClient::instance().post("https://events.pagerduty.com/v2/enqueue", body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) {
            cb(false, QString::fromStdString(res.error()));
            return;
        }
        const auto obj = res.value().object();
        const bool ok = obj.value("status").toString() == "success";
        cb(ok, ok ? QString{} : obj.value("message").toString());
    });
}

} // namespace fincept::notifications
