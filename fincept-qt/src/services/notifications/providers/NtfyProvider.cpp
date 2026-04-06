#include "services/notifications/providers/NtfyProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void NtfyProvider::load_fields(SettingsRepository& r, const QString& cat) {
    server_url_ = get_str(r, cat + ".server_url");
    topic_      = get_str(r, cat + ".topic");
    token_      = get_str(r, cat + ".token");
}

void NtfyProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".server_url", server_url_, cat);
    r.set(cat + ".topic",      topic_,      cat);
    r.set(cat + ".token",      token_,      cat);
}

void NtfyProvider::send(const NotificationRequest& req,
                        std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const QString base = server_url_.isEmpty() ? "https://ntfy.sh" : server_url_;
    const QString url  = QString("%1/%2").arg(base, topic_);

    const QString priority = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:  return "default";
            case NotifLevel::Alert:    return "high";
            case NotifLevel::Critical: return "urgent";
            default:                   return "low";
        }
    }();

    QJsonObject body;
    body["topic"]    = topic_;
    body["title"]    = req.title;
    body["message"]  = req.message;
    body["priority"] = priority;

    // If token is set, we'd normally add an Authorization header.
    // HttpClient doesn't support per-request headers yet, so we embed
    // token as a query param (ntfy supports ?auth=<base64>).
    const QString final_url = token_.isEmpty() ? url
        : url + "?auth=" + QString(token_.toUtf8().toBase64());

    HttpClient::instance().post(final_url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        cb(true, {});
    });
}

} // namespace fincept::notifications
