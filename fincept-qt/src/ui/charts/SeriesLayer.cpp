#include "ui/charts/SeriesLayer.h"

#include <QChart>
#include <QGraphicsScene>
#include <QLineSeries>

#include <cmath>

namespace fincept::ui {

SeriesLayer::SeriesLayer(const QString& id, const QString& name,
                         const QColor& color, int width, QObject* parent)
    : OverlayLayer(parent), id_(id), name_(name), color_(color), width_(width) {}

void SeriesLayer::set_color(const QColor& c) {
    color_ = c;
    if (series_) {
        QPen pen = series_->pen();
        pen.setColor(c);
        series_->setPen(pen);
    }
}

void SeriesLayer::set_line_width(int w) {
    width_ = w;
    if (series_) {
        QPen pen = series_->pen();
        pen.setWidth(w);
        series_->setPen(pen);
    }
}

void SeriesLayer::attach(QGraphicsScene*, QChart* chart) {
    chart_ = chart;
    if (!series_) {
        series_ = new QLineSeries();
        QPen pen(color_);
        pen.setWidth(width_);
        series_->setPen(pen);
    }
    if (!chart_->series().contains(series_))
        chart_->addSeries(series_);

    for (auto* axis : chart_->axes(Qt::Horizontal))
        series_->attachAxis(axis);
    for (auto* axis : chart_->axes(Qt::Vertical))
        series_->attachAxis(axis);

    series_->setVisible(visible());
}

void SeriesLayer::detach(QGraphicsScene*, QChart* chart) {
    if (series_ && chart->series().contains(series_))
        chart->removeSeries(series_);
    delete series_;
    series_ = nullptr;
    chart_ = nullptr;
}

void SeriesLayer::reposition(QChart*) {
    // QLineSeries auto-repositions with chart axes — no manual work needed.
}

void SeriesLayer::update_series_data(const QVector<int64_t>& timestamps, const QVector<double>& values) {
    if (!series_)
        return;

    QList<QPointF> points;
    points.reserve(timestamps.size());
    for (int i = 0; i < timestamps.size() && i < values.size(); ++i) {
        if (!std::isnan(values[i]))
            points.append(QPointF(static_cast<qreal>(timestamps[i]), values[i]));
    }
    series_->replace(points);
    series_->setVisible(visible());
}

} // namespace fincept::ui
