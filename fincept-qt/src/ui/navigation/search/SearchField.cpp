// SearchField.cpp — see SearchField.h.
#include "ui/navigation/search/SearchField.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeIcon.h"

#include <QAction>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFrame>

#include <algorithm>

namespace fincept::ui::search {

SearchField::SearchField(QWidget* parent) : QLineEdit(parent) {
    setObjectName(QStringLiteral("cmdSearchField"));
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    glyph_ = addAction(theme_icon(ThemeGlyph::Search), QLineEdit::LeadingPosition);
    glyph_->setToolTip(tr("Search"));
    clear_ = addAction(theme_icon(ThemeGlyph::Close), QLineEdit::TrailingPosition);
    clear_->setToolTip(tr("Clear"));
    clear_->setVisible(false);
    connect(clear_, &QAction::triggered, this, [this]() {
        clear();
        setFocus(Qt::OtherFocusReason);
    });
    connect(this, &QLineEdit::textChanged, this, [this](const QString&) { update_clear_action(); });
    refit();
}

void SearchField::set_shortcut_hint(const QString& hint) {
    if (hint_ == hint)
        return;
    hint_ = hint;
    update();
}

void SearchField::update_clear_action() {
    const bool show = !text().isEmpty();
    if (clear_->isVisible() != show)
        clear_->setVisible(show);
}

void SearchField::refit() {
    const QString css =
        QStringLiteral("QLineEdit#cmdSearchField{background:%1;color:%2;border:1px solid %3;border-radius:2px;"
                       "padding:0 4px;font-size:%4px;selection-background-color:%5;selection-color:%6;}"
                       "QLineEdit#cmdSearchField:hover{border-color:%7;}"
                       "QLineEdit#cmdSearchField:focus{border-color:%8;background:%9;}")
            .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED())
            .arg(fonts::px(-1))
            .arg(colors::AMBER_DIM(), colors::TEXT_ON_ACCENT(), colors::BORDER_BRIGHT(), colors::AMBER(),
                 colors::BG_SURFACE());
    if (css != applied_css_) {
        applied_css_ = css;
        setStyleSheet(css);
    }
    // Fits the toolbar row at base 9 / 14 / 24 (ToolBar: 32 px at 14, growing 1:1 with the font).
    const int text_h = QFontMetrics(fonts::font(-1)).height();
    const int h = text_h + std::max(6, text_h / 4);
    if (height() != h)
        setFixedHeight(h);
    const QString placeholder = tr("Search symbols, screens, actions…");
    if (placeholderText() != placeholder)
        setPlaceholderText(placeholder);
    setAccessibleName(tr("Search"));
    setAccessibleDescription(tr("Type a symbol, a screen, a setting or what you want to do. Results appear below."));
}

void SearchField::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        glyph_->setToolTip(tr("Search"));
        clear_->setToolTip(tr("Clear"));
        refit();
    }
    QLineEdit::changeEvent(e);
}

bool SearchField::claims(const QKeyEvent* e) const {
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    const bool plain = (mods & ~Qt::ShiftModifier) == 0;
    switch (e->key()) {
        case Qt::Key_Escape:
            return mods == Qt::NoModifier; // also closes / restores focus with the popup shut
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            return popup_open_ && plain;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return popup_open_ && (mods == Qt::NoModifier || mods == Qt::ControlModifier || mods == Qt::ShiftModifier);
        default:
            return false;
    }
}

bool SearchField::event(QEvent* e) {
    if (e->type() == QEvent::ShortcutOverride && claims(static_cast<QKeyEvent*>(e))) {
        e->accept();
        return true;
    }
    // Tab must not walk focus while the popup is open.
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if ((ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) && popup_open_) {
            keyPressEvent(ke);
            return true;
        }
    }
    return QLineEdit::event(e);
}

void SearchField::keyPressEvent(QKeyEvent* e) {
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    switch (e->key()) {
        case Qt::Key_Escape:
            if (mods == Qt::NoModifier) {
                emit escape();
                e->accept();
                return;
            }
            break;
        case Qt::Key_Up:
        case Qt::Key_Down:
            if (popup_open_) {
                emit navigate(e->key() == Qt::Key_Up ? -1 : 1);
                e->accept();
                return;
            }
            if (e->key() == Qt::Key_Down) {
                emit navigate(1); // opens the popup on the empty / current text
                e->accept();
                return;
            }
            break;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            if (popup_open_) {
                emit navigate(e->key() == Qt::Key_PageUp ? -8 : 8);
                e->accept();
                return;
            }
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit accept(mods);
            e->accept();
            return;
        case Qt::Key_Tab:
            if (popup_open_) {
                emit more_actions();
                e->accept();
                return;
            }
            break;
        case Qt::Key_Backtab:
            if (popup_open_) {
                emit navigate(-1);
                e->accept();
                return;
            }
            break;
        case Qt::Key_Right:
            // → at the end of the text opens the row's actions; anywhere else it moves the cursor.
            if (popup_open_ && mods == Qt::NoModifier && cursorPosition() == text().size() && !hasSelectedText()) {
                emit more_actions();
                e->accept();
                return;
            }
            break;
        default:
            break;
    }
    QLineEdit::keyPressEvent(e);
}

void SearchField::focusInEvent(QFocusEvent* e) {
    QLineEdit::focusInEvent(e);
    update();
    emit focus_entered(e->reason());
}

void SearchField::focusOutEvent(QFocusEvent* e) {
    QLineEdit::focusOutEvent(e);
    update();
    emit focus_left(e->reason());
}

void SearchField::paintEvent(QPaintEvent* e) {
    QLineEdit::paintEvent(e);
    if (hint_.isEmpty() || hasFocus() || !text().isEmpty())
        return;
    // The key that opens the box, right-aligned and dim, so mouse and keyboard users both see it.
    QPainter p(this);
    QFont f = fonts::font(-4, QFont::DemiBold);
    p.setFont(f);
    const QFontMetrics fm(f);
    const int pad = fm.averageCharWidth();
    const int w = fm.horizontalAdvance(hint_) + 2 * pad;
    if (w * 3 > width())
        return; // too narrow: the placeholder matters more
    QRect r(width() - w - pad, (height() - fm.height() - 2) / 2, w, fm.height() + 2);
    r = QStyle::visualRect(layoutDirection(), rect(), r);
    p.setPen(QColor(colors::BORDER_MED()));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setPen(QColor(colors::TEXT_DIM()));
    p.drawText(r, Qt::AlignCenter, hint_);
}

} // namespace fincept::ui::search
