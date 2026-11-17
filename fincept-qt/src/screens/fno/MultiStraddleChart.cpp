#include "screens/fno/MultiStraddleChart.h"

#include "screens/fno/FnoFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
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
    const ColorToken palette[] = {colors::POSITIVE, colors::AMBER, colors::TEXT_PRIMARY, colors::NEGATIVE};
    return QColor(palette[i % 4]());
}

QPen series_pen_at(int i) {
    QPen pen(series_color_at(i));
    pen.setWidth(2);
    return pen;
}

} // namespace

MultiStraddleChart::MultiStraddleChart(QWidget* parent) : QChartView(parent), ccy_(fno_ccy()) {
    setRenderHint(QPainter::Antialiasing, true);
    setMouseTracking(true);

    chart_ = new QChart();
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(tr("Synthetic Premium (intraday)"));
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    setChart(chart_);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("HH:mm");

    axis_y_ = new QValueAxis();
    apply_axis_unit();

    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);

    hover_line_ = new QGraphicsLineItem(chart_);
    hover_line_->setVisible(false);
    hover_line_->setZValue(10);

    tooltip_ = new QLabel(this);
    tooltip_->setWindowFlags(Qt::ToolTip);
    tooltip_->setStyleSheet(QString("QLabel { background:%1; color:%2; border:1px solid %3;"
                                    " font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:600; padding:4px 8px; }")
                                .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED()));
    tooltip_->hide();

    refresh_theme();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    // The axis carries the currency unit; the hover tooltip is built on each move.
    fincept::ui::formatting::on_format_changed(this, [this]() { apply_axis_unit(); });
}

void MultiStraddleChart::apply_axis_unit() {
    // Premium: the unit sits on the axis labels, placed per the user's settings.
    if (axis_y_)
        axis_y_->setLabelFormat(fincept::ui::formatting::axis_printf_format(ccy_, 2));
}

void MultiStraddleChart::set_currency(const QString& ccy) {
    const QString c = ccy.trimmed().isEmpty() ? fno_ccy() : ccy.trimmed();
    if (c == ccy_)
        return;
    ccy_ = c;
    apply_axis_unit();
}

void MultiStraddleChart::refresh_theme() {
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    chart_->setTitleFont(fonts::font(-2, QFont::Bold));
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    chart_->legend()->setFont(fonts::font(-3));

    const QFont label_font = fonts::font(-3);
    for (QAbstractAxis* axis : {static_cast<QAbstractAxis*>(axis_x_), static_cast<QAbstractAxis*>(axis_y_)}) {
        axis->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
        axis->setLabelsFont(label_font);
        axis->setGridLineColor(QColor(colors::BORDER_DIM()));
        axis->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));
    }

    hover_line_->setPen(QPen(QColor(colors::TEXT_TERTIARY()), 1, Qt::DashLine));
    for (int i = 0; i < series_.size(); ++i)
        series_[i]->setPen(series_pen_at(i));
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
        line->setPen(series_pen_at(i));
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
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(now - 3'600'000), QDateTime::fromMSecsSinceEpoch(now));
        axis_y_->setRange(0, 1);
        return;
    }
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(ts_min), QDateTime::fromMSecsSinceEpoch(ts_max));
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
        lines.append(QString("%1: %2").arg(sel.label, fincept::ui::formatting::price_with_symbol(
                                                           sel.points.at(near).premium, fno_price_fmt(ccy_))));
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

void MultiStraddleChart::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QChartView::changeEvent(event);
}

void MultiStraddleChart::retranslateUi() {
    if (chart_)
        chart_->setTitle(tr("Synthetic Premium (intraday)"));
}

} // namespace fincept::screens::fno
