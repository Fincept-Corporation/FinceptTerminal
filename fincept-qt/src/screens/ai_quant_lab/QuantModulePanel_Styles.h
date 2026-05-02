// src/screens/ai_quant_lab/QuantModulePanel_Styles.h
//
// Private header — included only by QuantModulePanel*.cpp translation units.
// Provides the shared style-sheet helpers that were originally file-static in
// QuantModulePanel.cpp. Each helper reads live theme tokens at call time, so
// it must be invoked at widget-creation time (not cached at startup).
#pragma once

#include "ui/theme/Theme.h"

#include <QString>

namespace fincept::screens::quant_styles {

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

inline QString tab_ss(const QString& accent) {
    return QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                   "QTabBar::tab { background:%3; color:%4; padding:6px 14px;"
                   "border:1px solid %1; border-bottom:none; margin-right:1px; }"
                   "QTabBar::tab:selected { background:%2; color:%5; font-weight:700;"
                   "border-bottom:2px solid %5; }"
                   "QTabBar::tab:hover { color:%4; background:%6; }")
        .arg(ui::colors::BORDER_DIM())
        .arg(ui::colors::BG_SURFACE())
        .arg(ui::colors::BG_RAISED())
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(accent)
        .arg(ui::colors::BG_HOVER());
}

inline QString sub_tab_ss(const QString& accent) {
    return QString("QTabWidget::pane { border:none; background:%1; }"
                   "QTabBar::tab { background:%2; color:%3; padding:5px 12px;"
                   "border:1px solid %4; border-bottom:none; margin-right:1px; }"
                   "QTabBar::tab:selected { background:%1; color:%5; font-weight:700; }"
                   "QTabBar { background:%2; }")
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

} // namespace fincept::screens::quant_styles
