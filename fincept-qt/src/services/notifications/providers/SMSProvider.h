#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// SMS via Twilio SMS API.
class SMSProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "sms"; }
    QString display_name() const override { return "SMS (Twilio)"; }
    QString icon() const override { return "💬"; }
    bool is_configured() const override {
        return !account_sid_.isEmpty() && !auth_token_.isEmpty() && !from_number_.isEmpty() && !to_number_.isEmpty();
    }

    void send(const NotificationRequest& req, std::function<void(bool, QString)> cb) override;

  protected:
    void load_fields(SettingsRepository& r, const QString& cat) override;
    void save_fields(SettingsRepository& r, const QString& cat) override;

  public:
    QString account_sid_;
    QString auth_token_;
    QString from_number_;
    QString to_number_;
};

} // namespace fincept::notifications
