#include "ui/charts/Gauge.h"

#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>

namespace fincept::ui {

Gauge::Gauge(QWidget* parent) : PaintedChart(parent) {
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void Gauge::set_style(Style style) {
    style_ = style;
    QSizePolicy sp = sizePolicy();
    sp.setHeightForWidth(style_ == Style::SemiCircle);
    sp.setVerticalPolicy(style_ == Style::SemiCircle ? QSizePolicy::Preferred : QSizePolicy::Fixed);
    setSizePolicy(sp);
    updateGeometry();
    invalidate();
}

void Gauge::set_range(double min, double max) {
    if (!(max > min))
        return;
    min_ = min;
    max_ = max;
    invalidate();
}

void Gauge::set_value(double value) {
    value_ = value;
    invalidate();
}

void Gauge::set_zones(const QVector<GaugeZone>& zones) {
    zones_ = zones;
    invalidate();
}

void Gauge::set_caption(const QString& caption) {
    caption_ = caption;
    invalidate();
}

void Gauge::set_show_range_labels(bool show) {
    show_range_labels_ = show;
    invalidate();
}

const GaugeZone* Gauge::zone_for(double v) const {
    for (const GaugeZone& z : zones_) {
        if (v >= std::min(z.from, z.to) && v <= std::max(z.from, z.to))
            return &z;
    }
    return nullptr;
}

QSize Gauge::sizeHint() const {
    return style_ == Style::SemiCircle ? QSize(220, heightForWidth(220)) : QSize(260, minimumSizeHint().height() + 8);
}

QSize Gauge::minimumSizeHint() const {
    const QFontMetricsF fm(chart_style::font(-4));
    if (style_ == Style::Linear)
        return {120, qCeil(fm.height() * 3 + 18)};
    return {100, 64};
}

int Gauge::heightForWidth(int w) const {
    const QFontMetricsF fm(chart_style::font(-4));
    return qCeil(w / 2.0 + fm.height() + 10);
}

QColor Gauge::tone_color(GaugeZone::Tone tone) const {
    switch (tone) {
        case GaugeZone::Tone::Bad:
            return QColor(colors::DANGER());
        case GaugeZone::Tone::Caution:
            return QColor(colors::WARNING());
        case GaugeZone::Tone::Good:
            return QColor(colors::SUCCESS());
        case GaugeZone::Tone::Down:
            return QColor(colors::NEGATIVE());
        case GaugeZone::Tone::Up:
            return QColor(colors::POSITIVE());
        case GaugeZone::Tone::Accent:
            return QColor(colors::AMBER());
        case GaugeZone::Tone::Neutral:
            break;
    }
    return QColor(colors::TEXT_TERTIARY());
}

QColor Gauge::value_color() const {
    const GaugeZone* z = zone_for(value_);
    return z ? tone_color(z->tone) : QColor(colors::AMBER());
}

void Gauge::paint_chart(QPainter& p, const QRectF& r) {
    if (style_ == Style::SemiCircle)
        paint_semicircle(p, r);
    else
        paint_linear(p, r);
}

void Gauge::paint_semicircle(QPainter& p, const QRectF& r) {
    const QFont small = chart_style::font(-4);
    const QFontMetricsF sfm(small);
    const qreal label_h = show_range_labels_ ? sfm.height() + 4 : 0;
    const qreal radius = std::max<qreal>(10, std::min(r.width() / 2.0 - 4, r.height() - label_h - 6));
    const QPointF c(r.center().x(), r.top() + 4 + radius);
    const qreal thick = std::max<qreal>(6, radius * 0.18);
    const qreal zone_w = std::max<qreal>(2, thick * 0.3);
    const qreal track_w = thick - zone_w - 2;
    const auto angle_of = [this](double v) { return 180.0 - qBound(0.0, (v - min_) / (max_ - min_), 1.0) * 180.0; };
    const auto arc = [&](qreal rad, qreal width, double from_v, double to_v, const QColor& color) {
        const QRectF box(c.x() - rad, c.y() - rad, 2 * rad, 2 * rad);
        const double a0 = angle_of(from_v);
        const double a1 = angle_of(to_v);
        QPen pen(color, width, Qt::SolidLine, Qt::FlatCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(box, qRound(a0 * 16), qRound((a1 - a0) * 16));
    };

    // Outer ring: the zones (or a plain track).
    const qreal zone_r = radius - zone_w / 2.0;
    if (zones_.isEmpty()) {
        arc(zone_r, zone_w, min_, max_, QColor(colors::BORDER_DIM()));
    } else {
        arc(zone_r, zone_w, min_, max_, QColor(colors::BORDER_DIM()));
        for (const GaugeZone& z : zones_)
            arc(zone_r, zone_w, std::min(z.from, z.to), std::max(z.from, z.to), tone_color(z.tone));
    }
    // Inner ring: track + value.
    const qreal track_r = radius - zone_w - 2 - track_w / 2.0;
    arc(track_r, track_w, min_, max_, QColor(colors::BORDER_DIM()));
    const double v = qBound(min_, value_, max_);
    if (v > min_)
        arc(track_r, track_w, min_, v, value_color());
    // Value tick across both rings.
    const double a = qDegreesToRadians(angle_of(v));
    const QPointF outer(c.x() + std::cos(a) * (radius + 2), c.y() - std::sin(a) * (radius + 2));
    const QPointF inner(c.x() + std::cos(a) * (track_r - track_w / 2.0 - 2), c.y() - std::sin(a) * (track_r - track_w / 2.0 - 2));
    p.setPen(QPen(QColor(colors::TEXT_PRIMARY()), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(inner, outer);

    // Value + caption in the bowl.
    const int value_offset = radius > 90 ? 10 : (radius > 60 ? 6 : (radius > 40 ? 2 : -2));
    const QFont vf = chart_style::font(value_offset, QFont::Bold);
    const QFontMetricsF vfm(vf);
    const QString text = format_value(value_);
    const qreal cap_h = caption_.isEmpty() ? 0 : sfm.height();
    const qreal text_bottom = c.y() - cap_h;
    p.setFont(vf);
    p.setPen(QColor(colors::TEXT_PRIMARY()));
    p.drawText(QRectF(c.x() - radius, text_bottom - vfm.height(), 2 * radius, vfm.height()), Qt::AlignCenter, text);
    if (!caption_.isEmpty()) {
        p.setFont(small);
        p.setPen(value_color());
        p.drawText(QRectF(c.x() - radius, text_bottom, 2 * radius, sfm.height()), Qt::AlignCenter,
                   sfm.elidedText(caption_, Qt::ElideRight, 2 * (track_r - track_w)));
    }
    if (show_range_labels_) {
        p.setFont(small);
        p.setPen(chart_style::axis_text_color());
        const qreal y = c.y() + 4;
        const QString lo = format_value(min_);
        const QString hi = format_value(max_);
        p.drawText(QRectF(c.x() - radius, y, radius, sfm.height()), Qt::AlignLeft | Qt::AlignTop, lo);
        p.drawText(QRectF(c.x(), y, radius, sfm.height()), Qt::AlignRight | Qt::AlignTop, hi);
    }
}

void Gauge::paint_linear(QPainter& p, const QRectF& r) {
    const QFont small = chart_style::font(-4);
    const QFontMetricsF sfm(small);
    const QFont vf = chart_style::font(-2, QFont::Bold);
    const QFontMetricsF vfm(vf);
    const qreal track_h = 8;
    const qreal top = r.top() + 2;
    const QRectF track(r.left() + 2, top + vfm.height() + 8, std::max<qreal>(10, r.width() - 4), track_h);
    const auto xmap = [&](double v) { return track.left() + qBound(0.0, (v - min_) / (max_ - min_), 1.0) * track.width(); };

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(colors::BORDER_DIM()));
    p.drawRoundedRect(track, track_h / 2, track_h / 2);
    const double v = qBound(min_, value_, max_);
    if (zones_.isEmpty()) {
        p.setBrush(QColor(colors::AMBER()));
        p.drawRoundedRect(QRectF(track.left(), track.top(), std::max<qreal>(track_h, xmap(v) - track.left()), track_h),
                          track_h / 2, track_h / 2);
    } else {
        for (const GaugeZone& z : zones_) {
            const qreal x0 = xmap(std::min(z.from, z.to));
            const qreal x1 = xmap(std::max(z.from, z.to));
            p.setBrush(tone_color(z.tone));
            p.drawRect(QRectF(x0 + 0.5, track.top(), std::max<qreal>(0.5, x1 - x0 - 1), track_h));
        }
    }
    // Marker: a pin above the track.
    const qreal x = xmap(v);
    QPainterPath pin;
    pin.moveTo(x - 5, track.top() - 7);
    pin.lineTo(x + 5, track.top() - 7);
    pin.lineTo(x, track.top() - 1);
    pin.closeSubpath();
    p.setBrush(QColor(colors::TEXT_PRIMARY()));
    p.drawPath(pin);

    // Value (+ caption) above the pin.
    QString text = format_value(value_);
    const qreal tw = vfm.horizontalAdvance(text);
    const qreal cw = caption_.isEmpty() ? 0 : sfm.horizontalAdvance(caption_) + 6;
    const qreal tx = qBound(r.left(), x - tw / 2.0, r.right() - tw - cw);
    p.setFont(vf);
    p.setPen(QColor(colors::TEXT_PRIMARY()));
    p.drawText(QRectF(tx, top, tw + 1, vfm.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
    if (!caption_.isEmpty()) {
        p.setFont(small);
        p.setPen(value_color());
        p.drawText(QRectF(tx + tw + 6, top, cw, vfm.height()), Qt::AlignLeft | Qt::AlignVCenter, caption_);
    }

    // Range labels and zone labels under the track.
    p.setFont(small);
    const qreal ly = track.bottom() + 4;
    QVector<QRectF> taken;
    if (show_range_labels_) {
        p.setPen(chart_style::axis_text_color());
        const QString lo = format_value(min_);
        const QString hi = format_value(max_);
        const QRectF lr(track.left(), ly, sfm.horizontalAdvance(lo) + 1, sfm.height());
        const QRectF hr(track.right() - sfm.horizontalAdvance(hi) - 1, ly, sfm.horizontalAdvance(hi) + 1, sfm.height());
        p.drawText(lr, Qt::AlignLeft, lo);
        p.drawText(hr, Qt::AlignRight, hi);
        taken << lr << hr;
    }
    for (const GaugeZone& z : zones_) {
        if (z.label.isEmpty())
            continue;
        const qreal x0 = xmap(std::min(z.from, z.to));
        const qreal x1 = xmap(std::max(z.from, z.to));
        const qreal w = sfm.horizontalAdvance(z.label) + 1;
        if (w > x1 - x0)
            continue;
        const QRectF lr((x0 + x1) / 2 - w / 2, ly, w, sfm.height());
        bool clash = false;
        for (const QRectF& o : taken)
            clash = clash || o.adjusted(-4, 0, 4, 0).intersects(lr);
        if (clash)
            continue;
        taken << lr;
        p.setPen(chart_style::muted_color());
        p.drawText(lr, Qt::AlignCenter, z.label);
    }
}

void Gauge::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    QVector<QPair<QString, QString>> rows{{tr("Value"), format_value(value_)},
                                         {tr("Range"), format_value(min_) + QStringLiteral(" - ") + format_value(max_)}};
    if (const GaugeZone* z = zone_for(value_); z && !z->label.isEmpty())
        rows.append({tr("Zone"), z->label});
    show_tip(e->position().toPoint(), chart_style::tooltip_html(caption_, rows), rect());
}

} // namespace fincept::ui
