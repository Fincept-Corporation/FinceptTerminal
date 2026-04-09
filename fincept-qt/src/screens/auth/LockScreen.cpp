#include "screens/auth/LockScreen.h"

#include "auth/PinManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Design System Styles ────────────────────────────────────────────
// Identical palette to LoginScreen for visual consistency.

static QString card_style() {
    return QString("background: %1; border: 1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM);
}

static QString input_style() {
    return QString("QLineEdit {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  padding: 6px 10px; font-size: 15px;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "  selection-background-color: %4; selection-color: %5;"
                   "}"
                   "QLineEdit:focus { border: 1px solid %6; }"
                   "QLineEdit::placeholder { color: %7; }")
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
             ui::colors::AMBER, ui::colors::BG_BASE,
             ui::colors::BORDER_BRIGHT, ui::colors::TEXT_DIM);
}

static QString pin_input_style() {
    return QString("QLineEdit {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  font-size: 24px; letter-spacing: 12px;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "  selection-background-color: %4; selection-color: %5;"
                   "}"
                   "QLineEdit:focus { border: 1px solid %6; }")
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
             ui::colors::AMBER, ui::colors::BG_BASE,
             ui::colors::BORDER_BRIGHT);
}

static QString btn_primary() {
    return QString("QPushButton {"
                   "  background: rgba(217,119,6,0.1); color: %1;"
                   "  border: 1px solid %2;"
                   "  padding: 0 16px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { color: %4; background: %5; border-color: %6; }")
        .arg(ui::colors::AMBER, ui::colors::AMBER_DIM, ui::colors::BG_BASE,
             ui::colors::TEXT_DIM, ui::colors::BG_RAISED, ui::colors::BORDER_DIM);
}

static QString btn_danger() {
    return QString("QPushButton {"
                   "  background: rgba(220,38,38,0.1); color: %1;"
                   "  border: 1px solid #7f1d1d;"
                   "  padding: 0 16px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { background: %1; color: %2; }"
                   "QPushButton:disabled { color: %3; background: %4; border-color: %5; }")
        .arg(ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY,
             ui::colors::TEXT_DIM, ui::colors::BG_RAISED, ui::colors::BORDER_DIM);
}

static QString label_style() {
    return QString("color: %1; font-size: 13px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY);
}

static QString muted_style() {
    return QString("color: %1; font-size: 13px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY);
}

static QString error_style() {
    return QString("color: %1; font-size: 13px;"
                   "background: rgba(220,38,38,0.08);"
                   "border: 1px solid #7f1d1d; padding: 6px 8px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE);
}

static QString success_style() {
    return QString("color: %1; font-size: 13px;"
                   "background: rgba(22,163,74,0.08);"
                   "border: 1px solid #14532d; padding: 6px 8px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::POSITIVE);
}

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM));
    return sep;
}

// ── Constructor ──────────────────────────────────────────────────────────────

LockScreen::LockScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* overlay = new QVBoxLayout;
    overlay->setAlignment(Qt::AlignCenter);
    root->addLayout(overlay, 1);

    pages_ = new QStackedWidget;
    pages_->setMinimumWidth(420);
    pages_->setMaximumWidth(520);
    pages_->setStyleSheet("background: transparent;");

    build_setup_page();
    build_unlock_page();
    build_lockout_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

    // Lockout countdown timer — P3 compliant: only runs when visible
    lockout_timer_ = new QTimer(this);
    lockout_timer_->setInterval(1000);
    connect(lockout_timer_, &QTimer::timeout, this, &LockScreen::update_lockout_display);

    // Listen for PinManager lockout signals
    auto& pm = auth::PinManager::instance();
    connect(&pm, &auth::PinManager::lockout_changed, this, [this](bool locked, int /*secs*/) {
        if (locked)
            update_lockout_display();
    });
    connect(&pm, &auth::PinManager::max_attempts_exceeded, this, [this]() {
        show_lockout();
    });
}

// ── Background Paint (matches LoginScreen grid pattern) ─────────────────────

void LockScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(ui::colors::BG_BASE()));

    p.setPen(QPen(QColor(ui::colors::BG_RAISED()), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);

    // Subtle crosshair at centre
    int cx = width() / 2;
    int cy = height() / 2;
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    p.drawLine(cx - 60, cy, cx + 60, cy);
    p.drawLine(cx, cy - 60, cx, cy + 60);
}

void LockScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (lockout_timer_ && auth::PinManager::instance().is_locked_out())
        lockout_timer_->start();
}

void LockScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (lockout_timer_)
        lockout_timer_->stop();
}

// ── PIN Setup Page ──────────────────────────────────────────────────────────

void LockScreen::build_setup_page() {
    auto* page = new QWidget;
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    // Header bar
    auto* header = new QWidget;
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("SECURITY SETUP");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                         "background: transparent; letter-spacing: 1px;"
                         "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    auto* badge = new QLabel("REQUIRED");
    badge->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                         "background: transparent; letter-spacing: 0.5px;"
                         "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE));
    hl->addWidget(badge);
    vl->addWidget(header);

    auto* subtitle = new QLabel("Create a 6-digit PIN to secure your terminal");
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(muted_style());
    vl->addWidget(subtitle);

    auto* info = new QLabel("This PIN will be required each time you open\n"
                            "the terminal or after a period of inactivity.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;"
                        "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_DIM));
    vl->addWidget(info);

    vl->addWidget(make_separator());

    // PIN input
    auto* pin_lbl = new QLabel("ENTER PIN");
    pin_lbl->setStyleSheet(label_style());
    vl->addWidget(pin_lbl);

    setup_pin_input_ = new QLineEdit;
    setup_pin_input_->setPlaceholderText("------");
    setup_pin_input_->setMaxLength(6);
    setup_pin_input_->setAlignment(Qt::AlignCenter);
    setup_pin_input_->setEchoMode(QLineEdit::Password);
    setup_pin_input_->setFixedHeight(44);
    setup_pin_input_->setStyleSheet(pin_input_style());
    vl->addWidget(setup_pin_input_);

    // Confirm PIN input
    auto* confirm_lbl = new QLabel("CONFIRM PIN");
    confirm_lbl->setStyleSheet(label_style());
    vl->addWidget(confirm_lbl);

    setup_confirm_input_ = new QLineEdit;
    setup_confirm_input_->setPlaceholderText("------");
    setup_confirm_input_->setMaxLength(6);
    setup_confirm_input_->setAlignment(Qt::AlignCenter);
    setup_confirm_input_->setEchoMode(QLineEdit::Password);
    setup_confirm_input_->setFixedHeight(44);
    setup_confirm_input_->setStyleSheet(pin_input_style());
    vl->addWidget(setup_confirm_input_);

    // Error label
    setup_error_ = new QLabel;
    setup_error_->setWordWrap(true);
    setup_error_->setStyleSheet(error_style());
    setup_error_->hide();
    vl->addWidget(setup_error_);

    vl->addWidget(make_separator());

    // Submit button
    setup_btn_ = new QPushButton("  SET PIN  ");
    setup_btn_->setFixedHeight(34);
    setup_btn_->setStyleSheet(btn_primary());
    connect(setup_btn_, &QPushButton::clicked, this, &LockScreen::on_setup_submit);
    vl->addWidget(setup_btn_);

    // Security note
    auto* note = new QLabel("PIN is encrypted and stored locally on this device.\n"
                            "It cannot be recovered if forgotten.");
    note->setWordWrap(true);
    note->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                        "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_DIM));
    note->setAlignment(Qt::AlignCenter);
    vl->addWidget(note);

    vl->addStretch();

    connect(setup_pin_input_, &QLineEdit::returnPressed, this, [this]() {
        setup_confirm_input_->setFocus();
    });
    connect(setup_confirm_input_, &QLineEdit::returnPressed, this, &LockScreen::on_setup_submit);

    pages_->addWidget(page); // index 0
}

// ── PIN Unlock Page ─────────────────────────────────────────────────────────

void LockScreen::build_unlock_page() {
    auto* page = new QWidget;
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    // Header bar
    auto* header = new QWidget;
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("TERMINAL LOCKED");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                         "background: transparent; letter-spacing: 1px;"
                         "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    auto* secure_badge = new QLabel("SECURE");
    secure_badge->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                "background: transparent; letter-spacing: 0.5px;"
                                "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::POSITIVE));
    hl->addWidget(secure_badge);
    vl->addWidget(header);

    auto* subtitle = new QLabel("Enter your 6-digit PIN to unlock");
    subtitle->setStyleSheet(muted_style());
    vl->addWidget(subtitle);

    vl->addWidget(make_separator());

    // PIN input
    auto* pin_lbl = new QLabel("PIN");
    pin_lbl->setStyleSheet(label_style());
    vl->addWidget(pin_lbl);

    unlock_pin_input_ = new QLineEdit;
    unlock_pin_input_->setPlaceholderText("------");
    unlock_pin_input_->setMaxLength(6);
    unlock_pin_input_->setAlignment(Qt::AlignCenter);
    unlock_pin_input_->setEchoMode(QLineEdit::Password);
    unlock_pin_input_->setFixedHeight(48);
    unlock_pin_input_->setStyleSheet(pin_input_style());
    vl->addWidget(unlock_pin_input_);

    // Lockout / attempt display
    unlock_lockout_label_ = new QLabel;
    unlock_lockout_label_->setWordWrap(true);
    unlock_lockout_label_->setStyleSheet(QString("color: %1; font-size: 12px;"
                                         "background: rgba(202,138,4,0.08);"
                                         "border: 1px solid #713f12; padding: 6px 8px;"
                                         "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::WARNING));
    unlock_lockout_label_->hide();
    vl->addWidget(unlock_lockout_label_);

    // Error label
    unlock_error_ = new QLabel;
    unlock_error_->setWordWrap(true);
    unlock_error_->setStyleSheet(error_style());
    unlock_error_->hide();
    vl->addWidget(unlock_error_);

    // Attempts remaining
    unlock_attempts_ = new QLabel;
    unlock_attempts_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;"
                                    "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_DIM));
    unlock_attempts_->setAlignment(Qt::AlignCenter);
    unlock_attempts_->hide();
    vl->addWidget(unlock_attempts_);

    vl->addWidget(make_separator());

    // Unlock button
    unlock_btn_ = new QPushButton("  UNLOCK  ");
    unlock_btn_->setFixedHeight(34);
    unlock_btn_->setStyleSheet(btn_primary());
    connect(unlock_btn_, &QPushButton::clicked, this, &LockScreen::on_unlock_submit);
    vl->addWidget(unlock_btn_);

    vl->addStretch();

    connect(unlock_pin_input_, &QLineEdit::returnPressed, this, &LockScreen::on_unlock_submit);

    pages_->addWidget(page); // index 1
}

// ── Lockout Page (max attempts exceeded) ────────────────────────────────────

void LockScreen::build_lockout_page() {
    auto* page = new QWidget;
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    // Header bar
    auto* header = new QWidget;
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("ACCOUNT LOCKED");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                         "background: transparent; letter-spacing: 1px;"
                         "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE));
    hl->addWidget(title);
    hl->addStretch();

    auto* warn = new QLabel("SECURITY");
    warn->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                        "background: transparent; letter-spacing: 0.5px;"
                        "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE));
    hl->addWidget(warn);
    vl->addWidget(header);

    lockout_msg_ = new QLabel("Too many failed PIN attempts.\n\n"
                              "For your security, the terminal has been locked.\n"
                              "You must sign in again with your email and password\n"
                              "to reset your PIN and regain access.");
    lockout_msg_->setWordWrap(true);
    lockout_msg_->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent;"
                                "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(lockout_msg_);

    vl->addWidget(make_separator());

    auto* reauth_btn = new QPushButton("  SIGN IN AGAIN  ");
    reauth_btn->setFixedHeight(34);
    reauth_btn->setStyleSheet(btn_danger());
    connect(reauth_btn, &QPushButton::clicked, this, [this]() {
        emit reauth_requested();
    });
    vl->addWidget(reauth_btn);

    vl->addStretch();

    pages_->addWidget(page); // index 2
}

// ── Page activation ─────────────────────────────────────────────────────────

void LockScreen::activate() {
    auto& pm = auth::PinManager::instance();
    if (pm.failed_attempts() >= auth::PinManager::kMaxAttempts) {
        show_lockout();
    } else if (pm.has_pin()) {
        show_unlock();
    } else {
        show_setup();
    }
}

void LockScreen::show_setup() {
    setup_pin_input_->clear();
    setup_confirm_input_->clear();
    setup_error_->hide();
    pages_->setCurrentIndex(0);
    setup_pin_input_->setFocus();
}

void LockScreen::show_unlock() {
    unlock_pin_input_->clear();
    unlock_error_->hide();
    unlock_lockout_label_->hide();
    pages_->setCurrentIndex(1);

    auto& pm = auth::PinManager::instance();
    if (pm.failed_attempts() > 0 && pm.failed_attempts() < auth::PinManager::kMaxAttempts) {
        int remaining = auth::PinManager::kMaxAttempts - pm.failed_attempts();
        unlock_attempts_->setText(QString("%1 ATTEMPT%2 REMAINING")
                                  .arg(remaining).arg(remaining == 1 ? "" : "S"));
        unlock_attempts_->show();
    } else {
        unlock_attempts_->hide();
    }

    update_lockout_display();
    unlock_pin_input_->setFocus();
}

void LockScreen::show_lockout() {
    pages_->setCurrentIndex(2);
}

// ── Actions ─────────────────────────────────────────────────────────────────

void LockScreen::on_setup_submit() {
    QString pin = setup_pin_input_->text();
    QString confirm = setup_confirm_input_->text();

    if (pin.length() != 6) {
        setup_error_->setText("PIN must be exactly 6 digits");
        setup_error_->show();
        return;
    }

    // Check all digits
    for (const QChar& c : pin) {
        if (!c.isDigit()) {
            setup_error_->setText("PIN must contain only digits");
            setup_error_->show();
            return;
        }
    }

    if (pin != confirm) {
        setup_error_->setText("PINs do not match");
        setup_error_->show();
        setup_confirm_input_->clear();
        setup_confirm_input_->setFocus();
        return;
    }

    // Check for trivially weak PINs
    bool all_same = true;
    for (int i = 1; i < pin.length(); ++i) {
        if (pin[i] != pin[0]) { all_same = false; break; }
    }
    if (all_same) {
        setup_error_->setText("PIN is too simple — use unique digits");
        setup_error_->show();
        return;
    }

    // Sequential check (123456, 654321)
    bool sequential_up = true;
    bool sequential_down = true;
    for (int i = 1; i < pin.length(); ++i) {
        if (pin[i].unicode() != pin[i-1].unicode() + 1) sequential_up = false;
        if (pin[i].unicode() != pin[i-1].unicode() - 1) sequential_down = false;
    }
    if (sequential_up || sequential_down) {
        setup_error_->setText("PIN is too simple — avoid sequential digits");
        setup_error_->show();
        return;
    }

    auto result = auth::PinManager::instance().set_pin(pin);
    if (result.is_err()) {
        setup_error_->setText(QString::fromStdString(result.error()));
        setup_error_->show();
        return;
    }

    setup_error_->hide();
    LOG_INFO("Auth", "PIN setup complete — unlocking terminal");
    emit unlocked();
}

void LockScreen::on_unlock_submit() {
    auto& pm = auth::PinManager::instance();

    if (pm.is_locked_out()) {
        update_lockout_display();
        return;
    }

    QString pin = unlock_pin_input_->text();
    if (pin.isEmpty()) {
        unlock_error_->setText("Enter your PIN");
        unlock_error_->show();
        return;
    }

    if (pm.verify_pin(pin)) {
        unlock_error_->hide();
        unlock_lockout_label_->hide();
        unlock_attempts_->hide();
        emit unlocked();
        return;
    }

    // Failed
    unlock_pin_input_->clear();
    unlock_pin_input_->setFocus();

    if (pm.failed_attempts() >= auth::PinManager::kMaxAttempts) {
        // max_attempts_exceeded signal will trigger show_lockout()
        return;
    }

    int remaining = auth::PinManager::kMaxAttempts - pm.failed_attempts();
    unlock_attempts_->setText(QString("%1 ATTEMPT%2 REMAINING")
                              .arg(remaining).arg(remaining == 1 ? "" : "S"));
    unlock_attempts_->show();

    if (pm.is_locked_out()) {
        unlock_error_->hide();
        update_lockout_display();
        lockout_timer_->start();
    } else {
        unlock_error_->setText("Incorrect PIN");
        unlock_error_->show();
        unlock_lockout_label_->hide();
    }
}

void LockScreen::update_lockout_display() {
    auto& pm = auth::PinManager::instance();
    int secs = pm.lockout_remaining_seconds();

    if (secs <= 0) {
        unlock_lockout_label_->hide();
        unlock_btn_->setEnabled(true);
        unlock_pin_input_->setEnabled(true);
        unlock_pin_input_->setFocus();
        lockout_timer_->stop();
        return;
    }

    int mins = secs / 60;
    int remaining_secs = secs % 60;
    QString time_str;
    if (mins > 0)
        time_str = QString("%1m %2s").arg(mins).arg(remaining_secs, 2, 10, QChar('0'));
    else
        time_str = QString("%1s").arg(secs);

    unlock_lockout_label_->setText(QString("Locked for %1 — too many failed attempts").arg(time_str));
    unlock_lockout_label_->show();
    unlock_btn_->setEnabled(false);
    unlock_pin_input_->setEnabled(false);
}

} // namespace fincept::screens
