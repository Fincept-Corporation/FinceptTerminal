#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

namespace fincept::ui::styles {

QString card_frame() {
    return QString(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 2px; }")
        .arg(colors::CARD_BG, colors::BORDER);
}

QString card_title() {
    return QString(
        "color: %1; font-size: 13px; font-weight: bold; background: transparent;")
        .arg(colors::GRAY);
}

QString card_close_button() {
    return QString(
        "QPushButton { color: %1; background: transparent; border: none; font-size: 12px; }"
        "QPushButton:hover { color: %2; }")
        .arg(colors::MUTED, colors::GRAY);
}

QString section_header() {
    return QString(
        "color: %1; font-size: 12px; font-weight: bold; "
        "background: transparent; padding: 2px 0; "
        "border-bottom: 1px solid %2;")
        .arg(colors::MUTED, colors::BORDER);
}

QString data_label() {
    return QString("color: %1; font-size: 13px; background: transparent;")
        .arg(colors::GRAY);
}

QString data_value() {
    return QString("color: %1; font-size: 13px; background: transparent;")
        .arg(colors::WHITE);
}

QString accent_button() {
    return QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "padding: 4px 12px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #111111; color: #FF8800; }")
        .arg(colors::PANEL, colors::ORANGE, colors::BORDER);
}

QString muted_button() {
    return QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #111111; color: %4; }")
        .arg(colors::PANEL, colors::MUTED, colors::BORDER, colors::GRAY);
}

} // namespace fincept::ui::styles
