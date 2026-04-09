#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QVector>
#include <QWidget>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>

namespace fincept::ui {

/// Factory for creating themed Qt Charts.
class ChartFactory {
  public:
    struct DataPoint {
        double x;
        double y;
    };

    /// Line chart — used for price history, indices.
    static QChartView* line_chart(const QString& title, const QVector<DataPoint>& data, const QString& color = {});

    /// Bar chart — used for volume, comparisons.
    static QChartView* bar_chart(const QString& title, const QStringList& categories, const QVector<double>& values,
                                 const QString& color = {});

    /// Mini sparkline — no axes, compact.
    static QChartView* sparkline(const QVector<double>& data, const QString& color = {}, int width = 120,
                                 int height = 30);

    /// Apply active theme tokens to an existing chart (call on theme_changed).
    static void apply_theme(QChart* chart);
};

} // namespace fincept::ui
