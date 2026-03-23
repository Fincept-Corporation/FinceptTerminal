// src/screens/dbnomics/DBnomicsChartWidget.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QLabel>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>
#include <QtCharts/QChartView>

namespace fincept::screens {

/// Chart widget for DBnomics data.
/// Supports Line, Area, Bar and Scatter chart types.
/// Call set_compact(true) when used inside the 2-column comparison grid
/// to use tighter margins and hide the legend.
class DBnomicsChartWidget : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsChartWidget(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series, services::DbnChartType chart_type);
    void clear();
    void set_loading(bool on);

    /// Compact mode: tighter margins, no legend — use in comparison grid slots.
    void set_compact(bool compact);

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void build_ui();
    void render_line_area(const QVector<services::DbnDataPoint>& series, services::DbnChartType type);
    void render_bar(const QVector<services::DbnDataPoint>& series);
    void render_scatter(const QVector<services::DbnDataPoint>& series);

    int y_tick_count() const;
    int x_tick_count(int period_count) const;

    QStackedWidget* stack_ = nullptr;
    QLabel* spin_label_ = nullptr;
    QTimer* spin_timer_ = nullptr;
    QChartView* chart_view_ = nullptr;
    int frame_ = 0;
    bool compact_ = false;
};

} // namespace fincept::screens
