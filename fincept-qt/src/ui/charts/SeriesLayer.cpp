#include "ui/charts/SeriesLayer.h"

#include <QChart>
#include <QGraphicsScene>
#include <QLineSeries>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

SeriesLayer::SeriesLayer(const QString& id, const QString& name, const QColor& color, int width, QObject* parent)
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
    apply_points(series_, points);
    series_->setVisible(visible());
}

void SeriesLayer::apply_points(QLineSeries* series, const QList<QPointF>& points) {
    if (!series)
        return;
    const QList<QPointF> current = series->points();
    const qsizetype n_old = current.size();
    const qsizetype n_new = points.size();
    // Common prefix, compared exactly: QPointF::operator== is fuzzy and would let
    // a genuinely different value survive.
    qsizetype same = 0;
    const qsizetype limit = std::min(n_old, n_new);
    while (same < limit && current[same].x() == points[same].x() && current[same].y() == points[same].y())
        ++same;

    if (n_new == n_old && same == n_old)
        return; // nothing moved — skip the repaint entirely
    if (n_new == n_old && same == n_old - 1) {
        series->replace(static_cast<int>(same), points[same]); // the forming bar's value moved
        return;
    }
    if (n_new == n_old + 1 && same == n_old) {
        series->append(points[same]); // one new bar
        return;
    }
    series->replace(points);
}

} // namespace fincept::ui
