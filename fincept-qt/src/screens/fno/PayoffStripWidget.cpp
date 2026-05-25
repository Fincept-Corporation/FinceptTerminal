#include "screens/fno/PayoffStripWidget.h"

#include "ui/theme/Theme.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using namespace fincept::ui;

PayoffStripWidget::PayoffStripWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(80);
    setObjectName("fnoPayoffStrip");
}

void PayoffStripWidget::set_payoff(const QVector<fincept::services::options::PayoffPoint>& curve,
                                   double spot, const QVector<double>& breakevens) {
    curve_ = curve;
    spot_ = spot;
    breakevens_ = breakevens;
    dirty_ = true;
    update();
}

void PayoffStripWidget::clear_payoff() {
    curve_.clear();
    breakevens_.clear();
    spot_ = 0;
    dirty_ = true;
    update();
}

void PayoffStripWidget::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    if (dirty_)
        rebuild_pixmap();
    QPainter p(this);
    p.drawPixmap(0, 0, cached_);
}

void PayoffStripWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void PayoffStripWidget::rebuild_pixmap() {
    dirty_ = false;
    const QSize sz = size();
    if (sz.isEmpty()) {
        cached_ = QPixmap();
        return;
    }
    cached_ = QPixmap(sz);
    cached_.fill(QColor(colors::BG_BASE()));

    if (curve_.size() < 2) return;

    QPainter p(&cached_);
    p.setRenderHint(QPainter::Antialiasing, false);

    const double w = sz.width();
    const double h = sz.height();

    double spot_min = curve_.first().spot;
    double spot_max = curve_.last().spot;
    double spot_range = spot_max - spot_min;
    if (spot_range < 1e-6) spot_range = 1.0;

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();
    for (const auto& pt : curve_) {
        if (std::isfinite(pt.pnl_expiry)) {
            y_min = std::min(y_min, pt.pnl_expiry);
            y_max = std::max(y_max, pt.pnl_expiry);
        }
    }
    if (y_min >= y_max) {
        y_min = -1.0;
        y_max = 1.0;
    }
    double y_pad = (y_max - y_min) * 0.05;
    y_min -= y_pad;
    y_max += y_pad;
    double y_range = y_max - y_min;

    auto to_x = [&](double spot) -> double {
        return (spot - spot_min) / spot_range * w;
    };
    auto to_y = [&](double pnl) -> double {
        return h - ((pnl - y_min) / y_range * h);
    };

    double zero_y = to_y(0);

    // Zero baseline
    p.setPen(QPen(QColor(colors::BORDER_DIM()), 1));
    p.drawLine(QPointF(0, zero_y), QPointF(w, zero_y));

    // Profit/loss fill
    QColor profit_color(colors::POSITIVE());
    profit_color.setAlpha(50);
    QColor loss_color(colors::NEGATIVE());
    loss_color.setAlpha(50);

    for (int i = 0; i < curve_.size() - 1; ++i) {
        double x1 = to_x(curve_[i].spot);
        double x2 = to_x(curve_[i + 1].spot);
        double y1 = to_y(curve_[i].pnl_expiry);
        double y2 = to_y(curve_[i + 1].pnl_expiry);

        // Fill above zero (profit)
        if (curve_[i].pnl_expiry > 0 || curve_[i + 1].pnl_expiry > 0) {
            QPolygonF poly;
            poly << QPointF(x1, std::min(y1, zero_y))
                 << QPointF(x2, std::min(y2, zero_y))
                 << QPointF(x2, zero_y)
                 << QPointF(x1, zero_y);
            p.setPen(Qt::NoPen);
            p.setBrush(profit_color);
            p.drawPolygon(poly);
        }
        // Fill below zero (loss)
        if (curve_[i].pnl_expiry < 0 || curve_[i + 1].pnl_expiry < 0) {
            QPolygonF poly;
            poly << QPointF(x1, std::max(y1, zero_y))
                 << QPointF(x2, std::max(y2, zero_y))
                 << QPointF(x2, zero_y)
                 << QPointF(x1, zero_y);
            p.setPen(Qt::NoPen);
            p.setBrush(loss_color);
            p.drawPolygon(poly);
        }
    }

    // PnL expiry curve
    p.setPen(QPen(QColor(colors::TEXT_PRIMARY()), 1));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < curve_.size() - 1; ++i) {
        double x1 = to_x(curve_[i].spot);
        double x2 = to_x(curve_[i + 1].spot);
        double y1 = to_y(curve_[i].pnl_expiry);
        double y2 = to_y(curve_[i + 1].pnl_expiry);
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Spot marker (amber vertical)
    if (spot_ > spot_min && spot_ < spot_max) {
        double sx = to_x(spot_);
        p.setPen(QPen(QColor(colors::AMBER()), 1));
        p.drawLine(QPointF(sx, 0), QPointF(sx, h));
        QFont f = p.font();
        f.setPixelSize(9);
        p.setFont(f);
        p.drawText(QPointF(sx + 2, 10), QString::number(spot_, 'f', 0));
    }

    // Breakeven markers (dotted) — show at most 4
    QPen be_pen(QColor(colors::TEXT_SECONDARY()), 1, Qt::DotLine);
    QFont small_font = p.font();
    small_font.setPixelSize(9);
    p.setFont(small_font);
    int be_drawn = 0;
    for (double be : breakevens_) {
        if (be_drawn >= 4) break;
        if (be < spot_min || be > spot_max)
            continue;
        double bx = to_x(be);
        p.setPen(be_pen);
        p.drawLine(QPointF(bx, 0), QPointF(bx, h));
        p.setPen(QPen(QColor(colors::TEXT_SECONDARY())));
        p.drawText(QPointF(bx + 2, h - 4), QString::number(be, 'f', 0));
        ++be_drawn;
    }

    // X-axis scale labels (5 evenly spaced)
    p.setPen(QPen(QColor(colors::TEXT_DIM())));
    for (int i = 0; i <= 4; ++i) {
        double spot_val = spot_min + spot_range * i / 4.0;
        double lx = to_x(spot_val);
        p.drawText(QRectF(lx - 25, h - 12, 50, 12), Qt::AlignCenter,
                   QString::number(spot_val, 'f', 0));
    }
}

} // namespace fincept::screens::fno
