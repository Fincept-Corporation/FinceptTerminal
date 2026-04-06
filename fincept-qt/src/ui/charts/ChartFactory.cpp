#include "ui/charts/ChartFactory.h"

#include "ui/theme/ThemeManager.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

namespace fincept::ui {

void ChartFactory::apply_theme(QChart* chart) {
    const auto& t = ThemeManager::instance().tokens();
    chart->setBackgroundBrush(QBrush(QColor(t.bg_surface)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(t.bg_surface)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));

    for (auto* axis : chart->axes()) {
        axis->setLabelsColor(QColor(t.text_secondary));
        axis->setGridLineColor(QColor(t.border_dim));
        axis->setLinePenColor(QColor(t.border_med));
    }
}

QChartView* ChartFactory::line_chart(const QString& title, const QVector<DataPoint>& data, const QString& color) {
    const auto& t = ThemeManager::instance().tokens();
    const QString line_color = color.isEmpty() ? QString(t.accent) : color;
    auto* series = new QLineSeries;
    series->setPen(QPen(QColor(line_color), 1.5));
    for (const auto& p : data) {
        series->append(p.x, p.y);
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setTitleBrush(QBrush(QColor(t.text_secondary)));
    chart->createDefaultAxes();
    apply_theme(chart);

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent; border: none;");
    return view;
}

QChartView* ChartFactory::bar_chart(const QString& title, const QStringList& categories, const QVector<double>& values,
                                    const QString& color) {
    const auto& t = ThemeManager::instance().tokens();
    const QString bar_color = color.isEmpty() ? QString(t.accent) : color;
    auto* set = new QBarSet("");
    set->setColor(QColor(bar_color));
    for (double v : values) {
        *set << v;
    }

    auto* series = new QBarSeries;
    series->append(set);

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setTitleBrush(QBrush(QColor(t.text_secondary)));

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
    const auto& t = ThemeManager::instance().tokens();
    const QString spark_color = color.isEmpty() ? QString(t.text_secondary) : color;
    auto* series = new QLineSeries;
    series->setPen(QPen(QColor(spark_color), 1.0));
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
