#include "services/notifications/providers/EmailProvider.h"

#include "python/PythonRunner.h"
#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

#include <utility>

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

bool EmailProvider::uses_http_relay() const {
    const QString host = smtp_host_.trimmed();
    return host.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
           host.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

QString EmailProvider::build_endpoint() const {
    // Relay mode only. A bare host is an SMTP server (send_smtp) — it used to be
    // turned into "https://<host>:<port>/send", i.e. a TLS handshake against an
    // SMTP port, so every real mail server failed.
    if (!uses_http_relay())
        return {};
    const QString candidate = smtp_host_.trimmed();

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

    const QString subject = QString("[Fincept] %1").arg(req.title);
    const QString text =
        QString("%1\n\n%2\n\nSent: %3").arg(req.title, req.message, req.timestamp.toString("yyyy-MM-dd hh:mm:ss"));

    if (uses_http_relay())
        send_relay(subject, text, std::move(cb));
    else
        send_smtp(subject, text, std::move(cb));
}

void EmailProvider::send_smtp(const QString& subject, const QString& text, std::function<void(bool, QString)> cb) {
    // Real SMTP through scripts/notify_smtp.py (smtplib: implicit TLS on 465,
    // STARTTLS otherwise; it refuses to send the password over a connection
    // that stays unencrypted unless the server is on this machine).
    QJsonObject job;
    job["host"] = smtp_host_.trimmed();
    job["port"] = smtp_port_.trimmed().isEmpty() ? QStringLiteral("587") : smtp_port_.trimmed();
    job["user"] = smtp_user_.trimmed();
    job["password"] = smtp_pass_;
    job["from"] = from_addr_.trimmed().isEmpty() ? smtp_user_.trimmed() : from_addr_.trimmed();
    job["to"] = to_addr_.trimmed();
    job["subject"] = subject;
    job["body"] = text;

    python::PythonRunner::RunOptions opts;
    // Connect + TLS + login + send, each bounded by the script's own 30 s timeout.
    opts.timeout_ms = 90 * 1000;
    // The password goes on stdin — argv is readable by other processes.
    opts.stdin_data = QJsonDocument(job).toJson(QJsonDocument::Compact);
    // The script reads no stored credentials: inject none into its environment.
    opts.credential_allowlist = QStringList{};

    python::PythonRunner::instance().run_with_options(
        QStringLiteral("notify_smtp.py"), {}, opts, [cb](python::PythonResult r) {
            if (r.success) {
                cb(true, {});
                return;
            }
            const QString err = r.error.trimmed();
            cb(false, err.isEmpty() ? QStringLiteral("The email could not be sent") : err);
        });
}

void EmailProvider::send_relay(const QString& subject, const QString& text, std::function<void(bool, QString)> cb) {
    // POST to the configured SMTP-over-HTTP relay endpoint.
    // Expected API: POST <url> with JSON {to, from, subject, body, auth}
    //
    // SECURITY: the request body carries the SMTP password in cleartext. It is
    // stored encrypted at rest, so shipping it over plain http would be the one
    // place in the tree where an external credential crosses the network
    // unprotected. An http:// relay is therefore rejected unless it is loopback
    // (a relay running on the user's own machine, where there is no network to sniff).
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
    body["subject"] = subject;
    body["body"] = text;
    body["auth"] = auth;

    // The user's own relay: any 2xx is a delivery, whatever its body.
    DeliveryRequest request;
    request.service = DeliveryService::Generic;
    request.url = url;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
