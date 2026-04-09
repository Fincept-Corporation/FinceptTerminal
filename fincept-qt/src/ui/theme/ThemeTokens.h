#pragma once
#include <array>

namespace fincept::ui {

/// All color, font, and spacing tokens for a single theme preset.
/// Custom painters and QSS builders read from this struct — no hardcoded hex anywhere else.
struct ThemeTokens {
    // --- Identity ---
    const char* name;

    // --- Backgrounds (darkest → lightest) ---
    const char* bg_base;       // deepest background (#080808 in Obsidian)
    const char* bg_surface;    // card / panel surface
    const char* bg_raised;     // slightly elevated surfaces (headers, toolbars)
    const char* bg_hover;      // hover state background

    // --- Borders ---
    const char* border_dim;    // hairline, barely visible
    const char* border_med;    // standard separator
    const char* border_bright; // active / focused border

    // --- Text hierarchy ---
    const char* text_primary;   // body / data text
    const char* text_secondary; // labels, captions
    const char* text_tertiary;  // disabled, placeholder
    const char* text_dim;       // near-invisible muted text

    // --- Accent (theme-defining color) ---
    const char* accent;         // primary highlight (amber/green/blue/orange)
    const char* accent_dim;     // dimmed accent for backgrounds / fills
    const char* text_on_accent; // text color when accent is the background

    // --- Interactive chrome (close buttons, toolbar icons) ---
    const char* icon_dim;       // muted icon/button color (close, menu, detach)
    const char* icon_hover;     // icon color on hover

    // --- Functional (semantic, consistent meaning across all themes) ---
    const char* positive;      // gain / success / buy
    const char* positive_dim;  // dimmed positive for borders
    const char* negative;      // loss / error / sell
    const char* negative_dim;  // dimmed negative for borders
    const char* warning;       // caution
    const char* info;          // informational
    const char* cyan;          // secondary highlight / links

    // --- Tinted backgrounds (subtle fills for buy/sell/accent areas) ---
    const char* accent_bg;     // translucent accent background
    const char* positive_bg;   // translucent positive background
    const char* negative_bg;   // translucent negative background

    // --- Table ---
    const char* row_alt;   // alternating row background

    // --- Font ---
    const char* font_family;  // CSS font-family string
    int         font_size_base; // default body/data size in px (e.g. 14)

    // --- Chart color palette (6 series colors for multi-line/bar charts) ---
    std::array<const char*, 6> chart_colors;
};

// ---------------------------------------------------------------------------
// Built-in theme presets — defined in ThemeManager.cpp
// ---------------------------------------------------------------------------
extern const ThemeTokens THEME_OBSIDIAN;
extern const ThemeTokens THEME_BLOOMBERG;
extern const ThemeTokens THEME_LIGHT;

} // namespace fincept::ui
