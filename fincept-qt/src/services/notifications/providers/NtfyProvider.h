#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// ntfy.sh — open-source push notification service.
class NtfyProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "ntfy"; }
    QString display_name() const override { return "ntfy"; }
    QString icon() const override { return "📢"; }
    bool is_configured() const override { return !topic_.isEmpty(); }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString server_url_; // default: https://ntfy.sh
    QString topic_;
    QString token_; // optional auth token
};

} // namespace fincept::notifications
