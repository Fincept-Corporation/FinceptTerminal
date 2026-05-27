#pragma once

#include "ui/charts/SeriesLayer.h"

class QLineSeries;

namespace fincept::ui {

class VwapLayer : public SeriesLayer {
    Q_OBJECT
public:
    explicit VwapLayer(bool show_bands = true, QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;

private:
    bool show_bands_;
    QLineSeries* upper_1_ = nullptr;
    QLineSeries* lower_1_ = nullptr;
    QLineSeries* upper_2_ = nullptr;
    QLineSeries* lower_2_ = nullptr;
    QChart* chart_ref_ = nullptr;

    void attach_band(QLineSeries*& s, QChart* chart, const QColor& color, int width);
    void detach_band(QLineSeries*& s, QChart* chart);
    void update_band(QLineSeries* s, const QVector<int64_t>& ts,
                     const QVector<double>& base, const QVector<double>& dev, double mult);
};

} // namespace fincept::ui
