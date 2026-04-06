#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class GotifyProvider final : public BaseProvider {
  public:
    QString provider_id()   const override { return "gotify"; }
    QString display_name()  const override { return "Gotify"; }
    QString icon()          const override { return "🔊"; }
    bool    is_configured() const override { return !server_url_.isEmpty() && !app_token_.isEmpty(); }

    void send(const NotificationRequest& req,
              std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString server_url_;
    QString app_token_;
};

} // namespace fincept::notifications
