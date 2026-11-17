#include "ui/charts/HeatmapGrid.h"

#include "ui/theme/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {
constexpr qreal kHeatmapGridGap = 2.0;
} // namespace

HeatmapGrid::HeatmapGrid(QWidget* parent) : PaintedChart(parent) {
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void HeatmapGrid::set_items(const QVector<TreemapItem>& items) {
    items_ = items;
    hover_ = -1;
    updateGeometry();
    invalidate();
}

void HeatmapGrid::clear() {
    set_items({});
}

void HeatmapGrid::set_color_clamp(double clamp) {
    clamp_ = clamp > 0 ? clamp : 3.0;
    invalidate();
}

void HeatmapGrid::set_columns(int columns) {
    columns_ = qMax(0, columns);
    updateGeometry();
    invalidate();
}

void HeatmapGrid::set_min_cell_size(const QSize& size) {
    min_cell_ = size.expandedTo(QSize(24, 18));
    updateGeometry();
    invalidate();
}

void HeatmapGrid::set_show_legend(bool show) {
    show_legend_ = show;
    updateGeometry();
    invalidate();
}

void HeatmapGrid::set_tooltip_provider(std::function<QString(const TreemapItem&)> provider) {
    tooltip_provider_ = std::move(provider);
}

QString HeatmapGrid::default_format(double v) const {
    return chart_style::percent_text(v, 2);
}

int HeatmapGrid::columns_for(qreal width) const {
    const int n = qMax(1, static_cast<int>(items_.size()));
    int cols = columns_ > 0 ? columns_
                            : static_cast<int>(std::floor((width + kHeatmapGridGap) / (min_cell_.width() + kHeatmapGridGap)));
    return qBound(1, cols, n);
}

qreal HeatmapGrid::grid_height(qreal width) const {
    if (items_.isEmpty())
        return min_cell_.height();
    const int cols = columns_for(width);
    const int rows = static_cast<int>((items_.size() + cols - 1) / cols);
    qreal h = rows * min_cell_.height() + (rows - 1) * kHeatmapGridGap;
    if (show_legend_)
        h += chart_style::legend_height() + 6;
    return h;
}

int HeatmapGrid::heightForWidth(int width) const {
    const QMargins m = contentsMargins();
    return qCeil(grid_height(width - m.left() - m.right())) + m.top() + m.bottom();
}

QSize HeatmapGrid::sizeHint() const {
    const int w = 4 * min_cell_.width() + 3 * static_cast<int>(kHeatmapGridGap);
    return {w, heightForWidth(w)};
}

QSize HeatmapGrid::minimumSizeHint() const {
    return {min_cell_.width(), min_cell_.height()};
}

QRectF HeatmapGrid::cell_rect(int index, const QRectF& r) const {
    const int n = static_cast<int>(items_.size());
    if (index < 0 || index >= n)
        return {};
    QRectF area = r;
    if (show_legend_)
        area.setBottom(area.bottom() - chart_style::legend_height() - 6);
    const int cols = columns_for(area.width());
    const int rows = (n + cols - 1) / cols;
    const qreal cw = (area.width() - kHeatmapGridGap * (cols - 1)) / cols;
    qreal ch = (area.height() - kHeatmapGridGap * (rows - 1)) / rows;
    ch = qBound<qreal>(min_cell_.height(), ch, qMax<qreal>(min_cell_.height(), cw * 0.6));
    const int row = index / cols;
    const int col = index % cols;
    return QRectF(area.left() + col * (cw + kHeatmapGridGap), area.top() + row * (ch + kHeatmapGridGap), cw, ch);
}

int HeatmapGrid::cell_at(const QPointF& pos) const {
    const QRectF r = content_rect();
    for (int i = 0; i < items_.size(); ++i) {
        if (cell_rect(i, r).contains(pos))
            return i;
    }
    return -1;
}

void HeatmapGrid::paint_chart(QPainter& p, const QRectF& r) {
    QRectF last;
    for (int i = 0; i < items_.size(); ++i) {
        const TreemapItem& it = items_.at(i);
        const QRectF cell = cell_rect(i, r);
        last = cell;
        const QColor fill =
            std::isnan(it.value) ? QColor(colors::CHART_NEUTRAL()) : colors::diverging_color(it.value / clamp_);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(cell, 2, 2);
        const qreal side = std::min(cell.width(), cell.height());
        const int max_offset = side > 90 ? 2 : (side > 56 ? 0 : -2);
        QStringList lines{it.label, std::isnan(it.value) ? QString() : format_value(it.value)};
        if (!it.sublabel.isEmpty())
            lines.append(it.sublabel);
        chart_style::draw_cell_text(p, cell, lines, colors::contrast_text_on(fill), max_offset, -6);
    }
    if (show_legend_ && !items_.isEmpty()) {
        const qreal lh = chart_style::legend_height();
        const qreal lw = std::min<qreal>(240.0, r.width() * 0.6);
        const qreal top = std::min(last.bottom() + 6, r.bottom() - lh);
        chart_style::draw_scale_legend(p, QRectF(r.right() - lw, top, lw, lh), true, -clamp_, clamp_,
                                       [this](double v) { return format_value(v); });
    }
}

void HeatmapGrid::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= items_.size())
        return;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(chart_style::highlight_color(), 2));
    p.drawRoundedRect(cell_rect(hover_, r).adjusted(1, 1, -1, -1), 2, 2);
}

void HeatmapGrid::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    const QPointF pos = e->position();
    const int hit = cell_at(pos);
    if (hit != hover_) {
        hover_ = hit;
        update();
    }
    if (hit < 0) {
        hide_tip();
        unsetCursor();
        return;
    }
    setCursor(Qt::PointingHandCursor);
    const TreemapItem& it = items_.at(hit);
    QString tip;
    if (tooltip_provider_) {
        tip = tooltip_provider_(it);
    } else {
        QVector<QPair<QString, QString>> rows;
        if (!it.sublabel.isEmpty())
            rows.append({tr("Name"), it.sublabel});
        rows.append({tr("Change"), std::isnan(it.value) ? QStringLiteral("-") : format_value(it.value)});
        tip = chart_style::tooltip_html(it.label, rows);
    }
    show_tip(pos.toPoint(), tip, cell_rect(hit, content_rect()).toAlignedRect());
}

void HeatmapGrid::mousePressEvent(QMouseEvent* e) {
    press_ = (e->button() == Qt::LeftButton && chart_visible()) ? cell_at(e->position()) : -1;
    PaintedChart::mousePressEvent(e);
}

void HeatmapGrid::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int hit = cell_at(e->position());
        if (hit >= 0 && hit == press_)
            emit item_clicked(items_.at(hit).id);
    }
    press_ = -1;
    PaintedChart::mouseReleaseEvent(e);
}

void HeatmapGrid::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int hit = cell_at(e->position());
        if (hit >= 0)
            emit item_double_clicked(items_.at(hit).id);
    }
    PaintedChart::mouseDoubleClickEvent(e);
}

void HeatmapGrid::on_hover_left() {
    hover_ = -1;
    unsetCursor();
}

} // namespace fincept::ui
