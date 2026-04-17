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
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QResizeEvent>
#include <QScatterSeries>
#include <QSet>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace fincept::screens {

// ── helpers ──────────────────────────────────────────────────────────────────

static QValueAxis* make_y_axis(double y_min, double y_max, int tick_count) {
    auto* ax = new QValueAxis();
    ax->setRange(y_min, y_max);
    ax->setTickCount(tick_count);
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ax->setLinePen(QPen(QColor(ui::colors::BORDER_MED())));
    ax->setLabelFormat("%.4g"); // compact notation — avoids long decimals in small charts
    ax->setMinorTickCount(0);
    return ax;
}

static QValueAxis* make_x_axis(int count) {
    auto* ax = new QValueAxis();
    ax->setRange(0, std::max(1, count - 1));
    ax->setTickCount(std::min(8, std::max(2, count)));
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ax->setLinePen(QPen(QColor(ui::colors::BORDER_MED())));
    ax->setLabelFormat("%d");
    ax->setMinorTickCount(0);
    return ax;
}

static void style_chart(QChart* chart, bool compact) {
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    // Explicit margins so axis labels are never clipped.
    // Left margin is wider to accommodate Y-axis label text.
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
    chart->setTitle("NO DATA — SELECT A SERIES FROM THE LEFT PANEL");
    chart_view_->setChart(chart);
    chart_view_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    stack_->addWidget(chart_view_); // index 1

    stack_->setCurrentIndex(1);
    root->addWidget(stack_, 1);

    // ── Animation timer ───────────────────────────────────────────────────────
    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(120);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString frames[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
        spin_label_->setText(QString("%1  FETCHING DATA...").arg(frames[frame_ % 8]));
        ++frame_;
    });
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
        spin_label_->setText("⣾  FETCHING DATA...");
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
    for (auto* axis : axes)
        chart->removeAxis(axis);
    chart->setTitle("NO DATA — SELECT A SERIES FROM THE LEFT PANEL");
}

void DBnomicsChartWidget::set_data(const QVector<services::DbnDataPoint>& series, services::DbnChartType chart_type) {
    clear();
    chart_view_->chart()->setTitle({});
    if (series.isEmpty())
        return;

    if (chart_type == services::DbnChartType::Bar) {
        render_bar(series);
    } else if (chart_type == services::DbnChartType::Scatter) {
        render_scatter(series);
    } else {
        render_line_area(series, chart_type);
    }

    // Re-apply style after rendering (compact flag may differ per instance)
    style_chart(chart_view_->chart(), compact_);

    LOG_DEBUG("DBnomicsChartWidget", QString("Rendered %1 series").arg(series.size()));
}

// ── Adaptive tick count based on available pixel height ──────────────────────
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
    int max_from_width = std::max(2, w / 80);
    return std::min(max_from_width, std::min(12, period_count));
}

// ── resizeEvent: update tick counts so axes stay legible ─────────────────────
void DBnomicsChartWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    QChart* chart = chart_view_->chart();
    if (!chart)
        return;

    // Update Y axis tick count
    const auto y_axes = chart->axes(Qt::Vertical);
    for (auto* ax : y_axes) {
        if (auto* vax = qobject_cast<QValueAxis*>(ax))
            vax->setTickCount(y_tick_count());
    }

    // Update X axis tick count
    const auto x_axes = chart->axes(Qt::Horizontal);
    for (auto* ax : x_axes) {
        if (auto* vax = qobject_cast<QValueAxis*>(ax)) {
            int range = static_cast<int>(vax->max() - vax->min()) + 1;
            vax->setTickCount(x_tick_count(range));
        }
    }
}

// ── render_line_area ─────────────────────────────────────────────────────────
void DBnomicsChartWidget::render_line_area(const QVector<services::DbnDataPoint>& series, services::DbnChartType type) {
    QChart* chart = chart_view_->chart();
    double y_min = std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();

    // Collect all valid periods in ascending order
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid)
                period_set.insert(obs.period);
    QVector<QString> all_periods(period_set.begin(), period_set.end());
    std::sort(all_periods.begin(), all_periods.end());

    if (all_periods.isEmpty())
        return;

    // ── Create axes first, add to chart, then attach series ──────────────────
    auto* x_axis = make_x_axis(all_periods.size());
    x_axis->setTickCount(x_tick_count(all_periods.size()));
    chart->addAxis(x_axis, Qt::AlignBottom);

    // Scan all data to determine y range before creating y_axis
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            if (obs.valid) {
                y_min = std::min(y_min, obs.value);
                y_max = std::max(y_max, obs.value);
            }
        }
    }
    if (y_min > y_max) {
        y_min = 0.0;
        y_max = 1.0;
    }
    double margin = (y_max - y_min) * 0.1;
    if (margin == 0.0)
        margin = std::abs(y_min) * 0.1 + 1.0;

    auto* y_axis = make_y_axis(y_min - margin, y_max + margin, y_tick_count());
    chart->addAxis(y_axis, Qt::AlignLeft);

    // ── Build and attach each series ─────────────────────────────────────────
    for (const auto& dp : series) {
        QMap<QString, double> period_val;
        for (const auto& obs : dp.observations)
            if (obs.valid)
                period_val[obs.period] = obs.value;

        auto* line = new QLineSeries();
        line->setName(dp.series_name);
        line->setColor(dp.color);
        QPen pen(dp.color);
        pen.setWidth(2);
        line->setPen(pen);

        for (int i = 0; i < all_periods.size(); ++i) {
            if (period_val.contains(all_periods[i]))
                line->append(i, period_val[all_periods[i]]);
        }

        chart->addSeries(line);
        line->attachAxis(x_axis);
        line->attachAxis(y_axis);

        if (type == services::DbnChartType::Area) {
            auto* zero_line = new QLineSeries();
            for (int i = 0; i < all_periods.size(); ++i)
                zero_line->append(i, 0.0);
            auto* area = new QAreaSeries(line, zero_line);
            QColor fill = dp.color;
            fill.setAlpha(40);
            area->setBrush(fill);
            area->setPen(Qt::NoPen);
            chart->addSeries(area);
            area->attachAxis(x_axis);
            area->attachAxis(y_axis);
        }
    }
}

// ── render_bar ────────────────────────────────────────────────────────────────
void DBnomicsChartWidget::render_bar(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();

    // Use last 20 periods for readability
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid)
                period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());
    if (periods.size() > 20)
        periods = periods.mid(periods.size() - 20);

    if (periods.isEmpty())
        return;

    auto* bar_series = new QBarSeries();
    for (const auto& dp : series) {
        auto* bar_set = new QBarSet(dp.series_name);
        bar_set->setColor(dp.color);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid)
                pv[obs.period] = obs.value;
        for (const auto& p : periods)
            *bar_set << (pv.contains(p) ? pv[p] : 0.0);
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

    auto* y_axis = new QValueAxis();
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLabelFormat("%.4g");
    y_axis->setTickCount(y_tick_count());
    y_axis->setMinorTickCount(0);
    chart->addAxis(y_axis, Qt::AlignLeft);
    bar_series->attachAxis(y_axis);
}

// ── render_scatter ────────────────────────────────────────────────────────────
void DBnomicsChartWidget::render_scatter(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();
    double y_min = std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();

    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid)
                period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());
    if (periods.isEmpty())
        return;

    // ── Create axes first ─────────────────────────────────────────────────────
    auto* x_axis = make_x_axis(periods.size());
    x_axis->setTickCount(x_tick_count(periods.size()));
    chart->addAxis(x_axis, Qt::AlignBottom);

    // Scan y range
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            if (obs.valid) {
                y_min = std::min(y_min, obs.value);
                y_max = std::max(y_max, obs.value);
            }
        }
    }
    if (y_min > y_max) {
        y_min = 0.0;
        y_max = 1.0;
    }
    double margin = (y_max - y_min) * 0.1;
    if (margin == 0.0)
        margin = std::abs(y_min) * 0.1 + 1.0;

    auto* y_axis = make_y_axis(y_min - margin, y_max + margin, y_tick_count());
    chart->addAxis(y_axis, Qt::AlignLeft);

    // ── Build and attach scatter series ───────────────────────────────────────
    for (const auto& dp : series) {
        auto* scatter = new QScatterSeries();
        scatter->setName(dp.series_name);
        scatter->setColor(dp.color);
        scatter->setMarkerSize(compact_ ? 4.0 : 6.0);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid)
                pv[obs.period] = obs.value;
        for (int i = 0; i < periods.size(); ++i) {
            if (pv.contains(periods[i]))
                scatter->append(i, pv[periods[i]]);
        }
        chart->addSeries(scatter);
        scatter->attachAxis(x_axis);
        scatter->attachAxis(y_axis);
    }
}

} // namespace fincept::screens
