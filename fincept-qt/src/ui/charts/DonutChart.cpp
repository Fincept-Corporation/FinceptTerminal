#include "ui/charts/DonutChart.h"

#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {
constexpr const char* kDonutOtherId = "__other__";
} // namespace

DonutChart::DonutChart(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DonutChart::set_slices(const QVector<DonutSlice>& slices) {
    input_ = slices;
    rebuild();
}

void DonutChart::clear() {
    set_slices({});
}

void DonutChart::set_max_slices(int n) {
    max_slices_ = qMax(0, n);
    rebuild();
}

void DonutChart::set_other_label(const QString& label) {
    other_label_ = label;
    rebuild();
}

void DonutChart::set_center_text(const QString& title, const QString& value) {
    center_title_ = title;
    center_value_ = value;
    invalidate();
}

void DonutChart::set_hole_ratio(double ratio) {
    hole_ = qBound(0.0, ratio, 0.9);
    invalidate();
}

void DonutChart::set_legend_position(LegendPosition position) {
    legend_pos_ = position;
    invalidate();
}

void DonutChart::set_show_percent(bool show) {
    show_percent_ = show;
    invalidate();
}

void DonutChart::rebuild() {
    shown_.clear();
    start_deg_.clear();
    span_deg_.clear();
    total_ = 0;
    hover_ = -1;
    for (int i = 0; i < input_.size(); ++i) {
        const DonutSlice& s = input_.at(i);
        if (!(s.value > 0) || !std::isfinite(s.value))
            continue;
        shown_.append(Shown{s.id, s.label, s.value, s.color.isValid() ? s.color : colors::chart_color(i), 1});
        total_ += s.value;
    }
    std::stable_sort(shown_.begin(), shown_.end(), [](const Shown& a, const Shown& b) { return a.value > b.value; });
    if (max_slices_ > 0 && shown_.size() > max_slices_) {
        Shown other;
        other.id = QString::fromLatin1(kDonutOtherId);
        other.label = other_label_.isEmpty() ? tr("Other") : other_label_;
        other.color = QColor(colors::CHART_NEUTRAL());
        other.value = 0;
        other.count = 0;
        for (int i = max_slices_ - 1; i < shown_.size(); ++i) {
            other.value += shown_.at(i).value;
            other.count += shown_.at(i).count;
        }
        shown_.resize(max_slices_ - 1);
        shown_.append(other);
    }
    double acc = 0;
    for (const Shown& s : shown_) {
        const double span = total_ > 0 ? s.value / total_ * 360.0 : 0;
        start_deg_.append(acc);
        span_deg_.append(span);
        acc += span;
    }
    invalidate();
}

DonutChart::Geometry DonutChart::geometry(const QRectF& r) const {
    Geometry g;
    const QFontMetricsF fm(chart_style::font(-3));
    const qreal row_h = fm.height() + 4;
    QRectF area = r;
    const QRectF lr = r.adjusted(0, 0, -4, 0); // legend text never touches the edge
    LegendPosition pos = legend_pos_;
    if (pos == LegendPosition::Auto)
        pos = r.width() >= r.height() * 1.3 ? LegendPosition::Right : LegendPosition::Bottom;
    if (pos == LegendPosition::Right && r.width() > 220) {
        const qreal w = std::min<qreal>(lr.width() * 0.55, 300);
        const qreal h = std::min<qreal>(lr.height(), row_h * shown_.size());
        g.legend = QRectF(lr.right() - w, lr.center().y() - h / 2, w, h);
        g.legend_right = true;
        area.setRight(g.legend.left() - 12);
    } else if (pos != LegendPosition::None && r.height() > 160) {
        const qreal h = std::min<qreal>(lr.height() * 0.45, row_h * std::ceil(shown_.size() / 2.0));
        g.legend = QRectF(lr.left() + 3, lr.bottom() - h, lr.width() - 3, h);
        g.legend_right = false;
        area.setBottom(g.legend.top() - 8);
    }
    g.center = area.center();
    g.outer = std::max<qreal>(8, std::min(area.width(), area.height()) / 2.0 - 5);
    g.inner = g.outer * hole_;
    return g;
}

QPainterPath DonutChart::slice_path(const Geometry& g, int index, qreal grow) const {
    QPainterPath path;
    if (index < 0 || index >= shown_.size())
        return path;
    const qreal outer = g.outer + grow;
    const qreal inner = g.inner;
    double start = start_deg_[index];
    double span = span_deg_[index];
    // A 1.5 px gap at the outer edge between neighbours (none for a lone slice).
    if (shown_.size() > 1) {
        const double gap = qRadiansToDegrees(1.5 / std::max<qreal>(outer, 1.0));
        if (span > 3 * gap) {
            start += gap;
            span -= 2 * gap;
        }
    }
    // Clockwise from 12 o'clock → Qt's counter-clockwise from 3 o'clock.
    const double qt_start = 90.0 - start;
    const QRectF orect(g.center.x() - outer, g.center.y() - outer, 2 * outer, 2 * outer);
    if (inner < 1) {
        path.moveTo(g.center);
        path.arcTo(orect, qt_start, -span);
        path.closeSubpath();
        return path;
    }
    const QRectF irect(g.center.x() - inner, g.center.y() - inner, 2 * inner, 2 * inner);
    path.arcMoveTo(orect, qt_start);
    path.arcTo(orect, qt_start, -span);
    path.arcTo(irect, qt_start - span, span);
    path.closeSubpath();
    return path;
}

void DonutChart::paint_legend(QPainter& p, const Geometry& g) const {
    if (g.legend.isEmpty())
        return;
    const QFont f = chart_style::font(-3);
    const QFontMetricsF fm(f);
    const qreal row_h = fm.height() + 4;
    const int cols = g.legend_right ? 1 : 2;
    const qreal col_w = g.legend.width() / cols - (cols > 1 ? 8 : 0);
    const int rows_per_col = std::max(1, static_cast<int>(std::floor(g.legend.height() / row_h)));
    const qreal sw = std::max<qreal>(8, qRound(fm.height() * 0.6));
    qreal pct_w = show_percent_ ? fm.horizontalAdvance(QStringLiteral("100.0%")) + 6 : 0;
    qreal val_w = 0;
    for (const Shown& s : shown_)
        val_w = std::max(val_w, fm.horizontalAdvance(format_value(s.value)));
    val_w += 8;
    if (col_w - sw - 6 - val_w - pct_w < 40) { // too narrow: drop the share, then the value
        pct_w = 0;
        if (col_w - sw - 6 - val_w < 40)
            val_w = 0;
    }
    p.setFont(f);
    for (int i = 0; i < shown_.size(); ++i) {
        const int col = i / rows_per_col;
        const int row = i % rows_per_col;
        if (col >= cols)
            break; // out of room; the tooltip still names every slice
        const Shown& s = shown_.at(i);
        const qreal x = g.legend.left() + col * (col_w + 8);
        const qreal y = g.legend.top() + row * row_h;
        p.setPen(Qt::NoPen);
        p.setBrush(s.color);
        p.drawRoundedRect(QRectF(x, y + (row_h - sw) / 2, sw, sw), 2, 2);
        const qreal label_x = x + sw + 6;
        const qreal label_w = col_w - (label_x - x) - val_w - pct_w;
        p.setPen(chart_style::label_color());
        p.drawText(QRectF(label_x, y, label_w, row_h), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(s.label, Qt::ElideRight, label_w));
        if (val_w > 0) {
            p.setPen(chart_style::label_color());
            p.drawText(QRectF(label_x + label_w, y, val_w, row_h), Qt::AlignRight | Qt::AlignVCenter,
                       format_value(s.value));
        }
        if (pct_w > 0) {
            p.setPen(chart_style::muted_color());
            p.drawText(QRectF(label_x + label_w + val_w, y, pct_w, row_h), Qt::AlignRight | Qt::AlignVCenter,
                       chart_style::percent_text(total_ > 0 ? 100.0 * s.value / total_ : 0, 1, false));
        }
    }
}

void DonutChart::paint_chart(QPainter& p, const QRectF& r) {
    const Geometry g = geometry(r);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < shown_.size(); ++i) {
        p.setBrush(shown_.at(i).color);
        p.drawPath(slice_path(g, i, 0));
    }

    // Centre text.
    if (g.inner > 22) {
        const qreal w = g.inner * 1.6;
        const QString value = center_value_.isEmpty() ? format_value(total_) : center_value_;
        int offset = g.inner > 70 ? 6 : (g.inner > 45 ? 2 : -2);
        QFont vf = chart_style::font(offset, QFont::Bold);
        while (QFontMetricsF(vf).horizontalAdvance(value) > w && offset > -6)
            vf = chart_style::font(--offset, QFont::Bold);
        const QFontMetricsF vfm(vf);
        const QFont tf = chart_style::font(-4);
        const QFontMetricsF tfm(tf);
        const qreal th = center_title_.isEmpty() ? 0 : tfm.height();
        const qreal top = g.center.y() - (vfm.height() + th) / 2;
        if (!center_title_.isEmpty()) {
            p.setFont(tf);
            p.setPen(chart_style::muted_color());
            p.drawText(QRectF(g.center.x() - w / 2, top, w, th), Qt::AlignCenter,
                       tfm.elidedText(center_title_, Qt::ElideRight, w));
        }
        p.setFont(vf);
        p.setPen(chart_style::label_color());
        p.drawText(QRectF(g.center.x() - w / 2, top + th, w, vfm.height()), Qt::AlignCenter,
                   vfm.elidedText(value, Qt::ElideRight, w));
    }
    paint_legend(p, g);
}

void DonutChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= shown_.size())
        return;
    const Geometry g = geometry(r);
    p.setPen(Qt::NoPen);
    p.setBrush(shown_.at(hover_).color);
    p.drawPath(slice_path(g, hover_, 4));
    if (!g.legend.isEmpty()) {
        const QFontMetricsF fm(chart_style::font(-3));
        const qreal row_h = fm.height() + 4;
        const int cols = g.legend_right ? 1 : 2;
        const int rows_per_col = std::max(1, static_cast<int>(std::floor(g.legend.height() / row_h)));
        const int col = hover_ / rows_per_col;
        if (col < cols) {
            const qreal col_w = g.legend.width() / cols - (cols > 1 ? 8 : 0);
            const QRectF row(g.legend.left() + col * (col_w + 8) - 3, g.legend.top() + (hover_ % rows_per_col) * row_h,
                             col_w + 6, row_h);
            p.setBrush(colors::overlay_color(0.08));
            p.drawRoundedRect(row, 2, 2);
        }
    }
}

int DonutChart::slice_at(const QPointF& pos) const {
    const Geometry g = geometry(content_rect());
    const QPointF d = pos - g.center;
    const double dist = std::hypot(d.x(), d.y());
    if (dist > g.outer + 4 || dist < g.inner)
        return -1;
    // Clockwise degrees from 12 o'clock.
    double deg = qRadiansToDegrees(std::atan2(d.x(), -d.y()));
    if (deg < 0)
        deg += 360.0;
    for (int i = 0; i < shown_.size(); ++i) {
        if (deg >= start_deg_[i] && deg < start_deg_[i] + span_deg_[i])
            return i;
    }
    return -1;
}

int DonutChart::legend_row_at(const QPointF& pos) const {
    const Geometry g = geometry(content_rect());
    if (g.legend.isEmpty() || !g.legend.contains(pos))
        return -1;
    const QFontMetricsF fm(chart_style::font(-3));
    const qreal row_h = fm.height() + 4;
    const int cols = g.legend_right ? 1 : 2;
    const qreal col_w = g.legend.width() / cols;
    const int rows_per_col = std::max(1, static_cast<int>(std::floor(g.legend.height() / row_h)));
    const int col = static_cast<int>((pos.x() - g.legend.left()) / col_w);
    const int row = static_cast<int>((pos.y() - g.legend.top()) / row_h);
    const int i = col * rows_per_col + row;
    return (row < rows_per_col && i >= 0 && i < shown_.size()) ? i : -1;
}

void DonutChart::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    const QPointF pos = e->position();
    int hit = slice_at(pos);
    if (hit < 0)
        hit = legend_row_at(pos);
    if (hit != hover_) {
        hover_ = hit;
        emit slice_hovered(hit >= 0 ? shown_.at(hit).id : QString());
        update();
    }
    if (hit < 0) {
        hide_tip();
        unsetCursor();
        return;
    }
    setCursor(Qt::PointingHandCursor);
    const Shown& s = shown_.at(hit);
    QVector<QPair<QString, QString>> rows{
        {tr("Value"), format_value(s.value)},
        {tr("Share"), chart_style::percent_text(total_ > 0 ? 100.0 * s.value / total_ : 0, 2, false)}};
    if (s.count > 1)
        rows.append({tr("Items"), QString::number(s.count)});
    show_tip(pos.toPoint(), chart_style::tooltip_html(s.label, rows));
}

void DonutChart::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        int hit = slice_at(e->position());
        if (hit < 0)
            hit = legend_row_at(e->position());
        if (hit >= 0)
            emit slice_clicked(shown_.at(hit).id);
    }
    PaintedChart::mouseReleaseEvent(e);
}

void DonutChart::on_hover_left() {
    if (hover_ >= 0)
        emit slice_hovered(QString());
    hover_ = -1;
    unsetCursor();
}

} // namespace fincept::ui
