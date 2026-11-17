// src/screens/maritime/MaritimeScreen_internal.h
//
// Private header — included only by MaritimeScreen*.cpp TUs. Hosts the token
// formatters (C/F), the screen's ONE root stylesheet and the role / tone helpers.
//
// Styling model: MaritimeScreen::apply_theme() sets maritime_root_ss() on the
// screen once (and again on theme change). Widgets carry an objectName or a
// dynamic "mtRole" property (primary / outline / toolbar buttons; section, tiny,
// detail, hint, ... labels) and, for state colours, a "tone" property
// (positive / amber / negative / muted). This replaced ~80 per-widget inline
// stylesheets — each a separate CSS parse — and stops the old bare
// "background: ...; border-bottom: ..." panel sheets cascading a border onto
// every child label.

#pragma once

#include "ui/theme/Theme.h"

#include <QColor>
#include <QFontMetrics>
#include <QString>
#include <QStyle>
#include <QWidget>

namespace fincept::screens::maritime_internal {

// Token → QString — works for both ColorToken and FontFamilyToken.
inline QString C(const fincept::ui::ColorToken& t) {
    return QString::fromLatin1(t.get());
}
inline QString F(const fincept::ui::fonts::FontFamilyToken& t) {
    return QString::fromUtf8(t.get()); // the user's font family may be any Unicode name
}

/// Map markers sit on the basemap imagery (satellite / dark tiles), not on a
/// theme surface, so they take the bright colour of the dark preset whatever
/// the active theme: e.g. mt_marker_color(&ui::ThemeTokens::accent).
inline QColor mt_marker_color(const char* fincept::ui::ThemeTokens::*field) {
    return QColor(fincept::ui::THEME_OBSIDIAN.*field);
}

/// Width of `chars` characters in the user's font at `offset` px from the base
/// size — for panel and control widths that must grow with the font.
inline int mt_text_width(int chars, int offset, int weight = QFont::Normal) {
    return QFontMetrics(fincept::ui::fonts::font(offset, weight)).horizontalAdvance(QString(chars, QLatin1Char('M')));
}

/// Item-data role on a vessel-table name cell: true when its position fix is
/// stale (drawn in the accent colour; re-applied on theme change).
inline constexpr int kMtStaleRole = Qt::UserRole + 1;

/// Text colour of a vessel-table name cell.
inline QColor mt_name_color(bool stale) {
    return QColor(stale ? fincept::ui::colors::AMBER() : fincept::ui::colors::TEXT_PRIMARY());
}

/// Set a dynamic style property and re-polish so the root sheet's selector applies.
inline void mt_set_style_prop(QWidget* w, const char* name, const char* value) {
    if (!w)
        return;
    w->setProperty(name, QString::fromLatin1(value));
    w->style()->unpolish(w);
    w->style()->polish(w);
}
inline void mt_set_role(QWidget* w, const char* role) {
    if (w)
        w->setProperty("mtRole", QString::fromLatin1(role)); // set before first show: no re-polish needed
}
inline void mt_set_tone(QWidget* w, const char* tone) {
    mt_set_style_prop(w, "tone", tone);
}

inline QString maritime_root_ss() {
    using namespace fincept::ui;
    const QString f = F(fonts::DATA_FAMILY);
    QString ss;
    // ── Surfaces ────────────────────────────────────────────────────────────
    ss += QString("#maritimeScreen { background:%1; }"
                  "#mtTopBar { background:%2; border-bottom:1px solid %3; }"
                  "#mtLeftPanel { background:%4; border-right:1px solid %5; }"
                  "#mtRightPanel { background:%4; border-left:1px solid %5; }"
                  "#mtStatusBar { background:%4; border-top:1px solid %5; }"
                  "#mtMapToolbar { background:%1; border-bottom:1px solid %5; }"
                  "#mtStatBox { background:%1; border:1px solid %3; border-radius:2px; }"
                  "#mtRouteCard, #mtSearchCard { background:%4; border:1px solid %6; border-left:3px solid %6;"
                  " border-radius:2px; }"
                  "QSplitter::handle { background:%5; }"
                  "QScrollArea { border:none; background:transparent; }"
                  "QLabel#mtAccent { background:%6; border-radius:1px; }")
              .arg(C(colors::BG_BASE), C(colors::BG_RAISED), C(colors::BORDER_MED), C(colors::BG_SURFACE),
                   C(colors::BORDER_DIM), C(colors::AMBER));
    // ── Tables / inputs / combo ─────────────────────────────────────────────
    ss += QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                  "font-family:%4; font-size:%5px; border:none; outline:none; }"
                  "QTableWidget::item { padding:3px 8px; border:none; }"
                  "QTableWidget::item:selected { background:%6; color:%7; }"
                  "QTableWidget::item:alternate { background:%8; }"
                  "QHeaderView::section { background:%9; color:%10; font-weight:700;"
                  "padding:5px 8px; border:none; border-bottom:1px solid %11;"
                  "font-family:%4; font-size:%5px; letter-spacing:0.5px; }")
              .arg(C(colors::BG_BASE), C(colors::TEXT_PRIMARY), C(colors::BG_RAISED), f)
              .arg(fonts::SMALL.get())
              .arg(C(colors::BG_HOVER), C(colors::AMBER), C(colors::ROW_ALT), C(colors::BG_RAISED),
                   C(colors::TEXT_SECONDARY), C(colors::BORDER_MED));
    ss += QString("QLineEdit, QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                  "font-family:%4; font-size:%5px; padding:6px 8px; border-radius:2px; }"
                  "QLineEdit:focus, QDoubleSpinBox:focus { border-color:%6; }"
                  "QComboBox { background:%7; color:%2; border:1px solid %3; border-radius:2px;"
                  "font-family:%4; font-size:%5px; font-weight:700; padding:3px 8px; }"
                  "QComboBox:hover, QComboBox:focus { border-color:%6; }"
                  "QComboBox::drop-down { border:none; width:16px; }"
                  "QComboBox::down-arrow { image:none; width:0; height:0;"
                  "border-left:3px solid transparent; border-right:3px solid transparent;"
                  "border-top:4px solid %6; margin-right:6px; }"
                  "QComboBox QAbstractItemView { background:%7; color:%2; border:1px solid %3;"
                  "selection-background-color:%8; selection-color:%6; outline:0; font-family:%4; font-size:%5px; }")
              .arg(C(colors::BG_SURFACE), C(colors::TEXT_PRIMARY), C(colors::BORDER_MED), f)
              .arg(fonts::SMALL.get())
              .arg(C(colors::AMBER), C(colors::BG_RAISED), C(colors::BG_HOVER));
    // ── Buttons by role ─────────────────────────────────────────────────────
    ss += QString("QPushButton[mtRole=\"primary\"], QPushButton#mtPlansBtn { background:%1; color:%2; font-family:%3;"
                  " font-size:%4px; font-weight:700; border:none; padding:8px; border-radius:2px; letter-spacing:1px; }"
                  "QPushButton[mtRole=\"primary\"]:hover, QPushButton#mtPlansBtn:hover { background:%5; }"
                  "QPushButton#mtPlansBtn { padding:4px 10px; }"
                  "QPushButton[mtRole=\"outline\"] { background:transparent; color:%6; font-family:%3; font-size:%4px;"
                  " font-weight:700; border:1px solid %7; padding:7px; border-radius:2px; }"
                  "QPushButton[mtRole=\"outline\"]:hover, QPushButton[mtRole=\"outline\"]:checked"
                  " { background:%8; color:%1; border-color:%1; }"
                  "QPushButton[mtRole=\"toolbar\"] { background:transparent; color:%6; border:none;"
                  " font-family:%3; font-size:%4px; font-weight:700; padding:3px 8px; }"
                  "QPushButton[mtRole=\"toolbar\"]:hover, QPushButton[mtRole=\"toolbar\"]:checked { color:%1; }")
              .arg(C(colors::AMBER), C(colors::TEXT_ON_ACCENT), f)
              .arg(fonts::SMALL.get())
              .arg(C(colors::AMBER_DIM), C(colors::TEXT_SECONDARY), C(colors::BORDER_BRIGHT), C(colors::BG_HOVER));
    // ── Labels by role / name ───────────────────────────────────────────────
    ss += QString("QLabel#mtBrand { color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:2px; }"
                  "QLabel#mtCenterTitle { color:%1; font-size:%4px; font-weight:700; font-family:%3; letter-spacing:1px; }"
                  "QLabel#mtCredits, QLabel#mtVesselCount { color:%5; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; font-family:%3;"
                  " padding:4px 10px; background:%6; border:1px solid %7; border-radius:2px; }"
                  "QLabel#mtVesselCount { color:%1; }"
                  "QLabel[mtRole=\"section\"] { color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %8; }"
                  "QLabel[mtRole=\"tiny\"], QLabel[mtRole=\"statCap\"] { color:%5; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700;"
                  " font-family:%3; letter-spacing:1px; }"
                  "QLabel[mtRole=\"statVal\"] { color:%9; font-size:" + fincept::ui::fonts::px(4) + "px; font-weight:700; font-family:%3; }"
                  "QLabel[mtRole=\"detail\"] { color:%5; font-size:" + fincept::ui::fonts::px(-2) + "px; font-family:%3; }"
                  "QLabel[mtRole=\"hint\"] { color:%10; font-size:" + fincept::ui::fonts::px(-5) + "px; font-family:%3; }"
                  "QLabel[mtRole=\"cardTitle\"] { color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; font-family:%3; }"
                  "QLabel[mtRole=\"cardCaption\"] { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; }"
                  "QLabel[mtRole=\"sbCap\"] { color:%10; font-size:" + fincept::ui::fonts::px(-4) + "px; font-family:%3; }"
                  "QLabel[mtRole=\"sbVal\"] { color:%9; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; font-family:%3; }"
                  "QLabel#mtStatus { font-size:" + fincept::ui::fonts::px(-6) + "px; font-weight:700; font-family:%3; }"
                  "QLabel#mtSearchError { color:%11; font-size:" + fincept::ui::fonts::px(-5) + "px; font-family:%3; }"
                  "QLabel#mtNotFound { color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-family:%3; padding:6px; border:1px solid %7;"
                  " background:%6; border-radius:2px; }")
              .arg(C(colors::AMBER))
              .arg(fonts::DATA.get())
              .arg(f)
              .arg(fonts::TINY.get())
              .arg(C(colors::TEXT_SECONDARY), C(colors::BG_SURFACE), C(colors::BORDER_MED), C(colors::BORDER_DIM),
                   C(colors::TEXT_PRIMARY), C(colors::TEXT_TERTIARY), C(colors::DANGER));
    // ── State tones (last, so they win over the role colours above) ─────────
    // Tones are states (service health, credit balance, route status), not
    // price direction: green / red stay green / red under every direction palette.
    ss += QString("QLabel[tone=\"positive\"] { color:%1; } QLabel[tone=\"amber\"] { color:%2; }"
                  "QLabel[tone=\"negative\"] { color:%3; } QLabel[tone=\"muted\"] { color:%4; }"
                  "QLabel#mtCredits[tone=\"positive\"] { border-color:%1; }"
                  "QLabel#mtCredits[tone=\"amber\"] { border-color:%2; }"
                  "QLabel#mtCredits[tone=\"negative\"] { border-color:%3; }")
              .arg(C(colors::SUCCESS), C(colors::AMBER), C(colors::DANGER), C(colors::TEXT_TERTIARY));
    return ss;
}

/// Typeahead popups are separate top-level windows (the screen's sheet does not
/// reach them), so they keep their own small sheet.
inline QString maritime_popup_ss() {
    using namespace fincept::ui;
    return QString("background:%1; color:%2; border:1px solid %3; font-family:%4; font-size:%5px; outline:0;"
                   "selection-background-color:%6; selection-color:%7;")
        .arg(C(colors::BG_RAISED), C(colors::TEXT_PRIMARY), C(colors::BORDER_MED), F(fonts::DATA_FAMILY))
        .arg(fonts::SMALL.get())
        .arg(C(colors::BG_HOVER), C(colors::AMBER));
}

} // namespace fincept::screens::maritime_internal
