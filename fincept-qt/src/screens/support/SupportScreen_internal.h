// src/screens/support/SupportScreen_internal.h
//
// Private header — included only by SupportScreen*.cpp TUs. Hosts the shared
// style/label/badge helpers so the partial-class split files don't duplicate
// them. All helpers are `inline` for safe multi-TU inclusion.

#pragma once

#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QString>
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens::support_internal {

/// The user's font as a stylesheet declaration (the terminal uses one font everywhere).
inline QString MF() {
    return QStringLiteral("font-family:%1;").arg(QString(fincept::ui::fonts::DATA_FAMILY));
}

inline QString SS_INPUT() {
    return QString("QLineEdit, QTextEdit, QComboBox {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 7px 10px; font-size: " + fincept::ui::fonts::px(-2) + "px; %4 }"
                   "QLineEdit:focus, QTextEdit:focus { border-color: %5; outline: none; }"
                   "QComboBox::drop-down { border: none; width: 22px; }"
                   "QComboBox::down-arrow { width: 10px; height: 10px; }"
                   "QComboBox QAbstractItemView {"
                   "  background: %6; color: %2; border: 1px solid %3;"
                   "  selection-background-color: %5; selection-color: %7; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_MED(),
             MF(), fincept::ui::colors::AMBER(), fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_ON_ACCENT());
}

inline QString SS_BTN_PRIMARY() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: %4; }"
                   "QPushButton:pressed { background: %5; }"
                   "QPushButton:disabled { background: %6; color: %7; }")
        .arg(fincept::ui::colors::AMBER(), fincept::ui::colors::TEXT_ON_ACCENT(), MF(), fincept::ui::colors::AMBER_DIM(),
             fincept::ui::colors::AMBER_DIM(), fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_TERTIARY());
}

inline QString SS_BTN_SUCCESS() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: %6; color: %8; }"
                   "QPushButton:pressed { background: %7; color: %8; }"
                   "QPushButton:disabled { background: %4; color: %5; }")
        .arg(fincept::ui::colors::POSITIVE(), fincept::ui::colors::TEXT_ON_ACCENT(), MF(), fincept::ui::colors::BG_RAISED(),
             fincept::ui::colors::TEXT_TERTIARY(), fincept::ui::colors::POSITIVE_DIM(), fincept::ui::colors::POSITIVE_BG(),
             fincept::ui::colors::TEXT_PRIMARY());
}

inline QString SS_BTN_DANGER() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(fincept::ui::colors::NEGATIVE(), MF(), fincept::ui::colors::TEXT_ON_ACCENT(), fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_OUTLINE() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(fincept::ui::colors::POSITIVE(), MF(), fincept::ui::colors::TEXT_ON_ACCENT(), fincept::ui::colors::TEXT_DIM());
}

inline QString SS_BTN_GHOST() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                   "  padding: 5px 12px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 500; %3 }"
                   "QPushButton:hover { background: %2; color: %4; }"
                   "QPushButton:pressed { background: %5; }")
        .arg(fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_MED(), MF(),
             fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_DIM());
}

/// `px` is the size at the default font size; the label follows the user's.
inline QLabel* lbl(const QString& text, const QString& color, int px = 12, bool bold = false, bool wrap = false) {
    auto* l = new QLabel(text);
    l->setWordWrap(wrap);
    l->setStyleSheet(QString("color:%1;font-size:%2px;background:transparent;%3%4")
                         .arg(color)
                         .arg(fincept::ui::fonts::px(px - fincept::ui::ThemeManager::kDefaultFontPx))
                         .arg(MF())
                         .arg(bold ? "font-weight:600;" : ""));
    return l;
}

/// Status line label (dot or text) whose colour follows its "tone" property:
/// "" = ready (success), "busy" (warning), "bad" (danger), "dim" (tertiary).
/// The sheet is set once; support_set_tone() only flips the property.
inline QLabel* status_label_widget(const QString& text, int size_offset, bool bold) {
    auto* l = new QLabel(text);
    l->setProperty("tone", QString());
    l->setStyleSheet(QString("QLabel{color:%1;font-size:%2px;background:transparent;%3%4}"
                             "QLabel[tone=\"busy\"]{color:%5;}"
                             "QLabel[tone=\"bad\"]{color:%6;}"
                             "QLabel[tone=\"dim\"]{color:%7;}")
                         .arg(fincept::ui::colors::SUCCESS())
                         .arg(fincept::ui::fonts::px(size_offset))
                         .arg(MF())
                         .arg(bold ? "font-weight:600;" : "")
                         .arg(fincept::ui::colors::WARNING(), fincept::ui::colors::DANGER(),
                              fincept::ui::colors::TEXT_TERTIARY()));
    return l;
}

/// Re-polish `w` only when its tone actually changes.
inline void support_set_tone(QWidget* w, const char* tone) {
    if (!w)
        return;
    const QString t = QString::fromLatin1(tone);
    if (w->property("tone").toString() == t)
        return;
    w->setProperty("tone", t);
    if (QStyle* st = w->style()) {
        st->unpolish(w);
        st->polish(w);
    }
}

/// Remove every item of `lay` except the trailing stretch. The old
/// `delete lay->takeAt(0)->widget()` freed the widget but leaked the
/// QLayoutItem, and deleted widgets synchronously from inside slots (P13).
inline void clear_layout_keep_stretch(QVBoxLayout* lay) {
    if (!lay)
        return;
    while (lay->count() > 1) {
        QLayoutItem* item = lay->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
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
    b->setFixedHeight(qMax(20, QFontMetrics(fincept::ui::fonts::font(-5)).height() + 4));
    b->setStyleSheet(QString("background:%1;color:%2;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;"
                             "padding:0 8px;letter-spacing:0.5px;%3")
                         .arg(bg, fg, MF()));
    return b;
}

} // namespace fincept::screens::support_internal
