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
        QHash<QString, QLineEdit*> fields;    // field_key → input widget
        QHash<QString, QLabel*> field_labels; // field_key → row label (for retranslateUi)
        QPushButton* test_btn = nullptr;
        QFrame* body_frame = nullptr;     // collapsible config area
        QLabel* status_lbl = nullptr;     // inline test result / read problem
        QPushButton* retry_btn = nullptr; // shown while its stored settings cannot be read
    };

    void build_ui();
    /// Read one provider's stored settings into its widgets. A value that cannot
    /// be read (settings row, or a secret in SecureStorage) is never shown as
    /// "not set": the box stays empty, the status line says why, and saving that
    /// provider stays off until a read succeeds (RETRY reads again). A damaged
    /// secret (it can never be read) is kept until the user enters it again.
    void reload_provider(const QString& provider_id);
    /// Validates then persists one provider. Returns false (and, for invalid input,
    /// fills *error_out) when nothing was written.
    bool save_provider_fields(const QString& provider_id, const ProviderWidgets& pw, QString* error_out = nullptr);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QHash<QString, ProviderWidgets> provider_widgets_;

    QCheckBox* trigger_inapp_ = nullptr;
    QCheckBox* trigger_price_ = nullptr;
    QCheckBox* trigger_news_ = nullptr;
    QCheckBox* trigger_orders_ = nullptr;

    // News alert sub-options (visible only when trigger_news_ is checked)
    QCheckBox* news_breaking_ = nullptr;
    QCheckBox* news_monitors_ = nullptr;
    QCheckBox* news_deviations_ = nullptr;
    QCheckBox* news_flash_ = nullptr;
    QFrame* news_subopts_frame_ = nullptr;

    // Section headers + save button (cached for retranslateUi).
    QLabel* providers_hdr_ = nullptr;
    QLabel* triggers_hdr_ = nullptr;
    QPushButton* save_btn_ = nullptr;

    // Trigger / news-subopt row labels + descriptions (cached for retranslateUi).
    QLabel* row_inapp_lbl_ = nullptr;
    QLabel* row_inapp_desc_ = nullptr;
    QLabel* row_price_lbl_ = nullptr;
    QLabel* row_price_desc_ = nullptr;
    QLabel* row_news_lbl_ = nullptr;
    QLabel* row_news_desc_ = nullptr;
    QLabel* row_orders_lbl_ = nullptr;
    QLabel* row_orders_desc_ = nullptr;
    QLabel* row_breaking_lbl_ = nullptr;
    QLabel* row_breaking_desc_ = nullptr;
    QLabel* row_monitors_lbl_ = nullptr;
    QLabel* row_monitors_desc_ = nullptr;
    QLabel* row_deviations_lbl_ = nullptr;
    QLabel* row_deviations_desc_ = nullptr;
    QLabel* row_flash_lbl_ = nullptr;
    QLabel* row_flash_desc_ = nullptr;
};

} // namespace fincept::screens
