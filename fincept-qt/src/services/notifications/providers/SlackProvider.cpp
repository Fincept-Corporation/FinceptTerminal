#include "services/notifications/providers/SlackProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void SlackProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_str(r, cat + ".webhook_url");
    channel_ = get_str(r, cat + ".channel");
}

void SlackProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".webhook_url", webhook_url_, cat);
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

    HttpClient::instance().post(webhook_url_, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) {
            cb(false, QString::fromStdString(res.error()));
            return;
        }
        // Slack returns plain text "ok" on success
        cb(true, {});
    });
}

} // namespace fincept::notifications
