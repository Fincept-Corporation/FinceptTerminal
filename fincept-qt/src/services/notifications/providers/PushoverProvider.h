#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class PushoverProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "pushover"; }
    QString display_name() const override { return "Pushover"; }
    QString icon() const override { return "🔔"; }
    bool is_configured() const override { return !api_token_.isEmpty() && !user_key_.isEmpty(); }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString api_token_;
    QString user_key_;
};

} // namespace fincept::notifications
