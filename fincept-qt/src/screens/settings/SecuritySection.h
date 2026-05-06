#pragma once
// SecuritySection.h — PIN status, change PIN form, auto-lock policy,
// and security audit log display.

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class SecuritySection : public QWidget {
    Q_OBJECT
  public:
    explicit SecuritySection(QWidget* parent = nullptr);

    /// Refresh PIN status, lockout counter, auto-lock toggles, and audit log.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();
    void refresh_audit_log();

    QLabel*      sec_pin_status_       = nullptr;
    QComboBox*   sec_lock_timeout_     = nullptr;
    QCheckBox*   sec_autolock_toggle_  = nullptr;
    QCheckBox*   sec_lock_on_minimize_ = nullptr;
    QListWidget* sec_audit_list_       = nullptr;
    QLabel*      sec_lockout_status_   = nullptr;
    QPushButton* sec_change_pin_btn_   = nullptr;

    // Change PIN form (shown/hidden dynamically)
    QWidget*   sec_change_pin_form_ = nullptr;
    QLineEdit* sec_current_pin_    = nullptr;
    QLineEdit* sec_new_pin_        = nullptr;
    QLineEdit* sec_confirm_pin_    = nullptr;
    QLabel*    sec_pin_error_      = nullptr;
    QLabel*    sec_pin_success_    = nullptr;
};

} // namespace fincept::screens
