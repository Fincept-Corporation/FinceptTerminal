// ThemeRestyle.cpp — the parts of ThemeManager that keep widgets which already
// exist in step with an appearance change.
//
// Most of the terminal styles its widgets once, when they are built:
//
//     label->setStyleSheet(QString("color:%1; font-size:%2px;")
//                              .arg(ui::colors::TEXT_PRIMARY()).arg(ui::fonts::font_px(-3)));
//
// The string handed to Qt holds the values the tokens had at that moment, so a
// later theme or font change would not reach it. Rather than require every one
// of several hundred files to rebuild its stylesheets on theme_changed, the
// manager translates them centrally: after an appearance change it walks every
// widget that carries its own stylesheet and rewrites
//
//   * each colour that is a token of the outgoing preset (as #rrggbb,
//     #aarrggbb or rgb()/rgba()) to the same token of the incoming preset,
//   * each derived colour produced by the helpers below (adapt_foreground,
//     tint_background, overlay_rgba) to the same derivation under the incoming
//     preset — the helpers record what they produced for exactly this purpose,
//   * the token font-family list to the new one,
//   * each `font-size: Npx` by the change in the base font size — a size the
//     legibility floor clamped carries its real offset in a comment marker
//     (FontSizeText.h), so it comes back exactly after any number of changes.
//
// A stylesheet ThemeManager builds itself for a widget (the dock manager's,
// apply_ads_qss()) is rebuilt instead: it holds sizes derived from the font
// that no font-size rewrite reaches.
//
// What it cannot reach — colours cached in member variables, pixmaps, item
// brushes, rich-text documents, custom painting — is what theme_changed is
// for; widgets that own such state still connect to it and refresh themselves.

#include "core/logging/Logger.h"
#include "ui/theme/FontSizeText.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QVariant>
#include <QWidget>

#include <cmath>
#include <utility>

#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
#include <private/qwidget_p.h>
#endif

namespace fincept::ui {

namespace {

// ── Colour arithmetic ────────────────────────────────────────────────────────

/// WCAG relative luminance of an sRGB colour (0 = black, 1 = white).
double luminance(const QColor& c) {
    const auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

/// WCAG contrast ratio between two colours (1 = identical, 21 = black on white).
double contrast(const QColor& a, const QColor& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

QColor mix(const QColor& base, const QColor& over, double amount) {
    const double k = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(base.redF() + (over.redF() - base.redF()) * k,
                            base.greenF() + (over.greenF() - base.greenF()) * k,
                            base.blueF() + (over.blueF() - base.blueF()) * k);
}

// ── Derived-colour recipes ───────────────────────────────────────────────────

struct Recipe {
    enum Kind { Adapt, Tint, Overlay } kind = Adapt;
    QRgb source = 0;    // Adapt / Tint: the colour the caller passed
    double param = 0.0; // Tint: strength, Overlay: alpha
};

/// Minimum contrast an adapted foreground must reach against the light
/// preset's card surface. 4.0 keeps coloured data text readable while leaving
/// each hue recognisable; body text tokens are well above it.
constexpr double kAdaptMinContrast = 4.0;

QColor eval_adapt(const QColor& src, const ThemeTokens& preset) {
    if (preset.is_dark)
        return src; // these colours were chosen against the dark preset
    const QColor surface(preset.bg_surface);
    if (contrast(src, surface) >= kAdaptMinContrast)
        return src;
    float h = 0, s = 0, l = 0, a = 1;
    src.getHslF(&h, &s, &l, &a);
    QColor out = src;
    // Walk the lightness down until the colour reads on the light surface.
    for (int i = 0; i < 40 && l > 0.0f; ++i) {
        l = qMax(0.0f, l - 0.025f);
        out = QColor::fromHslF(h, s, l, a);
        if (contrast(out, surface) >= kAdaptMinContrast)
            break;
    }
    return out;
}

QColor eval_tint(const QColor& src, double strength, const ThemeTokens& preset) {
    return mix(QColor(preset.bg_base), src, strength);
}

QString format_alpha(double alpha) {
    return QString::number(qBound(0.0, alpha, 1.0), 'g', 3);
}

QString eval_overlay(double alpha, const ThemeTokens& preset) {
    return (preset.is_dark ? QStringLiteral("rgba(255,255,255,%1)") : QStringLiteral("rgba(0,0,0,%1)"))
        .arg(format_alpha(alpha));
}

QString eval_recipe(const Recipe& r, const ThemeTokens& preset) {
    switch (r.kind) {
        case Recipe::Adapt:
            return eval_adapt(QColor::fromRgb(r.source), preset).name();
        case Recipe::Tint:
            return eval_tint(QColor::fromRgb(r.source), r.param, preset).name();
        case Recipe::Overlay:
            return eval_overlay(r.param, preset);
    }
    return {};
}

/// What each preset's derived colours were derived from, keyed by the exact
/// text the helper returned. Guarded by a mutex because HTML / stylesheet
/// strings are occasionally assembled off the GUI thread.
QMutex g_recipe_mutex;
QHash<const ThemeTokens*, QHash<QString, Recipe>> g_recipes;

void remember(const ThemeTokens* preset, const QString& output, const Recipe& recipe) {
    QMutexLocker lock(&g_recipe_mutex);
    g_recipes[preset].insert(output, recipe);
}

bool lookup(const ThemeTokens* preset, const QString& output, Recipe* out) {
    QMutexLocker lock(&g_recipe_mutex);
    const auto preset_it = g_recipes.constFind(preset);
    if (preset_it == g_recipes.constEnd())
        return false;
    const auto it = preset_it->constFind(output);
    if (it == preset_it->constEnd())
        return false;
    *out = *it;
    return true;
}

// ── Token colour table ───────────────────────────────────────────────────────

using TokenField = const char* ThemeTokens::*;

/// Every colour token. Order matters only when a preset breaks the
/// "all values distinct" rule: the first token with a given value wins.
const TokenField kColorFields[] = {
    &ThemeTokens::bg_base,      &ThemeTokens::bg_surface,   &ThemeTokens::bg_raised,     &ThemeTokens::bg_hover,
    &ThemeTokens::border_dim,   &ThemeTokens::border_med,   &ThemeTokens::border_bright, &ThemeTokens::text_primary,
    &ThemeTokens::text_secondary, &ThemeTokens::text_tertiary, &ThemeTokens::text_dim,   &ThemeTokens::accent,
    &ThemeTokens::accent_dim,   &ThemeTokens::text_on_accent, &ThemeTokens::icon_dim,    &ThemeTokens::icon_hover,
    &ThemeTokens::positive,     &ThemeTokens::positive_dim, &ThemeTokens::negative,      &ThemeTokens::negative_dim,
    &ThemeTokens::warning,      &ThemeTokens::info,         &ThemeTokens::cyan,          &ThemeTokens::accent_bg,
    &ThemeTokens::positive_bg,  &ThemeTokens::negative_bg,  &ThemeTokens::row_alt,       &ThemeTokens::success,
    &ThemeTokens::danger,       &ThemeTokens::chart_neutral, &ThemeTokens::seq_low,      &ThemeTokens::seq_high,
};

QRgb rgb_only(const QColor& c) {
    return c.rgb() & 0x00ffffffu;
}

/// Rewrites one stylesheet from the appearance it was built with to another.
class Translator {
  public:
    Translator(const ThemeTokens* from, const ThemeTokens* to, const QByteArray& from_family,
               const QByteArray& to_family, int from_px, int to_px)
        : from_(from), to_(to), from_family_(QString::fromUtf8(from_family)), to_family_(QString::fromUtf8(to_family)),
          from_px_(from_px), to_px_(to_px), colors_change_(from != to) {
        if (colors_change_) {
            for (TokenField f : kColorFields) {
                const QRgb key = rgb_only(QColor(from->*f));
                if (!token_map_.contains(key))
                    token_map_.insert(key, rgb_only(QColor(to->*f)));
            }
            // Categorical chart colours, slot for slot (chart_colors mirrors
            // the first six slots, so it needs no entries of its own).
            for (size_t i = 0; i < from->chart_palette.size(); ++i) {
                const QRgb key = rgb_only(QColor(from->chart_palette[i]));
                if (!token_map_.contains(key))
                    token_map_.insert(key, rgb_only(QColor(to->chart_palette[i])));
            }
        }
    }

    /// `in` rewritten from the outgoing appearance to the incoming one.
    QString translate(const QString& in) const {
        QString text = in;
        if (from_family_ != to_family_ && !from_family_.isEmpty())
            text.replace(from_family_, to_family_);

        static const QRegularExpression re(
            QStringLiteral(
                // 1: hex colour
                "(#[0-9a-fA-F]{3,8})(?![0-9A-Za-z_-])"
                // 2: rgb()/rgba(), 3-5: channels, 6: alpha (optional)
                "|(rgba?\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*(?:,\\s*([0-9.]+%?)\\s*)?\\))"
                "|") +
                // 7: "font-size:" 8: offset marker (optional) 9: number
                font_size_text::pattern(),
            QRegularExpression::CaseInsensitiveOption);

        QString out;
        out.reserve(text.size() + 16);
        int last = 0;
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out += QStringView(text).mid(last, m.capturedStart() - last);
            last = m.capturedEnd();
            if (m.hasCaptured(1)) {
                out += colors_change_ ? map_hex(m.captured(1)) : m.captured(1);
            } else if (m.hasCaptured(2)) {
                out += colors_change_ ? map_rgb(m) : m.captured(2);
            } else {
                out += font_size_text::rewrite(m, 7, from_px_, to_px_, ThemeManager::kFloorFontPx);
            }
        }
        out += QStringView(text).mid(last);
        return out;
    }

  private:
    /// A recipe derived from a token colour (a tint of POSITIVE, say) is
    /// re-derived from that token's value in the incoming preset, so it matches
    /// what a fresh build would produce — and so a tinted up/down background
    /// follows a direction-palette switch.
    Recipe translate_recipe(Recipe r) const {
        if (r.kind == Recipe::Adapt || r.kind == Recipe::Tint) {
            const auto found = token_map_.constFind(r.source & 0x00ffffffu);
            if (found != token_map_.constEnd())
                r.source = (r.source & 0xff000000u) | *found;
        }
        return r;
    }

    QString map_hex(const QString& hex) const {
        const QString lower = hex.toLower();
        Recipe recipe;
        if (lookup(from_, lower, &recipe)) {
            recipe = translate_recipe(recipe);
            const QString v = eval_recipe(recipe, *to_);
            remember(to_, v, recipe);
            return v;
        }
        const int digits = lower.size() - 1;
        if (digits == 3 || digits == 6) {
            const QColor c(lower);
            const auto found = token_map_.constFind(rgb_only(c));
            return found == token_map_.constEnd() ? hex : QColor::fromRgb(*found).name();
        }
        if (digits == 8) { // Qt stylesheets spell this #AARRGGBB
            const QColor c(QLatin1Char('#') + lower.mid(3));
            const auto found = token_map_.constFind(rgb_only(c));
            return found == token_map_.constEnd() ? hex : lower.left(3) + QColor::fromRgb(*found).name().mid(1);
        }
        return hex;
    }

    QString map_rgb(const QRegularExpressionMatch& m) const {
        const QString whole = m.captured(2);
        Recipe recipe;
        if (lookup(from_, whole, &recipe)) {
            recipe = translate_recipe(recipe);
            const QString v = eval_recipe(recipe, *to_);
            remember(to_, v, recipe);
            return v;
        }
        const int r = m.captured(3).toInt();
        const int g = m.captured(4).toInt();
        const int b = m.captured(5).toInt();
        if (r > 255 || g > 255 || b > 255)
            return whole;
        const QString alpha = m.captured(6);

        // A hand-written faint white veil — the dark-theme idiom for a hover
        // wash or a hairline — means "a little contrast", so on a light preset
        // it becomes the same amount of black. Recorded so it flips back.
        if (from_->is_dark && !to_->is_dark && r == 255 && g == 255 && b == 255 && !alpha.isEmpty() &&
            !alpha.endsWith(QLatin1Char('%'))) {
            bool ok = false;
            const double a = alpha.toDouble(&ok);
            if (ok && a <= 0.3) {
                const Recipe veil{Recipe::Overlay, 0, a};
                const QString v = eval_overlay(a, *to_);
                remember(to_, v, veil);
                return v;
            }
        }

        const auto found = token_map_.constFind(rgb_only(QColor(r, g, b)));
        if (found == token_map_.constEnd())
            return whole;
        const QColor c = QColor::fromRgb(*found);
        return alpha.isEmpty()
                   ? QStringLiteral("rgb(%1,%2,%3)").arg(c.red()).arg(c.green()).arg(c.blue())
                   : QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
    }

    const ThemeTokens* from_;
    const ThemeTokens* to_;
    QString from_family_;
    QString to_family_;
    int from_px_;
    int to_px_;
    bool colors_change_;
    QHash<QRgb, QRgb> token_map_;
};

/// Which stylesheet ThemeManager built for this widget ("ads": build_ads_qss()),
/// so the pass rebuilds it rather than translating it.
constexpr const char* kPropThemeSheet = "_fincept_theme_sheet";
/// Set to true on a widget (setProperty("fincept_no_restyle", true)) to keep the
/// pass away from its stylesheet.
constexpr const char* kPropNoRestyle = "fincept_no_restyle";

} // namespace

// ── Stylesheets this manager builds for a widget ────────────────────────────

void ThemeManager::apply_ads_qss(QWidget* dock_manager) const {
    if (!dock_manager)
        return;
    dock_manager->setProperty(kPropThemeSheet, QStringLiteral("ads"));
    const QString qss = build_ads_qss();
    if (dock_manager->styleSheet() != qss)
        dock_manager->setStyleSheet(qss);
}

// ── Derived-colour helpers ───────────────────────────────────────────────────

QColor ThemeManager::adapt_foreground(const QColor& color) const {
    if (!color.isValid())
        return color;
    const QColor out = eval_adapt(color, *preset_);
    if (color.alpha() == 255)
        remember(preset_, out.name(), Recipe{Recipe::Adapt, color.rgb(), 0.0});
    return out;
}

QColor ThemeManager::tint_background(const QColor& color, double strength) const {
    if (!color.isValid())
        return color;
    const QColor out = eval_tint(color, strength, *preset_);
    remember(preset_, out.name(), Recipe{Recipe::Tint, color.rgb(), strength});
    return out;
}

QString ThemeManager::overlay_rgba(double alpha) const {
    const QString out = eval_overlay(alpha, *preset_);
    remember(preset_, out, Recipe{Recipe::Overlay, 0, qBound(0.0, alpha, 1.0)});
    return out;
}

// ── Own-border scoping ───────────────────────────────────────────────────────
//
// A widget stylesheet written without a selector —
//
//     card->setStyleSheet("background:#0b0b0b; border:1px solid #262626;");
//
// — is applied by Qt to the widget AND to every widget inside it, so each label
// and row in the card is drawn with the card's border: a box around every
// piece of text. The terminal has hundreds of such cards. Colours and fonts are
// meant to be inherited that way; a border never is. So when a widget is
// polished or restyled with a selector-less stylesheet that draws a border,
// the border declarations are moved into a rule that matches only that widget:
//
//     * { background:#0b0b0b; }  *[_fincept_box="17"] { border:1px solid #262626; }
//
// Everything else keeps cascading exactly as before.

namespace {

constexpr const char* kPropBoxId = "_fincept_box";

/// Split a declaration list on top-level ';' (not inside quotes or parentheses).
QStringList split_declarations(const QString& sheet) {
    QStringList out;
    QString cur;
    int depth = 0;
    QChar quote;
    for (const QChar c : sheet) {
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(')')) {
            depth = qMax(0, depth - 1);
        } else if (c == QLatin1Char(';') && depth == 0) {
            if (!cur.trimmed().isEmpty())
                out.append(cur.trimmed());
            cur.clear();
            continue;
        }
        cur.append(c);
    }
    if (!cur.trimmed().isEmpty())
        out.append(cur.trimmed());
    return out;
}

/// A selector-less stylesheet split for scoping: the declarations that keep
/// cascading, and the border / outline ones that must stay on the widget.
/// `scoped` is false when the sheet needs no change.
struct BorderSplit {
    bool scoped = false;
    QString inherited; // "* { …; } " or empty
    QString own;       // "border…; outline…" (no braces)
};

BorderSplit split_own_border(const QString& sheet) {
    BorderSplit out;
    if (sheet.isEmpty() || sheet.contains(QLatin1Char('{')) || !sheet.contains(QLatin1String("border")))
        return out;
    static const QRegularExpression none_value(QStringLiteral("^(none|0|0px)(\\s|$)"),
                                               QRegularExpression::CaseInsensitiveOption);
    QStringList inherited;
    QStringList own;
    bool draws_border = false;
    for (const QString& decl : split_declarations(sheet)) {
        const int colon = decl.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            return out; // not a plain declaration list — leave it alone
        const QString name = decl.left(colon).trimmed().toLower();
        const QString value = decl.mid(colon + 1).trimmed();
        if (name.startsWith(QLatin1String("border")) || name == QLatin1String("outline")) {
            own.append(decl);
            if (!name.startsWith(QLatin1String("border-radius")) && !none_value.match(value).hasMatch())
                draws_border = true;
        } else {
            inherited.append(decl);
        }
    }
    if (!draws_border)
        return out; // "border: none" and the like are harmless to inherit
    out.scoped = true;
    if (!inherited.isEmpty())
        out.inherited = QStringLiteral("* { ") + inherited.join(QStringLiteral("; ")) + QStringLiteral("; } ");
    out.own = own.join(QStringLiteral("; "));
    return out;
}

/// split_own_border() for `sheet`, memoised. The split is a pure function of
/// the text, and the same few hundred sheets arrive many thousands of times —
/// the filter sees every Polish and StyleChange of every widget that carries
/// one, and a re-polish sends StyleChange to a whole subtree. GUI thread only.
const BorderSplit& split_own_border_cached(const QString& sheet) {
    static QHash<QString, BorderSplit> cache;
    auto it = cache.constFind(sheet);
    if (it != cache.constEnd())
        return *it;
    if (cache.size() >= 4096)
        cache.clear(); // bounded: sheets with live values (prices, %) are not reused
    return *cache.insert(sheet, split_own_border(sheet));
}

/// The scoped form of `sheet` for the widget whose box id is `box_id`.
QString compose_scoped(const BorderSplit& split, const QString& box_id) {
    return split.inherited + QStringLiteral("*[%1=\"%2\"] { ").arg(QLatin1String(kPropBoxId), box_id) + split.own +
           QStringLiteral("; }");
}

/// Swap in the scoped sheet during `w`'s own first polish without the second
/// style pass. The Polish event arrives after Qt has marked `w` polished but
/// before it styles it, so QWidget::setStyleSheet() re-styles `w` and its
/// whole subtree on the spot — and then the polish under way styles them all
/// again (~0.8 s measured for a large view). Nothing has been styled with the
/// old text yet, though: `w` is styled next, from the new text, and each child
/// right after it. So the text is stored without the immediate pass, which is
/// exactly what setStyleSheet() does for a widget that is not yet polished.
/// Not when a descendant is already polished (its style would keep the old
/// text): returns false then, and the caller falls back to setStyleSheet().
bool store_sheet_during_first_polish(QWidget* w, const QString& sheet) {
#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
    const auto descendants = w->findChildren<QWidget*>();
    for (QWidget* c : descendants) {
        if (QWidgetPrivate::get(c)->polished)
            return false;
    }
    QWidgetPrivate* d = QWidgetPrivate::get(w);
    const QMetaObject* polished = d->polished;
    d->polished = nullptr; // setStyleSheet() on an unpolished widget only stores the text
    w->setStyleSheet(sheet);
    d->polished = polished;
    // Qt keeps a parsed copy of each widget's sheet (for its descendants' rule
    // matching) and drops it when it polishes the widget — but only if the
    // widget's own rules are cached. A descendant that asked the style
    // something early (a spin box or header does in its constructor) may have
    // cached the old text's parse; one style query on `w` caches its rules, so
    // the polish that follows this event drops that copy and parses the new
    // text. Descendants re-compute their own rules when they are polished next.
    (void)w->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, w);
    return true;
#else
    Q_UNUSED(w);
    Q_UNUSED(sheet);
    return false;
#endif
}

/// The border-scoped replacement for `w`'s stylesheet `sheet`, or an empty
/// string when the sheet needs none (it has selectors, draws no border, or the
/// widget opted out). Gives `w` its box id the first time. Cheapest tests
/// first: this runs for every Polish / StyleChange in the application.
QString scoped_replacement(QWidget* w, const QString& sheet) {
    if (sheet.isEmpty() || sheet.contains(QLatin1Char('{')))
        return {};
    const BorderSplit& split = split_own_border_cached(sheet);
    if (!split.scoped || w->property(kPropNoRestyle).toBool())
        return {};
    static quint64 next_id = 0;
    QString id = w->property(kPropBoxId).toString();
    if (id.isEmpty()) {
        id = QString::number(++next_id);
        w->setProperty(kPropBoxId, id);
    }
    return compose_scoped(split, id);
}

} // namespace

namespace {

/// Give `w` a new stylesheet text without making Qt recompute styles for it and
/// everything inside it right away (QWidget::setStyleSheet does, so calling it
/// on several hundred nested widgets recomputes the same subtrees over and
/// over). The caller re-polishes the whole application once afterwards.
/// Returns false when the text could not be stored silently and the normal,
/// immediately re-polishing call was used instead.
bool store_style_sheet(QWidget* w, const QString& sheet) {
#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
    QWidgetPrivate* d = QWidgetPrivate::get(w);
    if (d && d->extra) {
        d->extra->styleSheet = sheet;
        return true;
    }
#endif
    w->setStyleSheet(sheet);
    return false;
}

/// FINCEPT_THEME_LEGACY_RESTYLE=1 switches the appearance change back to Qt's
/// own application-wide re-style (QApplication::setStyleSheet) and to handlers
/// that re-style as they go — the behaviour before the single pass below.
bool legacy_restyle() {
#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
    static const bool legacy = qEnvironmentVariableIntValue("FINCEPT_THEME_LEGACY_RESTYLE") != 0;
    return legacy;
#else
    return true; // the single pass needs to store stylesheet texts silently
#endif
}

/// Widgets whose re-style is owed: they were hidden when the last pass ran
/// (the screens behind the current tab, closed menus and dialogs — most of the
/// application), so the pass skipped them and the event filter styles each one
/// when it is next shown. Styling a widget costs about a quarter of a
/// millisecond inside Qt whatever is done around it; doing it only for what is
/// on screen is what keeps a theme or font change short. Plain addresses: a
/// stale one can at worst style a new widget once more than needed.
/// GUI thread only.
QSet<const QObject*> g_restyle_pending;

/// The same widgets, in the order they were found (a parent before what is
/// inside it), for the idle pass: instead of the whole cost landing on the
/// first visit to each screen after a theme or font change (0.6 s of a 1.3 s
/// Dashboard visit, measured), the hidden widgets are styled a few
/// milliseconds at a time while the application is otherwise idle
/// (restyle_hidden_slice()). A widget shown before its turn is styled by the
/// Show filter as before. Guarded pointers: this list is walked later.
QList<QPointer<QWidget>> g_restyle_idle_queue;
QTimer* g_restyle_idle_timer = nullptr;
constexpr int kIdleRestyleStartMs = 300; // let the change paint first
constexpr int kIdleRestyleGapMs = 4;     // between slices: input and painting go first
constexpr int kIdleRestyleSliceMs = 8;
// For the log line when a background pass completes: how many widgets it
// styled, in how many slices, and the time spent styling.
int g_restyle_idle_styled = 0;
int g_restyle_idle_slices = 0;
qint64 g_restyle_idle_work_ms = 0;
QElapsedTimer g_restyle_idle_clock; // since the pass was queued

/// FINCEPT_THEME_IDLE_RESTYLE=0 leaves every hidden widget for its Show.
bool idle_restyle_enabled() {
    static const bool enabled = !qEnvironmentVariableIsSet("FINCEPT_THEME_IDLE_RESTYLE") ||
                                qEnvironmentVariableIntValue("FINCEPT_THEME_IDLE_RESTYLE") != 0;
    return enabled;
}

/// `w` was not styled by the pass under way because it is hidden.
void note_restyle_pending(QWidget* w) {
    g_restyle_pending.insert(w);
    if (idle_restyle_enabled())
        g_restyle_idle_queue.append(QPointer<QWidget>(w));
}

/// True while the theme_changed handlers run under PolishSuspension, and how
/// many widgets Qt polished meanwhile. Normally a handful: a handler that
/// shows a widget or asks for size hints there makes Qt activate the layouts
/// up to the window, which styles every widget they reach (the suspension
/// makes each look unstyled) — over a second, and from then on every
/// stylesheet a later handler sets re-styles its subtree on the spot. The
/// count is logged so such a handler shows up.
bool g_handler_phase = false;
int g_handler_phase_polished = 0;

/// Re-style one widget: what QStyleSheetStyle does for a widget when its
/// stylesheet changes (polish, then a StyleChange event), with two additions
/// for texts that were stored without Qt noticing (store_style_sheet, and
/// handlers that ran under PolishSuspension):
///   * a selector-less sheet that draws a border is scoped to its widget here,
///     so the StyleChange event below does not find it unscoped and re-style
///     the subtree a second time;
///   * Qt keeps a parsed copy of each widget's sheet and only drops it, when it
///     polishes the widget, if the widget's rules are cached — one style query
///     caches them, so the polish that follows parses the text now in place.
///     Without it a widget keeps the colours of the sheet it had before.
void restyle_widget(QWidget* w) {
    QStyle* style = w->style();
    const QString sheet = w->styleSheet();
    if (!sheet.isEmpty()) {
        const QString scoped = scoped_replacement(w, sheet);
        if (!scoped.isEmpty())
            store_style_sheet(w, scoped);
        (void)style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, w);
    }
    style->polish(w);
    QEvent change(QEvent::StyleChange);
    QCoreApplication::sendEvent(w, &change);
}

/// Re-style `w` and everything shown with it, each widget exactly once, parent
/// before children; hidden children are left for the idle pass or for when
/// they are shown (g_restyle_pending). Returns the number of widgets styled.
int repolish_tree(QWidget* w) {
    if (w->windowType() == Qt::Desktop)
        return 0;
    g_restyle_pending.remove(w); // styled here, whoever asked
    restyle_widget(w);

    // A StyleChange handler may rebuild or delete children: walk a guarded copy.
    QList<QPointer<QWidget>> children;
    const QObjectList& objects = w->children();
    children.reserve(objects.size());
    for (QObject* o : objects) {
        if (o->isWidgetType())
            children.append(static_cast<QWidget*>(o));
    }
    int count = 1;
    for (const QPointer<QWidget>& child : std::as_const(children)) {
        if (!child)
            continue;
        if (child->isHidden())
            note_restyle_pending(child);
        else
            count += repolish_tree(child);
    }
    return count;
}

/// A few milliseconds of the idle pass: style hidden widgets from the front of
/// the queue, one at a time, and queue what is inside each (all of it — under
/// a hidden parent nothing is on screen). Returns whether work is left.
bool restyle_hidden_slice() {
    QElapsedTimer timer;
    timer.start();
    while (!g_restyle_idle_queue.isEmpty()) {
        const QPointer<QWidget> guard = g_restyle_idle_queue.takeFirst();
        QWidget* w = guard.data();
        // Gone, or styled since (it was shown: the Show filter took it).
        if (!w || !g_restyle_pending.contains(w))
            continue;
        // Never styled yet: Qt styles it from the current sheets when it is
        // first shown, and the Show filter follows as it always has.
        if (!w->testAttribute(Qt::WA_WState_Polished))
            continue;
        g_restyle_pending.remove(w);
        restyle_widget(w);
        ++g_restyle_idle_styled;
        if (!guard)
            continue; // a StyleChange handler deleted it
        const QObjectList& objects = w->children();
        for (QObject* o : objects) {
            if (o->isWidgetType())
                note_restyle_pending(static_cast<QWidget*>(o));
        }
        if (timer.elapsed() >= kIdleRestyleSliceMs)
            break;
    }
    ++g_restyle_idle_slices;
    g_restyle_idle_work_ms += timer.elapsed();
    return !g_restyle_idle_queue.isEmpty();
}

#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
/// While alive, QWidget::setStyleSheet() only stores its text: Qt re-styles a
/// widget and its whole subtree on every such call when the widget (or a
/// descendant) has been polished, and the theme_changed handlers make
/// thousands of them — a screen sets a sheet on its root, then on each
/// container inside it, and each call re-styles everything below again
/// (4 s measured with five screens open). The handlers run under this
/// instead, and repolish_all_widgets() styles every widget once afterwards.
/// A widget that Qt polishes meanwhile (first show, sizeHint()) is simply
/// styled then and again in that pass.
class PolishSuspension {
  public:
    PolishSuspension() {
        const QWidgetList widgets = QApplication::allWidgets();
        saved_.reserve(widgets.size());
        for (QWidget* w : widgets) {
            QWidgetPrivate* d = QWidgetPrivate::get(w);
            if (d->polished) {
                saved_.append({QPointer<QWidget>(w), d->polished});
                d->polished = nullptr;
            }
        }
    }
    ~PolishSuspension() {
        for (const auto& entry : std::as_const(saved_)) {
            if (!entry.first)
                continue;
            QWidgetPrivate* d = QWidgetPrivate::get(entry.first);
            if (!d->polished)
                d->polished = entry.second;
        }
    }
    PolishSuspension(const PolishSuspension&) = delete;
    PolishSuspension& operator=(const PolishSuspension&) = delete;

  private:
    QList<std::pair<QPointer<QWidget>, const QMetaObject*>> saved_;
};
#endif

} // namespace

bool ThemeManager::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type type = event->type();
    if (type == QEvent::Show) {
        // A widget the last re-style pass skipped because it was hidden.
        if (!g_restyle_pending.isEmpty() && watched->isWidgetType() && g_restyle_pending.remove(watched))
            repolish_tree(static_cast<QWidget*>(watched));
    } else if ((type == QEvent::Polish || type == QEvent::StyleChange) && watched->isWidgetType()) {
        auto* w = static_cast<QWidget*>(watched);
        if (g_handler_phase && type == QEvent::Polish)
            ++g_handler_phase_polished;
        const QString scoped = scoped_replacement(w, w->styleSheet());
        if (!scoped.isEmpty()) {
            if (g_handler_phase && type == QEvent::StyleChange) {
                // Qt re-styled `w` in the middle of the handlers. Every widget is
                // styled again once they are done (or when it is next shown), from
                // the text in place then — so the scoped text is only stored;
                // setStyleSheet() would re-style the whole subtree here and now.
                store_style_sheet(w, scoped);
            } else if (type != QEvent::Polish || !store_sheet_during_first_polish(w, scoped)) {
                w->setStyleSheet(scoped);
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

// ── One style recalculation for the whole application ───────────────────────
//
// QApplication::setStyleSheet() on an application that already has a
// stylesheet re-polishes every widget whose rules are cached and then, for
// each of them, every widget below it — so a widget nested twenty deep is
// styled twenty times. With five screens open (3,900 widgets) that was
// 12-16 s on the GUI thread for a theme or font change, and widgets whose
// own stylesheet text had been translated kept their old colours whenever
// that redundant second visit did not happen to reach them (a top-level
// menu, for one).
//
// The same result is reached in one pass: drop Qt's caches, store the new
// application sheet, then style each widget once (repolish_tree).

bool ThemeManager::set_application_stylesheet(const QString& qss, bool defer_widgets) {
    if (legacy_restyle() || qApp->styleSheet().isEmpty()) {
        // First stylesheet of the process (Qt installs its stylesheet style and
        // polishes the few widgets that exist), or the legacy switch.
        qApp->setStyleSheet(qss);
        return false;
    }
    QStyle* style = QApplication::style();
    // QStyleSheetStyle::unpolish(QApplication*) empties the rule caches and
    // forgets the parsed application sheet; with nothing cached, setStyleSheet()
    // only stores the text.
    style->unpolish(qApp);
    qApp->setStyleSheet(qss);
    style->polish(qApp);
    if (!defer_widgets)
        repolish_all_widgets();
    return true;
}

void ThemeManager::repolish_all_widgets() {
    QElapsedTimer timer;
    timer.start();
    QList<QPointer<QWidget>> roots;
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* w : widgets) {
        if (!w->parentWidget())
            roots.append(w);
    }
    // What an earlier pass left for the idle pass is found again below, under
    // its root (those widgets stay in g_restyle_pending for their Show meanwhile).
    g_restyle_idle_queue.clear();
    int count = 0;
    for (const QPointer<QWidget>& root : std::as_const(roots)) {
        if (!root)
            continue;
        if (root->isHidden())
            note_restyle_pending(root);
        else
            count += repolish_tree(root);
    }
    if (!g_restyle_idle_queue.isEmpty()) {
        if (!g_restyle_idle_timer) {
            g_restyle_idle_timer = new QTimer(this);
            g_restyle_idle_timer->setSingleShot(true);
            connect(g_restyle_idle_timer, &QTimer::timeout, this, []() {
                if (restyle_hidden_slice()) {
                    g_restyle_idle_timer->start(kIdleRestyleGapMs);
                    return;
                }
                LOG_DEBUG("ThemeManager",
                         QString("background pass styled %1 hidden widgets in %2 slices (%3 ms of styling, done %4 ms "
                                 "after the change)")
                             .arg(g_restyle_idle_styled)
                             .arg(g_restyle_idle_slices)
                             .arg(g_restyle_idle_work_ms)
                             .arg(g_restyle_idle_clock.elapsed()));
            });
        }
        g_restyle_idle_styled = 0;
        g_restyle_idle_slices = 0;
        g_restyle_idle_work_ms = 0;
        g_restyle_idle_clock.start();
        g_restyle_idle_timer->start(kIdleRestyleStartMs);
    }
    LOG_DEBUG("ThemeManager", QString("styled %1 of %2 widgets in one pass in %3 ms (the hidden ones follow when shown)")
                                 .arg(count)
                                 .arg(widgets.size())
                                 .arg(timer.elapsed()));
}

void ThemeManager::notify_theme_changed() {
    QElapsedTimer timer;
    timer.start();
#ifdef FINCEPT_HAS_WIDGETS_PRIVATE
    if (!legacy_restyle()) {
        {
            PolishSuspension quiet;
            g_handler_phase_polished = 0;
            g_handler_phase = true;
            notifying_ = true;
            emit theme_changed(current_);
            notifying_ = false;
            g_handler_phase = false;
        }
        LOG_DEBUG("ThemeManager", QString("theme_changed handlers ran in %1 ms (%2 widgets polished meanwhile)")
                                     .arg(timer.elapsed())
                                     .arg(g_handler_phase_polished));
        // The handlers' stylesheets were only stored (and the application sheet
        // too, when it changed): style everything once, now that all of it is in.
        repolish_all_widgets();
        // Now the layout work the handlers held back (see appearance_settled()).
        emit appearance_settled();
        return;
    }
#endif
    notifying_ = true;
    emit theme_changed(current_);
    notifying_ = false;
    LOG_DEBUG("ThemeManager", QString("theme_changed handlers ran in %1 ms").arg(timer.elapsed()));
    emit appearance_settled();
}

// ── The re-style pass ────────────────────────────────────────────────────────

bool ThemeManager::restyle_existing_widgets(const Snapshot& from) {
    if (from.preset == preset_ && from.font_family_css == font_family_css_ && from.font_size_px == font_size_px_)
        return false;
    const QWidgetList widgets = QApplication::allWidgets();
    if (widgets.isEmpty())
        return false;

    QElapsedTimer timer;
    timer.start();
    const Translator translator(from.preset, preset_, from.font_family_css, font_family_css_, from.font_size_px,
                                font_size_px_);

    int restyled = 0;
    for (QWidget* w : widgets) {
        const QString sheet = w->styleSheet();
        if (sheet.isEmpty())
            continue;
        // A sheet this manager built for the widget: rebuilt for the incoming
        // appearance, exactly as a fresh start would build it.
        if (w->property(kPropThemeSheet).toString() == QLatin1String("ads")) {
            const QString fresh = build_ads_qss();
            if (fresh != sheet) {
                store_style_sheet(w, fresh);
                ++restyled;
            }
            continue;
        }
        // A widget whose stylesheet holds user or document colours (a report
        // page, a user-picked swatch) opts out: those values are content, and
        // one that happens to equal a token must not be translated.
        if (w->property(kPropNoRestyle).toBool())
            continue;

        // Clamped font sizes carry their offset in the text (FontSizeText.h),
        // so the translation needs nothing remembered from earlier passes.
        const QString translated = translator.translate(sheet);
        if (translated != sheet) {
            store_style_sheet(w, translated);
            ++restyled;
        }
    }

    LOG_DEBUG("ThemeManager", QString("re-styled %1 of %2 widgets in %3 ms")
                                 .arg(restyled)
                                 .arg(widgets.size())
                                 .arg(timer.elapsed()));
    return restyled > 0;
}

} // namespace fincept::ui
