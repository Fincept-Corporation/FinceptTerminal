#pragma once
#include "services/notifications/providers/BaseProvider.h"

namespace fincept::notifications {

/// WhatsApp via Twilio's WhatsApp API.
class WhatsAppProvider final : public BaseProvider {
  public:
    QString provider_id() const override { return "whatsapp"; }
    QString display_name() const override { return "WhatsApp"; }
    QString icon() const override { return "📱"; }
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
    QString from_number_; // whatsapp:+14155238886
    QString to_number_;   // whatsapp:+1XXXXXXXXXX
};

} // namespace fincept::notifications
