#include "services/notifications/providers/EmailProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void EmailProvider::load_fields(SettingsRepository& r, const QString& cat) {
    smtp_host_  = get_str(r, cat + ".smtp_host");
    smtp_port_  = get_str(r, cat + ".smtp_port");
    smtp_user_  = get_str(r, cat + ".smtp_user");
    smtp_pass_  = get_str(r, cat + ".smtp_pass");
    to_addr_    = get_str(r, cat + ".to_addr");
    from_addr_  = get_str(r, cat + ".from_addr");
}

void EmailProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".smtp_host",  smtp_host_,  cat);
    r.set(cat + ".smtp_port",  smtp_port_,  cat);
    r.set(cat + ".smtp_user",  smtp_user_,  cat);
    r.set(cat + ".smtp_pass",  smtp_pass_,  cat);
    r.set(cat + ".to_addr",    to_addr_,    cat);
    r.set(cat + ".from_addr",  from_addr_,  cat);
}

void EmailProvider::send(const NotificationRequest& req,
                         std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    // POST to the configured SMTP relay HTTP endpoint
    // Expected API: POST /send with JSON {to, from, subject, body, auth}
    const QString url = smtp_host_.startsWith("http") ? smtp_host_
                      : QString("http://%1:%2/send").arg(smtp_host_, smtp_port_.isEmpty() ? "587" : smtp_port_);

    QJsonObject auth;
    auth["user"]     = smtp_user_;
    auth["password"] = smtp_pass_;

    QJsonObject body;
    body["to"]      = to_addr_;
    body["from"]    = from_addr_.isEmpty() ? smtp_user_ : from_addr_;
    body["subject"] = QString("[Fincept] %1").arg(req.title);
    body["body"]    = QString("%1\n\n%2\n\nSent: %3")
                          .arg(req.title, req.message,
                               req.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    body["auth"]    = auth;

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        cb(true, {});
    });
}

} // namespace fincept::notifications
