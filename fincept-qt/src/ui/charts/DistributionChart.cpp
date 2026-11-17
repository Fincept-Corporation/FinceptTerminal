#include "ui/charts/DistributionChart.h"

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
constexpr double kDcPi = 3.14159265358979323846;

double dc_sorted_quantile(const QVector<double>& sorted, double q) {
    if (sorted.isEmpty())
        return std::nan("");
    const double pos = qBound(0.0, q, 1.0) * (sorted.size() - 1);
    const int lo = static_cast<int>(std::floor(pos));
    const int hi = static_cast<int>(std::ceil(pos));
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}
} // namespace

DistributionChart::DistributionChart(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DistributionChart::set_samples(const QVector<double>& values, int bins) {
    sorted_.clear();
    sorted_.reserve(values.size());
    for (double v : values) {
        if (std::isfinite(v))
            sorted_.append(v);
    }
    std::sort(sorted_.begin(), sorted_.end());
    requested_bins_ = bins;
    rebin();
}

void DistributionChart::clear() {
    sorted_.clear();
    markers_.clear();
    rebin();
}

void DistributionChart::set_normal_overlay(bool on) {
    normal_ = on;
    invalidate();
}

void DistributionChart::add_marker(double x, const QString& label, MarkerRole role) {
    if (!std::isfinite(x))
        return;
    markers_.append(Marker{x, label, role});
    invalidate();
}

void DistributionChart::clear_markers() {
    markers_.clear();
    invalidate();
}

void DistributionChart::set_signed_colors(bool on) {
    signed_ = on;
    invalidate();
}

void DistributionChart::set_show_percent(bool on) {
    percent_ = on;
    invalidate();
}

double DistributionChart::quantile(double q) const {
    return dc_sorted_quantile(sorted_, q);
}

void DistributionChart::rebin() {
    counts_.clear();
    hover_bin_ = -1;
    n_ = static_cast<int>(sorted_.size());
    mean_ = sd_ = 0;
    if (n_ >= 1) {
        double s = 0;
        for (double v : sorted_)
            s += v;
        mean_ = s / n_;
        double ss = 0;
        for (double v : sorted_)
            ss += (v - mean_) * (v - mean_);
        sd_ = n_ > 1 ? std::sqrt(ss / (n_ - 1)) : 0;
    }
    if (n_ < 2) {
        invalidate();
        return;
    }
    double mn = sorted_.first();
    double mx = sorted_.last();
    if (mx - mn < 1e-12) {
        const double pad = std::abs(mn) > 1e-12 ? std::abs(mn) * 0.05 : 0.5;
        mn -= pad;
        mx += pad;
    }
    int bins = requested_bins_;
    if (bins <= 0) {
        const double iqr = dc_sorted_quantile(sorted_, 0.75) - dc_sorted_quantile(sorted_, 0.25);
        const double h = 2.0 * iqr / std::cbrt(static_cast<double>(n_));
        bins = h > 0 ? static_cast<int>(std::ceil((mx - mn) / h)) : static_cast<int>(std::ceil(std::sqrt(n_)));
        bins = qBound(qMin(10, qMax(5, n_)), bins, 80);
    }
    bins = qBound(1, bins, 500);
    width_ = (mx - mn) / bins;
    // Edges on multiples of the bin width, so 0 is always an edge and no bin
    // straddles the sign change.
    lo_ = std::floor(mn / width_) * width_;
    // The maximum lands in the last bin even when it sits exactly on an edge.
    const int n_bins = qMax(1, static_cast<int>(std::ceil((mx - lo_) / width_ - 1e-9)));
    counts_.fill(0, qMin(n_bins, 1000));
    for (double v : sorted_) {
        const int i = qBound(0, static_cast<int>(std::floor((v - lo_) / width_)), static_cast<int>(counts_.size()) - 1);
        ++counts_[i];
    }
    invalidate();
}

double DistributionChart::y_of(int count) const {
    return percent_ ? (n_ > 0 ? 100.0 * count / n_ : 0.0) : static_cast<double>(count);
}

QColor DistributionChart::marker_color(MarkerRole role) const {
    switch (role) {
        case MarkerRole::Mean:
        case MarkerRole::Accent:
            return QColor(colors::AMBER());
        case MarkerRole::VaR:
        case MarkerRole::CVaR:
            return QColor(colors::NEGATIVE());
        case MarkerRole::Median:
        case MarkerRole::Neutral:
            break;
    }
    return QColor(colors::TEXT_SECONDARY());
}

DistributionChart::Frame DistributionChart::frame(const QRectF& r) const {
    Frame f;
    f.x_lo = lo_;
    f.x_hi = lo_ + width_ * counts_.size();
    for (const Marker& m : markers_) {
        f.x_lo = std::min(f.x_lo, m.x);
        f.x_hi = std::max(f.x_hi, m.x);
    }
    double y_max = 0;
    for (int c : counts_)
        y_max = std::max(y_max, y_of(c));
    if (normal_ && sd_ > 0) {
        const double peak = 1.0 / (sd_ * std::sqrt(2.0 * kDcPi)) * width_ * (percent_ ? 100.0 : n_);
        y_max = std::max(y_max, peak);
    }
    double nice_hi = y_max;
    f.y_ticks = chart_style::nice_ticks(0, std::max(y_max, 1e-9), 4, nullptr, &nice_hi);
    f.y_hi = nice_hi;

    const auto yfmt = [this](double v) {
        return percent_ ? chart_style::percent_text(v, v < 10 && v != std::floor(v) ? 1 : 0, false)
                        : chart_style::compact_number(v);
    };
    const QFontMetricsF afm(chart_style::axis_font());
    const qreal left = chart_style::axis_label_width(f.y_ticks, yfmt) + 10;
    const qreal top = markers_.isEmpty() ? 6 : afm.height() * 2 + 8;
    const qreal bottom = afm.height() + 8;
    f.plot = QRectF(r.left() + left, r.top() + top, std::max<qreal>(10, r.width() - left - 16),
                    std::max<qreal>(10, r.height() - top - bottom));
    f.x_ticks = chart_style::nice_ticks(f.x_lo, f.x_hi, std::max(2, static_cast<int>(f.plot.width() / 90)));
    return f;
}

void DistributionChart::paint_chart(QPainter& p, const QRectF& r) {
    const Frame f = frame(r);
    const auto xmap = [&f](double x) { return f.plot.left() + (x - f.x_lo) / (f.x_hi - f.x_lo) * f.plot.width(); };
    const auto ymap = [&f](double y) { return f.plot.bottom() - y / f.y_hi * f.plot.height(); };
    const auto yfmt = [this](double v) {
        return percent_ ? chart_style::percent_text(v, v < 10 && v != std::floor(v) ? 1 : 0, false)
                        : chart_style::compact_number(v);
    };

    chart_style::draw_value_axis(p, f.plot, f.y_ticks, 0, f.y_hi, Qt::Vertical, yfmt);

    // Bars.
    const QColor series = colors::chart_color(0);
    const QColor up(colors::POSITIVE());
    const QColor down(colors::NEGATIVE());
    p.setPen(Qt::NoPen);
    for (int i = 0; i < counts_.size(); ++i) {
        if (counts_[i] == 0)
            continue;
        const double b0 = lo_ + i * width_;
        const qreal x0 = xmap(b0);
        const qreal x1 = xmap(b0 + width_);
        const qreal gap = (x1 - x0) >= 5 ? 1.0 : 0.0;
        const qreal y = ymap(y_of(counts_[i]));
        QColor c = series;
        if (signed_)
            c = (b0 + width_ / 2.0) < 0 ? down : up;
        p.setBrush(c);
        p.drawRect(QRectF(x0 + gap, y, std::max<qreal>(0.5, x1 - x0 - 2 * gap), f.plot.bottom() - y));
    }

    // Baseline and x tick labels.
    const QFont af = chart_style::axis_font();
    const QFontMetricsF afm(af);
    p.setPen(QPen(chart_style::axis_color(), 1));
    p.drawLine(QPointF(f.plot.left(), f.plot.bottom() + 0.5), QPointF(f.plot.right(), f.plot.bottom() + 0.5));
    p.setFont(af);
    for (double t : f.x_ticks) {
        if (t < f.x_lo || t > f.x_hi)
            continue;
        const qreal x = std::round(xmap(t)) + 0.5;
        p.setPen(chart_style::axis_color());
        p.drawLine(QPointF(x, f.plot.bottom()), QPointF(x, f.plot.bottom() + 3));
        p.setPen(chart_style::axis_text_color());
        p.drawText(QRectF(x - 80, f.plot.bottom() + 4, 160, afm.height()), Qt::AlignHCenter | Qt::AlignTop,
                   format_value(t));
    }

    // Fitted normal curve.
    if (normal_ && sd_ > 0) {
        QPainterPath path;
        const int steps = std::max(40, static_cast<int>(f.plot.width() / 3));
        const double scale = width_ * (percent_ ? 100.0 : n_);
        for (int i = 0; i <= steps; ++i) {
            const double x = f.x_lo + (f.x_hi - f.x_lo) * i / steps;
            const double z = (x - mean_) / sd_;
            const double y = std::exp(-0.5 * z * z) / (sd_ * std::sqrt(2.0 * kDcPi)) * scale;
            const QPointF pt(xmap(x), ymap(std::min(y, f.y_hi)));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(colors::TEXT_SECONDARY()), 1.5));
        p.drawPath(path);
    }

    // Markers: dashed lines, labels staggered over two rows above the plot.
    QVector<QPair<qreal, qreal>> rows[2];
    p.setFont(af);
    for (const Marker& m : markers_) {
        const QColor c = marker_color(m.role);
        const qreal x = std::round(xmap(m.x)) + 0.5;
        QPen pen(c, 1.25, Qt::DashLine);
        p.setPen(pen);
        p.drawLine(QPointF(x, f.plot.top()), QPointF(x, f.plot.bottom()));
        const QString text = m.label.isEmpty() ? format_value(m.x) : m.label + QLatin1Char(' ') + format_value(m.x);
        const qreal tw = afm.horizontalAdvance(text);
        qreal tx = qBound(r.left(), x - tw / 2.0, r.right() - tw);
        int row = 0;
        for (const auto& span : rows[0]) {
            if (tx < span.second + 6 && tx + tw > span.first - 6) {
                row = 1;
                break;
            }
        }
        rows[row].append({tx, tx + tw});
        const qreal ty = f.plot.top() - (row == 0 ? afm.height() + 3 : afm.height() * 2 + 5);
        p.setPen(c);
        p.drawText(QRectF(tx, ty, tw + 1, afm.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
    }
}

int DistributionChart::bin_at(const QPointF& pos) const {
    const Frame f = frame(content_rect());
    if (!f.plot.adjusted(0, 0, 0, 4).contains(pos))
        return -1;
    const double x = f.x_lo + (pos.x() - f.plot.left()) / f.plot.width() * (f.x_hi - f.x_lo);
    const int i = static_cast<int>(std::floor((x - lo_) / width_));
    return (i >= 0 && i < counts_.size()) ? i : -1;
}

void DistributionChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_bin_ < 0 || hover_bin_ >= counts_.size())
        return;
    const Frame f = frame(r);
    const auto xmap = [&f](double x) { return f.plot.left() + (x - f.x_lo) / (f.x_hi - f.x_lo) * f.plot.width(); };
    const double b0 = lo_ + hover_bin_ * width_;
    const QRectF col(xmap(b0), f.plot.top(), xmap(b0 + width_) - xmap(b0), f.plot.height());
    p.setPen(Qt::NoPen);
    p.setBrush(colors::overlay_color(0.08));
    p.drawRect(col);
}

void DistributionChart::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    const QPointF pos = e->position();
    const int bin = bin_at(pos);
    if (bin != hover_bin_) {
        hover_bin_ = bin;
        update();
    }
    if (bin < 0) {
        hide_tip();
        return;
    }
    const double b0 = lo_ + bin * width_;
    const QString title = QStringLiteral("[") + format_value(b0) + QStringLiteral(", ") + format_value(b0 + width_) +
                          QStringLiteral(")");
    show_tip(pos.toPoint(),
             chart_style::tooltip_html(title, {{tr("Count"), QString::number(counts_[bin])},
                                               {tr("Share"), chart_style::percent_text(100.0 * counts_[bin] / n_, 1,
                                                                                        false)}}));
}

void DistributionChart::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int bin = bin_at(e->position());
        if (bin >= 0)
            emit bin_clicked(lo_ + bin * width_, lo_ + (bin + 1) * width_, counts_[bin]);
    }
    PaintedChart::mouseReleaseEvent(e);
}

void DistributionChart::on_hover_left() {
    hover_bin_ = -1;
}

} // namespace fincept::ui
