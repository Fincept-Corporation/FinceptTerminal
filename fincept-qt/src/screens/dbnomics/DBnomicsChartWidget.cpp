// src/screens/dbnomics/DBnomicsChartWidget.cpp
#include "screens/dbnomics/DBnomicsChartWidget.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QMap>
#include <QResizeEvent>
#include <QScatterSeries>
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

static QValueAxis* make_y_axis(double y_min, double y_max, int tick_count, const QString& format) {
    auto* ax = new QValueAxis();
    ax->setRange(y_min, y_max);
    ax->setTickCount(tick_count);
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ax->setLinePen(QPen(QColor(ui::colors::BORDER_MED())));
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

static qint64 dbn_msecs(const QDate& d) {
    return QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();
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
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ax->setLinePen(QPen(QColor(ui::colors::BORDER_MED())));
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
    }
    chart->setTitleFont(QFont("Consolas", compact ? 9 : 11, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER())));
}

// ── DBnomicsChartWidget ───────────────────────────────────────────────────────

DBnomicsChartWidget::DBnomicsChartWidget(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsChartWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);

    // ── Page 0: loading spinner ───────────────────────────────────────────────
    auto* loading_page = new QWidget(stack_);
    loading_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* loading_vl = new QVBoxLayout(loading_page);
    spin_label_ = new QLabel(loading_page);
    spin_label_->setAlignment(Qt::AlignCenter);
    spin_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:16px; background:transparent;")
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::fonts::DATA_FAMILY));
    loading_vl->addWidget(spin_label_);
    stack_->addWidget(loading_page); // index 0

    // ── Page 1: chart view ────────────────────────────────────────────────────
    chart_view_ = new QChartView(stack_);
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    chart_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* chart = new QChart();
    style_chart(chart, false);
    chart->setTitle(tr("NO DATA — SELECT A SERIES FROM THE LEFT PANEL"));
    chart_view_->setChart(chart);
    chart_view_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    stack_->addWidget(chart_view_); // index 1

    stack_->setCurrentIndex(1);
    root->addWidget(stack_, 1);

    // ── Animation timer (runs only while loading) ─────────────────────────────
    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(120);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString frames[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
        spin_label_->setText(tr("%1  FETCHING DATA...").arg(frames[frame_ % 8]));
        ++frame_;
    });
}

// ── Live language switch ─────────────────────────────────────────────────────

void DBnomicsChartWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DBnomicsChartWidget::retranslateUi() {
    // Only the empty-state title is translatable chrome; rendered series titles
    // carry data names and are rebuilt by set_data().
    if (showing_placeholder_ && chart_view_ && chart_view_->chart())
        chart_view_->chart()->setTitle(tr("NO DATA — SELECT A SERIES FROM THE LEFT PANEL"));
}

void DBnomicsChartWidget::set_compact(bool compact) {
    compact_ = compact;
    // Re-style the existing chart if already built
    if (chart_view_ && chart_view_->chart())
        style_chart(chart_view_->chart(), compact_);
}

void DBnomicsChartWidget::set_loading(bool on) {
    if (on) {
        frame_ = 0;
        spin_label_->setText(tr("%1  FETCHING DATA...").arg(QStringLiteral("⣾")));
        stack_->setCurrentIndex(0);
        spin_timer_->start();
    } else {
        spin_timer_->stop();
        stack_->setCurrentIndex(1);
    }
}

void DBnomicsChartWidget::clear() {
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
    chart->setTitle(tr("NO DATA — SELECT A SERIES FROM THE LEFT PANEL"));
    showing_placeholder_ = true;
}

void DBnomicsChartWidget::set_data(const QVector<services::DbnDataPoint>& series, services::DbnChartType chart_type) {
    clear();
    chart_view_->chart()->setTitle({});
    // Past this point the chart shows either real data or a blank title — never
    // the translatable placeholder — so retranslateUi must not re-apply it.
    showing_placeholder_ = false;
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

// ── Line / area / scatter on a time axis ─────────────────────────────────────
// Each series' points are handed over in ONE replace() call — per-point append() emitted a signal and
// scheduled a repaint for every observation.
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
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    chart->addAxis(y_axis, Qt::AlignLeft);

    for (const auto& dp : series) {
        QList<QPointF> points;
        points.reserve(dp.observations.size());
        for (const auto& obs : dp.observations)
            if (obs.valid && obs.start.isValid())
                points.append(QPointF(static_cast<qreal>(dbn_msecs(obs.start)), obs.value / sc.divisor));

        if (scatter) {
            auto* dots = new QScatterSeries();
            dots->setName(dp.series_name);
            dots->setColor(dp.color);
            dots->setMarkerSize(compact_ ? 4.0 : 6.0);
            dots->replace(points);
            chart->addSeries(dots);
            dots->attachAxis(x_axis);
            dots->attachAxis(y_axis);
            continue;
        }

        auto* line = new QLineSeries();
        line->setName(dp.series_name);
        line->setColor(dp.color);
        QPen pen(dp.color);
        pen.setWidth(2);
        line->setPen(pen);
        line->replace(points);

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
    QMap<QString, QDate> period_start;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid && !period_start.contains(obs.period))
                period_start.insert(obs.period, obs.start);
    QVector<QString> periods(period_start.keyBegin(), period_start.keyEnd());
    std::sort(periods.begin(), periods.end(), [&period_start](const QString& a, const QString& b) {
        const QDate da = period_start.value(a);
        const QDate db = period_start.value(b);
        if (da.isValid() && db.isValid() && da != db)
            return da < db;
        return a < b;
    });
    if (periods.size() > 20)
        periods = periods.mid(periods.size() - 20);
    if (periods.isEmpty())
        return;

    // Explicit Y range computed from the *valid* observations only, so the NaN
    // gap markers below can never poison the auto-domain.
    double y_min = 0.0; // bars are measured from zero
    double y_max = 0.0;
    bool any = false;
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            if (!obs.valid || !periods.contains(obs.period))
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
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid)
                pv[obs.period] = obs.value;
        QList<qreal> values;
        values.reserve(periods.size());
        // A period this series does not report is a GAP (NaN), not a zero — zero-filling made every
        // missing observation look like a real "0" reading.
        for (const auto& p : periods)
            values.append(pv.contains(p) ? pv[p] / sc.divisor : std::numeric_limits<double>::quiet_NaN());
        bar_set->append(values); // one call, not one signal per bar
        bar_series->append(bar_set);
    }
    chart->addSeries(bar_series);

    auto* x_axis = new QBarCategoryAxis();
    x_axis->append(QList<QString>(periods.begin(), periods.end()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    // Rotate labels if many periods to avoid overlap
    if (periods.size() > 10)
        x_axis->setLabelsAngle(-45);
    chart->addAxis(x_axis, Qt::AlignBottom);
    bar_series->attachAxis(x_axis);

    auto* y_axis =
        make_y_axis(y_min - (y_min < 0.0 ? bar_margin : 0.0), y_max + bar_margin, y_tick_count(), sc.format);
    y_axis->setTitleText(sc.title);
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    chart->addAxis(y_axis, Qt::AlignLeft);
    bar_series->attachAxis(y_axis);
}

} // namespace fincept::screens
