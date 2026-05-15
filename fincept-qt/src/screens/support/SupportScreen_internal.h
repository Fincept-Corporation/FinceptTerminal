// src/screens/support/SupportScreen_internal.h
//
// Private header — included only by SupportScreen*.cpp TUs. Hosts the shared
// style/label/badge helpers so the partial-class split files don't duplicate
// them. All helpers are `inline` for safe multi-TU inclusion.

#pragma once

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QLabel>
#include <QString>

namespace fincept::screens::support_internal {

inline constexpr const char* MF = "font-family:'Inter','Segoe UI','Consolas',monospace;";

inline QString SS_INPUT() {
    return QString("QLineEdit, QTextEdit, QComboBox {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 7px 10px; font-size: 12px; %4 }"
                   "QLineEdit:focus, QTextEdit:focus { border-color: %5; outline: none; }"
                   "QComboBox::drop-down { border: none; width: 22px; }"
                   "QComboBox::down-arrow { width: 10px; height: 10px; }"
                   "QComboBox QAbstractItemView {"
                   "  background: %6; color: %2; border: 1px solid %3;"
                   "  selection-background-color: %5; selection-color: %7; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
             fincept::ui::colors::BORDER_MED(), MF, fincept::ui::colors::AMBER(),
             fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BG_BASE());
}

inline QString SS_BTN_PRIMARY() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: 12px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: %4; }"
                   "QPushButton:pressed { background: %5; }"
                   "QPushButton:disabled { background: %6; color: %7; }")
        .arg(fincept::ui::colors::AMBER(), fincept::ui::colors::BG_BASE(), MF,
             fincept::ui::colors::AMBER_DIM(), "#92400e",
             fincept::ui::colors::BORDER_MED(), fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_SUCCESS() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: 12px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: #15803d; }"
                   "QPushButton:pressed { background: #14532d; }"
                   "QPushButton:disabled { background: %4; color: %5; }")
        .arg(fincept::ui::colors::POSITIVE(), fincept::ui::colors::BG_BASE(), MF,
             fincept::ui::colors::BORDER_MED(), fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_DANGER() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: 11px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(fincept::ui::colors::NEGATIVE(), MF, fincept::ui::colors::BG_BASE(),
             fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_OUTLINE() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: 11px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(fincept::ui::colors::POSITIVE(), MF, fincept::ui::colors::BG_BASE(),
             fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_GHOST() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                   "  padding: 5px 12px; font-size: 11px; font-weight: 500; %3 }"
                   "QPushButton:hover { background: %2; color: %4; }"
                   "QPushButton:pressed { background: %5; }")
        .arg(fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_MED(), MF,
             fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_DIM());
}

inline QLabel* lbl(const QString& text, const QString& color, int px = 12, bool bold = false, bool wrap = false) {
    auto* l = new QLabel(text);
    l->setWordWrap(wrap);
    l->setStyleSheet(QString("color:%1;font-size:%2px;background:transparent;%3%4")
                         .arg(color)
                         .arg(px)
                         .arg(MF)
                         .arg(bold ? "font-weight:600;" : ""));
    return l;
}

inline QFrame* hsep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background:%1;border:none;").arg(fincept::ui::colors::BORDER_DIM()));
    return f;
}

inline QLabel* status_badge(const QString& text, const QString& bg, const QString& fg) {
    auto* b = new QLabel(text);
    b->setAlignment(Qt::AlignCenter);
    b->setFixedHeight(20);
    b->setStyleSheet(QString("background:%1;color:%2;font-size:9px;font-weight:700;"
                             "padding:0 8px;letter-spacing:0.5px;%3")
                         .arg(bg, fg, MF));
    return b;
}

} // namespace fincept::screens::support_internal
