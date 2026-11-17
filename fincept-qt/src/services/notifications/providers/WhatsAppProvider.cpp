#include "services/notifications/providers/WhatsAppProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QByteArray>
#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void WhatsAppProvider::load_fields(SettingsRepository& r, const QString& cat) {
    account_sid_ = get_str(r, cat + ".account_sid");
    auth_token_ = get_secret(r, cat + ".auth_token");
    from_number_ = get_str(r, cat + ".from_number");
    to_number_ = get_str(r, cat + ".to_number");
}

void WhatsAppProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".account_sid", account_sid_, cat);
    set_secret(r, cat + ".auth_token", auth_token_, cat);
    r.set(cat + ".from_number", from_number_, cat);
    r.set(cat + ".to_number", to_number_, cat);
}

void WhatsAppProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // Twilio WhatsApp API endpoint. The Messages API only accepts
    // application/x-www-form-urlencoded (a JSON body is rejected) and authenticates
    // with HTTP Basic, so send a form with an Authorization header — no credentials
    // embedded in the URL.
    const QString url = QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json").arg(account_sid_);

    const QString msg = QString("[Fincept] %1\n%2").arg(req.title, req.message);

    // 201 + a message resource on success; an error object carries "code".
    DeliveryRequest request;
    request.service = DeliveryService::Twilio;
    request.verb = DeliveryRequest::Verb::PostForm;
    request.url = url;
    request.form["To"] = to_number_.startsWith("whatsapp:") ? to_number_ : "whatsapp:" + to_number_;
    request.form["From"] = from_number_.startsWith("whatsapp:") ? from_number_ : "whatsapp:" + from_number_;
    request.form["Body"] = msg;
    request.headers.insert("Authorization",
                           "Basic " + QString("%1:%2").arg(account_sid_, auth_token_).toUtf8().toBase64());
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
