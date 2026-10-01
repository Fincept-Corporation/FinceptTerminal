#include "services/notifications/providers/OpsgenieProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void OpsgenieProvider::load_fields(SettingsRepository& r, const QString& cat) {
    api_key_ = get_secret(r, cat + ".api_key");
}

void OpsgenieProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".api_key", api_key_, cat);
}

void OpsgenieProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    const QString priority = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:
                return "P3";
            case NotifLevel::Alert:
                return "P2";
            case NotifLevel::Critical:
                return "P1";
            default:
                return "P4";
        }
    }();

    // Opsgenie v2 authenticates with "Authorization: GenieKey <key>". Sent as a
    // per-request header (HttpClient::Headers) so the key never sits in the URL.
    const QString url = "https://api.opsgenie.com/v2/alerts";
    HttpClient::Headers headers;
    headers.insert("Authorization", QByteArray("GenieKey ") + api_key_.toUtf8());

    QJsonObject body;
    body["message"] = req.title;
    body["description"] = req.message;
    body["priority"] = priority;
    body["source"] = "Fincept Terminal";

    HttpClient::instance().post(
        url, body,
        [cb](Result<QJsonDocument> res) {
            if (res.is_err()) {
                cb(false, QString::fromStdString(res.error()));
                return;
            }
            const auto obj = res.value().object();
            const bool ok = obj.contains("requestId");
            cb(ok, ok ? QString{} : obj.value("message").toString());
        },
        nullptr, headers);
}

} // namespace fincept::notifications
