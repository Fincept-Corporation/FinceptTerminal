#pragma once

#include "ui/charts/CandleData.h"
#include "ui/charts/OverlayLayer.h"

#include <QObject>
#include <QVector>

class QChart;
class QGraphicsScene;

namespace fincept::ui {

class ChartOverlayManager : public QObject {
    Q_OBJECT
public:
    explicit ChartOverlayManager(QObject* parent = nullptr);
    ~ChartOverlayManager() override;

    void set_chart(QGraphicsScene* scene, QChart* chart);

    void add_layer(OverlayLayer* layer);
    void remove_layer(const QString& id);
    OverlayLayer* layer(const QString& id) const;
    QVector<OverlayLayer*> layers() const { return layers_; }
    bool has_layer(const QString& id) const;

    void set_candles(QVector<CandleData> candles);
    void append_candle(const CandleData& candle);

    void recompute_all();
    void reposition_all();

    QStringList active_layer_ids() const;

    // Returns the min/max price across all HorizontalLineLayer levels.
    // Charts should expand their Y axis to include these values.
    bool overlay_price_range(double& out_min, double& out_max) const;

signals:
    void layer_added(const QString& id);
    void layer_removed(const QString& id);

private:
    QVector<OverlayLayer*> layers_;
    QVector<CandleData> candles_;
    QGraphicsScene* scene_ = nullptr;
    QChart* chart_ = nullptr;
};

} // namespace fincept::ui
