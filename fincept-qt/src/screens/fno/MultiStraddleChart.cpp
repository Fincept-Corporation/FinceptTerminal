#include "screens/fno/MultiStraddleChart.h"

#include "ui/theme/Theme.h"

#include <QChart>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QGuiApplication>
#include <QLineSeries>
#include <QMouseEvent>
#include <QPen>
#include <QScreen>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using namespace fincept::ui;

namespace {

QColor series_color_at(int i) {
    static const QStringList kPalette = {
        colors::POSITIVE(),
        colors::AMBER(),
        colors::TEXT_PRIMARY(),
        colors::NEGATIVE(),
    };
    return QColor(kPalette.at(i % kPalette.size()));
}

}  // namespace

MultiStraddleChart::MultiStraddleChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);
    setMouseTracking(true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(QStringLiteral("Synthetic Premium (intraday)"));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    QFont title_font = chart_->titleFont();
    title_font.setPointSize(9);
    title_font.setBold(true);
    chart_->setTitleFont(title_font);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    setChart(chart_);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("HH:mm");
    axis_x_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_x_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_x_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("%.2f");
    axis_y_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_y_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_y_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);

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

void MultiStraddleChart::set_straddles(const QVector<Selection>& selections) {
    // Drop existing series — Qt deletes them when removed from the chart.
    for (auto* s : series_)
        chart_->removeSeries(s);
    qDeleteAll(series_);
    series_.clear();
    data_ = selections;

    qint64 ts_min = std::numeric_limits<qint64>::max();
    qint64 ts_max = 0;
    double y_max = 0;

    for (int i = 0; i < selections.size(); ++i) {
        auto* line = new QLineSeries();
        line->setName(selections[i].label);
        QPen pen(series_color_at(i));
        pen.setWidth(2);
        line->setPen(pen);
        for (const auto& p : selections[i].points) {
            const qint64 ms = p.ts_secs * 1000;
            line->append(double(ms), p.premium);
            ts_min = std::min(ts_min, ms);
            ts_max = std::max(ts_max, ms);
            y_max = std::max(y_max, p.premium);
        }
        chart_->addSeries(line);
        line->attachAxis(axis_x_);
        line->attachAxis(axis_y_);
        series_.append(line);
    }

    if (selections.isEmpty() || ts_min >= ts_max) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(now - 3'600'000),
                          QDateTime::fromMSecsSinceEpoch(now));
        axis_y_->setRange(0, 1);
        return;
    }
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(ts_min),
                      QDateTime::fromMSecsSinceEpoch(ts_max));
    axis_y_->setRange(0, y_max > 0 ? y_max * 1.1 : 1);
}

void MultiStraddleChart::mouseMoveEvent(QMouseEvent* e) {
    QChartView::mouseMoveEvent(e);
    update_crosshair(e->pos());
}

void MultiStraddleChart::leaveEvent(QEvent* e) {
    QChartView::leaveEvent(e);
    hide_crosshair();
}

void MultiStraddleChart::update_crosshair(const QPoint& widget_pos) {
    if (data_.isEmpty() || data_.first().points.isEmpty()) {
        hide_crosshair();
        return;
    }
    const QPointF chart_val = chart_->mapToValue(QPointF(widget_pos));
    const qint64 target_ms = qint64(chart_val.x());

    // Snap to first series' nearest point — they share the time grid.
    const auto& base = data_.first().points;
    int best = 0;
    qint64 best_dist = std::numeric_limits<qint64>::max();
    for (int i = 0; i < base.size(); ++i) {
        const qint64 d = std::abs(qint64(base[i].ts_secs * 1000) - target_ms);
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    const qint64 snap_ms = base.at(best).ts_secs * 1000;

    const QPointF scene_pos = chart_->mapToPosition(QPointF(double(snap_ms), 0));
    const QRectF plot = chart_->plotArea();
    hover_line_->setLine(scene_pos.x(), plot.top(), scene_pos.x(), plot.bottom());
    hover_line_->setVisible(true);

    QStringList lines;
    lines.append(QDateTime::fromMSecsSinceEpoch(snap_ms).toString("dd MMM, HH:mm"));
    for (const auto& sel : data_) {
        // Find the closest point in this selection to snap_ms.
        if (sel.points.isEmpty())
            continue;
        int near = 0;
        qint64 nd = std::numeric_limits<qint64>::max();
        for (int i = 0; i < sel.points.size(); ++i) {
            const qint64 d = std::abs(qint64(sel.points[i].ts_secs * 1000) - snap_ms);
            if (d < nd) {
                nd = d;
                near = i;
            }
        }
        lines.append(QString("%1: ₹ %2")
                          .arg(sel.label)
                          .arg(sel.points.at(near).premium, 0, 'f', 2));
    }
    tooltip_->setText(lines.join("\n"));
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

void MultiStraddleChart::hide_crosshair() {
    hover_line_->setVisible(false);
    if (tooltip_)
        tooltip_->hide();
}

} // namespace fincept::screens::fno
