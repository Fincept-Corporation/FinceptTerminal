// src/screens/dbnomics/DBnomicsChartWidget.cpp
#include "screens/dbnomics/DBnomicsChartWidget.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QAreaSeries>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QHash>
#include <QLegend>
#include <QLineSeries>
#include <QMap>
#include <QResizeEvent>
#include <QScatterSeries>
#include <QSet>
#include <QStackedWidget>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

// ── helpers ──────────────────────────────────────────────────────────────────

/// Theme colours and the user's font for an axis — applied when it is built and again on a theme change.
static void style_axis(QAbstractAxis* ax) {
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setLabelsFont(ui::fonts::font(-3));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ax->setLinePen(QPen(QColor(ui::colors::BORDER_MED())));
    ax->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    ax->setTitleFont(ui::fonts::font(-3));
}

static QValueAxis* make_y_axis(double y_min, double y_max, int tick_count, const QString& format) {
    auto* ax = new QValueAxis();
    ax->setRange(y_min, y_max);
    ax->setTickCount(tick_count);
    style_axis(ax);
    ax->setLabelFormat(format);
    ax->setMinorTickCount(0);
    return ax;
}

// Y scaling. "%.4g" printed 21433 as "2.143e+04". Large magnitudes are divided by a power of 1,000
// (named in the axis title) and every range gets a fixed-decimal format that fits it.
struct DbnYScale {
    double divisor = 1.0;
    QString title;
    QString format = QStringLiteral("%.2f");
};

static DbnYScale dbn_y_scale(double y_min, double y_max) {
    DbnYScale sc;
    const double max_abs = std::max(std::abs(y_min), std::abs(y_max));
    if (max_abs >= 1e4) {
        const int exp3 = static_cast<int>(std::floor(std::log10(max_abs) / 3.0)) * 3;
        sc.divisor = std::pow(10.0, exp3);
        switch (exp3) {
            case 3:
                sc.title = DBnomicsChartWidget::tr("thousands");
                break;
            case 6:
                sc.title = DBnomicsChartWidget::tr("millions");
                break;
            case 9:
                sc.title = DBnomicsChartWidget::tr("billions");
                break;
            case 12:
                sc.title = DBnomicsChartWidget::tr("trillions");
                break;
            default:
                sc.title = DBnomicsChartWidget::tr("x 1e%1").arg(exp3);
                break;
        }
        sc.format = QStringLiteral("%.1f");
    } else if (max_abs >= 100) {
        sc.format = QStringLiteral("%.0f");
    } else if (max_abs >= 10) {
        sc.format = QStringLiteral("%.1f");
    } else if (max_abs >= 0.1) {
        sc.format = QStringLiteral("%.2f");
    } else {
        sc.format = QStringLiteral("%.4f");
    }
    return sc;
}

/// Midnight UTC of `d` in ms since the epoch — arithmetic on the Julian day (a QDateTime per
/// observation cost more than the rest of the chart for a long daily series).
static qint64 dbn_msecs(const QDate& d) {
    constexpr qint64 kUnixEpochJulianDay = 2440588; // 1970-01-01
    return (d.toJulianDay() - kUnixEpochJulianDay) * 86400000LL;
}

/// Date-axis label format from the finest frequency on the chart.
static QString dbn_date_format(const QVector<services::DbnDataPoint>& series) {
    bool daily = false;
    bool sub_annual = false;
    for (const auto& dp : series) {
        const QString f = dp.frequency.toLower();
        if (f.contains(QLatin1String("daily")) || f.contains(QLatin1String("week")) ||
            f.contains(QLatin1String("business")))
            daily = true;
        else if (!f.contains(QLatin1String("annual")) || f.contains(QLatin1String("semi")))
            sub_annual = true;
    }
    if (daily)
        return QStringLiteral("dd MMM yy");
    return sub_annual ? QStringLiteral("MMM yyyy") : QStringLiteral("yyyy");
}

// X axis on real dates (DBnomics period_start_day). The previous index axis spaced a missing year like
// any other step, and periods of different frequency ("2024-12" vs "2024-Q1") sorted lexically.
static QDateTimeAxis* make_time_axis(qint64 min_ms, qint64 max_ms, int tick_count, const QString& format) {
    auto* ax = new QDateTimeAxis();
    if (max_ms <= min_ms)
        max_ms = min_ms + 86400000LL; // a single point still needs a non-empty range
    ax->setRange(QDateTime::fromMSecsSinceEpoch(min_ms, QTimeZone::UTC),
                 QDateTime::fromMSecsSinceEpoch(max_ms, QTimeZone::UTC));
    ax->setFormat(format);
    ax->setTickCount(std::max(2, tick_count));
    style_axis(ax);
    return ax;
}

static void style_chart(QChart* chart, bool compact) {
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    // Explicit margins so axis labels are never clipped.
    chart->setMargins(compact ? QMargins(4, 4, 4, 4) : QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(!compact);
    if (!compact) {
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
        chart->legend()->setFont(ui::fonts::font(-3));
    }
    chart->setTitleFont(ui::fonts::font(compact ? -2 : 0, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER())));
}

// ── DBnomicsChartWidget ───────────────────────────────────────────────────────

DBnomicsChartWidget::DBnomicsChartWidget(QWidget* parent) : QWidget(parent) {
    // A resize changes the plot width step by step: the series are sampled again once it settles.
    resample_timer_ = new QTimer(this);
    resample_timer_->setSingleShot(true);
    resample_timer_->setInterval(60);
    connect(resample_timer_, &QTimer::timeout, this, &DBnomicsChartWidget::resample);
    build_ui();
    // Chart, axis and legend colours / fonts live in QtCharts objects, which no stylesheet reaches:
    // re-applied once the new appearance has settled. Series colours are re-assigned (and the data
    // re-rendered) by the owning screen.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &DBnomicsChartWidget::refresh_theme);
}

void DBnomicsChartWidget::refresh_theme() {
    QChart* chart = chart_view_ ? chart_view_->chart() : nullptr;
    if (!chart)
        return;
    style_chart(chart, compact_);
    chart_view_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    const auto axes = chart->axes();
    for (auto* axis : axes)
        style_axis(axis);
}

void DBnomicsChartWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);

    // ── Page 0: loading — a chart-shaped skeleton (custom-painted from the live theme) ──
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Chart, stack_);
    stack_->addWidget(skeleton_); // index 0

    // ── Page 1: chart view ────────────────────────────────────────────────────
    chart_view_ = new QChartView(stack_);
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    chart_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* chart = new QChart();
    style_chart(chart, false);
    chart_view_->setChart(chart);
    // The plot area is known (and changes) only after QtCharts lays the chart out: sample the series
    // again for the width it really has.
    connect(chart, &QChart::plotAreaChanged, this, [this](const QRectF&) {
        if (!plotted_.isEmpty())
            resample_timer_->start();
    });
    chart_view_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    stack_->addWidget(chart_view_); // index 1

    stack_->setCurrentIndex(1);
    root->addWidget(stack_, 1);
}

// ── Live language switch ─────────────────────────────────────────────────────

void DBnomicsChartWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && skeleton_ && !skeleton_->message().isEmpty())
        skeleton_->set_message(tr("Loading observations…"));
    QWidget::changeEvent(event);
}

void DBnomicsChartWidget::set_compact(bool compact) {
    compact_ = compact;
    // Re-style the existing chart if already built
    if (chart_view_ && chart_view_->chart())
        style_chart(chart_view_->chart(), compact_);
}

void DBnomicsChartWidget::set_loading(bool on) {
    if (on) {
        skeleton_->set_message(tr("Loading observations…"));
        stack_->setCurrentIndex(0);
    } else {
        stack_->setCurrentIndex(1);
    }
}

void DBnomicsChartWidget::clear() {
    plotted_.clear();
    resample_timer_->stop();
    QChart* chart = chart_view_->chart();
    chart->removeAllSeries();
    const auto axes = chart->axes();
    for (auto* axis : axes) {
        // QChart::removeAxis() hands ownership back to the caller — without a
        // delete every axis built by set_data() leaked on each re-render.
        // deleteLater() (P13) keeps us off the current call stack.
        chart->removeAxis(axis);
        axis->deleteLater();
    }
    // No placeholder title: an empty chart is never on screen on its own (the screen shows a
    // designed empty state instead).
    chart->setTitle(QString());
}

void DBnomicsChartWidget::set_data(const QVector<services::DbnDataPoint>& series, services::DbnChartType chart_type) {
    clear();
    stack_->setCurrentIndex(1);
    if (series.isEmpty())
        return;

    if (chart_type == services::DbnChartType::Bar)
        render_bar(series);
    else if (chart_type == services::DbnChartType::Scatter)
        render_time_series(series, false, true);
    else
        render_time_series(series, chart_type == services::DbnChartType::Area, false);

    // Re-apply style after rendering (compact flag may differ per instance)
    style_chart(chart_view_->chart(), compact_);

    LOG_DEBUG("DBnomicsChartWidget", QString("Rendered %1 series").arg(series.size()));
}

// ── Adaptive tick count based on available pixel size ───────────────────────
int DBnomicsChartWidget::y_tick_count() const {
    int h = chart_view_->height();
    if (h <= 0)
        h = height();
    return std::max(2, std::min(8, h / 60));
}

int DBnomicsChartWidget::x_tick_count(int period_count) const {
    int w = chart_view_->width();
    if (w <= 0)
        w = width();
    int max_from_width = std::max(2, w / 90);
    return std::min(max_from_width, std::min(12, period_count));
}

// ── resizeEvent: update tick counts so axes stay legible ─────────────────────
void DBnomicsChartWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    QChart* chart = chart_view_->chart();
    if (!chart)
        return;
    const auto y_axes = chart->axes(Qt::Vertical);
    for (auto* ax : y_axes) {
        if (auto* vax = qobject_cast<QValueAxis*>(ax))
            vax->setTickCount(y_tick_count());
    }
    const auto x_axes = chart->axes(Qt::Horizontal);
    for (auto* ax : x_axes) {
        if (auto* dax = qobject_cast<QDateTimeAxis*>(ax))
            dax->setTickCount(x_tick_count(12));
    }
}

// ── Down-sampling to the plot width ──────────────────────────────────────────
int DBnomicsChartWidget::plot_pixel_width() const {
    qreal w = chart_view_->chart() ? chart_view_->chart()->plotArea().width() : 0.0;
    if (w < 50.0)
        w = chart_view_->width() - 80; // not laid out yet: the view less the axis labels
    if (w < 50.0)
        w = width() - 80;
    if (w < 50.0)
        w = 800.0; // hidden and never shown: a typical plot width; sampled again once it is shown
    return std::clamp(static_cast<int>(std::ceil(w)), 50, 8192);
}

void DBnomicsChartWidget::resample() {
    if (plotted_.isEmpty())
        return;
    const int buckets = plot_pixel_width();
    if (buckets == sampled_buckets_)
        return;
    sampled_buckets_ = buckets;
    for (PlottedSeries& p : plotted_) {
        if (!p.series)
            continue;
        const bool sample = p.full.size() > 2 * qsizetype(buckets) + 2;
        if (!sample && !p.sampled)
            continue; // every point is on the chart already
        p.series->replace(dbn_downsample(p.full, plot_x_min_, plot_x_max_, buckets));
        p.sampled = sample;
    }
}

// ── Line / area / scatter on a time axis ─────────────────────────────────────
// Each series' points are handed over in ONE replace() call — per-point append() emitted a signal and
// scheduled a repaint for every observation — and only as many as the plot is wide (dbn_downsample);
// the full points are kept for resizes.
void DBnomicsChartWidget::render_time_series(const QVector<services::DbnDataPoint>& series, bool area, bool scatter) {
    QChart* chart = chart_view_->chart();
    double y_min = std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();
    qint64 x_min = std::numeric_limits<qint64>::max();
    qint64 x_max = std::numeric_limits<qint64>::min();
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            if (!obs.valid || !obs.start.isValid())
                continue;
            y_min = std::min(y_min, obs.value);
            y_max = std::max(y_max, obs.value);
            const qint64 x = dbn_msecs(obs.start);
            x_min = std::min(x_min, x);
            x_max = std::max(x_max, x);
        }
    }
    if (y_min > y_max)
        return; // nothing plottable

    const DbnYScale sc = dbn_y_scale(y_min, y_max);
    const double lo = y_min / sc.divisor;
    const double hi = y_max / sc.divisor;
    double margin = (hi - lo) * 0.1;
    if (margin == 0.0)
        margin = std::abs(lo) * 0.1 + 1.0;

    auto* x_axis = make_time_axis(x_min, x_max, x_tick_count(12), dbn_date_format(series));
    chart->addAxis(x_axis, Qt::AlignBottom);
    auto* y_axis = make_y_axis(lo - margin, hi + margin, y_tick_count(), sc.format);
    y_axis->setTitleText(sc.title);
    chart->addAxis(y_axis, Qt::AlignLeft);

    plot_x_min_ = static_cast<double>(x_min);
    plot_x_max_ = static_cast<double>(std::max(x_max, x_min + 86400000LL)); // the axis' range
    const int buckets = plot_pixel_width();
    sampled_buckets_ = buckets;

    for (const auto& dp : series) {
        QList<QPointF> full;
        full.reserve(dp.observations.size());
        for (const auto& obs : dp.observations)
            if (obs.valid && obs.start.isValid())
                full.append(QPointF(static_cast<qreal>(dbn_msecs(obs.start)), obs.value / sc.divisor));
        const bool sampled = full.size() > 2 * qsizetype(buckets) + 2;
        const QList<QPointF> points = sampled ? dbn_downsample(full, plot_x_min_, plot_x_max_, buckets) : full;

        if (scatter) {
            auto* dots = new QScatterSeries();
            dots->setName(dp.series_name);
            dots->setColor(dp.color);
            dots->setMarkerSize(compact_ ? 4.0 : 6.0);
            dots->replace(points);
            chart->addSeries(dots);
            dots->attachAxis(x_axis);
            dots->attachAxis(y_axis);
            plotted_.append({QPointer<QXYSeries>(dots), std::move(full), sampled});
            continue;
        }

        auto* line = new QLineSeries();
        line->setName(dp.series_name);
        line->setColor(dp.color);
        QPen pen(dp.color);
        pen.setWidth(2);
        line->setPen(pen);
        line->replace(points);
        plotted_.append({QPointer<QXYSeries>(line), std::move(full), sampled});

        if (area && !points.isEmpty()) {
            // Fill down to zero, or to the axis floor when the whole series sits away from zero.
            auto* base = new QLineSeries();
            const double base_y = std::clamp(0.0, lo - margin, hi + margin);
            base->replace(QList<QPointF>{QPointF(points.first().x(), base_y), QPointF(points.last().x(), base_y)});
            auto* fill = new QAreaSeries(line, base);
            fill->setName(dp.series_name);
            QColor c = dp.color;
            c.setAlpha(40);
            fill->setBrush(c);
            fill->setPen(pen);
            chart->addSeries(fill);
            fill->attachAxis(x_axis);
            fill->attachAxis(y_axis);
        } else {
            chart->addSeries(line);
            line->attachAxis(x_axis);
            line->attachAxis(y_axis);
        }
    }
}

// ── render_bar ────────────────────────────────────────────────────────────────
void DBnomicsChartWidget::render_bar(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();

    // Last 20 periods, ordered by period START (period strings of mixed frequency do not sort lexically).
    // Observations are ascending by period start, so the newest 20 periods of the chart are among each
    // series' last 20 valid observations: only those are collected (a long daily series used to put
    // every observation through a map and a linear contains()).
    constexpr int kBars = 20;
    QMap<QString, QDate> period_start;
    for (const auto& dp : series) {
        int taken = 0;
        for (auto it = dp.observations.crbegin(); it != dp.observations.crend() && taken < kBars; ++it) {
            if (!it->valid)
                continue;
            ++taken;
            if (!period_start.contains(it->period))
                period_start.insert(it->period, it->start);
        }
    }
    QVector<QString> periods(period_start.keyBegin(), period_start.keyEnd());
    std::sort(periods.begin(), periods.end(), [&period_start](const QString& a, const QString& b) {
        const QDate da = period_start.value(a);
        const QDate db = period_start.value(b);
        if (da.isValid() && db.isValid() && da != db)
            return da < db;
        return a < b;
    });
    if (periods.size() > kBars)
        periods = periods.mid(periods.size() - kBars);
    if (periods.isEmpty())
        return;
    const QSet<QString> shown(periods.cbegin(), periods.cend());
    // The shown periods are the newest: a series is scanned from its end, and the scan stops at the
    // first observation older than all of them (when every shown period has a start day).
    QDate earliest;
    bool all_dated = true;
    for (const QString& p : std::as_const(periods)) {
        const QDate d = period_start.value(p);
        if (!d.isValid())
            all_dated = false;
        else if (!earliest.isValid() || d < earliest)
            earliest = d;
    }
    const auto before_shown = [all_dated, earliest](const services::DbnObservation& o) {
        return all_dated && o.start.isValid() && o.start < earliest;
    };

    // Explicit Y range computed from the *valid* observations only, so the NaN
    // gap markers below can never poison the auto-domain.
    double y_min = 0.0; // bars are measured from zero
    double y_max = 0.0;
    bool any = false;
    for (const auto& dp : series) {
        for (auto it = dp.observations.crbegin(); it != dp.observations.crend(); ++it) {
            const services::DbnObservation& obs = *it;
            if (before_shown(obs))
                break;
            if (!obs.valid || !shown.contains(obs.period))
                continue;
            y_min = any ? std::min(y_min, obs.value) : std::min(0.0, obs.value);
            y_max = any ? std::max(y_max, obs.value) : std::max(0.0, obs.value);
            any = true;
        }
    }
    if (!any) {
        y_min = 0.0;
        y_max = 1.0;
    }
    const DbnYScale sc = dbn_y_scale(y_min, y_max);
    y_min /= sc.divisor;
    y_max /= sc.divisor;
    double bar_margin = (y_max - y_min) * 0.1;
    if (bar_margin == 0.0)
        bar_margin = std::abs(y_max) * 0.1 + 1.0;

    auto* bar_series = new QBarSeries();
    for (const auto& dp : series) {
        auto* bar_set = new QBarSet(dp.series_name);
        bar_set->setColor(dp.color);
        QHash<QString, double> pv; // only the periods on the chart; the latest observation of a period wins
        for (auto it = dp.observations.crbegin(); it != dp.observations.crend(); ++it) {
            if (before_shown(*it))
                break;
            if (it->valid && shown.contains(it->period) && !pv.contains(it->period))
                pv.insert(it->period, it->value);
        }
        QList<qreal> values;
        values.reserve(periods.size());
        // A period this series does not report is a GAP (NaN), not a zero — zero-filling made every
        // missing observation look like a real "0" reading.
        for (const auto& p : periods) {
            const auto v = pv.constFind(p);
            values.append(v != pv.cend() ? v.value() / sc.divisor : std::numeric_limits<double>::quiet_NaN());
        }
        bar_set->append(values); // one call, not one signal per bar
        bar_series->append(bar_set);
    }
    chart->addSeries(bar_series);

    auto* x_axis = new QBarCategoryAxis();
    x_axis->append(QList<QString>(periods.begin(), periods.end()));
    style_axis(x_axis);
    // Rotate labels if many periods to avoid overlap
    if (periods.size() > 10)
        x_axis->setLabelsAngle(-45);
    chart->addAxis(x_axis, Qt::AlignBottom);
    bar_series->attachAxis(x_axis);

    auto* y_axis =
        make_y_axis(y_min - (y_min < 0.0 ? bar_margin : 0.0), y_max + bar_margin, y_tick_count(), sc.format);
    y_axis->setTitleText(sc.title);
    chart->addAxis(y_axis, Qt::AlignLeft);
    bar_series->attachAxis(y_axis);
}

} // namespace fincept::screens
