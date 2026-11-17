#pragma once
// Private state of ui::TimeSeriesChart, shared by TimeSeriesChart.cpp (data,
// view, interaction) and TsChartPaint.cpp (layout, painting). Not for use
// outside those two files.

#include "ui/charts/ChartAxisTicks.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/charts/TsChartPanes.h"

#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace fincept::ui::ts_detail {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Series {
    int id = 0;
    TimeSeriesChart::SeriesSpec spec; ///< points / xs / ys are emptied after ingest
    QVector<double> xs;               ///< sorted ascending, all finite
    QVector<double> ys;
    int palette_slot = 0;             ///< resolved categorical colour index
};

struct Pane {
    double ratio = 1.0;
    QString title;
    TimeSeriesChart::ValueFormatter fmt[2];
    std::optional<std::pair<double, double>> fixed[2];
    bool visible = true;  ///< set_pane_visible()
    bool removed = false; ///< remove_pane(): tombstone, the index is never reused
};

/// The pane rules' view of the panes (see TsChartPanes.h).
inline QVector<ts_panes::PaneState> pane_states(const QVector<Pane>& panes) {
    QVector<ts_panes::PaneState> out;
    out.reserve(panes.size());
    for (const auto& p : panes)
        out.append({p.ratio, p.visible, p.removed});
    return out;
}

/// True when pane `pane` exists, is not removed and is not hidden.
inline bool pane_is_shown(const QVector<Pane>& panes, int pane) {
    return pane >= 0 && pane < panes.size() && panes[pane].visible && !panes[pane].removed;
}

enum class AnnType { Marker, HLine, Band };

struct Annotation {
    int id = 0;
    AnnType type = AnnType::Marker;
    double x = 0.0;   ///< marker x / band start
    double x2 = 0.0;  ///< band end
    double y = kNaN;  ///< hline y / marker price
    int series = -1;  ///< marker attached series
    int pane = 0;
    TimeSeriesChart::Axis axis = TimeSeriesChart::Axis::Left;
    ChartMarkerKind kind = ChartMarkerKind::Custom;
    ChartColorRole role = ChartColorRole::Auto;
    QString label;
    QString tooltip;
};

struct AxisScale {
    bool used = false;
    bool log = false;
    double lo = 0.0;
    double hi = 1.0;
    double max_abs = 1.0;
    chart_ticks::NumericTicks ticks;
};

struct PaneLayout {
    bool shown = false; ///< false for hidden / removed panes (empty rect, unused axes)
    QRect rect;
    AxisScale axis[2];
};

enum class HitKind { Legend, Preset, ModeLog, ModeRebase, ModePercent };

struct HitItem {
    HitKind kind = HitKind::Legend;
    QRect rect;
    int id = -1;  ///< series id (Legend) / preset index (Preset)
};

struct MarkerHit {
    QRect rect;
    int id = -1;
};

struct Layout {
    bool valid = false;
    int header_h = 0;
    int header_row_h = 0;
    int x_axis_h = 0;
    QRect title_rect;
    int left_w = 0;
    int right_w = 0;
    QRect plot;                 ///< union of the shown pane rects
    QVector<PaneLayout> panes;  ///< indexed by pane index (hidden / removed: shown == false)
    int first_pane = 0;         ///< top-most shown pane (band labels go there)
    double x0 = 0.0;
    double x1 = 1.0;
    chart_ticks::TimeTicks time_ticks;
    chart_ticks::NumericTicks num_ticks;
    QVector<HitItem> hits;      ///< header items
    QVector<MarkerHit> marker_hits;
    QHash<int, double> rebase;  ///< series id → base value (relative modes)
    double typical_spacing = 0.0;
};

/// Pixel x of data x.
inline double px_of(const Layout& L, double x) {
    const double span = L.x1 - L.x0;
    return L.plot.left() + (span > 0 ? (x - L.x0) / span : 0.0) * L.plot.width();
}

/// Data x of pixel x.
inline double x_of(const Layout& L, double px) {
    const double w = std::max(1, L.plot.width());
    return L.x0 + (px - L.plot.left()) / w * (L.x1 - L.x0);
}

/// Pixel y of value v on a pane axis (NaN when unmappable, e.g. v <= 0 on log).
inline double py_of(const QRect& r, const AxisScale& sc, double v) {
    if (!std::isfinite(v))
        return kNaN;
    double t;
    if (sc.log) {
        if (v <= 0.0)
            return kNaN;
        const double l0 = std::log10(sc.lo);
        const double l1 = std::log10(sc.hi);
        t = (l1 > l0) ? (std::log10(v) - l0) / (l1 - l0) : 0.5;
    } else {
        t = (sc.hi > sc.lo) ? (v - sc.lo) / (sc.hi - sc.lo) : 0.5;
    }
    return r.top() + (1.0 - t) * r.height();
}

/// Value at pixel y on a pane axis.
inline double v_of(const QRect& r, const AxisScale& sc, double py) {
    const double t = r.height() > 0 ? 1.0 - (py - r.top()) / r.height() : 0.5;
    if (sc.log) {
        const double l0 = std::log10(sc.lo);
        const double l1 = std::log10(sc.hi);
        return std::pow(10.0, l0 + t * (l1 - l0));
    }
    return sc.lo + t * (sc.hi - sc.lo);
}

/// Index of the last point with x <= `x` (-1 when none).
inline int asof_index(const QVector<double>& xs, double x) {
    const auto it = std::upper_bound(xs.begin(), xs.end(), x);
    return static_cast<int>(it - xs.begin()) - 1;
}

} // namespace fincept::ui::ts_detail

namespace fincept::ui {

struct TimeSeriesChart::Impl {
    QVector<ts_detail::Series> series;
    QVector<ts_detail::Pane> panes;
    QVector<ts_detail::Annotation> annotations;
    int next_series_id = 1;
    int next_annotation_id = 1;
    int next_palette_slot = 0;

    // View
    ViewChange view_mode = ViewChange::Auto;
    double view_from = 0.0;
    double view_to = 0.0;
    QString active_preset;
    QStringList presets;
    ValueMode value_mode = ValueMode::Absolute;
    bool log_scale = false;

    // Appearance / behaviour
    QString title;
    bool show_legend = true;
    bool show_mode_buttons = false;
    bool crosshair_enabled = true;
    bool zoom_enabled = true;
    bool x_is_time = true;
    bool utc = false;
    bool transparent = false;
    TimeFormatter time_fmt;
    TimeFormatter tick_fmt;
    State state = State::Ready;
    QString state_message;
    QString export_name;
    std::function<void(QMenu*)> menu_extra;

    // Render cache
    QPixmap cache;
    bool dirty = true;
    ts_detail::Layout layout;

    // Interaction
    QPoint mouse;
    bool mouse_in = false;
    bool dragging = false;
    bool drag_moved = false;
    QPoint press_pos;
    double press_from = 0.0;
    double press_to = 0.0;
    int hover_hit = -1;     ///< index into layout.hits
    int hover_marker = -1;  ///< annotation id
    double cross_x = ts_detail::kNaN;
    int cross_pane = -1;
    double linked_x = ts_detail::kNaN;

    ts_detail::Series* find(int id) {
        for (auto& s : series)
            if (s.id == id)
                return &s;
        return nullptr;
    }
    const ts_detail::Series* find(int id) const {
        for (const auto& s : series)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    /// True when pane `pane` is drawn (exists, not hidden, not removed).
    bool pane_shown(int pane) const { return ts_detail::pane_is_shown(panes, pane); }
    /// True when series `s` is drawn: visible itself and on a shown pane.
    bool series_shown(const ts_detail::Series& s) const { return s.spec.visible && pane_shown(s.spec.pane); }

    /// True when `s` is shown relative to its base in the current value mode.
    bool is_rebased(const ts_detail::Series& s) const {
        return value_mode != ValueMode::Absolute && s.spec.pane == 0 && s.spec.rebase && s.spec.style != Style::Bars;
    }
    /// True when any drawn series on pane 0 is shown rebased.
    bool pane0_rebased() const {
        if (value_mode == ValueMode::Absolute)
            return false;
        for (const auto& s : series)
            if (series_shown(s) && is_rebased(s))
                return true;
        return false;
    }
    /// Axis a series is drawn against (rebased series share the left axis).
    int axis_of(const ts_detail::Series& s) const {
        return is_rebased(s) ? 0 : static_cast<int>(s.spec.axis);
    }
    /// The displayed value of raw `y` for series `s`.
    double transform(const ts_detail::Series& s, double y) const {
        if (!is_rebased(s))
            return y;
        const double base = layout.rebase.value(s.id, ts_detail::kNaN);
        if (!std::isfinite(base) || base == 0.0 || !std::isfinite(y))
            return ts_detail::kNaN;
        const double rel = (y - base) / std::abs(base) * 100.0;
        return value_mode == ValueMode::Percent ? rel : 100.0 + rel;
    }
    QColor color_of(const ts_detail::Series& s) const {
        return chartkit::role_color(s.spec.color, s.palette_slot, s.spec.custom_color);
    }
};

} // namespace fincept::ui
