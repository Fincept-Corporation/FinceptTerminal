#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class OpsgenieProvider final : public BaseProvider {
  public:
    QString provider_id()   const override { return "opsgenie"; }
    QString display_name()  const override { return "Opsgenie"; }
    QString icon()          const override { return "🔴"; }
    bool    is_configured() const override { return !api_key_.isEmpty(); }

    void send(const NotificationRequest& req,
              std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString api_key_;
};

} // namespace fincept::notifications
