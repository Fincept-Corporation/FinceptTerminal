// src/screens/dbnomics/DBnomicsChartWidget.cpp
#include "screens/dbnomics/DBnomicsChartWidget.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QAreaSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QScatterSeries>
#include <QValueAxis>
#include <QBarCategoryAxis>
#include <QLegend>

#include <QVBoxLayout>
#include <QSet>
#include <algorithm>
#include <limits>

namespace fincept::screens {

DBnomicsChartWidget::DBnomicsChartWidget(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsChartWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    chart_view_ = new QChartView(this);
    chart_view_->setRenderHint(QPainter::Antialiasing, false);

    auto* chart = new QChart();
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY));
    chart->setTitleFont(QFont("Consolas", 11, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER)));
    chart_view_->setChart(chart);
    chart_view_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));

    root->addWidget(chart_view_);
}

void DBnomicsChartWidget::clear() {
    QChart* chart = chart_view_->chart();
    chart->removeAllSeries();
    const auto axes = chart->axes();
    for (auto* axis : axes)
        chart->removeAxis(axis);
    chart->setTitle("NO DATA — SELECT A SERIES FROM THE LEFT PANEL");
}

void DBnomicsChartWidget::set_data(const QVector<services::DbnDataPoint>& series,
                                    services::DbnChartType chart_type) {
    clear();
    chart_view_->chart()->setTitle({});
    if (series.isEmpty()) return;

    if (chart_type == services::DbnChartType::Bar) {
        render_bar(series);
    } else if (chart_type == services::DbnChartType::Scatter) {
        render_scatter(series);
    } else {
        render_line_area(series, chart_type);
    }
    LOG_DEBUG("DBnomicsChartWidget", QString("Rendered %1 series").arg(series.size()));
}

void DBnomicsChartWidget::render_line_area(const QVector<services::DbnDataPoint>& series,
                                            services::DbnChartType type) {
    QChart* chart = chart_view_->chart();
    double y_min =  std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();

    // Collect all valid periods in ascending order
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    QVector<QString> all_periods(period_set.begin(), period_set.end());
    std::sort(all_periods.begin(), all_periods.end());

    if (all_periods.isEmpty()) return;

    auto* x_axis = new QValueAxis();
    x_axis->setRange(0, all_periods.size() - 1);
    const int max_labels = 12;
    x_axis->setTickCount(std::min(max_labels, (int)all_periods.size()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_MED)));
    chart->addAxis(x_axis, Qt::AlignBottom);

    QVector<QAbstractSeries*> added_series;

    for (const auto& dp : series) {
        QMap<QString, double> period_val;
        for (const auto& obs : dp.observations)
            if (obs.valid) period_val[obs.period] = obs.value;

        auto* line = new QLineSeries();
        line->setName(dp.series_name);
        line->setColor(dp.color);
        QPen pen(dp.color);
        pen.setWidth(2);
        line->setPen(pen);

        for (int i = 0; i < all_periods.size(); ++i) {
            if (period_val.contains(all_periods[i])) {
                double v = period_val[all_periods[i]];
                line->append(i, v);
                y_min = std::min(y_min, v);
                y_max = std::max(y_max, v);
            }
        }
        chart->addSeries(line);
        line->attachAxis(x_axis);
        added_series.push_back(line);

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
        }
    }

    // Y axis
    if (y_min > y_max) { y_min = 0.0; y_max = 1.0; } // guard for no-data
    double margin = (y_max - y_min) * 0.1;
    if (margin == 0.0) margin = 1.0; // flat line guard
    auto* y_axis = new QValueAxis();
    y_axis->setRange(y_min - margin, y_max + margin);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_MED)));
    y_axis->setTickCount(6);
    chart->addAxis(y_axis, Qt::AlignLeft);

    for (auto* s : chart->series())
        s->attachAxis(y_axis);
}

void DBnomicsChartWidget::render_bar(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();

    // Use last 20 periods for readability
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());
    if (periods.size() > 20) periods = periods.mid(periods.size() - 20);

    if (periods.isEmpty()) return;

    auto* bar_series = new QBarSeries();
    for (const auto& dp : series) {
        auto* bar_set = new QBarSet(dp.series_name);
        bar_set->setColor(dp.color);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid) pv[obs.period] = obs.value;
        for (const auto& p : periods)
            *bar_set << (pv.contains(p) ? pv[p] : 0.0);
        bar_series->append(bar_set);
    }
    chart->addSeries(bar_series);

    auto* x_axis = new QBarCategoryAxis();
    x_axis->append(QList<QString>(periods.begin(), periods.end()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(x_axis, Qt::AlignBottom);
    bar_series->attachAxis(x_axis);

    auto* y_axis = new QValueAxis();
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(y_axis, Qt::AlignLeft);
    bar_series->attachAxis(y_axis);
}

void DBnomicsChartWidget::render_scatter(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();
    double y_min =  std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();

    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());
    if (periods.isEmpty()) return;

    for (const auto& dp : series) {
        auto* scatter = new QScatterSeries();
        scatter->setName(dp.series_name);
        scatter->setColor(dp.color);
        scatter->setMarkerSize(6.0);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid) pv[obs.period] = obs.value;
        for (int i = 0; i < periods.size(); ++i) {
            if (pv.contains(periods[i])) {
                double v = pv[periods[i]];
                scatter->append(i, v);
                y_min = std::min(y_min, v);
                y_max = std::max(y_max, v);
            }
        }
        chart->addSeries(scatter);
    }

    auto* x_axis = new QValueAxis();
    x_axis->setRange(0, periods.size() - 1);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(x_axis, Qt::AlignBottom);

    if (y_min > y_max) { y_min = 0.0; y_max = 1.0; }
    double margin = (y_max - y_min) * 0.1;
    if (margin == 0.0) margin = 1.0;
    auto* y_axis = new QValueAxis();
    y_axis->setRange(y_min - margin, y_max + margin);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(y_axis, Qt::AlignLeft);

    for (auto* s : chart->series()) {
        s->attachAxis(x_axis);
        s->attachAxis(y_axis);
    }
}

} // namespace fincept::screens
