#include "screens/alpha_arena/EquityCurveWidget.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::alpha_arena {

EquityCurveWidget::EquityCurveWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(220);
    setMinimumWidth(160);   // plot_rect() inverts below ~76px; keep a sane floor
}

void EquityCurveWidget::set_data(QVector<EquitySeries> series, double baseline) {
    series_ = std::move(series);
    baseline_ = baseline;
    min_y_ = max_y_ = baseline;
    min_x_ = std::numeric_limits<qint64>::max(); max_x_ = 0;
    for (const auto& s : series_)
        for (const auto& p : s.points) {
            min_y_ = std::min(min_y_, p.y()); max_y_ = std::max(max_y_, p.y());
            min_x_ = std::min(min_x_, qint64(p.x())); max_x_ = std::max(max_x_, qint64(p.x()));
        }
    if (min_x_ > max_x_) { min_x_ = 0; max_x_ = 1; }
    const double pad = std::max(1.0, (max_y_ - min_y_) * 0.08);
    min_y_ -= pad; max_y_ += pad;
    update();
}

QRectF EquityCurveWidget::plot_rect() const { return QRectF(64, 12, width() - 76, height() - 40); }

void EquityCurveWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#0D0D0D"));
    const QRectF r = plot_rect();
    auto x_of = [&](double ts) { return r.left() + (max_x_ == min_x_ ? 0 : (ts - min_x_) / double(max_x_ - min_x_) * r.width()); };
    auto y_of = [&](double v)  { return r.bottom() - (v - min_y_) / (max_y_ - min_y_) * r.height(); };

    // Grid + y labels.
    p.setPen(QColor("#222"));
    p.setFont(QFont(p.font().family(), 9));
    for (int i = 0; i <= 4; ++i) {
        const double v = min_y_ + (max_y_ - min_y_) * i / 4.0;
        const double y = y_of(v);
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.setPen(QColor("#777"));
        p.drawText(QRectF(0, y - 8, 58, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 0));
        p.setPen(QColor("#222"));
    }
    // Baseline.
    p.setPen(QPen(QColor("#555"), 1, Qt::DashLine));
    p.drawLine(QPointF(r.left(), y_of(baseline_)), QPointF(r.right(), y_of(baseline_)));

    // Series.
    for (const auto& s : series_) {
        if (s.points.size() < 2) continue;
        QPainterPath path(QPointF(x_of(s.points[0].x()), y_of(s.points[0].y())));
        for (int i = 1; i < s.points.size(); ++i)
            path.lineTo(x_of(s.points[i].x()), y_of(s.points[i].y()));
        p.setPen(QPen(s.color, 2));
        p.drawPath(path);
        // End label.
        const QPointF last(x_of(s.points.last().x()), y_of(s.points.last().y()));
        p.drawText(QPointF(std::min(last.x() + 4, r.right() - 60), last.y() - 2), s.label);
    }

    // Hover crosshair + values.
    if (hover_.x() >= r.left() && hover_.x() <= r.right() && !series_.isEmpty() && max_x_ > min_x_) {
        p.setPen(QPen(QColor("#444"), 1, Qt::DotLine));
        p.drawLine(hover_.x(), int(r.top()), hover_.x(), int(r.bottom()));
        const qint64 ts = min_x_ + qint64((hover_.x() - r.left()) / r.width() * (max_x_ - min_x_));
        QStringList lines{QDateTime::fromMSecsSinceEpoch(ts).toString("MMM d HH:mm")};
        for (const auto& s : series_) {
            double best = baseline_; qint64 bd = std::numeric_limits<qint64>::max();
            for (const auto& pt : s.points)
                if (std::llabs(qint64(pt.x()) - ts) < bd) { bd = std::llabs(qint64(pt.x()) - ts); best = pt.y(); }
            lines << QStringLiteral("%1  $%2").arg(s.label, QString::number(best, 'f', 0));
        }
        const QRectF box(std::min<double>(hover_.x() + 8, r.right() - 170), r.top() + 4, 165, 16.0 * lines.size() + 8);
        p.fillRect(box, QColor(20, 20, 20, 230));
        p.setPen(QColor("#DDD"));
        for (int i = 0; i < lines.size(); ++i)
            p.drawText(box.adjusted(6, 4 + i * 16, -4, 0), Qt::AlignLeft | Qt::AlignTop, lines[i]);
    }
}

void EquityCurveWidget::mouseMoveEvent(QMouseEvent* e) { hover_ = e->pos(); update(); }
void EquityCurveWidget::leaveEvent(QEvent*) { hover_ = {-1, -1}; update(); }

} // namespace fincept::screens::alpha_arena
