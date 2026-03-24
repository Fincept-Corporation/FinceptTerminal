// src/screens/portfolio/PortfolioSparkline.cpp
#include "screens/portfolio/PortfolioSparkline.h"

#include <QPainter>

#include <algorithm>

namespace fincept::screens {

PortfolioSparkline::PortfolioSparkline(int w, int h, QWidget* parent) : QWidget(parent) {
    setFixedSize(w, h);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void PortfolioSparkline::set_data(const QVector<double>& data) {
    data_ = data;
    dirty_ = true;
    update();
}

void PortfolioSparkline::set_color(const QColor& color) {
    color_ = color;
    dirty_ = true;
    update();
}

void PortfolioSparkline::paintEvent(QPaintEvent*) {
    if (dirty_)
        rebuild_pixmap();
    QPainter p(this);
    p.drawPixmap(0, 0, cached_);
}

void PortfolioSparkline::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    dirty_ = true;
}

void PortfolioSparkline::rebuild_pixmap() {
    cached_ = QPixmap(size());
    cached_.fill(Qt::transparent);

    if (data_.size() < 2) {
        dirty_ = false;
        return;
    }

    double min_val = *std::min_element(data_.begin(), data_.end());
    double max_val = *std::max_element(data_.begin(), data_.end());
    double range = max_val - min_val;
    if (range < 1e-9)
        range = 1.0;

    int w = width();
    int h = height();
    int n = data_.size();
    double x_step = static_cast<double>(w - 2) / (n - 1);

    QPainter painter(&cached_);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(color_, 1.2);
    painter.setPen(pen);

    QVector<QPointF> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) {
        double x = 1.0 + i * x_step;
        double y = h - 1.0 - ((data_[i] - min_val) / range) * (h - 2.0);
        points.append({x, y});
    }

    for (int i = 0; i < points.size() - 1; ++i) {
        painter.drawLine(points[i], points[i + 1]);
    }

    dirty_ = false;
}

} // namespace fincept::screens
