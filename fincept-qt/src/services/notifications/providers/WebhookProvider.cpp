#include "services/notifications/providers/WebhookProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void WebhookProvider::load_fields(SettingsRepository& r, const QString& cat) {
    url_ = get_str(r, cat + ".url");
    method_ = get_str(r, cat + ".method");
    if (method_.isEmpty())
        method_ = "POST";
}

void WebhookProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".url", url_, cat);
    r.set(cat + ".method", method_, cat);
}

void WebhookProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    QJsonObject body;
    body["title"] = req.title;
    body["message"] = req.message;
    body["level"] = static_cast<int>(req.level);
    body["timestamp"] = req.timestamp.toUTC().toString(Qt::ISODate);

    auto handler = [cb](Result<QJsonDocument> res) {
        if (res.is_err()) {
            cb(false, QString::fromStdString(res.error()));
            return;
        }
        cb(true, {});
    };

    if (method_.toUpper() == "PUT")
        HttpClient::instance().put(url_, body, handler);
    else
        HttpClient::instance().post(url_, body, handler);
}

} // namespace fincept::notifications
