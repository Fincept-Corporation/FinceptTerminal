#pragma once
// ChartStyle.h — the shared base and paint helpers for the terminal's
// custom-painted (QPainter) chart widgets: Treemap, HeatmapGrid,
// MatrixHeatmap, MonthlyReturnsHeatmap, DistributionChart, FanChart, RangeBar,
// Gauge, DonutChart, BarChart and the table delegates.
//
// Everything here reads colours and fonts from the active theme at paint time
// (never a hex literal or a font family / pixel size of its own), so a chart
// follows theme, font-size and direction-palette changes.

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QPainter;
class QTimer;

namespace fincept::ui {

/// Base class of the custom-painted charts. Provides:
///   * a Loading / Empty / Error state overlay (set_state),
///   * a value formatter hook (set_value_formatter → format_value),
///   * a HiDPI pixmap cache of the static rendering (paint_chart), with a
///     per-repaint overlay pass for hover feedback (paint_overlay),
///   * theme-change handling (cache dropped, on_theme_changed() called),
///   * hover tooltips (show_tip / hide_tip) styled by the theme's QToolTip rule.
class PaintedChart : public QWidget {
    Q_OBJECT
  public:
    enum State { Ready, Loading, Empty, Error };
    Q_ENUM(State)
    using Formatter = std::function<QString(double)>;

    explicit PaintedChart(QWidget* parent = nullptr);
    ~PaintedChart() override;

    /// Loading / Empty / Error draw an overlay instead of the chart, with
    /// `message` (empty = a default text). Ready draws the chart. A chart with
    /// no data draws the Empty state even while Ready.
    void set_state(State state, const QString& message = {});
    State state() const { return state_; }
    QString state_message() const { return state_message_; }

    /// Formats values in labels, legends and tooltips. An empty function
    /// restores the widget's default formatting.
    void set_value_formatter(Formatter formatter);
    QString format_value(double v) const;

  protected:
    /// Draw the chart into `r` (widget coordinates, contents margins removed).
    /// Cached in a device-pixel-ratio pixmap until invalidate(): draw here
    /// everything that does not depend on the mouse.
    virtual void paint_chart(QPainter& p, const QRectF& r) = 0;
    /// Drawn on every repaint on top of the cache — hover highlight, crosshair.
    virtual void paint_overlay(QPainter& p, const QRectF& r);
    /// Value text when no formatter is set.
    virtual QString default_format(double v) const;
    /// False when there is nothing to draw; the Empty state is shown then.
    virtual bool has_data() const { return true; }
    /// Theme / font changed: drop own colour or geometry caches (the pixmap
    /// cache is dropped by the base).
    virtual void on_theme_changed() {}
    /// The mouse left the widget (the tooltip is already hidden).
    virtual void on_hover_left() {}

    /// Drop the cached rendering and repaint — call after any data / option change.
    void invalidate();
    /// True when the chart itself is drawn (Ready with data) — use to skip hit-testing.
    bool chart_visible() const;

    /// Show `text` (rich text allowed) as a tooltip near `local_pos`; it stays
    /// while the mouse is inside `hot_rect` (widget coordinates; empty = until
    /// the next call). Empty `text` hides it.
    void show_tip(const QPoint& local_pos, const QString& text, const QRect& hot_rect = {});
    void hide_tip();

    /// Widget rect minus contents margins.
    QRectF content_rect() const;

    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void paint_state_overlay(QPainter& p, const QRectF& r, State shown);
    void sync_animation();

    QPixmap cache_;
    bool cache_valid_ = false;
    State state_ = Ready;
    QString state_message_;
    Formatter formatter_;
    QTimer* anim_ = nullptr;
    int anim_phase_ = 0;
    QString tip_text_;
};

/// Paint helpers shared by the chart widgets and table delegates.
namespace chart_style {

/// The user's font at `offset` px from the base size (the chart default is -3).
QFont font(int offset = -3, int weight = QFont::Normal);

QColor grid_color();       // gridlines
QColor axis_color();       // axis lines / baselines
QColor axis_text_color();  // tick labels
QColor label_color();      // primary labels and values
QColor muted_color();      // secondary labels
QColor highlight_color();  // hover outline / selection ring
QColor empty_cell_color(); // a cell without a value (NaN)

/// Default number text: thousands separators, up to `decimals` decimals,
/// K / M / B / T suffixes from 10 000 up.
QString compact_number(double v, int decimals = 2);
/// "+1.23%" style; `v` is already in percent units.
QString percent_text(double v, int decimals = 2, bool show_sign = true);

/// "Nice" axis ticks covering [lo, hi] with about `target` steps. Writes the
/// rounded outer bounds to `nice_lo` / `nice_hi` when given.
QVector<double> nice_ticks(double lo, double hi, int target, double* nice_lo = nullptr, double* nice_hi = nullptr);

/// Widest tick label among `ticks` formatted by `fmt` in the axis font.
qreal axis_label_width(const QVector<double>& ticks, const std::function<QString(double)>& fmt);
/// Axis font (tick labels).
QFont axis_font();

/// A value axis for `ticks` spanning [lo, hi] across `plot`:
///   Qt::Vertical   — values run bottom → top: horizontal gridlines, labels
///                    right-aligned just left of the plot;
///   Qt::Horizontal — values run left → right: vertical gridlines, labels
///                    centred just below the plot.
/// The gridline at 0 (when inside the range) is drawn in the axis colour.
void draw_value_axis(QPainter& p, const QRectF& plot, const QVector<double>& ticks, double lo, double hi,
                     Qt::Orientation orientation, const std::function<QString(double)>& fmt);

/// Centre up to three `lines` in `cell` with the largest font (offset from
/// `max_offset` down to `min_offset`) at which the first line fits; the first
/// line is bold, later lines are 2 px smaller and dropped when they do not
/// fit. Returns false (drawing nothing) when not even the first line fits.
bool draw_cell_text(QPainter& p, const QRectF& cell, const QStringList& lines, const QColor& color, int max_offset,
                    int min_offset);

/// A colour-scale legend: a ramp across `r` (diverging when `diverging`, else
/// sequential) with the formatted `lo`, middle and `hi` values under it.
/// Needs about legend_height() of vertical space.
void draw_scale_legend(QPainter& p, const QRectF& r, bool diverging, double lo, double hi,
                       const std::function<QString(double)>& fmt);
int legend_height();

/// A legend swatch + label row; returns the width used.
qreal draw_swatch_label(QPainter& p, const QPointF& top_left, const QColor& swatch, const QString& label,
                        const QColor& text_color);

/// Escaped, theme-coloured HTML tooltip body: a bold title line and
/// "label: value" rows.
QString tooltip_html(const QString& title, const QVector<QPair<QString, QString>>& rows);

} // namespace chart_style

} // namespace fincept::ui
