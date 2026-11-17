// src/ui/widgets/EmptyState.cpp — see the header.

#include "ui/widgets/EmptyState.h"

#include "core/logging/LogRedact.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::ui {

namespace {

constexpr int kEsMaxDetailRows = 8;
/// Column spacing between stacked blocks, and the action row's top margin.
constexpr int kEsSpacing = 10;
constexpr int kEsActionsTop = 4;
/// Margins once the state compacts (any level below Full).
constexpr int kEsTightH = 8;
constexpr int kEsTightV = 4;

QColor es_tone_color(EmptyState::Tone tone) {
    switch (tone) {
        case EmptyState::Tone::Neutral:
            return QColor(colors::TEXT_SECONDARY());
        case EmptyState::Tone::Info:
            return QColor(colors::INFO());
        case EmptyState::Tone::Accent:
            return QColor(colors::AMBER());
        case EmptyState::Tone::Warning:
            return QColor(colors::WARNING());
        case EmptyState::Tone::Danger:
            return QColor(colors::DANGER());
        case EmptyState::Tone::Success:
            return QColor(colors::SUCCESS());
    }
    return QColor(colors::AMBER());
}

QString es_style() {
    const QString f_title = fonts::px(2);
    const QString f_text = fonts::px(-1);
    const QString f_small = fonts::px(-3);
    const QString f_tiny = fonts::px(-4);
    const QString f_btn = fonts::px(-2);
    return QStringLiteral(
               "#feEmptyState { background: transparent; }"
               "#feEsTitle { color: %1; font-size: %9px; font-weight: 700; background: transparent; }"
               "#feEsInline { color: %1; font-size: %10px; font-weight: 700; background: transparent; }"
               "#feEsText { color: %2; font-size: %10px; background: transparent; }"
               "#feEsDetails { background: %4; border: 1px solid %5; }"
               "#feEsDetailsHead { color: %3; font-size: %13px; font-weight: 700; letter-spacing: 0.5px; "
               "  background: transparent; }"
               "#feEsKey { color: %2; font-size: %11px; background: transparent; }"
               "#feEsVal { color: %1; font-size: %12px; font-weight: 700; background: transparent; }"
               "#feEsBullet { color: %1; font-size: %12px; background: transparent; }"
               "#feEsNote { color: %2; font-size: %11px; font-weight: 700; padding: 3px 10px; "
               "  background: transparent; border: 1px solid %6; }"
               "#feEsNote[tone=\"accent\"] { color: %7; border-color: %7; }"
               "#feEsNote[tone=\"warning\"] { color: %14; border-color: %14; }"
               "#feEsNote[tone=\"info\"] { color: %15; border-color: %15; }"
               "#feEsNote[tone=\"danger\"] { color: %16; border-color: %16; }"
               "#feEsNote[tone=\"success\"] { color: %17; border-color: %17; }"
               "#feEsPrimary { background: %7; color: %8; border: 1px solid %7; padding: 6px 18px; "
               "  font-weight: 700; font-size: %12px; }"
               "#feEsPrimary:hover { background: %18; border-color: %18; }"
               "#feEsPrimary:disabled { background: %19; color: %3; border-color: %6; }"
               "#feEsSecondary { background: transparent; color: %1; border: 1px solid %6; padding: 6px 14px; "
               "  font-weight: 700; font-size: %12px; }"
               "#feEsSecondary:hover { background: %20; border-color: %21; }"
               "#feEsLink { background: transparent; color: %7; border: none; padding: 2px 4px; "
               "  font-weight: 700; font-size: %13px; }"
               "#feEsLink:hover { color: %1; }"
               "#feEsTech { background: %4; color: %2; border: 1px solid %5; font-size: %11px; "
               "  font-family: %22; selection-background-color: %7; selection-color: %8; }")
        .arg(colors::TEXT_PRIMARY())   // %1
        .arg(colors::TEXT_SECONDARY()) // %2
        .arg(colors::TEXT_TERTIARY())  // %3
        .arg(colors::BG_SURFACE())     // %4
        .arg(colors::BORDER_DIM())     // %5
        .arg(colors::BORDER_MED())     // %6
        .arg(colors::AMBER())          // %7
        .arg(colors::TEXT_ON_ACCENT()) // %8
        .arg(f_title)                  // %9
        .arg(f_text)                   // %10
        .arg(f_small)                  // %11
        .arg(f_btn)                    // %12
        .arg(f_tiny)                   // %13
        .arg(colors::WARNING())        // %14
        .arg(colors::INFO())           // %15
        .arg(colors::DANGER())         // %16
        .arg(colors::SUCCESS())        // %17
        .arg(colors::AMBER_DIM())      // %18
        .arg(colors::BG_RAISED())      // %19
        .arg(colors::BG_HOVER())       // %20
        .arg(colors::BORDER_BRIGHT())  // %21
        .arg(fonts::DATA_FAMILY())     // %22
        ;
}

/// Height of the technical-details box: ten lines of its font.
int es_technical_height() {
    return QFontMetrics(fonts::font(-3)).lineSpacing() * 10 + 12;
}

QString es_button_text(const QString& text) {
    QString t = text;
    t.replace(QLatin1Char('&'), QStringLiteral("&&"));
    return t;
}

void es_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Show / hide only when it changes (no needless layout invalidation).
void es_set_shown(QWidget* w, bool shown) {
    if (w && w->isHidden() == shown)
        w->setVisible(shown);
}

/// Height `w` needs at `width` (polished first: the stylesheet sets its font).
int es_height_for(QWidget* w, int width) {
    w->ensurePolished();
    if (w->hasHeightForWidth()) {
        const int h = w->heightForWidth(width);
        if (h > 0)
            return h;
    }
    return w->sizeHint().height();
}

} // namespace

// ── Glyph: a ring with one or two characters, painted from the live theme ────

class EmptyStateGlyph : public QWidget {
  public:
    explicit EmptyStateGlyph(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    /// Visibility is the EmptyState's (content and compaction level).
    void set(const QString& glyph, EmptyState::Tone tone) {
        glyph_ = glyph;
        tone_ = tone;
        update();
    }
    bool has_glyph() const { return !glyph_.isEmpty(); }
    void set_compact(bool compact) {
        compact_ = compact;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const int d = diameter();
        return {d, d};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor tone = es_tone_color(tone_);
        const qreal d = std::min(width(), height()) - 2.0;
        const QRectF ring((width() - d) / 2.0, (height() - d) / 2.0, d, d);
        p.setPen(QPen(tone, compact_ ? 1.2 : 1.5));
        p.setBrush(colors::tint_color(tone, 0.10));
        p.drawEllipse(ring);
        QFont f = font();
        f.setPixelSize(fonts::font_px(compact_ ? 3 : 9));
        f.setBold(true);
        p.setFont(f);
        p.setPen(colors::legible_color(tone));
        p.drawText(ring, Qt::AlignCenter, glyph_);
    }

  private:
    int diameter() const { return int(fonts::font_px(0) * (compact_ ? 2.4 : 3.6)); }

    QString glyph_;
    EmptyState::Tone tone_ = EmptyState::Tone::Accent;
    bool compact_ = false;
};

// ── Suggestion row: a keyboard-reachable button painted from the live theme ──

class EmptyStateSuggestion : public QAbstractButton {
  public:
    explicit EmptyStateSuggestion(QWidget* parent) : QAbstractButton(parent) {
        setFocusPolicy(Qt::StrongFocus); // Tab reaches it; Space (QAbstractButton) and Enter click
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true); // repaint on enter / leave
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void set(const EmptyState::Suggestion& s) {
        label_ = s.label;
        secondary_ = s.secondary;
        // The accessible interface (QAccessibleButton) names the row by its text;
        // '&' doubled so a label like "S&P 500" grabs no mnemonic shortcut.
        setText(es_button_text(s.label));
        setAccessibleName(s.label);
        setAccessibleDescription(s.secondary);
        setToolTip(s.tooltip);
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const QFontMetrics lm(label_font());
        const QFontMetrics sm(secondary_font());
        int w = lm.horizontalAdvance(label_);
        if (!secondary_.isEmpty())
            w = std::max(w, sm.horizontalAdvance(secondary_));
        w += 3 * pad_h() + lm.horizontalAdvance(arrow());
        int h = 2 * pad_v() + lm.height();
        if (!secondary_.isEmpty())
            h += gap() + sm.height();
        return {w, h};
    }
    QSize minimumSizeHint() const override {
        const QFontMetrics lm(label_font());
        return {lm.averageCharWidth() * 10 + 3 * pad_h(), sizeHint().height()};
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = rect();
        const bool hot = isEnabled() && (underMouse() || isDown());
        p.fillRect(r, QColor(hot ? colors::BG_HOVER() : colors::BG_SURFACE()));
        const QColor border(hasFocus() ? colors::AMBER() : hot ? colors::BORDER_BRIGHT() : colors::BORDER_DIM());
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, 1));
        p.drawRect(r.adjusted(0, 0, -1, -1));
        if (hasFocus())
            p.drawRect(r.adjusted(1, 1, -2, -2)); // 2 px: the keyboard focus must be obvious

        const QFont lf = label_font();
        const QFont sf = secondary_font();
        const QFontMetrics lm(lf);
        const QFontMetrics sm(sf);
        const int ph = pad_h();
        const int arrow_w = lm.horizontalAdvance(arrow());
        const int text_w = std::max(0, r.width() - 3 * ph - arrow_w);
        int y = pad_v();
        p.setFont(lf);
        p.setPen(QColor(colors::TEXT_PRIMARY()));
        p.drawText(QRect(ph, y, text_w, lm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   lm.elidedText(label_, Qt::ElideRight, text_w));
        if (!secondary_.isEmpty()) {
            y += lm.height() + gap();
            p.setFont(sf);
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRect(ph, y, text_w, sm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       sm.elidedText(secondary_, Qt::ElideRight, text_w));
        }
        p.setFont(lf);
        p.setPen(QColor(colors::AMBER()));
        p.drawText(QRect(r.width() - ph - arrow_w, 0, arrow_w, r.height()), Qt::AlignCenter, arrow());
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !e->isAutoRepeat()) {
            click();
            e->accept();
            return;
        }
        QAbstractButton::keyPressEvent(e);
    }

  private:
    static QFont label_font() { return fonts::font(-1, QFont::Bold); }
    static QFont secondary_font() { return fonts::font(-3); }
    static QString arrow() { return QStringLiteral("›"); } // ›
    int pad_h() const { return std::max(6, QFontMetrics(label_font()).averageCharWidth()); }
    int pad_v() const { return std::max(4, QFontMetrics(label_font()).height() / 4); }
    int gap() const { return std::max(1, QFontMetrics(secondary_font()).height() / 8); }

    QString label_;
    QString secondary_;
};

// ── Clickable detail row (set_details_clickable): key left, value right, "›" ──

class EmptyStateDetailRow : public QAbstractButton {
  public:
    EmptyStateDetailRow(const QString& key, const QString& value, QWidget* parent)
        : QAbstractButton(parent), key_(key), value_(value) {
        setFocusPolicy(Qt::StrongFocus); // Tab reaches it; Space (QAbstractButton) and Enter click
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true); // repaint on enter / leave
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const QString name = key.isEmpty() ? value : key;
        setText(es_button_text(name)); // '&' doubled: no mnemonic grabbed
        setAccessibleName(name);
        setAccessibleDescription(key.isEmpty() ? QString() : value);
    }

    QSize sizeHint() const override {
        const QFontMetrics km(key_font());
        const QFontMetrics vm(value_font());
        int w = 3 * pad_h() + vm.horizontalAdvance(arrow()) + vm.horizontalAdvance(shown_value());
        if (!key_.isEmpty())
            w += km.horizontalAdvance(key_) + gap_h();
        return {w, 2 * pad_v() + std::max(km.height(), vm.height())};
    }
    QSize minimumSizeHint() const override {
        return {QFontMetrics(value_font()).averageCharWidth() * 12 + 3 * pad_h(), sizeHint().height()};
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = rect();
        const bool hot = isEnabled() && (underMouse() || isDown());
        if (hot)
            p.fillRect(r, QColor(colors::BG_HOVER()));
        if (hasFocus()) {
            // 2 px: the keyboard focus must be obvious in both themes.
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(colors::AMBER()), 1));
            p.drawRect(r.adjusted(0, 0, -1, -1));
            p.drawRect(r.adjusted(1, 1, -2, -2));
        }
        const QFont kf = key_font();
        const QFont vf = value_font();
        const QFontMetrics km(kf);
        const QFontMetrics vm(vf);
        const int ph = pad_h();
        const int arrow_w = vm.horizontalAdvance(arrow());
        const int room = std::max(0, r.width() - 3 * ph - arrow_w);
        int key_w = 0;
        if (!key_.isEmpty()) {
            // The key gets what the value leaves, and at least 45 % of the row when both are long.
            const int value_full = vm.horizontalAdvance(shown_value());
            key_w = std::max(0, std::min(km.horizontalAdvance(key_),
                                         std::max(room * 45 / 100, room - gap_h() - value_full)));
            p.setFont(kf);
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRect(ph, 0, key_w, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       km.elidedText(key_, Qt::ElideRight, key_w));
        }
        const int value_x = ph + (key_w > 0 ? key_w + gap_h() : 0);
        const int value_w = std::max(0, ph + room - value_x);
        p.setFont(vf);
        p.setPen(QColor(colors::TEXT_PRIMARY()));
        p.drawText(QRect(value_x, 0, value_w, r.height()),
                   (key_.isEmpty() ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                   vm.elidedText(shown_value(), Qt::ElideRight, value_w));
        p.setPen(QColor(colors::AMBER()));
        p.drawText(QRect(r.width() - ph - arrow_w, 0, arrow_w, r.height()), Qt::AlignCenter, arrow());
    }

    void resizeEvent(QResizeEvent* e) override {
        QAbstractButton::resizeEvent(e);
        // What an elided row leaves out is one hover away.
        const QString tip = sizeHint().width() > width()
                                ? (key_.isEmpty() ? value_ : key_ + QStringLiteral(": ") + value_)
                                : QString();
        if (toolTip() != tip)
            setToolTip(tip);
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !e->isAutoRepeat()) {
            click();
            e->accept();
            return;
        }
        QAbstractButton::keyPressEvent(e);
    }

  private:
    // The fonts of the plain rows (#feEsKey / #feEsVal / #feEsBullet), from the live theme.
    QFont key_font() const { return fonts::font(-3); }
    QFont value_font() const { return key_.isEmpty() ? fonts::font(-2) : fonts::font(-2, QFont::Bold); }
    QString shown_value() const {
        return key_.isEmpty() ? QString(QChar(0x2022)) + QLatin1Char(' ') + value_ : value_;
    }
    static QString arrow() { return QStringLiteral("›"); } // ›
    int pad_h() const { return std::max(4, QFontMetrics(value_font()).averageCharWidth() / 2); }
    int pad_v() const { return std::max(3, QFontMetrics(value_font()).height() / 5); }
    int gap_h() const { return std::max(8, QFontMetrics(value_font()).averageCharWidth() * 2); }

    QString key_;
    QString value_;
};

// ── EmptyState ───────────────────────────────────────────────────────────────

EmptyState::EmptyState(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("feEmptyState"));
    setStyleSheet(es_style()); // before any child exists
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    outer_ = new QVBoxLayout(this);
    outer_->setContentsMargins(24, 24, 24, 24);
    outer_->setSpacing(0);
    outer_->addStretch(1);

    column_ = new QWidget(this);
    column_->setMaximumWidth(fonts::font_px(0) * 42);
    auto* col = new QVBoxLayout(column_);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(kEsSpacing);

    glyph_ = new EmptyStateGlyph(column_);
    glyph_->hide();
    col->addWidget(glyph_, 0, Qt::AlignHCenter);

    title_ = new QLabel(column_);
    title_->setObjectName(QStringLiteral("feEsTitle"));
    title_->setAlignment(Qt::AlignCenter);
    title_->setWordWrap(true);
    title_->setTextFormat(Qt::PlainText);
    title_->hide();
    col->addWidget(title_);

    message_ = new QLabel(column_);
    message_->setObjectName(QStringLiteral("feEsText"));
    message_->setAlignment(Qt::AlignCenter);
    message_->setWordWrap(true);
    message_->setTextFormat(Qt::PlainText);
    message_->hide();
    col->addWidget(message_);

    note_ = new QLabel(column_);
    note_->setObjectName(QStringLiteral("feEsNote"));
    note_->setAlignment(Qt::AlignCenter);
    note_->setWordWrap(true);
    note_->setTextFormat(Qt::PlainText);
    note_->hide();
    col->addWidget(note_, 0, Qt::AlignHCenter);

    details_box_ = new QWidget(column_);
    details_box_->setObjectName(QStringLiteral("feEsDetails"));
    details_box_->setAttribute(Qt::WA_StyledBackground, true);
    auto* dl = new QVBoxLayout(details_box_);
    dl->setContentsMargins(12, 8, 12, 10);
    dl->setSpacing(6);
    details_heading_ = new QLabel(details_box_);
    details_heading_->setObjectName(QStringLiteral("feEsDetailsHead"));
    dl->addWidget(details_heading_);
    details_grid_ = new QGridLayout;
    details_grid_->setContentsMargins(0, 0, 0, 0);
    details_grid_->setHorizontalSpacing(14);
    details_grid_->setVerticalSpacing(4);
    dl->addLayout(details_grid_);
    details_box_->hide();
    col->addWidget(details_box_);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, kEsActionsTop, 0, 0);
    actions->setSpacing(8);
    actions->addStretch(1);
    // The title on the action row when the state compacts to one line (Inline /
    // TitleOnly): elided to the room left, the hidden text in its tooltip.
    inline_title_ = new QLabel(column_);
    inline_title_->setObjectName(QStringLiteral("feEsInline"));
    inline_title_->setTextFormat(Qt::PlainText);
    inline_title_->setMinimumWidth(1); // its width follows the elided text, never the full title
    inline_title_->hide();
    primary_ = new QPushButton(column_);
    primary_->setObjectName(QStringLiteral("feEsPrimary"));
    primary_->setCursor(Qt::PointingHandCursor);
    primary_->hide();
    connect(primary_, &QPushButton::clicked, this, &EmptyState::primary_clicked);
    secondary_ = new QPushButton(column_);
    secondary_->setObjectName(QStringLiteral("feEsSecondary"));
    secondary_->setCursor(Qt::PointingHandCursor);
    secondary_->hide();
    connect(secondary_, &QPushButton::clicked, this, &EmptyState::secondary_clicked);
    actions->addWidget(inline_title_);
    actions->addWidget(primary_);
    actions->addWidget(secondary_);
    actions->addStretch(1);
    col->addLayout(actions);

    // Suggestion rows under the actions (built on first use, in this box).
    suggestions_box_ = new QWidget(column_);
    auto* sl = new QVBoxLayout(suggestions_box_);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(4);
    suggestions_heading_ = new QLabel(suggestions_box_);
    suggestions_heading_->setObjectName(QStringLiteral("feEsDetailsHead"));
    suggestions_heading_->setTextFormat(Qt::PlainText);
    suggestions_heading_->hide();
    sl->addWidget(suggestions_heading_);
    suggestions_box_->hide();
    col->addWidget(suggestions_box_);

    outer_->addWidget(column_, 0, Qt::AlignHCenter);
    outer_->addStretch(1);

    // The column's width and the fit follow the base font; re-fit once a change
    // has settled (never inside theme_changed — see ThemeManager::notifying()).
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        column_->setMaximumWidth(fonts::font_px(0) * (compact_ ? 30 : 42));
        glyph_->updateGeometry();
        if (technical_box_)
            technical_box_->setFixedHeight(es_technical_height());
        for (EmptyStateSuggestion* row : suggestion_rows_)
            row->updateGeometry();
        for (EmptyStateDetailRow* row : detail_buttons_)
            row->updateGeometry();
        refit();
    });
    // An opaque backdrop is painted from the live theme: repaint on a change
    // (update() only — nothing is shown, hidden or restyled here).
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        if (opaque_)
            update();
    });
}

void EmptyState::ensure_technical() {
    if (technical_btn_)
        return;
    auto* col = qobject_cast<QVBoxLayout*>(column_->layout());
    // Under the message (the plain sentence stays first), built in its final parent.
    const int at = col ? col->indexOf(message_) + 1 : 0;
    technical_btn_ = new QPushButton(column_);
    technical_btn_->setObjectName(QStringLiteral("feEsLink"));
    technical_btn_->setCursor(Qt::PointingHandCursor);
    technical_btn_->setCheckable(true);
    technical_btn_->hide();
    connect(technical_btn_, &QPushButton::toggled, this, [this](bool open) { set_technical_open(open); });
    technical_box_ = new QPlainTextEdit(column_);
    technical_box_->setObjectName(QStringLiteral("feEsTech"));
    technical_box_->setReadOnly(true); // selectable and copyable (Ctrl+C, context menu), never edited
    technical_box_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    technical_box_->setLineWrapMode(QPlainTextEdit::NoWrap); // a traceback keeps its lines
    technical_box_->setFixedHeight(es_technical_height());
    technical_box_->hide();
    if (col) {
        col->insertWidget(at, technical_btn_, 0, Qt::AlignHCenter);
        col->insertWidget(at + 1, technical_box_);
    }
}

void EmptyState::set_technical_open(bool open) {
    if (!technical_btn_)
        return;
    technical_open_ = open;
    technical_btn_->setText(open ? tr("HIDE DETAILS ▴") : tr("DETAILS ▾")); // ▴ / ▾
    refit();
}

void EmptyState::set_technical_details(const QString& text) {
    setter_used();
    if (text == technical_ && (text.isEmpty() || technical_btn_))
        return;
    technical_ = text;
    if (text.isEmpty()) {
        if (technical_btn_) {
            technical_box_->clear();
            const QSignalBlocker block(technical_btn_);
            technical_btn_->setChecked(false);
            technical_open_ = false;
        }
        refit();
        return;
    }
    ensure_technical();
    // Masked before display: a traceback can carry keys, tokens, e-mail or home paths.
    technical_box_->setPlainText(fincept::log_scrub(text));
    const QSignalBlocker block(technical_btn_);
    technical_btn_->setChecked(false); // a new failure starts collapsed
    set_technical_open(false);
}

QString EmptyState::tone_key(Tone tone) {
    switch (tone) {
        case Tone::Neutral:
            return QStringLiteral("neutral");
        case Tone::Info:
            return QStringLiteral("info");
        case Tone::Accent:
            return QStringLiteral("accent");
        case Tone::Warning:
            return QStringLiteral("warning");
        case Tone::Danger:
            return QStringLiteral("danger");
        case Tone::Success:
            return QStringLiteral("success");
    }
    return QStringLiteral("neutral");
}

void EmptyState::set_glyph(const QString& glyph, Tone tone) {
    setter_used();
    glyph_->set(glyph, tone);
    refit();
}

void EmptyState::set_title(const QString& title) {
    setter_used();
    if (title_->text() != title)
        title_->setText(title);
    refit();
}

QString EmptyState::title() const {
    return title_->text();
}

void EmptyState::set_message(const QString& text) {
    setter_used();
    if (message_->text() != text)
        message_->setText(text);
    refit();
}

QList<QPair<QString, QString>> EmptyState::bullet_rows(const QStringList& lines) {
    QList<QPair<QString, QString>> rows;
    rows.reserve(lines.size());
    for (const QString& line : lines)
        rows.append({QString(), line});
    return rows;
}

void EmptyState::set_details(const QList<QPair<QString, QString>>& rows, const QString& heading) {
    setter_used();
    // Drop the previous rows (the heading label stays).
    while (QLayoutItem* item = details_grid_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide(); // out of the layout now: do not paint at the old place until deleted
            w->deleteLater();
        }
        delete item;
    }
    detail_buttons_.clear();
    details_ = rows;
    details_heading_text_ = heading;
    detail_rows_ = std::min<int>(int(rows.size()), kEsMaxDetailRows);
    if (detail_rows_ > 0) {
        details_heading_->setText(heading.isEmpty() ? tr("SAMPLE INPUTS") : heading);
        // One row per line: stays readable in a narrow pane and at large base fonts.
        for (int i = 0; i < detail_rows_; ++i) {
            const QPair<QString, QString>& row = rows.at(i);
            if (details_clickable_) {
                // An action row across the block (key left, value right, "›").
                auto* btn = new EmptyStateDetailRow(row.first, row.second, details_box_);
                mark_agent_risk(btn, detail_agent_risk(i)); // round 7: the row's own button carries its kind
                const QString key = row.first;
                connect(btn, &QAbstractButton::clicked, this, [this, i, key]() { emit detail_clicked(i, key); });
                details_grid_->addWidget(btn, i, 0, 1, 2);
                detail_buttons_.append(btn);
                continue;
            }
            if (row.first.isEmpty()) {
                // No key column: a bullet across the block, wrapped.
                auto* bullet = new QLabel(QString(QChar(0x2022)) + QLatin1Char(' ') + row.second, details_box_);
                bullet->setObjectName(QStringLiteral("feEsBullet"));
                bullet->setTextFormat(Qt::PlainText);
                bullet->setWordWrap(true);
                details_grid_->addWidget(bullet, i, 0, 1, 2);
                continue;
            }
            auto* key = new QLabel(row.first, details_box_);
            key->setObjectName(QStringLiteral("feEsKey"));
            key->setTextFormat(Qt::PlainText);
            auto* val = new QLabel(row.second, details_box_);
            val->setObjectName(QStringLiteral("feEsVal"));
            val->setTextFormat(Qt::PlainText);
            val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            details_grid_->addWidget(key, i, 0);
            details_grid_->addWidget(val, i, 1);
        }
    }
    // Tighter rows when each is a button with its own padding.
    const int vspace = details_clickable_ ? 0 : 4;
    if (details_grid_->verticalSpacing() != vspace)
        details_grid_->setVerticalSpacing(vspace);
    if (!detail_buttons_.isEmpty())
        order_tab_chain();
    refit();
}

void EmptyState::set_details_clickable(bool clickable) {
    setter_used();
    if (details_clickable_ == clickable)
        return;
    details_clickable_ = clickable;
    if (!details_.isEmpty())
        set_details(details_, details_heading_text_); // rebuilt as buttons / plain rows
}

void EmptyState::set_details_agent_risk(const QString& kind, const QStringList& per_row) {
    if (kind == details_risk_ && per_row == details_risks_)
        return; // unchanged: the apply memo stays valid
    setter_used();
    details_risk_ = kind;
    details_risks_ = per_row;
    mark_detail_rows();
}

void EmptyState::mark_detail_rows() {
    // detail_buttons_[i] is row i: with clickable rows every row is a button.
    for (int i = 0; i < detail_buttons_.size(); ++i)
        mark_agent_risk(detail_buttons_.at(i), detail_agent_risk(i));
}

void EmptyState::order_tab_chain() {
    // Built before it is added to a layout (no parent, not shown): chain on the parent change /
    // first show instead, so the order is set inside the window the EmptyState will live in.
    if (!parentWidget() && !isVisible()) {
        tab_chain_pending_ = true;
        return;
    }
    chain_tab_order();
}

void EmptyState::chain_tab_order() {
    tab_chain_pending_ = false;
    // Reading order: DETAILS (under the message) → detail rows → actions → suggestions.
    // Safe by construction: only non-null, focusable widgets of this window — QWidget::setTabOrder
    // warns ("must be in the same window") for a pair across windows. Widgets hidden by the
    // compaction level stay in the layout and keep their place; replaced detail rows are already
    // out of detail_buttons_ (deleted later), so they are never chained.
    QWidget* const win = window();
    QList<QWidget*> chain;
    const auto add = [&chain, win](QWidget* w) {
        if (w && !w->isWindow() && w->window() == win && (w->focusPolicy() & Qt::TabFocus))
            chain.append(w);
    };
    add(technical_btn_);
    for (EmptyStateDetailRow* row : std::as_const(detail_buttons_))
        add(row);
    add(primary_);
    add(secondary_);
    for (EmptyStateSuggestion* row : std::as_const(suggestion_rows_))
        add(row);
    for (int i = 1; i < chain.size(); ++i)
        QWidget::setTabOrder(chain.at(i - 1), chain.at(i));
}

bool EmptyState::event(QEvent* e) {
    const bool handled = QWidget::event(e);
    // A deferred tab chain is applied once the EmptyState has its parent (first show: showEvent).
    if (e->type() == QEvent::ParentChange && tab_chain_pending_ && parentWidget())
        chain_tab_order();
    return handled;
}

void EmptyState::set_suggestions(const QList<Suggestion>& rows, const QString& heading) {
    setter_used();
    const int n = std::min<int>(int(rows.size()), kMaxSuggestions);
    auto* sl = qobject_cast<QVBoxLayout*>(suggestions_box_->layout());
    while (suggestion_rows_.size() < n) {
        const int index = int(suggestion_rows_.size());
        auto* row = new EmptyStateSuggestion(suggestions_box_);
        connect(row, &QAbstractButton::clicked, this, [this, index]() { emit suggestion_clicked(index); });
        if (sl)
            sl->addWidget(row);
        suggestion_rows_.append(row);
    }
    for (int i = 0; i < suggestion_rows_.size(); ++i) {
        EmptyStateSuggestion* row = suggestion_rows_.at(i);
        if (i < n)
            row->set(rows.at(i));
        // Each row is its own button: its marker sits on it (spare rows carry none).
        mark_agent_risk(row, i < n ? rows.at(i).agent_risk : QString());
        es_set_shown(row, i < n); // spare rows stay hidden for the next list
    }
    suggestions_shown_ = n;
    if (suggestions_heading_->text() != heading)
        suggestions_heading_->setText(heading);
    es_set_shown(suggestions_heading_, !heading.isEmpty());
    refit();
}

int EmptyState::suggestion_count() const {
    return suggestions_shown_;
}

void EmptyState::set_note(const QString& text, Tone tone) {
    setter_used();
    const QString key = tone_key(tone);
    if (note_->text() != text)
        note_->setText(text);
    if (note_tone_ != key) {
        note_tone_ = key;
        note_->setProperty("tone", key);
        es_repolish(note_);
    }
    refit();
}

void EmptyState::set_primary_action(const QString& text) {
    setter_used();
    primary_->setText(es_button_text(text));
    refit();
}

void EmptyState::set_secondary_action(const QString& text) {
    setter_used();
    secondary_->setText(es_button_text(text));
    refit();
}

void EmptyState::set_primary_enabled(bool enabled) {
    setter_used();
    primary_->setEnabled(enabled);
}

void EmptyState::set_primary_tooltip(const QString& tooltip) {
    setter_used();
    if (primary_->toolTip() != tooltip)
        primary_->setToolTip(tooltip);
}

void EmptyState::set_secondary_tooltip(const QString& tooltip) {
    setter_used();
    if (secondary_->toolTip() != tooltip)
        secondary_->setToolTip(tooltip);
}

void EmptyState::set_primary_agent_risk(const QString& kind) {
    if (kind == primary_risk_)
        return; // unchanged: the apply memo stays valid
    setter_used();
    primary_risk_ = kind;
    mark_agent_risk(primary_, primary_agent_risk());
}

void EmptyState::set_secondary_agent_risk(const QString& kind) {
    if (kind == secondary_risk_)
        return;
    setter_used();
    secondary_risk_ = kind;
    mark_agent_risk(secondary_, kind);
}

void EmptyState::set_primary_agent_risk_default(const QString& kind) {
    // Not part of the Spec (kept across apply()): no memo invalidation.
    if (kind == primary_risk_default_)
        return;
    primary_risk_default_ = kind;
    mark_agent_risk(primary_, primary_agent_risk());
}

QString EmptyState::primary_agent_risk() const {
    return primary_risk_.isEmpty() ? primary_risk_default_ : primary_risk_;
}

void EmptyState::set_compact(bool compact) {
    if (compact_ == compact)
        return;
    compact_ = compact;
    glyph_->set_compact(compact);
    column_->setMaximumWidth(fonts::font_px(0) * (compact ? 30 : 42));
    refit(); // margins follow the mode
}

void EmptyState::set_alignment(Qt::Alignment vertical) {
    // Stretch items sit at index 0 (above the column) and 2 (below it).
    const int above = (vertical & Qt::AlignTop) ? 0 : 1;
    const int below = (vertical & Qt::AlignBottom) ? 0 : 1;
    outer_->setStretch(0, above);
    outer_->setStretch(2, below);
}

void EmptyState::set_opaque(bool opaque, Backdrop backdrop) {
    if (opaque_ == opaque && backdrop_ == backdrop)
        return;
    opaque_ = opaque;
    backdrop_ = backdrop;
    // Every pixel is painted (paintEvent): Qt need not paint what lies behind.
    setAttribute(Qt::WA_OpaquePaintEvent, opaque);
    update();
}

void EmptyState::set_auto_fit(bool enabled) {
    if (auto_fit_ == enabled)
        return;
    auto_fit_ = enabled;
    if (!enabled)
        full_height_ = 0;
    updateGeometry(); // size hints change with the mode
    refit();
}

void EmptyState::set_preview_shape(LoadingSkeleton::Shape shape) {
    setter_used();
    if (!illustration_) {
        illustration_ = new LoadingSkeleton(shape, this);
        illustration_->set_ghost(true);
        illustration_->lower(); // behind the text column
        illustration_->setGeometry(rect());
    }
    illustration_->set_shape(shape);
    illustration_->show();
}

void EmptyState::clear_preview_shape() {
    setter_used();
    if (illustration_)
        illustration_->hide();
}

void EmptyState::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (illustration_)
        illustration_->setGeometry(rect());
    refit(); // deferred to appearance_settled while the theme manager notifies
}

void EmptyState::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (tab_chain_pending_) // built unparented and shown as it is (or the parent change was missed)
        chain_tab_order();
    refit();
}

void EmptyState::paintEvent(QPaintEvent* e) {
    if (opaque_) {
        QPainter p(this);
        const char* token = backdrop_ == Backdrop::Base     ? colors::BG_BASE()
                            : backdrop_ == Backdrop::Raised ? colors::BG_RAISED()
                                                            : colors::BG_SURFACE();
        p.fillRect(e->rect(), QColor(token));
    }
    QWidget::paintEvent(e);
}

// ── Auto-compaction ──────────────────────────────────────────────────────────

int EmptyState::full_margins() const {
    return compact_ ? 10 : 24; // vertical; horizontal is 12 / 24
}

int EmptyState::column_width() const {
    const QMargins m = outer_->contentsMargins();
    const int w = width() - m.left() - m.right();
    return std::max(0, std::min(w, column_->maximumWidth()));
}

empty_state_fit::Parts EmptyState::measure(int colw) const {
    empty_state_fit::Parts p;
    p.margins = 2 * full_margins();
    p.tight_margins = 2 * kEsTightV;
    p.spacing = kEsSpacing;
    if (glyph_->has_glyph())
        p.glyph = glyph_->sizeHint().height();
    if (!title_->text().isEmpty())
        p.title = es_height_for(title_, colw);
    if (!message_->text().isEmpty())
        p.message = es_height_for(message_, colw);
    if (!note_->text().isEmpty()) {
        note_->ensurePolished();
        p.note = es_height_for(note_, std::min(colw, note_->sizeHint().width())); // centred at its own width
    }
    if (technical_btn_ && !technical_.isEmpty()) {
        technical_btn_->ensurePolished();
        p.technical = technical_btn_->sizeHint().height();
        if (technical_open_)
            p.technical += kEsSpacing + es_technical_height();
    }
    if (detail_rows_ > 0)
        p.details = es_height_for(details_box_, colw);
    if (suggestions_shown_ > 0)
        p.suggestions = es_height_for(suggestions_box_, colw);
    int primary_h = 0;
    if (!primary_->text().isEmpty()) {
        primary_->ensurePolished();
        primary_h = primary_->sizeHint().height();
    }
    int secondary_h = 0;
    if (!secondary_->text().isEmpty()) {
        secondary_->ensurePolished();
        secondary_h = secondary_->sizeHint().height();
    }
    if (primary_h > 0 || secondary_h > 0)
        p.actions = kEsActionsTop + std::max(primary_h, secondary_h);
    if (!title_->text().isEmpty()) {
        inline_title_->ensurePolished();
        const int line = QFontMetrics(inline_title_->font()).height();
        p.title_line = kEsActionsTop + line;
        p.inline_row = kEsActionsTop + std::max(line, primary_h);
    } else if (primary_h > 0) {
        p.inline_row = kEsActionsTop + primary_h;
    }
    return p;
}

void EmptyState::refit() {
    if (fitting_ || batching_)
        return;
    const QScopedValueRollback<bool> guard(fitting_, true);
    using empty_state_fit::Level;
    Level level = fit_level_;
    if (!auto_fit_) {
        level = Level::Full;
    } else if (isVisible() && height() > 0 && !ThemeManager::instance().notifying()) {
        // Sizes come from the children's font metrics, so never while the theme
        // manager notifies (appearance_settled refits); hidden: decided on show.
        const empty_state_fit::Parts parts = measure(column_width());
        const int full = empty_state_fit::needed_height(parts, Level::Full);
        if (full != full_height_) {
            full_height_ = full;
            updateGeometry();
        }
        level = empty_state_fit::choose_level(parts, height());
    }
    fit_level_ = level;
    apply_fit();
}

void EmptyState::apply_fit() {
    using empty_state_fit::Level;
    const Level lv = fit_level_;
    const bool full = lv == Level::Full;
    const bool rows = lv == Level::Full || lv == Level::NoGlyph;
    const bool stacked = rows || lv == Level::NoDetails;
    const bool has_title = !title_->text().isEmpty();

    const int mh = full ? (compact_ ? 12 : 24) : kEsTightH;
    const int mv = full ? full_margins() : kEsTightV;
    if (outer_->contentsMargins() != QMargins(mh, mv, mh, mv))
        outer_->setContentsMargins(mh, mv, mh, mv);

    es_set_shown(glyph_, glyph_->has_glyph() && full);
    es_set_shown(title_, has_title && stacked);
    es_set_shown(message_, !message_->text().isEmpty() && stacked);
    es_set_shown(note_, !note_->text().isEmpty() && stacked);
    es_set_shown(details_box_, detail_rows_ > 0 && rows);
    es_set_shown(suggestions_box_, suggestions_shown_ > 0 && rows);
    if (technical_btn_) {
        const bool tech = !technical_.isEmpty() && rows;
        es_set_shown(technical_btn_, tech);
        es_set_shown(technical_box_, tech && technical_open_);
    }
    es_set_shown(primary_, !primary_->text().isEmpty() && lv != Level::TitleOnly);
    es_set_shown(secondary_, !secondary_->text().isEmpty() && stacked);
    es_set_shown(inline_title_, has_title && !stacked);
    update_inline_title();
}

void EmptyState::update_inline_title() {
    if (inline_title_->isHidden())
        return;
    const QString title = title_->text();
    int room = column_width();
    if (!primary_->isHidden())
        room -= primary_->sizeHint().width() + 8; // the action row's spacing
    inline_title_->ensurePolished();
    const QFontMetrics fm(inline_title_->font());
    const QString shown = fm.elidedText(title, Qt::ElideRight, std::max(room, fm.averageCharWidth() * 3));
    if (inline_title_->text() != shown)
        inline_title_->setText(shown);
    // What the compact line leaves out is one hover away.
    QStringList tip;
    if (shown != title)
        tip << title;
    if (!message_->text().isEmpty())
        tip << message_->text();
    if (!note_->text().isEmpty())
        tip << note_->text();
    const QString tooltip = tip.join(QLatin1Char('\n'));
    if (inline_title_->toolTip() != tooltip)
        inline_title_->setToolTip(tooltip);
    inline_title_->setAccessibleName(title);
    inline_title_->setAccessibleDescription(message_->text());
}

QSize EmptyState::sizeHint() const {
    QSize s = QWidget::sizeHint();
    // Preferred: the FULL layout, also while compacted (a parent with room gives it back).
    if (auto_fit_ && full_height_ > s.height())
        s.setHeight(full_height_);
    return s;
}

QSize EmptyState::minimumSizeHint() const {
    QSize s = QWidget::minimumSizeHint();
    if (!auto_fit_)
        return s;
    // The least it needs: the title alone on one line (TitleOnly).
    const int line = QFontMetrics(fonts::font(-1, QFont::Bold)).height();
    s.setHeight(kEsActionsTop + line + 2 * kEsTightV);
    return s;
}

bool EmptyState::hasHeightForWidth() const {
    // With auto-fit the state fits itself into the height it gets; height-for-width
    // would make the full layout a minimum in box layouts.
    return auto_fit_ ? false : QWidget::hasHeightForWidth();
}

void EmptyState::apply(const Spec& spec) {
    // The spec already on screen (and nothing changed since by a single setter):
    // nothing to do — no relayout, repaint or fit pass.
    if (!applied_.should_apply(spec))
        return;
    {
        const QScopedValueRollback<bool> batch(batching_, true); // one refit at the end
        set_glyph(spec.glyph, spec.glyph_tone);
        set_title(spec.title);
        set_message(spec.message);
        details_clickable_ = spec.details_clickable; // before the rows are built
        details_risk_ = spec.details_agent_risk;     // likewise: each row button is marked as it is built
        details_risks_ = spec.details_agent_risks;
        set_details(spec.details, spec.details_heading);
        set_note(spec.note, spec.note_tone);
        set_primary_action(spec.primary);
        set_secondary_action(spec.secondary);
        set_primary_enabled(true);
        set_primary_tooltip(spec.primary_tooltip);
        set_secondary_tooltip(spec.secondary_tooltip);
        set_primary_agent_risk(spec.primary_agent_risk);
        set_secondary_agent_risk(spec.secondary_agent_risk);
        set_technical_details(spec.technical);
        set_suggestions(spec.suggestions, spec.suggestions_heading);
        if (spec.illustration)
            set_preview_shape(spec.illustration_shape);
        else
            clear_preview_shape();
        if (!detail_buttons_.isEmpty())
            order_tab_chain(); // the DETAILS button / suggestion rows may be new
    }
    refit();
}

EmptyState* EmptyState::create(const Spec& spec, QWidget* parent) {
    auto* es = new EmptyState(parent);
    es->apply(spec);
    return es;
}

EmptyState::Spec EmptyState::preview_spec(const QString& title, const QString& what_it_returns,
                                          const QString& run_text, const QString& cost_note, Tone cost_tone,
                                          LoadingSkeleton::Shape illustration,
                                          const QList<QPair<QString, QString>>& details,
                                          const QString& details_heading) {
    Spec s;
    s.glyph = QStringLiteral("▶"); // ▶
    s.glyph_tone = Tone::Accent;
    s.title = title;
    s.message = what_it_returns;
    s.details = details;
    s.details_heading = details_heading; // empty = "SAMPLE INPUTS" (set_details)
    s.note = cost_note;
    s.note_tone = cost_tone;
    s.primary = run_text.isEmpty() ? tr("RUN") : run_text;
    s.illustration = true;
    s.illustration_shape = illustration;
    return s;
}

EmptyState::Spec EmptyState::no_data_spec(const QString& title, const QString& message, const QString& action_text) {
    Spec s;
    s.glyph = QStringLiteral("○"); // ○
    s.glyph_tone = Tone::Neutral;
    s.title = title;
    s.message = message;
    s.primary = action_text;
    return s;
}

EmptyState::Spec EmptyState::error_spec(const QString& message, const QString& title, const QString& action_text,
                                        const QString& technical) {
    Spec s;
    s.glyph = QStringLiteral("!");
    s.glyph_tone = Tone::Danger;
    s.title = title.isEmpty() ? tr("Could not load") : title;
    s.message = message;
    s.primary = action_text.isEmpty() ? tr("RETRY") : action_text;
    s.technical = technical;
    return s;
}

EmptyState::Spec EmptyState::needs_setup_spec(const RequirementStatus& status) {
    Spec s;
    s.glyph = QStringLiteral("⚙"); // ⚙
    s.glyph_tone = Tone::Warning;
    s.title = status.title.isEmpty() ? tr("Needs setup") : status.title;
    s.message = status.message;
    s.primary = status.action_text;
    s.secondary = status.secondary_text;
    // A check that could not be made (status.retry) is a problem to retry, not setup to do.
    if (status.retry)
        s.glyph = QStringLiteral("!");
    // Not decided yet (a saved sign-in being confirmed): neutral, nothing to press.
    if (status.pending) {
        s.glyph = QStringLiteral("…"); // …
        s.glyph_tone = Tone::Neutral;
    }
    return s;
}

void EmptyState::set_content(const QString& glyph, const QString& title, const QString& message,
                             const QString& primary_action, const QString& note, Tone note_tone) {
    setter_used(); // before the batch: its setters do not invalidate
    {
        const QScopedValueRollback<bool> batch(batching_, true);
        set_glyph(glyph, Tone::Accent);
        set_title(title);
        set_message(message);
        set_primary_action(primary_action);
        set_note(note, note_tone);
    }
    refit();
}

} // namespace fincept::ui
