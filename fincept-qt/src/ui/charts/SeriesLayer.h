#pragma once

#include "ui/charts/OverlayLayer.h"

#include <QColor>
#include <QList>
#include <QPen>
#include <QPointF>

class QLineSeries;

namespace fincept::ui {

class SeriesLayer : public OverlayLayer {
    Q_OBJECT
  public:
    explicit SeriesLayer(const QString& id, const QString& name, const QColor& color = QColor("#d97706"), int width = 2,
                         QObject* parent = nullptr);

    QString id() const override { return id_; }
    QString display_name() const override { return name_; }
    LayerType type() const override { return LayerType::Series; }

    void set_color(const QColor& c);
    void set_line_width(int w);

    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;
    void reposition(QChart* chart) override;

  protected:
    void update_series_data(const QVector<int64_t>& timestamps, const QVector<double>& values);
    QLineSeries* series() const { return series_; }

    /// Make `series` hold exactly `points`, touching the chart as little as
    /// possible. Every QXYSeries change re-strokes the whole line path, so a
    /// live tick that only moves the forming bar's value replaces that ONE
    /// point, a new bar appends one point, an unchanged result is skipped, and
    /// anything else falls back to a single full replace(). Points compare
    /// exactly (never fuzzily), so the end state is identical to replace(points).
    /// Subclasses with extra line series (bands) can route them through this too.
    static void apply_points(QLineSeries* series, const QList<QPointF>& points);

  private:
    QString id_;
    QString name_;
    QColor color_;
    int width_;
    QLineSeries* series_ = nullptr;
    QChart* chart_ = nullptr;
};

} // namespace fincept::ui
