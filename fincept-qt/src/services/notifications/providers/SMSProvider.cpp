#include "services/notifications/providers/SMSProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void SMSProvider::load_fields(SettingsRepository& r, const QString& cat) {
    account_sid_ = get_str(r, cat + ".account_sid");
    auth_token_ = get_secret(r, cat + ".auth_token");
    from_number_ = get_str(r, cat + ".from_number");
    to_number_ = get_str(r, cat + ".to_number");
}

void SMSProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".account_sid", account_sid_, cat);
    set_secret(r, cat + ".auth_token", auth_token_, cat);
    r.set(cat + ".from_number", from_number_, cat);
    r.set(cat + ".to_number", to_number_, cat);
}

void SMSProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // Twilio's Messages API only accepts application/x-www-form-urlencoded (a JSON
    // body is rejected) and authenticates with HTTP Basic: send it as a form with an
    // Authorization header rather than credentials embedded in the URL.
    const QString url = QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json").arg(account_sid_);

    QMap<QString, QString> form;
    form["To"] = to_number_;
    form["From"] = from_number_;
    form["Body"] = QString("[Fincept] %1: %2").arg(req.title, req.message);

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
            const bool ok = !obj.contains("code");
            cb(ok, ok ? QString{} : obj.value("message").toString());
        },
        nullptr, headers);
}

} // namespace fincept::notifications
