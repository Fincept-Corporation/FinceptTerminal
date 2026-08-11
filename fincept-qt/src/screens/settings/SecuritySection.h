#pragma once
// SecuritySection.h — PIN status, change PIN form, auto-lock policy,
// and security audit log display.

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
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
    /// Wipe the three PIN entry fields and collapse the change-PIN form when
    /// the section leaves view — a typed-but-unsubmitted PIN must not sit in a
    /// QLineEdit for the rest of the session.
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void refresh_audit_log();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* sec_pin_status_ = nullptr;
    QComboBox* sec_lock_timeout_ = nullptr;
    QCheckBox* sec_autolock_toggle_ = nullptr;
    QCheckBox* sec_lock_on_minimize_ = nullptr;
    QListWidget* sec_audit_list_ = nullptr;
    QLabel* sec_lockout_status_ = nullptr;
    QPushButton* sec_change_pin_btn_ = nullptr;

    /// Grants destructive MCP tools (`mcp/allow_destructive_tools`). Applies
    /// immediately on toggle — it is a capability grant, not a form field, so
    /// it deliberately does not wait for "Save Security Settings".
    QCheckBox* sec_allow_destructive_ = nullptr;

    // Change PIN form (shown/hidden dynamically)
    QWidget* sec_change_pin_form_ = nullptr;
    QLineEdit* sec_current_pin_ = nullptr;
    QLineEdit* sec_new_pin_ = nullptr;
    QLineEdit* sec_confirm_pin_ = nullptr;
    QLabel* sec_pin_error_ = nullptr;
    QLabel* sec_pin_success_ = nullptr;

    // Section titles, buttons, and row labels (cached for retranslateUi).
    QLabel* title_pin_ = nullptr;
    QLabel* title_change_ = nullptr;
    QLabel* title_lock_ = nullptr;
    QLabel* title_ai_tools_ = nullptr;
    QLabel* title_audit_ = nullptr;
    QLabel* audit_note_ = nullptr;
    QPushButton* save_pin_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    /// Inline confirmation / warning for the "Save Security Settings" button.
    /// Saving previously produced no visible feedback at all, and enabling
    /// auto-lock without a configured PIN silently did nothing.
    QLabel* save_status_ = nullptr;
    QPushButton* refresh_audit_btn_ = nullptr;

    QLabel* row_pin_status_lbl_ = nullptr;
    QLabel* row_pin_status_desc_ = nullptr;
    QLabel* row_attempts_lbl_ = nullptr;
    QLabel* row_attempts_desc_ = nullptr;
    QLabel* row_current_lbl_ = nullptr;
    QLabel* row_new_lbl_ = nullptr;
    QLabel* row_confirm_lbl_ = nullptr;
    QLabel* row_autolock_lbl_ = nullptr;
    QLabel* row_autolock_desc_ = nullptr;
    QLabel* row_timeout_lbl_ = nullptr;
    QLabel* row_timeout_desc_ = nullptr;
    QLabel* row_minimize_lbl_ = nullptr;
    QLabel* row_minimize_desc_ = nullptr;
    QLabel* row_destructive_lbl_ = nullptr;
    QLabel* row_destructive_desc_ = nullptr;
};

} // namespace fincept::screens
