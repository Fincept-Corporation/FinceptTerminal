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

/// Factory for creating themed Qt Charts. Charts it creates follow later theme
/// and font changes on their own (background, axes, fonts, and series colours
/// that are theme token values); a series colour the caller set from anything
/// else is the caller's to refresh.
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

    /// Apply the active theme tokens and the user's font to a chart's
    /// background, legend and axes — for charts built or changed outside the
    /// factory (call again on theme_changed for those).
    static void apply_theme(QChart* chart);
};

} // namespace fincept::ui
