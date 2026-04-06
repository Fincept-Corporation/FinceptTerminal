#include "services/notifications/providers/GotifyProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void GotifyProvider::load_fields(SettingsRepository& r, const QString& cat) {
    server_url_ = get_str(r, cat + ".server_url");
    app_token_  = get_str(r, cat + ".app_token");
}

void GotifyProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".server_url", server_url_, cat);
    r.set(cat + ".app_token",  app_token_,  cat);
}

void GotifyProvider::send(const NotificationRequest& req,
                          std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const int priority = [&]() -> int {
        switch (req.level) {
            case NotifLevel::Warning:  return 5;
            case NotifLevel::Alert:    return 7;
            case NotifLevel::Critical: return 10;
            default:                   return 3;
        }
    }();

    const QString base = server_url_.endsWith('/') ? server_url_.chopped(1) : server_url_;
    const QString url  = QString("%1/message?token=%2").arg(base, app_token_);

    QJsonObject body;
    body["title"]    = req.title;
    body["message"]  = req.message;
    body["priority"] = priority;

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        cb(true, {});
    });
}

} // namespace fincept::notifications
