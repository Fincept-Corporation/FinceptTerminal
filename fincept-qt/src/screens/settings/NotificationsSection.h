#pragma once
// NotificationsSection.h — per-provider accordion settings + alert triggers.
// Providers: telegram, discord, slack, email, whatsapp, pushover, ntfy,
// pushbullet, gotify, mattermost, teams, webhook, pagerduty, opsgenie, sms.

#include <QCheckBox>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace fincept::screens {

class NotificationsSection : public QWidget {
    Q_OBJECT
  public:
    explicit NotificationsSection(QWidget* parent = nullptr);

    /// Restore field values + toggles from SettingsRepository.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    struct ProviderWidgets {
        QCheckBox* enabled = nullptr;
        QHash<QString, QLineEdit*> fields; // field_key → input widget
        QPushButton* test_btn = nullptr;
        QFrame* body_frame = nullptr;      // collapsible config area
        QLabel* status_lbl = nullptr;      // inline test result
    };

    void build_ui();
    void save_provider_fields(const QString& provider_id, const ProviderWidgets& pw);

    QHash<QString, ProviderWidgets> provider_widgets_;

    QCheckBox* trigger_inapp_   = nullptr;
    QCheckBox* trigger_price_   = nullptr;
    QCheckBox* trigger_news_    = nullptr;
    QCheckBox* trigger_orders_  = nullptr;

    // News alert sub-options (visible only when trigger_news_ is checked)
    QCheckBox* news_breaking_   = nullptr;
    QCheckBox* news_monitors_   = nullptr;
    QCheckBox* news_deviations_ = nullptr;
    QCheckBox* news_flash_      = nullptr;
    QFrame*    news_subopts_frame_ = nullptr;
};

} // namespace fincept::screens
