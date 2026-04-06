#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// Generic HTTP webhook — posts a JSON payload to any URL.
class WebhookProvider final : public BaseProvider {
  public:
    QString provider_id()   const override { return "webhook"; }
    QString display_name()  const override { return "Webhook"; }
    QString icon()          const override { return "🌐"; }
    bool    is_configured() const override { return !url_.isEmpty(); }

    void send(const NotificationRequest& req,
              std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString url_;
    QString method_;  // POST, PUT, PATCH
};

} // namespace fincept::notifications
