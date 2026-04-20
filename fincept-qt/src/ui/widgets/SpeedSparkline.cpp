// src/ui/widgets/SpeedSparkline.cpp
#include "ui/widgets/SpeedSparkline.h"

#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>
#include <algorithm>

namespace fincept::ui {

SpeedSparkline::SpeedSparkline(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(80, 18);
}

void SpeedSparkline::set_line_color(const QColor& c) {
    line_color_ = c;
    update();
}

void SpeedSparkline::set_fill_color(const QColor& c) {
    fill_color_ = c;
    update();
}

void SpeedSparkline::set_capacity(int samples) {
    capacity_ = std::max(4, samples);
    while (samples_.size() > capacity_)
        samples_.pop_front();
    update();
}

void SpeedSparkline::push(qint64 bytes_per_sec) {
    if (bytes_per_sec < 0)
        bytes_per_sec = 0;
    samples_.push_back(bytes_per_sec);
    while (samples_.size() > capacity_)
        samples_.pop_front();
    update();
}

void SpeedSparkline::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const QRectF r = rect().adjusted(1, 1, -1, -1);

    if (samples_.isEmpty() || r.width() < 2 || r.height() < 2)
        return;

    qint64 peak = 0;
    for (qint64 v : samples_)
        peak = std::max(peak, v);
    // Floor so an idle meter still shows a visible baseline instead of a flat
    // spike when the first non-zero sample arrives.
    const double y_max = std::max<double>(peak, 64.0 * 1024.0); // min scale 64 KB/s

    const int n = samples_.size();
    const double x_step = r.width() / static_cast<double>(std::max(1, n - 1));

    QPainterPath line;
    QPainterPath fill;
    fill.moveTo(r.left(), r.bottom());

    for (int i = 0; i < n; ++i) {
        const double x = r.left() + i * x_step;
        const double norm = static_cast<double>(samples_[i]) / y_max;
        const double y = r.bottom() - norm * r.height();
        if (i == 0)
            line.moveTo(x, y);
        else
            line.lineTo(x, y);
        fill.lineTo(x, y);
    }
    fill.lineTo(r.right(), r.bottom());
    fill.closeSubpath();

    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(fill, fill_color_);

    QPen pen(line_color_);
    pen.setWidthF(1.3);
    p.setPen(pen);
    p.drawPath(line);
}

} // namespace fincept::ui
