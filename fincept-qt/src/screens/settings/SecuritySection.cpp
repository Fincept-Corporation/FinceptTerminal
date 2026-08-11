// SecuritySection.cpp — PIN management, auto-lock policy, and audit log.

#include "screens/settings/SecuritySection.h"

#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
/// Dynamic-property guard set by reload() when a settings *read* failed. While
/// it is set, Save is disabled and its handler refuses to write — the widgets
/// hold defaults, not the user's values, and set() is an INSERT OR REPLACE.
constexpr const char* kSecurityReadFailedProp = "fincept_security_read_failed";

// make_row() builds the label (and optional description) QLabel internally;
// grab them back from the row's direct child QLabels so retranslateUi() can
// re-apply text. Order is deterministic: title label first, description second.
void capture_row_labels(QWidget* row, QLabel** label_out, QLabel** desc_out = nullptr) {
    const auto labels = row->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    if (!labels.isEmpty() && label_out)
        *label_out = labels.at(0);
    if (labels.size() > 1 && desc_out)
        *desc_out = labels.at(1);
}

// Long-form explanation for the destructive-tools grant. One definition so
// build_ui() and retranslateUi() cannot drift apart.
QString destructive_tools_desc() {
    return SecuritySection::tr(
        "Off by default. With this off, AI chat and agents can read your terminal but cannot change anything "
        "— no file writes, no spreadsheet, note, watchlist or portfolio edits, no dashboard or workspace "
        "changes, no script execution. Turning it on lets them act for you; you can turn it back off at any "
        "time. Live trading orders and access to your saved credentials stay blocked either way. Applies "
        "immediately — no need to press Save.");
}
} // namespace

SecuritySection::SecuritySection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void SecuritySection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void SecuritySection::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // Spontaneous hides are window-manager events (minimise / workspace
    // switch); the user has not left the form.
    if (e && e->spontaneous())
        return;

    for (QLineEdit* f : {sec_current_pin_, sec_new_pin_, sec_confirm_pin_}) {
        if (f)
            f->clear();
    }
    if (sec_pin_error_)
        sec_pin_error_->hide();
    if (sec_pin_success_)
        sec_pin_success_->hide();
    if (sec_change_pin_form_)
        sec_change_pin_form_->setVisible(false);
    if (sec_change_pin_btn_)
        sec_change_pin_btn_->setText(tr("Change PIN"));
}

void SecuritySection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    // ── PIN STATUS ────────────────────────────────────────────────────────────
    title_pin_ = new QLabel(tr("PIN AUTHENTICATION"));
    title_pin_->setStyleSheet(section_title_ss());
    vl->addWidget(title_pin_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    sec_pin_status_ = new QLabel;
    sec_pin_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    auto* row_pin_status =
        make_row(tr("PIN Status"), sec_pin_status_, tr("A 6-digit PIN is required to unlock the terminal."));
    capture_row_labels(row_pin_status, &row_pin_status_lbl_, &row_pin_status_desc_);
    vl->addWidget(row_pin_status);

    sec_lockout_status_ = new QLabel;
    sec_lockout_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    auto* row_attempts =
        make_row(tr("Failed Attempts"), sec_lockout_status_, tr("PIN lockout engages after 5 consecutive failures."));
    capture_row_labels(row_attempts, &row_attempts_lbl_, &row_attempts_desc_);
    vl->addWidget(row_attempts);

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── CHANGE PIN ────────────────────────────────────────────────────────────
    title_change_ = new QLabel(tr("CHANGE PIN"));
    title_change_->setStyleSheet(sub_title_ss());
    vl->addWidget(title_change_);
    vl->addSpacing(4);

    sec_change_pin_btn_ = new QPushButton(tr("Change PIN"));
    sec_change_pin_btn_->setFixedWidth(140);
    sec_change_pin_btn_->setStyleSheet(btn_secondary_ss());
    vl->addWidget(sec_change_pin_btn_);

    sec_change_pin_form_ = new QWidget(this);
    sec_change_pin_form_->setStyleSheet("background: transparent;");
    auto* cpfl = new QVBoxLayout(sec_change_pin_form_);
    cpfl->setContentsMargins(0, 8, 0, 0);
    cpfl->setSpacing(6);

    auto make_pin_field = [&](const QString& placeholder) {
        auto* input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setMaxLength(6);
        input->setEchoMode(QLineEdit::Password);
        input->setFixedWidth(200);
        input->setStyleSheet(input_ss());
        // Match the lock screen's hardening: digits only, no predictive text,
        // no IME candidate window, no context menu (paste-based scripting).
        input->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText |
                                   Qt::ImhNoAutoUppercase | Qt::ImhHiddenText);
        input->setContextMenuPolicy(Qt::NoContextMenu);
        input->setAccessibleName(placeholder);
        return input;
    };

    sec_current_pin_ = make_pin_field(tr("Current PIN"));
    auto* row_current = make_row(tr("Current PIN"), sec_current_pin_);
    capture_row_labels(row_current, &row_current_lbl_);
    cpfl->addWidget(row_current);

    sec_new_pin_ = make_pin_field(tr("New PIN"));
    auto* row_new = make_row(tr("New PIN"), sec_new_pin_);
    capture_row_labels(row_new, &row_new_lbl_);
    cpfl->addWidget(row_new);

    sec_confirm_pin_ = make_pin_field(tr("Confirm PIN"));
    auto* row_confirm = make_row(tr("Confirm PIN"), sec_confirm_pin_);
    capture_row_labels(row_confirm, &row_confirm_lbl_);
    cpfl->addWidget(row_confirm);

    sec_pin_error_ = new QLabel;
    sec_pin_error_->setWordWrap(true);
    sec_pin_error_->setStyleSheet(
        QString("color:%1;background:rgba(220,38,38,0.08);border:1px solid %2;padding:6px 8px;")
            .arg(ui::colors::NEGATIVE(), ui::colors::NEGATIVE_DIM()));
    sec_pin_error_->hide();
    cpfl->addWidget(sec_pin_error_);

    sec_pin_success_ = new QLabel;
    sec_pin_success_->setWordWrap(true);
    sec_pin_success_->setStyleSheet(
        QString("color:%1;background:rgba(22,163,74,0.08);border:1px solid %2;padding:6px 8px;")
            .arg(ui::colors::POSITIVE(), ui::colors::POSITIVE_DIM()));
    sec_pin_success_->hide();
    cpfl->addWidget(sec_pin_success_);

    save_pin_btn_ = new QPushButton(tr("Update PIN"));
    save_pin_btn_->setFixedWidth(140);
    save_pin_btn_->setStyleSheet(btn_primary_ss());
    cpfl->addWidget(save_pin_btn_);

    // Enter anywhere in the change-PIN form submits it; explicit tab order so
    // the three fields chain into the Update button.
    for (QLineEdit* f : {sec_current_pin_, sec_new_pin_, sec_confirm_pin_})
        connect(f, &QLineEdit::returnPressed, save_pin_btn_, &QPushButton::click);
    setTabOrder(sec_current_pin_, sec_new_pin_);
    setTabOrder(sec_new_pin_, sec_confirm_pin_);
    setTabOrder(sec_confirm_pin_, save_pin_btn_);
    save_pin_btn_->setAccessibleName(tr("Update PIN"));
    sec_change_pin_btn_->setAccessibleName(tr("Change PIN"));

    sec_change_pin_form_->hide();
    vl->addWidget(sec_change_pin_form_);

    connect(sec_change_pin_btn_, &QPushButton::clicked, this, [this]() {
        bool showing = sec_change_pin_form_->isVisible();
        sec_change_pin_form_->setVisible(!showing);
        sec_change_pin_btn_->setText(showing ? tr("Change PIN") : tr("Cancel"));
        if (!showing) {
            sec_current_pin_->clear();
            sec_new_pin_->clear();
            sec_confirm_pin_->clear();
            sec_pin_error_->hide();
            sec_pin_success_->hide();
            sec_current_pin_->setFocus();
        }
    });

    // Save PIN — all validation in PinManager::change_pin so wrong-current-PIN
    // correctly feeds the shared lockout counter; UI only checks new == confirm.
    connect(save_pin_btn_, &QPushButton::clicked, this, [this]() {
        sec_pin_error_->hide();
        sec_pin_success_->hide();

        auto& pm = auth::PinManager::instance();
        const QString current = sec_current_pin_->text();
        const QString new_pin = sec_new_pin_->text();
        const QString confirm = sec_confirm_pin_->text();

        if (pm.is_locked_out()) {
            int secs = pm.lockout_remaining_seconds();
            sec_pin_error_->setText(tr("Locked out — try again in %1s").arg(secs));
            sec_pin_error_->show();
            sec_current_pin_->clear();
            sec_new_pin_->clear();
            sec_confirm_pin_->clear();
            return;
        }

        if (new_pin != confirm) {
            sec_pin_error_->setText(tr("New PINs do not match"));
            sec_pin_error_->show();
            sec_new_pin_->clear();
            sec_confirm_pin_->clear();
            sec_new_pin_->setFocus();
            return;
        }

        auto result = pm.change_pin(current, new_pin);
        if (result.is_err()) {
            if (pm.is_locked_out()) {
                int secs = pm.lockout_remaining_seconds();
                sec_pin_error_->setText(tr("Too many failed attempts — locked for %1s").arg(secs));
            } else {
                sec_pin_error_->setText(QString::fromStdString(result.error()));
            }
            sec_pin_error_->show();
            sec_current_pin_->clear();
            sec_current_pin_->setFocus();
            return;
        }

        sec_pin_success_->setText(tr("PIN updated successfully"));
        sec_pin_success_->show();
        sec_current_pin_->clear();
        sec_new_pin_->clear();
        sec_confirm_pin_->clear();
        reload();

        LOG_INFO("Settings", "PIN changed via Security settings");
    });

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── AUTO-LOCK ─────────────────────────────────────────────────────────────
    title_lock_ = new QLabel(tr("AUTO-LOCK"));
    title_lock_->setStyleSheet(sub_title_ss());
    vl->addWidget(title_lock_);
    vl->addSpacing(4);

    sec_autolock_toggle_ = new QCheckBox(tr("Enable auto-lock on inactivity"));
    sec_autolock_toggle_->setChecked(true);
    sec_autolock_toggle_->setStyleSheet(check_ss());
    auto* row_autolock =
        make_row(tr("Auto-Lock"), sec_autolock_toggle_, tr("Locks the terminal after a period of inactivity."));
    capture_row_labels(row_autolock, &row_autolock_lbl_, &row_autolock_desc_);
    vl->addWidget(row_autolock);

    sec_lock_timeout_ = new QComboBox;
    sec_lock_timeout_->addItem(tr("1 min"), 1);
    sec_lock_timeout_->addItem(tr("2 min"), 2);
    sec_lock_timeout_->addItem(tr("5 min"), 5);
    sec_lock_timeout_->addItem(tr("10 min"), 10);
    sec_lock_timeout_->addItem(tr("15 min"), 15);
    sec_lock_timeout_->addItem(tr("30 min"), 30);
    sec_lock_timeout_->addItem(tr("60 min"), 60);
    sec_lock_timeout_->setCurrentIndex(3);
    sec_lock_timeout_->setStyleSheet(combo_ss());
    auto* row_timeout =
        make_row(tr("Lock Timeout"), sec_lock_timeout_, tr("Time of inactivity before the terminal locks."));
    capture_row_labels(row_timeout, &row_timeout_lbl_, &row_timeout_desc_);
    vl->addWidget(row_timeout);

    connect(sec_autolock_toggle_, &QCheckBox::toggled, this,
            [this](bool checked) { sec_lock_timeout_->setEnabled(checked); });

    sec_lock_on_minimize_ = new QCheckBox(tr("Lock when the window is minimized"));
    sec_lock_on_minimize_->setStyleSheet(check_ss());
    auto* row_minimize = make_row(tr("Lock on Minimize"), sec_lock_on_minimize_,
                                  tr("When on, minimizing the terminal immediately shows the PIN screen."));
    capture_row_labels(row_minimize, &row_minimize_lbl_, &row_minimize_desc_);
    vl->addWidget(row_minimize);

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    save_btn_ = new QPushButton(tr("Save Security Settings"));
    save_btn_->setFixedWidth(200);
    save_btn_->setStyleSheet(btn_primary_ss());
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        if (property(kSecurityReadFailedProp).toBool()) {
            LOG_WARN("Settings", "Refusing to save security settings — the stored values could not be read, so the "
                                 "widgets on screen are defaults rather than the user's settings");
            return;
        }

        auto& repo = SettingsRepository::instance();
        auto& guard = auth::InactivityGuard::instance();
        auto& pm = auth::PinManager::instance();

        bool autolock = sec_autolock_toggle_->isChecked();
        int minutes = sec_lock_timeout_->currentData().toInt();

        repo.set("security.autolock_enabled", autolock ? "true" : "false", "security");
        repo.set("security.lock_timeout_minutes", QString::number(minutes), "security");
        if (sec_lock_on_minimize_) {
            repo.set("security.lock_on_minimize", sec_lock_on_minimize_->isChecked() ? "true" : "false", "security");
        }

        // Always update interval; only flip enabled if a PIN is configured
        // (otherwise the timer fires but show_lock_screen suppresses the
        // request, leaving an invisible churning timer).
        guard.set_timeout_minutes(minutes);
        guard.set_enabled(autolock && pm.has_pin());

        if (save_status_) {
            if (autolock && !pm.has_pin()) {
                save_status_->setText(tr("Saved — but auto-lock stays off until you set a PIN."));
                save_status_->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::WARNING()));
            } else {
                save_status_->setText(tr("Security settings saved."));
                save_status_->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
            }
            save_status_->show();
            QTimer::singleShot(4000, save_status_, &QLabel::hide);
        }

        LOG_INFO("Settings", QString("Security settings saved: autolock=%1, timeout=%2min, has_pin=%3")
                                 .arg(autolock)
                                 .arg(minutes)
                                 .arg(pm.has_pin()));
    });
    vl->addWidget(save_btn_);

    save_status_ = new QLabel;
    save_status_->setWordWrap(true);
    save_status_->hide();
    vl->addWidget(save_status_);

    // ── AI TOOL PERMISSIONS ───────────────────────────────────────────────────
    //
    // Backs `mcp/allow_destructive_tools`. Every MCP tool flagged
    // `is_destructive` — spreadsheet and file writes, dashboard / workspace /
    // layout mutations, notes, watchlist, portfolio, report builder, agent
    // execution, run_python_script — fails closed unless this is on. It is the
    // ONLY place that grant can be made, so without it the whole "let the
    // assistant drive the terminal" surface is permanently read-only.
    //
    // Deliberately outside the Save button above: this is a capability grant,
    // not a form field, and a half-applied capability is worse than either
    // state. McpProvider::set_destructive_allowed persists it (AppConfig, same
    // mechanism LoggingSection and VoiceConfigSection use) and updates the live
    // session in one call — there is no second copy of this setting to keep in
    // sync.
    vl->addSpacing(16);
    title_ai_tools_ = new QLabel(tr("AI TOOL PERMISSIONS"));
    title_ai_tools_->setStyleSheet(sub_title_ss());
    vl->addWidget(title_ai_tools_);
    vl->addSpacing(4);

    sec_allow_destructive_ = new QCheckBox(tr("Allow AI agents to modify files, workspaces and data"));
    sec_allow_destructive_->setStyleSheet(check_ss());
    sec_allow_destructive_->setAccessibleName(tr("Allow AI agents to modify files, workspaces and data"));
    auto* row_destructive =
        make_row(tr("Destructive AI Tools"), sec_allow_destructive_, destructive_tools_desc());
    capture_row_labels(row_destructive, &row_destructive_lbl_, &row_destructive_desc_);
    vl->addWidget(row_destructive);

    connect(sec_allow_destructive_, &QCheckBox::toggled, this, [this](bool checked) {
        mcp::McpProvider::set_destructive_allowed(checked);
        // A capability grant belongs in the same audit stream the user is
        // already looking at, two rows further down this page.
        auth::SecurityAuditLog::instance().record(checked ? QStringLiteral("ai_destructive_tools_enabled")
                                                          : QStringLiteral("ai_destructive_tools_disabled"));
        refresh_audit_log();
        LOG_INFO("Settings",
                 QString("Destructive AI tools %1 from Security settings").arg(checked ? "enabled" : "disabled"));
    });

    // ── AUDIT LOG ─────────────────────────────────────────────────────────────
    vl->addSpacing(16);
    title_audit_ = new QLabel(tr("AUDIT LOG"));
    title_audit_->setStyleSheet(sub_title_ss());
    vl->addWidget(title_audit_);
    vl->addSpacing(4);

    audit_note_ = new QLabel(tr("Recent security events (PIN setup, failed unlocks, inactivity locks)."));
    audit_note_->setWordWrap(true);
    audit_note_->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(ui::colors::TEXT_DIM()));
    vl->addWidget(audit_note_);

    sec_audit_list_ = new QListWidget;
    sec_audit_list_->setFixedHeight(180);
    sec_audit_list_->setStyleSheet(
        QString("QListWidget { background:%1; color:%2; border:1px solid %3;"
                "font-family:'Consolas','Courier New',monospace; font-size:12px; }"
                "QListWidget::item { padding:2px 6px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    vl->addWidget(sec_audit_list_);

    refresh_audit_btn_ = new QPushButton(tr("Refresh"));
    refresh_audit_btn_->setFixedWidth(140);
    refresh_audit_btn_->setStyleSheet(btn_secondary_ss());
    connect(refresh_audit_btn_, &QPushButton::clicked, this, [this]() { refresh_audit_log(); });
    vl->addWidget(refresh_audit_btn_);

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

void SecuritySection::refresh_audit_log() {
    if (!sec_audit_list_)
        return;
    sec_audit_list_->clear();
    const auto events = auth::SecurityAuditLog::instance().recent(100);
    for (const auto& e : events) {
        const QString ts = e.timestamp.toString("yyyy-MM-dd hh:mm:ss");
        const QString line = e.detail.isEmpty() ? QString("%1  %2").arg(ts, e.event)
                                                : QString("%1  %2  (%3)").arg(ts, e.event, e.detail);
        sec_audit_list_->addItem(line);
    }
    if (events.isEmpty())
        sec_audit_list_->addItem(tr("(no events recorded yet)"));
}

void SecuritySection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void SecuritySection::retranslateUi() {
    // Section titles.
    if (title_pin_)
        title_pin_->setText(tr("PIN AUTHENTICATION"));
    if (title_change_)
        title_change_->setText(tr("CHANGE PIN"));
    if (title_lock_)
        title_lock_->setText(tr("AUTO-LOCK"));
    if (title_ai_tools_)
        title_ai_tools_->setText(tr("AI TOOL PERMISSIONS"));
    if (title_audit_)
        title_audit_->setText(tr("AUDIT LOG"));
    if (audit_note_)
        audit_note_->setText(tr("Recent security events (PIN setup, failed unlocks, inactivity locks)."));

    // Row labels + descriptions.
    if (row_pin_status_lbl_)
        row_pin_status_lbl_->setText(tr("PIN Status"));
    if (row_pin_status_desc_)
        row_pin_status_desc_->setText(tr("A 6-digit PIN is required to unlock the terminal."));
    if (row_attempts_lbl_)
        row_attempts_lbl_->setText(tr("Failed Attempts"));
    if (row_attempts_desc_)
        row_attempts_desc_->setText(tr("PIN lockout engages after 5 consecutive failures."));
    if (row_current_lbl_)
        row_current_lbl_->setText(tr("Current PIN"));
    if (row_new_lbl_)
        row_new_lbl_->setText(tr("New PIN"));
    if (row_confirm_lbl_)
        row_confirm_lbl_->setText(tr("Confirm PIN"));
    if (row_autolock_lbl_)
        row_autolock_lbl_->setText(tr("Auto-Lock"));
    if (row_autolock_desc_)
        row_autolock_desc_->setText(tr("Locks the terminal after a period of inactivity."));
    if (row_timeout_lbl_)
        row_timeout_lbl_->setText(tr("Lock Timeout"));
    if (row_timeout_desc_)
        row_timeout_desc_->setText(tr("Time of inactivity before the terminal locks."));
    if (row_minimize_lbl_)
        row_minimize_lbl_->setText(tr("Lock on Minimize"));
    if (row_minimize_desc_)
        row_minimize_desc_->setText(tr("When on, minimizing the terminal immediately shows the PIN screen."));
    if (row_destructive_lbl_)
        row_destructive_lbl_->setText(tr("Destructive AI Tools"));
    if (row_destructive_desc_)
        row_destructive_desc_->setText(destructive_tools_desc());

    // Checkbox texts.
    if (sec_autolock_toggle_)
        sec_autolock_toggle_->setText(tr("Enable auto-lock on inactivity"));
    if (sec_lock_on_minimize_)
        sec_lock_on_minimize_->setText(tr("Lock when the window is minimized"));
    if (sec_allow_destructive_) {
        sec_allow_destructive_->setText(tr("Allow AI agents to modify files, workspaces and data"));
        sec_allow_destructive_->setAccessibleName(tr("Allow AI agents to modify files, workspaces and data"));
    }

    // PIN field placeholders.
    if (sec_current_pin_)
        sec_current_pin_->setPlaceholderText(tr("Current PIN"));
    if (sec_new_pin_)
        sec_new_pin_->setPlaceholderText(tr("New PIN"));
    if (sec_confirm_pin_)
        sec_confirm_pin_->setPlaceholderText(tr("Confirm PIN"));

    // Buttons. The change-PIN button toggles label with form visibility.
    if (sec_change_pin_btn_)
        sec_change_pin_btn_->setText(sec_change_pin_form_ && sec_change_pin_form_->isVisible() ? tr("Cancel")
                                                                                               : tr("Change PIN"));
    if (save_pin_btn_)
        save_pin_btn_->setText(tr("Update PIN"));
    if (save_btn_)
        save_btn_->setText(tr("Save Security Settings"));
    if (refresh_audit_btn_)
        refresh_audit_btn_->setText(tr("Refresh"));

    // Lock-timeout combo items are fixed UI labels (data = minutes). Re-apply
    // text per index without disturbing the selection.
    if (sec_lock_timeout_ && sec_lock_timeout_->count() >= 7) {
        const QSignalBlocker b(sec_lock_timeout_);
        sec_lock_timeout_->setItemText(0, tr("1 min"));
        sec_lock_timeout_->setItemText(1, tr("2 min"));
        sec_lock_timeout_->setItemText(2, tr("5 min"));
        sec_lock_timeout_->setItemText(3, tr("10 min"));
        sec_lock_timeout_->setItemText(4, tr("15 min"));
        sec_lock_timeout_->setItemText(5, tr("30 min"));
        sec_lock_timeout_->setItemText(6, tr("60 min"));
    }

    // State-dependent text (PIN status, lockout counter, audit log) is re-derived
    // by reload() in the current language.
    reload();
}

void SecuritySection::reload() {
    auto& pm = auth::PinManager::instance();
    auto& repo = SettingsRepository::instance();

    if (sec_pin_status_) {
        sec_pin_status_->setText(pm.has_pin() ? tr("CONFIGURED") : tr("NOT SET"));
        sec_pin_status_->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;")
                                           .arg(pm.has_pin() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    }

    if (sec_lockout_status_) {
        int attempts = pm.failed_attempts();
        sec_lockout_status_->setText(QString("%1 / 5").arg(attempts));
        sec_lockout_status_->setStyleSheet(
            QString("color:%1;font-weight:700;background:transparent;")
                .arg(attempts > 0 ? ui::colors::WARNING() : ui::colors::TEXT_SECONDARY()));
    }

    if (sec_change_pin_btn_)
        sec_change_pin_btn_->setEnabled(pm.has_pin());

    // A read error is not "unset". The Save handler writes every widget on this
    // page straight back, so a failed read must disarm Save rather than let a
    // default (10 min / lock-on-minimize off) overwrite the real setting.
    bool read_failed = false;
    auto log_read_error = [&read_failed](const char* key, const std::string& err) {
        LOG_ERROR("Settings", QString("settings read failed for '%1' — leaving the stored value untouched and "
                                      "disabling Save: %2")
                                  .arg(QString::fromLatin1(key), QString::fromStdString(err)));
        read_failed = true;
    };

    if (sec_autolock_toggle_ && sec_lock_timeout_) {
        const QSignalBlocker b1(sec_autolock_toggle_);
        const QSignalBlocker b2(sec_lock_timeout_);

        auto r_enabled = repo.get("security.autolock_enabled");
        if (r_enabled.is_err()) {
            log_read_error("security.autolock_enabled", r_enabled.error());
        } else {
            bool enabled = r_enabled.value() != "false";
            sec_autolock_toggle_->setChecked(enabled);
            sec_lock_timeout_->setEnabled(enabled);
        }

        auto r_timeout = repo.get("security.lock_timeout_minutes");
        if (r_timeout.is_err()) {
            log_read_error("security.lock_timeout_minutes", r_timeout.error());
        } else {
            int minutes = r_timeout.value().toInt();
            for (int i = 0; i < sec_lock_timeout_->count(); ++i) {
                if (sec_lock_timeout_->itemData(i).toInt() == minutes) {
                    sec_lock_timeout_->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    if (sec_lock_on_minimize_) {
        const QSignalBlocker b(sec_lock_on_minimize_);
        auto r = repo.get("security.lock_on_minimize");
        if (r.is_err())
            log_read_error("security.lock_on_minimize", r.error());
        else
            sec_lock_on_minimize_->setChecked(r.value() == "true");
    }

    setProperty(kSecurityReadFailedProp, read_failed);
    if (save_btn_)
        save_btn_->setEnabled(!read_failed);
    if (read_failed && save_status_) {
        save_status_->setText(tr("Could not read your saved security settings — saving is disabled so the values "
                                 "shown cannot overwrite them. Reopen Settings to retry."));
        save_status_->show();
    }

    if (sec_allow_destructive_) {
        // Read back through McpProvider rather than the raw setting key: it is
        // the owner of `mcp/allow_destructive_tools` and of the session grant
        // seeded from it, so this can never show a state the tool gate does not
        // actually apply. (Its per-call agent token path is thread-local and
        // always false here on the GUI thread.)
        const QSignalBlocker b(sec_allow_destructive_);
        sec_allow_destructive_->setChecked(mcp::McpProvider::destructive_allowed());
    }

    refresh_audit_log();
}

} // namespace fincept::screens
