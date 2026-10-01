#include "services/notifications/providers/WhatsAppProvider.h"

#include "network/http/HttpClient.h"

#include <QByteArray>
#include <QJsonObject>

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

    QMap<QString, QString> form;
    form["To"] = to_number_.startsWith("whatsapp:") ? to_number_ : "whatsapp:" + to_number_;
    form["From"] = from_number_.startsWith("whatsapp:") ? from_number_ : "whatsapp:" + from_number_;
    form["Body"] = msg;

    HttpClient::Headers headers;
    headers.insert("Authorization", "Basic " + QString("%1:%2").arg(account_sid_, auth_token_).toUtf8().toBase64());

    HttpClient::instance().post_form(
        url, form,
        [cb](Result<QJsonDocument> res) {
            if (res.is_err()) {
                cb(false, QString::fromStdString(res.error()));
                return;
            }
            const auto obj = res.value().object();
            const bool ok = !obj.contains("code"); // Twilio errors have a "code" field
            cb(ok, ok ? QString{} : obj.value("message").toString());
        },
        nullptr, headers);
}

} // namespace fincept::notifications
