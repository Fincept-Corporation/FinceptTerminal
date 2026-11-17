#include "screens/auth/LockScreen.h"

#include "auth/PinManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/LanguageSwitcher.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/VersionLabel.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

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
                   "  font-size: " + fincept::ui::fonts::px(10) + "px; letter-spacing: 12px;"
                   "  selection-background-color: %4; selection-color: %5;"
                   "}"
                   "QLineEdit:focus { border: 1px solid %6; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
             ui::colors::TEXT_ON_ACCENT(), ui::colors::BORDER_BRIGHT());
}

static QString btn_primary() {
    return QString("QPushButton {"
                   "  background: %7; color: %1;"
                   "  border: 1px solid %2;"
                   "  padding: 0 16px; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700;"
                   "}"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { color: %4; background: %5; border-color: %6; }")
        .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_TERTIARY(),
             ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::ACCENT_BG());
}

static QString btn_danger() {
    return QString("QPushButton {"
                   "  background: %6; color: %1;"
                   "  border: 1px solid %7;"
                   "  padding: 0 16px; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700;"
                   "}"
                   "QPushButton:hover { background: %1; color: %2; }"
                   "QPushButton:disabled { color: %3; background: %4; border-color: %5; }")
        .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM(), ui::colors::NEGATIVE_BG(), ui::colors::NEGATIVE_DIM());
}

static QString label_style() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString muted_style() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; background: transparent;")
        .arg(ui::colors::TEXT_TERTIARY());
}

static QString error_style() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px;"
                   "background: %2;"
                   "border: 1px solid %3; padding: 6px 8px;")
        .arg(ui::colors::NEGATIVE(), ui::colors::NEGATIVE_BG(), ui::colors::NEGATIVE_DIM());
}

// Hardens a PIN QLineEdit against input-method leakage, predictive text, and
// paste-based brute-force scripts. Called on every PIN entry field in this
// screen.
static void harden_pin_input(QLineEdit* edit) {
    if (!edit)
        return;

    edit->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText |
                              Qt::ImhNoAutoUppercase | Qt::ImhHiddenText);

    auto* validator = new QIntValidator(0, 999999, edit);
    edit->setValidator(validator);

    edit->setDragEnabled(false);
    edit->setAcceptDrops(false);

    edit->setContextMenuPolicy(Qt::NoContextMenu);

    QObject::connect(edit, &QLineEdit::textChanged, edit, [edit](const QString& txt) {
        QString cleaned;
        cleaned.reserve(txt.size());
        for (const QChar& c : txt)
            if (c.isDigit())
                cleaned.append(c);
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

// Minimum height of a text-bearing control: one line of its font (`font_offset`
// px from the base size) plus `padding`, never below `min_px` — re-applied when
// the appearance (font size) changes. Fixed pixel heights cramped the card at
// large font sizes.
static void follow_text_min_height(QWidget* w, int font_offset, int padding, int min_px) {
    if (!w)
        return;
    auto apply = [w, font_offset, padding, min_px]() {
        w->setMinimumHeight(std::max(min_px, QFontMetrics(ui::fonts::font(font_offset)).height() + padding));
    };
    apply();
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, w,
                     [apply](const ui::ThemeTokens&) { apply(); });
}

// Card header strip: tall enough for its title at the current font size.
static QWidget* make_card_header(QWidget* parent) {
    auto* header = new QWidget(parent);
    ui::metrics::follow_fixed_height(header, 38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    return header;
}

// ── Constructor ──────────────────────────────────────────────────────────────

LockScreen::LockScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Top-right language picker — available on the lock screen too so a user
    // returning to a non-English locale can switch before unlocking.
    auto* top_row = new QHBoxLayout;
    top_row->setContentsMargins(20, 20, 20, 0);
    top_row->addWidget(ui::make_version_label(this));
    top_row->addStretch();
    top_row->addWidget(new ui::LanguageSwitcher(this));
    root->addLayout(top_row);
    top_row_ = top_row;

    // Body: [stretch] [column: PIN card, Enterprise strip] [Enterprise panel] [stretch].
    // Box-layout spacing only falls between visible widgets, so the gap before
    // the panel disappears with it. fit_layout() picks what is shown.
    auto* body = new QHBoxLayout;
    body->setContentsMargins(20, 0, 20, 0);
    body->setSpacing(ui::metrics::width(36));
    body_ = body;
    root->addLayout(body, 1);

    pages_ = new QStackedWidget;
    pages_->setStyleSheet("background: transparent;");

    build_setup_page();
    build_unlock_page();
    build_lockout_page();

    // The card is as tall as the page it shows. A stacked widget takes the
    // height of its tallest page (PIN set-up, two inputs and the rules), which
    // left the lower half of the unlock card empty.
    const auto fit_card_to_page = [this]() {
        for (int i = 0; i < pages_->count(); ++i) {
            pages_->widget(i)->setSizePolicy(QSizePolicy::Preferred, i == pages_->currentIndex()
                                                                         ? QSizePolicy::Preferred
                                                                         : QSizePolicy::Ignored);
        }
        pages_->updateGeometry();
    };
    connect(pages_, &QStackedWidget::currentChanged, this, fit_card_to_page);
    fit_card_to_page();

    // Enterprise promotion — static copy, no network, no account data; nothing
    // in it can take focus, so the PIN field keeps the keyboard. Both start
    // hidden; fit_layout() shows the one that fits once the screen is laid out.
    promo_panel_ = new ui::EnterpriseLockPromo(ui::EnterpriseLockPromo::Form::Panel, this);
    promo_panel_->hide();
    promo_strip_ = new ui::EnterpriseLockPromo(ui::EnterpriseLockPromo::Form::Strip, this);
    promo_strip_->hide();

    auto* pin_col = new QVBoxLayout;
    pin_col->setContentsMargins(0, 0, 0, 0);
    pin_col->setSpacing(10);
    pin_col->addStretch(1);
    pin_col->addWidget(pages_, 0, Qt::AlignHCenter);
    pin_col->addWidget(promo_strip_); // no alignment: exactly the card's width
    pin_col->addStretch(1);

    body->addStretch(1);
    body->addLayout(pin_col);
    body->addWidget(promo_panel_, 0, Qt::AlignVCenter);
    body->addStretch(1);

    fit_layout(); // 420-520 px card at the default font size, wider with larger fonts
    // Re-fit after every appearance change has settled — it asks for size
    // hints and shows / hides widgets, which a theme_changed handler must not.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_layout(); });
    connect(pages_, &QStackedWidget::currentChanged, this, [this](int) { fit_layout(); });

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

void LockScreen::fit_layout() {
    if (!pages_)
        return;
    // Size hints asked for while the theme handlers run would style the window
    // widget by widget; appearance_settled re-runs this afterwards.
    if (ui::ThemeManager::instance().notifying())
        return;

    // Card: 420-520 px at the default font size, growing with the font so the
    // text in the card is not squeezed into extra lines; never wider than the
    // screen leaves room for (20 px margin each side once it is laid out).
    const bool laid_out = width() > 0 && isVisible();
    const int room = laid_out ? std::max(0, width() - 40) : 0;
    const int card_min = ui::metrics::width(420);
    const int card_max = ui::metrics::width(520);
    const int gap = ui::metrics::width(36);

    // Enterprise promotion: never on the lockout page (its security message
    // stands alone), never before the screen is laid out, and not for an
    // account whose in-memory plan is already Enterprise (nothing is fetched,
    // nothing about the account is shown).
    const int page = pages_->currentIndex();
    const bool promo_allowed = laid_out && page != 2 && promo_panel_ && promo_strip_ &&
                               !ui::enterprise::session_has_enterprise_plan();

    bool wide = false;
    bool strip = false;
    int panel_w = 0;
    if (promo_allowed) {
        // Height left under the top row, with a little air above and below.
        const int top_h = top_row_ ? top_row_->sizeHint().height() : 0;
        const int avail_h = height() - top_h - 24;
        // The card as tall as its page is now, plus room for an error and an
        // attempts line appearing after a wrong PIN.
        const QFontMetrics msg_fm(ui::fonts::font(-1));
        const int slack = 2 * msg_fm.height() + 34;
        const int card_h = pages_->sizeHint().height() + slack;

        // Wide: the panel beside the card. The breakpoint is font metrics —
        // the card's minimum, the gap and the panel's minimum must all fit.
        const int panel_min = ui::EnterpriseLockPromo::panel_min_width();
        if (room >= card_min + gap + panel_min) {
            const int panel_max = ui::EnterpriseLockPromo::panel_max_width();
            // The card keeps its full width first; the panel takes what is left.
            panel_w = std::clamp(room - gap - card_max, panel_min, panel_max);
            promo_panel_->ensurePolished(); // stylesheet font sizes decide the wrapping
            const int panel_h = promo_panel_->hasHeightForWidth() ? promo_panel_->heightForWidth(panel_w)
                                                                  : promo_panel_->sizeHint().height();
            wide = std::max(card_h, panel_h) <= avail_h;
        }
        // Narrow: one slim line under the unlock card — never under the PIN
        // set-up card (its instructions come first), and only when the card
        // still fits above it.
        if (!wide && page == 1) {
            promo_strip_->ensurePolished();
            strip = card_h + 10 + promo_strip_->sizeHint().height() <= avail_h;
        }
    }

    int min_w = card_min;
    int max_w = card_max;
    if (room > 0) {
        const int card_room = wide ? room - gap - panel_w : room;
        min_w = std::min(min_w, card_room);
        max_w = std::max(min_w, std::min(max_w, card_room));
    }
    pages_->setMinimumWidth(min_w);
    pages_->setMaximumWidth(max_w);
    if (body_ && body_->spacing() != gap)
        body_->setSpacing(gap);

    if (promo_panel_) {
        if (wide && (promo_panel_->minimumWidth() != panel_w || promo_panel_->maximumWidth() != panel_w))
            promo_panel_->setFixedWidth(panel_w);
        if (promo_panel_->isVisibleTo(this) != wide)
            promo_panel_->setVisible(wide);
    }
    if (promo_strip_) {
        promo_strip_->setMaximumWidth(max_w);
        if (promo_strip_->isVisibleTo(this) != strip)
            promo_strip_->setVisible(strip);
    }
}

void LockScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    fit_layout();
}

void LockScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    fit_layout();
    if (lockout_timer_ && auth::PinManager::instance().is_locked_out())
        lockout_timer_->start();
}

void LockScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // Taken off screen by the app (a sibling window unlocked, sign-out, window
    // closing) — not a minimise, which is spontaneous. Drop the PIN checks this
    // screen still has queued (nothing counted) and abandon the running one: it
    // is accounted by PinManager, but its result must not unlock from here.
    if (e && !e->spontaneous() && (unlock_busy_ || setup_busy_)) {
        auth::PinManager::instance().cancel_pending(this);
        ++attempt_serial_;
        set_unlock_busy(false);
        set_setup_busy(false);
    }
    if (lockout_timer_)
        lockout_timer_->stop();
    if (setup_pin_input_)
        setup_pin_input_->clear();
    if (setup_confirm_input_)
        setup_confirm_input_->clear();
    if (unlock_pin_input_)
        unlock_pin_input_->clear();
}

void LockScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        // Re-render the dynamic countdown + attempts text in the new language.
        if (unlock_lockout_label_ && unlock_lockout_label_->isVisible())
            update_lockout_display();
        refresh_attempts_label();
        // New texts, new heights: re-fit once every widget has re-translated.
        QTimer::singleShot(0, this, [this]() { fit_layout(); });
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

    auto* header = make_card_header(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    setup_title_ = new QLabel;
    setup_title_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 1px;")
                                    .arg(ui::colors::AMBER()));
    hl->addWidget(setup_title_);
    hl->addStretch();

    setup_badge_ = new QLabel;
    setup_badge_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 0.5px;")
                                    .arg(ui::colors::NEGATIVE()));
    hl->addWidget(setup_badge_);
    vl->addWidget(header);

    setup_subtitle_ = new QLabel;
    setup_subtitle_->setWordWrap(true);
    setup_subtitle_->setStyleSheet(muted_style());
    vl->addWidget(setup_subtitle_);

    setup_info_ = new QLabel;
    setup_info_->setWordWrap(true);
    setup_info_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;")
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
    follow_text_min_height(setup_pin_input_, 10, 16, 44); // the PIN digits are px(10)
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
    follow_text_min_height(setup_confirm_input_, 10, 16, 44);
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
    follow_text_min_height(setup_btn_, 0, 14, 34);
    setup_btn_->setStyleSheet(btn_primary());
    connect(setup_btn_, &QPushButton::clicked, this, &LockScreen::on_setup_submit);
    vl->addWidget(setup_btn_);

    setup_note_ = new QLabel;
    setup_note_->setWordWrap(true);
    setup_note_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;")
                                   .arg(ui::colors::TEXT_DIM()));
    setup_note_->setAlignment(Qt::AlignCenter);
    vl->addWidget(setup_note_);

    vl->addStretch();

    connect(setup_pin_input_, &QLineEdit::returnPressed, this, [this]() { setup_confirm_input_->setFocus(); });
    connect(setup_confirm_input_, &QLineEdit::returnPressed, this, &LockScreen::on_setup_submit);

    setup_pin_input_->setAccessibleName(tr("New PIN"));
    setup_pin_input_->setAccessibleDescription(tr("Six digit PIN used to unlock the terminal"));
    setup_confirm_input_->setAccessibleName(tr("Confirm new PIN"));
    setup_btn_->setAccessibleName(tr("Set PIN"));
    setup_error_->setAccessibleName(tr("PIN setup error"));
    setTabOrder(setup_pin_input_, setup_confirm_input_);
    setTabOrder(setup_confirm_input_, setup_btn_);

    pages_->addWidget(page); // index 0
}

// ── PIN Unlock Page ─────────────────────────────────────────────────────────

void LockScreen::build_unlock_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = make_card_header(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    unlock_title_ = new QLabel;
    unlock_title_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700;"
                                         "background: transparent; letter-spacing: 1px;")
                                     .arg(ui::colors::AMBER()));
    hl->addWidget(unlock_title_);
    hl->addStretch();

    unlock_badge_ = new QLabel;
    unlock_badge_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700;"
                                         "background: transparent; letter-spacing: 0.5px;")
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
    follow_text_min_height(unlock_pin_input_, 10, 20, 48); // the PIN digits are px(10)
    unlock_pin_input_->setStyleSheet(pin_input_style());
    harden_pin_input(unlock_pin_input_);
    vl->addWidget(unlock_pin_input_);

    unlock_lockout_label_ = new QLabel;
    unlock_lockout_label_->setWordWrap(true);
    // Warning-tinted panel: a faint wash of WARNING behind it, a deeper one as the border.
    unlock_lockout_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px;"
                                                 "background: %2;"
                                                 "border: 1px solid %3; padding: 6px 8px;")
                                             .arg(ui::colors::WARNING(), ui::colors::tint(QColor(ui::colors::WARNING()), 0.10),
                                                  ui::colors::tint(QColor(ui::colors::WARNING()), 0.45)));
    unlock_lockout_label_->hide();
    vl->addWidget(unlock_lockout_label_);

    unlock_error_ = new QLabel;
    unlock_error_->setWordWrap(true);
    unlock_error_->setStyleSheet(error_style());
    unlock_error_->hide();
    vl->addWidget(unlock_error_);

    unlock_attempts_ = new QLabel;
    unlock_attempts_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;")
                                        .arg(ui::colors::TEXT_DIM()));
    unlock_attempts_->setAlignment(Qt::AlignCenter);
    unlock_attempts_->hide();
    vl->addWidget(unlock_attempts_);

    vl->addWidget(make_separator());

    unlock_btn_ = new QPushButton;
    follow_text_min_height(unlock_btn_, 0, 14, 34);
    unlock_btn_->setStyleSheet(btn_primary());
    connect(unlock_btn_, &QPushButton::clicked, this, &LockScreen::on_unlock_submit);
    vl->addWidget(unlock_btn_);

    // "Forgot PIN?" escape — a forgotten PIN is recoverable by signing in again
    // (this clears the PIN). Without this link the only path was to fail the unlock
    // kMaxAttempts times through escalating lockouts, which is minutes of dead-end.
    unlock_forgot_btn_ = new QPushButton(tr("Forgot PIN?  Sign in again"));
    unlock_forgot_btn_->setCursor(Qt::PointingHandCursor);
    unlock_forgot_btn_->setStyleSheet(QString("QPushButton{color:%1; background:transparent; border:none;"
                                              "font-size:" + fincept::ui::fonts::px(-2) + "px; text-decoration:underline;"
                                              "}"
                                              "QPushButton:hover{color:%2;}")
                                          .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    connect(unlock_forgot_btn_, &QPushButton::clicked, this, [this]() { emit reauth_requested(); });
    vl->addWidget(unlock_forgot_btn_, 0, Qt::AlignHCenter);

    vl->addStretch();

    connect(unlock_pin_input_, &QLineEdit::returnPressed, this, &LockScreen::on_unlock_submit);

    unlock_pin_input_->setAccessibleName(tr("Unlock PIN"));
    unlock_pin_input_->setAccessibleDescription(tr("Six digit PIN that unlocks the terminal"));
    unlock_btn_->setAccessibleName(tr("Unlock terminal"));
    unlock_forgot_btn_->setAccessibleName(tr("Forgot PIN, sign in again"));
    unlock_error_->setAccessibleName(tr("Unlock error"));
    unlock_lockout_label_->setAccessibleName(tr("Lockout status"));
    setTabOrder(unlock_pin_input_, unlock_btn_);
    setTabOrder(unlock_btn_, unlock_forgot_btn_);

    pages_->addWidget(page); // index 1
}

// ── Lockout Page (max attempts exceeded) ────────────────────────────────────

void LockScreen::build_lockout_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = make_card_header(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    lockout_title_ = new QLabel;
    lockout_title_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 1px;")
                                      .arg(ui::colors::NEGATIVE()));
    hl->addWidget(lockout_title_);
    hl->addStretch();

    lockout_badge_ = new QLabel;
    lockout_badge_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 0.5px;")
                                      .arg(ui::colors::NEGATIVE()));
    hl->addWidget(lockout_badge_);
    vl->addWidget(header);

    lockout_msg_ = new QLabel;
    lockout_msg_->setWordWrap(true);
    lockout_msg_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; background: transparent;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(lockout_msg_);

    vl->addWidget(make_separator());

    lockout_reauth_btn_ = new QPushButton;
    follow_text_min_height(lockout_reauth_btn_, 0, 14, 34);
    lockout_reauth_btn_->setStyleSheet(btn_danger());
    connect(lockout_reauth_btn_, &QPushButton::clicked, this, [this]() { emit reauth_requested(); });
    vl->addWidget(lockout_reauth_btn_);

    vl->addStretch();

    lockout_reauth_btn_->setAccessibleName(tr("Sign in again to reset your PIN"));
    lockout_msg_->setAccessibleName(tr("Account locked message"));

    pages_->addWidget(page); // index 2
}

// ── Re-translation ───────────────────────────────────────────────────────────

void LockScreen::retranslateUi() {
    if (setup_title_)
        setup_title_->setText(tr("SECURITY SETUP"));
    if (setup_badge_)
        setup_badge_->setText(tr("REQUIRED"));
    if (setup_subtitle_)
        setup_subtitle_->setText(tr("Create a 6-digit PIN to secure your terminal"));
    if (setup_info_)
        setup_info_->setText(
            tr("This PIN will be required each time you open\nthe terminal or after a period of inactivity."));
    if (setup_pin_lbl_)
        setup_pin_lbl_->setText(tr("ENTER PIN"));
    if (setup_confirm_lbl_)
        setup_confirm_lbl_->setText(tr("CONFIRM PIN"));
    if (setup_btn_)
        setup_btn_->setText(setup_busy_ ? tr("  SAVING…  ") : tr("  SET PIN  "));
    if (setup_note_)
        setup_note_->setText(
            tr("PIN is encrypted and stored locally on this device.\nIt cannot be recovered if forgotten."));

    if (unlock_title_)
        unlock_title_->setText(tr("TERMINAL LOCKED"));
    if (unlock_badge_)
        unlock_badge_->setText(tr("SECURE"));
    if (unlock_subtitle_)
        unlock_subtitle_->setText(tr("Enter your 6-digit PIN to unlock"));
    if (unlock_pin_lbl_)
        unlock_pin_lbl_->setText(tr("PIN"));
    if (unlock_btn_)
        unlock_btn_->setText(unlock_busy_ ? tr("  VERIFYING…  ") : tr("  UNLOCK  "));
    if (unlock_forgot_btn_)
        unlock_forgot_btn_->setText(tr("Forgot PIN?  Sign in again"));

    if (lockout_title_)
        lockout_title_->setText(tr("ACCOUNT LOCKED"));
    if (lockout_badge_)
        lockout_badge_->setText(tr("SECURITY"));
    if (lockout_msg_)
        lockout_msg_->setText(tr("Too many failed PIN attempts.\n\n"
                                 "For your security, the terminal has been locked.\n"
                                 "You must sign in again with your email and password\n"
                                 "to reset your PIN and regain access."));
    if (lockout_reauth_btn_)
        lockout_reauth_btn_->setText(tr("  SIGN IN AGAIN  "));
}

void LockScreen::refresh_attempts_label() {
    if (!unlock_attempts_)
        return;
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
    if (setup_busy_)
        return; // Enter / click while the PIN is being saved
    const QString pin = setup_pin_input_->text();
    const QString confirm = setup_confirm_input_->text();

    // Length is checked first so an empty/short entry gets a specific message
    // instead of falling through to PinManager's generic rejection (an empty
    // PIN also equals an empty confirmation, so the mismatch check misses it).
    if (pin.length() != 6) {
        setup_error_->setText(tr("PIN must be exactly 6 digits"));
        setup_error_->show();
        setup_pin_input_->setFocus();
        return;
    }

    // Clear BOTH fields on mismatch — see header note about masked-text leak.
    if (pin != confirm) {
        setup_error_->setText(tr("PINs do not match"));
        setup_error_->show();
        setup_pin_input_->clear();
        setup_confirm_input_->clear();
        setup_pin_input_->setFocus();
        return;
    }

    // Hashing the PIN takes a deliberately slow key derivation; it runs off the
    // UI thread and the page shows a busy state until the PIN is stored.
    set_setup_busy(true);
    const quint64 serial = attempt_serial_;
    auth::PinManager::instance().set_pin_async(pin, this, [this, serial](const auto& result) {
        if (serial != attempt_serial_)
            return; // the app took this screen away meanwhile
        set_setup_busy(false);
        if (result.is_err()) {
            setup_error_->setText(QString::fromStdString(result.error()));
            setup_error_->show();
            return;
        }

        setup_error_->hide();
        LOG_INFO("Auth", "PIN setup complete — unlocking terminal");
        emit unlocked();
    });
}

void LockScreen::set_setup_busy(bool busy) {
    setup_busy_ = busy;
    if (setup_btn_) {
        setup_btn_->setText(busy ? tr("  SAVING…  ") : tr("  SET PIN  "));
        setup_btn_->setEnabled(!busy);
    }
    if (setup_pin_input_)
        setup_pin_input_->setEnabled(!busy);
    if (setup_confirm_input_)
        setup_confirm_input_->setEnabled(!busy);
    if (busy && setup_error_)
        setup_error_->hide();
    if (unlock_busy_ || setup_busy_)
        setCursor(Qt::BusyCursor);
    else
        unsetCursor();
}

void LockScreen::set_unlock_busy(bool busy) {
    unlock_busy_ = busy;
    // While busy the button reads VERIFYING… in its (theme) disabled style;
    // afterwards the lockout state decides whether input is accepted.
    const bool locked = auth::PinManager::instance().lockout_remaining_seconds() > 0;
    if (unlock_btn_) {
        unlock_btn_->setText(busy ? tr("  VERIFYING…  ") : tr("  UNLOCK  "));
        unlock_btn_->setEnabled(!busy && !locked);
    }
    if (unlock_pin_input_)
        unlock_pin_input_->setEnabled(!busy && !locked);
    // "Forgot PIN" signs out; it waits for the running check so a late success
    // can never unlock a session that is being torn down.
    if (unlock_forgot_btn_)
        unlock_forgot_btn_->setEnabled(!busy);
    if (busy && unlock_error_)
        unlock_error_->hide();
    if (unlock_busy_ || setup_busy_)
        setCursor(Qt::BusyCursor);
    else
        unsetCursor();
}

void LockScreen::on_unlock_submit() {
    // Enter / click while a check is running: that PIN is already being
    // verified, and a second submission would be a second counted attempt.
    if (unlock_busy_)
        return;
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

    set_unlock_busy(true);
    const quint64 serial = attempt_serial_;
    pm.verify_pin_async(pin, this, [this, serial](bool ok) {
        if (serial != attempt_serial_)
            return; // abandoned — PinManager has accounted the attempt
        set_unlock_busy(false);
        on_unlock_result(ok);
    });
}

void LockScreen::on_unlock_result(bool ok) {
    auto& pm = auth::PinManager::instance();
    if (ok) {
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
        // A check still running keeps the page disabled until its result is in.
        unlock_btn_->setEnabled(!unlock_busy_);
        unlock_pin_input_->setEnabled(!unlock_busy_);
        if (!unlock_busy_)
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

    // showEvent only starts the countdown when the screen is shown while already
    // locked out. If show_unlock()/activate() runs on a visible screen (no new
    // showEvent) nothing would tick, leaving the PIN field disabled past expiry.
    if (isVisible() && lockout_timer_ && !lockout_timer_->isActive())
        lockout_timer_->start();
}

} // namespace fincept::screens
