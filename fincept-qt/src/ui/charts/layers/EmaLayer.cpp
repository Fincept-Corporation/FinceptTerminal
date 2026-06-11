#include "ui/charts/layers/EmaLayer.h"

#include "ui/charts/ChartIndicators.h"

namespace fincept::ui {

EmaLayer::EmaLayer(int period, const QColor& color, QObject* parent)
    : SeriesLayer(QStringLiteral("ema_%1").arg(period),
                  QStringLiteral("EMA (%1)").arg(period),
                  color, 2, parent),
      period_(period) {}

void EmaLayer::compute(const QVector<CandleData>& candles) {
    QVector<double> closes;
    QVector<int64_t> timestamps;
    closes.reserve(candles.size());
    timestamps.reserve(candles.size());

    for (const auto& c : candles) {
        closes.append(c.close);
        timestamps.append(c.timestamp);
    }

    const auto values = indicators::ema(closes, period_);
    update_series_data(timestamps, values);
}

void EmaLayer::set_period(int p) {
    period_ = p;
}

} // namespace fincept::ui
