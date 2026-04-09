#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

class TelegramProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "telegram"; }
    QString display_name() const override { return "Telegram"; }
    QString icon() const override { return "✈"; }
    bool is_configured() const override { return !bot_token_.isEmpty() && !chat_id_.isEmpty(); }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    // Exposed for settings page direct access
    QString bot_token_;
    QString chat_id_;
};

} // namespace fincept::notifications
