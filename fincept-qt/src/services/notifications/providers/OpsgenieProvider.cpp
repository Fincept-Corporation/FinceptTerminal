#include "services/notifications/providers/OpsgenieProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonObject>

#include <utility>

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
    QJsonObject body;
    body["message"] = req.title;
    body["description"] = req.message;
    body["priority"] = priority;
    body["source"] = "Fincept Terminal";

    // 202 {"result":"Request will be processed","requestId":…} = accepted. Its
    // errors are 4xx and carry a requestId as well, so the old "has requestId"
    // test never told anything apart: the HTTP status decides.
    DeliveryRequest request;
    request.service = DeliveryService::Opsgenie;
    request.url = QStringLiteral("https://api.opsgenie.com/v2/alerts");
    request.json = body;
    request.headers.insert("Authorization", QByteArray("GenieKey ") + api_key_.toUtf8());
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
