#include "screens/auth/LockScreen.h"

#include "auth/PinManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LanguageSwitcher.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Design System Styles ────────────────────────────────────────────
// Identical palette to LoginScreen for visual consistency.

static QString card_style() {
    return QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
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
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
             ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT());
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
        .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE(), ui::colors::TEXT_DIM(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM());
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
        .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_DIM(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM());
}

static QString label_style() {
    return QString("color: %1; font-size: 13px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString muted_style() {
    return QString("color: %1; font-size: 13px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY());
}

static QString error_style() {
    return QString("color: %1; font-size: 13px;"
                   "background: rgba(220,38,38,0.08);"
                   "border: 1px solid #7f1d1d; padding: 6px 8px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE());
}

// Hardens a PIN QLineEdit against input-method leakage, predictive text, and
// paste-based brute-force scripts. Called on every PIN entry field in this
// screen.
static void harden_pin_input(QLineEdit* edit) {
    if (!edit) return;

    edit->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhSensitiveData |
                              Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase |
                              Qt::ImhHiddenText);

    auto* validator = new QIntValidator(0, 999999, edit);
    edit->setValidator(validator);

    edit->setDragEnabled(false);
    edit->setAcceptDrops(false);

    edit->setContextMenuPolicy(Qt::NoContextMenu);

    QObject::connect(edit, &QLineEdit::textChanged, edit, [edit](const QString& txt) {
        QString cleaned;
        cleaned.reserve(txt.size());
        for (const QChar& c : txt) if (c.isDigit()) cleaned.append(c);
        if (cleaned != txt) {
            QSignalBlocker b(edit);
            edit->setText(cleaned);
        }
    });
}

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

// ── Constructor ──────────────────────────────────────────────────────────────

LockScreen::LockScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Top-right language picker — available on the lock screen too so a user
    // returning to a non-English locale can switch before unlocking.
    auto* top_row = new QHBoxLayout;
    top_row->setContentsMargins(20, 20, 20, 0);
    top_row->addStretch();
    top_row->addWidget(new ui::LanguageSwitcher(this));
    root->addLayout(top_row);

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

    retranslateUi();

    // Lockout countdown timer — P3 compliant: only runs when visible
    lockout_timer_ = new QTimer(this);
    lockout_timer_->setInterval(1000);
    connect(lockout_timer_, &QTimer::timeout, this, &LockScreen::update_lockout_display);

    auto& pm = auth::PinManager::instance();
    connect(&pm, &auth::PinManager::lockout_changed, this, [this](bool locked, int /*secs*/) {
        if (locked)
            update_lockout_display();
    });
    connect(&pm, &auth::PinManager::max_attempts_exceeded, this, [this]() { show_lockout(); });
}

// ── Background Paint ────────────────────────────────────────────────────────

void LockScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(ui::colors::BG_BASE()));

    p.setPen(QPen(QColor(ui::colors::BG_RAISED()), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);

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
    if (setup_pin_input_) setup_pin_input_->clear();
    if (setup_confirm_input_) setup_confirm_input_->clear();
    if (unlock_pin_input_) unlock_pin_input_->clear();
}

void LockScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        // Re-render the dynamic countdown + attempts text in the new language.
        if (unlock_lockout_label_ && unlock_lockout_label_->isVisible())
            update_lockout_display();
        refresh_attempts_label();
    }
    QWidget::changeEvent(event);
}

// ── PIN Setup Page ──────────────────────────────────────────────────────────

void LockScreen::build_setup_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    setup_title_ = new QLabel;
    setup_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 1px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::AMBER()));
    hl->addWidget(setup_title_);
    hl->addStretch();

    setup_badge_ = new QLabel;
    setup_badge_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 0.5px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::NEGATIVE()));
    hl->addWidget(setup_badge_);
    vl->addWidget(header);

    setup_subtitle_ = new QLabel;
    setup_subtitle_->setWordWrap(true);
    setup_subtitle_->setStyleSheet(muted_style());
    vl->addWidget(setup_subtitle_);

    setup_info_ = new QLabel;
    setup_info_->setWordWrap(true);
    setup_info_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;"
                                       "font-family: 'Consolas','Courier New',monospace;")
                                   .arg(ui::colors::TEXT_DIM()));
    vl->addWidget(setup_info_);

    vl->addWidget(make_separator());

    setup_pin_lbl_ = new QLabel;
    setup_pin_lbl_->setStyleSheet(label_style());
    vl->addWidget(setup_pin_lbl_);

    setup_pin_input_ = new QLineEdit;
    setup_pin_input_->setPlaceholderText(QStringLiteral("------"));
    setup_pin_input_->setMaxLength(6);
    setup_pin_input_->setAlignment(Qt::AlignCenter);
    setup_pin_input_->setEchoMode(QLineEdit::Password);
    setup_pin_input_->setFixedHeight(44);
    setup_pin_input_->setStyleSheet(pin_input_style());
    harden_pin_input(setup_pin_input_);
    vl->addWidget(setup_pin_input_);

    setup_confirm_lbl_ = new QLabel;
    setup_confirm_lbl_->setStyleSheet(label_style());
    vl->addWidget(setup_confirm_lbl_);

    setup_confirm_input_ = new QLineEdit;
    setup_confirm_input_->setPlaceholderText(QStringLiteral("------"));
    setup_confirm_input_->setMaxLength(6);
    setup_confirm_input_->setAlignment(Qt::AlignCenter);
    setup_confirm_input_->setEchoMode(QLineEdit::Password);
    setup_confirm_input_->setFixedHeight(44);
    setup_confirm_input_->setStyleSheet(pin_input_style());
    harden_pin_input(setup_confirm_input_);
    vl->addWidget(setup_confirm_input_);

    setup_error_ = new QLabel;
    setup_error_->setWordWrap(true);
    setup_error_->setStyleSheet(error_style());
    setup_error_->hide();
    vl->addWidget(setup_error_);

    vl->addWidget(make_separator());

    setup_btn_ = new QPushButton;
    setup_btn_->setFixedHeight(34);
    setup_btn_->setStyleSheet(btn_primary());
    connect(setup_btn_, &QPushButton::clicked, this, &LockScreen::on_setup_submit);
    vl->addWidget(setup_btn_);

    setup_note_ = new QLabel;
    setup_note_->setWordWrap(true);
    setup_note_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                       "font-family: 'Consolas','Courier New',monospace;")
                                   .arg(ui::colors::TEXT_DIM()));
    setup_note_->setAlignment(Qt::AlignCenter);
    vl->addWidget(setup_note_);

    vl->addStretch();

    connect(setup_pin_input_, &QLineEdit::returnPressed, this, [this]() { setup_confirm_input_->setFocus(); });
    connect(setup_confirm_input_, &QLineEdit::returnPressed, this, &LockScreen::on_setup_submit);

    pages_->addWidget(page); // index 0
}

// ── PIN Unlock Page ─────────────────────────────────────────────────────────

void LockScreen::build_unlock_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    unlock_title_ = new QLabel;
    unlock_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                         "background: transparent; letter-spacing: 1px;"
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::AMBER()));
    hl->addWidget(unlock_title_);
    hl->addStretch();

    unlock_badge_ = new QLabel;
    unlock_badge_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                         "background: transparent; letter-spacing: 0.5px;"
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::POSITIVE()));
    hl->addWidget(unlock_badge_);
    vl->addWidget(header);

    unlock_subtitle_ = new QLabel;
    unlock_subtitle_->setStyleSheet(muted_style());
    vl->addWidget(unlock_subtitle_);

    vl->addWidget(make_separator());

    unlock_pin_lbl_ = new QLabel;
    unlock_pin_lbl_->setStyleSheet(label_style());
    vl->addWidget(unlock_pin_lbl_);

    unlock_pin_input_ = new QLineEdit;
    unlock_pin_input_->setPlaceholderText(QStringLiteral("------"));
    unlock_pin_input_->setMaxLength(6);
    unlock_pin_input_->setAlignment(Qt::AlignCenter);
    unlock_pin_input_->setEchoMode(QLineEdit::Password);
    unlock_pin_input_->setFixedHeight(48);
    unlock_pin_input_->setStyleSheet(pin_input_style());
    harden_pin_input(unlock_pin_input_);
    vl->addWidget(unlock_pin_input_);

    unlock_lockout_label_ = new QLabel;
    unlock_lockout_label_->setWordWrap(true);
    unlock_lockout_label_->setStyleSheet(QString("color: %1; font-size: 12px;"
                                                 "background: rgba(202,138,4,0.08);"
                                                 "border: 1px solid #713f12; padding: 6px 8px;"
                                                 "font-family: 'Consolas','Courier New',monospace;")
                                             .arg(ui::colors::WARNING()));
    unlock_lockout_label_->hide();
    vl->addWidget(unlock_lockout_label_);

    unlock_error_ = new QLabel;
    unlock_error_->setWordWrap(true);
    unlock_error_->setStyleSheet(error_style());
    unlock_error_->hide();
    vl->addWidget(unlock_error_);

    unlock_attempts_ = new QLabel;
    unlock_attempts_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;"
                                            "font-family: 'Consolas','Courier New',monospace;")
                                        .arg(ui::colors::TEXT_DIM()));
    unlock_attempts_->setAlignment(Qt::AlignCenter);
    unlock_attempts_->hide();
    vl->addWidget(unlock_attempts_);

    vl->addWidget(make_separator());

    unlock_btn_ = new QPushButton;
    unlock_btn_->setFixedHeight(34);
    unlock_btn_->setStyleSheet(btn_primary());
    connect(unlock_btn_, &QPushButton::clicked, this, &LockScreen::on_unlock_submit);
    vl->addWidget(unlock_btn_);

    // "Forgot PIN?" escape — a forgotten PIN is recoverable by signing in again
    // (this clears the PIN). Without this link the only path was to fail the unlock
    // kMaxAttempts times through escalating lockouts, which is minutes of dead-end.
    auto* forgot_link = new QPushButton(tr("Forgot PIN?  Sign in again"));
    forgot_link->setCursor(Qt::PointingHandCursor);
    forgot_link->setStyleSheet(QString("QPushButton{color:%1; background:transparent; border:none;"
                                       "font-size:12px; text-decoration:underline;"
                                       "font-family:'Consolas','Courier New',monospace;}"
                                       "QPushButton:hover{color:%2;}")
                                   .arg(ui::colors::TEXT_DIM(), ui::colors::AMBER()));
    connect(forgot_link, &QPushButton::clicked, this, [this]() { emit reauth_requested(); });
    vl->addWidget(forgot_link, 0, Qt::AlignHCenter);

    vl->addStretch();

    connect(unlock_pin_input_, &QLineEdit::returnPressed, this, &LockScreen::on_unlock_submit);

    pages_->addWidget(page); // index 1
}

// ── Lockout Page (max attempts exceeded) ────────────────────────────────────

void LockScreen::build_lockout_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    lockout_title_ = new QLabel;
    lockout_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 1px;"
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::NEGATIVE()));
    hl->addWidget(lockout_title_);
    hl->addStretch();

    lockout_badge_ = new QLabel;
    lockout_badge_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 0.5px;"
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::NEGATIVE()));
    hl->addWidget(lockout_badge_);
    vl->addWidget(header);

    lockout_msg_ = new QLabel;
    lockout_msg_->setWordWrap(true);
    lockout_msg_->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(lockout_msg_);

    vl->addWidget(make_separator());

    lockout_reauth_btn_ = new QPushButton;
    lockout_reauth_btn_->setFixedHeight(34);
    lockout_reauth_btn_->setStyleSheet(btn_danger());
    connect(lockout_reauth_btn_, &QPushButton::clicked, this, [this]() { emit reauth_requested(); });
    vl->addWidget(lockout_reauth_btn_);

    vl->addStretch();

    pages_->addWidget(page); // index 2
}

// ── Re-translation ───────────────────────────────────────────────────────────

void LockScreen::retranslateUi() {
    if (setup_title_)     setup_title_->setText(tr("SECURITY SETUP"));
    if (setup_badge_)     setup_badge_->setText(tr("REQUIRED"));
    if (setup_subtitle_)  setup_subtitle_->setText(tr("Create a 6-digit PIN to secure your terminal"));
    if (setup_info_)      setup_info_->setText(tr("This PIN will be required each time you open\nthe terminal or after a period of inactivity."));
    if (setup_pin_lbl_)   setup_pin_lbl_->setText(tr("ENTER PIN"));
    if (setup_confirm_lbl_) setup_confirm_lbl_->setText(tr("CONFIRM PIN"));
    if (setup_btn_)       setup_btn_->setText(tr("  SET PIN  "));
    if (setup_note_)      setup_note_->setText(tr("PIN is encrypted and stored locally on this device.\nIt cannot be recovered if forgotten."));

    if (unlock_title_)    unlock_title_->setText(tr("TERMINAL LOCKED"));
    if (unlock_badge_)    unlock_badge_->setText(tr("SECURE"));
    if (unlock_subtitle_) unlock_subtitle_->setText(tr("Enter your 6-digit PIN to unlock"));
    if (unlock_pin_lbl_)  unlock_pin_lbl_->setText(tr("PIN"));
    if (unlock_btn_)      unlock_btn_->setText(tr("  UNLOCK  "));

    if (lockout_title_)   lockout_title_->setText(tr("ACCOUNT LOCKED"));
    if (lockout_badge_)   lockout_badge_->setText(tr("SECURITY"));
    if (lockout_msg_)
        lockout_msg_->setText(tr("Too many failed PIN attempts.\n\n"
                                 "For your security, the terminal has been locked.\n"
                                 "You must sign in again with your email and password\n"
                                 "to reset your PIN and regain access."));
    if (lockout_reauth_btn_) lockout_reauth_btn_->setText(tr("  SIGN IN AGAIN  "));
}

void LockScreen::refresh_attempts_label() {
    if (!unlock_attempts_) return;
    auto& pm = auth::PinManager::instance();
    if (pm.failed_attempts() > 0 && pm.failed_attempts() < auth::PinManager::kMaxAttempts) {
        const int remaining = auth::PinManager::kMaxAttempts - pm.failed_attempts();
        // Qt plural form — translators define numerusform variants in the .ts.
        unlock_attempts_->setText(tr("%n ATTEMPT(S) REMAINING", "", remaining));
    }
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
        refresh_attempts_label();
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
    const QString pin = setup_pin_input_->text();
    const QString confirm = setup_confirm_input_->text();

    // Clear BOTH fields on mismatch — see header note about masked-text leak.
    if (pin != confirm) {
        setup_error_->setText(tr("PINs do not match"));
        setup_error_->show();
        setup_pin_input_->clear();
        setup_confirm_input_->clear();
        setup_pin_input_->setFocus();
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
        unlock_error_->setText(tr("Enter your PIN"));
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

    refresh_attempts_label();
    unlock_attempts_->show();

    if (pm.is_locked_out()) {
        unlock_error_->hide();
        update_lockout_display();
        lockout_timer_->start();
    } else {
        unlock_error_->setText(tr("Incorrect PIN"));
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

    unlock_lockout_label_->setText(tr("Locked for %1 — too many failed attempts").arg(time_str));
    unlock_lockout_label_->show();
    unlock_btn_->setEnabled(false);
    unlock_pin_input_->setEnabled(false);
}

} // namespace fincept::screens
