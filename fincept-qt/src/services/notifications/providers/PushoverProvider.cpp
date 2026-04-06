#include "services/notifications/providers/PushoverProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void PushoverProvider::load_fields(SettingsRepository& r, const QString& cat) {
    api_token_ = get_str(r, cat + ".api_token");
    user_key_  = get_str(r, cat + ".user_key");
}

void PushoverProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".api_token", api_token_, cat);
    r.set(cat + ".user_key",  user_key_,  cat);
}

void PushoverProvider::send(const NotificationRequest& req,
                            std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const int priority = [&]() -> int {
        switch (req.level) {
            case NotifLevel::Warning:  return 0;
            case NotifLevel::Alert:    return 1;
            case NotifLevel::Critical: return 2;
            default:                   return -1;
        }
    }();

    QJsonObject body;
    body["token"]    = api_token_;
    body["user"]     = user_key_;
    body["title"]    = req.title;
    body["message"]  = req.message;
    body["priority"] = priority;
    if (priority == 2) {
        body["retry"]  = 60;
        body["expire"] = 3600;
    }

    HttpClient::instance().post("https://api.pushover.net/1/messages.json", body,
        [cb](Result<QJsonDocument> res) {
            if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
            const auto obj = res.value().object();
            const bool ok  = obj.value("status").toInt() == 1;
            cb(ok, ok ? QString{} : obj.value("errors").toArray().first().toString());
        });
}

} // namespace fincept::notifications
