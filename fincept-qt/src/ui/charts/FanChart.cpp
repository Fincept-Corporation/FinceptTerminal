#include "ui/charts/FanChart.h"

#include "ui/charts/ChartRanges.h"
#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::ui {

namespace {
double fan_quantile_sorted(const QVector<double>& sorted, double q) {
    if (sorted.isEmpty())
        return std::nan("");
    const double pos = qBound(0.0, q, 1.0) * (sorted.size() - 1);
    const int lo = static_cast<int>(std::floor(pos));
    const int hi = static_cast<int>(std::ceil(pos));
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}

QString fan_pct_label(double pct) {
    return QString::number(pct, 'g', 3);
}

/// Path through the points i in [0, count) with ok(i); a point that is not ok breaks it.
template <class PointAt, class Ok>
QPainterPath fan_line_path(int count, PointAt point, Ok ok) {
    QPainterPath path;
    bool open = false;
    for (int i = 0; i < count; ++i) {
        if (!ok(i)) {
            open = false;
            continue;
        }
        const QPointF pt = point(i);
        if (open)
            path.lineTo(pt);
        else
            path.moveTo(pt);
        open = true;
    }
    return path;
}
} // namespace

FanChart::FanChart(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FanChart::set_fan(const QVector<double>& x, const QVector<FanBand>& bands, const QVector<double>& median) {
    x_ = x;
    bands_.clear();
    for (const FanBand& b : bands) {
        if (b.lower.size() == x.size() && b.upper.size() == x.size())
            bands_.append(b);
    }
    std::sort(bands_.begin(), bands_.end(), [](const FanBand& a, const FanBand& b) {
        return (a.upper_pct - a.lower_pct) > (b.upper_pct - b.lower_pct);
    });
    median_ = median.size() == x.size() ? median : QVector<double>{};
    samples_.clear();
    hover_index_ = -1;
    invalidate();
}

void FanChart::set_paths(const QVector<double>& x, const QVector<QVector<double>>& paths,
                         const QVector<QPair<double, double>>& percentile_pairs, int sample_paths) {
    const int n = static_cast<int>(x.size());
    QVector<FanBand> bands;
    for (const auto& pr : percentile_pairs) {
        FanBand b;
        b.lower_pct = pr.first;
        b.upper_pct = pr.second;
        b.lower.resize(n);
        b.upper.resize(n);
        bands.append(b);
    }
    QVector<double> median(n);
    QVector<double> column;
    column.reserve(paths.size());
    for (int i = 0; i < n; ++i) {
        column.clear();
        for (const auto& path : paths) {
            if (i < path.size() && std::isfinite(path[i]))
                column.append(path[i]);
        }
        std::sort(column.begin(), column.end());
        median[i] = fan_quantile_sorted(column, 0.5);
        for (FanBand& b : bands) {
            b.lower[i] = fan_quantile_sorted(column, b.lower_pct / 100.0);
            b.upper[i] = fan_quantile_sorted(column, b.upper_pct / 100.0);
        }
    }
    set_fan(x, bands, median);
    const int keep = qBound(0, sample_paths, static_cast<int>(paths.size()));
    for (int k = 0; k < keep; ++k) {
        const auto& path = paths.at(static_cast<int>(static_cast<qint64>(k) * paths.size() / keep));
        if (path.size() == n)
            samples_.append(path);
    }
    invalidate();
}

void FanChart::clear() {
    lines_.clear();
    next_line_slot_ = 0;
    set_fan({}, {}, {});
}

int FanChart::add_line(const QVector<double>& values, const QString& label, ChartColorRole role, bool dashed) {
    Line line;
    line.id = next_line_id_++;
    line.values = values;
    line.label = label;
    line.role = role;
    line.dashed = dashed;
    if (role == ChartColorRole::Auto || role == ChartColorRole::Palette) {
        // Categorical slots in order, skipping slot 1 (the fan's own colour, fan_color()).
        const int k = next_line_slot_++;
        line.palette_slot = k == 0 ? 0 : k + 1;
    }
    lines_.append(line);
    invalidate();
    return line.id;
}

void FanChart::remove_line(int id) {
    lines_.removeIf([id](const Line& l) { return l.id == id; });
    invalidate();
}

void FanChart::clear_lines() {
    if (lines_.isEmpty())
        return;
    lines_.clear();
    next_line_slot_ = 0;
    invalidate();
}

QColor FanChart::line_color(const Line& line) const {
    return chartkit::role_color(line.role, line.palette_slot);
}

QString FanChart::line_label(int index) const {
    const QString& label = lines_.at(index).label;
    return label.isEmpty() ? tr("Line %1").arg(index + 1) : label;
}

void FanChart::add_reference_line(double y, const QString& label) {
    if (!std::isfinite(y))
        return;
    refs_.append({y, label});
    invalidate();
}

void FanChart::clear_reference_lines() {
    refs_.clear();
    invalidate();
}

void FanChart::set_x_formatter(Formatter formatter) {
    x_formatter_ = std::move(formatter);
    invalidate();
}

QString FanChart::x_text(double x) const {
    return x_formatter_ ? x_formatter_(x) : chart_style::compact_number(x);
}

QColor FanChart::fan_color() const {
    // A categorical slot, never a direction colour (a fan is not up or down).
    return colors::chart_color(1);
}

FanChart::Frame FanChart::frame(const QRectF& r) const {
    Frame f;
    f.x_lo = x_.isEmpty() ? 0 : *std::min_element(x_.begin(), x_.end());
    f.x_hi = x_.isEmpty() ? 1 : *std::max_element(x_.begin(), x_.end());
    if (f.x_hi <= f.x_lo)
        f.x_hi = f.x_lo + 1;
    chart_ranges::Extent ext;
    for (const FanBand& b : bands_) {
        ext.take(b.lower);
        ext.take(b.upper);
    }
    ext.take(median_);
    for (const auto& s : samples_)
        ext.take(s);
    for (const auto& ref : refs_)
        ext.take(ref.first);
    for (const Line& line : lines_)
        ext.take(line.values, x_.size()); // only the part drawn against x
    double lo = ext.lo;
    double hi = ext.hi;
    if (!ext.valid()) {
        lo = 0;
        hi = 1;
    }
    const QFontMetricsF afm(chart_style::axis_font());
    // Legend row above the plot, only while extra lines exist (no lines = today's layout).
    const qreal legend_h = lines_.isEmpty() ? 0 : QFontMetricsF(chart_style::font(-3)).height() + 6;
    if (legend_h > 0)
        f.legend = QRectF(r.left(), r.top() + 2, r.width(), legend_h - 6);
    const qreal plot_h_guess = std::max<qreal>(40, r.height() - legend_h - afm.height() - 14);
    f.y_ticks = chart_style::nice_ticks(lo, hi, std::max(3, static_cast<int>(plot_h_guess / 50)), &f.y_lo, &f.y_hi);
    const auto yfmt = [this](double v) { return format_value(v); };
    const qreal left = chart_style::axis_label_width(f.y_ticks, yfmt) + 10;
    f.plot = QRectF(r.left() + left, r.top() + 8 + legend_h, std::max<qreal>(10, r.width() - left - 16),
                    std::max<qreal>(10, r.height() - legend_h - afm.height() - 16));
    f.x_ticks = chart_style::nice_ticks(f.x_lo, f.x_hi, std::max(2, static_cast<int>(f.plot.width() / 100)));
    return f;
}

void FanChart::paint_chart(QPainter& p, const QRectF& r) {
    const Frame f = frame(r);
    const auto xmap = [&f](double x) { return f.plot.left() + (x - f.x_lo) / (f.x_hi - f.x_lo) * f.plot.width(); };
    const auto ymap = [&f](double y) { return f.plot.bottom() - (y - f.y_lo) / (f.y_hi - f.y_lo) * f.plot.height(); };
    chart_style::draw_value_axis(p, f.plot, f.y_ticks, f.y_lo, f.y_hi, Qt::Vertical,
                                 [this](double v) { return format_value(v); });

    // x labels
    const QFont af = chart_style::axis_font();
    const QFontMetricsF afm(af);
    p.setFont(af);
    p.setPen(chart_style::axis_text_color());
    for (double t : f.x_ticks) {
        if (t < f.x_lo || t > f.x_hi)
            continue;
        const qreal x = xmap(t);
        p.drawText(QRectF(x - 80, f.plot.bottom() + 4, 160, afm.height()), Qt::AlignHCenter | Qt::AlignTop, x_text(t));
    }

    p.save();
    p.setClipRect(f.plot.adjusted(-1, -1, 1, 1));
    const QColor base = fan_color();
    const int nb = static_cast<int>(bands_.size());
    for (int bi = 0; bi < nb; ++bi) {
        const FanBand& b = bands_.at(bi);
        QPainterPath path;
        for (int i = 0; i < x_.size(); ++i) {
            const QPointF pt(xmap(x_[i]), ymap(b.upper[i]));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        for (int i = static_cast<int>(x_.size()) - 1; i >= 0; --i)
            path.lineTo(QPointF(xmap(x_[i]), ymap(b.lower[i])));
        path.closeSubpath();
        QColor c = base;
        c.setAlphaF(static_cast<float>(0.16 + 0.18 * bi)); // inner bands are stronger
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(path);
    }
    // Sample paths.
    QColor sample = QColor(colors::TEXT_TERTIARY());
    sample.setAlphaF(0.35f);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(sample, 1));
    for (const auto& s : samples_) {
        QPainterPath path;
        for (int i = 0; i < s.size(); ++i) {
            const QPointF pt(xmap(x_[i]), ymap(s[i]));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.drawPath(path);
    }
    // Median.
    if (!median_.isEmpty()) {
        QPainterPath path;
        for (int i = 0; i < median_.size(); ++i) {
            const QPointF pt(xmap(x_[i]), ymap(median_[i]));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.setPen(QPen(base, 2));
        p.drawPath(path);
    }
    // Extra lines (original / realised path) over the fan.
    const int nx = static_cast<int>(x_.size());
    for (const Line& line : lines_) {
        const int count = std::min(nx, static_cast<int>(line.values.size()));
        const QPainterPath path = fan_line_path(
            count, [&](int i) { return QPointF(xmap(x_[i]), ymap(line.values[i])); },
            [&](int i) { return std::isfinite(line.values[i]); });
        QPen pen(line_color(line), 2, line.dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(path);
    }
    p.restore();

    // Reference lines with right-aligned labels.
    for (const auto& ref : refs_) {
        const qreal y = std::round(ymap(ref.first)) + 0.5;
        p.setPen(QPen(QColor(colors::TEXT_SECONDARY()), 1, Qt::DashLine));
        p.drawLine(QPointF(f.plot.left(), y), QPointF(f.plot.right(), y));
        if (!ref.second.isEmpty()) {
            p.setFont(af);
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRectF(f.plot.left() + 4, y - afm.height() - 1, f.plot.width() - 8, afm.height()),
                       Qt::AlignRight | Qt::AlignBottom, ref.second + QLatin1Char(' ') + format_value(ref.first));
        }
    }
    p.setPen(QPen(chart_style::axis_color(), 1));
    p.drawLine(QPointF(f.plot.left(), f.plot.bottom() + 0.5), QPointF(f.plot.right(), f.plot.bottom() + 0.5));
    if (!f.legend.isEmpty())
        paint_legend(p, QRectF(f.plot.left(), f.legend.top(), f.legend.right() - f.plot.left(), f.legend.height()));
}

void FanChart::paint_legend(QPainter& p, const QRectF& legend) const {
    // Extra lines first (they are what the caller added), then the median, then the bands
    // innermost first; entries that do not fit are dropped from the end. Lines get a line glyph
    // (dashed when dashed), bands a filled swatch — the kind reads by shape, not only by colour.
    const QFont lf = chart_style::font(-3);
    const QFontMetricsF lfm(lf);
    const qreal glyph_w = std::max<qreal>(14.0, std::round(lfm.height() * 1.2));
    const qreal cy = legend.top() + lfm.height() / 2.0;
    const QColor text = chart_style::label_color();
    qreal x = legend.left();
    const auto line_entry = [&](const QString& label, const QPen& pen) {
        const qreal need = glyph_w + 5 + lfm.horizontalAdvance(label);
        if (x + need > legend.right())
            return false;
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(pen);
        p.drawLine(QPointF(x, cy), QPointF(x + glyph_w, cy));
        p.setFont(lf);
        p.setPen(text);
        p.drawText(QRectF(x + glyph_w + 5, legend.top(), lfm.horizontalAdvance(label) + 1, lfm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, label);
        p.restore();
        x += need + 14;
        return true;
    };
    for (int i = 0; i < lines_.size(); ++i) {
        const Line& line = lines_.at(i);
        if (!line_entry(line_label(i), QPen(line_color(line), 2, line.dashed ? Qt::DashLine : Qt::SolidLine,
                                            Qt::FlatCap)))
            return;
    }
    if (!median_.isEmpty() && !line_entry(tr("Median"), QPen(fan_color(), 2)))
        return;
    const int nb = static_cast<int>(bands_.size());
    for (int bi = nb - 1; bi >= 0; --bi) {
        const FanBand& b = bands_.at(bi);
        const QString label = QStringLiteral("P%1 - P%2").arg(fan_pct_label(b.lower_pct), fan_pct_label(b.upper_pct));
        const qreal need = lfm.height() * 0.6 + 5 + lfm.horizontalAdvance(label);
        if (x + need > legend.right())
            return;
        QColor c = fan_color();
        c.setAlphaF(static_cast<float>(0.16 + 0.18 * bi)); // as painted
        x += chart_style::draw_swatch_label(p, QPointF(x, legend.top()), c, label, text) + 14;
    }
}

void FanChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_index_ < 0 || hover_index_ >= x_.size())
        return;
    const Frame f = frame(r);
    const qreal x = f.plot.left() + (x_[hover_index_] - f.x_lo) / (f.x_hi - f.x_lo) * f.plot.width();
    p.setPen(QPen(chart_style::muted_color(), 1, Qt::DashLine));
    p.drawLine(QPointF(x, f.plot.top()), QPointF(x, f.plot.bottom()));
    if (!median_.isEmpty()) {
        const qreal y = f.plot.bottom() - (median_[hover_index_] - f.y_lo) / (f.y_hi - f.y_lo) * f.plot.height();
        p.setPen(QPen(chart_style::label_color(), 1.5));
        p.setBrush(fan_color());
        p.drawEllipse(QPointF(x, y), 4, 4);
    }
    for (const Line& line : lines_) {
        if (hover_index_ >= line.values.size() || !std::isfinite(line.values[hover_index_]))
            continue;
        const qreal y = f.plot.bottom() - (line.values[hover_index_] - f.y_lo) / (f.y_hi - f.y_lo) * f.plot.height();
        p.setPen(QPen(chart_style::label_color(), 1.5));
        p.setBrush(line_color(line));
        p.drawEllipse(QPointF(x, y), 4, 4);
    }
}

void FanChart::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    const Frame f = frame(content_rect());
    const QPointF pos = e->position();
    if (!f.plot.adjusted(-4, -4, 4, 4).contains(pos)) {
        if (hover_index_ >= 0) {
            hover_index_ = -1;
            update();
        }
        hide_tip();
        return;
    }
    const double xv = f.x_lo + (pos.x() - f.plot.left()) / f.plot.width() * (f.x_hi - f.x_lo);
    int best = 0;
    for (int i = 1; i < x_.size(); ++i) {
        if (std::abs(x_[i] - xv) < std::abs(x_[best] - xv))
            best = i;
    }
    if (best != hover_index_) {
        hover_index_ = best;
        update();
    }
    QVector<QPair<QString, QString>> rows;
    for (int li = 0; li < lines_.size(); ++li) {
        const Line& line = lines_.at(li);
        if (best < line.values.size() && std::isfinite(line.values[best]))
            rows.append({line_label(li), format_value(line.values[best])});
    }
    if (!median_.isEmpty())
        rows.append({tr("Median"), format_value(median_[best])});
    for (int bi = static_cast<int>(bands_.size()) - 1; bi >= 0; --bi) { // innermost first
        const FanBand& b = bands_.at(bi);
        rows.append({QStringLiteral("P%1 - P%2").arg(fan_pct_label(b.lower_pct), fan_pct_label(b.upper_pct)),
                     format_value(b.lower[best]) + QStringLiteral(" - ") + format_value(b.upper[best])});
    }
    show_tip(pos.toPoint(), chart_style::tooltip_html(x_text(x_[best]), rows));
}

void FanChart::on_hover_left() {
    hover_index_ = -1;
}

} // namespace fincept::ui
