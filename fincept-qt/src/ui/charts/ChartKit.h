#pragma once
// ChartKit — shared building blocks of the custom-painted chart widgets
// (TimeSeriesChart, CandleChart, Sparkline, SparklineDelegate, StatTile).
//
//   * colour roles and chrome colours resolved from the ACTIVE theme,
//   * the chart fonts (the user's font at sizes relative to their base size),
//   * locale-aware number formatting defaults,
//   * min/max decimation to pixel columns (keeps spikes visible at 10k+ points),
//   * painting primitives: tooltip box, axis tag, state message, marker glyphs.
//
// Every colour / font accessor reads the theme at the moment it is called.
// Call them while painting; never keep the result across a
// ThemeManager::theme_changed.

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;
class QWidget;

namespace fincept::ui {

/// Colour of a series, marker, line or band. Auto/Palette take a slot of the
/// theme's categorical chart palette (colors::chart_color, 12 colours, wraps);
/// the rest map to semantic theme tokens. Positive/Negative are price direction
/// and follow the user's direction palette (green-up, red-up, colour-blind).
/// Custom takes a caller colour (made legible on the active theme).
enum class ChartColorRole { Auto, Palette, Accent, Positive, Negative, Warning, Info, Cyan, Muted, Text, Custom };

/// What a chart shows in its plot area instead of (or over) its data.
enum class ChartState { Ready, Loading, Empty, Error };

/// Event / trade marker kinds.
enum class ChartMarkerKind { Buy, Sell, Earnings, Dividend, Split, News, Custom };

namespace chartkit {

/// Translation context for the kit's free functions ("ChartKit" in .ts files).
class ChartKitText {
    Q_DECLARE_TR_FUNCTIONS(ChartKit)
};

// ── Colours (live theme) ─────────────────────────────────────────────────────
QColor role_color(ChartColorRole role, int palette_index = 0, const QColor& custom = {});
QColor background(); ///< chart canvas (bg_base)
QColor surface();    ///< tiles / panels (bg_surface)
QColor raised();     ///< tooltip and tag fill (bg_raised)
QColor grid();       ///< grid lines (border_dim)
QColor axis_line();  ///< axis / pane separators (border_med)
QColor axis_text();  ///< tick labels (text_secondary)
QColor text();       ///< primary text
QColor text_dim();   ///< de-emphasised text (text_tertiary)
QColor crosshair();  ///< crosshair lines
QColor up();         ///< gain / bullish
QColor down();       ///< loss / bearish
QColor accent();
QColor warning();
QColor with_alpha(QColor c, double alpha);
/// Whichever of the theme's base background / primary text reads best on `bg`.
QColor text_on(const QColor& bg);

// ── Fonts (the user's font, sizes relative to their base size) ───────────────
QFont axis_font();                   ///< tick labels, legend, presets
QFont tooltip_font(bool bold = false); ///< tooltip rows and tags
QFont title_font();                  ///< chart / pane titles

// ── Number formatting (QLocale-aware defaults) ───────────────────────────────
/// Fixed decimals with locale group separators; NaN/inf → an em dash.
QString format_number(double v, int decimals);
/// Decimals that suit a value of this magnitude in a tooltip (2 for prices,
/// more for sub-unit values, 0 for very large ones).
int auto_decimals(double v);
/// format_number(v, auto_decimals(v)), switching to K/M/B/T above 10 million.
QString format_auto(double v);
/// 1.25K, 3.4M, 2.1B, 1.0T.
QString format_compact(double v, int decimals);
/// "+1.23%" / "-0.40%".
QString format_signed_percent(double pct, int decimals = 2);
/// An axis tick label: compact once `max_abs` >= 1e6, otherwise fixed with
/// enough decimals to tell ticks `step` apart.
QString format_tick(double v, double step, double max_abs);

// ── Decimation ───────────────────────────────────────────────────────────────
/// Min/max decimation to pixel columns. Scans indices [i0, i1) of the sorted
/// `xs`/`ys` arrays, buckets them by pixel column
/// (floor((x - x0) * px_per_unit)) and appends to `out` — in index order —
/// the first, min, max and last index of every column. Non-finite y values
/// are gaps: one gap index is emitted per run so the caller can break the line.
/// Every extreme survives, so spikes stay visible at any zoom level.
void decimate_minmax(const double* xs, const double* ys, int i0, int i1, double x0, double px_per_unit,
                     QVector<int>& out);

// ── Fast stroking / filling of dense lines ───────────────────────────────────
/// Stroke `poly` with the painter's current pen in short overlapping chunks.
/// Qt rasterises a wide antialiased polyline as one outline whose cost grows
/// steeply when the line is dense and folds back on itself (min/max-decimated
/// data); chunks keep every outline small. Joins inside a chunk are kept; with
/// a round cap the chunk seams are invisible.
void stroke_polyline(QPainter& p, const QPolygonF& poly);

/// `c` composited over the chart background into an opaque colour.
QColor opaque_over_background(const QColor& c);

/// Fill many axis-aligned rects (bars, candle bodies, wicks, column spans),
/// non-antialiased, in `color` made opaque over the chart background.
/// Why opaque: Qt 6 blends every translucent fill taller than ~96 rows on its
/// GUI thread pool and waits for it, so thousands of translucent bars cost
/// seconds in synchronisation; an opaque fill is a plain memory fill. (A single
/// path holding all the rects is slower still.) Use this for any bar-like
/// mark; keep translucency for a few large shapes only.
void fill_rects(QPainter& p, const QVector<QRectF>& rects, const QColor& color);

/// True when `line` (pixel coordinates) has more than ~1.5 points per pixel
/// column — the case where antialiased stroking gets expensive and adds
/// nothing visible.
bool is_dense(const QPolygonF& line);

/// Draw a series line with `pen`. Sparse lines are stroked (antialiased,
/// chunked); dense lines are drawn as one crisp rect per pixel column spanning
/// that column's extremes, joined to the neighbouring column — the same
/// picture at a tiny fraction of the cost.
void draw_series_line(QPainter& p, const QPolygonF& line, const QPen& pen);

/// draw_series_line with the part above `base_y` in `above` and the part below
/// in `below` (a Baseline series). `bounds` is the plot rectangle.
void draw_series_line_split(QPainter& p, const QPolygonF& line, const QPen& pen, double base_y, const QColor& above,
                            const QColor& below, const QRectF& bounds);

/// How fill_envelope picks one point per pixel column.
enum class EnvelopeMode {
    Farthest, ///< the point farthest from the base (area fills)
    Above,    ///< the highest point, clamped to the base (part above the base)
    Below,    ///< the lowest point, clamped to the base (part below the base)
};
/// Polygon for filling the region between `line` (pixel coordinates, x
/// ascending) and the horizontal `base_y`: one vertex per pixel column plus
/// the two base corners. Exact baseline-crossing points are inserted first,
/// so sparse lines fill exactly; dense ones fill their per-column extreme.
/// Simple (non-self-intersecting) by construction, so it fills fast.
QPolygonF fill_envelope(const QPolygonF& line, double base_y, EnvelopeMode mode);

// ── Layers ───────────────────────────────────────────────────────────────────
/// True when `layer` matches `w`'s size and device pixel ratio.
bool layer_valid(const QPixmap& layer, const QWidget* w);
/// A pixmap the size of `w` at its device pixel ratio, filled with `fill`
/// (pass Qt::transparent for a see-through layer).
QPixmap make_layer(const QWidget* w, const QColor& fill);

// ── Painting primitives ──────────────────────────────────────────────────────
struct TooltipRow {
    QColor swatch;      ///< invalid → no swatch
    QString name;
    QString value;
    QColor value_color; ///< invalid → primary text
};

/// Size the box drawn by draw_tooltip would take.
QSize tooltip_size(const QString& title, const QVector<TooltipRow>& rows, const QString& footer = {});
/// Paint a tooltip box beside `anchor`, kept inside `bounds` (flips to the
/// other side of the anchor when it would overflow). Returns the box drawn.
QRect draw_tooltip(QPainter& p, const QRect& bounds, const QPoint& anchor, const QString& title,
                   const QVector<TooltipRow>& rows, const QString& footer = {});

/// A filled label on an axis (crosshair value, last price). `r` is the box.
void draw_axis_tag(QPainter& p, const QRectF& r, const QString& text, const QColor& bg);

/// Centered message for Loading / Empty / Error (default texts when
/// `message` is empty). Ready paints nothing.
void draw_state_message(QPainter& p, const QRect& r, ChartState state, const QString& message);

/// Default colour of a marker kind (Buy = up, Sell = down, …).
QColor marker_color(ChartMarkerKind kind);
/// One-letter badge text of a marker kind ("E", "D", …; empty for Buy/Sell).
QString marker_letter(ChartMarkerKind kind);
/// Paint a marker glyph. Buy is an up-triangle whose tip touches `at` from
/// below; Sell a down-triangle touching `at` from above; the others a round
/// badge centered on `at`. Returns the glyph's hit rectangle.
QRectF draw_marker(QPainter& p, const QPointF& at, ChartMarkerKind kind, const QColor& color, qreal size);

} // namespace chartkit
} // namespace fincept::ui
