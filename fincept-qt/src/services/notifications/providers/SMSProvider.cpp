#include "services/notifications/providers/SMSProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void SMSProvider::load_fields(SettingsRepository& r, const QString& cat) {
    account_sid_  = get_str(r, cat + ".account_sid");
    auth_token_   = get_str(r, cat + ".auth_token");
    from_number_  = get_str(r, cat + ".from_number");
    to_number_    = get_str(r, cat + ".to_number");
}

void SMSProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".account_sid",  account_sid_,  cat);
    r.set(cat + ".auth_token",   auth_token_,   cat);
    r.set(cat + ".from_number",  from_number_,  cat);
    r.set(cat + ".to_number",    to_number_,    cat);
}

void SMSProvider::send(const NotificationRequest& req,
                       std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const QString url = QString("https://%1:%2@api.twilio.com/2010-04-01/Accounts/%3/Messages.json")
                            .arg(account_sid_, auth_token_, account_sid_);

    QJsonObject body;
    body["To"]   = to_number_;
    body["From"] = from_number_;
    body["Body"] = QString("[Fincept] %1: %2").arg(req.title, req.message);

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        const auto obj = res.value().object();
        const bool ok  = !obj.contains("code");
        cb(ok, ok ? QString{} : obj.value("message").toString());
    });
}

} // namespace fincept::notifications
