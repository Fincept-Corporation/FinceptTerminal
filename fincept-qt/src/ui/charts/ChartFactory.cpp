#include "ui/charts/ChartFactory.h"

#include "ui/theme/Theme.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

namespace fincept::ui {

void ChartFactory::apply_theme(QChart* chart) {
    chart->setBackgroundBrush(QBrush(QColor(colors::DARK)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(colors::DARK)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));

    for (auto* axis : chart->axes()) {
        axis->setLabelsColor(QColor(colors::MUTED));
        axis->setGridLineColor(QColor(colors::BORDER));
        axis->setLinePenColor(QColor(colors::BORDER));
    }
}

QChartView* ChartFactory::line_chart(const QString& title, const QVector<DataPoint>& data, const QString& color) {
    auto* series = new QLineSeries;
    series->setPen(QPen(QColor(color), 1.5));
    for (const auto& p : data) {
        series->append(p.x, p.y);
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setTitleBrush(QBrush(QColor(colors::GRAY)));
    chart->createDefaultAxes();
    apply_theme(chart);

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent; border: none;");
    return view;
}

QChartView* ChartFactory::bar_chart(const QString& title, const QStringList& categories, const QVector<double>& values,
                                    const QString& color) {
    auto* set = new QBarSet("");
    set->setColor(QColor(color));
    for (double v : values) {
        *set << v;
    }

    auto* series = new QBarSeries;
    series->append(set);

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setTitleBrush(QBrush(QColor(colors::GRAY)));

    auto* axisX = new QBarCategoryAxis;
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    apply_theme(chart);

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent; border: none;");
    return view;
}

QChartView* ChartFactory::sparkline(const QVector<double>& data, const QString& color, int width, int height) {
    auto* series = new QLineSeries;
    series->setPen(QPen(QColor(color), 1.0));
    for (int i = 0; i < data.size(); ++i) {
        series->append(i, data[i]);
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->setBackgroundBrush(Qt::transparent);
    chart->setPlotAreaBackgroundVisible(false);

    for (auto* axis : chart->axes()) {
        axis->setVisible(false);
    }

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setFixedSize(width, height);
    view->setStyleSheet("background: transparent; border: none;");
    return view;
}

} // namespace fincept::ui
