#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;

namespace fincept::ui {

/// Central appearance singleton — the one place that decides how the terminal
/// looks. It owns:
///   * the active colour tokens (dark "Obsidian", light "Daylight", or
///     whichever of the two matches the operating system when the user picks
///     "System"),
///   * the user's font family and base font size,
///   * the content density,
/// builds the application stylesheet + palette from them, re-styles widgets
/// that already exist when any of them changes, and emits theme_changed so
/// custom-painted widgets can re-render.
///
/// Usage:
///   ThemeManager::instance().apply_appearance("Light", "JetBrains Mono", 14, "Default");
///   connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [...]);
///
/// Code that styles widgets must take every colour and font size from here
/// (ui::colors::*, ui::fonts::font_px(), ui::fonts::DATA_FAMILY in Theme.h) so
/// that it follows the user's settings.
class ThemeManager : public QObject {
    Q_OBJECT

  public:
    /// Which colours mean price up / down. Changes the values behind the
    /// `positive` / `negative` tokens (and their _dim / _bg variants), so every
    /// caller of colors::POSITIVE / NEGATIVE / ui::change_color() follows.
    ///   GreenUp        — green up, red down (Americas / Europe / India).
    ///   RedUp          — red up, green down (China, Japan, Korea, Taiwan).
    ///   ColorBlindSafe — blue up, orange down (red/green colour-vision deficiency).
    /// The `success` / `danger` status tokens are not affected.
    enum class DirectionPalette { GreenUp, RedUp, ColorBlindSafe };
    Q_ENUM(DirectionPalette)

    static ThemeManager& instance();

    /// Smallest / largest base font size the user can choose, and the default.
    static constexpr int kMinFontPx = 9;
    static constexpr int kMaxFontPx = 24;
    static constexpr int kDefaultFontPx = 14;
    /// No text is ever rendered smaller than this, whatever the base size.
    static constexpr int kFloorFontPx = 7;
    /// The font the terminal uses until the user picks another (bundled).
    static constexpr const char* kDefaultFontFamily = "JetBrains Mono";

    /// Apply a theme choice: "Dark", "Light" or "System" (case-insensitive;
    /// the legacy names "Obsidian" / "Daylight" are accepted). Anything else
    /// falls back to "Dark".
    void apply_theme(const QString& choice);

    /// Change the application font family and base size.
    void apply_font(const QString& family, int size_px);

    /// Change content density: "Compact" (2px), "Default" (4px), "Comfortable" (8px).
    void apply_density(const QString& density);

    /// Coalesced apply: updates font family/size and density, then re-styles
    /// exactly once. Use this in live-preview / debounced handlers so
    /// qApp->setStyleSheet() runs once instead of twice — important on KDE
    /// Plasma 6 + Qt6 + Wayland where back-to-back global restyles during a
    /// QComboBox event chain can crash the process. See GitHub issue #247.
    void apply_typography_and_density(const QString& family, int size_px, const QString& density);

    /// Coalesced apply of everything the Appearance settings control. An empty
    /// `theme_choice` / `family` / `density` or a non-positive `size_px`
    /// leaves that part as it is. Re-styles exactly once.
    void apply_appearance(const QString& theme_choice, const QString& family, int size_px, const QString& density);

    /// Read access to the currently active token set.
    /// Custom painters call: QColor(ThemeManager::instance().tokens().accent)
    const ThemeTokens& tokens() const { return current_; }

    /// Name of the preset in effect ("Obsidian" or "Daylight").
    QString current_theme_name() const;
    /// The user's choice: "Dark", "Light" or "System".
    QString theme_choice() const { return theme_choice_; }
    /// True when the preset in effect has dark backgrounds.
    bool is_dark() const { return current_.is_dark; }

    QFont current_font() const;
    /// The family at the head of the font fallback chain (the user's choice).
    QString font_family() const { return font_families_.value(0); }
    int font_size_px() const { return font_size_px_; }
    QString density() const;

    static QStringList available_themes();    // {"Dark", "Light", "System"}
    static QStringList available_densities(); // single source of truth for density names

    // ── Direction palette ───────────────────────────────────────────────────
    /// Switch the up/down colours, re-style existing widgets and emit
    /// theme_changed + direction_palette_changed. `persist` stores the choice
    /// in the settings table ("appearance.direction_palette"); pass false for a
    /// live preview that a Save button commits later. The stored choice is
    /// restored automatically by the first apply_appearance() call at startup
    /// (the FINCEPT_DIRECTION_PALETTE environment variable overrides it).
    void set_direction_palette(DirectionPalette palette, bool persist = true);
    DirectionPalette direction_palette() const { return direction_; }
    /// Settings value for a palette: "GreenUp", "RedUp" or "ColorBlindSafe".
    static QString direction_palette_key(DirectionPalette palette);
    /// Inverse of direction_palette_key (case-insensitive); unknown → GreenUp.
    static DirectionPalette direction_palette_from_key(const QString& key);
    /// All settings keys, in display order.
    static QStringList available_direction_palettes();

    // ── Data-visualisation colours (all follow theme + direction palette) ───
    /// Categorical series colour `index` (wraps; negative indices are folded).
    /// At least 12 distinct colours, none of them a price-direction hue.
    QColor chart_color(int index) const;
    static constexpr int kChartColorCount = 12;
    /// Diverging scale: t = -1 → negative, 0 → chart_neutral, +1 → positive
    /// (clamped). Interpolated in OKLab, so it follows the direction palette.
    QColor diverging_color(double t) const;
    /// Sequential single-hue scale: t = 0 recedes into the surface, 1 is the
    /// strongest (clamped). Its hue avoids the direction colours.
    QColor sequential_color(double t) const;
    /// Text colour (near-white or near-black ink) with the higher WCAG
    /// contrast on `bg`. A translucent `bg` is composited over bg_surface first.
    QColor contrast_text_on(const QColor& bg) const;
    /// WCAG contrast ratio between two opaque colours (1..21).
    static double contrast_ratio(const QColor& a, const QColor& b);

    /// Returns QSS that must be applied directly to the CDockManager widget
    /// to override ADS's internally loaded default.css / focus_highlighting.css.
    QString build_ads_qss() const;

    /// Give `dock_manager` the ADS stylesheet (build_ads_qss()) and mark it so
    /// that every later appearance change REBUILDS that sheet rather than
    /// translating it: the dock tab height in it follows the font size, and no
    /// font-size rewrite reaches a min-height — a live font change kept the old
    /// tab strip while a fresh start sized it right.
    void apply_ads_qss(QWidget* dock_manager) const;

    // ── Derived colours ─────────────────────────────────────────────────────
    // Each helper returns a colour computed from the active preset and records
    // how it was computed, so the live re-style pass can recompute it for the
    // other preset when it meets the value inside an already-built stylesheet.
    // Prefer the wrappers in ui::colors (Theme.h) over calling these directly.

    /// `color` (a foreground colour chosen for a dark background — a brand,
    /// category or series colour) made legible on the active background:
    /// unchanged on a dark preset, darkened until it contrasts on a light one.
    QColor adapt_foreground(const QColor& color) const;

    /// `color` mixed into the base background at `strength` (0..1) — an opaque
    /// tinted fill that is dark on a dark preset and pastel on a light one.
    QColor tint_background(const QColor& color, double strength) const;

    /// A translucent veil in the contrast colour (white on dark, black on
    /// light) as a QSS "rgba(...)" string — for hover washes and hairlines.
    QString overlay_rgba(double alpha) const;

    /// True while the theme_changed handlers run. They run with Qt's re-styling
    /// held back (each stylesheet they set is only stored; everything is styled
    /// once afterwards). Showing a widget inside a visible window, or asking
    /// for size hints, at that point makes Qt activate the layouts up to the
    /// window and style every widget they reach, one by one — seconds, with a
    /// few screens open. A handler (or a resizeEvent it can cause) that needs
    /// to do so checks this and does the work on appearance_settled() instead.
    bool notifying() const { return notifying_; }

  signals:
    /// Emitted after every appearance change (theme, font or density), once
    /// the application stylesheet, palette, font and all existing widgets'
    /// own stylesheets have been updated.
    /// Custom paintEvent widgets connect to this to invalidate caches and call update().
    /// Also emitted when the direction palette changes.
    void theme_changed(const fincept::ui::ThemeTokens& tokens);

    /// Emitted once per appearance change, after every theme_changed handler has
    /// run and every widget on screen has its new style. Work that shows or
    /// hides widgets, or asks widgets for their size hints, belongs here rather
    /// than in a theme_changed handler (see notifying()).
    void appearance_settled();

    /// Emitted after set_direction_palette() changed the palette (after theme_changed).
    void direction_palette_changed(fincept::ui::ThemeManager::DirectionPalette palette);

  protected:
    /// Application-wide filter (installed on qApp) that keeps a container's
    /// own border from being inherited by the widgets inside it — see
    /// scope_own_border() in ThemeRestyle.cpp.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    ThemeManager();

    /// The preset `base` (THEME_OBSIDIAN / THEME_DAYLIGHT) with the direction
    /// palette `p` applied. Stable addresses — the re-style pass keys on them.
    /// Defined in ThemePalettes.cpp.
    static const ThemeTokens* direction_variant(const ThemeTokens* base, DirectionPalette p);
    /// Restore the persisted direction palette once (no re-style).
    void restore_direction_palette_once();

    /// Everything that determines how a stylesheet built from the tokens
    /// looks — captured before a change so the re-style pass knows what the
    /// existing widgets were built with.
    struct Snapshot {
        const ThemeTokens* preset = nullptr;
        QByteArray font_family_css;
        int font_size_px = 0;
    };
    Snapshot snapshot() const;

    void set_theme_choice(const QString& choice);
    void set_font(const QString& family, int size_px);
    void set_density(const QString& density);
    const ThemeTokens* resolve_preset() const;
    void sync_tokens();

    /// Apply colour scheme, palette, font and the application stylesheet.
    /// `force_repolish`: recompute every widget's style even when the
    /// application stylesheet text itself did not change.
    /// `defer_widgets`: the caller re-styles the widgets itself afterwards
    /// (notify_theme_changed()), so only the application sheet is stored here.
    void rebuild_and_apply(bool force_repolish = false, bool defer_widgets = false);

    /// Give the application its stylesheet. On an application that already has
    /// one this stores the text and re-styles every widget once (or leaves
    /// that to the caller when `defer_widgets`), instead of Qt's own update,
    /// which re-styles each widget once per ancestor. Returns false when Qt's
    /// own path was used (first stylesheet, or FINCEPT_THEME_LEGACY_RESTYLE).
    /// Defined in ThemeRestyle.cpp.
    bool set_application_stylesheet(const QString& qss, bool defer_widgets);
    /// Re-style every existing widget exactly once. Defined in ThemeRestyle.cpp.
    void repolish_all_widgets();
    /// Emit theme_changed() with the handlers' own setStyleSheet() calls only
    /// stored, then re-style every widget once. Defined in ThemeRestyle.cpp.
    void notify_theme_changed();
    QString build_global_qss() const;
    void apply_palette();
    void apply_color_scheme();

    /// Translate the per-widget stylesheets of every existing widget from
    /// `from` to the current appearance. The new texts are only stored: they
    /// take effect at the next application-wide style recalculation
    /// (repolish_all_widgets()). Returns whether any text changed.
    /// Defined in ThemeRestyle.cpp.
    bool restyle_existing_widgets(const Snapshot& from);

    /// Build a platform-aware monospace fallback chain by prepending the
    /// user's preferred family (if any) to a cross-platform default list.
    /// Qt's QFont::setFamilies / QSS resolver picks the first available
    /// font on the running system. Including the macOS natives (SF Mono,
    /// Menlo) is what stops text fields from "fragmenting" — on macOS,
    /// Consolas isn't installed and the silent fallback to a proportional
    /// system font breaks the glyph metrics QLineEdit/QTextEdit assume.
    static QStringList build_font_chain(const QString& preferred);

    /// Format `font_families_` as a CSS `font-family` value. `quote` is the
    /// quote character put around named families: '"' for the application
    /// stylesheet, '\'' for the token (which is also embedded in HTML
    /// style="..." attributes, where a double quote would end the attribute).
    QString css_font_family_list(QChar quote) const;

    /// Height of a dock tab: 22px at the default font size, growing with the
    /// tab label font (base size - 3) so the label never clips.
    int dock_tab_height() const { return qMax(22, qMax(kFloorFontPx, font_size_px_ - 3) + 11); }

    const ThemeTokens* preset_ = &THEME_OBSIDIAN; // preset in effect (base preset + direction palette)
    ThemeTokens current_;                         // preset_ + the user's font
    DirectionPalette direction_ = DirectionPalette::GreenUp;
    bool direction_restored_ = false; // persisted direction palette has been read
    QString theme_choice_ = QStringLiteral("Dark");
    bool following_system_ = false; // colorSchemeChanged is connected
    /// Resolved font fallback chain. `font_families_.first()` is the user's
    /// preferred family (or the cross-platform default head); subsequent
    /// entries are the fallback cascade.
    QStringList font_families_ = build_font_chain({});
    QByteArray font_family_css_; // backing storage for current_.font_family
    int font_size_px_ = kDefaultFontPx;
    int density_pad_ = 4;        // px padding driven by density setting
    mutable QString cached_qss_; // last built QSS — skip rebuild if tokens unchanged
    bool applied_once_ = false;  // a stylesheet has been applied at least once
    bool notifying_ = false;     // theme_changed handlers are running
};

} // namespace fincept::ui
