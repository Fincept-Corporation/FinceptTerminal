#include "services/notifications/providers/EmailProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>
#include <QUrl>

namespace fincept::notifications {

void EmailProvider::load_fields(SettingsRepository& r, const QString& cat) {
    smtp_host_ = get_str(r, cat + ".smtp_host");
    smtp_port_ = get_str(r, cat + ".smtp_port");
    smtp_user_ = get_str(r, cat + ".smtp_user");
    smtp_pass_ = get_secret(r, cat + ".smtp_pass");
    to_addr_ = get_str(r, cat + ".to_addr");
    from_addr_ = get_str(r, cat + ".from_addr");
}

void EmailProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".smtp_host", smtp_host_, cat);
    r.set(cat + ".smtp_port", smtp_port_, cat);
    r.set(cat + ".smtp_user", smtp_user_, cat);
    set_secret(r, cat + ".smtp_pass", smtp_pass_, cat);
    r.set(cat + ".to_addr", to_addr_, cat);
    r.set(cat + ".from_addr", from_addr_, cat);
}

QString EmailProvider::build_endpoint() const {
    const QString host = smtp_host_.trimmed();
    if (host.isEmpty())
        return {};

    // A bare host:port (no scheme) gets https, not http. The old code used
    // `startsWith("http")` — which also matched "https" but forced http:// on
    // literally every other value, including a hostname beginning with "http".
    const bool has_scheme = host.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
                            host.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    const QString candidate =
        has_scheme ? host
                   : QStringLiteral("https://%1:%2/send").arg(host, smtp_port_.isEmpty() ? "587" : smtp_port_);

    const QUrl url(candidate);
    if (!url.isValid() || url.host().isEmpty())
        return {};

    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        // Only tolerate cleartext when the relay is on this machine — there is
        // no network hop for an attacker to sit on. Everything else is refused.
        const QString h = url.host();
        const bool is_loopback = h == QStringLiteral("127.0.0.1") || h == QStringLiteral("::1") ||
                                 h == QStringLiteral("[::1]") ||
                                 h.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0;
        if (!is_loopback)
            return {};
    }
    return candidate;
}

void EmailProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // POST to the configured SMTP relay HTTP endpoint.
    // Expected API: POST /send with JSON {to, from, subject, body, auth}
    //
    // SECURITY: the request body carries the SMTP password in cleartext. It is
    // stored encrypted at rest, so shipping it over plain http would be the one
    // place in the tree where an external credential crosses the network
    // unprotected. The scheme therefore defaults to https, and an explicit
    // http:// host is rejected unless it is loopback (a relay running on the
    // user's own machine, where there is no network to sniff).
    const QString url = build_endpoint();
    if (url.isEmpty()) {
        cb(false, "Refusing to send the SMTP password over an unencrypted connection. "
                  "Use an https:// relay endpoint (or a relay on 127.0.0.1).");
        return;
    }

    QJsonObject auth;
    auth["user"] = smtp_user_;
    auth["password"] = smtp_pass_;

    QJsonObject body;
    body["to"] = to_addr_;
    body["from"] = from_addr_.isEmpty() ? smtp_user_ : from_addr_;
    body["subject"] = QString("[Fincept] %1").arg(req.title);
    body["body"] =
        QString("%1\n\n%2\n\nSent: %3").arg(req.title, req.message, req.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    body["auth"] = auth;

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) {
            cb(false, QString::fromStdString(res.error()));
            return;
        }
        cb(true, {});
    });
}

} // namespace fincept::notifications
