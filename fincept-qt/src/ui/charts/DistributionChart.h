#pragma once
// DistributionChart.h — histogram of samples (returns, P&L, simulated
// outcomes) with an optional fitted normal curve and labelled vertical markers
// (mean, median, VaR, CVaR, ...). For Monte Carlo percentile fans over time
// see FanChart.

#include "ui/theme/ChartStyle.h"

#include <QString>
#include <QVector>

namespace fincept::ui {

class DistributionChart : public PaintedChart {
    Q_OBJECT
  public:
    /// Colour role of a marker line: Mean / Accent use the theme accent,
    /// Median / Neutral muted text, VaR / CVaR the "down" direction colour.
    enum class MarkerRole { Neutral, Mean, Median, VaR, CVaR, Accent };

    explicit DistributionChart(QWidget* parent = nullptr);

    /// Non-finite samples are ignored. `bins` <= 0 picks the bin count
    /// automatically (Freedman–Diaconis, 10..80).
    void set_samples(const QVector<double>& values, int bins = 0);
    void clear();

    /// A normal curve with the samples' mean and standard deviation. Default false.
    void set_normal_overlay(bool on);
    void add_marker(double x, const QString& label, MarkerRole role = MarkerRole::Neutral);
    void clear_markers();
    /// Colour bins below 0 with the "down" and above with the "up" direction
    /// colour (return / P&L distributions). Default false: one series colour.
    void set_signed_colors(bool on);
    /// Y axis as a percentage of samples (default) instead of counts.
    void set_show_percent(bool on);

    int bin_count() const { return static_cast<int>(counts_.size()); }
    int sample_count() const { return n_; }
    double mean() const { return mean_; }
    double stdev() const { return sd_; }
    /// Sample quantile q in [0, 1] (linear interpolation) of the current samples.
    double quantile(double q) const;

    QSize sizeHint() const override { return {420, 240}; }
    QSize minimumSizeHint() const override { return {160, 100}; }

  signals:
    void bin_clicked(double lo, double hi, int count);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return n_ >= 2 && !counts_.isEmpty(); }
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

  private:
    struct Marker {
        double x = 0;
        QString label;
        MarkerRole role = MarkerRole::Neutral;
    };
    struct Frame {
        QRectF plot;
        double x_lo = 0;
        double x_hi = 1;
        double y_hi = 1;
        QVector<double> x_ticks;
        QVector<double> y_ticks;
    };

    void rebin();
    Frame frame(const QRectF& r) const;
    double y_of(int count) const; // count → displayed unit
    QColor marker_color(MarkerRole role) const;
    int bin_at(const QPointF& pos) const;

    QVector<double> sorted_;
    int requested_bins_ = 0;
    QVector<int> counts_;
    double lo_ = 0;
    double width_ = 1;
    int n_ = 0;
    double mean_ = 0;
    double sd_ = 0;
    bool normal_ = false;
    bool signed_ = false;
    bool percent_ = true;
    QVector<Marker> markers_;
    int hover_bin_ = -1;
};

} // namespace fincept::ui
