#include "services/notifications/providers/SlackProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void SlackProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_secret(r, cat + ".webhook_url");
    channel_ = get_str(r, cat + ".channel");
}

void SlackProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".webhook_url", webhook_url_, cat);
    r.set(cat + ".channel", channel_, cat);
}

void SlackProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    const QString emoji = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:
                return ":warning:";
            case NotifLevel::Alert:
                return ":bell:";
            case NotifLevel::Critical:
                return ":rotating_light:";
            default:
                return ":information_source:";
        }
    }();

    QJsonObject body;
    body["text"] = QString("%1 *%2*\n%3").arg(emoji, req.title, req.message);
    if (!channel_.isEmpty())
        body["channel"] = channel_;

    // An incoming webhook answers the text "ok" (a Workflow / Web API URL answers
    // JSON {"ok":…}): send_delivery() decides on the HTTP status and that flag.
    DeliveryRequest request;
    request.service = DeliveryService::Slack;
    request.url = webhook_url_;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
