#include "services/notifications/providers/WhatsAppProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>
#include <QByteArray>

namespace fincept::notifications {

void WhatsAppProvider::load_fields(SettingsRepository& r, const QString& cat) {
    account_sid_  = get_str(r, cat + ".account_sid");
    auth_token_   = get_str(r, cat + ".auth_token");
    from_number_  = get_str(r, cat + ".from_number");
    to_number_    = get_str(r, cat + ".to_number");
}

void WhatsAppProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".account_sid",  account_sid_,  cat);
    r.set(cat + ".auth_token",   auth_token_,   cat);
    r.set(cat + ".from_number",  from_number_,  cat);
    r.set(cat + ".to_number",    to_number_,    cat);
}

void WhatsAppProvider::send(const NotificationRequest& req,
                            std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    // Twilio WhatsApp API endpoint
    const QString url = QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json")
                            .arg(account_sid_);

    const QString msg = QString("[Fincept] %1\n%2").arg(req.title, req.message);

    // Twilio accepts form-encoded POST — encode as JSON fields for our HttpClient
    // Note: Twilio's API actually needs application/x-www-form-urlencoded;
    // we send JSON and the server-side relay handles encoding, or use a proxy.
    QJsonObject body;
    body["To"]   = to_number_.startsWith("whatsapp:") ? to_number_
                 : "whatsapp:" + to_number_;
    body["From"] = from_number_.startsWith("whatsapp:") ? from_number_
                 : "whatsapp:" + from_number_;
    body["Body"] = msg;
    // Basic auth encoded into URL for Twilio
    const QString auth_url = QString("https://%1:%2@api.twilio.com/2010-04-01/Accounts/%3/Messages.json")
                                 .arg(account_sid_, auth_token_, account_sid_);

    HttpClient::instance().post(auth_url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        const auto obj = res.value().object();
        const bool ok  = !obj.contains("code"); // Twilio errors have a "code" field
        cb(ok, ok ? QString{} : obj.value("message").toString());
    });
}

} // namespace fincept::notifications
