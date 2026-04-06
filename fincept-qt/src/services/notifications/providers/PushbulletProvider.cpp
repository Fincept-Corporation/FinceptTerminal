#include "services/notifications/providers/PushbulletProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void PushbulletProvider::load_fields(SettingsRepository& r, const QString& cat) {
    api_key_     = get_str(r, cat + ".api_key");
    channel_tag_ = get_str(r, cat + ".channel_tag");
}

void PushbulletProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".api_key",     api_key_,     cat);
    r.set(cat + ".channel_tag", channel_tag_, cat);
}

void PushbulletProvider::send(const NotificationRequest& req,
                              std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    // Pushbullet requires the API key as the username in basic auth.
    // We embed it in the URL since HttpClient doesn't have per-request auth.
    const QString url = QString("https://%1:@api.pushbullet.com/v2/pushes").arg(api_key_);

    QJsonObject body;
    body["type"]  = "note";
    body["title"] = req.title;
    body["body"]  = req.message;
    if (!channel_tag_.isEmpty())
        body["channel_tag"] = channel_tag_;

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        const auto obj = res.value().object();
        const bool ok  = !obj.contains("error");
        cb(ok, ok ? QString{} : obj.value("error").toObject().value("message").toString());
    });
}

} // namespace fincept::notifications
