#include "ui/charts/BarChart.h"

#include "ui/theme/Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {
constexpr qreal kBarMaxThickness = 56.0;
constexpr qreal kBarGroupFill = 0.72; // share of a category band taken by its bars
} // namespace

BarChart::BarChart(QWidget* parent) : PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BarChart::set_categories(const QStringList& categories) {
    categories_ = categories;
    hover_ = -1;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_series(const QVector<BarSeries>& series) {
    series_ = series;
    hover_ = -1;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_data(const QStringList& categories, const QVector<double>& values, const QString& name) {
    categories_ = categories;
    set_series({BarSeries{name, values, QColor()}});
}

void BarChart::clear() {
    categories_.clear();
    set_series({});
}

void BarChart::set_mode(Mode mode) {
    mode_ = mode;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_orientation(Qt::Orientation orientation) {
    orientation_ = orientation;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_signed_colors(bool on) {
    signed_ = on;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_show_values(bool on) {
    show_values_ = on;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_waterfall_total(bool show, const QString& label) {
    waterfall_total_ = show;
    total_label_ = label;
    bars_for_ = QRectF();
    invalidate();
}

void BarChart::set_show_legend(bool on) {
    legend_mode_ = on ? 1 : 0;
    bars_for_ = QRectF();
    invalidate();
}

bool BarChart::has_data() const {
    for (const BarSeries& s : series_) {
        for (double v : s.values) {
            if (std::isfinite(v))
                return true;
        }
    }
    return false;
}

int BarChart::category_count() const {
    int n = static_cast<int>(categories_.size());
    for (const BarSeries& s : series_)
        n = std::max(n, static_cast<int>(s.values.size()));
    if (mode_ == Mode::Waterfall && waterfall_total_ && n > 0)
        ++n;
    return n;
}

QString BarChart::category_label(int c) const {
    const int base = static_cast<int>(categories_.size());
    const int data_n = category_count() - ((mode_ == Mode::Waterfall && waterfall_total_) ? 1 : 0);
    if (mode_ == Mode::Waterfall && waterfall_total_ && c == data_n)
        return total_label_.isEmpty() ? tr("Total") : total_label_;
    return c < base ? categories_.at(c) : QString::number(c + 1);
}

QColor BarChart::series_color(int s) const {
    if (s >= 0 && s < series_.size() && series_.at(s).color.isValid())
        return series_.at(s).color;
    return colors::chart_color(s);
}

BarChart::Frame BarChart::frame(const QRectF& r) const {
    Frame f;
    f.n_cat = category_count();
    const int data_n = f.n_cat - ((mode_ == Mode::Waterfall && waterfall_total_ && f.n_cat > 0) ? 1 : 0);

    double lo = 0;
    double hi = 0;
    if (mode_ == Mode::Waterfall) {
        double cum = 0;
        const QVector<double> vals = series_.isEmpty() ? QVector<double>{} : series_.first().values;
        for (int c = 0; c < data_n && c < vals.size(); ++c) {
            if (std::isfinite(vals[c]))
                cum += vals[c];
            lo = std::min(lo, cum);
            hi = std::max(hi, cum);
        }
    } else if (mode_ == Mode::Stacked) {
        for (int c = 0; c < data_n; ++c) {
            double pos = 0;
            double neg = 0;
            for (const BarSeries& s : series_) {
                const double v = c < s.values.size() ? s.values[c] : std::nan("");
                if (!std::isfinite(v))
                    continue;
                (v >= 0 ? pos : neg) += v;
            }
            lo = std::min(lo, neg);
            hi = std::max(hi, pos);
        }
    } else {
        for (const BarSeries& s : series_) {
            for (double v : s.values) {
                if (std::isfinite(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
        }
    }

    const QFontMetricsF lfm(chart_style::font(-3));
    const QFontMetricsF afm(chart_style::axis_font());
    const bool legend = legend_mode_ == 1 || (legend_mode_ == -1 && series_.size() >= 2 && mode_ != Mode::Waterfall);
    const qreal legend_h = legend ? lfm.height() + 8 : 0;
    if (legend)
        f.legend = QRectF(r.left(), r.top(), r.width(), lfm.height());

    const bool vertical = orientation_ == Qt::Vertical;
    const qreal value_span_guess = vertical ? r.height() - legend_h - 30 : r.width() - 120;
    const int target = std::max(2, static_cast<int>(value_span_guess / (vertical ? 45.0 : 90.0)));
    f.ticks = chart_style::nice_ticks(lo, hi, target, &f.lo, &f.hi);
    const auto vfmt = [this](double v) { return format_value(v); };

    if (vertical) {
        const qreal left = chart_style::axis_label_width(f.ticks, vfmt) + 10;
        const qreal top = legend_h + (show_values_ ? afm.height() + 4 : 6);
        const qreal bottom = lfm.height() + 8;
        f.plot = QRectF(r.left() + left, r.top() + top, std::max<qreal>(10, r.width() - left - 10),
                        std::max<qreal>(10, r.height() - top - bottom));
        f.band = f.n_cat > 0 ? f.plot.width() / f.n_cat : f.plot.width();
        qreal widest = 0;
        for (int c = 0; c < f.n_cat; ++c)
            widest = std::max(widest, lfm.horizontalAdvance(category_label(c)));
        const qreal min_label = std::min<qreal>(widest + 8, 44);
        f.label_step = f.band >= min_label ? 1 : std::max(1, qCeil(min_label / std::max<qreal>(f.band, 1)));
    } else {
        qreal widest = 0;
        for (int c = 0; c < f.n_cat; ++c)
            widest = std::max(widest, lfm.horizontalAdvance(category_label(c)));
        const qreal left = std::min<qreal>(widest + 10, r.width() * 0.35);
        qreal right = 12;
        if (show_values_) {
            qreal vw = 0;
            for (const BarSeries& s : series_)
                for (double v : s.values)
                    if (std::isfinite(v))
                        vw = std::max(vw, afm.horizontalAdvance(format_value(v)));
            right = vw + 10;
        }
        const qreal top = legend_h + 4;
        const qreal bottom = afm.height() + 8;
        f.plot = QRectF(r.left() + left, r.top() + top, std::max<qreal>(10, r.width() - left - right),
                        std::max<qreal>(10, r.height() - top - bottom));
        f.band = f.n_cat > 0 ? f.plot.height() / f.n_cat : f.plot.height();
        f.label_step = f.band >= lfm.height() ? 1 : std::max(1, qCeil(lfm.height() / std::max<qreal>(f.band, 1)));
    }
    return f;
}

void BarChart::layout_bars(const Frame& f, QVector<Bar>* bars) const {
    bars->clear();
    const bool vertical = orientation_ == Qt::Vertical;
    const auto vmap = [&f, vertical](double v) {
        const double k = (v - f.lo) / (f.hi - f.lo);
        return vertical ? f.plot.bottom() - k * f.plot.height() : f.plot.left() + k * f.plot.width();
    };
    const auto make_rect = [&](qreal cat_start, qreal thickness, double from, double to) {
        const qreal a = vmap(from);
        const qreal b = vmap(to);
        qreal lo_px = std::min(a, b);
        qreal len = std::max<qreal>(1.0, std::abs(b - a));
        if (vertical)
            return QRectF(cat_start, lo_px, thickness, len);
        return QRectF(lo_px, cat_start, len, thickness);
    };
    const qreal band_origin = vertical ? f.plot.left() : f.plot.top();
    const QColor up(colors::POSITIVE());
    const QColor down(colors::NEGATIVE());
    const int ns = static_cast<int>(series_.size());
    const int data_n = f.n_cat - ((mode_ == Mode::Waterfall && waterfall_total_ && f.n_cat > 0) ? 1 : 0);

    if (mode_ == Mode::Waterfall) {
        if (series_.isEmpty())
            return;
        const QVector<double>& vals = series_.first().values;
        const qreal t = std::min(f.band * kBarGroupFill, kBarMaxThickness);
        double cum = 0;
        for (int c = 0; c < data_n; ++c) {
            const double v = c < vals.size() ? vals[c] : std::nan("");
            if (!std::isfinite(v))
                continue;
            Bar b;
            b.series = 0;
            b.category = c;
            b.from = cum;
            b.to = cum + v;
            b.value = v;
            b.color = v >= 0 ? up : down;
            b.rect = make_rect(band_origin + c * f.band + (f.band - t) / 2, t, b.from, b.to);
            bars->append(b);
            cum += v;
        }
        if (waterfall_total_) {
            Bar b;
            b.series = 0;
            b.category = data_n;
            b.from = 0;
            b.to = cum;
            b.value = cum;
            b.color = colors::chart_color(2); // a total is not a direction
            b.rect = make_rect(band_origin + data_n * f.band + (f.band - t) / 2, t, 0, cum);
            bars->append(b);
        }
        return;
    }

    if (mode_ == Mode::Stacked) {
        const qreal t = std::min(f.band * kBarGroupFill, kBarMaxThickness);
        for (int c = 0; c < data_n; ++c) {
            double pos = 0;
            double neg = 0;
            for (int s = 0; s < ns; ++s) {
                const double v = c < series_[s].values.size() ? series_[s].values[c] : std::nan("");
                if (!std::isfinite(v) || v == 0)
                    continue;
                Bar b;
                b.series = s;
                b.category = c;
                b.value = v;
                double& acc = v >= 0 ? pos : neg;
                b.from = acc;
                b.to = acc + v;
                acc += v;
                b.color = series_color(s);
                b.rect = make_rect(band_origin + c * f.band + (f.band - t) / 2, t, b.from, b.to);
                bars->append(b);
            }
        }
        return;
    }

    // Grouped.
    const qreal gap = ns > 1 ? 2.0 : 0.0;
    const qreal group = std::min(f.band * kBarGroupFill, ns * kBarMaxThickness + (ns - 1) * gap);
    const qreal t = std::max<qreal>(1.0, (group - (ns - 1) * gap) / std::max(1, ns));
    for (int c = 0; c < data_n; ++c) {
        for (int s = 0; s < ns; ++s) {
            const double v = c < series_[s].values.size() ? series_[s].values[c] : std::nan("");
            if (!std::isfinite(v))
                continue;
            Bar b;
            b.series = s;
            b.category = c;
            b.from = 0;
            b.to = v;
            b.value = v;
            b.color = (signed_ && ns == 1) ? (v >= 0 ? up : down) : series_color(s);
            const qreal start = band_origin + c * f.band + (f.band - group) / 2 + s * (t + gap);
            b.rect = make_rect(start, t, 0, v);
            bars->append(b);
        }
    }
}

void BarChart::paint_chart(QPainter& p, const QRectF& r) {
    const Frame f = frame(r);
    layout_bars(f, &bars_);
    bars_for_ = r;
    const bool vertical = orientation_ == Qt::Vertical;
    const auto vfmt = [this](double v) { return format_value(v); };

    chart_style::draw_value_axis(p, f.plot, f.ticks, f.lo, f.hi, vertical ? Qt::Vertical : Qt::Horizontal, vfmt);
    // Baseline at 0 (or at the low end when 0 is outside).
    {
        const double base = qBound(f.lo, 0.0, f.hi);
        const double k = (base - f.lo) / (f.hi - f.lo);
        p.setPen(QPen(chart_style::axis_color(), 1));
        if (vertical) {
            const qreal y = std::round(f.plot.bottom() - k * f.plot.height()) + 0.5;
            p.drawLine(QPointF(f.plot.left(), y), QPointF(f.plot.right(), y));
        } else {
            const qreal x = std::round(f.plot.left() + k * f.plot.width()) + 0.5;
            p.drawLine(QPointF(x, f.plot.top()), QPointF(x, f.plot.bottom()));
        }
    }

    // Waterfall connectors.
    if (mode_ == Mode::Waterfall && bars_.size() > 1) {
        p.setPen(QPen(chart_style::muted_color(), 1, Qt::DashLine));
        for (int i = 0; i + 1 < bars_.size(); ++i) {
            const Bar& a = bars_.at(i);
            const Bar& b = bars_.at(i + 1);
            const double level = a.to;
            const double k = (level - f.lo) / (f.hi - f.lo);
            if (vertical) {
                const qreal y = std::round(f.plot.bottom() - k * f.plot.height()) + 0.5;
                p.drawLine(QPointF(a.rect.right(), y), QPointF(b.rect.left(), y));
            } else {
                const qreal x = std::round(f.plot.left() + k * f.plot.width()) + 0.5;
                p.drawLine(QPointF(x, a.rect.bottom()), QPointF(x, b.rect.top()));
            }
        }
    }

    // Bars.
    p.setPen(Qt::NoPen);
    for (const Bar& b : bars_) {
        QRectF rc = b.rect;
        if (mode_ == Mode::Stacked) // 1 px surface gap between stacked segments
            rc = vertical ? rc.adjusted(0, 0.5, 0, -0.5) : rc.adjusted(0.5, 0, -0.5, 0);
        p.setBrush(b.color);
        const qreal thick = vertical ? rc.width() : rc.height();
        if (thick >= 6 && mode_ != Mode::Stacked)
            p.drawRoundedRect(rc, 2, 2);
        else
            p.drawRect(rc);
    }

    // Value labels.
    if (show_values_) {
        const QFont af = chart_style::axis_font();
        const QFontMetricsF afm(af);
        p.setFont(af);
        for (const Bar& b : bars_) {
            const QString text = format_value(b.value); // delta, segment, bar or total value
            const qreal tw = afm.horizontalAdvance(text);
            if (mode_ == Mode::Stacked) {
                const bool fits = vertical ? (b.rect.height() >= afm.height() && b.rect.width() >= tw + 4)
                                           : (b.rect.width() >= tw + 6 && b.rect.height() >= afm.height());
                if (!fits)
                    continue;
                p.setPen(colors::contrast_text_on(b.color));
                p.drawText(b.rect, Qt::AlignCenter, text);
                continue;
            }
            const bool positive_end = b.to >= b.from;
            p.setPen(chart_style::axis_text_color());
            if (vertical) {
                const qreal y = positive_end ? b.rect.top() - afm.height() - 1 : b.rect.bottom() + 1;
                p.drawText(QRectF(b.rect.center().x() - tw / 2 - 2, y, tw + 4, afm.height()), Qt::AlignCenter, text);
            } else {
                const qreal x = positive_end ? b.rect.right() + 4 : b.rect.left() - tw - 4;
                p.drawText(QRectF(x, b.rect.center().y() - afm.height() / 2, tw + 1, afm.height()),
                           Qt::AlignLeft | Qt::AlignVCenter, text);
            }
        }
    }

    // Category labels.
    const QFont lf = chart_style::font(-3);
    const QFontMetricsF lfm(lf);
    p.setFont(lf);
    p.setPen(chart_style::axis_text_color());
    for (int c = 0; c < f.n_cat; c += f.label_step) {
        const QString label = category_label(c);
        if (vertical) {
            const qreal w = f.band * f.label_step;
            const qreal cx = f.plot.left() + (c + 0.5) * f.band;
            p.drawText(QRectF(cx - w / 2, f.plot.bottom() + 4, w, lfm.height()), Qt::AlignHCenter | Qt::AlignTop,
                       lfm.elidedText(label, Qt::ElideRight, w - 2));
        } else {
            const qreal cy = f.plot.top() + (c + 0.5) * f.band;
            const qreal w = f.plot.left() - r.left() - 8;
            p.drawText(QRectF(r.left(), cy - lfm.height() / 2, w, lfm.height()), Qt::AlignRight | Qt::AlignVCenter,
                       lfm.elidedText(label, Qt::ElideRight, w));
        }
    }

    // Legend.
    if (!f.legend.isEmpty()) {
        qreal x = f.legend.left() + (vertical ? f.plot.left() - r.left() : 0);
        for (int s = 0; s < series_.size(); ++s) {
            const QString name = series_.at(s).name.isEmpty() ? tr("Series %1").arg(s + 1) : series_.at(s).name;
            const qreal need = lfm.height() * 0.6 + 5 + lfm.horizontalAdvance(name);
            if (x + need > f.legend.right())
                break;
            x += chart_style::draw_swatch_label(p, QPointF(x, f.legend.top()), series_color(s), name,
                                                chart_style::label_color()) +
                 14;
        }
    }
}

int BarChart::bar_at(const QPointF& pos) const {
    for (int i = 0; i < bars_.size(); ++i) {
        const QRectF& rc = bars_.at(i).rect;
        // A bigger hit target than the mark for thin bars.
        const QRectF hit = orientation_ == Qt::Vertical ? rc.adjusted(-1, -3, 1, 3) : rc.adjusted(-3, -1, 3, 1);
        if (hit.contains(pos))
            return i;
    }
    return -1;
}

void BarChart::paint_overlay(QPainter& p, const QRectF&) {
    if (hover_ < 0 || hover_ >= bars_.size())
        return;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(chart_style::highlight_color(), 1.5));
    p.drawRect(bars_.at(hover_).rect.adjusted(-1, -1, 1, 1));
}

void BarChart::mouseMoveEvent(QMouseEvent* e) {
    if (!chart_visible())
        return;
    if (bars_for_ != content_rect()) {
        const Frame f = frame(content_rect());
        layout_bars(f, &bars_);
        bars_for_ = content_rect();
    }
    const QPointF pos = e->position();
    const int hit = bar_at(pos);
    if (hit != hover_) {
        hover_ = hit;
        if (hit >= 0)
            emit bar_hovered(bars_.at(hit).series, bars_.at(hit).category);
        else
            emit bar_hovered(-1, -1);
        update();
    }
    if (hit < 0) {
        hide_tip();
        return;
    }
    const Bar& b = bars_.at(hit);
    QVector<QPair<QString, QString>> rows;
    if (mode_ == Mode::Waterfall) {
        const bool total = waterfall_total_ && b.category == category_count() - 1;
        if (total) {
            rows.append({tr("Total"), format_value(b.to)});
        } else {
            rows.append({tr("Change"), format_value(b.value)});
            rows.append({tr("Running total"), format_value(b.to)});
        }
    } else if (series_.size() == 1) {
        rows.append({series_.first().name.isEmpty() ? tr("Value") : series_.first().name, format_value(b.value)});
    } else {
        for (int s = 0; s < series_.size(); ++s) {
            const double v = b.category < series_[s].values.size() ? series_[s].values[b.category] : std::nan("");
            if (!std::isfinite(v))
                continue;
            const QString name = series_[s].name.isEmpty() ? tr("Series %1").arg(s + 1) : series_[s].name;
            rows.append({(s == b.series ? QStringLiteral("> ") : QString()) + name, format_value(v)});
        }
    }
    show_tip(pos.toPoint(), chart_style::tooltip_html(category_label(b.category), rows), b.rect.toAlignedRect());
}

void BarChart::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && chart_visible()) {
        const int hit = bar_at(e->position());
        if (hit >= 0)
            emit bar_clicked(bars_.at(hit).series, bars_.at(hit).category);
    }
    PaintedChart::mouseReleaseEvent(e);
}

void BarChart::on_hover_left() {
    if (hover_ >= 0)
        emit bar_hovered(-1, -1);
    hover_ = -1;
}

} // namespace fincept::ui
