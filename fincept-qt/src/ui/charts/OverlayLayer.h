#pragma once

#include "ui/charts/CandleData.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

class QChart;
class QGraphicsScene;

namespace fincept::ui {

enum class LayerType { Series, HorizontalLine, Region, Annotation };

class OverlayLayer : public QObject {
    Q_OBJECT
public:
    explicit OverlayLayer(QObject* parent = nullptr);
    ~OverlayLayer() override;

    virtual QString id() const = 0;
    virtual QString display_name() const = 0;
    virtual LayerType type() const = 0;

    bool visible() const { return visible_; }
    void set_visible(bool v);

    virtual void compute(const QVector<CandleData>& candles) = 0;
    virtual void attach(QGraphicsScene* scene, QChart* chart) = 0;
    virtual void detach(QGraphicsScene* scene, QChart* chart) = 0;
    virtual void reposition(QChart* chart) = 0;

signals:
    void visibility_changed(bool visible);

private:
    bool visible_ = true;
};

} // namespace fincept::ui
