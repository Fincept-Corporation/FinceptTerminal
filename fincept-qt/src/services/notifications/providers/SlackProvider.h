#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class SlackProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "slack"; }
    QString display_name() const override { return "Slack"; }
    QString icon() const override { return "💬"; }
    bool is_configured() const override { return !webhook_url_.isEmpty(); }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString webhook_url_;
    QString channel_; // optional override, e.g. "#alerts"
};

} // namespace fincept::notifications
