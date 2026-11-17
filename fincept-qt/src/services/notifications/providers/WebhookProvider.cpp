#include "services/notifications/providers/WebhookProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void WebhookProvider::load_fields(SettingsRepository& r, const QString& cat) {
    url_ = get_secret(r, cat + ".url");
    method_ = get_str(r, cat + ".method");
    if (method_.isEmpty())
        method_ = "POST";
}

void WebhookProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".url", url_, cat);
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

    // POST or PUT only — Settings rejects anything else (it used to be sent as
    // POST without a word). The user's own endpoint may answer with any body —
    // empty, text, JSON, large — and any 2xx is a delivery: send_delivery() takes
    // the answer unparsed (post_raw / put_raw), so nothing is parsed on the GUI thread.
    DeliveryRequest request;
    request.service = DeliveryService::Generic;
    request.verb = method_.trimmed().toUpper() == "PUT" ? DeliveryRequest::Verb::PutJson
                                                         : DeliveryRequest::Verb::PostJson;
    request.url = url_;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
