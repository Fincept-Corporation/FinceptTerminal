#pragma once
// SettingsStyles.h — shared token-based style builders for SettingsScreen
// sub-sections. All helpers read live ThemeManager tokens — no font-size
// overrides (global QSS handles font). Inline so no .cpp is required.

#include "ui/theme/Theme.h"

#include <QString>

namespace fincept::screens::settings_styles {

inline QString section_title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::AMBER());
}

inline QString sub_title_ss() {
    return QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::TEXT_SECONDARY());
}

inline QString label_ss() {
    return QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY());
}

inline QString nav_btn_ss() {
    return QString("QPushButton{background:transparent;color:%1;border:none;text-align:left;padding:0 12px;}"
                   "QPushButton:hover{background:%2;color:%3;}"
                   "QPushButton:checked{color:%4;background:%2;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_PRIMARY(), ui::colors::AMBER());
}

inline QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER());
}

inline QString btn_primary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:none;font-weight:700;padding:0 16px;height:32px;}"
                   "QPushButton:hover{background:%3;}")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM());
}

inline QString btn_secondary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;height:32px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_BRIGHT(), ui::colors::BG_HOVER());
}

inline QString btn_danger_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE(),
             ui::colors::BORDER_DIM(), ui::colors::BG_HOVER());
}

inline QString combo_ss() {
    return QString(
               "QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 8px;min-width:160px;}"
               "QComboBox:focus{border:1px solid %4;}"
               "QComboBox::drop-down{border:none;width:20px;}"
               "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%5;border:1px solid %3;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::BG_HOVER());
}

inline QString check_ss() {
    return QString("QCheckBox{color:%1;background:transparent;}"
                   "QCheckBox::indicator{width:14px;height:14px;}"
                   "QCheckBox::indicator:unchecked{border:1px solid %2;background:%3;}"
                   "QCheckBox::indicator:checked{border:1px solid %4;background:%4;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_BRIGHT(),
             ui::colors::BG_RAISED(), ui::colors::AMBER());
}

} // namespace fincept::screens::settings_styles
