#include "screens/fno/PayoffChartWidget.h"

#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QGuiApplication>
#include <QLineSeries>
#include <QMouseEvent>
#include <QPen>
#include <QResizeEvent>
#include <QScreen>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::PayoffPoint;
using namespace fincept::ui;

namespace {

constexpr int kProfitAreaAlpha = 38;
constexpr int kLossAreaAlpha = 38;

QColor with_alpha(const QString& hex, int alpha) {
    QColor c(hex);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

PayoffChartWidget::PayoffChartWidget(QWidget* parent) : QChartView(parent) {
    setMouseTracking(true);
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setContentsMargins(0, 0, 0, 0);
    chart_->legend()->setVisible(false);
    setChart(chart_);

    // Series — created once, data swapped via replace().
    expiry_series_ = new QLineSeries();
    target_series_ = new QLineSeries();
    profit_curve_ = new QLineSeries();
    loss_curve_ = new QLineSeries();
    zero_baseline_ = new QLineSeries();

    // Solid expiry curve in the foreground. Brace-init avoids the vexing-
    // parse: `QPen p(QColor(f()))` parses as a function declaration when
    // the inner expression is itself a parenthesised "type-or-value".
    QPen expiry_pen{QColor(colors::TEXT_PRIMARY())};
    expiry_pen.setWidth(2);
    expiry_series_->setPen(expiry_pen);

    // Dashed target-day curve.
    QPen target_pen{QColor(colors::AMBER())};
    target_pen.setWidth(2);
    target_pen.setStyle(Qt::DashLine);
    target_series_->setPen(target_pen);

    // The profit/loss bounding curves are invisible; they exist only to
    // anchor the QAreaSeries against the zero baseline.
    profit_curve_->setPen(Qt::NoPen);
    loss_curve_->setPen(Qt::NoPen);
    zero_baseline_->setPen(Qt::NoPen);

    profit_area_ = new QAreaSeries(profit_curve_, zero_baseline_);
    profit_area_->setPen(Qt::NoPen);
    profit_area_->setBrush(with_alpha(colors::POSITIVE(), kProfitAreaAlpha));

    loss_area_ = new QAreaSeries(loss_curve_, zero_baseline_);
    loss_area_->setPen(Qt::NoPen);
    loss_area_->setBrush(with_alpha(colors::NEGATIVE(), kLossAreaAlpha));

    chart_->addSeries(profit_area_);
    chart_->addSeries(loss_area_);
    chart_->addSeries(target_series_);
    chart_->addSeries(expiry_series_);

    axis_x_ = new QValueAxis();
    axis_y_ = new QValueAxis();
    axis_x_->setLabelFormat("%.0f");
    axis_y_->setLabelFormat("%.0f");
    axis_x_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_y_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_x_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_y_->setGridLineColor(QColor(colors::BORDER_DIM()));
    QPen axis_pen(QColor(colors::BORDER_DIM()), 1);
    axis_x_->setLinePen(axis_pen);
    axis_y_->setLinePen(axis_pen);
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    for (auto* s :
         {static_cast<QAbstractSeries*>(profit_area_), static_cast<QAbstractSeries*>(loss_area_),
          static_cast<QAbstractSeries*>(target_series_), static_cast<QAbstractSeries*>(expiry_series_)}) {
        s->attachAxis(axis_x_);
        s->attachAxis(axis_y_);
    }

    // Vertical markers — graphics-scene line items so they live above the
    // series but inside the chart's plot area.
    QPen spot_pen(QColor(colors::AMBER()), 1, Qt::SolidLine);
    spot_marker_ = new QGraphicsLineItem(chart_);
    spot_marker_->setPen(spot_pen);
    spot_marker_->setVisible(false);
    spot_marker_->setZValue(8);

    QPen hover_pen(QColor(colors::TEXT_TERTIARY()), 1, Qt::DashLine);
    hover_line_ = new QGraphicsLineItem(chart_);
    hover_line_->setPen(hover_pen);
    hover_line_->setVisible(false);
    hover_line_->setZValue(10);

    tooltip_ = new QLabel(this);
    tooltip_->setWindowFlags(Qt::ToolTip);
    tooltip_->setStyleSheet(QString("QLabel { background:%1; color:%2; border:1px solid %3;"
                                    " font-size:11px; font-weight:600; padding:4px 8px; }")
                                .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED()));
    tooltip_->hide();
}

void PayoffChartWidget::set_payoff(const QVector<PayoffPoint>& curve, double current_spot,
                                   const QVector<double>& breakevens) {
    curve_ = curve;
    current_spot_ = current_spot;
    breakevens_ = breakevens;

    expiry_series_->clear();
    target_series_->clear();
    profit_curve_->clear();
    loss_curve_->clear();
    zero_baseline_->clear();

    if (curve.isEmpty()) {
        spot_marker_->setVisible(false);
        for (auto* m : breakeven_markers_)
            delete m;
        breakeven_markers_.clear();
        return;
    }

    double y_min = std::numeric_limits<double>::infinity();
    double y_max = -std::numeric_limits<double>::infinity();
    for (const auto& p : curve) {
        expiry_series_->append(p.spot, p.pnl_expiry);
        target_series_->append(p.spot, p.pnl_target);
        profit_curve_->append(p.spot, std::max(0.0, p.pnl_expiry));
        loss_curve_->append(p.spot, std::min(0.0, p.pnl_expiry));
        y_min = std::min({y_min, p.pnl_expiry, p.pnl_target});
        y_max = std::max({y_max, p.pnl_expiry, p.pnl_target});
    }
    const double x_min = curve.first().spot;
    const double x_max = curve.last().spot;
    zero_baseline_->append(x_min, 0);
    zero_baseline_->append(x_max, 0);

    // Pad y-range by 8% so the curve isn't clipped at the edges.
    const double y_pad = std::max((y_max - y_min) * 0.08, 1.0);
    axis_x_->setRange(x_min, x_max);
    axis_y_->setRange(y_min - y_pad, y_max + y_pad);

    rebuild_spot_marker(current_spot, y_min - y_pad, y_max + y_pad);
    rebuild_breakeven_markers(breakevens, y_min - y_pad, y_max + y_pad);
}

void PayoffChartWidget::set_target_curve_visible(bool on) {
    target_series_->setVisible(on);
}

void PayoffChartWidget::clear_payoff() {
    set_payoff({}, 0, {});
}

void PayoffChartWidget::rebuild_spot_marker(double current_spot, double y_min, double y_max) {
    if (current_spot <= 0 || curve_.isEmpty()) {
        spot_marker_->setVisible(false);
        return;
    }
    const QPointF top = chart_->mapToPosition(QPointF(current_spot, y_max));
    const QPointF bot = chart_->mapToPosition(QPointF(current_spot, y_min));
    spot_marker_->setLine(top.x(), top.y(), bot.x(), bot.y());
    spot_marker_->setVisible(true);
}

void PayoffChartWidget::rebuild_breakeven_markers(const QVector<double>& breakevens, double y_min,
                                                  double y_max) {
    for (auto* m : breakeven_markers_)
        delete m;
    breakeven_markers_.clear();
    QPen be_pen(QColor(colors::TEXT_SECONDARY()), 1, Qt::DotLine);
    for (double be : breakevens) {
        auto* item = new QGraphicsLineItem(chart_);
        item->setPen(be_pen);
        item->setZValue(7);
        const QPointF top = chart_->mapToPosition(QPointF(be, y_max));
        const QPointF bot = chart_->mapToPosition(QPointF(be, y_min));
        item->setLine(top.x(), top.y(), bot.x(), bot.y());
        item->setVisible(true);
        breakeven_markers_.append(item);
    }
}

void PayoffChartWidget::resizeEvent(QResizeEvent* event) {
    QChartView::resizeEvent(event);
    if (curve_.isEmpty())
        return;
    // Refresh marker geometry — chart->mapToPosition resolves against the
    // new plot area after resize.
    const auto y_min = axis_y_->min();
    const auto y_max = axis_y_->max();
    rebuild_spot_marker(current_spot_, y_min, y_max);
    rebuild_breakeven_markers(breakevens_, y_min, y_max);
}

void PayoffChartWidget::mouseMoveEvent(QMouseEvent* event) {
    QChartView::mouseMoveEvent(event);
    update_crosshair(event->pos());
}

void PayoffChartWidget::leaveEvent(QEvent* event) {
    QChartView::leaveEvent(event);
    hide_crosshair();
}

void PayoffChartWidget::update_crosshair(const QPoint& widget_pos) {
    if (curve_.isEmpty()) {
        hide_crosshair();
        return;
    }
    const QPointF chart_val = chart_->mapToValue(QPointF(widget_pos));
    if (chart_val.x() < curve_.first().spot || chart_val.x() > curve_.last().spot) {
        hide_crosshair();
        return;
    }

    int best = 0;
    double best_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < curve_.size(); ++i) {
        const double d = std::abs(curve_[i].spot - chart_val.x());
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    const PayoffPoint& p = curve_.at(best);
    const QPointF scene_pos = chart_->mapToPosition(QPointF(p.spot, 0));
    const QRectF plot = chart_->plotArea();
    hover_line_->setLine(scene_pos.x(), plot.top(), scene_pos.x(), plot.bottom());
    hover_line_->setVisible(true);

    QString text = QString("Spot: %1\nExpiry P/L: %2\nTarget P/L: %3")
                       .arg(p.spot, 0, 'f', 2)
                       .arg(p.pnl_expiry, 0, 'f', 2)
                       .arg(p.pnl_target, 0, 'f', 2);
    tooltip_->setText(text);
    tooltip_->adjustSize();

    QPoint global_pos = mapToGlobal(widget_pos) + QPoint(14, -tooltip_->height() - 6);
    const QRect scr = QGuiApplication::primaryScreen()->geometry();
    if (global_pos.x() + tooltip_->width() > scr.right())
        global_pos.rx() -= tooltip_->width() + 28;
    if (global_pos.y() < scr.top())
        global_pos.ry() = mapToGlobal(widget_pos).y() + 14;
    tooltip_->move(global_pos);
    tooltip_->show();
    tooltip_->raise();
}

void PayoffChartWidget::hide_crosshair() {
    hover_line_->setVisible(false);
    if (tooltip_)
        tooltip_->hide();
}

} // namespace fincept::screens::fno
