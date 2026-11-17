#pragma once
// BarChart.h — category bar chart: vertical columns or horizontal bars;
// grouped, stacked or waterfall; optional signed (up / down) colouring and
// value labels; legend for multiple series; hover tooltip and click signal.

#include "ui/theme/ChartStyle.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::ui {

struct BarSeries {
    QString name;
    QVector<double> values; // one per category; NaN = no bar
    QColor color;           // invalid = categorical palette colour of the series index
};

class BarChart : public PaintedChart {
    Q_OBJECT
  public:
    /// Grouped: series side by side. Stacked: series on top of each other
    /// (positive up, negative down). Waterfall: the first series' values are
    /// deltas from a running total, with an optional total bar.
    enum class Mode { Grouped, Stacked, Waterfall };

    explicit BarChart(QWidget* parent = nullptr);

    void set_categories(const QStringList& categories);
    void set_series(const QVector<BarSeries>& series);
    /// Single-series convenience.
    void set_data(const QStringList& categories, const QVector<double>& values, const QString& name = {});
    void clear();

    void set_mode(Mode mode);
    Mode mode() const { return mode_; }
    /// Qt::Vertical = columns (categories along x, the default);
    /// Qt::Horizontal = bars (categories down the left side).
    void set_orientation(Qt::Orientation orientation);
    /// Colour by sign with the direction colours (single series / waterfall).
    /// Default false; waterfall bars are always signed.
    void set_signed_colors(bool on);
    /// Value labels at the bar ends (inside stacked segments when they fit). Default false.
    void set_show_values(bool on);
    /// Waterfall: append a total bar. Default true, label "Total".
    void set_waterfall_total(bool show, const QString& label = {});
    /// Legend above the plot. Default: shown for two or more series.
    /// (Values on the axis, labels and tooltip use set_value_formatter.)
    void set_show_legend(bool on);

    const QStringList& categories() const { return categories_; }

    QSize sizeHint() const override { return {480, 260}; }
    QSize minimumSizeHint() const override { return {160, 100}; }

  signals:
    /// `category` == categories().size() for the waterfall total bar.
    void bar_clicked(int series, int category);
    /// (-1, -1) when the mouse leaves the bars.
    void bar_hovered(int series, int category);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override;
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

  private:
    struct Bar {
        int series = 0;
        int category = 0;
        double from = 0; // value-axis start
        double to = 0;   // value-axis end
        double value = 0; // the series value (delta for waterfall)
        QColor color;
        QRectF rect; // widget coordinates
    };
    struct Frame {
        QRectF plot;
        QRectF legend;
        double lo = 0;
        double hi = 1;
        QVector<double> ticks;
        int n_cat = 0;
        qreal band = 0; // category band size along the category axis
        int label_step = 1;
    };

    Frame frame(const QRectF& r) const;
    void layout_bars(const Frame& f, QVector<Bar>* bars) const;
    QColor series_color(int s) const;
    QString category_label(int c) const;
    int category_count() const;
    int bar_at(const QPointF& pos) const;

    QStringList categories_;
    QVector<BarSeries> series_;
    Mode mode_ = Mode::Grouped;
    Qt::Orientation orientation_ = Qt::Vertical;
    bool signed_ = false;
    bool show_values_ = false;
    bool waterfall_total_ = true;
    QString total_label_;
    int legend_mode_ = -1; // -1 auto, 0 off, 1 on

    QVector<Bar> bars_; // last layout (for hit-testing and the overlay)
    QRectF bars_for_;
    int hover_ = -1;
};

} // namespace fincept::ui
