#include "services/notifications/providers/NtfyProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void NtfyProvider::load_fields(SettingsRepository& r, const QString& cat) {
    server_url_ = get_str(r, cat + ".server_url");
    topic_ = get_str(r, cat + ".topic");
    token_ = get_secret(r, cat + ".token");
}

void NtfyProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".server_url", server_url_, cat);
    r.set(cat + ".topic", topic_, cat);
    set_secret(r, cat + ".token", token_, cat);
}

void NtfyProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // ntfy's JSON publish API takes a POST to the server ROOT with the topic in the
    // body. Posting this JSON to "<server>/<topic>" instead publishes the raw JSON
    // text as the message body.
    QString base = server_url_.isEmpty() ? QStringLiteral("https://ntfy.sh") : server_url_;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString url = base;

    // JSON publishing needs the numeric priority (1=min .. 5=max/urgent); the
    // names ("high", "urgent") are only valid in the X-Priority header.
    const int priority = [&]() -> int {
        switch (req.level) {
            case NotifLevel::Warning:
                return 3; // default
            case NotifLevel::Alert:
                return 4; // high
            case NotifLevel::Critical:
                return 5; // urgent
            default:
                return 2; // low
        }
    }();

    QJsonObject body;
    body["topic"] = topic_;
    body["title"] = req.title;
    body["message"] = req.message;
    body["priority"] = priority;

    // An access token goes in "Authorization: Bearer <token>" (per-request header,
    // never the URL). The previous "?auth=" query param carried base64(token) —
    // ntfy expects base64 of the *whole* header value there, so it was rejected
    // as well as being a secret in the URL.
    HttpClient::Headers headers;
    if (!token_.isEmpty())
        headers.insert("Authorization", QByteArray("Bearer ") + token_.toUtf8());

    HttpClient::instance().post(
        url, body,
        [cb](Result<QJsonDocument> res) {
            if (res.is_err()) {
                cb(false, QString::fromStdString(res.error()));
                return;
            }
            cb(true, {});
        },
        nullptr, headers);
}

} // namespace fincept::notifications
