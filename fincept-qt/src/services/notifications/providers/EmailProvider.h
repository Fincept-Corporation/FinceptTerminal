#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// Email provider via SMTP-over-HTTP relay (uses a simple HTTP API endpoint
/// that wraps SMTP, e.g. a self-hosted relay or a service like MailHog/Mailpit).
/// For production use, point smtp_host_ at an HTTP-to-SMTP bridge endpoint.
class EmailProvider final : public BaseProvider {
  public:
    QString provider_id()   const override { return "email"; }
    QString display_name()  const override { return "Email"; }
    QString icon()          const override { return "📧"; }
    bool    is_configured() const override {
        return !smtp_host_.isEmpty() && !to_addr_.isEmpty();
    }

    void send(const NotificationRequest& req,
              std::function<void(bool, QString)> cb) override;

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
};

} // namespace fincept::notifications
