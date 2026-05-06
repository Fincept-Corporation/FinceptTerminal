// SecuritySection.cpp — PIN management, auto-lock policy, and audit log.

#include "screens/settings/SecuritySection.h"

#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"
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
#include <QVBoxLayout>

namespace fincept::screens {

SecuritySection::SecuritySection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void SecuritySection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
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
    auto* t1 = new QLabel("PIN AUTHENTICATION");
    t1->setStyleSheet(section_title_ss());
    vl->addWidget(t1);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    sec_pin_status_ = new QLabel;
    sec_pin_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    vl->addWidget(make_row("PIN Status", sec_pin_status_, "A 6-digit PIN is required to unlock the terminal."));

    sec_lockout_status_ = new QLabel;
    sec_lockout_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(make_row("Failed Attempts", sec_lockout_status_,
                           "PIN lockout engages after 5 consecutive failures."));

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── CHANGE PIN ────────────────────────────────────────────────────────────
    auto* t2 = new QLabel("CHANGE PIN");
    t2->setStyleSheet(sub_title_ss());
    vl->addWidget(t2);
    vl->addSpacing(4);

    sec_change_pin_btn_ = new QPushButton("Change PIN");
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
        return input;
    };

    sec_current_pin_ = make_pin_field("Current PIN");
    cpfl->addWidget(make_row("Current PIN", sec_current_pin_));

    sec_new_pin_ = make_pin_field("New PIN");
    cpfl->addWidget(make_row("New PIN", sec_new_pin_));

    sec_confirm_pin_ = make_pin_field("Confirm PIN");
    cpfl->addWidget(make_row("Confirm PIN", sec_confirm_pin_));

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

    auto* save_pin_btn = new QPushButton("Update PIN");
    save_pin_btn->setFixedWidth(140);
    save_pin_btn->setStyleSheet(btn_primary_ss());
    cpfl->addWidget(save_pin_btn);

    sec_change_pin_form_->hide();
    vl->addWidget(sec_change_pin_form_);

    connect(sec_change_pin_btn_, &QPushButton::clicked, this, [this]() {
        bool showing = sec_change_pin_form_->isVisible();
        sec_change_pin_form_->setVisible(!showing);
        sec_change_pin_btn_->setText(showing ? "Change PIN" : "Cancel");
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
    connect(save_pin_btn, &QPushButton::clicked, this, [this]() {
        sec_pin_error_->hide();
        sec_pin_success_->hide();

        auto& pm = auth::PinManager::instance();
        const QString current = sec_current_pin_->text();
        const QString new_pin = sec_new_pin_->text();
        const QString confirm = sec_confirm_pin_->text();

        if (pm.is_locked_out()) {
            int secs = pm.lockout_remaining_seconds();
            sec_pin_error_->setText(QString("Locked out — try again in %1s").arg(secs));
            sec_pin_error_->show();
            sec_current_pin_->clear();
            sec_new_pin_->clear();
            sec_confirm_pin_->clear();
            return;
        }

        if (new_pin != confirm) {
            sec_pin_error_->setText("New PINs do not match");
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
                sec_pin_error_->setText(QString("Too many failed attempts — locked for %1s").arg(secs));
            } else {
                sec_pin_error_->setText(QString::fromStdString(result.error()));
            }
            sec_pin_error_->show();
            sec_current_pin_->clear();
            sec_current_pin_->setFocus();
            return;
        }

        sec_pin_success_->setText("PIN updated successfully");
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
    auto* t3 = new QLabel("AUTO-LOCK");
    t3->setStyleSheet(sub_title_ss());
    vl->addWidget(t3);
    vl->addSpacing(4);

    sec_autolock_toggle_ = new QCheckBox("Enable auto-lock on inactivity");
    sec_autolock_toggle_->setChecked(true);
    sec_autolock_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Auto-Lock", sec_autolock_toggle_, "Locks the terminal after a period of inactivity."));

    sec_lock_timeout_ = new QComboBox;
    sec_lock_timeout_->addItem("1 min",  1);
    sec_lock_timeout_->addItem("2 min",  2);
    sec_lock_timeout_->addItem("5 min",  5);
    sec_lock_timeout_->addItem("10 min", 10);
    sec_lock_timeout_->addItem("15 min", 15);
    sec_lock_timeout_->addItem("30 min", 30);
    sec_lock_timeout_->addItem("60 min", 60);
    sec_lock_timeout_->setCurrentIndex(3);
    sec_lock_timeout_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Lock Timeout", sec_lock_timeout_, "Time of inactivity before the terminal locks."));

    connect(sec_autolock_toggle_, &QCheckBox::toggled, this,
            [this](bool checked) { sec_lock_timeout_->setEnabled(checked); });

    sec_lock_on_minimize_ = new QCheckBox("Lock when the window is minimized");
    sec_lock_on_minimize_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Lock on Minimize", sec_lock_on_minimize_,
                           "When on, minimizing the terminal immediately shows the PIN screen."));

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Save Security Settings");
    save_btn->setFixedWidth(200);
    save_btn->setStyleSheet(btn_primary_ss());
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        auto& repo  = SettingsRepository::instance();
        auto& guard = auth::InactivityGuard::instance();
        auto& pm    = auth::PinManager::instance();

        bool autolock = sec_autolock_toggle_->isChecked();
        int  minutes  = sec_lock_timeout_->currentData().toInt();

        repo.set("security.autolock_enabled",     autolock ? "true" : "false", "security");
        repo.set("security.lock_timeout_minutes", QString::number(minutes),    "security");
        if (sec_lock_on_minimize_) {
            repo.set("security.lock_on_minimize",
                     sec_lock_on_minimize_->isChecked() ? "true" : "false", "security");
        }

        // Always update interval; only flip enabled if a PIN is configured
        // (otherwise the timer fires but show_lock_screen suppresses the
        // request, leaving an invisible churning timer).
        guard.set_timeout_minutes(minutes);
        guard.set_enabled(autolock && pm.has_pin());

        LOG_INFO("Settings",
                 QString("Security settings saved: autolock=%1, timeout=%2min, has_pin=%3")
                     .arg(autolock).arg(minutes).arg(pm.has_pin()));
    });
    vl->addWidget(save_btn);

    // ── AUDIT LOG ─────────────────────────────────────────────────────────────
    vl->addSpacing(16);
    auto* t_audit = new QLabel("AUDIT LOG");
    t_audit->setStyleSheet(sub_title_ss());
    vl->addWidget(t_audit);
    vl->addSpacing(4);

    auto* audit_note = new QLabel("Recent security events (PIN setup, failed unlocks, inactivity locks).");
    audit_note->setWordWrap(true);
    audit_note->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;")
                                  .arg(ui::colors::TEXT_DIM()));
    vl->addWidget(audit_note);

    sec_audit_list_ = new QListWidget;
    sec_audit_list_->setFixedHeight(180);
    sec_audit_list_->setStyleSheet(
        QString("QListWidget { background:%1; color:%2; border:1px solid %3;"
                "font-family:'Consolas','Courier New',monospace; font-size:12px; }"
                "QListWidget::item { padding:2px 6px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    vl->addWidget(sec_audit_list_);

    auto* refresh_audit_btn = new QPushButton("Refresh");
    refresh_audit_btn->setFixedWidth(140);
    refresh_audit_btn->setStyleSheet(btn_secondary_ss());
    connect(refresh_audit_btn, &QPushButton::clicked, this, [this]() { refresh_audit_log(); });
    vl->addWidget(refresh_audit_btn);

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

void SecuritySection::refresh_audit_log() {
    if (!sec_audit_list_) return;
    sec_audit_list_->clear();
    const auto events = auth::SecurityAuditLog::instance().recent(100);
    for (const auto& e : events) {
        const QString ts = e.timestamp.toString("yyyy-MM-dd hh:mm:ss");
        const QString line = e.detail.isEmpty()
                                 ? QString("%1  %2").arg(ts, e.event)
                                 : QString("%1  %2  (%3)").arg(ts, e.event, e.detail);
        sec_audit_list_->addItem(line);
    }
    if (events.isEmpty()) sec_audit_list_->addItem("(no events recorded yet)");
}

void SecuritySection::reload() {
    auto& pm   = auth::PinManager::instance();
    auto& repo = SettingsRepository::instance();

    if (sec_pin_status_) {
        sec_pin_status_->setText(pm.has_pin() ? "CONFIGURED" : "NOT SET");
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

    if (sec_change_pin_btn_) sec_change_pin_btn_->setEnabled(pm.has_pin());

    if (sec_autolock_toggle_ && sec_lock_timeout_) {
        const QSignalBlocker b1(sec_autolock_toggle_);
        const QSignalBlocker b2(sec_lock_timeout_);

        auto r_enabled = repo.get("security.autolock_enabled");
        bool enabled = !r_enabled.is_ok() || r_enabled.value() != "false";
        sec_autolock_toggle_->setChecked(enabled);
        sec_lock_timeout_->setEnabled(enabled);

        auto r_timeout = repo.get("security.lock_timeout_minutes");
        int minutes = r_timeout.is_ok() ? r_timeout.value().toInt() : 10;
        for (int i = 0; i < sec_lock_timeout_->count(); ++i) {
            if (sec_lock_timeout_->itemData(i).toInt() == minutes) {
                sec_lock_timeout_->setCurrentIndex(i);
                break;
            }
        }
    }

    if (sec_lock_on_minimize_) {
        const QSignalBlocker b(sec_lock_on_minimize_);
        auto r = repo.get("security.lock_on_minimize");
        sec_lock_on_minimize_->setChecked(r.is_ok() && r.value() == "true");
    }

    refresh_audit_log();
}

} // namespace fincept::screens
