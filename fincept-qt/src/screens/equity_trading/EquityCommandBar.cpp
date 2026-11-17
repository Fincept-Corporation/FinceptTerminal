// EquityCommandBar.cpp — see header.
#include "screens/equity_trading/EquityCommandBar.h"

#include "screens/equity_trading/EquityTickerBar.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenCommandBar.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyle>

#include <algorithm>

namespace fincept::screens::equity {

namespace {

QString eqcb_sheet() {
    namespace c = fincept::ui::colors;
    return QStringLiteral(
               "QWidget#eqCommandBar{background:%1;border:none;border-bottom:1px solid %2;}"
               "QLabel#eqInstrumentLine{color:%3;background:transparent;border:none;font-size:%4px;}"
               "QWidget#eqBarRule{background:%2;border:none;}"
               "QLabel#eqSessionChip{color:%3;background:transparent;border:1px solid %2;padding:0px 5px;"
               "font-size:%5px;font-weight:700;letter-spacing:0.6px;}"
               "QLabel#eqSessionChip[state=\"open\"]{color:%6;border-color:%6;}"
               "QLabel#eqConnLabel{color:%3;background:transparent;border:none;font-size:%5px;font-weight:700;}"
               "QLabel#eqConnLabel[tone=\"info\"]{color:%7;}"
               "QLabel#eqConnLabel[tone=\"ok\"]{color:%6;}"
               "QLabel#eqConnLabel[tone=\"warn\"]{color:%8;}"
               "QLabel#eqConnLabel[tone=\"danger\"]{color:%9;}")
        .arg(c::BG_SURFACE(), c::BORDER_DIM(), c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(fincept::ui::fonts::px(-4))
        .arg(c::SUCCESS(), c::INFO(), c::WARNING(), c::DANGER());
}

} // namespace

// ── EquityFitLabel ──────────────────────────────────────────────────────────

EquityFitLabel::EquityFitLabel(QWidget* parent) : QLabel(parent) {
    setTextFormat(Qt::PlainText);
    setWordWrap(false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void EquityFitLabel::set_status_text(const QString& full, const QString& compact) {
    if (full != full_ || compact != compact_) {
        full_ = full;
        compact_ = compact;
        updateGeometry(); // the wanted width follows the full text
    }
    refit();
}

QSize EquityFitLabel::sizeHint() const {
    ensurePolished(); // the stylesheet sets the font
    return {text_width(full_), QLabel::sizeHint().height()};
}

QSize EquityFitLabel::minimumSizeHint() const {
    ensurePolished();
    return {text_width(QStringLiteral("\xE2\x97\x8F \xE2\x80\xA6")), QLabel::minimumSizeHint().height()};
}

void EquityFitLabel::resizeEvent(QResizeEvent* e) {
    QLabel::resizeEvent(e);
    refit();
}

void EquityFitLabel::changeEvent(QEvent* e) {
    QLabel::changeEvent(e);
    if (e->type() == QEvent::FontChange || e->type() == QEvent::StyleChange) {
        updateGeometry();
        refit();
    }
}

int EquityFitLabel::text_width(const QString& s) const {
    const QMargins m = contentsMargins();
    return fontMetrics().horizontalAdvance(s) + m.left() + m.right() + 2 * margin() + 2;
}

void EquityFitLabel::refit() {
    // Only the displayed text changes here; the size hints depend on full_ alone, so
    // this never feeds back into the layout.
    const QFontMetrics fm = fontMetrics();
    const int avail = std::max(0, contentsRect().width() - 2 * margin());
    const QString& compact = compact_.isEmpty() ? full_ : compact_;
    QString shown;
    if (fm.horizontalAdvance(full_) <= avail)
        shown = full_;
    else if (fm.horizontalAdvance(compact) <= avail)
        shown = compact;
    else
        shown = fm.elidedText(compact, Qt::ElideRight, avail);
    if (shown != text())
        QLabel::setText(shown);
}

// ── EquityBarButton ─────────────────────────────────────────────────────────

EquityBarButton::EquityBarButton(QWidget* parent) : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);
    setFlat(true);
}

void EquityBarButton::set_label(const QString& text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&&")); // no mnemonics: '&' is a character
    if (QPushButton::text() != escaped)
        QPushButton::setText(escaped); // QPushButton re-lays out and repaints
    refresh_accessible();
}

QString EquityBarButton::label() const {
    QString t = QPushButton::text();
    t.replace(QStringLiteral("&&"), QStringLiteral("&"));
    return t;
}

void EquityBarButton::set_hint(const QString& hint) {
    if (hint_ == hint)
        return;
    hint_ = hint;
    refresh_accessible();
    updateGeometry();
    update();
}

void EquityBarButton::refresh_accessible() {
    // Only a name this button gave itself is refreshed; a caller's own name stays.
    if (!accessibleName().isEmpty() && property("_eq_auto_name").toString() != accessibleName())
        return;
    const QString l = label();
    const QString name = hint_.isEmpty() ? l : QStringLiteral("%1 (%2)").arg(l, hint_);
    setAccessibleName(name);
    setProperty("_eq_auto_name", name);
}

QString EquityBarButton::mode() const {
    return property("mode").toString();
}

int EquityBarButton::hint_width() const {
    const QFontMetrics hm(fincept::ui::fonts::font(-5, QFont::Bold));
    return std::max(hm.height(), hm.horizontalAdvance(hint_) + 6);
}

int EquityBarButton::chevron_width() const {
    const QFontMetrics fm(fincept::ui::fonts::font(-3, QFont::DemiBold));
    return std::max(10, fm.height() / 2 + 6);
}

QSize EquityBarButton::sizeHint() const {
    const QFont f = fincept::ui::fonts::font(-3, mode().isEmpty() ? QFont::DemiBold : QFont::Bold);
    const QFontMetrics fm(f);
    const int pad = std::max(8, fm.averageCharWidth());
    int w = pad * 2 + fm.horizontalAdvance(label());
    if (!hint_.isEmpty())
        w += hint_width() + std::max(5, fm.averageCharWidth() / 2 + 2);
    if (!mode().isEmpty())
        w += fm.height() / 2 + 6; // the dot
    if (menu())
        w += chevron_width();
    const int h = std::max(24, fm.height() + 10);
    return {w, h};
}

QSize EquityBarButton::minimumSizeHint() const {
    return sizeHint();
}

bool EquityBarButton::event(QEvent* e) {
    if (e->type() == QEvent::DynamicPropertyChange) {
        updateGeometry();
        update();
    }
    return QPushButton::event(e);
}

void EquityBarButton::paintEvent(QPaintEvent*) {
    namespace c = fincept::ui::colors;
    QPainter p(this);
    const QRect r = rect();
    const QString m = mode();
    const bool hover = underMouse() && isEnabled();
    const bool down = isDown() || (menu() && menu()->isVisible());
    QColor bg = Qt::transparent;
    QColor border = QColor(c::BORDER_DIM());
    QColor fg = QColor(c::TEXT_SECONDARY());
    if (m == QLatin1String("live")) {
        // LIVE: real orders — the strongest state tone, filled.
        bg = QColor(c::DANGER());
        border = QColor(c::DANGER());
        fg = c::contrast_text_on(bg);
    } else if (m == QLatin1String("paper")) {
        bg = c::tint_color(QColor(c::SUCCESS()), hover ? 0.22 : 0.12);
        border = QColor(c::SUCCESS());
        fg = QColor(c::SUCCESS());
    } else if (hover || down) {
        border = QColor(c::BORDER_MED());
        fg = QColor(c::TEXT_PRIMARY());
        if (down)
            bg = QColor(c::BG_HOVER());
    }
    if (!isEnabled() && m.isEmpty()) {
        bg = Qt::transparent;
        border = QColor(c::BORDER_DIM());
        fg = QColor(c::TEXT_TERTIARY());
    }
    if (bg.alpha() > 0)
        p.fillRect(r, bg);
    p.setPen(border);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    if (hasFocus()) {
        p.setPen(QPen(QColor(c::AMBER_DIM()), 1, Qt::DotLine));
        p.drawRect(r.adjusted(2, 2, -3, -3));
    }

    const QFont f = fincept::ui::fonts::font(-3, m.isEmpty() ? QFont::DemiBold : QFont::Bold);
    const QFontMetrics fm(f);
    const int pad = std::max(8, fm.averageCharWidth());
    int x = r.left() + pad;
    if (!m.isEmpty()) {
        // State dot: filled for LIVE, ring for PAPER (shape, not only colour).
        const int d = std::max(6, fm.height() / 2 - 1);
        const QRectF dot(x, r.center().y() - d / 2.0 + 0.5, d, d);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (m == QLatin1String("live")) {
            p.setPen(Qt::NoPen);
            p.setBrush(fg);
        } else {
            p.setPen(QPen(fg, 1.6));
            p.setBrush(Qt::NoBrush);
        }
        p.drawEllipse(dot);
        p.setRenderHint(QPainter::Antialiasing, false);
        x += fm.height() / 2 + 6;
    }
    if (!hint_.isEmpty()) {
        const QFont hf = fincept::ui::fonts::font(-5, QFont::Bold);
        const QFontMetrics hm(hf);
        const int hw = hint_width();
        const int hh = std::min(r.height() - 6, hm.height() + 2);
        const QRect cap(x, r.top() + (r.height() - hh) / 2, hw, hh);
        QColor cap_fg = QColor(c::TEXT_TERTIARY());
        QColor cap_border = cap_fg;
        cap_border.setAlphaF(0.55f);
        p.setPen(cap_border);
        p.drawRect(cap.adjusted(0, 0, -1, -1));
        p.setFont(hf);
        p.setPen(cap_fg);
        p.drawText(cap, Qt::AlignCenter, hint_);
        x += hw + std::max(5, fm.averageCharWidth() / 2 + 2);
    }
    const int chev = menu() ? chevron_width() : 0;
    const QRect text_rect(x, r.top(), r.right() - pad - chev - x + 1, r.height());
    p.setFont(f);
    p.setPen(fg);
    p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(label(), Qt::ElideRight, text_rect.width()));
    if (chev > 0) {
        const int s = std::max(3, fm.height() / 5);
        const QPointF c0(r.right() - pad - chev / 2.0 + 2, r.center().y() + 1);
        QPainterPath tri;
        tri.moveTo(c0.x() - s, c0.y() - s / 2.0);
        tri.lineTo(c0.x() + s, c0.y() - s / 2.0);
        tri.lineTo(c0.x(), c0.y() + s / 2.0 + 1);
        tri.closeSubpath();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(tri, fg);
    }
}

// ── EquityCommandBar ────────────────────────────────────────────────────────

EquityCommandBar::EquityCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("eqCommandBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(eqcb_sheet()); // before any child exists
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 0, 0, 0);
    row->setSpacing(8);

    account_btn_ = new EquityBarButton(this);
    account_btn_->setObjectName(QStringLiteral("eqAccountBtn"));
    row->addWidget(account_btn_, 0, Qt::AlignVCenter);

    symbol_slot_ = new QWidget(this);
    auto* sl = new QHBoxLayout(symbol_slot_);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    row->addWidget(symbol_slot_, 0, Qt::AlignVCenter);

    instrument_ = new EquityFitLabel(this);
    instrument_->setObjectName(QStringLiteral("eqInstrumentLine"));
    instrument_->setAccessibleName(tr("Instrument"));
    row->addWidget(instrument_, 0, Qt::AlignVCenter);

    rule_ = new QWidget(this);
    rule_->setObjectName(QStringLiteral("eqBarRule"));
    rule_->setAttribute(Qt::WA_StyledBackground, true);
    rule_->setFixedWidth(1);
    row->addWidget(rule_);

    ribbon_ = new EquityTickerBar(this);
    ribbon_->set_symbol_visible(false); // the symbol box already names it
    row->addWidget(ribbon_, 0, Qt::AlignVCenter);

    session_ = new QLabel(this);
    session_->setObjectName(QStringLiteral("eqSessionChip"));
    session_->setAccessibleName(tr("Market session"));
    session_->hide();
    row->addWidget(session_, 0, Qt::AlignVCenter);

    mode_btn_ = new EquityBarButton(this);
    mode_btn_->setObjectName(QStringLiteral("eqModeBtn"));
    row->addWidget(mode_btn_, 0, Qt::AlignVCenter);

    conn_ = new EquityFitLabel(this);
    conn_->setObjectName(QStringLiteral("eqConnLabel"));
    row->addWidget(conn_, 0, Qt::AlignVCenter);

    // The actions: the shared bar itself (painting, key caps, "⋯" collapse). Its own
    // bottom rule lies on ours (same colour, same line), so the row reads as one bar.
    actions_ = new fincept::ui::ScreenCommandBar(this);
    row->addWidget(actions_, 1);

    // A font change alters every width: re-reserve and re-fit once styles are final.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this, [this]() {
        apply_reserve();
        refit_optional();
    });
}

void EquityCommandBar::reserve_actions(const QStringList& ids) {
    reserved_ = ids;
    apply_reserve();
}

void EquityCommandBar::apply_reserve() {
    // Room for the reserved actions plus the "⋯" button and the bar's own margins,
    // so the leading items shrink before the trader loses BUY / SELL / More.
    const QFontMetrics fm(fincept::ui::fonts::font(-2, QFont::Bold));
    int w = 10 + 8 + fm.horizontalAdvance(QStringLiteral("\xE2\x8B\xAF")) + 18; // row margins + overflow button
    for (const QString& id : reserved_) {
        if (QPushButton* b = actions_->action(id))
            w += b->sizeHint().width() + 6;
    }
    actions_->setMinimumWidth(w);
}

void EquityCommandBar::set_symbol_widget(QWidget* w) {
    if (!w)
        return;
    if (w->parentWidget() != symbol_slot_)
        w->setParent(symbol_slot_);
    symbol_slot_->layout()->addWidget(w);
}

void EquityCommandBar::set_session(const QString& short_text, const QString& full_text, bool open) {
    session_->setText(short_text);
    session_->setToolTip(full_text);
    const QString state = open ? QStringLiteral("open") : QStringLiteral("closed");
    if (state != session_state_) {
        session_state_ = state;
        session_->setProperty("state", state);
        session_->style()->unpolish(session_);
        session_->style()->polish(session_);
    }
    refit_optional();
}

void EquityCommandBar::refit_optional() {
    // The session chip is the first leading item to give way on a narrow bar (the
    // venue session is also in the status bar).
    const bool has = !session_->text().isEmpty();
    const int need = account_btn_->sizeHint().width() + symbol_slot_->minimumSizeHint().width() +
                     ribbon_->minimumSizeHint().width() + mode_btn_->sizeHint().width() +
                     conn_->minimumSizeHint().width() + session_->sizeHint().width() + 8 * 9 + 10;
    session_->setVisible(has && width() >= need + fincept::ui::fonts::font_px(0) * 10);
}

void EquityCommandBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    refit_optional();
}

} // namespace fincept::screens::equity
