#pragma once

#include "ui/charts/ChartIndicators.h"
#include "ui/charts/HorizontalLineLayer.h"

namespace fincept::ui {

class PivotLayer : public HorizontalLineLayer {
    Q_OBJECT
public:
    explicit PivotLayer(PivotType type = PivotType::Standard, QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;

    PivotType pivot_type() const { return type_; }
    void set_pivot_type(PivotType t);

private:
    PivotType type_;
};

} // namespace fincept::ui
