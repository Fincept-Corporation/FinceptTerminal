#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class PagerDutyProvider final : public BaseProvider {
  public:
    QString provider_id()   const override { return "pagerduty"; }
    QString display_name()  const override { return "PagerDuty"; }
    QString icon()          const override { return "🚨"; }
    bool    is_configured() const override { return !routing_key_.isEmpty(); }

    void send(const NotificationRequest& req,
              std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString routing_key_;
};

} // namespace fincept::notifications
