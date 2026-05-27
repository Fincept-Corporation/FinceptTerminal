#pragma once

#include "ui/charts/SeriesLayer.h"

namespace fincept::ui {

class EmaLayer : public SeriesLayer {
    Q_OBJECT
public:
    explicit EmaLayer(int period = 21, const QColor& color = QColor("#d97706"),
                      QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;

    int period() const { return period_; }
    void set_period(int p);

private:
    int period_;
};

} // namespace fincept::ui
