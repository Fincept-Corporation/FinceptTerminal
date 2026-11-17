#include "screens/alpha_arena/EquityCurveWidget.h"

#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::alpha_arena {

namespace {
// Axis labels, series end labels and the hover readout.
QFont chart_font() {
    return ui::fonts::font(-2);
}
} // namespace

EquityCurveWidget::EquityCurveWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(220);
    setMinimumWidth(160); // plot_rect() inverts below ~76px; keep a sane floor
}

void EquityCurveWidget::set_data(QVector<EquitySeries> series, double baseline) {
    series_ = std::move(series);
    baseline_ = baseline;
    min_y_ = max_y_ = baseline;
    min_x_ = std::numeric_limits<qint64>::max();
    max_x_ = 0;
    for (const auto& s : series_)
        for (const auto& p : s.points) {
            min_y_ = std::min(min_y_, p.y());
            max_y_ = std::max(max_y_, p.y());
            min_x_ = std::min(min_x_, qint64(p.x()));
            max_x_ = std::max(max_x_, qint64(p.x()));
        }
    if (min_x_ > max_x_) {
        min_x_ = 0;
        max_x_ = 1;
    }
    const double pad = std::max(1.0, (max_y_ - min_y_) * 0.08);
    min_y_ -= pad;
    max_y_ += pad;
    update();
}

QRectF EquityCurveWidget::plot_rect() const {
    // Left gutter fits a seven-digit y label at the user's font size (64 px at the default).
    const int left = std::max(64, QFontMetrics(chart_font()).horizontalAdvance(QStringLiteral("0000000")) + 6);
    return QRectF(left, 12, width() - left - 12, height() - 40);
}

void EquityCurveWidget::paintEvent(QPaintEvent*) {
    using namespace ui::colors;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(BG_SURFACE()));
    const QRectF r = plot_rect();
    auto x_of = [&](double ts) {
        return r.left() + (max_x_ == min_x_ ? 0 : (ts - min_x_) / double(max_x_ - min_x_) * r.width());
    };
    auto y_of = [&](double v) { return r.bottom() - (v - min_y_) / (max_y_ - min_y_) * r.height(); };

    // Grid + y labels.
    const QColor grid(BORDER_DIM());
    p.setPen(grid);
    p.setFont(chart_font());
    const QFontMetrics fm(p.font());
    const int label_h = fm.height();
    for (int i = 0; i <= 4; ++i) {
        const double v = min_y_ + (max_y_ - min_y_) * i / 4.0;
        const double y = y_of(v);
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.setPen(QColor(TEXT_TERTIARY()));
        p.drawText(QRectF(0, y - label_h / 2.0, r.left() - 6, label_h), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 0));
        p.setPen(grid);
    }
    // Time axis: five ticks under the plot, their granularity from the span shown
    // (a competition can cover an hour or a week; nothing used to say which).
    bool has_points = false;
    for (const auto& s : series_)
        has_points = has_points || !s.points.isEmpty();
    if (has_points && max_x_ > min_x_) {
        const qint64 span_secs = std::max<qint64>(1, (max_x_ - min_x_) / 1000);
        for (int i = 0; i <= 4; ++i) {
            const qint64 ts = min_x_ + (max_x_ - min_x_) * i / 4;
            const double x = x_of(double(ts));
            p.setPen(grid);
            p.drawLine(QPointF(x, r.bottom()), QPointF(x, r.bottom() + 3));
            const QString lbl = ui::formatting::axis_label(QDateTime::fromMSecsSinceEpoch(ts), span_secs);
            const double w = fm.horizontalAdvance(lbl);
            const double lx = std::max(r.left() - w / 2.0, std::min(x - w / 2.0, r.right() - w));
            p.setPen(QColor(TEXT_TERTIARY()));
            p.drawText(QPointF(lx, r.bottom() + 4 + fm.ascent()), lbl);
        }
    }
    // Baseline.
    p.setPen(QPen(QColor(BORDER_BRIGHT()), 1, Qt::DashLine));
    p.drawLine(QPointF(r.left(), y_of(baseline_)), QPointF(r.right(), y_of(baseline_)));

    // Series — each agent in its provider's brand colour, made legible on the theme.
    // A single settled round is drawn as a dot; until any agent has two points the
    // chart says why it is empty instead of showing a bare grid.
    bool any_line = false;
    for (const auto& s : series_) {
        if (s.points.size() == 1) {
            const QColor c = legible_color(s.color);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(QPointF(x_of(s.points[0].x()), y_of(s.points[0].y())), 3.0, 3.0);
            p.setBrush(Qt::NoBrush);
            continue;
        }
        if (s.points.size() < 2)
            continue;
        any_line = true;
        QPainterPath path(QPointF(x_of(s.points[0].x()), y_of(s.points[0].y())));
        for (int i = 1; i < s.points.size(); ++i)
            path.lineTo(x_of(s.points[i].x()), y_of(s.points[i].y()));
        p.setPen(QPen(legible_color(s.color), 2));
        p.drawPath(path);
        // End label.
        const QPointF last(x_of(s.points.last().x()), y_of(s.points.last().y()));
        p.drawText(QPointF(std::min(last.x() + 4, r.right() - 60), last.y() - 2), s.label);
    }
    if (!any_line) {
        p.setPen(QColor(TEXT_TERTIARY()));
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap,
                   tr("The equity curve starts after the second settled round."));
    }

    // Hover crosshair + values.
    if (hover_.x() >= r.left() && hover_.x() <= r.right() && !series_.isEmpty() && max_x_ > min_x_) {
        p.setPen(QPen(QColor(BORDER_MED()), 1, Qt::DotLine));
        p.drawLine(hover_.x(), int(r.top()), hover_.x(), int(r.bottom()));
        const qint64 ts = min_x_ + qint64((hover_.x() - r.left()) / r.width() * (max_x_ - min_x_));
        QStringList lines{QDateTime::fromMSecsSinceEpoch(ts).toString("MMM d HH:mm")};
        for (const auto& s : series_) {
            double best = baseline_;
            qint64 bd = std::numeric_limits<qint64>::max();
            for (const auto& pt : s.points)
                if (std::llabs(qint64(pt.x()) - ts) < bd) {
                    bd = std::llabs(qint64(pt.x()) - ts);
                    best = pt.y();
                }
            lines << QStringLiteral("%1  $%2").arg(s.label, QString::number(best, 'f', 0));
        }
        int box_w = 165;
        for (const auto& line : lines)
            box_w = std::max(box_w, fm.horizontalAdvance(line) + 12);
        const int line_h = std::max(16, fm.lineSpacing());
        const QRectF box(std::min<double>(hover_.x() + 8, r.right() - box_w - 5), r.top() + 4, box_w,
                         double(line_h) * lines.size() + 8);
        QColor box_bg(BG_RAISED());
        box_bg.setAlpha(235);
        p.fillRect(box, box_bg);
        p.setPen(QColor(TEXT_PRIMARY()));
        for (int i = 0; i < lines.size(); ++i)
            p.drawText(box.adjusted(6, 4 + i * line_h, -4, 0), Qt::AlignLeft | Qt::AlignTop, lines[i]);
    }
}

void EquityCurveWidget::mouseMoveEvent(QMouseEvent* e) {
    hover_ = e->pos();
    update();
}
void EquityCurveWidget::leaveEvent(QEvent*) {
    hover_ = {-1, -1};
    update();
}

} // namespace fincept::screens::alpha_arena
