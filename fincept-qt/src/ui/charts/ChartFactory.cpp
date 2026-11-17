#include "ui/charts/ChartFactory.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QPointer>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

#include <memory>

namespace fincept::ui {

namespace {

/// Keeps a chart made by the factory in step with later theme / font changes:
/// background, axes and fonts from the active tokens (unless `transparent`),
/// and every series and title colour that was a token value (the defaults
/// below, or a token a caller passed) translated to the same token of the new
/// theme. Colours that are not token values are left as the caller set them.
void follow_theme(QChartView* view, QChart* chart, bool transparent) {
    auto recolor = std::make_shared<ThemeRecolor>();
    QPointer<QChart> guard(chart);
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, view,
                     [recolor, guard, transparent](const ThemeTokens&) {
                         recolor->advance();
                         QChart* c = guard.data();
                         if (!c)
                             return;
                         if (!transparent)
                             ChartFactory::apply_theme(c);
                         c->setTitleBrush(recolor->map(c->titleBrush()));
                         c->setTitleFont(fonts::font(-2, QFont::DemiBold));
                         for (QAbstractSeries* s : c->series()) {
                             if (auto* line = qobject_cast<QLineSeries*>(s)) {
                                 QPen pen = line->pen();
                                 pen.setColor(recolor->map(pen.color()));
                                 line->setPen(pen);
                             } else if (auto* bars = qobject_cast<QBarSeries*>(s)) {
                                 for (QBarSet* set : bars->barSets()) {
                                     set->setColor(recolor->map(set->color()));
                                     set->setBorderColor(recolor->map(set->borderColor()));
                                 }
                             }
                         }
                     });
}

} // namespace

void ChartFactory::apply_theme(QChart* chart) {
    const auto& t = ThemeManager::instance().tokens();
    chart->setBackgroundBrush(QBrush(QColor(t.bg_surface)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(t.bg_surface)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setVisible(false);
    chart->legend()->setLabelColor(QColor(t.text_secondary));
    chart->legend()->setFont(fonts::font(-3));
    chart->setMargins(QMargins(0, 0, 0, 0));

    for (auto* axis : chart->axes()) {
        axis->setLabelsColor(QColor(t.text_secondary));
        axis->setLabelsFont(fonts::font(-3));
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
    chart->setTitleFont(fonts::font(-2, QFont::DemiBold));
    chart->createDefaultAxes();
    apply_theme(chart);

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent; border: none;");
    follow_theme(view, chart, false);
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
    chart->setTitleFont(fonts::font(-2, QFont::DemiBold));

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
    follow_theme(view, chart, false);
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
    follow_theme(view, chart, true);
    return view;
}

} // namespace fincept::ui
