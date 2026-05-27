#pragma once

#include "ui/charts/HorizontalLineLayer.h"

namespace fincept::ui {

class SupportResistanceLayer : public HorizontalLineLayer {
    Q_OBJECT
public:
    explicit SupportResistanceLayer(QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;

private:
    struct Cluster {
        double price = 0;
        int touches = 0;
        double weight = 0;
    };

    QVector<Cluster> find_clusters(const QVector<CandleData>& candles, double tolerance_pct = 0.3);
};

} // namespace fincept::ui
