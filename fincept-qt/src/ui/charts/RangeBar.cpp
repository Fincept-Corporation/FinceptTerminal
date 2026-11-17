#include "ui/charts/RangeBar.h"

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

QString rb_text(const std::function<QString(double)>& fmt, double v) {
    return fmt ? fmt(v) : chart_style::compact_number(v);
}

QString rb_marker_text(const RangeMarker& m, const std::function<QString(double)>& fmt) {
    return m.label.isEmpty() ? rb_text(fmt, m.value) : m.label + QLatin1Char(' ') + rb_text(fmt, m.value);
}

QColor rb_band_color(RangeBand::Tone tone) {
    QColor c;
    switch (tone) {
        case RangeBand::Tone::Up:
            c = QColor(colors::POSITIVE());
            break;
        case RangeBand::Tone::Down:
            c = QColor(colors::NEGATIVE());
            break;
        case RangeBand::Tone::Accent:
            c = QColor(colors::AMBER());
            break;
        case RangeBand::Tone::Caution:
            c = QColor(colors::WARNING());
            break;
        case RangeBand::Tone::Neutral:
            c = QColor(colors::TEXT_TERTIARY());
            break;
    }
    c.setAlphaF(0.4f);
    return c;
}

QColor rb_marker_color(RangeMarker::Role role) {
    switch (role) {
        case RangeMarker::Role::Current:
            return QColor(colors::TEXT_PRIMARY());
        case RangeMarker::Role::Target:
        case RangeMarker::Role::Mean:
            return QColor(colors::AMBER());
        case RangeMarker::Role::Up:
            return QColor(colors::POSITIVE());
        case RangeMarker::Role::Down:
            return QColor(colors::NEGATIVE());
        case RangeMarker::Role::Low:
        case RangeMarker::Role::High:
        case RangeMarker::Role::Neutral:
            break;
    }
    return QColor(colors::TEXT_TERTIARY());
}

struct RbGeometry {
    QRectF track;
    bool tall = false;
    qreal label_h = 0;
    QRectF low_text;  // compact mode
    QRectF high_text; // compact mode
};

RbGeometry rb_geometry(const QRectF& r, const RangeBarData& d, const std::function<QString(double)>& fmt,
                       bool show_labels) {
    RbGeometry g;
    const QFontMetricsF fm(chart_style::font(-4));
    g.label_h = fm.height();
    const qreal track_h_tall = 6;
    const qreal need_tall = g.label_h * 2 + 6 + track_h_tall + 8;
    g.tall = show_labels && r.height() >= need_tall;
    if (g.tall) {
        const qreal top = r.top() + (r.height() - need_tall) / 2.0;
        const qreal track_y = top + g.label_h + 2 + 6;
        g.track = QRectF(r.left() + 2, track_y, std::max<qreal>(4, r.width() - 4), track_h_tall);
        return g;
    }
    const qreal track_h = std::min<qreal>(4, std::max<qreal>(2, r.height() / 4));
    QRectF track(r.left() + 2, r.center().y() - track_h / 2.0, std::max<qreal>(4, r.width() - 4), track_h);
    if (show_labels && r.width() > 120) {
        const QString lo = rb_text(fmt, d.low);
        const QString hi = rb_text(fmt, d.high);
        const qreal lw = fm.horizontalAdvance(lo);
        const qreal hw = fm.horizontalAdvance(hi);
        g.low_text = QRectF(r.left() + 2, r.center().y() - g.label_h / 2, lw, g.label_h);
        g.high_text = QRectF(r.right() - 2 - hw, r.center().y() - g.label_h / 2, hw, g.label_h);
        track.setLeft(g.low_text.right() + 6);
        track.setRight(g.high_text.left() - 6);
        if (track.width() < 20) {
            g.low_text = g.high_text = QRectF();
            track = QRectF(r.left() + 2, track.top(), std::max<qreal>(4, r.width() - 4), track_h);
        }
    }
    g.track = track;
    return g;
}

} // namespace

RangeBar::RangeBar(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RangeBar::set_data(const RangeBarData& data) {
    data_ = data;
    invalidate();
}

void RangeBar::set_range(double low, double high) {
    data_.low = low;
    data_.high = high;
    invalidate();
}

void RangeBar::set_current(double value, const QString& label) {
    data_.markers.erase(std::remove_if(data_.markers.begin(), data_.markers.end(),
                                       [](const RangeMarker& m) { return m.role == RangeMarker::Role::Current; }),
                        data_.markers.end());
    if (std::isfinite(value))
        data_.markers.append(RangeMarker{value, label, RangeMarker::Role::Current});
    invalidate();
}

void RangeBar::add_marker(double value, const QString& label, RangeMarker::Role role) {
    if (!std::isfinite(value))
        return;
    data_.markers.append(RangeMarker{value, label, role});
    invalidate();
}

void RangeBar::clear_markers() {
    data_.markers.clear();
    invalidate();
}

void RangeBar::set_bands(const QVector<RangeBand>& bands) {
    data_.bands = bands;
    invalidate();
}

void RangeBar::set_show_labels(bool show) {
    show_labels_ = show;
    updateGeometry();
    invalidate();
}

bool RangeBar::has_data() const {
    return std::isfinite(data_.low) && std::isfinite(data_.high) && data_.high > data_.low;
}

QSize RangeBar::sizeHint() const {
    const QFontMetricsF fm(chart_style::font(-4));
    return {240, show_labels_ ? qCeil(fm.height() * 2 + 26) : 16};
}

QSize RangeBar::minimumSizeHint() const {
    return {60, 12};
}

QRectF RangeBar::track_rect(const QRectF& r, const RangeBarData& d, const std::function<QString(double)>& fmt,
                            bool show_labels) {
    return rb_geometry(r, d, fmt, show_labels).track;
}

void RangeBar::paint_range(QPainter& p, const QRectF& r, const RangeBarData& d,
                           const std::function<QString(double)>& fmt, bool show_labels) {
    if (!(d.high > d.low) || !std::isfinite(d.low) || !std::isfinite(d.high))
        return;
    const RbGeometry g = rb_geometry(r, d, fmt, show_labels);
    const QRectF& t = g.track;
    const auto xmap = [&](double v) {
        return t.left() + qBound(0.0, (v - d.low) / (d.high - d.low), 1.0) * t.width();
    };
    const qreal radius = t.height() / 2.0;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(colors::BORDER_DIM()));
    p.drawRoundedRect(t, radius, radius);

    for (const RangeBand& b : d.bands) {
        const qreal x0 = xmap(std::min(b.from, b.to));
        const qreal x1 = xmap(std::max(b.from, b.to));
        if (x1 - x0 < 0.5)
            continue;
        p.setBrush(rb_band_color(b.tone));
        p.drawRect(QRectF(x0, t.top() - 2, x1 - x0, t.height() + 4));
    }

    const RangeMarker* current = nullptr;
    for (const RangeMarker& m : d.markers) {
        if (m.role == RangeMarker::Role::Current) {
            current = &m;
            break;
        }
    }
    if (current && d.fill_to_current) {
        QColor fill(colors::AMBER());
        fill.setAlphaF(0.55f);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(t.left(), t.top(), std::max<qreal>(t.height(), xmap(current->value) - t.left()),
                                 t.height()),
                          radius, radius);
    }

    // Secondary markers: ticks across the track.
    for (const RangeMarker& m : d.markers) {
        if (m.role == RangeMarker::Role::Current)
            continue;
        const qreal x = std::round(xmap(m.value)) + 0.5;
        const bool strong = m.role == RangeMarker::Role::Target || m.role == RangeMarker::Role::Mean ||
                            m.role == RangeMarker::Role::Up || m.role == RangeMarker::Role::Down;
        p.setPen(QPen(rb_marker_color(m.role), strong ? 2.0 : 1.5));
        p.drawLine(QPointF(x, t.top() - 4), QPointF(x, t.bottom() + 4));
    }

    const QFont f = chart_style::font(-4);
    const QFontMetricsF fm(f);
    p.setFont(f);

    // Current marker: pin + line (+ value above it in tall mode).
    if (current) {
        const qreal x = xmap(current->value);
        const QColor c = rb_marker_color(RangeMarker::Role::Current);
        p.setPen(QPen(c, 2));
        p.drawLine(QPointF(x, t.top() - (g.tall ? 2 : 3)), QPointF(x, t.bottom() + (g.tall ? 2 : 3)));
        if (g.tall) {
            QPainterPath pin;
            pin.moveTo(x - 5, t.top() - 8);
            pin.lineTo(x + 5, t.top() - 8);
            pin.lineTo(x, t.top() - 2);
            pin.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawPath(pin);
            const QFont bf = chart_style::font(-4, QFont::Bold);
            const QFontMetricsF bfm(bf);
            const QString text = rb_marker_text(*current, fmt);
            const qreal w = bfm.horizontalAdvance(text);
            const qreal tx = qBound(r.left(), x - w / 2.0, r.right() - w);
            p.setFont(bf);
            p.setPen(c);
            p.drawText(QRectF(tx, t.top() - 10 - g.label_h, w + 1, g.label_h), Qt::AlignCenter, text);
            p.setFont(f);
        }
    }

    // Labels.
    p.setPen(chart_style::axis_text_color());
    if (g.tall) {
        const qreal ly = t.bottom() + 5;
        const QString lo = rb_text(fmt, d.low);
        const QString hi = rb_text(fmt, d.high);
        const QRectF lo_rect(t.left(), ly, fm.horizontalAdvance(lo) + 1, g.label_h);
        const QRectF hi_rect(t.right() - fm.horizontalAdvance(hi) - 1, ly, fm.horizontalAdvance(hi) + 1, g.label_h);
        p.drawText(lo_rect, Qt::AlignLeft | Qt::AlignVCenter, lo);
        p.drawText(hi_rect, Qt::AlignRight | Qt::AlignVCenter, hi);
        QVector<QRectF> taken{lo_rect, hi_rect};
        for (const RangeMarker& m : d.markers) {
            if (m.role == RangeMarker::Role::Current)
                continue;
            const QString text = rb_marker_text(m, fmt);
            const qreal w = fm.horizontalAdvance(text) + 1;
            const qreal x = xmap(m.value);
            const QRectF rect(qBound(r.left(), x - w / 2.0, r.right() - w), ly, w, g.label_h);
            bool clash = false;
            for (const QRectF& o : taken)
                clash = clash || o.adjusted(-4, 0, 4, 0).intersects(rect);
            if (clash)
                continue;
            taken.append(rect);
            p.setPen(rb_marker_color(m.role));
            p.drawText(rect, Qt::AlignCenter, text);
        }
    } else if (!g.low_text.isEmpty()) {
        p.drawText(g.low_text, Qt::AlignLeft | Qt::AlignVCenter, rb_text(fmt, d.low));
        p.drawText(g.high_text, Qt::AlignRight | Qt::AlignVCenter, rb_text(fmt, d.high));
    }
    p.restore();
}

QString RangeBar::tooltip_for(const RangeBarData& d, const std::function<QString(double)>& fmt) {
    QVector<QPair<QString, QString>> rows;
    rows.append({QObject::tr("Low"), rb_text(fmt, d.low)});
    rows.append({QObject::tr("High"), rb_text(fmt, d.high)});
    for (const RangeMarker& m : d.markers) {
        QString name = m.label;
        if (name.isEmpty()) {
            switch (m.role) {
                case RangeMarker::Role::Current:
                    name = QObject::tr("Current");
                    break;
                case RangeMarker::Role::Target:
                    name = QObject::tr("Target");
                    break;
                case RangeMarker::Role::Mean:
                    name = QObject::tr("Mean");
                    break;
                default:
                    name = QObject::tr("Marker");
                    break;
            }
        }
        QString value = rb_text(fmt, m.value);
        if (m.role == RangeMarker::Role::Current && d.high > d.low) {
            const double pos = (m.value - d.low) / (d.high - d.low) * 100.0;
            value += QStringLiteral("  (") + QObject::tr("%1% of range").arg(qRound(pos)) + QLatin1Char(')');
        }
        rows.append({name, value});
    }
    for (const RangeBand& b : d.bands) {
        if (!b.label.isEmpty())
            rows.append({b.label, rb_text(fmt, b.from) + QStringLiteral(" - ") + rb_text(fmt, b.to)});
    }
    return chart_style::tooltip_html(QString(), rows);
}

void RangeBar::paint_chart(QPainter& p, const QRectF& r) {
    paint_range(p, r, data_, [this](double v) { return format_value(v); }, show_labels_);
}

void RangeBar::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    show_tip(e->position().toPoint(), tooltip_for(data_, [this](double v) { return format_value(v); }), rect());
}

} // namespace fincept::ui
