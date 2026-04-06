#include "services/notifications/providers/OpsgenieProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void OpsgenieProvider::load_fields(SettingsRepository& r, const QString& cat) {
    api_key_ = get_str(r, cat + ".api_key");
}

void OpsgenieProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".api_key", api_key_, cat);
}

void OpsgenieProvider::send(const NotificationRequest& req,
                            std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const QString priority = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:  return "P3";
            case NotifLevel::Alert:    return "P2";
            case NotifLevel::Critical: return "P1";
            default:                   return "P4";
        }
    }();

    // Opsgenie requires the API key in the Authorization header.
    // We embed it as a query param since HttpClient has shared auth headers.
    const QString url = "https://api.opsgenie.com/v2/alerts?apiKey=" + api_key_;

    QJsonObject body;
    body["message"]     = req.title;
    body["description"] = req.message;
    body["priority"]    = priority;
    body["source"]      = "Fincept Terminal";

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        const auto obj = res.value().object();
        const bool ok  = obj.contains("requestId");
        cb(ok, ok ? QString{} : obj.value("message").toString());
    });
}

} // namespace fincept::notifications
