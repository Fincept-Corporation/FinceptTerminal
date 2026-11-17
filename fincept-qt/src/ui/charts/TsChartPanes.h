#pragma once
// Pure pane rules of ui::TimeSeriesChart (QtCore only, header-inline): which
// panes are shown, the vertical slot of each shown pane, and whether a pane
// may be hidden or removed. Used by TimeSeriesChart.cpp / TsChartPaint.cpp
// and unit-tested by tests/tst_ts_chart_panes.cpp.
//
// Pane indices are stable handles: a hidden pane keeps its slot index, and a
// removed pane keeps its index as a tombstone (never reused), so the indices
// returned by add_pane() for the other panes never change.

#include <QVector>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fincept::ui::ts_panes {

struct PaneState {
    double ratio = 1.0;   ///< height relative to the main pane (1.0)
    bool visible = true;  ///< set_pane_visible()
    bool removed = false; ///< remove_pane() — a tombstone
};

/// Vertical slot of one pane inside the pane column (offsets from its top).
struct Slot {
    bool shown = false;
    int top = 0;    ///< 0 for panes that are not shown
    int height = 0; ///< 0 for panes that are not shown
};

inline bool is_alive(const QVector<PaneState>& panes, int pane) {
    return pane >= 0 && pane < panes.size() && !panes[pane].removed;
}

inline bool is_shown(const QVector<PaneState>& panes, int pane) {
    return is_alive(panes, pane) && panes[pane].visible;
}

inline int shown_count(const QVector<PaneState>& panes) {
    int n = 0;
    for (int i = 0; i < panes.size(); ++i)
        n += is_shown(panes, i) ? 1 : 0;
    return n;
}

/// First shown pane (-1 when none).
inline int first_shown(const QVector<PaneState>& panes) {
    for (int i = 0; i < panes.size(); ++i)
        if (is_shown(panes, i))
            return i;
    return -1;
}

/// First pane that has not been removed (-1 when none).
inline int first_alive(const QVector<PaneState>& panes) {
    for (int i = 0; i < panes.size(); ++i)
        if (is_alive(panes, i))
            return i;
    return -1;
}

/// The pane a new series / marker / line goes to when it names `pane`: the
/// pane itself when it exists (shown or hidden), else the first live pane
/// (pane 0 unless it was removed) — the chart's long-standing "out of range
/// means the main pane" rule.
inline int resolve_pane(const QVector<PaneState>& panes, int pane) {
    if (is_alive(panes, pane))
        return pane;
    const int f = first_alive(panes);
    return f >= 0 ? f : 0;
}

/// May `pane` be hidden? A live pane that is already hidden: yes (no-op).
/// A shown pane: only while another pane stays shown (the plot never goes empty).
inline bool can_hide(const QVector<PaneState>& panes, int pane) {
    if (!is_alive(panes, pane))
        return false;
    if (!panes[pane].visible)
        return true;
    return shown_count(panes) >= 2;
}

/// May `pane` be removed? Only a live pane, and only while another pane stays
/// shown afterwards (removing the last pane — or the last shown one while the
/// others are hidden — is refused; show another pane first).
inline bool can_remove(const QVector<PaneState>& panes, int pane) {
    if (!is_alive(panes, pane))
        return false;
    const int others_shown = shown_count(panes) - (panes[pane].visible ? 1 : 0);
    return others_shown >= 1;
}

/// Slots for every pane in a column `column_h` px tall with `gap` px between
/// neighbouring SHOWN panes. Heights are proportional to the ratios of the
/// shown panes; the last shown pane takes the rounding remainder, so the
/// shown heights plus the gaps fill the column exactly (at least one pixel per
/// shown pane in a tiny column). Hidden and removed panes get an empty slot.
/// With every pane shown this is exactly the chart's original layout rule.
inline QVector<Slot> layout_slots(const QVector<PaneState>& panes, int column_h, int gap) {
    QVector<Slot> out(panes.size());
    const int n = shown_count(panes);
    if (n == 0)
        return out;
    const int avail = std::max(n, column_h - (n - 1) * gap);
    double total_ratio = 0.0;
    int last = -1;
    for (int i = 0; i < panes.size(); ++i) {
        if (!is_shown(panes, i))
            continue;
        total_ratio += panes[i].ratio;
        last = i;
    }
    int used_h = 0;
    int y = 0;
    for (int i = 0; i < panes.size(); ++i) {
        if (!is_shown(panes, i))
            continue;
        const int h = (i == last) ? avail - used_h
                                  : static_cast<int>(avail * panes[i].ratio / std::max(1e-9, total_ratio));
        out[i].shown = true;
        out[i].top = y;
        out[i].height = h;
        used_h += h;
        y += h + gap;
    }
    return out;
}

inline constexpr double kMinLineWidth = 0.5;
inline constexpr double kMaxLineWidth = 12.0;

/// A requested series line width in device-independent pixels, clamped to
/// [0.5, 12]; nullopt for a non-finite or non-positive request (ignored).
inline std::optional<double> line_width_px(double px) {
    if (!std::isfinite(px) || !(px > 0.0))
        return std::nullopt;
    return std::clamp(px, kMinLineWidth, kMaxLineWidth);
}

} // namespace fincept::ui::ts_panes
