#include "services/notifications/providers/PushoverProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void PushoverProvider::load_fields(SettingsRepository& r, const QString& cat) {
    api_token_ = get_secret(r, cat + ".api_token");
    user_key_ = get_secret(r, cat + ".user_key");
}

void PushoverProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".api_token", api_token_, cat);
    set_secret(r, cat + ".user_key", user_key_, cat);
}

void PushoverProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    const int priority = [&]() -> int {
        switch (req.level) {
            case NotifLevel::Warning:
                return 0;
            case NotifLevel::Alert:
                return 1;
            case NotifLevel::Critical:
                return 2;
            default:
                return -1;
        }
    }();

    QJsonObject body;
    body["token"] = api_token_;
    body["user"] = user_key_;
    body["title"] = req.title;
    body["message"] = req.message;
    body["priority"] = priority;
    if (priority == 2) {
        body["retry"] = 60;
        body["expire"] = 3600;
    }

    // {"status":1} on success; any other status is a failure, explained in "errors".
    DeliveryRequest request;
    request.service = DeliveryService::Pushover;
    request.url = QStringLiteral("https://api.pushover.net/1/messages.json");
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
