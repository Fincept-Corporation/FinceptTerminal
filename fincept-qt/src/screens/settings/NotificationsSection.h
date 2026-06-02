#pragma once
// NotificationsSection.h — per-provider accordion settings + alert triggers.
// Providers: telegram, discord, slack, email, whatsapp, pushover, ntfy,
// pushbullet, gotify, mattermost, teams, webhook, pagerduty, opsgenie, sms.

#include <QCheckBox>
#include <QEvent>
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
    void changeEvent(QEvent* event) override;

  private:
    struct ProviderWidgets {
        QCheckBox* enabled = nullptr;
        QHash<QString, QLineEdit*> fields; // field_key → input widget
        QHash<QString, QLabel*> field_labels; // field_key → row label (for retranslateUi)
        QPushButton* test_btn = nullptr;
        QFrame* body_frame = nullptr;      // collapsible config area
        QLabel* status_lbl = nullptr;      // inline test result
    };

    void build_ui();
    void save_provider_fields(const QString& provider_id, const ProviderWidgets& pw);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

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

    // Section headers + save button (cached for retranslateUi).
    QLabel* providers_hdr_ = nullptr;
    QLabel* triggers_hdr_  = nullptr;
    QPushButton* save_btn_ = nullptr;

    // Trigger / news-subopt row labels + descriptions (cached for retranslateUi).
    QLabel* row_inapp_lbl_  = nullptr;  QLabel* row_inapp_desc_  = nullptr;
    QLabel* row_price_lbl_  = nullptr;  QLabel* row_price_desc_  = nullptr;
    QLabel* row_news_lbl_   = nullptr;  QLabel* row_news_desc_   = nullptr;
    QLabel* row_orders_lbl_ = nullptr;  QLabel* row_orders_desc_ = nullptr;
    QLabel* row_breaking_lbl_   = nullptr;  QLabel* row_breaking_desc_   = nullptr;
    QLabel* row_monitors_lbl_   = nullptr;  QLabel* row_monitors_desc_   = nullptr;
    QLabel* row_deviations_lbl_ = nullptr;  QLabel* row_deviations_desc_ = nullptr;
    QLabel* row_flash_lbl_      = nullptr;  QLabel* row_flash_desc_      = nullptr;
};

} // namespace fincept::screens
