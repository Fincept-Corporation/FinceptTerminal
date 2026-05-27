#include "ui/charts/layers/BollingerLayer.h"

#include "ui/charts/ChartIndicators.h"

#include <QChart>
#include <QLineSeries>

#include <cmath>

namespace fincept::ui {

BollingerLayer::BollingerLayer(int period, double num_std, QObject* parent)
    : SeriesLayer(QStringLiteral("bb_%1_%2").arg(period).arg(num_std, 0, 'f', 1),
                  QStringLiteral("BB (%1, %2)").arg(period).arg(num_std, 0, 'f', 1),
                  QColor("#ca8a04"), 1, parent),
      period_(period), num_std_(num_std) {}

void BollingerLayer::compute(const QVector<CandleData>& candles) {
    QVector<double> closes;
    QVector<int64_t> timestamps;
    closes.reserve(candles.size());
    timestamps.reserve(candles.size());

    for (const auto& c : candles) {
        closes.append(c.close);
        timestamps.append(c.timestamp);
    }

    const auto bb = indicators::bollinger(closes, period_, num_std_);
    update_series_data(timestamps, bb.middle);

    auto update_band_data = [&](QLineSeries* s, const QVector<double>& vals) {
        if (!s)
            return;
        QList<QPointF> pts;
        pts.reserve(timestamps.size());
        for (int i = 0; i < timestamps.size(); ++i) {
            if (!std::isnan(vals[i]))
                pts.append(QPointF(static_cast<qreal>(timestamps[i]), vals[i]));
        }
        s->replace(pts);
    };

    update_band_data(upper_, bb.upper);
    update_band_data(lower_, bb.lower);
}

void BollingerLayer::attach(QGraphicsScene* scene, QChart* chart) {
    SeriesLayer::attach(scene, chart);
    attach_band(upper_, chart);
    attach_band(lower_, chart);
}

void BollingerLayer::detach(QGraphicsScene* scene, QChart* chart) {
    detach_band(upper_, chart);
    detach_band(lower_, chart);
    SeriesLayer::detach(scene, chart);
}

void BollingerLayer::attach_band(QLineSeries*& s, QChart* chart) {
    if (!s)
        s = new QLineSeries();
    QPen pen(QColor("#ca8a04"));
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    s->setPen(pen);
    if (!chart->series().contains(s))
        chart->addSeries(s);
    for (auto* axis : chart->axes(Qt::Horizontal))
        s->attachAxis(axis);
    for (auto* axis : chart->axes(Qt::Vertical))
        s->attachAxis(axis);
}

void BollingerLayer::detach_band(QLineSeries*& s, QChart* chart) {
    if (s && chart->series().contains(s))
        chart->removeSeries(s);
    delete s;
    s = nullptr;
}

} // namespace fincept::ui
