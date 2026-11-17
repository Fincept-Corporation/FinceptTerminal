#pragma once
// MarketsStyle — the Markets screen's stylesheet builders and the one helper every
// setStyleSheet in the screen goes through (skips the call when the text is
// unchanged: re-setting a sheet re-styles the whole subtree).
//
// Every sheet is built from ui::colors tokens and ui::fonts sizes at call time
// (never a static QString), set once when its widget is created; after that the
// central re-style pass (ThemeRestyle.cpp) keeps colours and font sizes in step.
// Header-only: no translation unit of its own.

#include "ui/theme/Theme.h"

#include <QColor>
#include <QString>
#include <QWidget>

namespace fincept::screens {

/// Selected-row wash: the accent at this alpha (Portfolio blotter's selection).
inline constexpr double kMarketsSelectionAlpha = 0.14;

inline void markets_set_sheet(QWidget* w, const QString& sheet) {
    if (w && w->styleSheet() != sheet)
        w->setStyleSheet(sheet);
}

inline QString markets_selection_rgba() {
    return ui::colors::alpha(QColor(ui::colors::AMBER()), kMarketsSelectionAlpha);
}

/// Context / drop-down menus (one definition for the whole screen).
inline QString markets_menu_css() {
    return QStringLiteral("QMenu{background:%1;border:1px solid %2;color:%3;font-family:%4;font-size:%5px;}"
                          "QMenu::item{padding:4px 18px 4px 14px;}"
                          "QMenu::item:selected{background:%6;}"
                          "QMenu::item:disabled{color:%7;}"
                          "QMenu::separator{height:1px;background:%8;margin:4px 8px;}"
                          "QMenu::indicator{width:12px;height:12px;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(),
             QString(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::px(-3))
        .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM());
}

/// Thin (8 px) vertical scroll bars for the panel tables.
inline QString markets_scrollbar_css() {
    return QStringLiteral("QScrollBar:vertical{width:8px;background:transparent;margin:0;border:none;}"
                          "QScrollBar::handle:vertical{background:%1;min-height:24px;}"
                          "QScrollBar::handle:vertical:hover{background:%2;}"
                          "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;border:none;}"
                          "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}")
        .arg(ui::colors::BORDER_MED(), ui::colors::BORDER_BRIGHT());
}

/// Flat bordered buttons of a bar (Watchlist's TABLE / MAP / DETAIL look). A button
/// with the dynamic property active="true" (or checked) is the accent one. Selector
/// based, so one sheet on the bar root styles every button in it.
inline QString markets_bar_buttons_css() {
    return QStringLiteral(
               "QPushButton,QToolButton{background:transparent;color:%1;border:1px solid %2;padding:2px 10px;"
               "font-family:%3;font-size:%4px;font-weight:600;}"
               "QPushButton:hover,QToolButton:hover{color:%5;border-color:%6;}"
               "QPushButton:checked,QToolButton:checked,QPushButton[active=\"true\"]{color:%7;border-color:%7;"
               "background:%8;}"
               "QPushButton:disabled,QToolButton:disabled{color:%9;}"
               "QToolButton::menu-indicator{image:none;width:0;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), QString(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::px(-3))
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::ACCENT_BG(),
             ui::colors::TEXT_TERTIARY());
}

} // namespace fincept::screens
