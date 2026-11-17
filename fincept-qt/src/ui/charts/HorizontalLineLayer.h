#pragma once

#include "ui/charts/OverlayLayer.h"

#include <QColor>
#include <QVector>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;

namespace fincept::ui {

/// Theme colour role of a level. Anything but Custom is resolved from the
/// active theme when the level is drawn, and follows theme / direction-palette
/// changes; Custom draws `color` as given.
enum class HorizontalLevelTone { Custom, Neutral, Muted, Accent, Up, Down };

struct HorizontalLevel {
    double price = 0;
    QString label;
    QColor color; // used when tone == Custom
    Qt::PenStyle style = Qt::DashLine;
    HorizontalLevelTone tone = HorizontalLevelTone::Custom;
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

    /// The colour `level` is drawn with under the active theme.
    static QColor level_color(const HorizontalLevel& level);

  private:
    void rebuild_items();
    void clear_items();
    void on_theme_changed();

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
