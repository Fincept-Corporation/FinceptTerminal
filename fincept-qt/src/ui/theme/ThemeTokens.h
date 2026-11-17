#pragma once
#include <array>

namespace fincept::ui {

/// All color, font, and spacing tokens for a single theme preset.
/// Custom painters and QSS builders read from this struct — no hardcoded hex anywhere else.
///
/// Rule for anyone adding a preset: every colour value inside one preset must
/// be distinct, except for the documented aliases (icon_dim == text_secondary,
/// icon_hover == text_primary, chart_colors[] == chart_palette[0..5]).
/// The live re-style pass (ThemeRestyle.cpp) translates already-built
/// stylesheets from one preset to another by colour value, so two tokens that
/// share a value in the outgoing preset cannot be told apart.
///
/// Price direction vs. status: `positive` / `negative` (and their _dim / _bg
/// variants) mean *price direction* — up / down, gain / loss, buy / sell. Their
/// values follow the user's direction palette (green-up, red-up for CN/JP/KR/TW
/// markets, or blue/orange for colour-blind users; see
/// ThemeManager::set_direction_palette). `success` / `danger` mean *state* —
/// connected / failed, valid / invalid — and stay green / red whatever the
/// direction palette is. Pick by meaning, not by hue.
struct ThemeTokens {
    // --- Identity ---
    const char* name;
    bool is_dark; // true when backgrounds are dark and text is light

    // --- Backgrounds (base → most elevated) ---
    const char* bg_base;    // deepest background (#080808 in Obsidian)
    const char* bg_surface; // card / panel surface
    const char* bg_raised;  // slightly elevated surfaces (headers, toolbars)
    const char* bg_hover;   // hover state background

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
    const char* icon_dim;   // muted icon/button color (close, menu, detach)
    const char* icon_hover; // icon color on hover

    // --- Functional (semantic, consistent meaning across all themes) ---
    const char* positive;     // price up / gain / buy (follows the direction palette)
    const char* positive_dim; // dimmed positive for borders
    const char* negative;     // price down / loss / sell (follows the direction palette)
    const char* negative_dim; // dimmed negative for borders
    const char* warning;      // caution
    const char* info;         // informational
    const char* cyan;         // secondary highlight / links
    const char* success;      // good state: connected, passed, valid — always green
    const char* danger;       // bad state: error, failed, invalid — always red

    // --- Tinted backgrounds (subtle fills for buy/sell/accent areas) ---
    const char* accent_bg;   // translucent accent background
    const char* positive_bg; // translucent positive background
    const char* negative_bg; // translucent negative background

    // --- Table ---
    const char* row_alt; // alternating row background

    // --- Font ---
    // In the *active* token set (ThemeManager::tokens()) these two follow the
    // user's Appearance settings, not the preset: font_family is the CSS
    // font-family list headed by the chosen family, font_size_base the chosen
    // base size. The values in the preset definitions are only the defaults.
    const char* font_family; // CSS font-family string (single-quoted names)
    int font_size_base;      // default body/data size in px (e.g. 14)

    // --- Chart color palette (6 series colors for multi-line/bar charts) ---
    // The first six entries of chart_palette — kept for existing callers.
    std::array<const char*, 6> chart_colors;

    // --- Data-visualisation scales ---
    // Categorical series colours, in fixed order (assign by entity, never by
    // rank). None is a price-direction hue, so a multi-series chart never reads
    // as gain / loss. Validated for adjacent-pair colour-vision-deficiency
    // separation on the preset's surface.
    std::array<const char*, 12> chart_palette;
    const char* chart_neutral; // diverging-scale midpoint / "no change" fill
    const char* seq_low;       // sequential scale, smallest magnitude (recedes into the surface)
    const char* seq_high;      // sequential scale, largest magnitude
};

// ---------------------------------------------------------------------------
// Built-in theme presets — defined in ThemeManager.cpp
// ---------------------------------------------------------------------------
extern const ThemeTokens THEME_OBSIDIAN; // dark (the original look)
extern const ThemeTokens THEME_DAYLIGHT; // light

} // namespace fincept::ui
