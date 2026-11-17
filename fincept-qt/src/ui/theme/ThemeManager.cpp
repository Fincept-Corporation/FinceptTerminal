#include "ui/theme/ThemeManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/PlatformIcons.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QPointer>
#include <QStyleHints>
#include <QWidget>

namespace fincept::ui {

// Dark preset — the terminal's native look. Pure neutral black, greys and
// off-white (the "black-grey-white" palette: #000000, #525252, #8c8c8c,
// #b7b7b7, #e8e8e8) with no colour cast in any surface, border or text; the
// only hues on screen are the amber accent and the data / status colours.
// Text keeps at least 4.5:1 contrast against the base, surface and
// alternate-row backgrounds. Fills in the accent / positive / negative / info
// colours are bright, so text placed on them is the dark text_on_accent,
// never white.
const ThemeTokens THEME_OBSIDIAN = {
    .name = "Obsidian",
    .is_dark = true,
    .bg_base = "#000000",
    .bg_surface = "#0b0b0b",
    .bg_raised = "#161616",
    .bg_hover = "#222222",
    .border_dim = "#262626",
    .border_med = "#3a3a3a",
    .border_bright = "#525252",
    .text_primary = "#e8e8e8",
    .text_secondary = "#b7b7b7",
    .text_tertiary = "#8c8c8c",
    .text_dim = "#767676",
    .accent = "#f5a623",
    .accent_dim = "#c98a1b",
    .text_on_accent = "#1a1003",
    .icon_dim = "#b7b7b7",
    .icon_hover = "#e8e8e8",
    .positive = "#2fd07a",
    .positive_dim = "#1c6b42",
    .negative = "#ff5d5d",
    .negative_dim = "#8a2c2c",
    .warning = "#f2c94c",
    .info = "#5b9dff",
    .cyan = "#35c8dc",
    // Same look as positive / negative on purpose, one step apart in value so
    // the re-style pass can tell a status colour from a direction colour.
    .success = "#2ecf79",
    .danger = "#fe5c5c",
    .accent_bg = "#241a08",
    .positive_bg = "#0e2318",
    .negative_bg = "#2a1215",
    .row_alt = "#070707",
    .font_family = "'JetBrains Mono','Consolas',monospace",
    .font_size_base = 14,
    .chart_colors = {"#f5ae39", "#3fb1ea", "#9c51cc", "#ffb8df", "#4680f1", "#b98749"},
    // amber, sky, purple, pink, blue, tan, periwinkle, magenta, cyan, orange,
    // lavender, yellow — no green or red. Worst adjacent pair on #10131a:
    // CVD ΔE 13.3, normal-vision ΔE 24.5 (OKLab ×100); all ≥ 3.9:1 contrast.
    .chart_palette = {"#f5ae39", "#3fb1ea", "#9c51cc", "#ffb8df", "#4680f1", "#b98749", "#b7c9fc", "#e45cbe",
                      "#5ed9e6", "#f67f2f", "#b7a1f5", "#f0d947"},
    .chart_neutral = "#3d3d3d",
    .seq_low = "#142a41",
    .seq_high = "#78c7fd",
};

// Light preset. Same roles as Obsidian with the lightness inverted and the
// same neutrality: paper-grey base, near-white cards, "raised" chrome and
// hover states stepping darker, near-black text — no colour cast. Text and
// data colours are the deep shade of their hue so they keep at least 4.5:1
// contrast on the backgrounds. Fills are deep, so text_on_accent is white.
// Every value is distinct (see the rule in ThemeTokens.h).
const ThemeTokens THEME_DAYLIGHT = {
    .name = "Daylight",
    .is_dark = false,
    .bg_base = "#f4f4f4",
    .bg_surface = "#fdfdfd",
    .bg_raised = "#eaeaea",
    .bg_hover = "#dedede",
    .border_dim = "#dcdcdc",
    .border_med = "#c4c4c4",
    .border_bright = "#8f8f8f",
    .text_primary = "#141414",
    .text_secondary = "#4a4a4a",
    .text_tertiary = "#616161",
    .text_dim = "#6e6e6e",
    .accent = "#a84c08",
    .accent_dim = "#8f3f06",
    .text_on_accent = "#ffffff",
    .icon_dim = "#4a4a4a",
    .icon_hover = "#141414",
    .positive = "#0b7a43",
    .positive_dim = "#7fcba2",
    .negative = "#c62430",
    .negative_dim = "#eb9aa0",
    .warning = "#8f5f00",
    .info = "#1b55d9",
    .cyan = "#0a7186",
    .success = "#0a7942",
    .danger = "#c52330",
    .accent_bg = "#fbefdc",
    .positive_bg = "#e3f5eb",
    .negative_bg = "#fce8ea",
    .row_alt = "#eeeeee",
    .font_family = "'JetBrains Mono','Consolas',monospace",
    .font_size_base = 14,
    .chart_colors = {"#be7c09", "#077fa8", "#6c25a4", "#d76797", "#2750c7", "#865634"},
    // The Obsidian hues stepped for the light surface. Worst adjacent pair on
    // #fdfdfe: CVD ΔE 9.6, normal-vision ΔE 22.3. Yellow (slot 12) is 2.6:1 —
    // label it or offer a table view when it carries data alone.
    .chart_palette = {"#be7c09", "#077fa8", "#6c25a4", "#d76797", "#2750c7", "#865634", "#878cc9", "#b71d9a",
                      "#279ea4", "#c65102", "#816bd8", "#b49f03"},
    .chart_neutral = "#d4d7e0",
    .seq_low = "#e2f0ff",
    .seq_high = "#034393",
};

namespace {

/// While an appearance change is applied: wait cursor, and no repaint of any
/// window until the whole change is in (one repaint instead of thousands).
class BusyScope {
  public:
    BusyScope() {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        for (QWidget* top : QApplication::topLevelWidgets()) {
            if (top->isVisible() && top->updatesEnabled()) {
                top->setUpdatesEnabled(false);
                paused_.append(top);
            }
        }
    }
    ~BusyScope() {
        for (const QPointer<QWidget>& top : paused_) {
            if (top)
                top->setUpdatesEnabled(true);
        }
        QApplication::restoreOverrideCursor();
    }
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;

  private:
    QList<QPointer<QWidget>> paused_;
};

int density_to_pad(const QString& density) {
    if (density.compare(QLatin1String("Compact"), Qt::CaseInsensitive) == 0)
        return 2;
    if (density.compare(QLatin1String("Comfortable"), Qt::CaseInsensitive) == 0)
        return 8;
    return 4;
}

/// CJK text (AkShare, Asia Markets names) draws with fonts that ship with the
/// operating system, in a fixed order per script, instead of whatever the OS
/// picks (Windows chose the Japanese "MS UI Gothic" for Chinese). Application
/// fallback families (Qt 6.8) apply to every font — the UI and the monospace
/// data family alike — and only to glyphs the font lacks, so Latin metrics
/// never change. Only installed families are listed; nothing is bundled. Once,
/// from the constructor, before the first font lookup.
void install_cjk_fallbacks() {
#if defined(Q_OS_WIN)
    const QStringList zh{QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")};
    const QStringList ja{QStringLiteral("Yu Gothic UI")};
    const QStringList ko{QStringLiteral("Malgun Gothic")};
#elif defined(Q_OS_MACOS)
    const QStringList zh{QStringLiteral("PingFang SC")};
    const QStringList ja{QStringLiteral("Hiragino Sans")};
    const QStringList ko{QStringLiteral("Apple SD Gothic Neo")};
#else
    const QStringList zh{QStringLiteral("Noto Sans CJK SC"), QStringLiteral("WenQuanYi Micro Hei")};
    const QStringList ja{QStringLiteral("Noto Sans CJK JP")};
    const QStringList ko{QStringLiteral("Noto Sans CJK KR")};
#endif
    const auto installed = [](const QStringList& wanted) {
        QStringList out;
        for (const QString& family : wanted) {
            if (QFontDatabase::hasFamily(family))
                out.append(family);
        }
        return out;
    };
    const QStringList z = installed(zh);
    const QStringList j = installed(ja);
    const QStringList k = installed(ko);
    // The list order is the priority (first wins). Han: Chinese first — the CJK
    // text on screen is mostly Chinese names; kana / hangul: their own language first.
    const auto set = [](QChar::Script script, const QStringList& families) {
        if (!families.isEmpty())
            QFontDatabase::setApplicationFallbackFontFamilies(script, families);
    };
    set(QChar::Script_Han, z + j + k);
    set(QChar::Script_Bopomofo, z);
    set(QChar::Script_Hiragana, j + z + k);
    set(QChar::Script_Katakana, j + z + k);
    set(QChar::Script_Hangul, k + z + j);
    const QStringList all = z + j + k;
    LOG_INFO("ThemeManager", all.isEmpty() ? QStringLiteral("CJK font fallback: no system CJK font found")
                                           : QStringLiteral("CJK font fallback families=%1").arg(all.join(QLatin1Char(','))));
}

/// Canonical spelling of a theme choice; unknown input means "Dark".
QString normalise_theme_choice(const QString& choice) {
    const QString c = choice.trimmed();
    if (c.compare(QLatin1String("Light"), Qt::CaseInsensitive) == 0 ||
        c.compare(QLatin1String("Daylight"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Light");
    if (c.compare(QLatin1String("System"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("System");
    return QStringLiteral("Dark");
}

} // namespace

ThemeManager::ThemeManager() : QObject(nullptr), current_(THEME_OBSIDIAN) {
    if (qApp)
        qApp->installEventFilter(this);
    // The default font ships inside the executable so the terminal looks the
    // same on every machine; it has to be registered before the first font
    // lookup. (The singleton is first used after QApplication exists.)
    for (const char* file : {"JetBrainsMono-Regular.ttf", "JetBrainsMono-Medium.ttf",
                             "JetBrainsMono-SemiBold.ttf", "JetBrainsMono-Bold.ttf"}) {
        if (QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/") + QLatin1String(file)) < 0)
            LOG_WARN("ThemeManager", QString("bundled font %1 could not be loaded").arg(QLatin1String(file)));
    }
    // Chinese / Japanese / Korean glyphs: a fixed per-script fallback (once, process-wide).
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        install_cjk_fallbacks();
    // Before the first window: Qt's own dialog / clear-button icons must not
    // scan an uncached icon theme on the GUI thread (PlatformIcons.h).
    if (qApp)
        tune_platform_icon_lookup();
    sync_tokens();
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

QString ThemeManager::css_font_family_list(QChar quote) const {
    QStringList parts;
    parts.reserve(font_families_.size());
    for (const QString& f : font_families_) {
        // CSS spec: generic families (monospace, serif, sans-serif) are
        // unquoted; named families get double-quoted to survive families
        // with spaces ("SF Mono", "Cascadia Mono").
        if (f.compare("monospace", Qt::CaseInsensitive) == 0 || f.compare("serif", Qt::CaseInsensitive) == 0 ||
            f.compare("sans-serif", Qt::CaseInsensitive) == 0) {
            parts.append(f.toLower());
        } else {
            // A family name cannot contain the quote character it is wrapped
            // in; no real font does, so drop it rather than escape it.
            QString name = f;
            name.remove(quote);
            parts.append(quote + name + quote);
        }
    }
    return parts.join(QStringLiteral(", "));
}

QStringList ThemeManager::build_font_chain(const QString& preferred) {
    // Cross-platform monospace fallback cascade. Qt's font matcher walks
    // this list and picks the first family installed on the running
    // system. The macOS entries (SF Mono / Menlo / Monaco) come first
    // because that's where the original "Consolas only" default broke —
    // macOS doesn't ship Consolas, the silent fallback was a proportional
    // system font, and QLineEdit/QTextEdit rendered with mismatched
    // glyph metrics (the visible "fragmented" text symptom).
    static const QStringList kDefaults = {
        QStringLiteral("JetBrains Mono"),   // bundled (resources/fonts) — the default on every platform
        QStringLiteral("SF Mono"),          // macOS Mojave+ (system monospace)
        QStringLiteral("Menlo"),            // macOS standard
        QStringLiteral("Monaco"),           // macOS classic
        QStringLiteral("Cascadia Mono"),    // Windows 11 + Terminal
        QStringLiteral("Cascadia Code"),    // Windows 11
        QStringLiteral("Consolas"),         // Windows
        QStringLiteral("DejaVu Sans Mono"), // Linux (Debian/Ubuntu)
        QStringLiteral("Liberation Mono"),  // Linux (Fedora/RHEL)
        QStringLiteral("Source Code Pro"),  // Adobe — common 3rd-party
        QStringLiteral("Courier New"),      // universal but ugly last resort
        QStringLiteral("monospace"),        // Qt generic family
    };
    QStringList chain;
    const QString trimmed = preferred.trimmed();
    if (!trimmed.isEmpty()) {
        // Allow the caller to pass a CSS-style list like
        // "'Consolas','Courier New',monospace" without breaking the
        // single-string font matcher — split, strip quotes, dedup.
        const auto parts = trimmed.split(',', Qt::SkipEmptyParts);
        for (const QString& raw : parts) {
            QString name = raw.trimmed();
            if (name.startsWith('\'') || name.startsWith('"'))
                name = name.mid(1);
            if (name.endsWith('\'') || name.endsWith('"'))
                name.chop(1);
            name = name.trimmed();
            // Reject private/system families (macOS exposes hidden families
            // prefixed with '.', e.g. ".Apple Color Emoji UI",
            // ".AppleSystemUIFont") and emoji/symbol fonts. The emoji font in
            // particular *does* carry ASCII digit glyphs (keycap-emoji bases)
            // but renders them grossly letter-spaced, so if it lands first in
            // the chain every number in the UI renders mangled while letters
            // fall through to the next family. Never let one poison the data
            // font, even if a stale setting asks for it.
            if (name.startsWith('.') || name.contains("emoji", Qt::CaseInsensitive))
                continue;
            if (!name.isEmpty() && !chain.contains(name, Qt::CaseInsensitive))
                chain.append(name);
        }
    }
    for (const QString& f : kDefaults) {
        if (!chain.contains(f, Qt::CaseInsensitive))
            chain.append(f);
    }
    return chain;
}

// ── Setters (no re-style; the apply_* entry points re-style once) ─────────────

void ThemeManager::set_theme_choice(const QString& choice) {
    theme_choice_ = normalise_theme_choice(choice);
    const bool follow = theme_choice_ == QLatin1String("System");
    if (follow && !following_system_) {
        // The operating system switching between its light and dark modes
        // re-resolves the preset. Queued, so the restyle runs after Qt has
        // finished its own handling of the scheme change.
        connect(
            QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                if (theme_choice_ != QLatin1String("System") || resolve_preset() == preset_)
                    return;
                apply_appearance(theme_choice_, {}, 0, {});
            },
            Qt::QueuedConnection);
        following_system_ = true;
    }
    preset_ = resolve_preset();
}

const ThemeTokens* ThemeManager::resolve_preset() const {
    const ThemeTokens* base = &THEME_OBSIDIAN;
    if (theme_choice_ == QLatin1String("Light")) {
        base = &THEME_DAYLIGHT;
    } else if (theme_choice_ == QLatin1String("System")) {
        // Unknown (the platform cannot tell) keeps the terminal's native dark look.
        if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light)
            base = &THEME_DAYLIGHT;
    }
    return direction_variant(base, direction_);
}

void ThemeManager::set_font(const QString& family, int size_px) {
    if (!family.trimmed().isEmpty())
        font_families_ = build_font_chain(family);
    if (size_px > 0)
        font_size_px_ = qBound(kMinFontPx, size_px, kMaxFontPx);
}

void ThemeManager::set_density(const QString& density) {
    density_pad_ = density_to_pad(density);
}

void ThemeManager::sync_tokens() {
    current_ = *preset_;
    // The font tokens follow the user's settings, not the preset — so every
    // stylesheet that says font-family:%1 / font-size:%2px with the tokens
    // gets the font the user picked.
    font_family_css_ = css_font_family_list(QLatin1Char('\'')).toUtf8();
    current_.font_family = font_family_css_.constData();
    current_.font_size_base = font_size_px_;
}

ThemeManager::Snapshot ThemeManager::snapshot() const {
    return Snapshot{preset_, font_family_css_, font_size_px_};
}

// ── Public apply_* entry points ───────────────────────────────────────────────

void ThemeManager::apply_appearance(const QString& theme_choice, const QString& family, int size_px,
                                    const QString& density) {
    const Snapshot before = snapshot();
    restore_direction_palette_once();
    if (!theme_choice.trimmed().isEmpty())
        set_theme_choice(theme_choice);
    else
        preset_ = resolve_preset(); // the direction palette may have been restored
    set_font(family, size_px);
    if (!density.trimmed().isEmpty())
        set_density(density);
    if (theme_choice_ == QLatin1String("System")) {
        // Hand the scheme back to the OS *before* asking it which one it is in.
        QGuiApplication::styleHints()->unsetColorScheme();
        preset_ = resolve_preset();
    }
    sync_tokens();
    // One style recalculation for the whole change: the per-widget stylesheets
    // are rewritten first (stored, not applied), the application stylesheet is
    // stored, the theme_changed handlers run (their stylesheets are stored
    // too), and then every widget is styled exactly once.
    BusyScope busy;
    const bool widgets_restyled = restyle_existing_widgets(before);
    rebuild_and_apply(widgets_restyled, /*defer_widgets=*/true);
    notify_theme_changed();
}

void ThemeManager::apply_theme(const QString& choice) {
    // An empty choice means "re-apply what is in effect".
    apply_appearance(choice.trimmed().isEmpty() ? theme_choice_ : choice, {}, 0, {});
}

void ThemeManager::apply_font(const QString& family, int size_px) {
    apply_appearance({}, family, size_px > 0 ? size_px : kDefaultFontPx, {});
}

void ThemeManager::apply_density(const QString& density) {
    apply_appearance({}, {}, 0, density.isEmpty() ? QStringLiteral("Default") : density);
}

void ThemeManager::apply_typography_and_density(const QString& family, int size_px, const QString& density) {
    apply_appearance({}, family, size_px > 0 ? size_px : kDefaultFontPx,
                     density.isEmpty() ? QStringLiteral("Default") : density);
}

QString ThemeManager::current_theme_name() const {
    return QString::fromLatin1(current_.name);
}

QStringList ThemeManager::available_themes() {
    return {"Dark", "Light", "System"};
}

QStringList ThemeManager::available_densities() {
    return {"Compact", "Default", "Comfortable"};
}

// ── Direction palette ─────────────────────────────────────────────────────────

namespace {
constexpr const char* kDirectionPaletteSettingKey = "appearance.direction_palette";
} // namespace

QString ThemeManager::direction_palette_key(DirectionPalette palette) {
    switch (palette) {
        case DirectionPalette::RedUp:
            return QStringLiteral("RedUp");
        case DirectionPalette::ColorBlindSafe:
            return QStringLiteral("ColorBlindSafe");
        case DirectionPalette::GreenUp:
            break;
    }
    return QStringLiteral("GreenUp");
}

ThemeManager::DirectionPalette ThemeManager::direction_palette_from_key(const QString& key) {
    const QString k = key.trimmed();
    if (k.compare(QLatin1String("RedUp"), Qt::CaseInsensitive) == 0)
        return DirectionPalette::RedUp;
    if (k.compare(QLatin1String("ColorBlindSafe"), Qt::CaseInsensitive) == 0)
        return DirectionPalette::ColorBlindSafe;
    return DirectionPalette::GreenUp;
}

QStringList ThemeManager::available_direction_palettes() {
    return {QStringLiteral("GreenUp"), QStringLiteral("RedUp"), QStringLiteral("ColorBlindSafe")};
}

void ThemeManager::restore_direction_palette_once() {
    if (direction_restored_)
        return;
    QString key = qEnvironmentVariable("FINCEPT_DIRECTION_PALETTE");
    if (key.isEmpty()) {
        const auto r = SettingsRepository::instance().get(QString::fromLatin1(kDirectionPaletteSettingKey));
        if (r.is_err()) {
            // The settings database is not readable yet (or failed) — keep the
            // default and try again on the next apply.
            return;
        }
        key = r.value();
    }
    direction_restored_ = true;
    direction_ = direction_palette_from_key(key);
}

void ThemeManager::set_direction_palette(DirectionPalette palette, bool persist) {
    direction_restored_ = true; // an explicit choice wins over the stored one
    if (persist) {
        const auto r = SettingsRepository::instance().set(QString::fromLatin1(kDirectionPaletteSettingKey),
                                                          direction_palette_key(palette), QStringLiteral("appearance"));
        if (r.is_err())
            LOG_WARN("ThemeManager", QString("could not save the direction palette: %1")
                                         .arg(QString::fromStdString(r.error())));
    }
    if (palette == direction_)
        return;

    const Snapshot before = snapshot();
    direction_ = palette;
    preset_ = resolve_preset();
    sync_tokens();
    // One style recalculation for the whole change — see apply_appearance().
    BusyScope busy;
    const bool widgets_restyled = restyle_existing_widgets(before);
    rebuild_and_apply(widgets_restyled, /*defer_widgets=*/true);
    LOG_DEBUG("ThemeManager", QString("direction palette: %1").arg(direction_palette_key(palette)));
    notify_theme_changed();
    emit direction_palette_changed(direction_);
}

QString ThemeManager::density() const {
    if (density_pad_ == 2)
        return QStringLiteral("Compact");
    if (density_pad_ == 8)
        return QStringLiteral("Comfortable");
    return QStringLiteral("Default");
}

QFont ThemeManager::current_font() const {
    QFont f;
    // setFamilies (Qt 5.13+) accepts an explicit fallback list — Qt walks
    // it natively and picks the first available family. This is the right
    // primitive on macOS where the legacy single-string ctor with a
    // missing primary (e.g. "Consolas") silently resolved to a proportional
    // system font.
    f.setFamilies(font_families_);
    f.setStyleHint(QFont::Monospace);
    // Grayscale anti-aliasing everywhere, never subpixel (ClearType / LCD).
    // Subpixel AA paints red / blue fringes on the edges of glyphs; on the
    // neutral black-and-grey surfaces those fringes read as a colour cast on
    // small grey text. PreferAntialias alone leaves the choice to the
    // platform, so NoSubpixelAntialias is what actually turns it off on
    // Windows and Linux.
    f.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::NoSubpixelAntialias));
    f.setPixelSize(font_size_px_);
    return f;
}

void ThemeManager::apply_color_scheme() {
    // Tell Qt which scheme the application is in, so natively drawn parts —
    // the Windows title bar, native dialogs, the platform style's own
    // palette — match the preset. "System" leaves the decision with the OS
    // (apply_appearance has already unset any override).
    if (theme_choice_ == QLatin1String("System"))
        return;
    QStyleHints* hints = QGuiApplication::styleHints();
    const Qt::ColorScheme want = current_.is_dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
    if (hints->colorScheme() != want)
        hints->setColorScheme(want);
}

void ThemeManager::apply_palette() {
    // Widgets and Qt-drawn parts that no stylesheet rule reaches (item view
    // internals, QtCharts defaults, placeholder text, rich-text links, text
    // selection in documents) take their colours from the palette.
    const auto& t = current_;
    QPalette p;
    p.setColor(QPalette::Window, QColor(t.bg_base));
    p.setColor(QPalette::WindowText, QColor(t.text_primary));
    p.setColor(QPalette::Base, QColor(t.bg_surface));
    p.setColor(QPalette::AlternateBase, QColor(t.row_alt));
    p.setColor(QPalette::Text, QColor(t.text_primary));
    p.setColor(QPalette::Button, QColor(t.bg_raised));
    p.setColor(QPalette::ButtonText, QColor(t.text_primary));
    p.setColor(QPalette::BrightText, QColor(t.text_on_accent));
    p.setColor(QPalette::ToolTipBase, QColor(t.bg_surface));
    p.setColor(QPalette::ToolTipText, QColor(t.text_primary));
    p.setColor(QPalette::PlaceholderText, QColor(t.text_tertiary));
    p.setColor(QPalette::Highlight, QColor(t.accent));
    p.setColor(QPalette::HighlightedText, QColor(t.text_on_accent));
    p.setColor(QPalette::Link, QColor(t.cyan));
    p.setColor(QPalette::LinkVisited, QColor(t.info));
    p.setColor(QPalette::Light, QColor(t.border_bright));
    p.setColor(QPalette::Midlight, QColor(t.border_med));
    p.setColor(QPalette::Mid, QColor(t.border_med));
    p.setColor(QPalette::Dark, QColor(t.border_dim));
    p.setColor(QPalette::Shadow, QColor(t.is_dark ? "#000000" : "#8a8a94"));
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(t.text_dim));
    if (qApp->palette() != p)
        qApp->setPalette(p);
}

void ThemeManager::rebuild_and_apply(bool force_repolish, bool defer_widgets) {
    QElapsedTimer total;
    total.start();
    LOG_DEBUG("ThemeManager", QString("rebuild_and_apply: theme=%1 (%2) families=[%3] size=%4px density_pad=%5")
                                  .arg(theme_choice_, current_theme_name(), font_families_.join(", "))
                                  .arg(font_size_px_)
                                  .arg(density_pad_));

    apply_color_scheme();
    apply_palette();

    const QFont f = current_font();
    if (qApp->font() != f) {
        LOG_DEBUG("ThemeManager",
                  QString("Applying new QFont to qApp (resolved family: %1)").arg(QFontInfo(f).family()));
        qApp->setFont(f);
    }

    QString qss = build_global_qss();
    if (qss != cached_qss_ || !applied_once_ || force_repolish) {
        LOG_DEBUG("ThemeManager", QString("QSS changed (%1 chars) — applying the application stylesheet").arg(qss.length()));
        cached_qss_ = qss;
        QElapsedTimer timer;
        timer.start();
        const bool single_pass = set_application_stylesheet(qss, defer_widgets);
        LOG_DEBUG("ThemeManager", QString("application stylesheet %1 in %2 ms")
                                      .arg(single_pass && defer_widgets ? QStringLiteral("stored") : QStringLiteral("applied"))
                                      .arg(timer.elapsed()));
        applied_once_ = true;
    } else {
        LOG_DEBUG("ThemeManager", "QSS unchanged — skipping setStyleSheet");
    }
    LOG_INFO("ThemeManager", QString("Theme applied theme=%1 resolved=%2 font=\"%3\" size_px=%4 density_pad=%5 "
                                     "duration_ms=%6")
                                 .arg(theme_choice_, current_theme_name(), QFontInfo(f).family())
                                 .arg(font_size_px_)
                                 .arg(density_pad_)
                                 .arg(total.elapsed()));
}

QString ThemeManager::build_global_qss() const {
    const auto& t = current_;
    const int p = density_pad_;
    const int p2 = density_pad_ * 2;
    // Sizes of the fixed-role text styles, relative to the user's base size.
    const auto fpx = [this](int offset) { return QString::number(qMax(kFloorFontPx, font_size_px_ + offset)); };

    QString qss = QString(R"(
        * {
            font-family: %15;
            font-size: %16px;
        }
        QWidget {
            background-color: %1;
            color: %2;
        }
        QScrollBar:vertical {
            background: transparent; width: 5px; margin: 0;
        }
        QScrollBar::handle:vertical { background: %3; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background: %4; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent; height: 5px; margin: 0;
        }
        QScrollBar::handle:horizontal { background: %3; min-width: 20px; }
        QScrollBar::handle:horizontal:hover { background: %4; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background: %5; color: %2; border: 1px solid %3;
            padding: %7px %8px;
            selection-background-color: %9; selection-color: %17;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %4;
        }
        QPushButton {
            background: %5; color: %10; border: 1px solid %3;
            padding: %7px %8px;
        }
        QPushButton:hover { background: %11; color: %2; }
        QPushButton:pressed { background: %12; }
        QPushButton:disabled { color: %13; }
        QComboBox {
            background: %5; color: %2; border: 1px solid %3;
            padding: %7px %8px;
        }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox QAbstractItemView {
            background: %5; color: %2; border: 1px solid %3;
            selection-background-color: %11; selection-color: %2;
        }
        QHeaderView::section {
            background: %5; color: %13; border: none;
            border-bottom: 1px solid %6; padding: %7px %8px;
            font-weight: 600;
        }
        QTableView, QTreeView, QListView {
            background: %1; alternate-background-color: %14;
            gridline-color: %11; border: none;
        }
        QTableView::item, QTreeView::item, QListView::item {
            padding: 1px %7px; border-bottom: 1px solid %11;
        }
        QTableView::item:selected, QTreeView::item:selected, QListView::item:selected {
            background: %12; color: %2;
        }
        QSplitter::handle { background: %6; }
        QSplitter::handle:hover { background: %4; }
        QTabBar::tab {
            background: %5; color: %13; border: 1px solid %6;
            padding: %7px %8px;
        }
        QTabBar::tab:selected {
            background: %11; color: %2; border-bottom: 1px solid %9;
        }
        QToolTip {
            background: %5; color: %2; border: 1px solid %3;
            padding: %7px %8px;
        }
        QCheckBox { color: %10; }
        QCheckBox::indicator {
            width: 12px; height: 12px; border: 1px solid %4; background: %5;
        }
        QCheckBox::indicator:checked { background: %9; border-color: %9; }
        QProgressBar {
            background: %5; border: 1px solid %6; color: %13;
        }
        QProgressBar::chunk { background: %9; }
        QMenu { background: %5; color: %2; border: 1px solid %3; }
        QMenu::item:selected { background: %12; }
        QMenu::separator { background: %6; height: 1px; }

        QScrollArea { border: none; background: %1; }
        /* App-level QFrame transparent rule would erase ADS tab/title styling,
           so re-assert ADS widget backgrounds below — later, more-specific rules win. */
        QFrame { background: transparent; border: none; }
        ads--CDockContainerWidget { background: %1; border: none; }
        ads--CDockAreaWidget { background: %1; border: none; }
        ads--CDockWidget { background: %1; border: none; }
        ads--CDockAreaTitleBar {
            background: %12;
            border-bottom: 1px solid %6;
            padding: 0px;
            min-height: @DOCKBAR@px;
            max-height: @DOCKBAR@px;
        }
        ads--CDockAreaWidget[focused="true"] ads--CDockAreaTitleBar {
            background: %12;
            border-bottom: 1px solid %9;
        }
        ads--CDockWidgetTab {
            background: %5;
            border: none;
            border-right: 1px solid %6;
            padding: 2px 8px;
            min-height: @DOCKTAB@px;
            max-height: @DOCKTAB@px;
        }
        ads--CDockWidgetTab[activeTab="true"] {
            background: %12;
            border-bottom: 2px solid %9;
            border-right: 1px solid %6;
        }
        ads--CDockWidgetTab[focused="true"] {
            background: %12;
            border-bottom: 2px solid %9;
            border-right: 1px solid %6;
        }
        /* '>' (the tab's own labels are its direct children), not ' ': a
           descendant rule makes Qt walk the whole parent chain of every QLabel
           in every screen looking for a dock tab, each time it styles one. */
        ads--CDockWidgetTab > QLabel {
            color: %10;
            font-size: @FS-3@px;
            font-weight: 600;
            background: transparent;
        }
        ads--CDockWidgetTab[activeTab="true"] > QLabel { color: %2; }
        ads--CDockWidgetTab[focused="true"] > QLabel { color: %2; }
        ads--CDockSplitter::handle { background-color: %6; }
        ads--CTitleBarButton {
            padding: 2px; max-width: 16px; max-height: 16px;
            min-width: 16px; min-height: 16px;
            background: transparent; border: none;
        }
        ads--CTitleBarButton:hover { background: %11; }
        ads--CAutoHideSideBar { background: %1; border: none; }
        QScrollArea#dockWidgetScrollArea { padding: 0px; border: none; }

        QTabWidget::pane { border: none; background: %1; }

        QSpinBox, QDoubleSpinBox {
            background: %5; color: %2; border: 1px solid %3;
            padding: %7px %8px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid %4; }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            border: none; background: %11; width: 16px;
        }

        QTextBrowser {
            background: %1; color: %2; border: none; padding: 12px;
        }

        QToolButton {
            background: transparent; color: %10; border: none;
            padding: %7px %8px;
        }
        QToolButton:hover { background: %11; color: %2; }
        QToolButton:pressed { background: %12; }
        QToolButton:checked { background: %12; color: %2; border-bottom: 2px solid %9; }

        QRadioButton { color: %10; }
        QRadioButton::indicator {
            width: 12px; height: 12px; border: 1px solid %4;
            border-radius: 6px; background: %5;
        }
        QRadioButton::indicator:checked {
            background: %9; border-color: %9;
        }

        QGroupBox {
            border: 1px solid %6; margin-top: 12px;
            padding: %8px %7px; color: %10;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 8px;
            padding: 0 4px; color: %9;
        }

        QSlider::groove:horizontal {
            height: 4px; background: %6; border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 12px; height: 12px; margin: -4px 0;
            background: %9; border-radius: 6px;
        }
        QSlider::handle:horizontal:hover { background: %2; }

        QStatusBar { border: none; background: %1; padding: 0; }
        QToolBar { border: none; background: %1; spacing: 0; padding: 0; }

        /* ── ADS icon buttons ───────────────────────────────────────────── */
        #tabCloseButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabCloseButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabCloseButton:pressed { background: %12; }
        ads--CDockWidgetTab[focused="true"] > #tabCloseButton:hover { background: %12; }
        #tabsMenuButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='5' x2='13' y2='5' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='8' x2='13' y2='8' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='11' x2='13' y2='11' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabsMenuButton::menu-indicator { image: none; }
        #tabsMenuButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='5' x2='13' y2='5' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='8' x2='13' y2='8' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='11' x2='13' y2='11' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaCloseButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaCloseButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #detachGroupButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><rect x='3' y='7' width='6' height='6' fill='none' stroke='@ICON@' stroke-width='1.5' stroke-linejoin='round'/><polyline points='9,3 13,3 13,7' fill='none' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'/><line x1='8' y1='8' x2='13' y2='3' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #detachGroupButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><rect x='3' y='7' width='6' height='6' fill='none' stroke='@ICONH@' stroke-width='1.5' stroke-linejoin='round'/><polyline points='9,3 13,3 13,7' fill='none' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'/><line x1='8' y1='8' x2='13' y2='3' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaAutoHideButton, #dockAreaMinimizeButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='12' x2='13' y2='12' stroke='@ICON@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaAutoHideButton:hover, #dockAreaMinimizeButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='12' x2='13' y2='12' stroke='@ICONH@' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        /* ── Reusable object-name selectors ────────────────────────────── */
        #sectionTitle {
            color: %9; font-size: @FS-1@px; font-weight: 700;
            background: transparent; border: none; padding: 0;
        }
        #sectionHeader {
            color: %10; font-size: @FS-3@px; font-weight: 600;
            text-transform: uppercase; letter-spacing: 1px;
            background: transparent; border: none;
            border-bottom: 1px solid %6; padding-bottom: 4px;
        }
        #cardFrame {
            background: %5; border: 1px solid %6; border-radius: 6px;
            padding: %8px;
        }
        #commandBar {
            background: %5; border-bottom: 1px solid %6;
            padding: %7px %8px;
        }
        #accentButton {
            background: %9; color: %17; border: none;
            padding: %7px %8px; font-weight: 600;
        }
        #accentButton:hover { background: %4; }
        #accentButton:pressed { background: %12; }
        #mutedButton {
            background: %12; color: %10; border: 1px solid %6;
            padding: %7px %8px;
        }
        #mutedButton:hover { background: %11; color: %2; }
        #statusLabel {
            color: %10; font-size: @FS-3@px; background: transparent;
            border: none; padding: 0;
        }
        #dimLabel {
            color: %13; font-size: @FS-3@px; background: transparent;
            border: none; padding: 0;
        }
        #dataLabel {
            color: %10; font-size: @FS-1@px; background: transparent;
            border: none; padding: 0;
        }
        #dataValue {
            color: %2; font-size: @FS-1@px; font-weight: 600;
            background: transparent; border: none; padding: 0;
        }
        #positiveText { color: %18; }
        #negativeText { color: %19; }
        #warningText  { color: %20; }
    )")
        .arg(t.bg_base)              // %1
        .arg(t.text_primary)         // %2
        .arg(t.border_med)           // %3
        .arg(t.border_bright)        // %4
        .arg(t.bg_surface)           // %5
        .arg(t.border_dim)           // %6
        .arg(p)                      // %7  padding
        .arg(p2)                     // %8  double padding
        .arg(t.accent)               // %9
        .arg(t.text_secondary)       // %10
        .arg(t.bg_hover)             // %11
        .arg(t.bg_raised)            // %12
        .arg(t.text_dim)             // %13
        .arg(t.row_alt)              // %14
        .arg(css_font_family_list(QLatin1Char('"'))) // %15
        .arg(font_size_px_)          // %16
        .arg(t.text_on_accent)       // %17
        .arg(t.positive)             // %18
        .arg(t.negative)             // %19
        .arg(t.warning);             // %20
    // The icon colours are not %-placeholders (they are @ICON@ / @ICONH@
    // below): passing them through .arg() shifted every later value one
    // placeholder down and left #positiveText / #negativeText / #warningText
    // with the wrong colours.
    // NOTE: the chain stops at %20 on purpose — QString::arg fills placeholders
    // by ASCENDING number across the whole string, so every further value is
    // substituted by name below instead of as %23+. (A "%23" is also what a
    // URL-encoded '#' looks like, which is how the inline SVG icon data-URIs
    // must spell their stroke colours for Qt's QSS URL decoder.)
    const auto svg_color = [](const char* hex) { return QStringLiteral("%23") + QString::fromLatin1(hex).mid(1); };
    qss.replace(QLatin1String("@ICONH@"), svg_color(t.icon_hover));
    qss.replace(QLatin1String("@ICON@"), svg_color(t.icon_dim));
    qss.replace(QLatin1String("@FS-3@"), fpx(-3));
    qss.replace(QLatin1String("@FS-1@"), fpx(-1));
    // Dock tabs and their title bar are sized from the tab label font so the
    // labels do not clip at large font sizes (22 / 24px at the default size).
    qss.replace(QLatin1String("@DOCKTAB@"), QString::number(dock_tab_height()));
    qss.replace(QLatin1String("@DOCKBAR@"), QString::number(dock_tab_height() + 2));
    return qss;
}

QString ThemeManager::build_ads_qss() const {
    const auto& t = current_;
    LOG_DEBUG("ThemeManager", QString("build_ads_qss: bg_base=%1 bg_raised=%2 bg_surface=%3 accent=%4 border_dim=%5")
                                 .arg(t.bg_base)
                                 .arg(t.bg_raised)
                                 .arg(t.bg_surface)
                                 .arg(t.accent)
                                 .arg(t.border_dim));
    // Applied to CDockManager to override ADS's default.css/focus_highlighting.css palette() colours.
    QString qss = QString(R"(
        ads--CDockContainerWidget { background: %1; }
        ads--CDockAreaWidget { background: %1; }
        ads--CDockAreaTitleBar {
            background: %2;
            border-bottom: 1px solid %3;
            padding: 0px;
            min-height: @DOCKBAR@px;
            max-height: @DOCKBAR@px;
        }
        ads--CDockAreaWidget[focused="true"] ads--CDockAreaTitleBar {
            background: %2;
            border-bottom: 1px solid %4;
        }
        ads--CDockWidgetTab {
            background: %5;
            border: none;
            border-right: 1px solid %3;
            padding: 2px 8px;
            min-height: @DOCKTAB@px;
            max-height: @DOCKTAB@px;
        }
        ads--CDockWidgetTab[activeTab="true"] {
            background: %2;
            border-bottom: 2px solid %4;
            border-right: 1px solid %3;
        }
        ads--CDockWidgetTab[focused="true"] {
            background: %2;
            border-bottom: 2px solid %4;
            border-right: 1px solid %3;
        }
        ads--CDockWidgetTab > QLabel {
            color: %6;
            font-size: %9px;
            font-weight: 600;
            background: transparent;
        }
        ads--CDockWidgetTab[activeTab="true"] > QLabel {
            color: %7;
        }
        ads--CDockWidgetTab[focused="true"] > QLabel {
            color: %7;
        }
        ads--CDockWidget { background: %1; border: none; }
        ads--CDockSplitter::handle { background-color: %3; }
        ads--CTitleBarButton {
            padding: 2px; max-width: 16px; max-height: 16px;
            min-width: 16px; min-height: 16px;
            background: transparent; border: none;
        }
        ads--CTitleBarButton:hover { background: %8; }
        ads--CAutoHideSideBar { background: %1; border: none; }
        QScrollArea#dockWidgetScrollArea { padding: 0px; border: none; }

        /* Drop overlay + splitter hover both use accent for visual consistency across drag/resize. */
        ads--CDockOverlay { background: transparent; }
        ads--CDockOverlayCross { background: transparent; }
        ads--CDockSplitter::handle:hover { background-color: %4; }
    )")
                      .arg(t.bg_base)        // %1
                      .arg(t.bg_raised)      // %2
                      .arg(t.border_dim)     // %3
                      .arg(t.accent)         // %4
                      .arg(t.bg_surface)     // %5
                      .arg(t.text_secondary) // %6
                      .arg(t.text_primary)   // %7
                      .arg(t.bg_hover)       // %8
                      .arg(qMax(kFloorFontPx, font_size_px_ - 3)); // %9

    qss.replace(QLatin1String("@DOCKTAB@"), QString::number(dock_tab_height()));
    qss.replace(QLatin1String("@DOCKBAR@"), QString::number(dock_tab_height() + 2));
    LOG_DEBUG("ThemeManager", QString("build_ads_qss: produced %1 chars").arg(qss.length()));
    return qss;
}

} // namespace fincept::ui
