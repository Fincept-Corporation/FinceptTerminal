#pragma once

#include "ui/charts/OverlayLayer.h"

#include <QColor>
#include <QVector>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;

namespace fincept::ui {

struct HorizontalLevel {
    double price = 0;
    QString label;
    QColor color;
    Qt::PenStyle style = Qt::DashLine;
};

class HorizontalLineLayer : public OverlayLayer {
    Q_OBJECT
public:
    explicit HorizontalLineLayer(const QString& id, const QString& name, QObject* parent = nullptr);

    QString id() const override { return id_; }
    QString display_name() const override { return name_; }
    LayerType type() const override { return LayerType::HorizontalLine; }

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;
    void reposition(QChart* chart) override;

    void set_levels(const QVector<HorizontalLevel>& levels);
    const QVector<HorizontalLevel>& levels() const { return levels_; }

private:
    void rebuild_items();
    void clear_items();

    QString id_;
    QString name_;
    QVector<HorizontalLevel> levels_;

    struct LineItem {
        QGraphicsLineItem* line = nullptr;
        QGraphicsRectItem* tag_bg = nullptr;
        QGraphicsSimpleTextItem* tag_txt = nullptr;
    };
    QVector<LineItem> items_;

    QGraphicsScene* scene_ = nullptr;
    QChart* chart_ = nullptr;
};

} // namespace fincept::ui
