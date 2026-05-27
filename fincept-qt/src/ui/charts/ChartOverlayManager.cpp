#include "ui/charts/ChartOverlayManager.h"
#include "ui/charts/HorizontalLineLayer.h"

#include <QChart>
#include <QGraphicsScene>

#include <algorithm>

namespace fincept::ui {

ChartOverlayManager::ChartOverlayManager(QObject* parent) : QObject(parent) {}

ChartOverlayManager::~ChartOverlayManager() {
    if (scene_ && chart_) {
        for (auto* layer : layers_)
            layer->detach(scene_, chart_);
    }
}

void ChartOverlayManager::set_chart(QGraphicsScene* scene, QChart* chart) {
    if (scene_ && chart_) {
        for (auto* layer : layers_)
            layer->detach(scene_, chart_);
    }
    scene_ = scene;
    chart_ = chart;
    if (scene_ && chart_) {
        for (auto* layer : layers_)
            layer->attach(scene_, chart_);
    }
}

void ChartOverlayManager::add_layer(OverlayLayer* layer) {
    if (has_layer(layer->id()))
        return;

    layer->setParent(this);
    layers_.append(layer);

    if (scene_ && chart_)
        layer->attach(scene_, chart_);

    if (!candles_.isEmpty())
        layer->compute(candles_);

    if (scene_ && chart_)
        layer->reposition(chart_);

    emit layer_added(layer->id());
}

void ChartOverlayManager::remove_layer(const QString& id) {
    for (int i = 0; i < layers_.size(); ++i) {
        if (layers_[i]->id() == id) {
            auto* layer = layers_.takeAt(i);
            if (scene_ && chart_)
                layer->detach(scene_, chart_);
            layer->deleteLater();
            emit layer_removed(id);
            return;
        }
    }
}

OverlayLayer* ChartOverlayManager::layer(const QString& id) const {
    for (auto* l : layers_) {
        if (l->id() == id)
            return l;
    }
    return nullptr;
}

bool ChartOverlayManager::has_layer(const QString& id) const {
    return layer(id) != nullptr;
}

void ChartOverlayManager::set_candles(QVector<CandleData> candles) {
    CandleData::sort_by_time(candles);
    candles_ = std::move(candles);
    recompute_all();
}

void ChartOverlayManager::append_candle(const CandleData& candle) {
    if (!candles_.isEmpty() && candles_.last().timestamp == candle.timestamp) {
        candles_.last() = candle;
    } else {
        candles_.append(candle);
    }
    recompute_all();
}

void ChartOverlayManager::recompute_all() {
    for (auto* layer : layers_) {
        layer->compute(candles_);
        if (chart_)
            layer->reposition(chart_);
    }
}

void ChartOverlayManager::reposition_all() {
    if (!chart_)
        return;
    for (auto* layer : layers_) {
        if (layer->visible())
            layer->reposition(chart_);
    }
}

QStringList ChartOverlayManager::active_layer_ids() const {
    QStringList ids;
    for (auto* l : layers_)
        ids.append(l->id());
    return ids;
}

bool ChartOverlayManager::overlay_price_range(double& out_min, double& out_max) const {
    bool found = false;
    out_min = 1e18;
    out_max = 0;
    for (const auto* layer : layers_) {
        if (!layer->visible() || layer->type() != LayerType::HorizontalLine)
            continue;
        const auto* hl = qobject_cast<const HorizontalLineLayer*>(layer);
        if (!hl) continue;
        for (const auto& level : hl->levels()) {
            if (level.price > 0) {
                out_min = std::min(out_min, level.price);
                out_max = std::max(out_max, level.price);
                found = true;
            }
        }
    }
    return found;
}

} // namespace fincept::ui
