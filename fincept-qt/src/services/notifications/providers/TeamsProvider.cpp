#include "services/notifications/providers/TeamsProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonArray>
#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void TeamsProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_secret(r, cat + ".webhook_url");
}

void TeamsProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".webhook_url", webhook_url_, cat);
}

void TeamsProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // Card accent shown inside Microsoft Teams, not by the terminal: fixed
    // colours, independent of the terminal theme.
    const QString color = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:
                return "F59E0B";
            case NotifLevel::Alert:
                return "F97316";
            case NotifLevel::Critical:
                return "EF4444";
            default:
                return "06B6D4";
        }
    }();

    // MS Teams Adaptive Card payload
    QJsonObject fact_title;
    fact_title["title"] = "Alert";
    fact_title["value"] = req.title;

    QJsonObject fact_time;
    fact_time["title"] = "Time";
    fact_time["value"] = req.timestamp.toString("yyyy-MM-dd hh:mm:ss");

    QJsonObject section;
    section["activityTitle"] = req.title;
    section["activityText"] = req.message;
    section["activitySubtitle"] = req.timestamp.toString("yyyy-MM-dd hh:mm:ss");

    QJsonArray sections;
    sections.append(section);

    QJsonObject body;
    body["@type"] = "MessageCard";
    body["@context"] = "http://schema.org/extensions";
    body["themeColor"] = color;
    body["summary"] = req.title;
    body["title"] = req.title;
    body["text"] = req.message;
    body["sections"] = sections;

    // A connector answers the text "1", a Workflows webhook an empty 202: the
    // HTTP status decides, plus the connector's "Webhook message delivery failed …"
    // text inside a 200.
    DeliveryRequest request;
    request.service = DeliveryService::Teams;
    request.url = webhook_url_;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
