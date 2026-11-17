#pragma once
// FanChart.h — percentile fan over time (Monte Carlo paths, forecast
// intervals): nested shaded bands, a median line, optional sample paths and
// horizontal reference lines, with a hover crosshair that reads out every band.
// Extra lines (add_line: the original / realised path) are drawn over the fan,
// included in the auto-range and listed in a legend above the plot (the legend
// appears only while at least one extra line exists).

#include "ui/charts/ChartKit.h"
#include "ui/theme/ChartStyle.h"

#include <QPair>
#include <QString>
#include <QVector>

namespace fincept::ui {

struct FanBand {
    double lower_pct = 5;  // e.g. 5  (for the legend / tooltip)
    double upper_pct = 95; // e.g. 95
    QVector<double> lower; // one value per x
    QVector<double> upper;
};

class FanChart : public PaintedChart {
    Q_OBJECT
  public:
    explicit FanChart(QWidget* parent = nullptr);

    /// Bands in any order (drawn widest first); `median` optional. Every
    /// series must have x.size() values.
    void set_fan(const QVector<double>& x, const QVector<FanBand>& bands, const QVector<double>& median = {});
    /// Builds the bands from simulated `paths` (each x.size() long): one band
    /// per (lower, upper) percentile pair plus the median. `sample_paths` of the
    /// paths are drawn as faint lines (0 = none).
    void set_paths(const QVector<double>& x, const QVector<QVector<double>>& paths,
                   const QVector<QPair<double, double>>& percentile_pairs = {{5, 95}, {25, 75}},
                   int sample_paths = 0);
    /// Removes the fan, the sample paths and the extra lines (reference lines stay).
    void clear();

    /// An extra path over the fan — e.g. the original / realised path. `values` align
    /// index-for-index with x (NaN = gap; a shorter vector covers the first x values; it is kept
    /// across set_fan / set_paths, so it may be added before or after them). Colour by `role`
    /// (ui::ChartColorRole, live theme): Text (default) = primary text, the strongest contrast
    /// with the fan on both themes; Auto / Palette = the next categorical slot, never the fan's
    /// own; Accent, Positive / Negative (direction palette), Warning, Info, Cyan, Muted. Drawn
    /// 2 px (dashed when `dashed`), listed in the legend and the hover read-out, and included in
    /// the y auto-range. Returns its id.
    int add_line(const QVector<double>& values, const QString& label, ChartColorRole role = ChartColorRole::Text,
                 bool dashed = false);
    void remove_line(int id);
    void clear_lines();
    int line_count() const { return static_cast<int>(lines_.size()); }

    void add_reference_line(double y, const QString& label);
    void clear_reference_lines();
    /// Formats x values (axis and tooltip). Default: compact number. The value
    /// formatter (set_value_formatter) formats y values.
    void set_x_formatter(Formatter formatter);

    QSize sizeHint() const override { return {480, 260}; }
    QSize minimumSizeHint() const override { return {160, 100}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override {
        return x_.size() >= 2 && (!bands_.isEmpty() || !median_.isEmpty() || !lines_.isEmpty());
    }
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;

  private:
    struct Line {
        int id = 0;
        QVector<double> values;
        QString label;
        ChartColorRole role = ChartColorRole::Text;
        int palette_slot = 0;
        bool dashed = false;
    };
    struct Frame {
        QRectF plot;
        QRectF legend; // empty when no extra line exists
        double x_lo = 0;
        double x_hi = 1;
        double y_lo = 0;
        double y_hi = 1;
        QVector<double> y_ticks;
        QVector<double> x_ticks;
    };
    Frame frame(const QRectF& r) const;
    QString x_text(double x) const;
    QColor fan_color() const;
    QColor line_color(const Line& line) const;
    QString line_label(int index) const;
    void paint_legend(QPainter& p, const QRectF& legend) const;

    QVector<double> x_;
    QVector<FanBand> bands_; // sorted widest first
    QVector<double> median_;
    QVector<QVector<double>> samples_;
    QVector<QPair<double, QString>> refs_;
    QVector<Line> lines_;
    int next_line_id_ = 1;
    int next_line_slot_ = 0;
    Formatter x_formatter_;
    int hover_index_ = -1;
};

} // namespace fincept::ui
