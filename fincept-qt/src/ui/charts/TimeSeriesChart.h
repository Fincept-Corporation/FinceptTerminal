#pragma once
// TimeSeriesChart — the terminal's shared multi-series time chart (QPainter).
//
//   auto* chart = new ui::TimeSeriesChart(this);
//   const int eq = chart->add_series({.name = tr("Equity"), .points = equity});
//   const int dd_pane = chart->add_pane(0.35);
//   chart->add_series({.name = tr("Drawdown"), .points = dd, .style = Style::Area,
//                      .pane = dd_pane, .color = ColorRole::Negative, .baseline = 0.0});
//   chart->set_value_formatter(Axis::Left, pct_fmt, dd_pane);
//   chart->set_range_presets({"1M", "3M", "6M", "YTD", "1Y", "5Y", "ALL"});
//   chart->add_band(oos_start, oos_end, ColorRole::Info, tr("Out of sample"));
//   chart->add_marker(t, eq, MarkerKind::Buy, tr("B"), tr("Bought 100 @ 182.40"));
//
// X UNIT: x values are epoch MILLISECONDS held in a double — the unit of
// QDateTime::toMSecsSinceEpoch() and of QtCharts' QDateTimeAxis. Times are
// displayed in local time (set_utc(true) for UTC). For a plain numeric x axis
// call set_x_is_time(false); presets other than ALL/MAX then do nothing.
//
// Y values are plain doubles; NaN / inf are gaps in the line.
//
// FEATURES
//   * styles Line / Area / Step / Bars / Baseline, left or right axis, stacked
//     sub-panes with a shared x axis and crosshair (pane 0 is the main pane,
//     height ratio 1.0; add_pane ratios are relative to it) that can be
//     hidden / shown / removed at run time (indices stay stable),
//   * crosshair + tooltip listing every visible series' value at the snapped
//     x (as-of: the last point at or before it), wheel zoom about the cursor,
//     drag pan, double-click reset, range presets, clickable legend,
//   * value modes on pane 0 — Absolute, Rebased100, Percent (both relative to
//     the first visible point, so they re-base as you zoom) — and log scale on
//     pane 0's left axis,
//   * markers (buy/sell/earnings/dividend/split/news/custom), horizontal lines,
//     shaded x bands (recessions, out-of-sample regions),
//   * Loading / Empty / Error states drawn inside the plot,
//   * right-click: copy image, save PNG, save CSV, log / rebase / percent, reset.
//
// RENDERING: static content is cached in a pixmap at the device pixel ratio;
// mouse movement repaints only the crosshair / tooltip on top of it. Series are
// decimated to pixel columns with min/max preservation, so 10k+ points per
// series pan and zoom smoothly with every spike intact. Colours and fonts are
// resolved from the active theme on every rebuild (ThemeManager::theme_changed
// triggers one).

#include "ui/charts/ChartKit.h"

#include <QPointF>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <limits>
#include <memory>
#include <optional>

class QMenu;

namespace fincept::ui {

class TimeSeriesChart : public QWidget {
    Q_OBJECT
  public:
    enum class Style {
        Line,
        Area,     ///< line + gradient fill down to the pane bottom (or to `baseline` when set)
        Step,     ///< horizontal-then-vertical steps (rates, positions)
        Bars,     ///< columns from `baseline` (default 0); `color_by_sign` for returns
        Baseline, ///< area above `baseline` (default 0) in the up colour, below in the down colour
    };
    Q_ENUM(Style)
    enum class Axis { Left = 0, Right = 1 };
    Q_ENUM(Axis)
    /// How pane-0 series with `rebase = true` are shown. Both relative modes
    /// use the first visible point of each series as the base and put every
    /// rebased series on the left axis.
    enum class ValueMode { Absolute, Rebased100, Percent };
    Q_ENUM(ValueMode)

    using ColorRole = ChartColorRole;
    using MarkerKind = ChartMarkerKind;
    using State = ChartState;
    using ValueFormatter = std::function<QString(double)>;
    /// Receives an x value (epoch ms in time mode).
    using TimeFormatter = std::function<QString(double)>;

    struct SeriesSpec {
        QString name{};              ///< legend / tooltip label
        QVector<QPointF> points{};   ///< x = epoch ms, y = value (or fill xs / ys instead)
        QVector<double> xs{};        ///< alternative to `points`: x values (epoch ms)…
        QVector<double> ys{};        ///< …and matching y values
        Style style = Style::Line;
        Axis axis = Axis::Left;
        int pane = 0;                ///< 0 = main pane, or an index returned by add_pane()
        ColorRole color = ColorRole::Auto; ///< Auto = next categorical palette colour
        int palette_index = -1;      ///< with ColorRole::Palette (or to pin an Auto slot)
        QColor custom_color{};       ///< with ColorRole::Custom (made legible on the theme)
        bool dashed = false;
        qreal line_width = 1.5;
        std::optional<double> baseline{}; ///< Area fill target / Bars origin / Baseline split
        bool color_by_sign = false;  ///< Bars: up colour >= baseline, down colour below
        bool rebase = true;          ///< follows Rebased100 / Percent (pane 0, non-Bars only)
        bool show_in_legend = true;
        bool show_in_tooltip = true;
        bool visible = true;
        QString id{};                ///< optional stable key for find_series()
        ValueFormatter formatter{};  ///< tooltip value format (default: the axis formatter)
    };

    struct MarkerSpec {
        double x = 0.0;              ///< epoch ms
        int series = -1;             ///< attach to a series (y = its value at x, or `price` in its units)
        double price = std::numeric_limits<double>::quiet_NaN(); ///< explicit y (pane axis units)
        MarkerKind kind = MarkerKind::Custom;
        QString label{};             ///< short text drawn beside the glyph
        QString tooltip{};           ///< shown on hover (multi-line allowed)
        ColorRole color = ColorRole::Auto; ///< Auto = the kind's colour
        int pane = 0;                ///< for markers not attached to a series
    };

    explicit TimeSeriesChart(QWidget* parent = nullptr);
    ~TimeSeriesChart() override;

    // ── Series ──────────────────────────────────────────────────────────────
    /// Add a series; returns its handle (> 0). Unsorted input is sorted by x.
    int add_series(const SeriesSpec& spec);
    void set_series_data(int series, const QVector<QPointF>& points);
    void set_series_data(int series, const QVector<double>& xs, const QVector<double>& ys);
    /// Append a point (live data). An x equal to the last one replaces its y;
    /// an earlier x is inserted in order.
    void append_point(int series, double x, double y);
    void remove_series(int series);
    /// Remove every series and annotation (panes and settings stay).
    void clear();
    void set_series_visible(int series, bool visible);
    bool series_visible(int series) const;
    void set_series_name(int series, const QString& name);
    void set_series_style(int series, Style style);
    /// Stroke width of a series' line in device-independent pixels (logical px:
    /// HiDPI screens get the same physical look as the rest of the UI; it does
    /// not follow the font size). Clamped to [0.5, 12]; NaN / <= 0 are ignored.
    /// Applies to Line / Step / Area / Baseline strokes; Bars are filled and
    /// ignore it. The legend swatch thickens with it (2..4 px). Default 1.5
    /// (SeriesSpec::line_width).
    void set_series_line_width(int series, qreal px);
    /// Current stroke width (0 for an unknown series).
    qreal series_line_width(int series) const;
    /// Handle of the series added with `spec.id == id`, or -1.
    int find_series(const QString& id) const;
    QVector<int> series_ids() const;
    int point_count(int series) const;

    // ── Panes ───────────────────────────────────────────────────────────────
    /// Add a stacked pane below the existing ones; returns its index (1, 2, …).
    /// `height_ratio` is relative to the main pane (1.0).
    int add_pane(double height_ratio = 0.35);
    void set_pane_ratio(int pane, double height_ratio);
    void set_pane_title(int pane, const QString& title);
    /// Number of pane indices handed out so far (0 .. pane_count()-1), removed
    /// panes included — pane indices are stable handles and never shift.
    int pane_count() const;
    /// Show or hide a pane (any pane, 0 included). The shown panes re-flow to
    /// fill the plot by their ratios; the shared x axis, crosshair, tooltip and
    /// legend cover the shown panes only (series on a hidden pane leave the
    /// legend, the tooltip and the x extent until it is shown again; their own
    /// visibility is kept). Hiding the last shown pane is refused. Returns
    /// true when the pane now has the requested visibility (false: no such
    /// pane, removed, or refused). Emits pane_visibility_changed on a change.
    bool set_pane_visible(int pane, bool visible);
    /// True when the pane exists, is not removed and is not hidden.
    bool pane_visible(int pane) const;
    /// True when the pane exists and has not been removed (shown or hidden).
    bool has_pane(int pane) const;
    /// Remove a pane with its series, its horizontal lines and its markers
    /// (bands span every pane and stay). The other panes keep their indices
    /// and re-flow; a removed index is never reused (add_series / add_marker /
    /// add_hline naming it go to the first live pane, as an out-of-range pane
    /// always did). Removing the last pane — or the last shown pane while the
    /// others are hidden — is refused. Returns true when the pane was removed.
    bool remove_pane(int pane);
    /// Pin an axis range (e.g. 0..100 for RSI). NaN for either bound clears it.
    void set_y_range(int pane, Axis axis, double lo, double hi);

    // ── View ────────────────────────────────────────────────────────────────
    /// Show [from, to] (epoch ms). Clamped to the data. Emits range_changed.
    void set_x_range(double from, double to);
    /// Back to the full data range (follows new data). Emits range_changed.
    void reset_x_range();
    /// The visible range as currently resolved.
    double x_from() const;
    double x_to() const;

    /// Preset buttons, right-aligned in the header. Recognised codes: "<n>H",
    /// "<n>D", "<n>W", "<n>M" (months), "<n>Y", "YTD", "ALL"/"MAX". Unknown
    /// codes are still shown and still emit range_preset_selected (for the
    /// caller to handle) but leave the range at the full data.
    void set_range_presets(const QStringList& presets);
    QStringList range_presets() const;
    /// Apply a preset programmatically (no range_preset_selected; range_changed
    /// if the range moves). An empty string returns to the full range.
    void set_active_preset(const QString& preset);
    QString active_preset() const;

    void set_value_mode(ValueMode mode);
    ValueMode value_mode() const;
    void set_log_scale(bool on);
    bool log_scale() const;

    // ── Appearance / behaviour ──────────────────────────────────────────────
    void set_title(const QString& title);
    void set_show_legend(bool on);
    /// LOG / % / =100 toggle chips in the header (also in the context menu).
    void set_show_mode_buttons(bool on);
    void set_crosshair_enabled(bool on);
    void set_zoom_enabled(bool on);
    void set_x_is_time(bool on);
    void set_utc(bool on);
    /// Leave the canvas transparent (host panel shows through) instead of
    /// painting the theme's base background.
    void set_transparent_background(bool on);

    /// Tick / tooltip formatting of an axis' values (default: locale number,
    /// compact K/M/B above a million). Rebased100 / Percent modes override it
    /// for rebased series.
    void set_value_formatter(Axis axis, ValueFormatter fn, int pane = 0);
    /// Crosshair tag and tooltip title for an x value.
    void set_time_formatter(TimeFormatter fn);
    /// X-axis tick labels (default: calendar-aware, see ChartAxisTicks).
    void set_tick_time_formatter(TimeFormatter fn);

    /// Loading draws a small note over existing data (or the message alone
    /// when empty); Empty and Error replace the plot with the message.
    void set_state(State state, const QString& message = {});
    State state() const;

    /// Base file name for image / CSV export (default: the title, else "chart").
    void set_export_name(const QString& name);
    /// Extra actions appended to the right-click menu.
    void set_context_menu_extra(std::function<void(QMenu*)> fn);

    /// Draw a crosshair at `x` without a tooltip — for linking charts through
    /// crosshair_moved. NaN hides it.
    void set_linked_crosshair(double x);

    // ── Annotations (all share one id space) ────────────────────────────────
    int add_marker(const MarkerSpec& spec);
    /// Marker on `series` at x (y = the series' value at or before x).
    int add_marker(double x, int series, MarkerKind kind, const QString& label = {}, const QString& tooltip = {});
    /// Marker at an explicit price on pane 0's left axis. Hidden in
    /// Rebased100 / Percent mode (attach it to a series to keep it).
    int add_price_marker(double x, double price, MarkerKind kind, const QString& label = {},
                         const QString& tooltip = {});
    /// Horizontal line at `y` (axis units). Pane-0 left-axis lines are hidden
    /// in Rebased100 / Percent mode.
    int add_hline(double y, const QString& label = {}, ColorRole role = ColorRole::Muted, int pane = 0,
                  Axis axis = Axis::Left);
    /// Shaded x interval across every pane.
    int add_band(double x_from, double x_to, ColorRole role = ColorRole::Muted, const QString& label = {});
    void remove_annotation(int id);
    void clear_annotations();

    // ── Export ──────────────────────────────────────────────────────────────
    /// The chart without crosshair / tooltip, at the device pixel ratio.
    QPixmap render_image();
    /// Wide table: x column + one column per visible series (exact x matches;
    /// blank where a series has no point). Numbers in C locale.
    void csv_table(QStringList& headers, QVector<QStringList>& rows) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// Visible x range changed by zoom, pan, preset, set_x_range or reset (not
    /// when the range merely follows newly added data).
    void range_changed(double from, double to);
    /// The user clicked a preset button.
    void range_preset_selected(const QString& preset);
    void marker_clicked(int id);
    /// The user (legend click) or set_series_visible toggled a series.
    void series_visibility_changed(int series, bool visible);
    /// set_pane_visible changed a pane's visibility.
    void pane_visibility_changed(int pane, bool visible);
    void value_mode_changed(fincept::ui::TimeSeriesChart::ValueMode mode);
    void log_scale_changed(bool on);
    /// Snapped x under the mouse; NaN when the crosshair goes away.
    void crosshair_moved(double x);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d;

    // TimeSeriesChart.cpp — data, view, interaction
    void invalidate();
    bool data_extent(double& lo, double& hi) const;
    void resolve_view(double& from, double& to) const;
    bool preset_range(const QString& code, double lo, double hi, double& from, double& to) const;
    enum class ViewChange { Auto, Preset, Manual };
    void apply_view(ViewChange mode, double from, double to, const QString& preset);
    void zoom_at(double anchor, double factor);
    void update_hover(const QPoint& pos);
    void update_crosshair(const QPoint& pos);
    void click_header_item(int hit_index);
    void build_context_menu(QMenu* menu);
    void drop_pointer_state();

    // TsChartPaint.cpp — layout and painting
    void compute_layout();
    void rebuild_cache();
    void paint_header(QPainter& p);
    void paint_pane(QPainter& p, int pane);
    void paint_series(QPainter& p, int series_index, int pane);
    void paint_annotations(QPainter& p, int pane);
    void paint_x_axis(QPainter& p);
    void paint_overlay(QPainter& p);
    QString format_value(int pane, int axis, double v) const;
    QString format_tick_value(int pane, int axis, double v) const;
    QString format_time(double x) const;
};

} // namespace fincept::ui
