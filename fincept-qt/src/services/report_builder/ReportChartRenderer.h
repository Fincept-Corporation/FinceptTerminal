#pragma once
// ReportChartRenderer — finance-grade Report Builder charts, drawn with QPainter only.
//
// A chart is document content: no widgets, no QtCharts, no QPixmap and no terminal theme.
// The same paint code draws into a QImage on a worker thread (render_image / render_png, at
// print resolution) and straight into a QPdfWriter painter (vector PDF: the caller scales the
// painter from 96-dpi logical px to the device). Colours come only from the report's
// ChartPalette (paper ink).
//
// Every length is in LOGICAL px (96 dpi). Text is measured on a private 96-dpi device, so the
// layout is identical on screen, in PNGs and in the PDF. Fonts are pixel-sized (title 13 px
// bold, axis and legend 10 px, annotations 9 px; x0.85 when the chart is narrower than 360 px).
//
// Deterministic: identical input -> identical layout and pixels (no clock, no randomness, no
// unordered iteration). Thread-safe: no shared mutable state; fonts are the only global Qt
// state read. Time-axis labels follow QLocale() and the local time zone (both are part of
// cache_key).
//
// Kinds: line, area, stacked_area, bar (grouped), stacked_bar, hbar, candle (+ volume pane),
// donut, pie, scatter, histogram; combos through Series::kind (line | bar | area) and a
// right-hand axis through Series::right_axis.

#include "core/report/ReportChartSpec.h"

#include <QByteArray>
#include <QFont>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QVector>

class QPainter;

namespace fincept::services::report_chart {

/// Bumped whenever the drawing changes, so cached chart images are redrawn.
inline constexpr int kRendererVersion = 1;

/// An axis tick. `pos` is the pixel along the axis in the logical coordinates of the chart
/// rect (origin = the rect's top-left): a y coordinate for y / y2 ticks, an x coordinate for
/// x ticks. `value` is the data value (epoch ms on a date axis, the point index on a category
/// axis, the category index for hbar categories).
struct Tick {
    double value = 0;
    double pos = 0;
    QString label;
};

/// One legend line. `color` is "#rrggbb" when the layout was computed with a palette,
/// otherwise a colour role ("accent" | "series:N" | "up" | "down"); resolve_color() maps a
/// role to the palette. shape: "line" | "box" | "dot".
struct LegendEntry {
    QString name;
    QString color;
    QString shape;
};

/// Where everything goes, in the logical coordinates of the chart rect.
///
/// hbar: y_min / y_max is the VALUE range (drawn along the horizontal axis), x_ticks are the
/// value ticks and y_ticks the categories (value = category index, top to bottom).
/// histogram: y is the count axis; x_ticks are numeric values.
/// donut / pie: `plot` is the square around the circle; no axes.
struct Layout {
    QRectF outer, title_rect, plot, volume_plot /*empty unless candles with volume*/, legend_rect;
    double y_min = 0, y_max = 1;
    bool log_y = false;
    QVector<Tick> y_ticks;
    bool has_y2 = false;
    double y2_min = 0, y2_max = 1;
    QVector<Tick> y2_ticks;
    QVector<Tick> x_ticks;
    QVector<LegendEntry> legend;    ///< the entries drawn (empty when the legend is hidden)
    QString legend_position;        ///< "top" | "bottom" | "right" | "none"
    int series_count = 0;           ///< series drawn (candles: 1; donut / pie: the source series)
    int point_count = 0;            ///< spec.point_count()
    QVector<double> slice_fractions;     ///< donut / pie, positive slices in input order (sum 1)
    QVector<double> bin_edges;           ///< histogram: bins + 1 edges
    QVector<int> bin_counts;             ///< histogram: one count per bin (sum = finite values)
    bool empty = false;                  ///< nothing to draw: paint() writes a small "No data" note
};

/// The layout of `spec` drawn at `size` (logical px). LegendEntry::color holds colour roles.
Layout compute_layout(const report::chart::ChartSpec& spec, const QSizeF& size, const QString& font_family);

/// Same, with LegendEntry::color resolved to the palette's "#rrggbb".
Layout compute_layout(const report::chart::ChartSpec& spec, const QSizeF& size, const QString& font_family,
                      const report::chart::ChartPalette& pal);

/// Fills `rect` with the paper colour and draws the chart inside it, in rect's logical
/// coordinates (any top-left; the painter's state is saved and restored).
void paint(QPainter* p, const QRectF& rect, const report::chart::ChartSpec& spec,
           const report::chart::ChartPalette& pal, const QString& font_family);

/// The chart as an image of logical * scale pixels (ARGB32 premultiplied; dots-per-metre
/// say 96 * scale dpi). Null for an empty size.
QImage render_image(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                    const QString& font_family, const QSize& logical, qreal scale);

/// render_image() encoded as PNG (empty for an empty size).
QByteArray render_png(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                      const QString& font_family, const QSize& logical, qreal scale);

/// Hex SHA-1 identity of the pixels: content_hash(spec), palette_key(pal), family, size, scale,
/// kRendererVersion, QLocale().name() (and the system time zone when the chart has dates).
QString cache_key(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                  const QString& font_family, const QSize& logical, qreal scale);

// ── Helpers (also used by the tests) ───────────────────────────────────────────

/// The font the renderer uses for `base_px` text (13 title, 10 axis / legend, 9 annotations)
/// in a chart `chart_width` logical px wide: families {font_family, "Arial"}, pixel-sized.
QFont chart_font(const QString& font_family, int base_px, double chart_width, bool bold = false);

/// "#rrggbb" of a colour role ("accent" | "series:N" | "up" | "down" | "ink" | "muted" ...);
/// a "#..." value is returned unchanged.
QString resolve_color(const report::chart::ChartPalette& pal, const QString& role);

/// An axis-style number: prefix, locale digits, compact K/M/B/T when |v| >= 10,000 and the
/// format allows it, suffix. `decimals` < 0 picks a value-label precision.
QString format_value(double v, const report::chart::AxisFormat& fmt, int decimals = -1);

/// Logical size of a chart `width` px wide (height from spec.effective_aspect()).
QSize chart_size(const report::chart::ChartSpec& spec, int width);

} // namespace fincept::services::report_chart
