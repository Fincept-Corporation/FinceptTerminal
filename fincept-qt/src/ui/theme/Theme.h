#pragma once
#include "ui/theme/FontSizeText.h"
#include "ui/theme/ThemeManager.h"
#include "ui/theme/ThemeTokens.h"

#include <QColor>
#include <QFont>
#include <QString>

namespace fincept::ui {

// ── Color shim ────────────────────────────────────────────────────────────────
// ColorToken is a single type for all color names.
// It holds a pointer-to-member of ThemeTokens and resolves the live value
// from ThemeManager at the point of use.
//
// Supports all usage patterns:
//   .arg(colors::BG_BASE)              — implicit const char* via operator const char*
//   .arg(colors::BG_BASE, colors::X)   — multi-arg via operator const char* (Qt6 variadic)
//   QColor(colors::BG_BASE)            — via operator QColor (Qt accepts QString ctor)
//   cond ? colors::A : colors::B       — same type, works natively
//   colors::BG_BASE()                  — explicit call via operator()
//   QString var = colors::X            — via operator const char* -> QString(const char*)

struct ColorToken {
    using Field = const char* ThemeTokens::*;
    Field field_;

    constexpr explicit ColorToken(Field f) noexcept : field_(f) {}

    const char* get() const noexcept { return ThemeManager::instance().tokens().*field_; }

    operator const char*() const noexcept { return get(); }
    operator QColor() const { return QColor(get()); }
    operator QString() const { return QString::fromLatin1(get()); }
    const char* operator()() const noexcept { return get(); }
};

namespace colors {

inline constexpr ColorToken BG_BASE{&ThemeTokens::bg_base};
inline constexpr ColorToken BG_SURFACE{&ThemeTokens::bg_surface};
inline constexpr ColorToken BG_RAISED{&ThemeTokens::bg_raised};
inline constexpr ColorToken BG_HOVER{&ThemeTokens::bg_hover};

inline constexpr ColorToken BORDER_DIM{&ThemeTokens::border_dim};
inline constexpr ColorToken BORDER_MED{&ThemeTokens::border_med};
inline constexpr ColorToken BORDER_BRIGHT{&ThemeTokens::border_bright};

inline constexpr ColorToken TEXT_PRIMARY{&ThemeTokens::text_primary};
inline constexpr ColorToken TEXT_SECONDARY{&ThemeTokens::text_secondary};
inline constexpr ColorToken TEXT_TERTIARY{&ThemeTokens::text_tertiary};
inline constexpr ColorToken TEXT_DIM{&ThemeTokens::text_dim};

inline constexpr ColorToken AMBER{&ThemeTokens::accent};
inline constexpr ColorToken AMBER_DIM{&ThemeTokens::accent_dim};
inline constexpr ColorToken ORANGE{&ThemeTokens::accent};
inline constexpr ColorToken TEXT_ON_ACCENT{&ThemeTokens::text_on_accent};

inline constexpr ColorToken ICON_DIM{&ThemeTokens::icon_dim};
inline constexpr ColorToken ICON_HOVER{&ThemeTokens::icon_hover};

inline constexpr ColorToken POSITIVE{&ThemeTokens::positive};
inline constexpr ColorToken POSITIVE_DIM{&ThemeTokens::positive_dim};
inline constexpr ColorToken NEGATIVE{&ThemeTokens::negative};
inline constexpr ColorToken NEGATIVE_DIM{&ThemeTokens::negative_dim};
inline constexpr ColorToken WARNING{&ThemeTokens::warning};
inline constexpr ColorToken INFO{&ThemeTokens::info};
inline constexpr ColorToken CYAN{&ThemeTokens::cyan};

inline constexpr ColorToken ACCENT_BG{&ThemeTokens::accent_bg};
inline constexpr ColorToken POSITIVE_BG{&ThemeTokens::positive_bg};
inline constexpr ColorToken NEGATIVE_BG{&ThemeTokens::negative_bg};

// Status (not price direction): stay green / red under every direction palette.
// Use POSITIVE / NEGATIVE for up / down, gain / loss, buy / sell; use these for
// connected / failed, valid / invalid, passed / error.
inline constexpr ColorToken SUCCESS{&ThemeTokens::success};
inline constexpr ColorToken DANGER{&ThemeTokens::danger};

/// Diverging-scale midpoint and "no change" fill for charts.
inline constexpr ColorToken CHART_NEUTRAL{&ThemeTokens::chart_neutral};

// Legacy aliases
inline constexpr ColorToken GREEN{&ThemeTokens::positive};
inline constexpr ColorToken RED{&ThemeTokens::negative};
inline constexpr ColorToken WHITE{&ThemeTokens::text_primary};
inline constexpr ColorToken GRAY{&ThemeTokens::text_secondary};
inline constexpr ColorToken MUTED{&ThemeTokens::text_tertiary};
inline constexpr ColorToken DARK{&ThemeTokens::bg_base};
inline constexpr ColorToken PANEL{&ThemeTokens::bg_surface};
inline constexpr ColorToken HEADER{&ThemeTokens::bg_raised};
inline constexpr ColorToken BORDER{&ThemeTokens::border_dim};
inline constexpr ColorToken CARD_BG{&ThemeTokens::bg_surface};
inline constexpr ColorToken ROW_ALT{&ThemeTokens::row_alt};

// ── Derived colours ───────────────────────────────────────────────────────────
// For everything the fixed tokens do not cover. All of them follow the active
// theme, and stylesheets built from them are kept in step when the user
// switches theme (see ThemeRestyle.cpp).

/// True when the active theme has dark backgrounds.
inline bool is_dark() {
    return ThemeManager::instance().is_dark();
}

/// A foreground colour that is not a token — a brand, category, rating or
/// series colour, written as it should look on the dark theme — made legible
/// on the active theme. Use for text, icons, lines and markers:
///   .arg(colors::legible("#fbbf24"))          — stylesheet / HTML
///   painter.setPen(colors::legible_color(c))  — painting
inline QColor legible_color(const QColor& color) {
    return ThemeManager::instance().adapt_foreground(color);
}
inline QString legible(const QColor& color) {
    return legible_color(color).name();
}

/// An opaque background washed with `color`: `strength` (0..1) of it mixed
/// into the base background — dark-tinted on the dark theme, pastel on the
/// light one. Use for badges, highlighted rows, status chips.
inline QColor tint_color(const QColor& color, double strength = 0.14) {
    return ThemeManager::instance().tint_background(color, strength);
}
inline QString tint(const QColor& color, double strength = 0.14) {
    return tint_color(color, strength).name();
}

/// A translucent veil in the contrast colour — white on the dark theme, black
/// on the light one — as a stylesheet "rgba(...)" value. Use instead of a
/// literal rgba(255,255,255,0.05) for hover washes and faint separators.
inline QString overlay(double alpha) {
    return ThemeManager::instance().overlay_rgba(alpha);
}
inline QColor overlay_color(double alpha) {
    QColor c = is_dark() ? QColor(255, 255, 255) : QColor(0, 0, 0);
    c.setAlphaF(static_cast<float>(alpha));
    return c;
}

/// `color` at `alpha` (0..1) as a stylesheet "rgba(...)" value.
inline QString alpha(const QColor& color, double alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(alpha < 0 ? 0.0 : alpha > 1 ? 1.0 : alpha, 'g', 3));
}
inline QString alpha(const ColorToken& token, double a) {
    return alpha(QColor(token.get()), a);
}

/// The backdrop behind a modal / popover: black at `alpha` on every theme.
inline QString scrim(double a = 0.5) {
    return alpha(QColor(0, 0, 0), a);
}

/// One of the six chart series colours of the active theme (wraps around).
/// Same as the first six chart_color() slots; prefer chart_color().
inline QColor series(int index) {
    const auto& c = ThemeManager::instance().tokens().chart_colors;
    const int n = static_cast<int>(c.size());
    return QColor(c[static_cast<size_t>(((index % n) + n) % n)]);
}

// ── Data-visualisation scales ────────────────────────────────────────────────
// All follow the active theme and direction palette; painted widgets refresh
// on ThemeManager::theme_changed.

/// Categorical series colour (12 distinct colours, wraps). Assign by entity in a
/// fixed order — never by rank — so filtering does not repaint survivors. No
/// slot is a price-direction hue.
inline QColor chart_color(int index) {
    return ThemeManager::instance().chart_color(index);
}
inline int chart_color_count() {
    return ThemeManager::kChartColorCount;
}

/// Diverging scale for signed values: t in -1..1 (clamped) runs
/// NEGATIVE → CHART_NEUTRAL → POSITIVE. Map value v with a clamp c as v / c.
inline QColor diverging_color(double t) {
    return ThemeManager::instance().diverging_color(t);
}

/// Sequential single-hue scale for magnitudes: t in 0..1 (clamped); 0 recedes
/// into the surface.
inline QColor sequential_color(double t) {
    return ThemeManager::instance().sequential_color(t);
}

/// Near-white or near-black text, whichever reads better on `bg`.
inline QColor contrast_text_on(const QColor& bg) {
    return ThemeManager::instance().contrast_text_on(bg);
}

} // namespace colors

/// Fonts — read from the active appearance (the user's font family and size).
namespace fonts {

struct FontFamilyToken {
    const char* get() const noexcept { return ThemeManager::instance().tokens().font_family; }
    operator QString() const { return QString::fromUtf8(get()); }
    const char* operator()() const noexcept { return get(); }
};

/// The user's font as a CSS font-family list ('Family','Fallback',monospace).
/// Works in stylesheets, in HTML style="..." attributes and as the family
/// argument of a QFont constructor. The terminal uses one font everywhere.
inline constexpr FontFamilyToken DATA_FAMILY{};
inline constexpr FontFamilyToken UI_FAMILY{};

/// Font size in px for text that is `offset` px larger (+) or smaller (-) than
/// the user's base size. offset=0 → base (14 by default), offset=-2 → 12.
/// Never smaller than ThemeManager::kFloorFontPx.
///
/// This is how every font size in the application is expressed. A design
/// that calls for "11px" at the default size is font_px(-3).
///
/// In a STYLESHEET use px() instead: at base 9 or 10, `.arg(font_px(-4))`
/// writes the clamped 7 with nothing to say it meant -4, so a later font-size
/// change cannot restore the right size (see FontSizeText.h).
inline int font_px(int offset = 0) {
    const int v = ThemeManager::instance().tokens().font_size_base + offset;
    return v < ThemeManager::kFloorFontPx ? ThemeManager::kFloorFontPx : v;
}

/// font_px(offset) as text, for stylesheets assembled by concatenation:
///   "color:%1; font-size:" + ui::fonts::px(-3) + "px;"
/// When the floor clamps the size the number carries the requested offset in
/// a CSS comment ("/*fo:-5*/7"), which Qt's stylesheet parser reads as
/// whitespace — so a later font-size change restores the right size. Use it in
/// CSS (stylesheets, HTML style attributes) only, never as a number.
inline QString px(int offset = 0) {
    return font_size_text::encode(ThemeManager::instance().tokens().font_size_base, offset,
                                  ThemeManager::kFloorFontPx);
}

/// A named text size: `px_at_default` at the default base size, moving with
/// the user's font size. Converts to int wherever a pixel size is expected.
struct FontSizeToken {
    int px_at_default;
    constexpr explicit FontSizeToken(int px) noexcept : px_at_default(px) {}
    int get() const noexcept { return font_px(px_at_default - ThemeManager::kDefaultFontPx); }
    operator int() const noexcept { return get(); }
    int operator()() const noexcept { return get(); }
};

inline constexpr FontSizeToken TITLE{20};
inline constexpr FontSizeToken HEADER{16};
inline constexpr FontSizeToken BODY{16};
inline constexpr FontSizeToken DATA{14};
inline constexpr FontSizeToken SMALL{13};
inline constexpr FontSizeToken TINY{12};

/// The user's font at `offset` px from the base size — for QPainter text and
/// setFont() calls. Use this instead of QFont("Consolas", 9).
inline QFont font(int offset = 0, int weight = QFont::Normal) {
    QFont f = ThemeManager::instance().current_font();
    f.setPixelSize(font_px(offset));
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

} // namespace fonts

/// The active theme's positive colour for values >= 0, its negative colour otherwise.
QString change_color(double value);

/// Applies the built-in default appearance (delegates to ThemeManager).
void apply_global_stylesheet();

bool is_rtl();
void set_rtl(bool rtl);

} // namespace fincept::ui
