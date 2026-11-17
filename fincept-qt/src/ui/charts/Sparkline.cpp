#include "ui/charts/Sparkline.h"

#include "ui/charts/ChartKit.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>

namespace fincept::ui {

namespace {

/// Min/max decimation for evenly spaced values: index i falls in column
/// floor(i * cols / n). Appends the first/min/max/last index of each column
/// in index order; one index per run of non-finite values marks a gap.
void spk_decimate(const QVector<double>& v, int cols, QVector<int>& out) {
    const int n = static_cast<int>(v.size());
    long long cur = LLONG_MIN;
    int first = -1, last = -1, mn = -1, mx = -1;
    auto flush = [&]() {
        if (first < 0)
            return;
        int buf[4] = {first, mn, mx, last};
        std::sort(buf, buf + 4);
        int prev = -1;
        for (int k : buf) {
            if (k != prev)
                out.append(k);
            prev = k;
        }
        first = -1;
    };
    bool in_gap = false;
    for (int i = 0; i < n; ++i) {
        const double y = v[i];
        if (!std::isfinite(y)) {
            flush();
            if (!in_gap)
                out.append(i);
            in_gap = true;
            continue;
        }
        in_gap = false;
        const long long col = static_cast<long long>(i) * cols / n;
        if (first < 0 || col != cur) {
            flush();
            cur = col;
            first = last = mn = mx = i;
        } else {
            last = i;
            if (y < v[mn])
                mn = i;
            if (y > v[mx])
                mx = i;
        }
    }
    flush();
}

} // namespace

Sparkline::Sparkline(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        invalidate();
        updateGeometry();
    });
}

void Sparkline::set_data(const QVector<double>& values) {
    data_ = values;
    invalidate();
}

void Sparkline::set_baseline(double value) {
    style_.baseline = value;
    invalidate();
}

void Sparkline::clear_baseline() {
    style_.baseline.reset();
    invalidate();
}

void Sparkline::set_spark_style(const SparklineStyle& style) {
    style_ = style;
    invalidate();
}

void Sparkline::set_color_mode(SparklineStyle::ColorMode mode, int palette_index) {
    style_.color_mode = mode;
    style_.palette_index = palette_index;
    invalidate();
}

void Sparkline::set_fill(bool on) {
    style_.fill = on;
    invalidate();
}

void Sparkline::set_last_dot(bool on) {
    style_.last_dot = on;
    invalidate();
}

void Sparkline::set_min_max_dots(bool on) {
    style_.min_max_dots = on;
    invalidate();
}

void Sparkline::set_line_width(qreal w) {
    style_.line_width = std::max<qreal>(0.5, w);
    invalidate();
}

void Sparkline::set_opaque(bool on) {
    opaque_ = on;
    setAttribute(Qt::WA_OpaquePaintEvent, on);
    invalidate();
}

QSize Sparkline::sizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 14, fm.height() * 3 / 2};
}

QSize Sparkline::minimumSizeHint() const {
    const QFontMetrics fm(chartkit::axis_font());
    return {fm.averageCharWidth() * 4, fm.height() / 2 + 4};
}

void Sparkline::invalidate() {
    dirty_ = true;
    update();
}

void Sparkline::paintEvent(QPaintEvent*) {
    if (dirty_ || !chartkit::layer_valid(cache_, this)) {
        cache_ = chartkit::make_layer(this, opaque_ ? chartkit::surface() : QColor(Qt::transparent));
        if (!cache_.isNull()) {
            QPainter cp(&cache_);
            paint(cp, QRectF(rect()), data_, style_);
        }
        dirty_ = false;
    }
    if (cache_.isNull())
        return;
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void Sparkline::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void Sparkline::changeEvent(QEvent* e) {
    if (e->type() == QEvent::EnabledChange || e->type() == QEvent::PaletteChange)
        invalidate();
    QWidget::changeEvent(e);
}

QColor Sparkline::resolve_color(const QVector<double>& values, const SparklineStyle& style) {
    using M = SparklineStyle::ColorMode;
    switch (style.color_mode) {
        case M::Accent:
            return chartkit::accent();
        case M::Positive:
            return chartkit::up();
        case M::Negative:
            return chartkit::down();
        case M::Muted:
            return chartkit::text_dim();
        case M::Palette:
            return chartkit::role_color(ChartColorRole::Palette, style.palette_index);
        case M::Custom:
            return chartkit::role_color(ChartColorRole::Custom, style.palette_index, style.custom_color);
        case M::Auto:
            break;
    }
    double first = std::numeric_limits<double>::quiet_NaN();
    double last = first;
    for (double v : values) {
        if (!std::isfinite(v))
            continue;
        if (!std::isfinite(first))
            first = v;
        last = v;
    }
    const double ref = style.baseline.has_value() ? *style.baseline : first;
    if (!std::isfinite(last) || !std::isfinite(ref))
        return chartkit::axis_text();
    bool is_up = last > ref;
    bool is_down = last < ref;
    if (style.invert)
        std::swap(is_up, is_down);
    if (is_up)
        return chartkit::up();
    if (is_down)
        return chartkit::down();
    return chartkit::axis_text();
}

void Sparkline::paint(QPainter& p, const QRectF& r, const QVector<double>& values, const SparklineStyle& style) {
    const int n = static_cast<int>(values.size());
    if (n == 0 || r.width() < 2 || r.height() < 2)
        return;

    double lo = std::numeric_limits<double>::infinity();
    double hi = -lo;
    int finite = 0, last_i = -1, min_i = -1, max_i = -1;
    for (int i = 0; i < n; ++i) {
        const double v = values[i];
        if (!std::isfinite(v))
            continue;
        ++finite;
        last_i = i;
        if (v < lo) {
            lo = v;
            min_i = i;
        }
        if (v > hi) {
            hi = v;
            max_i = i;
        }
    }
    if (finite == 0)
        return;
    const bool has_base = style.baseline.has_value() && std::isfinite(*style.baseline);
    if (has_base && style.show_baseline) {
        lo = std::min(lo, *style.baseline);
        hi = std::max(hi, *style.baseline);
    }
    if (hi - lo < 1e-12) {
        const double pad = std::max(std::abs(hi) * 0.01, 1e-9);
        lo -= pad;
        hi += pad;
    }

    const QColor color = resolve_color(values, style);
    const qreal lw = std::max<qreal>(0.5, style.line_width);
    const qreal dot_r = lw * 1.4 + 0.6;
    const qreal pad = (style.last_dot || style.min_max_dots) ? dot_r + 0.5 : lw;
    QRectF in = r.adjusted(pad, pad, -pad, -pad);
    if (in.width() < 1 || in.height() < 1)
        in = r;

    auto px = [&](int i) { return n == 1 ? in.center().x() : in.left() + in.width() * i / (n - 1); };
    auto py = [&](double v) { return in.bottom() - (v - lo) / (hi - lo) * in.height(); };

    QVector<int> idx;
    const int cols = std::max(1, static_cast<int>(std::ceil(in.width())));
    if (n > cols * 2) {
        idx.reserve(cols * 4);
        spk_decimate(values, cols, idx);
    } else {
        idx.resize(n);
        for (int i = 0; i < n; ++i)
            idx[i] = i;
    }

    QVector<QPolygonF> segs;
    QPolygonF cur;
    for (int i : idx) {
        const double v = values[i];
        if (!std::isfinite(v)) {
            if (!cur.isEmpty())
                segs.append(cur);
            cur.clear();
            continue;
        }
        cur << QPointF(px(i), py(v));
    }
    if (!cur.isEmpty())
        segs.append(cur);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    if (has_base && style.show_baseline) {
        QPen guide(chartkit::with_alpha(chartkit::text_dim(), 0.7), 1, Qt::DotLine);
        p.setPen(guide);
        const qreal by = py(*style.baseline);
        p.drawLine(QPointF(in.left(), by), QPointF(in.right(), by));
    }

    if (style.fill) {
        const qreal base_y = has_base ? std::clamp(py(*style.baseline), in.top(), in.bottom()) : in.bottom();
        QLinearGradient grad(QPointF(0, in.top()), QPointF(0, in.bottom()));
        grad.setColorAt(0.0, chartkit::with_alpha(color, 0.30));
        grad.setColorAt(1.0, chartkit::with_alpha(color, 0.03));
        p.setRenderHint(QPainter::Antialiasing, false); // the line on top covers the edge
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        for (const auto& seg : segs) {
            if (seg.size() >= 2)
                p.drawPolygon(chartkit::fill_envelope(seg, base_y, chartkit::EnvelopeMode::Farthest));
        }
        p.setRenderHint(QPainter::Antialiasing, true);
    }

    const QPen pen(color, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto& seg : segs)
        chartkit::draw_series_line(p, seg, pen);

    p.setPen(Qt::NoPen);
    if (style.min_max_dots && finite >= 3 && min_i != max_i) {
        const qreal rr = dot_r * 0.8;
        p.setBrush(chartkit::up());
        p.drawEllipse(QPointF(px(max_i), py(values[max_i])), rr, rr);
        p.setBrush(chartkit::down());
        p.drawEllipse(QPointF(px(min_i), py(values[min_i])), rr, rr);
    }
    if (style.last_dot && last_i >= 0) {
        p.setBrush(color);
        p.drawEllipse(QPointF(px(last_i), py(values[last_i])), dot_r, dot_r);
    }
    p.restore();
}

} // namespace fincept::ui
