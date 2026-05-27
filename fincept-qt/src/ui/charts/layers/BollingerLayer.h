#pragma once

#include "ui/charts/SeriesLayer.h"

class QLineSeries;

namespace fincept::ui {

class BollingerLayer : public SeriesLayer {
    Q_OBJECT
public:
    explicit BollingerLayer(int period = 20, double num_std = 2.0, QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;

    int period() const { return period_; }
    double num_std() const { return num_std_; }

private:
    int period_;
    double num_std_;
    QLineSeries* upper_ = nullptr;
    QLineSeries* lower_ = nullptr;

    void attach_band(QLineSeries*& s, QChart* chart);
    void detach_band(QLineSeries*& s, QChart* chart);
};

} // namespace fincept::ui
