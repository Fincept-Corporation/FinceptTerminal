// src/screens/equity_research/EquityFinancialsCharts.cpp — painted charts of the Financials tab
// (see the header). Every colour and font is read from the live theme while painting; every size
// is derived from font metrics, so the charts hold at base font 9 … 24 px.
#include "screens/equity_research/EquityFinancialsCharts.h"

#include "ui/charts/Sparkline.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QHash>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPair>
#include <QPen>
#include <QPolygonF>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace fincept::screens::fin_charts {

namespace {

namespace fc_ck = fincept::ui::chartkit;
namespace fc_cs = fincept::ui::chart_style;
namespace fc_col = fincept::ui::colors;

using FcFmt = std::function<QString(double)>;
using FcRows = QVector<fincept::ui::chartkit::TooltipRow>;

/// Categorical slot of start / subtotal / end bars of a bridge (a blue that the theme re-hues when
/// blue is the "up" colour of the direction palette).
constexpr int kFcTotalSlot = 4;

bool fc_finite(double v) {
    return std::isfinite(v);
}

double fc_at(const QVector<double>& v, int i) {
    return (i >= 0 && i < v.size()) ? v[i] : kChartNaN;
}

QString fc_minus() {
    return QString(QChar(0x2212));
}

QString fc_dash() {
    return QString(QChar(0x2014));
}

/// ▲ for +1, ▼ for −1, empty for 0.
QString fc_arrow(int sign) {
    if (sign > 0)
        return QString(QChar(0x25B2));
    if (sign < 0)
        return QString(QChar(0x25BC));
    return QString();
}

/// "+$1.2B" / "−$4.1B": the sign is always written, the magnitude comes from `fmt`.
QString fc_signed(double v, const FcFmt& fmt) {
    if (!fc_finite(v))
        return fc_dash();
    const QString mag = fmt(std::abs(v));
    if (v > 0)
        return QStringLiteral("+") + mag;
    if (v < 0)
        return fc_minus() + mag;
    return mag;
}

QString fc_percent(double pct, int decimals) {
    if (!fc_finite(pct))
        return fc_dash();
    return QLocale().toString(pct, 'f', decimals) + QLatin1Char('%');
}

/// "$100.0B" → "$100B". `ok` is false when the label has a non-zero fraction.
QString fc_strip_zero_fraction(const QString& s, bool* ok) {
    *ok = true;
    const QString dp = QLocale().decimalPoint();
    if (dp.isEmpty())
        return s;
    const int i = static_cast<int>(s.lastIndexOf(dp));
    if (i <= 0 || !s.at(i - 1).isDigit())
        return s;
    int j = i + static_cast<int>(dp.size());
    int digits = 0;
    bool zeros = true;
    while (j < s.size() && s.at(j).isDigit()) {
        if (s.at(j) != QLatin1Char('0'))
            zeros = false;
        ++j;
        ++digits;
    }
    if (digits == 0)
        return s;
    if (!zeros) {
        *ok = false;
        return s;
    }
    return s.left(i) + s.mid(j);
}

/// Axis labels by `fmt`, without an all-zero fraction when every tick has one ("$100B", not
/// "$100.0B"); unchanged when any tick needs its decimals.
FcFmt fc_tick_formatter(const QVector<double>& ticks, const FcFmt& fmt) {
    bool all = !ticks.isEmpty();
    for (double t : ticks) {
        bool ok = false;
        fc_strip_zero_fraction(fmt(t), &ok);
        if (!ok) {
            all = false;
            break;
        }
    }
    if (!all)
        return fmt;
    return [fmt](double v) {
        bool ok = false;
        return fc_strip_zero_fraction(fmt(v), &ok);
    };
}

/// Nice ticks over [lo, hi] for an axis `length` px long with about `spacing` px between ticks.
QVector<double> fc_nice(double lo, double hi, qreal length, qreal spacing, double* nlo, double* nhi) {
    const int target = std::max(2, static_cast<int>(length / std::max<qreal>(spacing, 1.0)) + 1);
    return fc_cs::nice_ticks(lo, hi, target, nlo, nhi);
}

/// Ticks for a secondary axis that share the primary axis' gridlines: exactly `intervals` steps of
/// a nice size, starting at a multiple of the step, covering [lo, hi].
QVector<double> fc_aligned_ticks(double lo, double hi, int intervals, double* out_lo, double* out_hi) {
    intervals = std::max(1, intervals);
    if (!(hi - lo > 1e-12)) {
        const double pad = std::abs(lo) > 1e-12 ? std::abs(lo) * 0.1 : 1.0;
        lo -= pad;
        hi += pad;
    }
    const double span = hi - lo;
    double e = std::pow(10.0, std::floor(std::log10(span / intervals)));
    static const double kFcMantissas[] = {1.0, 2.0, 2.5, 5.0};
    double step = 0.0;
    double start = lo;
    for (int guard = 0; guard < 24 && step <= 0.0; ++guard) {
        for (double m : kFcMantissas) {
            const double s = m * e;
            const double a = std::floor(lo / s + 1e-9) * s;
            if (a + s * intervals >= hi - span * 1e-9) {
                step = s;
                start = a;
                break;
            }
        }
        e *= 10.0;
    }
    if (step <= 0.0) {
        step = span / intervals;
        start = lo;
    }
    QVector<double> ticks;
    ticks.reserve(intervals + 1);
    for (int k = 0; k <= intervals; ++k) {
        const double v = start + step * k;
        ticks.append(std::abs(v) < step * 1e-9 ? 0.0 : v);
    }
    *out_lo = start;
    *out_hi = start + step * intervals;
    return ticks;
}

/// Integer-edged rectangle (crisp fills), at least 1 px each way.
QRectF fc_snap(const QRectF& r) {
    const qreal l = std::round(r.left());
    const qreal t = std::round(r.top());
    qreal rr = std::round(r.right());
    qreal b = std::round(r.bottom());
    if (rr - l < 1)
        rr = l + 1;
    if (b - t < 1)
        b = t + 1;
    return QRectF(QPointF(l, t), QPointF(rr, b));
}

/// Faint veil over the hovered category / row.
QColor fc_hover_color() {
    return fc_ck::with_alpha(fc_ck::text(), 0.07);
}

/// "LIABILITIES + EQUITY" → "L+E", "Assets" → "A".
QString fc_abbrev(const QString& name) {
    QString out;
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& w : words) {
        if (w.at(0).isLetterOrNumber())
            out += w.at(0).toUpper();
        else
            out += w;
    }
    return out;
}

/// `x` written the way `sample` writes `sample_value`: same prefix, suffix, decimals and scale
/// ("48.8%" for 48.8 → "41.2%" for 41.2; "0.71x" → "2.40x"). Falls back to `fallback`.
QString fc_format_like(const QString& sample, double sample_value, double x, const FcFmt& fallback) {
    if (!fc_finite(x))
        return fc_dash();
    const QLocale loc;
    const QString dp = loc.decimalPoint();
    const QString gs = loc.groupSeparator();
    const int dpn = static_cast<int>(dp.size());
    const int gsn = static_cast<int>(gs.size());
    const int len = static_cast<int>(sample.size());
    int start = -1;
    for (int i = 0; i < len; ++i) {
        if (sample.at(i).isDigit()) {
            start = i;
            break;
        }
    }
    if (start < 0 || !fc_finite(sample_value))
        return fallback(x);
    int end = start;
    int decimals = 0;
    bool in_frac = false;
    while (end < len) {
        const QChar c = sample.at(end);
        if (c.isDigit()) {
            if (in_frac)
                ++decimals;
            ++end;
            continue;
        }
        if (!in_frac && dpn > 0 && sample.mid(end, dpn) == dp && end + dpn < len && sample.at(end + dpn).isDigit()) {
            in_frac = true;
            end += dpn;
            continue;
        }
        if (!in_frac && gsn > 0 && sample.mid(end, gsn) == gs && end + gsn < len && sample.at(end + gsn).isDigit()) {
            end += gsn;
            continue;
        }
        break;
    }
    bool ok = false;
    const double parsed = loc.toDouble(sample.mid(start, end - start), &ok);
    if (!ok)
        return fallback(x);
    QString prefix = sample.left(start);
    prefix.remove(QLatin1Char('-'));
    prefix.remove(QLatin1Char('+'));
    prefix.remove(QChar(0x2212));
    const QString suffix = sample.mid(end);
    double scale = 1.0;
    if (parsed != 0.0 && sample_value != 0.0) {
        const double lg = std::log10(std::abs(sample_value) / parsed);
        const double e = std::round(lg);
        if (std::abs(lg - e) > 0.02)
            return fallback(x);
        scale = std::pow(10.0, e);
    }
    const double y = x / scale;
    const QString num = loc.toString(std::abs(y), 'f', decimals);
    const bool neg = y < 0 && loc.toDouble(num) != 0.0;
    return (neg ? fc_minus() : QString()) + prefix + num + suffix;
}

// ── Legend ──────────────────────────────────────────────────────────────────────────────────

enum class FcSwatch { Box, Line, Dashed, Heading, Tick };

struct FcLegendItem {
    QString text;
    QColor color;
    FcSwatch kind = FcSwatch::Box;
};

struct FcLegendLayout {
    QVector<QRectF> rects; ///< per item; an empty rect = not shown
    qreal height = 0;
    int hidden = 0;
};

struct FcLegendMetrics {
    QFont font;
    QFont head_font;
    qreal h = 0;
    qreal swatch = 0;
    qreal line_w = 0;
    qreal gap_in = 0;
    qreal gap_items = 0;
    qreal row_gap = 0;
};

FcLegendMetrics fc_legend_metrics() {
    FcLegendMetrics m;
    m.font = fc_cs::font(-3);
    m.head_font = fc_cs::font(-4, QFont::DemiBold);
    const QFontMetricsF fm(m.font);
    m.h = std::ceil(fm.height());
    m.swatch = std::max<qreal>(7.0, std::round(fm.height() * 0.55));
    m.line_w = std::round(fm.height() * 1.25);
    m.gap_in = std::max<qreal>(4.0, std::round(fm.height() * 0.35));
    m.gap_items = std::round(fm.height() * 1.0);
    m.row_gap = std::max<qreal>(2.0, std::round(fm.height() * 0.2));
    return m;
}

qreal fc_legend_item_width(const FcLegendMetrics& m, const FcLegendItem& it) {
    if (it.kind == FcSwatch::Heading)
        return QFontMetricsF(m.head_font).horizontalAdvance(it.text);
    const qreal sw = (it.kind == FcSwatch::Line || it.kind == FcSwatch::Dashed) ? m.line_w : m.swatch;
    return sw + m.gap_in + QFontMetricsF(m.font).horizontalAdvance(it.text);
}

/// Items left to right from (left, top), wrapping inside `width`, at most `max_rows` rows (the rest
/// counted in `hidden`). A heading never ends a row without its first item.
FcLegendLayout fc_layout_legend(const QVector<FcLegendItem>& items, qreal left, qreal top, qreal width, int max_rows) {
    FcLegendLayout out;
    const int n = static_cast<int>(items.size());
    out.rects.resize(n);
    if (n == 0 || width <= 0)
        return out;
    const FcLegendMetrics m = fc_legend_metrics();
    QVector<qreal> w(n);
    for (int i = 0; i < n; ++i)
        w[i] = std::min(width, fc_legend_item_width(m, items[i]));
    int row = 0;
    int last_row = -1;
    qreal x = left;
    for (int i = 0; i < n; ++i) {
        const bool heading = items[i].kind == FcSwatch::Heading;
        qreal need = w[i];
        if (heading && i + 1 < n)
            need += m.gap_in * 1.5 + w[i + 1];
        if (x > left && x + need > left + width + 0.5) {
            ++row;
            x = left;
        }
        if (row >= max_rows) {
            ++out.hidden;
            continue;
        }
        out.rects[i] = QRectF(x, top + row * (m.h + m.row_gap), w[i], m.h);
        last_row = row;
        x += w[i] + (heading ? m.gap_in * 1.5 : m.gap_items);
    }
    const int rows_used = last_row + 1;
    out.height = rows_used > 0 ? rows_used * m.h + (rows_used - 1) * m.row_gap : 0.0;
    return out;
}

void fc_draw_legend(QPainter& p, const QVector<FcLegendItem>& items, const FcLegendLayout& lay) {
    if (lay.rects.size() != items.size())
        return;
    const FcLegendMetrics m = fc_legend_metrics();
    const QFontMetricsF fm(m.font);
    const QFontMetricsF hfm(m.head_font);
    p.save();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const QRectF rc = lay.rects[i];
        if (rc.isEmpty())
            continue;
        const FcLegendItem& it = items[i];
        if (it.kind == FcSwatch::Heading) {
            p.setFont(m.head_font);
            p.setPen(fc_cs::muted_color());
            p.drawText(rc.adjusted(0, 0, 1, 0), Qt::AlignLeft | Qt::AlignVCenter,
                       hfm.elidedText(it.text, Qt::ElideRight, rc.width() + 0.5));
            continue;
        }
        const qreal cy = rc.center().y();
        qreal sw = m.swatch;
        p.setRenderHint(QPainter::Antialiasing, true);
        switch (it.kind) {
            case FcSwatch::Line:
            case FcSwatch::Dashed: {
                sw = m.line_w;
                QPen pen(it.color, 2.0, Qt::SolidLine, Qt::FlatCap);
                if (it.kind == FcSwatch::Dashed) {
                    pen.setStyle(Qt::CustomDashLine);
                    pen.setDashPattern({2.0, 1.5});
                }
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawLine(QPointF(rc.left(), cy), QPointF(rc.left() + sw, cy));
                if (it.kind == FcSwatch::Line) {
                    const qreal dot = std::max<qreal>(2.0, m.swatch * 0.32);
                    p.setPen(Qt::NoPen);
                    p.setBrush(it.color);
                    p.drawEllipse(QPointF(rc.left() + sw / 2.0, cy), dot, dot);
                }
                break;
            }
            case FcSwatch::Tick: {
                const qreal th = std::round(m.swatch * 1.4);
                p.fillRect(QRectF(std::round(rc.left() + m.swatch / 2.0 - 1.0), std::round(cy - th / 2.0), 2.0, th),
                           it.color);
                break;
            }
            case FcSwatch::Box:
            case FcSwatch::Heading:
                p.setPen(Qt::NoPen);
                p.setBrush(it.color);
                p.drawRoundedRect(QRectF(rc.left(), std::round(cy - m.swatch / 2.0), m.swatch, m.swatch), 1.5, 1.5);
                break;
        }
        p.setFont(m.font);
        p.setPen(fc_cs::axis_text_color());
        const qreal tx = rc.left() + sw + m.gap_in;
        const qreal tw = std::max<qreal>(0.0, rc.right() - tx);
        p.drawText(QRectF(tx, rc.top(), tw + 1.0, rc.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(it.text, Qt::ElideRight, tw + 0.5));
    }
    p.restore();
}

// ── Tooltips ────────────────────────────────────────────────────────────────────────────────

/// The offset chartkit::draw_tooltip puts between its anchor and the box.
int fc_tip_offset() {
    return QFontMetrics(fc_ck::tooltip_font()).height();
}

bool fc_tip_fits(const QRect& bounds, const QString& title, const FcRows& rows, const QString& footer) {
    const QSize sz = fc_ck::tooltip_size(title, rows, footer);
    return sz.width() <= bounds.width() && sz.height() <= bounds.height();
}

/// The chartkit tooltip box with its top-left corner at `top_left` (moved inside `bounds`).
void fc_tip_at(QPainter& p, const QRect& bounds, QPoint top_left, const QString& title, const FcRows& rows,
               const QString& footer) {
    const QSize sz = fc_ck::tooltip_size(title, rows, footer);
    top_left.setX(std::clamp(top_left.x(), bounds.left(), std::max(bounds.left(), bounds.right() - sz.width())));
    top_left.setY(std::clamp(top_left.y(), bounds.top(), std::max(bounds.top(), bounds.bottom() - sz.height())));
    const int off = fc_tip_offset();
    fc_ck::draw_tooltip(p, bounds, top_left - QPoint(off, off), title, rows, footer);
}

/// Tooltip beside `avoid` (right of it, else left of it), its top at `top`.
void fc_tip_beside(QPainter& p, const QRect& bounds, const QRect& avoid, int top, const QString& title,
                   const FcRows& rows, const QString& footer) {
    const QSize sz = fc_ck::tooltip_size(title, rows, footer);
    const int gap = std::max(4, fc_tip_offset() / 3);
    int x = avoid.right() + 1 + gap;
    if (x + sz.width() > bounds.right()) {
        const int lx = avoid.left() - gap - sz.width();
        if (lx >= bounds.left())
            x = lx;
        else if (avoid.left() - bounds.left() > bounds.right() - avoid.right())
            x = bounds.left();
    }
    fc_tip_at(p, bounds, QPoint(x, top), title, rows, footer);
}

/// Tooltip below `avoid` (else above it), centred on `x_center`.
void fc_tip_below(QPainter& p, const QRect& bounds, const QRect& avoid, int x_center, const QString& title,
                  const FcRows& rows, const QString& footer) {
    const QSize sz = fc_ck::tooltip_size(title, rows, footer);
    const int gap = std::max(3, fc_tip_offset() / 4);
    int y = avoid.bottom() + 1 + gap;
    if (y + sz.height() > bounds.bottom()) {
        const int uy = avoid.top() - gap - sz.height();
        if (uy >= bounds.top())
            y = uy;
    }
    fc_tip_at(p, bounds, QPoint(x_center - sz.width() / 2, y), title, rows, footer);
}

/// Rich-text version of a tooltip, for the native tooltip when the painted box would not fit.
QString fc_tip_html(const QString& title, const FcRows& rows, const QString& footer) {
    QVector<QPair<QString, QString>> pairs;
    pairs.reserve(rows.size());
    for (const auto& row : rows)
        pairs.append({row.name, row.value});
    QString html = fc_cs::tooltip_html(title, pairs);
    if (!footer.isEmpty()) {
        html += QStringLiteral("<div style='color:%1;margin-top:2px'>%2</div>")
                    .arg(QColor(fc_col::TEXT_TERTIARY()).name(), footer.toHtmlEscaped());
    }
    return html;
}

// ── Waterfall helpers ───────────────────────────────────────────────────────────────────────

struct FcBridge {
    QVector<double> from;  ///< bar start per step (NaN = no value)
    QVector<double> to;    ///< bar end per step
    QVector<double> level; ///< running level after the step
    double start = kChartNaN;
    int start_index = -1;
    bool share = false; ///< the start is the largest level: steps read as a share of it
};

FcBridge fc_bridge(const QVector<FinWaterfallChart::Step>& steps) {
    using Kind = FinWaterfallChart::Kind;
    FcBridge b;
    const int n = static_cast<int>(steps.size());
    b.from.fill(kChartNaN, n);
    b.to.fill(kChartNaN, n);
    b.level.fill(kChartNaN, n);
    double level = 0.0;
    double max_level = 0.0;
    for (int i = 0; i < n; ++i) {
        const FinWaterfallChart::Step& s = steps[i];
        if (fc_finite(s.value)) {
            if (s.kind == Kind::Delta) {
                b.from[i] = level;
                b.to[i] = level + s.value;
                level += s.value;
            } else {
                b.from[i] = 0.0;
                b.to[i] = s.value;
                level = s.value;
                if (s.kind == Kind::Start && b.start_index < 0) {
                    b.start_index = i;
                    b.start = s.value;
                }
            }
        }
        b.level[i] = level;
        max_level = std::max(max_level, std::abs(level));
    }
    b.share = b.start_index >= 0 && b.start != 0.0 && std::abs(b.start) >= max_level * (1.0 - 1e-9);
    return b;
}

QColor fc_step_color(const FinWaterfallChart::Step& s) {
    using Kind = FinWaterfallChart::Kind;
    if (s.kind == Kind::Delta)
        return s.value >= 0 ? fc_ck::up() : fc_ck::down();
    const QColor total = fc_col::chart_color(kFcTotalSlot);
    return s.kind == Kind::Subtotal ? fc_ck::with_alpha(total, 0.68) : total;
}

FcRows fc_waterfall_rows(const QVector<FinWaterfallChart::Step>& steps, int i, const FcFmt& fmt) {
    using Kind = FinWaterfallChart::Kind;
    FcRows rows;
    if (i < 0 || i >= steps.size())
        return rows;
    const FcBridge b = fc_bridge(steps);
    const FinWaterfallChart::Step& s = steps[i];
    if (!fc_finite(s.value)) {
        rows.append({QColor(), FinWaterfallChart::tr("Value"), fc_dash(), QColor()});
        return rows;
    }
    const QColor c = fc_step_color(s);
    if (s.kind == Kind::Delta) {
        rows.append({c, FinWaterfallChart::tr("Change"), fc_signed(s.value, fmt),
                     s.value >= 0 ? fc_ck::up() : fc_ck::down()});
        rows.append({QColor(), FinWaterfallChart::tr("Running total"), fmt(b.level[i]), QColor()});
    } else {
        QString name = FinWaterfallChart::tr("Subtotal");
        if (s.kind == Kind::Start)
            name = FinWaterfallChart::tr("Start");
        else if (s.kind == Kind::End)
            name = FinWaterfallChart::tr("End");
        rows.append({c, name, fmt(s.value), QColor()});
    }
    if (b.share && i != b.start_index) {
        rows.append({QColor(), FinWaterfallChart::tr("Share of %1").arg(steps[b.start_index].label),
                     fc_percent(std::abs(s.value) / std::abs(b.start) * 100.0, 1), QColor()});
    }
    return rows;
}

// ── Stack helpers ───────────────────────────────────────────────────────────────────────────

double fc_side_total(const QVector<FinStackChart::Segment>& segs, int i) {
    double t = 0.0;
    for (const FinStackChart::Segment& s : segs) {
        const double v = fc_at(s.values, i);
        if (fc_finite(v) && v > 0)
            t += v;
    }
    return t;
}

// ── DuPont helpers ──────────────────────────────────────────────────────────────────────────

QColor fc_sign_color(int sign) {
    if (sign > 0)
        return fc_ck::up();
    if (sign < 0)
        return fc_ck::down();
    return fc_cs::muted_color();
}

QString fc_change_text(const FinDupontView::Node& node) {
    if (node.change_text.isEmpty())
        return QString();
    if (node.change_sign == 0)
        return node.change_text;
    return fc_arrow(node.change_sign) + QStringLiteral(" ") + node.change_text;
}

FcRows fc_dupont_rows(const FinDupontView::Node& node) {
    FcRows rows;
    rows.append({QColor(), FinDupontView::tr("Value"), node.value_text, QColor()});
    if (!node.change_text.isEmpty())
        rows.append({QColor(), FinDupontView::tr("Change"), fc_change_text(node), fc_sign_color(node.change_sign)});
    return rows;
}

/// Up to two lines of `text` within `width` (word wrap; the second line elided).
QStringList fc_wrap2(const QFontMetricsF& fm, const QString& text, qreal width) {
    if (fm.horizontalAdvance(text) <= width)
        return {text};
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString first;
    int used = 0;
    for (const QString& w : words) {
        const QString cand = first.isEmpty() ? w : first + QLatin1Char(' ') + w;
        if (fm.horizontalAdvance(cand) > width && !first.isEmpty())
            break;
        first = cand;
        ++used;
    }
    QStringList out;
    out.append(fm.elidedText(first, Qt::ElideRight, width));
    const QString rest = words.mid(used).join(QStringLiteral(" "));
    if (!rest.isEmpty())
        out.append(fm.elidedText(rest, Qt::ElideRight, width));
    return out;
}

// ── Bullet helpers ──────────────────────────────────────────────────────────────────────────

QColor fc_track_color() {
    return fc_ck::with_alpha(fc_ck::text_dim(), 0.08);
}
QColor fc_range_color() {
    return fc_ck::with_alpha(fc_ck::text_dim(), 0.24);
}
QColor fc_iqr_color() {
    return fc_ck::with_alpha(fc_ck::text_dim(), 0.50);
}

/// −1 worse than the peer median, +1 better, 0 in line or unknown.
int fc_bullet_verdict(const FinBulletChart::Row& row) {
    if (!fc_finite(row.value) || !fc_finite(row.median) || row.value == row.median)
        return 0;
    const bool above = row.value > row.median;
    return (above == row.higher_is_better) ? 1 : -1;
}

QColor fc_bullet_color(int verdict) {
    if (verdict > 0)
        return fc_ck::up();
    if (verdict < 0)
        return fc_ck::down();
    return fc_cs::axis_text_color();
}

struct FcBulletMetrics {
    QFont lf; ///< labels
    QFont vf; ///< values
    QFont mf; ///< "vs median" text
    qreal row_h = 0;
    qreal key_h = 0;
};

FcBulletMetrics fc_bullet_metrics() {
    FcBulletMetrics m;
    m.lf = fc_cs::font(-3);
    m.vf = fc_cs::font(-3, QFont::Bold);
    m.mf = fc_cs::font(-4);
    const QFontMetricsF lfm(m.lf);
    const QFontMetricsF vfm(m.vf);
    m.row_h = std::ceil(std::max(lfm.height(), vfm.height()) * 1.85);
    const FcLegendMetrics lm = fc_legend_metrics();
    m.key_h = lm.h + std::round(lm.h * 0.7);
    return m;
}

QVector<FcLegendItem> fc_bullet_key() {
    QVector<FcLegendItem> key;
    key.append({FinBulletChart::tr("Peer range"), fc_range_color(), FcSwatch::Box});
    key.append({FinBulletChart::tr("Peer 25th–75th"), fc_iqr_color(), FcSwatch::Box});
    key.append({FinBulletChart::tr("Peer median"), fc_ck::text(), FcSwatch::Tick});
    key.append({FinBulletChart::tr("Better"), fc_ck::up(), FcSwatch::Box});
    key.append({FinBulletChart::tr("Worse"), fc_ck::down(), FcSwatch::Box});
    return key;
}

FcRows fc_bullet_rows(const FinBulletChart::Row& row, const FcFmt& fallback) {
    FcRows rows;
    const auto like = [&row, &fallback](double v) { return fc_format_like(row.value_text, row.value, v, fallback); };
    const int verdict = fc_bullet_verdict(row);
    const QColor c = fc_bullet_color(verdict);
    rows.append({c, FinBulletChart::tr("This company"), row.value_text.isEmpty() ? like(row.value) : row.value_text, c});
    if (fc_finite(row.hi))
        rows.append({QColor(), FinBulletChart::tr("Peer maximum"), like(row.hi), QColor()});
    if (fc_finite(row.q3))
        rows.append({QColor(), FinBulletChart::tr("Peer 75th percentile"), like(row.q3), QColor()});
    if (fc_finite(row.median) || !row.median_text.isEmpty())
        rows.append({QColor(), FinBulletChart::tr("Peer median"),
                     row.median_text.isEmpty() ? like(row.median) : row.median_text, QColor()});
    if (fc_finite(row.q1))
        rows.append({QColor(), FinBulletChart::tr("Peer 25th percentile"), like(row.q1), QColor()});
    if (fc_finite(row.lo))
        rows.append({QColor(), FinBulletChart::tr("Peer minimum"), like(row.lo), QColor()});
    if (fc_finite(row.value) && fc_finite(row.median)) {
        QString verdict_text = FinBulletChart::tr("In line");
        if (verdict > 0)
            verdict_text = FinBulletChart::tr("Better");
        else if (verdict < 0)
            verdict_text = FinBulletChart::tr("Worse");
        const QString rule =
            row.higher_is_better ? FinBulletChart::tr("higher is better") : FinBulletChart::tr("lower is better");
        rows.append({QColor(), FinBulletChart::tr("vs peer median"),
                     FinBulletChart::tr("%1 (%2)").arg(verdict_text, rule), c});
    }
    return rows;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinChartBase
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinChartBase::FinChartBase(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FinChartBase::set_categories(const QStringList& labels, const QStringList& tooltips) {
    labels_ = labels;
    tips_ = tooltips;
    hover_ = -1;
    if (selected_ >= labels_.size())
        selected_ = -1;
    hide_tip();
    invalidate();
}

void FinChartBase::set_selected(int index) {
    if (index < 0)
        index = -1;
    if (index == selected_)
        return;
    selected_ = index;
    invalidate();
}

void FinChartBase::set_summary(const QString& text) {
    setAccessibleDescription(text);
}

qreal FinChartBase::band_width(const QRectF& plot) const {
    const int n = std::max(1, static_cast<int>(labels_.size()));
    return plot.width() / n;
}

qreal FinChartBase::band_center(const QRectF& plot, int i) const {
    return plot.left() + (i + 0.5) * band_width(plot);
}

int FinChartBase::category_at(const QPointF& pos) const {
    const int n = static_cast<int>(labels_.size());
    if (n <= 0 || plot_.width() <= 0 || !chart_visible())
        return -1;
    if (pos.x() < plot_.left() || pos.x() >= plot_.right())
        return -1;
    if (pos.y() < plot_.top() || pos.y() > content_rect().bottom())
        return -1;
    const int i = static_cast<int>(std::floor((pos.x() - plot_.left()) / band_width(plot_)));
    return std::clamp(i, 0, n - 1);
}

qreal FinChartBase::category_axis_height() const {
    const QFontMetricsF fm(fc_cs::font(-3, QFont::Bold));
    return std::ceil(fm.height()) + std::max<qreal>(3.0, std::round(fm.height() * 0.3)) + 2.0;
}

void FinChartBase::draw_selection(QPainter& p, const QRectF& plot) const {
    if (selected_ < 0 || selected_ >= labels_.size() || plot.width() <= 0)
        return;
    const qreal bw = band_width(plot);
    const QRectF band = fc_snap(QRectF(plot.left() + selected_ * bw, plot.top(), bw, plot.height()));
    const QColor acc = fc_ck::accent();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(band, fc_ck::with_alpha(acc, 0.10));
    p.fillRect(QRectF(band.left(), band.top(), band.width(), 2.0), fc_ck::with_alpha(acc, 0.85));
    p.restore();
}

void FinChartBase::draw_categories(QPainter& p, const QRectF& plot) const {
    const int n = static_cast<int>(labels_.size());
    if (n <= 0 || plot.width() <= 0)
        return;
    const QFont f = fc_cs::font(-3);
    const QFont fb = fc_cs::font(-3, QFont::Bold);
    const QFontMetricsF fm(f);
    const QFontMetricsF fmb(fb);
    const qreal bw = band_width(plot);
    qreal widest = 0;
    for (const QString& l : labels_)
        widest = std::max(widest, fmb.horizontalAdvance(l));
    const qreal need = widest + fm.averageCharWidth() * 1.5;
    int step = 1;
    while (step < n && bw * step < need)
        ++step;
    const bool has_sel = selected_ >= 0 && selected_ < n;
    const qreal y = plot.bottom() + std::max<qreal>(3.0, std::round(fm.height() * 0.3));
    const qreal h = std::ceil(fmb.height());
    p.save();
    for (int i = 0; i < n; ++i) {
        const bool sel = has_sel && i == selected_;
        bool shown = ((n - 1 - i) % step) == 0; // thinned from the latest period back
        if (!sel && has_sel && step > 1 && std::abs(i - selected_) < step)
            shown = false;
        if (!shown && !sel)
            continue;
        const QFontMetricsF& m = sel ? fmb : fm;
        const qreal avail = std::min(bw * step, plot.width());
        const QString text = m.elidedText(labels_.at(i), Qt::ElideRight, avail - 2.0);
        const qreal tw = m.horizontalAdvance(text);
        qreal x = band_center(plot, i) - tw / 2.0;
        x = std::clamp(x, plot.left(), std::max(plot.left(), plot.right() - tw));
        p.setFont(sel ? fb : f);
        p.setPen(sel ? fc_ck::accent() : fc_cs::axis_text_color());
        p.drawText(QRectF(x, y, tw + 1.0, h), Qt::AlignLeft | Qt::AlignTop, text);
    }
    p.restore();
}

void FinChartBase::mouseMoveEvent(QMouseEvent* e) {
    const int i = category_at(e->position());
    if (i == hover_)
        return;
    hover_ = i;
    if (i >= 0) {
        setCursor(Qt::PointingHandCursor);
        const FcRows rows = tooltip_rows(i);
        const QString title = labels_.value(i);
        const QString footer = tips_.value(i);
        if (fc_tip_fits(content_rect().toRect(), title, rows, footer))
            hide_tip();
        else
            show_tip(e->position().toPoint(), fc_tip_html(title, rows, footer));
    } else {
        unsetCursor();
        hide_tip();
    }
    update();
}

void FinChartBase::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        const int i = category_at(e->position());
        if (i >= 0) {
            emit category_clicked(i);
            e->accept();
            return;
        }
    }
    ui::PaintedChart::mouseReleaseEvent(e);
}

void FinChartBase::on_hover_left() {
    hover_ = -1;
    unsetCursor();
}

void FinChartBase::paint_overlay(QPainter& p, const QRectF& r) {
    const int n = static_cast<int>(labels_.size());
    if (hover_ < 0 || hover_ >= n || plot_.width() <= 0)
        return;
    const qreal bw = band_width(plot_);
    const QRectF band = fc_snap(QRectF(plot_.left() + hover_ * bw, plot_.top(), bw, plot_.height()));
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(band, fc_hover_color());
    p.restore();
    const FcRows rows = tooltip_rows(hover_);
    const QString title = labels_.at(hover_);
    const QString footer = tips_.value(hover_);
    const QRect bounds = r.toRect();
    if (!fc_tip_fits(bounds, title, rows, footer))
        return; // shown as a native tooltip instead (mouseMoveEvent)
    fc_tip_beside(p, bounds, band.toRect(), qRound(plot_.top()) + 2, title, rows, footer);
}

QVector<ui::chartkit::TooltipRow> FinChartBase::tooltip_rows(int) const {
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinComboChart
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinComboChart::FinComboChart(QWidget* parent) : FinChartBase(parent) {}

void FinComboChart::set_bars(const QVector<ComboBars>& bars) {
    bars_ = bars;
    hover_ = -1;
    hide_tip();
    invalidate();
}

void FinComboChart::set_lines(const QVector<ComboLine>& lines) {
    lines_ = lines;
    hover_ = -1;
    hide_tip();
    invalidate();
}

void FinComboChart::set_primary_formatter(Formatter f) {
    primary_fmt_ = std::move(f);
    invalidate();
}

void FinComboChart::set_secondary_formatter(Formatter f) {
    secondary_fmt_ = std::move(f);
    invalidate();
}

void FinComboChart::set_reference_line(double value, const QString& label, bool secondary) {
    ref_value_ = value;
    ref_label_ = label;
    ref_secondary_ = secondary;
    invalidate();
}

void FinComboChart::set_show_values(bool on) {
    show_values_ = on;
    invalidate();
}

void FinComboChart::set_zero_based(bool on) {
    zero_based_ = on;
    invalidate();
}

void FinComboChart::clear() {
    bars_.clear();
    lines_.clear();
    ref_value_ = kChartNaN;
    ref_label_.clear();
    labels_.clear();
    tips_.clear();
    hover_ = -1;
    selected_ = -1;
    plot_ = QRectF();
    hide_tip();
    invalidate();
}

bool FinComboChart::has_data() const {
    const int n = static_cast<int>(labels_.size());
    if (n <= 0)
        return false;
    for (const ComboBars& b : bars_)
        for (int i = 0; i < n; ++i)
            if (fc_finite(fc_at(b.values, i)))
                return true;
    for (const ComboLine& l : lines_)
        for (int i = 0; i < n; ++i)
            if (fc_finite(fc_at(l.values, i)))
                return true;
    return false;
}

void FinComboChart::paint_chart(QPainter& p, const QRectF& r) {
    plot_ = QRectF();
    const int n = static_cast<int>(labels_.size());
    if (n <= 0 || r.width() < 40 || r.height() < 40)
        return;

    const FcFmt base_fmt = [this](double v) { return format_value(v); };
    const FcFmt pf = primary_fmt_ ? FcFmt(primary_fmt_) : base_fmt;
    const FcFmt sf = secondary_fmt_ ? FcFmt(secondary_fmt_) : base_fmt;

    // Bar slots: one per stack key (or per series without a key), in order of appearance.
    const int nb = static_cast<int>(bars_.size());
    QVector<int> slot_of(nb, 0);
    QHash<QString, int> key_slot;
    int n_slots = 0;
    for (int s = 0; s < nb; ++s) {
        const QString& key = bars_[s].stack;
        if (key.isEmpty()) {
            slot_of[s] = n_slots++;
            continue;
        }
        const auto it = key_slot.constFind(key);
        if (it == key_slot.constEnd()) {
            key_slot.insert(key, n_slots);
            slot_of[s] = n_slots++;
        } else {
            slot_of[s] = it.value();
        }
    }

    // Value ranges.
    const double inf = std::numeric_limits<double>::infinity();
    double plo = inf, phi = -inf, slo = inf, shi = -inf;
    const auto widen = [](double& lo_ref, double& hi_ref, double v) {
        if (std::isfinite(v)) {
            lo_ref = std::min(lo_ref, v);
            hi_ref = std::max(hi_ref, v);
        }
    };
    bool any_bar = false;
    QVector<double> pos_acc(n_slots, 0.0);
    QVector<double> neg_acc(n_slots, 0.0);
    for (int i = 0; i < n; ++i) {
        pos_acc.fill(0.0);
        neg_acc.fill(0.0);
        bool cat_any = false;
        for (int s = 0; s < nb; ++s) {
            const double v = fc_at(bars_[s].values, i);
            if (!fc_finite(v))
                continue;
            any_bar = cat_any = true;
            if (v >= 0)
                pos_acc[slot_of[s]] += v;
            else
                neg_acc[slot_of[s]] += v;
        }
        if (!cat_any)
            continue;
        for (int k = 0; k < n_slots; ++k) {
            widen(plo, phi, pos_acc[k]);
            widen(plo, phi, neg_acc[k]);
        }
    }
    bool any_pline = false, any_sline = false;
    for (const ComboLine& l : lines_) {
        for (int i = 0; i < n; ++i) {
            const double v = fc_at(l.values, i);
            if (!fc_finite(v))
                continue;
            if (l.secondary) {
                widen(slo, shi, v);
                any_sline = true;
            } else {
                widen(plo, phi, v);
                any_pline = true;
            }
        }
    }
    const bool sec_left = any_sline && !any_bar && !any_pline; // only secondary lines: use the left axis
    const bool has_right = any_sline && !sec_left;
    if (sec_left) {
        plo = slo;
        phi = shi;
    }
    const bool ref_on_right = has_right && ref_secondary_;
    if (fc_finite(ref_value_)) {
        if (ref_on_right)
            widen(slo, shi, ref_value_);
        else
            widen(plo, phi, ref_value_);
    }
    if (!fc_finite(plo) || !fc_finite(phi)) {
        plo = 0.0;
        phi = 1.0;
    }
    if (zero_based_ || any_bar) {
        plo = std::min(plo, 0.0);
        phi = std::max(phi, 0.0);
    } else {
        const double pad = phi > plo ? (phi - plo) * 0.08 : std::max(std::abs(phi) * 0.05, 1e-9);
        plo -= pad;
        phi += pad;
    }
    if (has_right) {
        const double pad = shi > slo ? (shi - slo) * 0.08 : std::max(std::abs(shi) * 0.05, 1e-9);
        slo -= pad;
        shi += pad;
    }
    const FcFmt axis_fmt = sec_left ? sf : pf;

    // Legend items: bars, then lines.
    QVector<FcLegendItem> items;
    for (const ComboBars& b : bars_) {
        if (!b.name.isEmpty())
            items.append({b.name, fc_col::chart_color(b.color_slot), FcSwatch::Box});
    }
    for (const ComboLine& l : lines_) {
        if (l.name.isEmpty())
            continue;
        const QString name = (has_right && l.secondary) ? tr("%1 (R)").arg(l.name) : l.name;
        items.append({name, fc_col::chart_color(l.color_slot), l.dashed ? FcSwatch::Dashed : FcSwatch::Line});
    }

    // Frame: legend above, value axes left (and right), category labels below. The axis label
    // widths depend on the ticks, the ticks on the plot height, the plot height on the legend
    // rows, the legend rows on the plot width — iterate until stable.
    const QFont af = fc_cs::axis_font();
    const QFontMetricsF afm(af);
    const qreal ah = std::ceil(afm.height());
    const qreal tick_spacing = std::max<qreal>(ah * 2.6, 22.0);
    const qreal label_gap = 6.0; // chart_style::draw_value_axis puts labels 6 px off the plot
    const FcLegendMetrics lm = fc_legend_metrics();
    const int max_rows = std::max(1, static_cast<int>((r.height() * 0.3) / (lm.h + lm.row_gap)));
    const qreal top_pad = std::ceil(ah / 2.0) + 1.0 + (show_values_ ? ah + 2.0 : 0.0);
    const qreal bottom_h = category_axis_height();
    const qreal right_pad = std::max<qreal>(4.0, std::round(afm.averageCharWidth()));

    qreal left_w = 0.0;
    qreal right_w = right_pad;
    FcLegendLayout lg;
    QRectF plot;
    QVector<double> ticks;
    QVector<double> sticks;
    double lo = 0.0, hi = 1.0, s_lo = 0.0, s_hi = 1.0;
    FcFmt tick_fmt = axis_fmt;
    FcFmt stick_fmt = sf;
    for (int pass = 0; pass < 6; ++pass) {
        const qreal pl = r.left() + left_w;
        const qreal pr = r.right() - right_w;
        lg = fc_layout_legend(items, pl, r.top(), pr - pl, max_rows);
        const qreal legend_h = lg.height > 0 ? lg.height + std::round(lm.h * 0.6) : 0.0;
        plot = QRectF(QPointF(pl, r.top() + legend_h + top_pad), QPointF(pr, r.bottom() - bottom_h));
        ticks = fc_nice(plo, phi, plot.height(), tick_spacing, &lo, &hi);
        tick_fmt = fc_tick_formatter(ticks, axis_fmt);
        const qreal need_left = std::ceil(fc_cs::axis_label_width(ticks, tick_fmt)) + label_gap + 2.0;
        qreal need_right = right_pad;
        if (has_right) {
            sticks = fc_aligned_ticks(slo, shi, std::max(1, static_cast<int>(ticks.size()) - 1), &s_lo, &s_hi);
            stick_fmt = fc_tick_formatter(sticks, sf);
            need_right = std::ceil(fc_cs::axis_label_width(sticks, stick_fmt)) + label_gap + 2.0;
        }
        if (std::abs(need_left - left_w) < 0.5 && std::abs(need_right - right_w) < 0.5)
            break;
        left_w = need_left;
        right_w = need_right;
    }
    if (plot.width() < 20 || plot.height() < 20)
        return;
    if (!(hi > lo))
        hi = lo + 1.0;
    if (!(s_hi > s_lo))
        s_hi = s_lo + 1.0;
    const auto ymap = [&plot, lo, hi](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    const auto smap = [&plot, s_lo, s_hi](double v) {
        return plot.bottom() - (v - s_lo) / (s_hi - s_lo) * plot.height();
    };

    draw_selection(p, plot);
    fc_cs::draw_value_axis(p, plot, ticks, lo, hi, Qt::Vertical, tick_fmt);
    if (has_right) {
        p.save();
        p.setFont(af);
        p.setPen(fc_cs::axis_text_color());
        for (double t : sticks) {
            const qreal y = std::round(smap(t)) + 0.5;
            p.drawText(QRectF(plot.right() + label_gap, y - ah / 2.0, right_w, ah), Qt::AlignLeft | Qt::AlignVCenter,
                       stick_fmt(t));
        }
        p.restore();
    }

    // Bars: slots side by side inside the category group, series of a slot stacked.
    const qreal bw = band_width(plot);
    const qreal inner_gap = n_slots > 1 ? std::clamp<qreal>(std::round(bw * 0.03), 1.0, 3.0) : 0.0;
    qreal bar_w = 1.0;
    struct FcBarLabel {
        QRectF bar;
        double value = 0.0;
        bool up = true;
    };
    QVector<FcBarLabel> value_labels;
    if (n_slots > 0 && any_bar) {
        const qreal max_bar = std::max<qreal>(ah * 4.0, 6.0);
        const qreal raw = (bw * (n_slots > 1 ? 0.74 : 0.58) - inner_gap * (n_slots - 1)) / n_slots;
        bar_w = std::max<qreal>(1.0, std::floor(std::min(raw, max_bar)));
        const qreal group = bar_w * n_slots + inner_gap * (n_slots - 1);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        for (int i = 0; i < n; ++i) {
            const qreal gx = std::round(band_center(plot, i) - group / 2.0);
            for (int k = 0; k < n_slots; ++k) {
                const qreal x0 = gx + k * (bar_w + inner_gap);
                double pos = 0.0, neg = 0.0;
                QRectF pos_rect, neg_rect;
                for (int s = 0; s < nb; ++s) {
                    if (slot_of[s] != k)
                        continue;
                    const double v = fc_at(bars_[s].values, i);
                    if (!fc_finite(v) || v == 0.0)
                        continue;
                    const double from = v > 0 ? pos : neg;
                    const double to = from + v;
                    if (v > 0)
                        pos = to;
                    else
                        neg = to;
                    const qreal ya = std::round(ymap(from));
                    const qreal yb = std::round(ymap(to));
                    qreal top = std::min(ya, yb);
                    qreal bot = std::max(ya, yb);
                    if (bot - top < 1) {
                        if (v > 0)
                            top = bot - 1;
                        else
                            bot = top + 1;
                    }
                    if (from != 0.0 && bot - top > 2) { // 1 px gap between stacked segments
                        if (v > 0)
                            bot -= 1;
                        else
                            top += 1;
                    }
                    const QRectF rc(x0, top, bar_w, bot - top);
                    p.fillRect(rc, fc_col::chart_color(bars_[s].color_slot));
                    if (v > 0)
                        pos_rect = pos_rect.isNull() ? rc : pos_rect.united(rc);
                    else
                        neg_rect = neg_rect.isNull() ? rc : neg_rect.united(rc);
                }
                if (show_values_) {
                    if (pos > 0)
                        value_labels.append({pos_rect, pos, true});
                    if (neg < 0)
                        value_labels.append({neg_rect, neg, false});
                }
            }
        }
        p.restore();
    }

    // Zero line over the bar bases.
    if (lo <= 0.0 && hi >= 0.0) {
        const qreal y = std::round(ymap(0.0)) + 0.5;
        p.setPen(QPen(fc_cs::axis_color(), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // Reference (hurdle) line with its label at the right end.
    if (fc_finite(ref_value_)) {
        const double rv = ref_value_;
        const qreal yv = ref_on_right ? smap(rv) : ymap(rv);
        if (yv >= plot.top() - 0.5 && yv <= plot.bottom() + 0.5) {
            const qreal y = std::round(yv) + 0.5;
            const QColor rc = fc_ck::warning();
            QPen pen(rc, 1.0, Qt::CustomDashLine);
            pen.setDashPattern({5.0, 3.0});
            p.setPen(pen);
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            const QString val = ref_secondary_ ? sf(rv) : pf(rv);
            QString text = val;
            if (!ref_label_.isEmpty())
                text = ref_label_.contains(val) ? ref_label_ : ref_label_ + QStringLiteral(" ") + val;
            const qreal pad = std::max<qreal>(2.0, std::round(afm.averageCharWidth() * 0.4));
            const qreal tw = std::min(afm.horizontalAdvance(text), plot.width() - 4 * pad);
            qreal ty = y - ah - 2.0;
            if (ty < plot.top())
                ty = y + 2.0;
            const QRectF box(plot.right() - tw - 3 * pad, ty, tw + 2 * pad, ah);
            p.setPen(Qt::NoPen);
            p.setBrush(fc_ck::with_alpha(fc_ck::background(), 0.82));
            p.drawRoundedRect(box, 2.0, 2.0);
            p.setFont(af);
            p.setPen(rc);
            p.drawText(box, Qt::AlignCenter, afm.elidedText(text, Qt::ElideRight, tw + 0.5));
        }
    }

    // Lines (2 px, round joins) with ringed markers.
    const qreal mr = std::clamp<qreal>(ah * 0.24, 2.5, 5.0);
    for (const ComboLine& l : lines_) {
        const QColor c = fc_col::chart_color(l.color_slot);
        const bool right = has_right && l.secondary;
        QVector<QPolygonF> segs;
        QPolygonF cur;
        for (int i = 0; i < n; ++i) {
            const double v = fc_at(l.values, i);
            if (!fc_finite(v)) {
                if (!cur.isEmpty())
                    segs.append(cur);
                cur.clear();
                continue;
            }
            cur << QPointF(band_center(plot, i), right ? smap(v) : ymap(v));
        }
        if (!cur.isEmpty())
            segs.append(cur);
        QPen pen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (l.dashed) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern({3.0, 2.0});
            pen.setCapStyle(Qt::FlatCap);
        }
        for (const QPolygonF& seg : segs) {
            if (seg.size() >= 2) {
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPolyline(seg);
            }
            if (l.markers || seg.size() == 1) {
                p.setPen(QPen(fc_ck::background(), 1.5));
                p.setBrush(c);
                for (const QPointF& pt : seg)
                    p.drawEllipse(pt, mr, mr);
            }
        }
    }

    // Value labels over (under) the bars when they fit the bar.
    if (show_values_ && !value_labels.isEmpty()) {
        const qreal room = n_slots > 1 ? bar_w + inner_gap : bw - 4.0;
        p.setFont(af);
        p.setPen(fc_cs::axis_text_color());
        for (const FcBarLabel& bl : value_labels) {
            const QString text = pf(bl.value);
            const qreal tw = afm.horizontalAdvance(text);
            if (tw > room)
                continue;
            const qreal cx = bl.bar.center().x();
            const qreal y = bl.up ? bl.bar.top() - ah - 1.0 : bl.bar.bottom() + 1.0;
            p.drawText(QRectF(cx - tw / 2.0 - 1.0, y, tw + 2.0, ah), Qt::AlignCenter, text);
        }
    }

    draw_categories(p, plot);
    fc_draw_legend(p, items, lg);
    plot_ = plot;
}

QVector<ui::chartkit::TooltipRow> FinComboChart::tooltip_rows(int i) const {
    FcRows rows;
    const FcFmt base_fmt = [this](double v) { return format_value(v); };
    const FcFmt pf = primary_fmt_ ? FcFmt(primary_fmt_) : base_fmt;
    const FcFmt sf = secondary_fmt_ ? FcFmt(secondary_fmt_) : base_fmt;
    for (const ComboBars& b : bars_) {
        const double v = fc_at(b.values, i);
        rows.append({fc_col::chart_color(b.color_slot), b.name.isEmpty() ? tr("Value") : b.name,
                     fc_finite(v) ? pf(v) : fc_dash(), QColor()});
    }
    for (const ComboLine& l : lines_) {
        const double v = fc_at(l.values, i);
        const FcFmt& f = l.secondary ? sf : pf;
        rows.append({fc_col::chart_color(l.color_slot), l.name.isEmpty() ? tr("Value") : l.name,
                     fc_finite(v) ? f(v) : fc_dash(), QColor()});
    }
    if (fc_finite(ref_value_)) {
        rows.append({fc_ck::warning(), ref_label_.isEmpty() ? tr("Reference") : ref_label_,
                     ref_secondary_ ? sf(ref_value_) : pf(ref_value_), QColor()});
    }
    return rows;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinWaterfallChart
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinWaterfallChart::FinWaterfallChart(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FinWaterfallChart::set_steps(const QVector<Step>& steps) {
    steps_ = steps;
    hover_ = -1;
    rows_.clear();
    hide_tip();
    invalidate();
}

void FinWaterfallChart::set_caption(const QString& caption) {
    caption_ = caption;
    invalidate();
}

void FinWaterfallChart::set_summary(const QString& text) {
    setAccessibleDescription(text);
}

void FinWaterfallChart::clear() {
    steps_.clear();
    caption_.clear();
    rows_.clear();
    hover_ = -1;
    hide_tip();
    invalidate();
}

void FinWaterfallChart::paint_chart(QPainter& p, const QRectF& r) {
    rows_.clear();
    const int n = static_cast<int>(steps_.size());
    if (n == 0 || r.width() < 60 || r.height() < 40)
        return;
    const FcBridge b = fc_bridge(steps_);
    const FcFmt fmt = [this](double v) { return format_value(v); };

    // Header: key (Total / Increase / Decrease) left, caption right.
    const FcLegendMetrics lm = fc_legend_metrics();
    bool any_total = false, any_up = false, any_down = false;
    for (const Step& s : steps_) {
        if (!fc_finite(s.value))
            continue;
        if (s.kind != Kind::Delta)
            any_total = true;
        else if (s.value >= 0)
            any_up = true;
        else
            any_down = true;
    }
    QVector<FcLegendItem> items;
    if (any_total)
        items.append({tr("Total"), fc_col::chart_color(kFcTotalSlot), FcSwatch::Box});
    if (any_up)
        items.append({tr("Increase"), fc_ck::up(), FcSwatch::Box});
    if (any_down)
        items.append({tr("Decrease"), fc_ck::down(), FcSwatch::Box});
    const QFont cf = fc_cs::font(-3);
    const QFontMetricsF cfm(cf);
    const QString caption = cfm.elidedText(caption_, Qt::ElideRight, r.width() * 0.5);
    const qreal cap_w = caption.isEmpty() ? 0.0 : cfm.horizontalAdvance(caption) + lm.gap_items;
    const FcLegendLayout lg = fc_layout_legend(items, r.left(), r.top(), r.width() - cap_w, 1);
    const qreal header_h = (lg.height > 0 || !caption.isEmpty()) ? lm.h + std::round(lm.h * 0.5) : 0.0;

    // Rows.
    const QFont af = fc_cs::axis_font();
    const QFontMetricsF afm(af);
    const qreal ah = std::ceil(afm.height());
    const qreal rows_top = r.top() + header_h;
    const qreal rows_bottom = r.bottom() - (ah + 5.0);
    if (rows_bottom - rows_top < n * 4.0)
        return;
    const qreal row_h = (rows_bottom - rows_top) / n;
    int loff = -3;
    while (loff > -6 && QFontMetricsF(fc_cs::font(loff)).height() > row_h * 0.92)
        --loff;
    const QFont lf = fc_cs::font(loff);
    const QFont lfb = fc_cs::font(loff, QFont::DemiBold);
    const QFontMetricsF lfm(lf);
    const QFontMetricsF lfbm(lfb);
    const QFont vf = fc_cs::font(loff - 1);
    const QFont vfb = fc_cs::font(loff - 1, QFont::DemiBold);
    const QFontMetricsF vfm(vf);
    const QFontMetricsF vfbm(vfb);
    const qreal vh = std::ceil(std::max(vfm.height(), vfbm.height()));

    // Label column (deltas indented under their totals, like a statement).
    const qreal indent = std::round(lfm.averageCharWidth() * 1.2);
    qreal widest = 0.0;
    for (const Step& s : steps_) {
        const bool total = s.kind != Kind::Delta;
        const qreal w = total ? lfbm.horizontalAdvance(s.label) : indent + lfm.horizontalAdvance(s.label);
        widest = std::max(widest, w);
    }
    const qreal col_gap = std::round(lfm.averageCharWidth() * 1.5);
    const qreal label_w = std::min(widest + col_gap, std::round(r.width() * 0.38));

    // Value texts ("+$1.2B" for steps, "$478.8B" for totals).
    QStringList vtext;
    QVector<qreal> vwidth(n, 0.0);
    qreal vw_max = 0.0;
    for (int i = 0; i < n; ++i) {
        const Step& s = steps_[i];
        QString t;
        if (fc_finite(s.value))
            t = s.kind == Kind::Delta ? fc_signed(s.value, fmt) : fmt(s.value);
        vtext.append(t);
        vwidth[i] = (s.kind == Kind::Delta ? vfm : vfbm).horizontalAdvance(t);
        vw_max = std::max(vw_max, vwidth[i]);
    }
    const qreal vgap = std::max<qreal>(4.0, std::round(vfm.averageCharWidth() * 0.8));

    // Value scale: as wide as the value labels at the bar ends allow.
    double dlo = 0.0, dhi = 0.0;
    for (int i = 0; i < n; ++i) {
        if (!fc_finite(b.from[i]))
            continue;
        dlo = std::min({dlo, b.from[i], b.to[i]});
        dhi = std::max({dhi, b.from[i], b.to[i]});
    }
    if (!(dhi - dlo > 0))
        dhi = dlo + 1.0;
    const qreal plot_left = r.left() + label_w + (dlo < 0 ? vw_max + vgap : 0.0);
    qreal plot_w = r.right() - plot_left - vw_max - vgap;
    QVector<double> ticks;
    double lo = dlo, hi = dhi;
    FcFmt tick_fmt = fmt;
    for (int pass = 0; pass < 3 && plot_w >= 20; ++pass) {
        int target = std::max(2, static_cast<int>(plot_w / (afm.averageCharWidth() * 8.0 * 1.7)) + 1);
        for (int t = 0; t < 5; ++t) {
            ticks = fc_cs::nice_ticks(dlo, dhi, target, &lo, &hi);
            tick_fmt = fc_tick_formatter(ticks, fmt);
            const qreal tw = fc_cs::axis_label_width(ticks, tick_fmt);
            if (ticks.size() <= 2 || tw * 1.5 * (ticks.size() - 1) <= plot_w)
                break;
            target = std::max(2, target - 1);
        }
        if (!(hi > lo))
            hi = lo + 1.0;
        const qreal last_tick_w = fc_cs::axis_label_width(QVector<double>{hi}, tick_fmt);
        qreal w = r.right() - plot_left - std::ceil(last_tick_w / 2.0) - 1.0;
        for (int i = 0; i < n; ++i) {
            if (!fc_finite(b.to[i]))
                continue;
            const Step& s = steps_[i];
            const bool right_side = s.kind == Kind::Delta ? s.value >= 0 : b.to[i] >= 0;
            if (!right_side)
                continue;
            const double k = (b.to[i] - lo) / (hi - lo);
            if (k <= 1e-9)
                continue;
            w = std::min(w, (r.right() - plot_left - vgap - vwidth[i]) / k);
        }
        w = std::floor(w);
        const bool stable = std::abs(w - plot_w) < 1.0;
        plot_w = w;
        if (stable)
            break;
    }
    if (plot_w < 20)
        return;
    const QRectF plot(plot_left, rows_top, plot_w, rows_bottom - rows_top);
    const auto xmap = [&plot, lo, hi](double v) { return plot.left() + (v - lo) / (hi - lo) * plot.width(); };

    fc_cs::draw_value_axis(p, plot, ticks, lo, hi, Qt::Horizontal, tick_fmt);
    if (lo <= 0.0 && hi >= 0.0) { // zero line (under the bars)
        const qreal x = std::round(xmap(0.0)) + 0.5;
        p.setPen(QPen(fc_cs::axis_color(), 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }

    // Bars.
    const qreal th = std::max<qreal>(2.0, std::round(std::min(row_h * 0.62, lfm.height() * 1.5)));
    QVector<QRectF> bars(n);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < n; ++i) {
        const qreal row_top = rows_top + i * row_h;
        rows_.append(QRectF(r.left(), row_top, r.width(), row_h));
        if (!fc_finite(b.from[i]))
            continue;
        const qreal xa = std::round(xmap(b.from[i]));
        const qreal xb = std::round(xmap(b.to[i]));
        const qreal l = std::min(xa, xb);
        qreal rr = std::max(xa, xb);
        if (rr - l < 1)
            rr = l + 1;
        bars[i] = QRectF(l, std::round(row_top + (row_h - th) / 2.0), rr - l, th);
        p.fillRect(bars[i], fc_step_color(steps_[i]));
    }
    p.restore();

    // Dotted connectors from each bar's end to the next row.
    p.save();
    p.setPen(QPen(fc_cs::muted_color(), 1, Qt::DotLine));
    for (int i = 0; i + 1 < n; ++i) {
        if (bars[i].isNull() || bars[i + 1].isNull())
            continue;
        const bool right_edge = b.to[i] >= b.from[i];
        const qreal x = std::round(xmap(b.to[i])) + (right_edge ? -0.5 : 0.5);
        p.drawLine(QPointF(x, bars[i].bottom()), QPointF(x, bars[i + 1].top()));
    }
    p.restore();

    // Label column.
    p.save();
    for (int i = 0; i < n; ++i) {
        const Step& s = steps_[i];
        const bool total = s.kind != Kind::Delta;
        const qreal x = r.left() + (total ? 0.0 : indent);
        const qreal avail = std::max<qreal>(0.0, label_w - col_gap - (total ? 0.0 : indent));
        const QFontMetricsF& m = total ? lfbm : lfm;
        p.setFont(total ? lfb : lf);
        p.setPen(total ? fc_cs::label_color() : fc_cs::axis_text_color());
        p.drawText(QRectF(x, rows_[i].top(), avail + 1.0, row_h), Qt::AlignLeft | Qt::AlignVCenter,
                   m.elidedText(s.label, Qt::ElideRight, avail));
    }

    // Value labels: outside the bar end, else inside, else outside the other end.
    const qreal min_x = r.left() + label_w;
    for (int i = 0; i < n; ++i) {
        if (bars[i].isNull() || vtext[i].isEmpty())
            continue;
        const Step& s = steps_[i];
        const bool delta = s.kind == Kind::Delta;
        const QColor step_color = fc_step_color(s);
        QColor pen = delta ? step_color : fc_cs::label_color();
        const qreal tw = vwidth[i];
        const QRectF& bar = bars[i];
        const bool prefer_right = delta ? s.value >= 0 : b.to[i] >= 0;
        const qreal right_x = bar.right() + vgap;
        const qreal left_x = bar.left() - vgap - tw;
        const bool right_ok = right_x + tw <= r.right() + 0.5;
        const bool left_ok = left_x >= min_x;
        qreal x = 0.0;
        if (prefer_right && right_ok) {
            x = right_x;
        } else if (!prefer_right && left_ok) {
            x = left_x;
        } else if (bar.width() >= tw + 2 * vgap) {
            x = bar.center().x() - tw / 2.0;
            pen = fc_ck::text_on(step_color);
        } else if (left_ok) {
            x = left_x;
        } else if (right_ok) {
            x = right_x;
        } else {
            continue;
        }
        p.setFont(delta ? vf : vfb);
        p.setPen(pen);
        p.drawText(QRectF(x, bar.center().y() - vh / 2.0, tw + 1.0, vh), Qt::AlignLeft | Qt::AlignVCenter, vtext[i]);
    }

    // Header.
    fc_draw_legend(p, items, lg);
    if (!caption.isEmpty()) {
        p.setFont(cf);
        p.setPen(fc_cs::muted_color());
        p.drawText(QRectF(r.left(), r.top(), r.width(), lm.h), Qt::AlignRight | Qt::AlignVCenter, caption);
    }
    p.restore();
}

void FinWaterfallChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= rows_.size() || hover_ >= steps_.size())
        return;
    const QRectF row = fc_snap(rows_[hover_]);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(row, fc_hover_color());
    p.restore();
    const FcFmt fmt = [this](double v) { return format_value(v); };
    const FcRows rows = fc_waterfall_rows(steps_, hover_, fmt);
    const QString& title = steps_[hover_].label;
    const QString& footer = steps_[hover_].tip;
    const QRect bounds = r.toRect();
    if (!fc_tip_fits(bounds, title, rows, footer))
        return;
    fc_tip_below(p, bounds, row.toRect(), qRound(row.center().x()), title, rows, footer);
}

void FinWaterfallChart::mouseMoveEvent(QMouseEvent* e) {
    const int i = chart_visible() ? step_at(e->position()) : -1;
    if (i == hover_)
        return;
    hover_ = i;
    if (i >= 0) {
        const FcFmt fmt = [this](double v) { return format_value(v); };
        const FcRows rows = fc_waterfall_rows(steps_, i, fmt);
        const QString& title = steps_[i].label;
        const QString& footer = steps_[i].tip;
        if (fc_tip_fits(content_rect().toRect(), title, rows, footer))
            hide_tip();
        else
            show_tip(e->position().toPoint(), fc_tip_html(title, rows, footer));
    } else {
        hide_tip();
    }
    update();
}

void FinWaterfallChart::on_hover_left() {
    hover_ = -1;
}

int FinWaterfallChart::step_at(const QPointF& pos) const {
    const int n = std::min(static_cast<int>(rows_.size()), static_cast<int>(steps_.size()));
    for (int i = 0; i < n; ++i) {
        if (rows_[i].contains(pos))
            return i;
    }
    return -1;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinStackChart
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinStackChart::FinStackChart(QWidget* parent) : FinChartBase(parent) {}

void FinStackChart::set_sides(const QString& left_name, const QVector<Segment>& left, const QString& right_name,
                              const QVector<Segment>& right) {
    left_name_ = left_name;
    left_ = left;
    right_name_ = right_name;
    right_ = right;
    hover_ = -1;
    hide_tip();
    invalidate();
}

void FinStackChart::set_amount_formatter(Formatter f) {
    amount_fmt_ = std::move(f);
    invalidate();
}

void FinStackChart::clear() {
    left_name_.clear();
    right_name_.clear();
    left_.clear();
    right_.clear();
    labels_.clear();
    tips_.clear();
    hover_ = -1;
    selected_ = -1;
    plot_ = QRectF();
    hide_tip();
    invalidate();
}

bool FinStackChart::has_data() const {
    const int n = static_cast<int>(labels_.size());
    if (n <= 0)
        return false;
    for (int i = 0; i < n; ++i) {
        if (fc_side_total(left_, i) > 0 || fc_side_total(right_, i) > 0)
            return true;
    }
    return false;
}

void FinStackChart::paint_chart(QPainter& p, const QRectF& r) {
    plot_ = QRectF();
    const int n = static_cast<int>(labels_.size());
    if (n <= 0 || r.width() < 60 || r.height() < 60)
        return;
    const QFont af = fc_cs::axis_font();
    const QFontMetricsF afm(af);
    const qreal ah = std::ceil(afm.height());
    const QFont sidef = fc_cs::font(-4);
    const QFontMetricsF sfm(sidef);
    const qreal sh = std::ceil(sfm.height());

    const FcFmt pct_fmt = [](double v) { return QLocale().toString(v, 'f', 0) + QLatin1Char('%'); };
    const qreal left_w = std::ceil(fc_cs::axis_label_width(QVector<double>{0.0, 100.0}, pct_fmt)) + 8.0;
    const qreal right_pad = std::max<qreal>(4.0, std::round(afm.averageCharWidth()));
    const qreal pl = r.left() + left_w;
    const qreal pr = r.right() - right_pad;
    const qreal bw = (pr - pl) / n;
    const qreal col_cap = std::round(ah * 3.6);
    const qreal col_gap = std::max<qreal>(2.0, std::round(ah * 0.25));
    const qreal col_w = std::max<qreal>(2.0, std::floor(std::min((bw * 0.7 - col_gap) / 2.0, col_cap)));

    // Side labels under the columns: the names when both fit, else their initials ("A", "L+E").
    const qreal side_room = col_w + col_gap;
    const bool full_fit =
        sfm.horizontalAdvance(left_name_) <= side_room && sfm.horizontalAdvance(right_name_) <= side_room;
    const QString lab_l = full_fit ? left_name_ : fc_abbrev(left_name_);
    const QString lab_r = full_fit ? right_name_ : fc_abbrev(right_name_);

    // Legend: left-side segments under a muted heading, then the right side's.
    QVector<FcLegendItem> items;
    const auto add_side = [&items](const QString& name, const QString& abbrev, const QVector<Segment>& segs) {
        if (segs.isEmpty())
            return;
        if (!name.isEmpty()) {
            const QString head = (abbrev.isEmpty() || abbrev == name) ? name : tr("%1 (%2)").arg(name, abbrev);
            items.append({head, QColor(), FcSwatch::Heading});
        }
        for (const Segment& s : segs)
            items.append({s.name, fc_col::chart_color(s.color_slot), FcSwatch::Box});
    };
    add_side(left_name_, lab_l, left_);
    add_side(right_name_, lab_r, right_);
    const FcLegendMetrics lm = fc_legend_metrics();
    const int max_rows = std::max(1, static_cast<int>((r.height() * 0.32) / (lm.h + lm.row_gap)));
    const FcLegendLayout lg = fc_layout_legend(items, pl, r.top(), pr - pl, max_rows);
    const qreal legend_h = lg.height > 0 ? lg.height + std::round(lm.h * 0.6) : 0.0;
    const qreal side_h = sh + 3.0;
    const QRectF plot(QPointF(pl, r.top() + legend_h + std::ceil(ah / 2.0) + 1.0),
                      QPointF(pr, r.bottom() - side_h - category_axis_height()));
    if (plot.height() < 20 || plot.width() < 20)
        return;

    QVector<double> ticks;
    if (plot.height() / 4.0 >= ah * 1.8)
        ticks = {0.0, 25.0, 50.0, 75.0, 100.0};
    else if (plot.height() / 2.0 >= ah * 1.4)
        ticks = {0.0, 50.0, 100.0};
    else
        ticks = {0.0, 100.0};
    draw_selection(p, plot);
    fc_cs::draw_value_axis(p, plot, ticks, 0.0, 100.0, Qt::Vertical, pct_fmt);
    const auto ymap = [&plot](double pct) { return plot.bottom() - pct / 100.0 * plot.height(); };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < n; ++i) {
        const qreal cx = std::round(band_center(plot, i));
        const qreal xl = cx - std::floor(col_gap / 2.0) - col_w;
        const qreal xr = xl + col_w + col_gap;
        for (int side = 0; side < 2; ++side) {
            const QVector<Segment>& segs = side == 0 ? left_ : right_;
            const qreal x = side == 0 ? xl : xr;
            const double total = fc_side_total(segs, i);
            if (total <= 0)
                continue;
            double acc = 0.0;
            bool first = true;
            for (const Segment& s : segs) {
                const double v = fc_at(s.values, i);
                if (!fc_finite(v) || v <= 0)
                    continue;
                const qreal yb = std::round(ymap(acc / total * 100.0));
                acc += v;
                const qreal yt = std::round(ymap(acc / total * 100.0));
                qreal bot = yb;
                if (!first && bot - yt > 2) // 1 px gap between segments
                    bot -= 1;
                first = false;
                if (bot - yt < 1)
                    continue;
                const QRectF rc(x, yt, col_w, bot - yt);
                const QColor c = fc_col::chart_color(s.color_slot);
                p.fillRect(rc, c);
                const double share = v / total * 100.0;
                if (share >= 1.0) {
                    const QString t = QLocale().toString(share, 'f', 0) + QLatin1Char('%');
                    if (rc.height() >= ah && rc.width() >= afm.horizontalAdvance(t) + 4.0) {
                        p.setFont(af);
                        p.setPen(fc_ck::text_on(c));
                        p.drawText(rc, Qt::AlignCenter, t);
                    }
                }
            }
        }
        // Side labels.
        p.setFont(sidef);
        p.setPen(fc_cs::muted_color());
        const qreal sy = plot.bottom() + 3.0;
        const qreal half_gap = std::floor(col_gap / 2.0);
        p.drawText(QRectF(xl - half_gap, sy, col_w + col_gap, sh), Qt::AlignHCenter | Qt::AlignTop,
                   sfm.elidedText(lab_l, Qt::ElideRight, col_w + col_gap));
        p.drawText(QRectF(xr - half_gap, sy, col_w + col_gap, sh), Qt::AlignHCenter | Qt::AlignTop,
                   sfm.elidedText(lab_r, Qt::ElideRight, col_w + col_gap));
    }
    p.restore();

    draw_categories(p, QRectF(plot.left(), plot.top(), plot.width(), plot.height() + side_h));
    fc_draw_legend(p, items, lg);
    plot_ = plot;
}

QVector<ui::chartkit::TooltipRow> FinStackChart::tooltip_rows(int i) const {
    FcRows rows;
    const FcFmt fmt = amount_fmt_ ? FcFmt(amount_fmt_) : FcFmt([this](double v) { return format_value(v); });
    const auto add_side = [&rows, &fmt, i](const QString& name, const QVector<Segment>& segs) {
        if (segs.isEmpty())
            return;
        const double total = fc_side_total(segs, i);
        rows.append({QColor(), name, total > 0 ? fmt(total) : fc_dash(), QColor()});
        for (const Segment& s : segs) {
            const double v = fc_at(s.values, i);
            QString value = fc_dash();
            if (fc_finite(v)) {
                const double amount = std::max(0.0, v);
                value = QStringLiteral("%1  %2").arg(fmt(amount), fc_percent(total > 0 ? amount / total * 100.0 : 0.0, 1));
            }
            rows.append({fc_col::chart_color(s.color_slot), s.name, value, QColor()});
        }
    };
    add_side(left_name_, left_);
    add_side(right_name_, right_);
    return rows;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinDupontView
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinDupontView::FinDupontView(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void FinDupontView::set_tree(const Node& root, const QVector<Node>& drivers) {
    root_ = root;
    drivers_ = drivers;
    hover_ = -1;
    boxes_.clear();
    hide_tip();
    invalidate();
}

void FinDupontView::set_summary(const QString& text) {
    setAccessibleDescription(text);
}

void FinDupontView::clear() {
    root_ = Node();
    drivers_.clear();
    boxes_.clear();
    hover_ = -1;
    hide_tip();
    invalidate();
}

void FinDupontView::paint_chart(QPainter& p, const QRectF& r) {
    boxes_.clear();
    if (root_.label.isEmpty() || r.width() < 60 || r.height() < 40)
        return;
    const int k = static_cast<int>(drivers_.size());
    const QFont labf = fc_cs::font(-4, QFont::DemiBold);
    const QFontMetricsF labm(labf);
    const QFont chgf = fc_cs::font(-4);
    const QFontMetricsF chgm(chgf);
    const QFont glyphf = fc_cs::font(1, QFont::Bold);
    const QFontMetricsF glyphm(glyphf);
    const qreal lab_h = std::ceil(labm.height());
    const qreal chg_h = std::ceil(chgm.height());
    qreal pad = std::max<qreal>(4.0, std::round(lab_h * 0.6));
    const qreal gap_s = std::max<qreal>(1.0, std::round(lab_h * 0.15));
    const qreal stripe = 3.0;
    const QString times = QString(QChar(0x00D7));
    const qreal gap_x =
        std::round(std::max<qreal>(glyphm.horizontalAdvance(times) + 2.0 * pad, lab_h * 1.8));

    // Driver box width (the row is centred; boxes stop growing past a readable width).
    qreal w_d = k > 0 ? (r.width() - (k - 1) * gap_x) / k : r.width();
    w_d = std::floor(std::min(w_d, lab_h * 16.0));
    const qreal inner_d = w_d - 2.0 * pad;
    int lab_lines = 1;
    for (const Node& d : drivers_) {
        if (labm.horizontalAdvance(d.label.toUpper()) > inner_d)
            lab_lines = 2;
    }

    // Vertical budget: drop the sparkline, then the change text, then shrink the values.
    int root_off = 5;
    int drv_off = 2;
    bool show_spark = true;
    bool show_change = true;
    qreal spark_h = std::round(lab_h * 1.5);
    const qreal badge_d = std::round(glyphm.height() * 1.05);
    qreal vgap = std::max<qreal>(badge_d + 2.0 * std::round(lab_h * 0.45), std::round(lab_h * 2.0));
    const auto box_h = [&](int off, int lines) {
        const qreal vh = std::ceil(QFontMetricsF(fc_cs::font(off, QFont::Bold)).height());
        return pad + lab_h * lines + gap_s + vh + (show_change ? gap_s + chg_h : 0.0) +
               (show_spark ? gap_s * 2.0 + spark_h : 0.0) + pad;
    };
    const auto total_h = [&]() { return box_h(root_off, 1) + (k > 0 ? vgap + box_h(drv_off, lab_lines) : 0.0); };
    if (total_h() > r.height()) { // compact: tighter padding, a shorter trend line and connector
        pad = std::max<qreal>(4.0, std::round(lab_h * 0.45));
        spark_h = lab_h;
        vgap = badge_d + 4.0;
    }
    if (total_h() > r.height())
        show_spark = false;
    if (total_h() > r.height())
        show_change = false;
    while (total_h() > r.height() && (root_off > 0 || drv_off > -1)) {
        if (root_off > 0)
            --root_off;
        if (drv_off > -1)
            --drv_off;
    }
    if (total_h() > r.height() && lab_lines == 2)
        lab_lines = 1;
    if (show_spark) { // spare height goes to the trend lines first, then to the connector
        const qreal extra = r.height() - total_h();
        if (extra > 0)
            spark_h += std::floor(std::min(extra / (k > 0 ? 2.0 : 1.0), lab_h * 1.5));
    }
    if (k > 0) {
        const qreal extra = r.height() - total_h();
        if (extra > 0)
            vgap += std::floor(std::min(extra * 0.5, lab_h * 1.5));
    }
    const qreal top = r.top() + std::max<qreal>(0.0, std::floor((r.height() - total_h()) / 2.0));

    const QFont rootvf = fc_cs::font(root_off, QFont::Bold);
    const QFont drvvf = fc_cs::font(drv_off, QFont::Bold);
    const QFontMetricsF rvm(rootvf);

    // Geometry.
    const qreal cx = std::round(r.center().x());
    const qreal root_content = std::max({labm.horizontalAdvance(root_.label.toUpper()),
                                         rvm.horizontalAdvance(root_.value_text),
                                         show_change ? chgm.horizontalAdvance(fc_change_text(root_)) : 0.0}) +
                               2.0 * pad + stripe;
    qreal root_w = std::max(k > 0 ? w_d * 1.2 : w_d, root_content);
    root_w = std::floor(std::min(root_w, r.width()));
    const QRectF root_box(std::round(cx - root_w / 2.0), top, root_w, std::round(box_h(root_off, 1)));
    boxes_.append(root_box);
    if (k > 0) {
        const qreal row_w = k * w_d + (k - 1) * gap_x;
        const qreal row_left = std::round(cx - row_w / 2.0);
        const qreal drv_top = root_box.bottom() + std::round(vgap);
        const qreal drv_h = std::round(box_h(drv_off, lab_lines));
        for (int j = 0; j < k; ++j)
            boxes_.append(QRectF(row_left + j * (w_d + gap_x), drv_top, w_d, drv_h));
    }

    // Connectors: a stem from ROE to a bus over the drivers, an "=" node at the junction, and "×"
    // between the drivers.
    if (k > 0) {
        p.save();
        p.setPen(QPen(fc_cs::axis_color(), 1));
        const qreal sx = std::round(root_box.center().x()) + 0.5;
        const qreal bus_y = std::round(root_box.bottom() + vgap / 2.0) + 0.5;
        p.drawLine(QPointF(sx, root_box.bottom()), QPointF(sx, bus_y));
        const qreal x_first = std::round(boxes_[1].center().x()) + 0.5;
        const qreal x_last = std::round(boxes_[k].center().x()) + 0.5;
        if (k > 1)
            p.drawLine(QPointF(x_first, bus_y), QPointF(x_last, bus_y));
        for (int j = 1; j <= k; ++j) {
            const qreal x = std::round(boxes_[j].center().x()) + 0.5;
            p.drawLine(QPointF(x, bus_y), QPointF(x, boxes_[j].top()));
        }
        const QRectF badge(sx - 0.5 - badge_d / 2.0, bus_y - 0.5 - badge_d / 2.0, badge_d, badge_d);
        p.setBrush(fc_ck::raised());
        p.drawEllipse(badge);
        p.setFont(glyphf);
        p.setPen(fc_cs::axis_text_color());
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("="));
        for (int j = 1; j < k; ++j) {
            const QRectF gap(boxes_[j].right(), boxes_[j + 1].top(), boxes_[j + 1].left() - boxes_[j].right(),
                             boxes_[j + 1].height());
            p.drawText(gap, Qt::AlignCenter, times);
        }
        p.restore();
    }

    // Boxes.
    const auto draw_box = [&](const QRectF& box, const Node& node, bool is_root, const QFont& vf, int lines) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillRect(box, fc_ck::raised());
        p.setPen(QPen(QColor(fc_col::BORDER_DIM()), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
        if (is_root)
            p.fillRect(QRectF(box.left(), box.top(), stripe, box.height()), fc_ck::accent());
        p.restore();

        const qreal x = box.left() + pad + (is_root ? stripe : 0.0);
        const qreal w = std::max<qreal>(4.0, box.right() - pad - x);
        qreal y = box.top() + pad;
        p.setFont(labf);
        p.setPen(fc_cs::muted_color());
        const QString lab = node.label.toUpper();
        const QStringList lab_lines_text = lines >= 2 ? fc_wrap2(labm, lab, w) : QStringList{labm.elidedText(lab, Qt::ElideRight, w)};
        for (int li = 0; li < lab_lines_text.size() && li < lines; ++li)
            p.drawText(QRectF(x, y + li * lab_h, w + 1.0, lab_h), Qt::AlignLeft | Qt::AlignVCenter, lab_lines_text[li]);
        y += lab_h * lines + gap_s;

        const QFontMetricsF vm(vf);
        const qreal vh = std::ceil(vm.height());
        p.setFont(vf);
        p.setPen(fc_cs::label_color());
        p.drawText(QRectF(x, y, w + 1.0, vh), Qt::AlignLeft | Qt::AlignVCenter,
                   vm.elidedText(node.value_text, Qt::ElideRight, w));
        y += vh;
        if (show_change && !node.change_text.isEmpty()) {
            p.setFont(chgf);
            p.setPen(fc_sign_color(node.change_sign));
            p.drawText(QRectF(x, y + gap_s, w + 1.0, chg_h), Qt::AlignLeft | Qt::AlignVCenter,
                       chgm.elidedText(fc_change_text(node), Qt::ElideRight, w));
        }
        int finite = 0;
        for (double v : node.trend)
            finite += fc_finite(v) ? 1 : 0;
        if (show_spark && finite >= 2) {
            ui::SparklineStyle st;
            st.color_mode = node.change_sign > 0   ? ui::SparklineStyle::ColorMode::Positive
                            : node.change_sign < 0 ? ui::SparklineStyle::ColorMode::Negative
                                                   : ui::SparklineStyle::ColorMode::Muted;
            st.fill = true;
            st.last_dot = true;
            st.line_width = 1.5;
            ui::Sparkline::paint(p, QRectF(x, box.bottom() - pad - spark_h, w, spark_h), node.trend, st);
        }
    };
    draw_box(boxes_[0], root_, true, rootvf, 1);
    for (int j = 0; j < k; ++j)
        draw_box(boxes_[j + 1], drivers_[j], false, drvvf, lab_lines);
}

void FinDupontView::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= boxes_.size() || hover_ - 1 >= drivers_.size())
        return;
    const Node& node = hover_ == 0 ? root_ : drivers_[hover_ - 1];
    const QRectF box = boxes_[hover_];
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(QColor(fc_col::BORDER_BRIGHT()), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(box.adjusted(0, 0, -1, -1));
    p.restore();
    const FcRows rows = fc_dupont_rows(node);
    const QRect bounds = r.toRect();
    if (!fc_tip_fits(bounds, node.label, rows, node.tip))
        return;
    fc_tip_beside(p, bounds, box.toRect(), qRound(box.top()), node.label, rows, node.tip);
}

void FinDupontView::mouseMoveEvent(QMouseEvent* e) {
    int hit = -1;
    if (chart_visible()) {
        for (int i = 0; i < boxes_.size(); ++i) {
            if (boxes_[i].contains(e->position())) {
                hit = i;
                break;
            }
        }
    }
    if (hit - 1 >= drivers_.size())
        hit = -1;
    if (hit == hover_)
        return;
    hover_ = hit;
    if (hit >= 0) {
        const Node& node = hit == 0 ? root_ : drivers_[hit - 1];
        const FcRows rows = fc_dupont_rows(node);
        if (fc_tip_fits(content_rect().toRect(), node.label, rows, node.tip))
            hide_tip();
        else
            show_tip(e->position().toPoint(), fc_tip_html(node.label, rows, node.tip));
    } else {
        hide_tip();
    }
    update();
}

void FinDupontView::on_hover_left() {
    hover_ = -1;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// FinBulletChart
// ═════════════════════════════════════════════════════════════════════════════════════════════

FinBulletChart::FinBulletChart(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void FinBulletChart::set_rows(const QVector<Row>& rows) {
    rows_ = rows;
    hover_ = -1;
    hit_.clear();
    hide_tip();
    updateGeometry();
    invalidate();
}

void FinBulletChart::set_summary(const QString& text) {
    setAccessibleDescription(text);
}

void FinBulletChart::clear() {
    rows_.clear();
    hit_.clear();
    hover_ = -1;
    hide_tip();
    updateGeometry();
    invalidate();
}

QSize FinBulletChart::sizeHint() const {
    const FcBulletMetrics m = fc_bullet_metrics();
    const int n = std::max(1, static_cast<int>(rows_.size()));
    return QSize(480, static_cast<int>(std::ceil(m.key_h + n * m.row_h + 4.0)));
}

void FinBulletChart::paint_chart(QPainter& p, const QRectF& r) {
    hit_.clear();
    const int n = static_cast<int>(rows_.size());
    if (n == 0 || r.width() < 80 || r.height() < 20)
        return;
    const FcBulletMetrics m = fc_bullet_metrics();
    const QFontMetricsF lfm(m.lf);
    const QFontMetricsF vfm(m.vf);
    const QFontMetricsF mfm(m.mf);

    // Columns: label | track | value | "▲ vs median 41.2%".
    const qreal g = std::round(lfm.averageCharWidth() * 1.5);
    qreal widest_label = 0.0, widest_val = 0.0, widest_full = 0.0, widest_short = 0.0;
    QStringList med_full, med_short;
    for (const Row& row : rows_) {
        widest_label = std::max(widest_label, lfm.horizontalAdvance(row.label));
        widest_val = std::max(widest_val, vfm.horizontalAdvance(row.value_text));
        QString full, brief;
        if (!row.median_text.isEmpty()) {
            QString arrow = QStringLiteral("=");
            if (fc_finite(row.value) && fc_finite(row.median) && row.value != row.median)
                arrow = fc_arrow(row.value > row.median ? 1 : -1);
            full = tr("%1 vs median %2").arg(arrow, row.median_text);
            brief = arrow + QStringLiteral(" ") + row.median_text;
        }
        med_full.append(full);
        med_short.append(brief);
        widest_full = std::max(widest_full, mfm.horizontalAdvance(full));
        widest_short = std::max(widest_short, mfm.horizontalAdvance(brief));
    }
    const qreal label_w = std::min(widest_label + g, std::round(r.width() * 0.30));
    const qreal track_min = std::max<qreal>(r.width() * 0.26, lfm.height() * 5.0);
    const auto track_for = [&](int mode) {
        const qreal mw = mode == 2 ? widest_full : (mode == 1 ? widest_short : 0.0);
        return r.width() - label_w - g - widest_val - (mode > 0 && mw > 0 ? std::round(g * 0.6) + mw : 0.0) - 2.0;
    };
    int med_mode = 2; // 2 = "▲ vs median 41.2%", 1 = "▲ 41.2%", 0 = none
    while (med_mode > 0 && track_for(med_mode) < track_min)
        --med_mode;
    const qreal track_left = r.left() + label_w;
    const qreal track_w = std::max<qreal>(10.0, track_for(med_mode));
    const qreal val_right = track_left + track_w + g + widest_val;
    const qreal med_left = val_right + std::round(g * 0.6);

    // Key row (only when it fits on one line and leaves room for the rows).
    const QVector<FcLegendItem> key = fc_bullet_key();
    FcLegendLayout lg = fc_layout_legend(key, track_left, r.top(), r.right() - track_left, 1);
    if (lg.hidden > 0)
        lg = fc_layout_legend(key, r.left(), r.top(), r.width(), 1);
    const bool show_key = lg.hidden == 0 && r.height() - m.key_h >= n * (lfm.height() + 4.0);
    const qreal top = r.top() + (show_key ? m.key_h : 0.0);
    const qreal avail = r.bottom() - top;
    qreal row_h = m.row_h;
    if (n * row_h > avail)
        row_h = std::max<qreal>(lfm.height() + 2.0, avail / n);
    else
        row_h = std::min(avail / n, row_h * 1.3);
    const qreal th = std::max<qreal>(6.0, std::round(std::min(lfm.height() * 0.8, row_h * 0.6)));
    const qreal ext = std::max<qreal>(2.0, std::round(th * 0.25));

    if (show_key)
        fc_draw_legend(p, key, lg);

    for (int i = 0; i < n; ++i) {
        const Row& row = rows_[i];
        const qreal y0 = top + i * row_h;
        const QRectF rowr(r.left(), y0, r.width(), row_h);
        hit_.append(rowr);
        const qreal cy = y0 + row_h / 2.0;
        if (i > 0) {
            p.setPen(QPen(fc_cs::grid_color(), 1));
            const qreal ly = std::round(y0) + 0.5;
            p.drawLine(QPointF(r.left(), ly), QPointF(r.right(), ly));
        }
        // Label.
        p.setFont(m.lf);
        p.setPen(fc_cs::label_color());
        p.drawText(QRectF(r.left(), y0, label_w - g + 1.0, row_h), Qt::AlignLeft | Qt::AlignVCenter,
                   lfm.elidedText(row.label, Qt::ElideRight, label_w - g));
        // Value and median texts.
        p.setFont(m.vf);
        p.setPen(fc_cs::label_color());
        p.drawText(QRectF(val_right - widest_val - 1.0, y0, widest_val + 1.0, row_h), Qt::AlignRight | Qt::AlignVCenter,
                   row.value_text);
        if (med_mode > 0) {
            const QString t = med_mode == 2 ? med_full.at(i) : med_short.at(i);
            p.setFont(m.mf);
            p.setPen(fc_cs::muted_color());
            p.drawText(QRectF(med_left, y0, std::max<qreal>(0.0, r.right() - med_left) + 1.0, row_h),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       mfm.elidedText(t, Qt::ElideRight, std::max<qreal>(0.0, r.right() - med_left)));
        }

        // Scale of this row.
        double smin = 0.0, smax = 0.0;
        bool any = false;
        for (double v : {row.lo, row.hi, row.q1, row.q3, row.median, row.value}) {
            if (!fc_finite(v))
                continue;
            smin = std::min(smin, v);
            smax = std::max(smax, v);
            any = true;
        }
        if (!any)
            continue;
        if (!(smax - smin > 0))
            smax = smin + 1.0;
        const double span = smax - smin;
        smax += span * 0.04;
        if (smin < 0)
            smin -= span * 0.04;
        const bool crosses = smin < 0;
        const auto xmap = [track_left, track_w, smin, smax](double v) {
            return track_left + (v - smin) / (smax - smin) * track_w;
        };
        const qreal ty = std::round(cy - th / 2.0);

        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillRect(fc_snap(QRectF(track_left, ty, track_w, th)), fc_track_color());
        if (fc_finite(row.lo) && fc_finite(row.hi))
            p.fillRect(fc_snap(QRectF(QPointF(xmap(std::min(row.lo, row.hi)), ty), QPointF(xmap(std::max(row.lo, row.hi)), ty + th))),
                       fc_range_color());
        if (fc_finite(row.q1) && fc_finite(row.q3))
            p.fillRect(fc_snap(QRectF(QPointF(xmap(std::min(row.q1, row.q3)), ty), QPointF(xmap(std::max(row.q1, row.q3)), ty + th))),
                       fc_iqr_color());
        const int verdict = fc_bullet_verdict(row);
        const QColor c = fc_bullet_color(verdict);
        if (fc_finite(row.value) && !crosses) {
            const qreal bt = std::max<qreal>(3.0, std::round(th * 0.42));
            const qreal by = std::round(cy - bt / 2.0);
            p.fillRect(fc_snap(QRectF(QPointF(track_left, by), QPointF(xmap(row.value), by + bt))), c);
        }
        if (crosses) { // the zero reference
            const qreal x0 = std::round(xmap(0.0));
            p.fillRect(QRectF(x0, ty - 2.0, 1.0, th + 4.0), fc_cs::axis_color());
        }
        if (fc_finite(row.median)) {
            const qreal mx = std::round(xmap(row.median));
            p.fillRect(QRectF(mx - 1.0, ty - ext, 2.0, th + 2.0 * ext), fc_ck::text());
        }
        p.restore();
        if (fc_finite(row.value) && crosses) {
            const qreal d = std::round(th * 0.6);
            const qreal vx = xmap(row.value);
            QPolygonF dia;
            dia << QPointF(vx, cy - d) << QPointF(vx + d, cy) << QPointF(vx, cy + d) << QPointF(vx - d, cy);
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(fc_ck::background(), 1.25));
            p.setBrush(c);
            p.drawPolygon(dia);
            p.restore();
        }
    }
}

void FinBulletChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= hit_.size() || hover_ >= rows_.size())
        return;
    const QRectF row = fc_snap(hit_[hover_]);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(row, fc_hover_color());
    p.restore();
    const FcFmt fmt = [this](double v) { return format_value(v); };
    const FcRows rows = fc_bullet_rows(rows_[hover_], fmt);
    const QString& title = rows_[hover_].label;
    const QString& footer = rows_[hover_].tip;
    const QRect bounds = r.toRect();
    if (!fc_tip_fits(bounds, title, rows, footer))
        return;
    fc_tip_below(p, bounds, row.toRect(), qRound(row.center().x()), title, rows, footer);
}

void FinBulletChart::mouseMoveEvent(QMouseEvent* e) {
    int hit = -1;
    if (chart_visible()) {
        const int n = std::min(static_cast<int>(hit_.size()), static_cast<int>(rows_.size()));
        for (int i = 0; i < n; ++i) {
            if (hit_[i].contains(e->position())) {
                hit = i;
                break;
            }
        }
    }
    if (hit == hover_)
        return;
    hover_ = hit;
    if (hit >= 0) {
        const FcFmt fmt = [this](double v) { return format_value(v); };
        const FcRows rows = fc_bullet_rows(rows_[hit], fmt);
        const QString& title = rows_[hit].label;
        const QString& footer = rows_[hit].tip;
        if (fc_tip_fits(content_rect().toRect(), title, rows, footer))
            hide_tip();
        else
            show_tip(e->position().toPoint(), fc_tip_html(title, rows, footer));
    } else {
        hide_tip();
    }
    update();
}

void FinBulletChart::on_hover_left() {
    hover_ = -1;
}

} // namespace fincept::screens::fin_charts
