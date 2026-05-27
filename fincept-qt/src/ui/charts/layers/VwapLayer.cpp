#include "ui/charts/layers/VwapLayer.h"

#include "ui/charts/ChartIndicators.h"

#include <QChart>
#include <QGraphicsScene>
#include <QLineSeries>

#include <cmath>

namespace fincept::ui {

VwapLayer::VwapLayer(bool show_bands, QObject* parent)
    : SeriesLayer("vwap", "VWAP", QColor("#2563eb"), 2, parent),
      show_bands_(show_bands) {}

void VwapLayer::compute(const QVector<CandleData>& candles) {
    QVector<double> highs, lows, closes, volumes;
    QVector<int64_t> timestamps;
    const int n = candles.size();
    highs.reserve(n);
    lows.reserve(n);
    closes.reserve(n);
    volumes.reserve(n);
    timestamps.reserve(n);

    for (const auto& c : candles) {
        highs.append(c.high);
        lows.append(c.low);
        closes.append(c.close);
        volumes.append(c.volume);
        timestamps.append(c.timestamp);
    }

    const auto vwap_vals = indicators::vwap(highs, lows, closes, volumes);
    update_series_data(timestamps, vwap_vals);

    if (show_bands_) {
        const auto dev = indicators::vwap_std_dev(highs, lows, closes, volumes, vwap_vals);
        update_band(upper_1_, timestamps, vwap_vals, dev, 1.0);
        update_band(lower_1_, timestamps, vwap_vals, dev, -1.0);
        update_band(upper_2_, timestamps, vwap_vals, dev, 2.0);
        update_band(lower_2_, timestamps, vwap_vals, dev, -2.0);
    }
}

void VwapLayer::attach(QGraphicsScene* scene, QChart* chart) {
    chart_ref_ = chart;
    SeriesLayer::attach(scene, chart);
    if (show_bands_) {
        attach_band(upper_1_, chart, QColor(37, 99, 235, 60), 1);
        attach_band(lower_1_, chart, QColor(37, 99, 235, 60), 1);
        attach_band(upper_2_, chart, QColor(37, 99, 235, 30), 1);
        attach_band(lower_2_, chart, QColor(37, 99, 235, 30), 1);
    }
}

void VwapLayer::detach(QGraphicsScene* scene, QChart* chart) {
    if (show_bands_) {
        detach_band(upper_1_, chart);
        detach_band(lower_1_, chart);
        detach_band(upper_2_, chart);
        detach_band(lower_2_, chart);
    }
    SeriesLayer::detach(scene, chart);
    chart_ref_ = nullptr;
}

void VwapLayer::attach_band(QLineSeries*& s, QChart* chart, const QColor& color, int width) {
    if (!s)
        s = new QLineSeries();
    QPen pen(color);
    pen.setWidth(width);
    pen.setStyle(Qt::DotLine);
    s->setPen(pen);
    if (!chart->series().contains(s))
        chart->addSeries(s);
    for (auto* axis : chart->axes(Qt::Horizontal))
        s->attachAxis(axis);
    for (auto* axis : chart->axes(Qt::Vertical))
        s->attachAxis(axis);
}

void VwapLayer::detach_band(QLineSeries*& s, QChart* chart) {
    if (s && chart->series().contains(s))
        chart->removeSeries(s);
    delete s;
    s = nullptr;
}

void VwapLayer::update_band(QLineSeries* s, const QVector<int64_t>& ts,
                            const QVector<double>& base, const QVector<double>& dev, double mult) {
    if (!s)
        return;
    QList<QPointF> pts;
    pts.reserve(ts.size());
    for (int i = 0; i < ts.size(); ++i) {
        if (!std::isnan(base[i]) && !std::isnan(dev[i]))
            pts.append(QPointF(static_cast<qreal>(ts[i]), base[i] + mult * dev[i]));
    }
    s->replace(pts);
}

} // namespace fincept::ui
