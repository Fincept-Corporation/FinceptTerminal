// src/screens/ai_quant_lab/QuantModulePanel_Styles.h
//
// Private header — included only by QuantModulePanel*.cpp translation units.
// Provides the shared style-sheet helpers that were originally file-static in
// QuantModulePanel.cpp. Each helper reads live theme tokens at call time, so
// it must be invoked at widget-creation time (not cached at startup).
// Module colours (QuantModule::color) are category colours, not tokens: they
// are always passed through ui::colors::legible() / tint() before use.
#pragma once

#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QHeaderView>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fincept::screens::quant_styles {

/// Dynamic properties on a push button styled by accent_button_ss(): the
/// button's own shape rules (padding, radius, weight) and, when it sets one, its
/// font-size offset from the base size — kept so QuantModulePanel can rebuild
/// the hover / pressed shades when the theme or font changes.
inline constexpr const char* kAccentShapeProp = "quantAccentShape";
inline constexpr const char* kAccentFontProp = "quantAccentFont";

/// A text-bearing height (table row, header strip) that is `px_at_default` at
/// the default font size and moves 1:1 with the user's base size, so rows sized
/// for a 14px font never clip their text at a larger one.
inline int row_px(int px_at_default) {
    return ui::fonts::font_px(px_at_default - ui::ThemeManager::kDefaultFontPx);
}

/// Dynamic property holding the default-size width given to set_text_width().
inline constexpr const char* kTextWidthProp = "quantTextWidth";

/// Fixed width for a widget sized for its text: `px_at_default` at the default
/// font size, scaled with the user's base size so the text is not clipped at a
/// larger one. QuantModulePanel re-applies it when the font changes.
inline void set_text_width(QWidget* w, int px_at_default) {
    w->setProperty(kTextWidthProp, px_at_default);
    w->setFixedWidth(px_at_default * ui::fonts::font_px() / ui::ThemeManager::kDefaultFontPx);
}

/// Dynamic property on a chart series drawn in a module colour: the source
/// QColor, re-adapted to the active theme when the theme changes.
inline constexpr const char* kSeriesColorProp = "quantSeriesColor";

/// WCAG contrast ratio of two colours (1 = identical, 21 = black on white).
inline double contrast_ratio(const QColor& a, const QColor& b) {
    const auto lum = [](const QColor& c) {
        const auto ch = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
    };
    const double la = lum(a);
    const double lb = lum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

/// Push button filled with a module colour (a category colour, not a token).
/// The fill is that colour made legible on the active theme — stepped further
/// (lighter on the dark theme, darker on the light one) until TEXT_ON_ACCENT
/// reads on it at 4.5:1 — and hover / pressed step away from the fill.
/// `shape` carries the button's own rules; a valid `font_offset` adds a font
/// size that many px from the base size.
inline QString accent_button_ss(const QColor& color, const QString& shape, const QVariant& font_offset = {}) {
    QColor fill = ui::colors::legible_color(color);
    const bool dark = ui::colors::is_dark();
    const QColor text(ui::colors::TEXT_ON_ACCENT());
    for (int i = 0; i < 10 && contrast_ratio(fill, text) < 4.5; ++i)
        fill = dark ? fill.lighter(110) : fill.darker(110);
    const QColor hover = dark ? fill.lighter(115) : fill.darker(120);
    const QColor pressed = dark ? fill.darker(110) : fill.darker(140);
    const QString font =
        font_offset.isValid() ? "font-size:" + ui::fonts::px(font_offset.toInt()) + "px;" : QString();
    return QString("QPushButton { background:%1; color:%2; %3 %4 border-color:%1; }"
                   "QPushButton:hover { background:%5; border-color:%5; }"
                   "QPushButton:pressed { background:%6; border-color:%6; }")
        .arg(fill.name(), ui::colors::TEXT_ON_ACCENT(), shape, font, hover.name(), pressed.name());
}

inline QString input_ss() {
    return QString("QLineEdit,QTextEdit { background:%1; color:%2; border:1px solid %3;"
                   "padding:4px 6px; border-radius:2px; }"
                   "QLineEdit:focus,QTextEdit:focus { border-color:%4; }")
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED())
        .arg(ui::colors::BORDER_BRIGHT());
}

inline QString combo_ss() {
    return QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                   "padding:3px 6px; border-radius:2px; }"
                   "QComboBox:focus { border-color:%4; }"
                   "QComboBox::drop-down { border:none; width:18px; }"
                   "QComboBox QAbstractItemView { background:%1; color:%2;"
                   "selection-background-color:%5; border:1px solid %3; }")
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED())
        .arg(ui::colors::BORDER_BRIGHT())
        .arg(ui::colors::BG_HOVER());
}

inline QString spinbox_ss() {
    return QString("QSpinBox,QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                   "padding:4px 6px; border-radius:2px; }"
                   "QSpinBox:focus,QDoubleSpinBox:focus { border-color:%4; }"
                   "QSpinBox::up-button,QSpinBox::down-button,"
                   "QDoubleSpinBox::up-button,QDoubleSpinBox::down-button { width:16px; }")
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED())
        .arg(ui::colors::BORDER_BRIGHT());
}

inline QString dateedit_ss() {
    return QString("QDateEdit { background:%1; color:%2; border:1px solid %3;"
                   "padding:4px 6px; border-radius:2px; }"
                   "QDateEdit:focus { border-color:%4; }"
                   "QDateEdit::drop-down { border:none; width:18px; }"
                   "QCalendarWidget QWidget { background:%1; color:%2; }"
                   "QCalendarWidget QAbstractItemView { background:%1; color:%2;"
                   "selection-background-color:%5; selection-color:%2; }"
                   "QCalendarWidget QToolButton { background:%1; color:%2; }"
                   "QCalendarWidget QToolButton:hover { background:%5; }")
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED())
        .arg(ui::colors::BORDER_BRIGHT())
        .arg(ui::colors::BG_HOVER());
}

inline QString tab_ss(const QString& accent) {
    return QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                   "QTabBar::tab { background:%3; color:%4; padding:6px 14px;"
                   "border:1px solid %1; border-bottom:none; margin-right:1px; }"
                   "QTabBar::tab:selected { background:%2; color:%5; font-weight:700;"
                   "border-bottom:2px solid %5; }"
                   "QTabBar::tab:hover { color:%4; background:%6; }"
                   // Scroll arrows for a tab row wider than the panel (Statsmodels has 11 tabs): the
                   // default buttons are drawn dark on dark and cannot be seen.
                   "QTabBar::scroller { width:44px; }"
                   "QTabBar QToolButton { background:%3; color:%7; border:1px solid %8; }"
                   "QTabBar QToolButton:hover { border-color:%5; }")
        .arg(ui::colors::BORDER_DIM())
        .arg(ui::colors::BG_SURFACE())
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(accent)
        .arg(ui::colors::BG_HOVER())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED());
}

inline QString sub_tab_ss(const QString& accent) {
    return QString("QTabWidget::pane { border:none; background:%1; }"
                   "QTabBar::tab { background:%2; color:%3; padding:5px 12px;"
                   "border:1px solid %4; border-bottom:none; margin-right:1px; }"
                   "QTabBar::tab:selected { background:%1; color:%5; font-weight:700; }"
                   "QTabBar { background:%2; }"
                   "QTabBar::scroller { width:44px; }"
                   "QTabBar QToolButton { background:%2; color:%3; border:1px solid %4; }"
                   "QTabBar QToolButton:hover { border-color:%5; }")
        .arg(ui::colors::BG_SURFACE())
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(ui::colors::BORDER_DIM())
        .arg(accent);
}

inline QString output_ss() {
    return QString("QTextEdit { background:%1; color:%2; border:1px solid %3; padding:8px; }")
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_DIM());
}

inline QString table_ss() {
    return QString("QTableWidget { background:%1; color:%2; border:1px solid %3;"
                   "gridline-color:%3; alternate-background-color:%4; }"
                   "QTableWidget::item { padding:3px 6px; }"
                   "QHeaderView::section { background:%5; color:%6; font-weight:700;"
                   "padding:4px 8px; border:none; border-bottom:1px solid %3; }"
                   "QTableWidget::item:selected { background:%7; color:%2; }")
        .arg(ui::colors::BG_SURFACE())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_DIM())
        .arg(ui::colors::ROW_ALT())
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(ui::colors::BG_HOVER());
}

/// A right-aligned numeric cell that SORTS by `v` while showing `text` (a plain
/// QTableWidgetItem compares its formatted text, so "10%" sorted before "9%").
/// `color` (a theme token value, optional) is the foreground.
inline QTableWidgetItem* num_item(const QString& text, double v, const QString& color = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, v);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (!color.isEmpty())
        it->setForeground(QColor(color));
    return it;
}

/// Turn header-click sorting on once a table is filled. The rows stay in the
/// order they were filled in until the user clicks a header (sorting is
/// enabled with no sort column, so nothing is re-ordered by default).
inline void finish_table(QTableWidget* t) {
    t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    t->setSortingEnabled(true);
    t->horizontalHeader()->setSortIndicatorShown(true);
}

} // namespace fincept::screens::quant_styles
