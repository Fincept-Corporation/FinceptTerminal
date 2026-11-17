#include "services/notifications/providers/MattermostProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void MattermostProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_secret(r, cat + ".webhook_url");
    channel_ = get_str(r, cat + ".channel");
    username_ = get_str(r, cat + ".username");
}

void MattermostProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".webhook_url", webhook_url_, cat);
    r.set(cat + ".channel", channel_, cat);
    r.set(cat + ".username", username_, cat);
}

void MattermostProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    QJsonObject body;
    body["text"] = QString("**%1**\n%2").arg(req.title, req.message);
    if (!channel_.isEmpty())
        body["channel"] = channel_;
    if (!username_.isEmpty())
        body["username"] = username_;

    // An incoming webhook answers the text "ok": the HTTP status decides.
    DeliveryRequest request;
    request.service = DeliveryService::Mattermost;
    request.url = webhook_url_;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
