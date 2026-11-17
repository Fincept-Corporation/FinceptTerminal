// ThemePalettes.cpp — the direction-palette variants of the built-in presets
// and the data-visualisation colour scales (categorical, diverging,
// sequential) that ThemeManager exposes.
//
// A direction palette is applied by swapping the whole preset for a variant
// whose positive / negative tokens (and their _dim / _bg companions) carry the
// chosen colours. Every existing reader of the tokens — ui::change_color(),
// colors::POSITIVE, stylesheets built from them — follows without change, and
// the re-style pass (ThemeRestyle.cpp) translates already-built stylesheets
// between variants exactly as it does between Obsidian and Daylight. Each
// variant keeps the "all colour values distinct" rule of ThemeTokens.h.

#include "ui/theme/ThemeManager.h"

#include <QtGlobal>

#include <cmath>
#include <utility>

namespace fincept::ui {

namespace {

// ── Variant construction ─────────────────────────────────────────────────────

ThemeTokens tp_make_red_up(const ThemeTokens& base) {
    ThemeTokens t = base;
    std::swap(t.positive, t.negative);
    std::swap(t.positive_dim, t.negative_dim);
    std::swap(t.positive_bg, t.negative_bg);
    return t;
}

/// Blue up / vermilion-orange down (the Okabe–Ito pair that survives
/// protanopia and deuteranopia). The categorical slots that would read as
/// those two hues (sky, blue, orange) are swapped for teal, lime and
/// raspberry, and the sequential scale moves from blue to violet, so neither
/// can be mistaken for a direction. All values were checked for contrast
/// (direction text ≥ 4.8:1 on every surface) and for adjacent-pair CVD
/// separation (worst ΔE 13.3 dark / 8.9 light).
ThemeTokens tp_make_colorblind(const ThemeTokens& base, bool dark) {
    ThemeTokens t = base;
    if (dark) {
        t.positive = "#51a2f8";
        t.positive_dim = "#1d4f82";
        t.positive_bg = "#0c1b2d";
        t.negative = "#fa7a45";
        t.negative_dim = "#7f3d22";
        t.negative_bg = "#2d160d";
        t.chart_palette[1] = "#3bcabb";
        t.chart_palette[4] = "#a4d854";
        t.chart_palette[9] = "#e74783";
        t.seq_low = "#2d233e";
        t.seq_high = "#c9adfd";
    } else {
        t.positive = "#0961bb";
        t.positive_dim = "#a0c0eb";
        t.positive_bg = "#e9f3ff";
        t.negative = "#b44200";
        t.negative_dim = "#ecb6a1";
        t.negative_bg = "#feeee8";
        t.chart_palette[1] = "#0a8379";
        t.chart_palette[4] = "#5c8b11";
        t.chart_palette[9] = "#bc1560";
        t.seq_low = "#f1ebfd";
        t.seq_high = "#562c8b";
    }
    for (size_t i = 0; i < t.chart_colors.size(); ++i)
        t.chart_colors[i] = t.chart_palette[i];
    return t;
}

// ── Colour arithmetic (sRGB ↔ OKLab) ─────────────────────────────────────────

struct TpLab {
    double L = 0;
    double a = 0;
    double b = 0;
};

double tp_to_linear(double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double tp_to_srgb(double c) {
    c = qBound(0.0, c, 1.0);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

TpLab tp_to_oklab(const QColor& c) {
    const double r = tp_to_linear(c.redF());
    const double g = tp_to_linear(c.greenF());
    const double b = tp_to_linear(c.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

QColor tp_from_oklab(const TpLab& lab) {
    const double l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    const double m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    const double s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;
    const double l = l_ * l_ * l_;
    const double m = m_ * m_ * m_;
    const double s = s_ * s_ * s_;
    const double r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const double b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
    return QColor::fromRgbF(static_cast<float>(tp_to_srgb(r)), static_cast<float>(tp_to_srgb(g)),
                            static_cast<float>(tp_to_srgb(b)));
}

/// `from` → `to` at `t` (0..1), interpolated in OKLab (perceptually even).
QColor tp_mix_oklab(const QColor& from, const QColor& to, double t) {
    t = qBound(0.0, t, 1.0);
    const TpLab a = tp_to_oklab(from);
    const TpLab b = tp_to_oklab(to);
    return tp_from_oklab({a.L + (b.L - a.L) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t});
}

double tp_relative_luminance(const QColor& c) {
    return 0.2126 * tp_to_linear(c.redF()) + 0.7152 * tp_to_linear(c.greenF()) + 0.0722 * tp_to_linear(c.blueF());
}

/// `over` composited onto the opaque `under`.
QColor tp_composite(const QColor& over, const QColor& under) {
    const double a = over.alphaF();
    if (a >= 1.0)
        return over;
    return QColor::fromRgbF(static_cast<float>(over.redF() * a + under.redF() * (1.0 - a)),
                            static_cast<float>(over.greenF() * a + under.greenF() * (1.0 - a)),
                            static_cast<float>(over.blueF() * a + under.blueF() * (1.0 - a)));
}

} // namespace

// ── ThemeManager members ─────────────────────────────────────────────────────

const ThemeTokens* ThemeManager::direction_variant(const ThemeTokens* base, DirectionPalette p) {
    if (!base || p == DirectionPalette::GreenUp)
        return base;
    const bool light = base == &THEME_DAYLIGHT;
    if (p == DirectionPalette::RedUp) {
        static const ThemeTokens obsidian_red_up = tp_make_red_up(THEME_OBSIDIAN);
        static const ThemeTokens daylight_red_up = tp_make_red_up(THEME_DAYLIGHT);
        return light ? &daylight_red_up : &obsidian_red_up;
    }
    static const ThemeTokens obsidian_cb = tp_make_colorblind(THEME_OBSIDIAN, true);
    static const ThemeTokens daylight_cb = tp_make_colorblind(THEME_DAYLIGHT, false);
    return light ? &daylight_cb : &obsidian_cb;
}

QColor ThemeManager::chart_color(int index) const {
    const int n = static_cast<int>(current_.chart_palette.size());
    const int i = ((index % n) + n) % n;
    return QColor(current_.chart_palette[static_cast<size_t>(i)]);
}

QColor ThemeManager::diverging_color(double t) const {
    const QColor mid(current_.chart_neutral);
    if (std::isnan(t))
        return mid;
    t = qBound(-1.0, t, 1.0);
    QColor end(t >= 0 ? current_.positive : current_.negative);
    // The dark preset's direction colours are bright text colours; as large
    // fills they glare, so the ends of the scale sit a little deeper.
    if (current_.is_dark)
        end = tp_mix_oklab(end, QColor(current_.bg_base), 0.18);
    return tp_mix_oklab(mid, end, std::abs(t));
}

QColor ThemeManager::sequential_color(double t) const {
    if (std::isnan(t))
        t = 0.0;
    return tp_mix_oklab(QColor(current_.seq_low), QColor(current_.seq_high), t);
}

double ThemeManager::contrast_ratio(const QColor& a, const QColor& b) {
    const double la = tp_relative_luminance(a);
    const double lb = tp_relative_luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

QColor ThemeManager::contrast_text_on(const QColor& bg) const {
    const QColor solid = tp_composite(bg, QColor(current_.bg_surface));
    // The two ink colours of the built-in presets: near-white and near-black.
    const QColor light_ink(THEME_OBSIDIAN.text_primary);
    const QColor dark_ink(THEME_DAYLIGHT.text_primary);
    return contrast_ratio(light_ink, solid) >= contrast_ratio(dark_ink, solid) ? light_ink : dark_ink;
}

} // namespace fincept::ui
