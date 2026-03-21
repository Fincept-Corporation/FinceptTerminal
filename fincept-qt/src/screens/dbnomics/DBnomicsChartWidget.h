// src/screens/dbnomics/DBnomicsChartWidget.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>
#include <QtCharts/QChartView>

namespace fincept::screens {

class DBnomicsChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsChartWidget(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series,
                  services::DbnChartType chart_type);
    void clear();
    void set_loading(bool on);

private:
    void build_ui();
    void render_line_area(const QVector<services::DbnDataPoint>& series,
                          services::DbnChartType type);
    void render_bar(const QVector<services::DbnDataPoint>& series);
    void render_scatter(const QVector<services::DbnDataPoint>& series);

    QStackedWidget* stack_      = nullptr;
    QLabel*         spin_label_ = nullptr;
    QTimer*         spin_timer_ = nullptr;
    QChartView*     chart_view_ = nullptr;
    int             frame_      = 0;
};

} // namespace fincept::screens
