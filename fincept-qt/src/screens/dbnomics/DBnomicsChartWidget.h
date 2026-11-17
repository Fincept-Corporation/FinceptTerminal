// src/screens/dbnomics/DBnomicsChartWidget.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QEvent>
#include <QList>
#include <QPointF>
#include <QPointer>
#include <QStackedWidget>
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QXYSeries>

#include <algorithm>

class QTimer;

namespace fincept::ui {
class LoadingSkeleton;
}

namespace fincept::screens {

/// `points` (ascending x) reduced for a plot `buckets` pixels wide over [x_min, x_max]: per pixel
/// column the lowest and the highest point (in their order), plus the first and the last point — no
/// extreme is ever dropped, and the line looks the same at that width. O(n); at most 2 × buckets + 2
/// points; returned unchanged when it has no more than that. Pure; any thread.
inline QList<QPointF> dbn_downsample(const QList<QPointF>& points, double x_min, double x_max, int buckets) {
    const qsizetype n = points.size();
    if (buckets <= 0 || n <= 2 * qsizetype(buckets) + 2 || !(x_max > x_min))
        return points;
    QList<QPointF> out;
    out.reserve(2 * qsizetype(buckets) + 2);
    out.append(points.first());
    const double scale = buckets / (x_max - x_min);
    int bucket = -1;
    qsizetype lo = -1;
    qsizetype hi = -1;
    const auto flush = [&]() {
        if (lo < 0)
            return;
        if (lo == hi) {
            out.append(points.at(lo));
        } else {
            out.append(points.at(std::min(lo, hi)));
            out.append(points.at(std::max(lo, hi)));
        }
    };
    for (qsizetype i = 1; i + 1 < n; ++i) {
        const QPointF& p = points.at(i);
        const int b = std::clamp(static_cast<int>((p.x() - x_min) * scale), 0, buckets - 1);
        if (b != bucket) {
            flush();
            bucket = b;
            lo = hi = i;
            continue;
        }
        if (p.y() < points.at(lo).y())
            lo = i;
        if (p.y() > points.at(hi).y())
            hi = i;
    }
    flush();
    out.append(points.last());
    return out;
}

/// Chart widget for DBnomics data.
/// Supports Line, Area, Bar and Scatter chart types.
/// Call set_compact(true) when used inside the 2-column comparison grid
/// to use tighter margins and hide the legend.
/// set_loading(true) shows a chart-shaped ui::LoadingSkeleton; an empty chart has no placeholder
/// title — the owning screen shows a designed ui::EmptyState instead of an empty chart.
/// Time series are drawn down-sampled to the plot's pixel width (dbn_downsample: min / max per pixel
/// column, first / last point kept) and sampled again from the full series when the plot is resized,
/// so a long daily series costs QtCharts a few thousand points, not every observation.
class DBnomicsChartWidget : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsChartWidget(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series, services::DbnChartType chart_type);
    void clear();
    void set_loading(bool on);

    /// Compact mode: tighter margins, no legend — use in comparison grid slots.
    void set_compact(bool compact);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    /// Re-applies theme colours and fonts to the chart, its axes and legend (on appearance_settled).
    void refresh_theme();
    /// Line / area / scatter on a date axis (period start days).
    void render_time_series(const QVector<services::DbnDataPoint>& series, bool area, bool scatter);
    void render_bar(const QVector<services::DbnDataPoint>& series);

    int y_tick_count() const;
    int x_tick_count(int period_count) const;
    /// Width of the plot area in pixels (an estimate before the first layout) — the down-sampling buckets.
    int plot_pixel_width() const;
    /// Samples every plotted series again from its full points for the current plot width.
    void resample();

    /// One time series on the chart: the QtCharts series and its full-resolution points.
    struct PlottedSeries {
        QPointer<QXYSeries> series;
        QList<QPointF> full;
        bool sampled = false; ///< the points on the chart are a down-sampled subset
    };

    QStackedWidget* stack_ = nullptr;
    ui::LoadingSkeleton* skeleton_ = nullptr;
    QChartView* chart_view_ = nullptr;
    bool compact_ = false;
    QList<PlottedSeries> plotted_;
    double plot_x_min_ = 0.0;
    double plot_x_max_ = 0.0;
    int sampled_buckets_ = 0;           ///< the width the plotted series were sampled for
    QTimer* resample_timer_ = nullptr; ///< coalesces the plot-area changes of a live resize
};

} // namespace fincept::screens
