#pragma once
// EquityLayoutPlan — the Equity Trading screen's responsive rules, in ONE place.
//
// The body is one row of three zones: LEFT watchlist, CENTRE chart over the
// blotter, RIGHT quote / depth over the order ticket. How they flow depends on
// the width class of the screen, judged on the width in units of the user's base
// font (a 24 px font at 1920 px lays out like 14 px at 1120 px):
//
//   class     effective width (14 px base)   zones
//   Wide      >= 1600                         watchlist (full) | chart+blotter | depth (5 levels) + ticket
//   Medium    1280 .. 1599                    narrower watchlist | chart+blotter | depth collapsed (2 levels) + ticket
//   Narrow    1000 .. 1279                    watchlist rail | chart+blotter | ticket as a drawer (opened on demand)
//   Compact   <  1000                         one zone at a time behind zone tabs (CHART · WATCHLIST · TICKET)
//
// Nothing here touches a widget: EquityWorkspace applies the plan, the unit test
// (tst_equity_layout_plan) pins the rules. Header-only, QtCore only.

#include <QList>
#include <QString>

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

enum class EqWidthClass { Compact = 0, Narrow = 1, Medium = 2, Wide = 3 };

/// Breakpoints, in px at the reference 14 px base font.
inline constexpr int kEqBreakNarrow = 1000;
inline constexpr int kEqBreakMedium = 1280;
inline constexpr int kEqBreakWide = 1600;
/// Reference base font size the breakpoints are expressed in.
inline constexpr int kEqReferenceFontPx = 14;

/// Width of the screen as it would be at the reference font: a larger base font
/// makes the same window behave like a narrower one; a smaller one never widens it.
inline int eq_effective_width(int width_px, int base_font_px) {
    if (width_px <= 0)
        return 0;
    if (base_font_px <= kEqReferenceFontPx)
        return width_px;
    return static_cast<int>(std::lround(double(width_px) * kEqReferenceFontPx / base_font_px));
}

inline EqWidthClass eq_width_class(int width_px, int base_font_px) {
    const int w = eq_effective_width(width_px, base_font_px);
    if (w >= kEqBreakWide)
        return EqWidthClass::Wide;
    if (w >= kEqBreakMedium)
        return EqWidthClass::Medium;
    if (w >= kEqBreakNarrow)
        return EqWidthClass::Narrow;
    return EqWidthClass::Compact;
}

/// Stable key of a class (persisted splitter sizes are stored per key).
inline QString eq_width_class_key(EqWidthClass c) {
    switch (c) {
        case EqWidthClass::Wide:
            return QStringLiteral("wide");
        case EqWidthClass::Medium:
            return QStringLiteral("medium");
        case EqWidthClass::Narrow:
            return QStringLiteral("narrow");
        case EqWidthClass::Compact:
            break;
    }
    return QStringLiteral("compact");
}

struct EqLayoutPlan {
    EqWidthClass cls = EqWidthClass::Wide;
    bool single_column = false;  ///< Compact: zone tabs, one zone shown at a time
    bool watchlist_rail = false; ///< left zone starts collapsed to the rail
    bool right_drawer = false;   ///< right zone hidden until opened (BUY / SELL / TICKET)
    int depth_levels = 5;        ///< depth ladder levels per side
    int watchlist_chars = 30;    ///< default watchlist width in average characters (0 = rail)
    int right_chars = 36;        ///< default width of the quote + ticket column in average characters
};

inline EqLayoutPlan eq_layout_plan(EqWidthClass c) {
    EqLayoutPlan p;
    p.cls = c;
    switch (c) {
        case EqWidthClass::Wide:
            break;
        case EqWidthClass::Medium:
            p.depth_levels = 2;
            p.watchlist_chars = 24;
            p.right_chars = 34;
            break;
        case EqWidthClass::Narrow:
            p.depth_levels = 2;
            p.watchlist_rail = true;
            p.watchlist_chars = 0;
            p.right_drawer = true;
            p.right_chars = 34;
            break;
        case EqWidthClass::Compact:
            p.single_column = true;
            p.depth_levels = 5;
            p.watchlist_chars = 0;
            p.right_chars = 0;
            break;
    }
    return p;
}

/// Sizes for a splitter whose `flex` pane takes what the others leave.
/// `wanted` holds every pane's wish (the flex entry is ignored), `minimums` each
/// pane's floor; panes with wanted <= 0 are hidden and get 0. When the window is
/// too small the fixed panes give way proportionally down to their minimums; the
/// flex pane never drops below its own minimum unless nothing else can give.
/// The result always sums to `total` (when total > 0).
inline QList<int> eq_fit_sizes(const QList<int>& wanted, const QList<int>& minimums, int flex, int total) {
    const int n = int(wanted.size());
    QList<int> out(n, 0);
    if (n == 0 || flex < 0 || flex >= n || total <= 0)
        return out;
    auto min_of = [&](int i) { return std::max(0, i < minimums.size() ? minimums[i] : 0); };
    int fixed_sum = 0;
    int fixed_min = 0;
    for (int i = 0; i < n; ++i) {
        if (i == flex || wanted[i] <= 0)
            continue;
        out[i] = std::max(wanted[i], min_of(i));
        fixed_sum += out[i];
        fixed_min += min_of(i);
    }
    const int flex_min = min_of(flex);
    int room = total - fixed_sum;
    if (room < flex_min && fixed_sum > fixed_min) {
        // Shrink the fixed panes toward their minimums, proportionally to their slack.
        const int need = std::min(flex_min - room, fixed_sum - fixed_min);
        const int slack = fixed_sum - fixed_min;
        int given = 0;
        for (int i = 0; i < n; ++i) {
            if (i == flex || wanted[i] <= 0)
                continue;
            const int s = out[i] - min_of(i);
            const int take = slack > 0 ? int(std::floor(double(need) * s / slack)) : 0;
            out[i] -= take;
            given += take;
        }
        // Rounding remainder from the last pane that still has slack.
        for (int i = n - 1; i >= 0 && given < need; --i) {
            if (i == flex || wanted[i] <= 0)
                continue;
            const int more = std::min(need - given, out[i] - min_of(i));
            out[i] -= more;
            given += more;
        }
        fixed_sum -= given;
        room = total - fixed_sum;
    }
    out[flex] = std::max(0, room);
    // Still over (fixed minimums alone exceed the window): trim from the right-most fixed pane.
    int sum = 0;
    for (int v : out)
        sum += v;
    for (int i = n - 1; i >= 0 && sum > total; --i) {
        if (i == flex)
            continue;
        const int cut = std::min(out[i], sum - total);
        out[i] -= cut;
        sum -= cut;
    }
    return out;
}

/// Saved sizes (from the user's last drag in this width class) scaled to `total`;
/// empty when they do not fit the pane count, are all zero, or would leave a shown
/// pane under its minimum — the caller then uses the defaults (eq_fit_sizes).
/// `shown[i]` false = pane hidden now (its share goes to the others).
inline QList<int> eq_restore_sizes(const QList<int>& saved, const QList<int>& minimums, const QList<bool>& shown,
                                   int total) {
    const int n = int(shown.size());
    if (saved.size() != n || total <= 0)
        return {};
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        if (saved[i] < 0)
            return {};
        if (shown[i])
            sum += saved[i];
    }
    if (sum <= 0)
        return {};
    QList<int> out(n, 0);
    int acc = 0;
    int last = -1;
    for (int i = 0; i < n; ++i) {
        if (!shown[i])
            continue;
        out[i] = int(std::floor(double(saved[i]) * total / double(sum)));
        acc += out[i];
        last = i;
    }
    if (last >= 0)
        out[last] += total - acc;
    for (int i = 0; i < n; ++i) {
        if (shown[i] && out[i] < (i < minimums.size() ? minimums[i] : 0))
            return {};
    }
    return out;
}

} // namespace fincept::screens::equity
