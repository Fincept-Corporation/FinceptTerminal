#pragma once
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSeries>
#include <QVector>

namespace fincept::ui {

/// Factory for creating Bloomberg-themed Qt Charts.
class ChartFactory {
public:
    struct DataPoint {
        double x;
        double y;
    };

    /// Line chart — used for price history, indices.
    static QChartView* line_chart(const QString& title,
                                  const QVector<DataPoint>& data,
                                  const QString& color = "#FF6600");

    /// Bar chart — used for volume, comparisons.
    static QChartView* bar_chart(const QString& title,
                                 const QStringList& categories,
                                 const QVector<double>& values,
                                 const QString& color = "#FF6600");

    /// Mini sparkline — no axes, compact.
    static QChartView* sparkline(const QVector<double>& data,
                                 const QString& color = "#808080",
                                 int width = 120, int height = 30);

private:
    static void apply_theme(QChart* chart);
};

} // namespace fincept::ui
