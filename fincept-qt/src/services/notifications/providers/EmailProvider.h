#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// Email provider. Settings > Notifications > EMAIL asks for an SMTP server
/// (host, port, username, password) and this sends through it: real SMTP via
/// scripts/notify_smtp.py (Python standard library smtplib) — implicit TLS on
/// port 465, STARTTLS on any other port. The password reaches the script on
/// stdin, never on its command line.
///
/// A host written as a full http(s):// URL is the older "SMTP-over-HTTP relay"
/// mode, kept for anyone who set one up: a JSON POST {to, from, subject, body,
/// auth} to that URL.
class EmailProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "email"; }
    QString display_name() const override { return "Email"; }
    QString icon() const override { return ""; }
    bool is_configured() const override { return !smtp_host_.isEmpty() && !to_addr_.isEmpty(); }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

    /// True when smtp_host_ is an http:// or https:// URL (relay mode).
    bool uses_http_relay() const;

    /// Relay mode only: the URL to POST to.
    ///
    /// The request body carries the SMTP password in cleartext, so it must not
    /// travel over plain http. Returns an empty string when smtp_host_ is not a
    /// URL, or when it would force an unencrypted non-loopback connection — the
    /// caller must then refuse to send.
    QString build_endpoint() const;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString smtp_host_;
    QString smtp_port_;
    QString smtp_user_;
    QString smtp_pass_;
    QString to_addr_;
    QString from_addr_;

  private:
    void send_smtp(const QString& subject, const QString& text, std::function<void(bool, QString)> cb);
    void send_relay(const QString& subject, const QString& text, std::function<void(bool, QString)> cb);
};

} // namespace fincept::notifications
