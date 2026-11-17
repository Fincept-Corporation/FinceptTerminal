// ReportChartRenderer.cpp — see ReportChartRenderer.h.
//
// One pass builds a model (every rectangle, tick, label and tag, measured on a 96-dpi device);
// paint() draws that model. compute_layout() returns the model's public half.

#include "services/report_builder/ReportChartRenderer.h"

#include "ui/charts/ChartAxisTicks.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::report_chart {

namespace rcs = fincept::report::chart;
namespace rct = fincept::ui::chart_ticks;

namespace {

constexpr double kRcrNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kRcrInf = std::numeric_limits<double>::infinity();
constexpr double kRcrPi = 3.14159265358979323846;
constexpr double kRcrLabelGap = 8.0;       ///< minimum px between two x-axis labels
constexpr double kRcrMinSliceLabel = 0.03; ///< slices below 3% get no outside label

// ── Fonts and measuring ──────────────────────────────────────────────────────

int rcr_px(int base, double chart_width) {
    const double f = chart_width < 360.0 ? 0.85 : 1.0;
    return std::max(6, static_cast<int>(std::lround(base * f)));
}

QFont rcr_font(const QString& family, int px, bool bold) {
    QStringList fams;
    const QString fam = family.trimmed();
    if (!fam.isEmpty())
        fams << fam;
    if (!fams.contains(QStringLiteral("Arial"), Qt::CaseInsensitive))
        fams << QStringLiteral("Arial");
    QFont f(fams.first());
    f.setFamilies(fams);
    f.setPixelSize(px);
    f.setWeight(bold ? QFont::Bold : QFont::Normal);
    f.setItalic(false);
    f.setUnderline(false);
    f.setStrikeOut(false);
    f.setCapitalization(QFont::MixedCase);
    f.setLetterSpacing(QFont::PercentageSpacing, 100.0);
    f.setWordSpacing(0.0);
    f.setKerning(true);
    // Unhinted outlines scale linearly, so the 96-dpi layout matches print-resolution output.
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QImage rcr_measure_image() {
    QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
    img.setDotsPerMeterX(3780); // 96 dpi
    img.setDotsPerMeterY(3780);
    return img;
}

/// The fonts of one chart and their metrics on a private 96-dpi device.
struct RcrText {
    QImage dev;
    QFont title, axis, legend, annot;
    QFontMetricsF fm_title, fm_axis, fm_legend, fm_annot;

    RcrText(const QString& family, double width)
        : dev(rcr_measure_image()), title(rcr_font(family, rcr_px(13, width), true)),
          axis(rcr_font(family, rcr_px(10, width), false)), legend(rcr_font(family, rcr_px(10, width), false)),
          annot(rcr_font(family, rcr_px(9, width), false)), fm_title(title, &dev), fm_axis(axis, &dev),
          fm_legend(legend, &dev), fm_annot(annot, &dev) {}
    RcrText(const RcrText&) = delete;
    RcrText& operator=(const RcrText&) = delete;
};

// ── Colours ──────────────────────────────────────────────────────────────────

QColor rcr_qcolor(const QString& hex, const QColor& fallback) {
    const QColor c = QColor::fromString(hex);
    return c.isValid() ? c : fallback;
}

QColor rcr_alpha(QColor c, double a) {
    c.setAlphaF(static_cast<float>(std::clamp(a, 0.0, 1.0)));
    return c;
}

/// The palette as QColors. Every colour is paper ink from the ChartPalette; Qt's black / white
/// are only the guard for a palette entry that does not parse.
struct RcrInk {
    const rcs::ChartPalette& pal;
    QColor paper, ink, heading, grid, muted;

    explicit RcrInk(const rcs::ChartPalette& p)
        : pal(p), paper(rcr_qcolor(p.paper, QColor(Qt::white))), ink(rcr_qcolor(p.ink, QColor(Qt::black))),
          heading(rcr_qcolor(p.heading, ink)), grid(rcr_qcolor(p.grid, rcr_alpha(ink, 0.3))),
          muted(rcr_qcolor(p.muted, ink)) {}

    QColor role(const QString& r) const { return rcr_qcolor(resolve_color(pal, r), ink); }

    /// Paper or ink, whichever reads on `bg`.
    QColor text_on(const QColor& bg) const {
        const double lum = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
        const bool paper_light = paper.lightnessF() >= ink.lightnessF();
        const QColor& light = paper_light ? paper : ink;
        const QColor& dark = paper_light ? ink : paper;
        return lum > 0.6 ? dark : light;
    }
};

// ── Numbers ──────────────────────────────────────────────────────────────────

struct RcrUnit {
    double div = 1.0;
    QString sym;
};

RcrUnit rcr_unit(double max_abs, bool compact) {
    if (!compact || !(max_abs >= 10000.0))
        return {};
    if (max_abs >= 1e12)
        return {1e12, QStringLiteral("T")};
    if (max_abs >= 1e9)
        return {1e9, QStringLiteral("B")};
    if (max_abs >= 1e6)
        return {1e6, QStringLiteral("M")};
    return {1e3, QStringLiteral("K")};
}

QString rcr_format(double v, const rcs::AxisFormat& f, int decimals, const RcrUnit& u) {
    if (!std::isfinite(v))
        return {};
    const int d = std::clamp(decimals, 0, 8);
    const double x = v / u.div;
    const double ax = std::abs(x);
    const bool zero = !(std::round(ax * std::pow(10.0, d)) > 0);
    const bool neg = x < 0 && !zero; // never "-0"
    return (neg ? QStringLiteral("-") : QString()) + f.prefix + QLocale().toString(ax, 'f', d) +
           (zero ? QString() : u.sym) + f.suffix; // "0", not "0M"
}

int rcr_axis_decimals(const rcs::AxisFormat& f, double step, const RcrUnit& u) {
    if (f.decimals >= 0)
        return f.decimals;
    return std::clamp(rct::decimals_for_step(step / u.div), 0, 6);
}

/// Precision of a single value label (annotations, bar ends): whole numbers stay whole,
/// others get two decimals (or one more than the axis when the axis needs more).
int rcr_value_decimals(double v, const rcs::AxisFormat& f, const RcrUnit& u, int axis_dec) {
    if (f.decimals >= 0)
        return f.decimals;
    if (u.div > 1.0)
        return std::abs(v / u.div) >= 100.0 ? 1 : 2;
    if (axis_dec == 0 && std::abs(v - std::round(v)) < 1e-9)
        return 0;
    return std::clamp(std::max(2, axis_dec + 1), 0, 6);
}

QString rcr_value_text(double v, const rcs::AxisFormat& f, int axis_dec) {
    const RcrUnit u = rcr_unit(std::abs(v), f.compact);
    return rcr_format(v, f, rcr_value_decimals(v, f, u, axis_dec), u);
}

/// One unit and precision for a set of value labels drawn side by side (bar ends), so they
/// read as a column: whole numbers stay whole, others get two decimals (more for tiny values).
struct RcrLabelFormat {
    RcrUnit unit;
    int dec = 0;
    QString text(double v, const rcs::AxisFormat& f) const { return rcr_format(v, f, dec, unit); }
};

RcrLabelFormat rcr_label_format(const QVector<double>& vals, const rcs::AxisFormat& f) {
    double max_abs = 0.0;
    bool whole = true;
    for (double v : vals) {
        if (!std::isfinite(v))
            continue;
        max_abs = std::max(max_abs, std::abs(v));
        if (std::abs(v - std::round(v)) > 1e-9)
            whole = false;
    }
    RcrLabelFormat out;
    out.unit = rcr_unit(max_abs, f.compact);
    if (f.decimals >= 0)
        out.dec = f.decimals;
    else if (out.unit.div > 1.0)
        out.dec = max_abs / out.unit.div >= 100.0 ? 1 : 2;
    else if (whole)
        out.dec = 0;
    else if (max_abs >= 1.0 || max_abs == 0.0)
        out.dec = 2;
    else
        out.dec = std::clamp(1 - static_cast<int>(std::floor(std::log10(max_abs))), 2, 6);
    return out;
}

QString rcr_percent(double fraction) {
    return QLocale().toString(fraction * 100.0, 'f', 1) + QLatin1Char('%');
}

bool rcr_ok(double v, bool log) {
    return std::isfinite(v) && (!log || v > 0.0);
}

bool rcr_any_finite(const QVector<double>& v) {
    for (double x : v)
        if (std::isfinite(x))
            return true;
    return false;
}

// ── Axes ─────────────────────────────────────────────────────────────────────

struct RcrRange {
    double lo = kRcrInf, hi = -kRcrInf;
    void add(double v) {
        if (!std::isfinite(v))
            return;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    bool valid() const { return lo <= hi; }
};

struct RcrAxis {
    double lo = 0.0, hi = 1.0;
    bool log = false;
    double step = 0.0;
    int dec = 0; ///< decimals of the tick labels (value labels build on it)
    double label_w = 0.0;
    rcs::AxisFormat fmt;
    RcrLabelFormat values; ///< value labels of this axis (last-value tags, high / low marks)
    QVector<Tick> ticks;

    /// 0 at lo, 1 at hi (NaN for a non-positive value on a log axis).
    double frac(double v) const {
        if (log) {
            if (!(v > 0.0))
                return kRcrNaN;
            const double a = std::log10(lo), b = std::log10(hi);
            return b > a ? (std::log10(v) - a) / (b - a) : 0.5;
        }
        return hi > lo ? (v - lo) / (hi - lo) : 0.5;
    }
};

/// The drawn range of `r`: 0 included for bars / areas, padding on the data's free ends
/// (padding never pushes an all-positive or all-negative range across zero).
void rcr_set_range(RcrAxis& ax, const RcrRange& r, bool include_zero, double pad_lo, double pad_hi) {
    double lo = r.valid() ? r.lo : 0.0;
    double hi = r.valid() ? r.hi : 1.0;
    if (ax.log) {
        double a = std::log10(lo), b = std::log10(hi);
        const double span = b - a;
        if (span <= 1e-12) {
            a -= 0.05;
            b += 0.05;
        } else {
            a -= span * pad_lo;
            b += span * pad_hi;
        }
        ax.lo = std::pow(10.0, a);
        ax.hi = std::pow(10.0, b);
        return;
    }
    if (include_zero) {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }
    const double span = hi - lo;
    if (span <= 0.0) {
        if (lo == 0.0 && hi == 0.0) {
            ax.lo = include_zero ? 0.0 : -1.0;
            ax.hi = 1.0;
        } else {
            const double m = std::abs(hi) * 0.1;
            ax.lo = lo - m;
            ax.hi = hi + m;
        }
        return;
    }
    const double data_lo = lo, data_hi = hi;
    if (!(include_zero && lo == 0.0))
        lo -= span * pad_lo;
    if (!(include_zero && hi == 0.0))
        hi += span * pad_hi;
    if (data_lo >= 0.0 && lo < 0.0)
        lo = 0.0;
    if (data_hi <= 0.0 && hi > 0.0)
        hi = 0.0;
    ax.lo = lo;
    ax.hi = hi;
}

/// Ticks of `ax` laid along [px_lo (at lo), px_hi (at hi)], labelled with the axis format.
void rcr_make_ticks(RcrAxis& ax, int max_ticks, double px_lo, double px_hi, const QFontMetricsF& fm, bool integers) {
    const rct::NumericTicks nt =
        ax.log ? rct::log_ticks(ax.lo, ax.hi, max_ticks) : rct::nice_ticks(ax.lo, ax.hi, max_ticks);
    QVector<double> vals;
    for (double v : nt.values)
        if (!integers || std::abs(v - std::round(v)) < 1e-9)
            vals.append(v);
    double max_abs = 0.0;
    for (double v : vals)
        max_abs = std::max(max_abs, std::abs(v));
    const RcrUnit u = rcr_unit(max_abs, ax.fmt.compact);
    const bool uniform = nt.step > 0.0;
    ax.step = nt.step;
    ax.dec = ax.fmt.decimals >= 0 ? ax.fmt.decimals : (uniform ? rcr_axis_decimals(ax.fmt, nt.step, u) : 0);
    ax.ticks.clear();
    ax.label_w = 0.0;
    for (double v : vals) {
        const double f = ax.frac(v);
        if (!std::isfinite(f))
            continue;
        QString label;
        if (uniform) {
            label = rcr_format(v, ax.fmt, ax.dec, u);
        } else { // log decades: each label in its own unit, as precise as itself (1, 10, 100, 1,000, 10K)
            const RcrUnit own = rcr_unit(std::abs(v), ax.fmt.compact);
            const int d = ax.fmt.decimals >= 0 ? ax.fmt.decimals
                                               : std::clamp(rct::decimals_for_step(std::abs(v) / own.div), 0, 6);
            label = rcr_format(v, ax.fmt, d, own);
        }
        Tick t{v, px_lo + f * (px_hi - px_lo), label};
        ax.label_w = std::max(ax.label_w, fm.horizontalAdvance(t.label));
        ax.ticks.append(t);
    }
}

double rcr_label_left(double pos, double w, double width) {
    return std::clamp(pos - w / 2.0, 0.0, std::max(0.0, width - w));
}

/// True when two x labels (centred on their ticks, kept inside the chart) would touch.
bool rcr_overlaps(const QVector<Tick>& ticks, const QFontMetricsF& fm, double width) {
    double last_right = -kRcrInf;
    for (const Tick& t : ticks) {
        const double w = fm.horizontalAdvance(t.label);
        const double l = rcr_label_left(t.pos, w, width);
        if (l < last_right + kRcrLabelGap)
            return true;
        last_right = l + w;
    }
    return false;
}

/// Drops x labels, left to right, until none touch.
void rcr_thin(QVector<Tick>& ticks, const QFontMetricsF& fm, double width) {
    QVector<Tick> kept;
    double last_right = -kRcrInf;
    for (const Tick& t : ticks) {
        const double w = fm.horizontalAdvance(t.label);
        const double l = rcr_label_left(t.pos, w, width);
        if (!kept.isEmpty() && l < last_right + kRcrLabelGap)
            continue;
        kept.append(t);
        last_right = l + w;
    }
    ticks = kept;
}

/// Pushes sorted positions apart by `step`, keeping them inside [lo, hi] where possible.
void rcr_spread(QVector<double>& ys, double step, double lo, double hi) {
    const int n = static_cast<int>(ys.size());
    if (n == 0)
        return;
    hi = std::max(lo, hi);
    for (double& y : ys)
        y = std::clamp(y, lo, hi);
    for (int i = 1; i < n; ++i)
        ys[i] = std::max(ys[i], ys[i - 1] + step);
    if (ys[n - 1] > hi) {
        ys[n - 1] = hi;
        for (int i = n - 2; i >= 0; --i)
            ys[i] = std::min(ys[i], ys[i + 1] - step);
    }
    if (ys[0] < lo) {
        ys[0] = lo;
        for (int i = 1; i < n; ++i)
            ys[i] = std::max(ys[i], ys[i - 1] + step);
    }
}

// ── Model ────────────────────────────────────────────────────────────────────

enum class RcrMark { Line, Area, Bar, Dot };

RcrMark rcr_mark_override(const QString& k, RcrMark def) {
    if (k == QLatin1String("line"))
        return RcrMark::Line;
    if (k == QLatin1String("bar"))
        return RcrMark::Bar;
    if (k == QLatin1String("area"))
        return RcrMark::Area;
    return def;
}

struct RcrSer {
    int src = 0; ///< index in spec.series
    RcrMark mark = RcrMark::Line;
    bool right = false;
    QString role; ///< colour role
    QString name;
};

struct RcrTag {
    QString text;
    QString role;
    double target_y = 0.0;
    QRectF rect;
};

struct RcrLegendItem {
    QRectF rect;
    QString text;
    QString role;
    QString shape;
};

struct RcrSliceLabel {
    QString text;
    QRectF rect;
    QPointF edge, elbow, anchor;
    bool right_side = true;
};

struct RcrModel {
    Layout lay;
    rcs::Kind kind = rcs::Kind::Line;
    QString title_text;
    QVector<RcrSer> ser;
    int n = 1;              ///< points on the index axis
    bool slotted = false;   ///< bar-style x: point i at the centre of slot i
    bool numeric_x = false; ///< scatter / histogram
    bool stacked = false;
    double x_lo = 0.0, x_hi = 1.0;
    RcrAxis y1, y2, hx; ///< hx: the horizontal value axis of hbar
    double vol_hi = 1.0;
    QVector<Tick> vol_ticks;
    double y1_col_w = 0.0; ///< width of the left label column
    double y2_x = 0.0;     ///< left edge of the right-axis labels
    QVector<RcrTag> tags;
    QVector<RcrLegendItem> legend_items;
    QVector<QString> slice_names;
    QVector<double> slice_values;
    QVector<RcrSliceLabel> slice_labels;
    QPointF pie_c;
    double pie_r = 0.0;
    double cat_x = 0.0, cat_w = 0.0; ///< hbar category labels
    RcrLabelFormat bar_labels;        ///< hbar value labels: one unit and precision for all
    QVector<QString> cat_text;
};

double rcr_x_index(const RcrModel& m, double fi) {
    const QRectF& pl = m.lay.plot;
    if (m.slotted)
        return pl.left() + (fi + 0.5) * pl.width() / std::max(1, m.n);
    if (m.n <= 1)
        return pl.center().x();
    return pl.left() + fi * pl.width() / (m.n - 1);
}

double rcr_x_num(const RcrModel& m, double x) {
    const QRectF& pl = m.lay.plot;
    return m.x_hi > m.x_lo ? pl.left() + (x - m.x_lo) / (m.x_hi - m.x_lo) * pl.width() : pl.center().x();
}

double rcr_y(const RcrModel& m, const RcrAxis& ax, double v) {
    const QRectF& pl = m.lay.plot;
    return pl.bottom() - ax.frac(v) * pl.height();
}

double rcr_scatter_x(const rcs::Series& s, int j) {
    if (s.x.size() == s.values.size())
        return s.x[j];
    return j;
}

/// Stacked bands: positives stack up from 0, negatives down, separately; gaps count as 0.
struct RcrBands {
    QVector<QVector<double>> lower, upper;
};

RcrBands rcr_stack(const RcrModel& m, const rcs::ChartSpec& spec) {
    RcrBands b;
    QVector<double> pos(m.n, 0.0), neg(m.n, 0.0);
    for (const RcrSer& s : m.ser) {
        const QVector<double>& v = spec.series[s.src].values;
        QVector<double> lo(m.n, 0.0), hi(m.n, 0.0);
        for (int i = 0; i < m.n; ++i) {
            const double x = i < v.size() && std::isfinite(v[i]) ? v[i] : 0.0;
            if (x >= 0.0) {
                lo[i] = pos[i];
                pos[i] += x;
                hi[i] = pos[i];
            } else {
                hi[i] = neg[i];
                neg[i] += x;
                lo[i] = neg[i];
            }
        }
        b.lower.append(lo);
        b.upper.append(hi);
    }
    return b;
}

/// Fractional point index of time `t` on an ascending date axis (interpolated between points).
double rcr_frac_index(const QVector<qint64>& d, double t) {
    const int n = static_cast<int>(d.size());
    if (n == 0 || t < static_cast<double>(d.first()) || t > static_cast<double>(d.last()))
        return kRcrNaN;
    const auto it =
        std::upper_bound(d.cbegin(), d.cend(), t, [](double a, qint64 b) { return a < static_cast<double>(b); });
    const int j = static_cast<int>(it - d.cbegin()) - 1;
    if (j < 0)
        return 0.0;
    if (j >= n - 1)
        return n - 1;
    const double a = static_cast<double>(d[j]), b = static_cast<double>(d[j + 1]);
    return b > a ? j + (t - a) / (b - a) : j;
}

bool rcr_dates_ascending(const QVector<qint64>& d) {
    for (int i = 1; i < d.size(); ++i)
        if (d[i] < d[i - 1])
            return false;
    return true;
}

/// Where an event sits on the index axis: an exact label, an ISO date (same local day, or
/// between two points), or a point index.
double rcr_event_index(const rcs::ChartSpec& spec, const QString& key, int n) {
    const qsizetype li = spec.labels.indexOf(key);
    if (li >= 0 && li < n)
        return static_cast<double>(li);
    if (!spec.dates_ms.isEmpty() && key.size() >= 10) {
        const QDate day = QDate::fromString(key.left(10), Qt::ISODate);
        if (day.isValid()) {
            const QTimeZone local(QTimeZone::LocalTime);
            for (int i = 0; i < spec.dates_ms.size(); ++i)
                if (QDateTime::fromMSecsSinceEpoch(spec.dates_ms[i], local).date() == day)
                    return i;
            if (rcr_dates_ascending(spec.dates_ms))
                return rcr_frac_index(spec.dates_ms, static_cast<double>(day.startOfDay(local).toMSecsSinceEpoch()));
            return kRcrNaN;
        }
    }
    bool ok = false;
    const int idx = key.toInt(&ok);
    if (ok && idx >= 0 && idx < n)
        return idx;
    return kRcrNaN;
}

double rcr_quantile(const QVector<double>& sorted, double q) {
    const int n = static_cast<int>(sorted.size());
    if (n == 0)
        return kRcrNaN;
    const double h = (n - 1) * q;
    const int lo = static_cast<int>(std::floor(h));
    if (lo + 1 >= n)
        return sorted[n - 1];
    return sorted[lo] + (h - lo) * (sorted[lo + 1] - sorted[lo]);
}

void rcr_histogram(RcrModel& m, const rcs::ChartSpec& spec) {
    Layout& L = m.lay;
    QVector<double> v;
    if (!m.ser.isEmpty())
        for (double x : spec.series[m.ser.first().src].values)
            if (std::isfinite(x))
                v.append(x);
    std::sort(v.begin(), v.end());
    const int n = static_cast<int>(v.size());
    if (n == 0)
        return;
    int bins = spec.bins;
    if (bins <= 0) {
        // Freedman–Diaconis, Sturges when the spread is degenerate.
        const double iqr = rcr_quantile(v, 0.75) - rcr_quantile(v, 0.25);
        const double range = v.last() - v.first();
        double b = 0.0;
        if (iqr > 0.0 && range > 0.0)
            b = std::ceil(range / (2.0 * iqr / std::cbrt(static_cast<double>(n))));
        if (!(b >= 1.0) || !std::isfinite(b))
            b = std::ceil(std::log2(static_cast<double>(n))) + 1.0;
        bins = std::clamp(static_cast<int>(std::min(b, 1000.0)), 5, 40);
    }
    double lo = v.first(), hi = v.last();
    if (!(hi > lo)) {
        const double d = std::abs(lo) > 0.0 ? std::abs(lo) * 0.05 : 0.5;
        lo -= d;
        hi += d;
    }
    L.bin_edges.clear();
    for (int k = 0; k <= bins; ++k)
        L.bin_edges.append(k == bins ? hi : lo + (hi - lo) * k / bins);
    L.bin_counts = QVector<int>(bins, 0);
    for (double x : v) {
        const int k = std::clamp(static_cast<int>(std::floor((x - lo) / (hi - lo) * bins)), 0, bins - 1);
        ++L.bin_counts[k];
    }
}

void rcr_collect_series(RcrModel& m, const rcs::ChartSpec& spec) {
    if (spec.kind == rcs::Kind::Candle) {
        m.lay.series_count = 1;
        return;
    }
    for (int i = 0; i < spec.series.size(); ++i) {
        const rcs::Series& s = spec.series[i];
        if (!rcr_any_finite(s.values))
            continue;
        RcrSer r;
        r.src = i;
        r.name = s.name.trimmed().isEmpty() ? QStringLiteral("Series %1").arg(i + 1) : s.name.trimmed();
        r.right = s.right_axis;
        switch (spec.kind) {
            case rcs::Kind::Line:
                r.mark = rcr_mark_override(s.kind, RcrMark::Line);
                break;
            case rcs::Kind::Area:
                r.mark = rcr_mark_override(s.kind, RcrMark::Area);
                break;
            case rcs::Kind::Bar:
                r.mark = rcr_mark_override(s.kind, RcrMark::Bar);
                break;
            case rcs::Kind::StackedArea:
                r.mark = RcrMark::Area;
                r.right = false;
                break;
            case rcs::Kind::Scatter:
                r.mark = RcrMark::Dot;
                r.right = false;
                break;
            case rcs::Kind::StackedBar:
            case rcs::Kind::HBar:
            case rcs::Kind::Histogram:
            case rcs::Kind::Donut:
            case rcs::Kind::Pie:
            case rcs::Kind::Candle:
                r.mark = RcrMark::Bar;
                r.right = false;
                break;
        }
        m.ser.append(r);
        if (spec.kind == rcs::Kind::Histogram)
            break; // one distribution
    }
    // A right-hand axis needs a left-hand one; otherwise everything is on the left.
    bool any_left = false, any_right = false;
    for (const RcrSer& r : m.ser) {
        if (r.right)
            any_right = true;
        else
            any_left = true;
    }
    if (!any_left)
        for (RcrSer& r : m.ser)
            r.right = false;
    m.lay.has_y2 = any_left && any_right;
    m.lay.series_count = static_cast<int>(m.ser.size());
    for (int k = 0; k < m.ser.size(); ++k)
        m.ser[k].role = m.ser.size() == 1 ? QStringLiteral("accent") : QStringLiteral("series:%1").arg(k);
}

/// Donut / pie slices: the values of the first series named by the labels, or, when every
/// series holds one value, one slice per series. Only positive values make slices.
void rcr_collect_slices(RcrModel& m, const rcs::ChartSpec& spec) {
    if (m.ser.isEmpty())
        return;
    const rcs::Series& first = spec.series[m.ser.first().src];
    int finite = 0;
    for (double v : first.values)
        if (std::isfinite(v))
            ++finite;
    if (finite >= 2 || m.ser.size() == 1) {
        for (int j = 0; j < first.values.size(); ++j) {
            const double v = first.values[j];
            if (!std::isfinite(v) || v <= 0.0)
                continue;
            const QString label = j < spec.labels.size() ? spec.labels[j].trimmed() : QString();
            m.slice_names.append(label.isEmpty() ? QStringLiteral("Item %1").arg(j + 1) : label);
            m.slice_values.append(v);
        }
    } else {
        for (const RcrSer& r : m.ser) {
            for (double v : spec.series[r.src].values) {
                if (!std::isfinite(v))
                    continue;
                if (v > 0.0) {
                    m.slice_names.append(r.name);
                    m.slice_values.append(v);
                }
                break;
            }
        }
    }
    double total = 0.0;
    for (double v : m.slice_values)
        total += v;
    if (total > 0.0)
        for (double v : m.slice_values)
            m.lay.slice_fractions.append(v / total);
}

// ── Legend ───────────────────────────────────────────────────────────────────

struct RcrLegendSrc {
    QString name, role, shape;
};

QVector<RcrLegendSrc> rcr_legend_entries(const RcrModel& m) {
    QVector<RcrLegendSrc> out;
    if (m.kind == rcs::Kind::Candle) {
        out.append({QStringLiteral("Up"), QStringLiteral("up"), QStringLiteral("box")});
        out.append({QStringLiteral("Down"), QStringLiteral("down"), QStringLiteral("box")});
        return out;
    }
    if (m.kind == rcs::Kind::Donut || m.kind == rcs::Kind::Pie) {
        for (int k = 0; k < m.slice_names.size(); ++k)
            out.append({m.slice_names[k], QStringLiteral("series:%1").arg(k), QStringLiteral("box")});
        return out;
    }
    for (const RcrSer& s : m.ser) {
        const QString shape = s.mark == RcrMark::Line  ? QStringLiteral("line")
                              : s.mark == RcrMark::Dot ? QStringLiteral("dot")
                                                       : QStringLiteral("box");
        out.append({s.name, s.role, shape});
    }
    return out;
}

QString rcr_legend_position(const rcs::ChartSpec& spec, const RcrModel& m, int entries, double width) {
    if (entries == 0 || spec.legend == QLatin1String("none"))
        return QStringLiteral("none");
    const bool is_round = m.kind == rcs::Kind::Donut || m.kind == rcs::Kind::Pie;
    if (spec.legend == QLatin1String("top") || spec.legend == QLatin1String("bottom") ||
        spec.legend == QLatin1String("right"))
        return spec.legend;
    if (is_round)
        return width < 300.0 ? QStringLiteral("bottom") : QStringLiteral("right");
    return m.lay.series_count > 1 ? QStringLiteral("bottom") : QStringLiteral("none");
}

/// Places the legend and takes its room out of `area`.
void rcr_layout_legend(RcrModel& m, QRectF& area, const RcrText& tx, const QVector<RcrLegendSrc>& entries) {
    Layout& L = m.lay;
    const QString& pos = L.legend_position;
    if (pos == QLatin1String("none") || entries.isEmpty())
        return;
    const QFontMetricsF& fm = tx.fm_legend;
    const double lh = std::ceil(fm.height()) + 3.0;
    const double swatch = 17.0;
    const double gap = 14.0;
    const bool is_round = m.kind == rcs::Kind::Donut || m.kind == rcs::Kind::Pie;

    if (pos == QLatin1String("right")) {
        const double max_text = std::max(30.0, area.width() * 0.38 - swatch);
        double col_w = 0.0;
        for (const RcrLegendSrc& e : entries)
            col_w = std::max(col_w, swatch + std::min(max_text, fm.horizontalAdvance(e.name)));
        col_w = std::ceil(col_w);
        const int fit = std::max(1, static_cast<int>(area.height() / lh));
        const int count = std::min(static_cast<int>(entries.size()), fit);
        const double h = count * lh;
        const double y0 = is_round ? area.center().y() - h / 2.0 : area.top();
        L.legend_rect = QRectF(area.right() - col_w, y0, col_w, h);
        for (int k = 0; k < count; ++k) {
            const RcrLegendSrc& e = entries[k];
            m.legend_items.append({QRectF(L.legend_rect.left(), y0 + k * lh, col_w, lh),
                                   fm.elidedText(e.name, Qt::ElideRight, col_w - swatch), e.role, e.shape});
            L.legend.append({e.name, e.role, e.shape});
        }
        area.setRight(area.right() - col_w - 12.0);
        return;
    }

    // top / bottom: rows that flow left to right, at most three.
    const double max_text = std::max(40.0, std::min(220.0, area.width() * 0.5));
    double x = area.left();
    int row = 0;
    QVector<RcrLegendItem> items;
    for (const RcrLegendSrc& e : entries) {
        const QString text = fm.elidedText(e.name, Qt::ElideRight, max_text);
        const double w = std::ceil(swatch + fm.horizontalAdvance(text)) + 1.0;
        if (x > area.left() && x + w > area.right()) {
            ++row;
            x = area.left();
        }
        if (row >= 3)
            break;
        items.append({QRectF(x, row * lh, w, lh), text, e.role, e.shape});
        L.legend.append({e.name, e.role, e.shape});
        x += w + gap;
    }
    const int rows = items.isEmpty() ? 0 : row + (row >= 3 ? 0 : 1);
    const double h = std::min(3, rows) * lh;
    const double y0 = pos == QLatin1String("top") ? area.top() : area.bottom() - h;
    L.legend_rect = QRectF(area.left(), y0, area.width(), h);
    for (RcrLegendItem& it : items) {
        it.rect.translate(0.0, y0);
        m.legend_items.append(it);
    }
    if (pos == QLatin1String("top"))
        area.setTop(area.top() + h + 6.0);
    else
        area.setBottom(area.bottom() - h - 6.0);
}

// ── Kind layouts ─────────────────────────────────────────────────────────────

/// The x labels of the densest candidate set that fits. `build(max_ticks)` makes a candidate
/// set; sets are tried from `cap` (a comfortable density) towards coarser ones, and when that
/// leaves fewer than two labels, towards finer ones. A set that fits as it is wins; otherwise
/// the thinned set with the most labels.
template <typename Build>
QVector<Tick> rcr_pick_ticks(const QFontMetricsF& fm, double width, int cap, Build build) {
    QVector<Tick> best;
    auto offer = [&](const QVector<Tick>& cand) {
        if (!cand.isEmpty() && !rcr_overlaps(cand, fm, width)) {
            if (cand.size() >= best.size())
                best = cand;
            return cand.size() >= 2;
        }
        QVector<Tick> thinned = cand;
        rcr_thin(thinned, fm, width);
        if (thinned.size() > best.size())
            best = thinned;
        return false;
    };
    for (int mt = cap; mt >= 2; --mt)
        if (offer(build(mt)))
            return best;
    if (best.size() < 2)
        for (int mt = cap + 1; mt <= cap + 12; ++mt)
            if (offer(build(mt)))
                return best;
    return best;
}

void rcr_x_ticks_numeric(RcrModel& m, const rcs::AxisFormat& fmt, const RcrText& tx) {
    Layout& L = m.lay;
    RcrAxis ax;
    ax.lo = m.x_lo;
    ax.hi = m.x_hi;
    ax.fmt = fmt;
    const int cap = std::clamp(static_cast<int>(L.plot.width() / 64.0), 2, 10);
    L.x_ticks = rcr_pick_ticks(tx.fm_axis, L.outer.width(), cap, [&](int mt) {
        rcr_make_ticks(ax, mt, L.plot.left(), L.plot.right(), tx.fm_axis, false);
        return ax.ticks;
    });
}

/// Calendar labels show the larger unit only where it starts (January shows the year,
/// midnight the date). When no tick starts one, the first label carries it ("Feb 2025",
/// "16 Nov 10:00"), so a reader always knows the year / day.
void rcr_anchor_time_labels(QVector<Tick>& ticks, const rct::TimeStep& step) {
    if (ticks.isEmpty() || step.unit == rct::TimeUnit::Year)
        return;
    const bool intraday =
        step.unit == rct::TimeUnit::Second || step.unit == rct::TimeUnit::Minute || step.unit == rct::TimeUnit::Hour;
    const QTimeZone local(QTimeZone::LocalTime);
    auto at = [&](const Tick& t) { return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(t.value)), local); };
    for (const Tick& t : ticks) {
        const QDateTime dt = at(t);
        if (intraday ? dt.time() == QTime(0, 0) : (dt.date().month() == 1 && dt.date().day() == 1))
            return;
    }
    const QLocale loc;
    const QDateTime first = at(ticks.first());
    if (intraday)
        ticks.first().label = loc.toString(first.date(), QStringLiteral("d MMM ")) + ticks.first().label;
    else if (step.unit == rct::TimeUnit::Month)
        ticks.first().label = loc.toString(first.date(), QStringLiteral("MMM yyyy"));
    else
        ticks.first().label = loc.toString(first.date(), QStringLiteral("d MMM yyyy"));
}

void rcr_x_ticks_index(RcrModel& m, const rcs::ChartSpec& spec, const RcrText& tx) {
    Layout& L = m.lay;
    const QFontMetricsF& fm = tx.fm_axis;
    const double width = L.outer.width();
    const int n = m.n;
    const QVector<qint64>& dates = spec.dates_ms;
    const bool dated = dates.size() == n;

    if (dated && n >= 2 && rcr_dates_ascending(dates) && dates.last() > dates.first()) {
        const double d0 = static_cast<double>(dates.first()), d1 = static_cast<double>(dates.last());
        const double sample = fm.horizontalAdvance(QStringLiteral("28 Sep")) + 16.0;
        const int cap = std::clamp(static_cast<int>(L.plot.width() / sample), 2, 12);
        QVector<Tick> ticks = rcr_pick_ticks(fm, width, cap, [&](int mt) {
            const rct::TimeTicks tt = rct::time_ticks(d0, d1, mt);
            QVector<Tick> cand;
            for (double v : tt.values) {
                const double fi = rcr_frac_index(dates, v);
                if (std::isfinite(fi))
                    cand.append(Tick{v, rcr_x_index(m, fi), rct::time_tick_label(v, tt.step)});
            }
            rcr_anchor_time_labels(cand, tt.step);
            return cand;
        });
        if (ticks.isEmpty())
            ticks.append(Tick{d0, rcr_x_index(m, 0.0), rct::time_point_label(d0, 0.0)});
        L.x_ticks = ticks;
        return;
    }

    QVector<QString> texts(n);
    bool numbered = false;
    if (dated) {
        for (int i = 0; i < n; ++i)
            texts[i] = rct::time_point_label(static_cast<double>(dates[i]), 0.0);
    } else if (spec.labels.isEmpty()) {
        numbered = true;
        for (int i = 0; i < n; ++i)
            texts[i] = QString::number(i + 1);
    } else {
        for (int i = 0; i < n; ++i)
            texts[i] = i < spec.labels.size() ? spec.labels[i].simplified() : QString();
    }
    double widest = 0.0;
    for (QString& t : texts) {
        t = fm.elidedText(t, Qt::ElideRight, 140.0);
        widest = std::max(widest, fm.horizontalAdvance(t));
    }
    const double slot = m.slotted ? L.plot.width() / n : (n > 1 ? L.plot.width() / (n - 1) : L.plot.width());
    // A few bars: shorten the names rather than skip categories.
    if (m.slotted && n <= 16 && widest + kRcrLabelGap > slot) {
        const double avail = slot - kRcrLabelGap;
        if (avail >= fm.horizontalAdvance(QStringLiteral("Mmm") + QChar(0x2026))) {
            for (QString& t : texts)
                t = fm.elidedText(t, Qt::ElideRight, avail);
            widest = std::min(widest, avail);
        }
    }
    int k = std::max(1, static_cast<int>(std::ceil((widest + kRcrLabelGap) / std::max(1e-6, slot))));
    QVector<Tick> ticks;
    if (numbered && k > 1) {
        k = static_cast<int>(rct::nice_number(k, false));
        ticks.append(Tick{0.0, rcr_x_index(m, 0.0), texts[0]});
        for (int i = k - 1; i < n; i += k)
            if (i > 0)
                ticks.append(Tick{static_cast<double>(i), rcr_x_index(m, i), texts[i]});
    } else {
        for (int i = 0; i < n; i += k)
            if (!texts[i].isEmpty())
                ticks.append(Tick{static_cast<double>(i), rcr_x_index(m, i), texts[i]});
    }
    rcr_thin(ticks, fm, width);
    L.x_ticks = ticks;
}

/// line / area / bars / stacked / candle / scatter / histogram.
void rcr_layout_xy(RcrModel& m, const rcs::ChartSpec& spec, const QRectF& area, const RcrText& tx) {
    Layout& L = m.lay;
    const QFontMetricsF& fa = tx.fm_axis;
    const double lh = fa.height();
    const bool candle = m.kind == rcs::Kind::Candle;
    const bool scatter = m.kind == rcs::Kind::Scatter;
    const bool hist = m.kind == rcs::Kind::Histogram;
    m.stacked = m.kind == rcs::Kind::StackedArea || m.kind == rcs::Kind::StackedBar;
    m.n = std::max(1, spec.point_count());
    m.numeric_x = scatter || hist;
    bool any_bar = false;
    for (const RcrSer& s : m.ser)
        if (s.mark == RcrMark::Bar)
            any_bar = true;
    m.slotted = candle || (any_bar && !hist);
    const bool log_kind = m.kind == rcs::Kind::Line || m.kind == rcs::Kind::Area || candle || scatter;
    const bool want_log = spec.log_y && log_kind && !any_bar;

    // Data ranges (linear and positive-only for a log axis).
    RcrRange r1, r2, r1_log, r2_log;
    bool zero1 = false, zero2 = false;
    if (hist) {
        rcr_histogram(m, spec);
        int maxc = 0;
        for (int c : L.bin_counts)
            maxc = std::max(maxc, c);
        r1.add(0.0);
        r1.add(std::max(1, maxc));
        zero1 = true;
        if (!L.bin_edges.isEmpty()) {
            m.x_lo = L.bin_edges.first();
            m.x_hi = L.bin_edges.last();
        }
    } else if (candle) {
        for (const rcs::Candle& c : spec.ohlc) {
            for (double v : {c.low, c.high, c.open, c.close}) {
                r1.add(v);
                if (v > 0.0)
                    r1_log.add(v);
            }
        }
    } else if (m.stacked) {
        const RcrBands b = rcr_stack(m, spec);
        for (int k = 0; k < b.lower.size(); ++k)
            for (int i = 0; i < m.n; ++i) {
                r1.add(b.lower[k][i]);
                r1.add(b.upper[k][i]);
            }
        zero1 = true;
    } else {
        RcrRange xr;
        for (const RcrSer& s : m.ser) {
            RcrRange& r = s.right ? r2 : r1;
            RcrRange& rl = s.right ? r2_log : r1_log;
            if (s.mark == RcrMark::Bar || s.mark == RcrMark::Area) {
                if (s.right)
                    zero2 = true;
                else
                    zero1 = true;
            }
            const rcs::Series& ser = spec.series[s.src];
            for (int j = 0; j < ser.values.size(); ++j) {
                const double v = ser.values[j];
                if (scatter) {
                    const double x = rcr_scatter_x(ser, j);
                    if (!std::isfinite(x) || !std::isfinite(v))
                        continue;
                    xr.add(x);
                }
                r.add(v);
                if (v > 0.0)
                    rl.add(v);
            }
        }
        if (scatter && xr.valid()) {
            const double span = xr.hi - xr.lo;
            const double padx = span > 0.0 ? span * 0.05 : (xr.hi != 0.0 ? std::abs(xr.hi) * 0.1 : 1.0);
            m.x_lo = xr.lo - padx;
            m.x_hi = xr.hi + padx;
        }
    }
    if (!r1.valid()) { // e.g. a scatter without a single (x, y) pair
        L.empty = true;
        L.plot = area;
        return;
    }
    m.y1.log = want_log && r1_log.valid();
    m.y2.log = want_log && r2_log.valid();

    // Vertical geometry: the plot (and the volume pane) between the top label half-line and
    // the x-axis labels.
    const double top = area.top() + std::ceil(lh / 2.0);
    const double bottom = area.bottom() - std::ceil(lh + 6.0);
    const double plot_h = std::max(16.0, bottom - top);
    const bool vol = candle && !spec.volume.isEmpty();
    const double vol_gap = 8.0;
    double price_h = plot_h, vol_h = 0.0;
    if (vol) {
        vol_h = std::max(10.0, std::round(plot_h * 0.22));
        price_h = std::max(12.0, plot_h - vol_h - vol_gap);
    }
    double pad_lo = 0.05, pad_hi = 0.05;
    if (spec.annot_minmax && !hist) { // room for the high / low marks
        const double need = (tx.fm_annot.height() + 12.0) / price_h;
        pad_lo = std::max(pad_lo, need);
        pad_hi = std::max(pad_hi, need);
    }
    if (hist) {
        m.y1.fmt = rcs::AxisFormat();
        m.y1.fmt.decimals = 0;
    } else {
        m.y1.fmt = spec.y_fmt;
    }
    m.y2.fmt = spec.y2_fmt;
    {
        QVector<double> v1, v2;
        if (candle)
            for (const rcs::Candle& c : spec.ohlc)
                v1 << c.open << c.high << c.low << c.close;
        else
            for (const RcrSer& s : m.ser)
                (s.right ? v2 : v1) += spec.series[s.src].values;
        m.y1.values = rcr_label_format(v1, m.y1.fmt);
        m.y2.values = rcr_label_format(v2, m.y2.fmt);
    }
    rcr_set_range(m.y1, m.y1.log ? r1_log : r1, zero1 && !m.y1.log, pad_lo, pad_hi);
    if (L.has_y2)
        rcr_set_range(m.y2, m.y2.log ? r2_log : r2, zero2 && !m.y2.log, pad_lo, pad_hi);
    const int mt = std::clamp(static_cast<int>(price_h / (lh * 2.4)), 2, 6);
    rcr_make_ticks(m.y1, mt, top + price_h, top, fa, hist);
    if (L.has_y2)
        rcr_make_ticks(m.y2, mt, top + price_h, top, fa, false);

    double vol_w = 0.0;
    if (vol) {
        double vmax = 0.0;
        for (double v : spec.volume)
            if (std::isfinite(v) && v > vmax)
                vmax = v;
        m.vol_hi = vmax > 0.0 ? vmax * 1.08 : 1.0;
        RcrAxis va;
        va.lo = 0.0;
        va.hi = m.vol_hi;
        const double vt = top + price_h + vol_gap;
        rcr_make_ticks(va, 2, vt + vol_h, vt, fa, false);
        for (const Tick& t : va.ticks)
            if (t.value > 0.0) {
                m.vol_ticks.append(t);
                vol_w = std::max(vol_w, fa.horizontalAdvance(t.label));
            }
    }

    // The vertical extent is final; tags can be placed in y before the width is known.
    L.plot = QRectF(area.left(), top, area.width(), price_h);
    QVector<RcrTag> tags;
    if (spec.annot_last && candle) {
        for (int i = static_cast<int>(spec.ohlc.size()) - 1; i >= 0; --i) {
            const rcs::Candle& c = spec.ohlc[i];
            if (!rcr_ok(c.close, m.y1.log))
                continue;
            tags.append({m.y1.values.text(c.close, m.y1.fmt),
                         c.close >= c.open ? QStringLiteral("up") : QStringLiteral("down"), rcr_y(m, m.y1, c.close),
                         QRectF()});
            break;
        }
    } else if (spec.annot_last && !scatter && !hist) {
        const RcrBands bands = m.stacked ? rcr_stack(m, spec) : RcrBands();
        for (int k = 0; k < m.ser.size(); ++k) {
            const RcrSer& s = m.ser[k];
            if (s.mark != RcrMark::Line && s.mark != RcrMark::Area)
                continue;
            const RcrAxis& ax = s.right ? m.y2 : m.y1;
            const QVector<double>& v = spec.series[s.src].values;
            for (int j = std::min(static_cast<int>(v.size()), m.n) - 1; j >= 0; --j) {
                if (!rcr_ok(v[j], ax.log))
                    continue;
                double yv = v[j];
                if (m.stacked)
                    yv = v[j] >= 0.0 ? bands.upper[k][j] : bands.lower[k][j];
                tags.append({ax.values.text(v[j], ax.fmt), s.role, rcr_y(m, ax, yv), QRectF()});
                break;
            }
        }
    }
    double tag_w = 0.0;
    for (const RcrTag& t : tags)
        tag_w = std::max(tag_w, std::ceil(tx.fm_annot.horizontalAdvance(t.text)) + 8.0);

    // Horizontal geometry: left labels | plot | last-value tags | right-axis labels.
    m.y1_col_w = std::ceil(std::max(m.y1.label_w, vol_w));
    const double left_w = m.y1_col_w + 6.0;
    const double right_w = L.has_y2 ? std::ceil(m.y2.label_w) + 6.0 : 0.0;
    const double tag_zone = tag_w > 0.0 ? tag_w + 4.0 : 0.0;
    const double left = area.left() + left_w;
    double right = area.right() - right_w - tag_zone;
    if (right_w == 0.0 && tag_zone == 0.0)
        right -= 2.0;
    if (right - left < 24.0)
        right = left + 24.0;
    L.plot = QRectF(left, top, right - left, price_h);
    if (vol)
        L.volume_plot = QRectF(left, top + price_h + vol_gap, right - left, vol_h);
    m.y2_x = L.plot.right() + tag_zone + 6.0;

    if (!tags.isEmpty()) {
        std::stable_sort(tags.begin(), tags.end(),
                         [](const RcrTag& a, const RcrTag& b) { return a.target_y < b.target_y; });
        const double th = std::ceil(tx.fm_annot.height()) + 2.0;
        QVector<double> ys;
        for (const RcrTag& t : tags)
            ys.append(t.target_y);
        rcr_spread(ys, th + 1.0, L.plot.top() + th / 2.0, L.plot.bottom() - th / 2.0);
        for (int i = 0; i < tags.size(); ++i)
            tags[i].rect = QRectF(L.plot.right() + 3.0, ys[i] - th / 2.0, tag_w, th);
        m.tags = tags;
    }

    if (scatter)
        rcr_x_ticks_numeric(m, rcs::AxisFormat(), tx);
    else if (hist)
        rcr_x_ticks_numeric(m, spec.y_fmt, tx);
    else
        rcr_x_ticks_index(m, spec, tx);

    L.y_min = m.y1.lo;
    L.y_max = m.y1.hi;
    L.log_y = m.y1.log;
    L.y_ticks = m.y1.ticks;
    if (L.has_y2) {
        L.y2_min = m.y2.lo;
        L.y2_max = m.y2.hi;
        L.y2_ticks = m.y2.ticks;
    }
}

void rcr_layout_hbar(RcrModel& m, const rcs::ChartSpec& spec, const QRectF& area, const RcrText& tx) {
    Layout& L = m.lay;
    const QFontMetricsF& fa = tx.fm_axis;
    const double lh = fa.height();
    m.n = std::max(1, spec.point_count());
    const int n = m.n;

    const double max_cat = std::max(40.0, area.width() * 0.32);
    m.cat_text = QVector<QString>(n);
    double cat_w = 0.0;
    for (int i = 0; i < n; ++i) {
        QString t = i < spec.labels.size() ? spec.labels[i].simplified()
                                           : (spec.labels.isEmpty() ? QString::number(i + 1) : QString());
        t = fa.elidedText(t, Qt::ElideRight, max_cat);
        cat_w = std::max(cat_w, fa.horizontalAdvance(t));
        m.cat_text[i] = t;
    }

    m.hx.fmt = spec.y_fmt;
    RcrRange r;
    bool any_neg = false;
    QVector<double> all;
    for (const RcrSer& s : m.ser)
        for (double v : spec.series[s.src].values) {
            if (!std::isfinite(v))
                continue;
            r.add(v);
            any_neg = any_neg || v < 0.0;
            all.append(v);
        }
    m.bar_labels = rcr_label_format(all, m.hx.fmt);
    double val_w = 0.0;
    for (double v : all)
        val_w = std::max(val_w, tx.fm_annot.horizontalAdvance(m.bar_labels.text(v, m.hx.fmt)));
    val_w = std::ceil(val_w) + 6.0;

    const double top = area.top() + 2.0;
    const double bottom = area.bottom() - std::ceil(lh + 6.0);
    m.cat_x = area.left();
    m.cat_w = std::ceil(cat_w);
    const double left = area.left() + m.cat_w + 8.0 + (any_neg ? val_w : 0.0);
    double right = area.right() - val_w;
    if (right - left < 24.0)
        right = left + 24.0;
    L.plot = QRectF(left, top, right - left, std::max(12.0, bottom - top));

    rcr_set_range(m.hx, r, true, 0.03, 0.03);
    const int cap = std::clamp(static_cast<int>(L.plot.width() / 64.0), 2, 8);
    m.hx.ticks = rcr_pick_ticks(fa, L.outer.width(), cap, [&](int mt) {
        rcr_make_ticks(m.hx, mt, L.plot.left(), L.plot.right(), fa, false);
        return m.hx.ticks;
    });
    L.x_ticks = m.hx.ticks;
    L.y_min = m.hx.lo;
    L.y_max = m.hx.hi;
    // Category names: every k-th one when the bands are shorter than a line of text.
    const double band = L.plot.height() / n;
    const int every = std::max(1, static_cast<int>(std::ceil((lh + 1.0) / std::max(1e-6, band))));
    for (int i = 0; i < n; i += every)
        L.y_ticks.append(Tick{static_cast<double>(i), L.plot.top() + (i + 0.5) * band, m.cat_text[i]});
}

void rcr_layout_pie(RcrModel& m, const QRectF& area, const RcrText& tx) {
    Layout& L = m.lay;
    const QFontMetricsF& fn = tx.fm_annot;
    const double lh = fn.height();
    const int k = static_cast<int>(L.slice_fractions.size());
    QVector<QString> texts(k);
    double widest = 0.0;
    for (int i = 0; i < k; ++i) {
        if (L.slice_fractions[i] < kRcrMinSliceLabel)
            continue;
        texts[i] = m.slice_names[i] + QLatin1Char(' ') + rcr_percent(L.slice_fractions[i]);
        widest = std::max(widest, fn.horizontalAdvance(texts[i]));
    }
    const double side = widest > 0.0 ? std::min(widest + 22.0, area.width() * 0.3) : 4.0;
    const double vside = widest > 0.0 ? lh + 4.0 : 4.0;
    double r = std::min((area.width() - 2.0 * side) / 2.0, (area.height() - 2.0 * vside) / 2.0);
    r = std::max(r, std::min(area.width(), area.height()) * 0.18);
    r = std::max(r, 6.0);
    const QPointF c = area.center();
    m.pie_c = c;
    m.pie_r = r;
    L.plot = QRectF(c.x() - r, c.y() - r, 2.0 * r, 2.0 * r);

    struct Cand {
        int idx;
        double y;
        QPointF edge, elbow;
    };
    QVector<Cand> left_side, right_side;
    double cum = 0.0;
    for (int i = 0; i < k; ++i) {
        const double f = L.slice_fractions[i];
        const double mid = cum + f / 2.0;
        cum += f;
        if (texts[i].isEmpty())
            continue;
        const double a = (90.0 - mid * 360.0) * kRcrPi / 180.0;
        const QPointF dir(std::cos(a), -std::sin(a));
        const Cand cd{i, c.y() + dir.y() * (r + 8.0), c + dir * r, c + dir * (r + 8.0)};
        if (dir.x() >= 0.0)
            right_side.append(cd);
        else
            left_side.append(cd);
    }
    auto settle = [&](QVector<Cand>& v, bool right) {
        std::stable_sort(v.begin(), v.end(), [](const Cand& a, const Cand& b) { return a.y < b.y; });
        QVector<double> ys;
        for (const Cand& cd : v)
            ys.append(cd.y);
        rcr_spread(ys, lh + 1.0, area.top() + lh / 2.0, area.bottom() - lh / 2.0);
        const double tx0 = right ? c.x() + r + 14.0 : c.x() - r - 14.0;
        const double avail = right ? area.right() - tx0 : tx0 - area.left();
        for (int j = 0; j < v.size(); ++j) {
            const QString text = fn.elidedText(texts[v[j].idx], Qt::ElideRight, std::max(10.0, avail));
            const double w = fn.horizontalAdvance(text);
            RcrSliceLabel sl;
            sl.text = text;
            sl.right_side = right;
            sl.rect = right ? QRectF(tx0, ys[j] - lh / 2.0, w, lh) : QRectF(tx0 - w, ys[j] - lh / 2.0, w, lh);
            sl.edge = v[j].edge;
            sl.elbow = v[j].elbow;
            sl.anchor = QPointF(right ? tx0 - 3.0 : tx0 + 3.0, ys[j]);
            m.slice_labels.append(sl);
        }
    };
    settle(right_side, true);
    settle(left_side, false);
}

RcrModel rcr_build(const rcs::ChartSpec& spec, const QSizeF& size, const RcrText& tx) {
    RcrModel m;
    Layout& L = m.lay;
    const double W = std::max(1.0, size.width());
    const double H = std::max(1.0, size.height());
    L.outer = QRectF(0.0, 0.0, W, H);
    L.point_count = spec.point_count();
    L.legend_position = QStringLiteral("none");
    m.kind = spec.kind;

    const double pad = W < 360.0 ? 6.0 : 8.0;
    const QRectF inner = L.outer.adjusted(pad, pad, -pad, -pad);
    double top = inner.top();
    const QString title = spec.title.simplified();
    if (!title.isEmpty()) {
        L.title_rect = QRectF(inner.left(), top, inner.width(), std::ceil(tx.fm_title.height()));
        m.title_text = tx.fm_title.elidedText(title, Qt::ElideRight, inner.width());
        top = L.title_rect.bottom() + 6.0;
    }
    QRectF area(inner.left(), top, inner.width(), std::max(10.0, inner.bottom() - top));
    if (spec.empty()) {
        L.empty = true;
        L.plot = area;
        return m;
    }

    rcr_collect_series(m, spec);
    const bool is_round = m.kind == rcs::Kind::Donut || m.kind == rcs::Kind::Pie;
    if (is_round) {
        rcr_collect_slices(m, spec);
        if (L.slice_fractions.isEmpty()) {
            L.empty = true;
            L.plot = area;
            return m;
        }
    }

    const QVector<RcrLegendSrc> entries = rcr_legend_entries(m);
    L.legend_position = rcr_legend_position(spec, m, static_cast<int>(entries.size()), W);
    rcr_layout_legend(m, area, tx, entries);
    if (L.legend.isEmpty())
        L.legend_position = QStringLiteral("none");

    if (is_round)
        rcr_layout_pie(m, area, tx);
    else if (m.kind == rcs::Kind::HBar)
        rcr_layout_hbar(m, spec, area, tx);
    else
        rcr_layout_xy(m, spec, area, tx);
    return m;
}

// ── Painting ─────────────────────────────────────────────────────────────────

void rcr_paint_line(QPainter* p, const RcrModel& m, const RcrAxis& ax, const QVector<double>& v, const QColor& c,
                    double width) {
    QPainterPath path;
    QVector<QPointF> lone;
    const int n = std::min(static_cast<int>(v.size()), m.n);
    bool down = false;
    for (int i = 0; i < n; ++i) {
        if (!rcr_ok(v[i], ax.log)) {
            down = false;
            continue;
        }
        const QPointF pt(rcr_x_index(m, i), rcr_y(m, ax, v[i]));
        if (down)
            path.lineTo(pt);
        else
            path.moveTo(pt);
        down = true;
        const bool prev = i > 0 && rcr_ok(v[i - 1], ax.log);
        const bool next = i + 1 < n && rcr_ok(v[i + 1], ax.log);
        if (!prev && !next)
            lone.append(pt);
    }
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(c, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawPath(path);
    if (!lone.isEmpty()) { // a point between two gaps still shows
        p->setPen(Qt::NoPen);
        p->setBrush(c);
        for (const QPointF& pt : lone)
            p->drawEllipse(pt, width * 1.3, width * 1.3);
    }
}

void rcr_paint_area(QPainter* p, const RcrModel& m, const RcrAxis& ax, const QVector<double>& v, const QColor& c) {
    const double base = ax.log ? m.lay.plot.bottom() : rcr_y(m, ax, std::clamp(0.0, ax.lo, ax.hi));
    const int n = std::min(static_cast<int>(v.size()), m.n);
    p->setPen(Qt::NoPen);
    p->setBrush(rcr_alpha(c, 0.25));
    int i = 0;
    while (i < n) {
        while (i < n && !rcr_ok(v[i], ax.log))
            ++i;
        QPolygonF poly;
        int j = i;
        while (j < n && rcr_ok(v[j], ax.log)) {
            poly << QPointF(rcr_x_index(m, j), rcr_y(m, ax, v[j]));
            ++j;
        }
        if (poly.size() >= 2) {
            poly.prepend(QPointF(poly.first().x(), base));
            poly.append(QPointF(poly.last().x(), base));
            p->drawPolygon(poly);
        }
        i = j;
    }
    rcr_paint_line(p, m, ax, v, c, 1.4);
}

void rcr_paint_bars(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink) {
    QVector<int> bars;
    for (int k = 0; k < m.ser.size(); ++k)
        if (m.ser[k].mark == RcrMark::Bar)
            bars.append(k);
    const int nb = static_cast<int>(bars.size());
    if (nb == 0)
        return;
    const double slot = m.lay.plot.width() / m.n;
    const double group = slot * (nb == 1 ? 0.62 : 0.8);
    const double bw = group / nb;
    const double gap = nb > 1 ? std::min(2.0, bw * 0.15) : 0.0;
    p->setPen(Qt::NoPen);
    for (int g = 0; g < nb; ++g) {
        const RcrSer& s = m.ser[bars[g]];
        const RcrAxis& ax = s.right ? m.y2 : m.y1;
        const double base = rcr_y(m, ax, std::clamp(0.0, ax.lo, ax.hi));
        p->setBrush(ink.role(s.role));
        const QVector<double>& v = spec.series[s.src].values;
        for (int i = 0; i < v.size() && i < m.n; ++i) {
            if (!std::isfinite(v[i]))
                continue;
            const double x0 = rcr_x_index(m, i) - group / 2.0 + g * bw + gap / 2.0;
            double y = rcr_y(m, ax, v[i]);
            if (v[i] != 0.0 && std::abs(y - base) < 0.8)
                y = base + (v[i] > 0.0 ? -0.8 : 0.8);
            p->drawRect(QRectF(QPointF(x0, std::min(base, y)), QPointF(x0 + bw - gap, std::max(base, y))));
        }
    }
}

void rcr_paint_stacked(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink) {
    const RcrBands b = rcr_stack(m, spec);
    const bool bars = m.kind == rcs::Kind::StackedBar;
    const double bw = m.lay.plot.width() / m.n * 0.62;
    for (int k = 0; k < m.ser.size(); ++k) {
        const QColor c = ink.role(m.ser[k].role);
        const QVector<double>& v = spec.series[m.ser[k].src].values;
        if (bars) {
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            for (int i = 0; i < m.n; ++i) {
                if (i >= v.size() || !std::isfinite(v[i]) || v[i] == 0.0)
                    continue;
                const double x = rcr_x_index(m, i);
                const double ya = rcr_y(m, m.y1, b.lower[k][i]), yb = rcr_y(m, m.y1, b.upper[k][i]);
                p->drawRect(QRectF(QPointF(x - bw / 2.0, std::min(ya, yb)), QPointF(x + bw / 2.0, std::max(ya, yb))));
            }
            continue;
        }
        QPolygonF poly;
        QPolygonF edge;
        for (int i = 0; i < m.n; ++i) {
            const double x = i < v.size() && std::isfinite(v[i]) ? v[i] : 0.0;
            const QPointF pt(rcr_x_index(m, i), rcr_y(m, m.y1, b.upper[k][i]));
            poly << pt;
            edge << QPointF(pt.x(), rcr_y(m, m.y1, x >= 0.0 ? b.upper[k][i] : b.lower[k][i]));
        }
        for (int i = m.n - 1; i >= 0; --i)
            poly << QPointF(rcr_x_index(m, i), rcr_y(m, m.y1, b.lower[k][i]));
        p->setPen(Qt::NoPen);
        p->setBrush(rcr_alpha(c, 0.6));
        p->drawPolygon(poly);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(c, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPolyline(edge);
    }
}

void rcr_paint_candles(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink) {
    const QColor up = ink.role(QStringLiteral("up")), down = ink.role(QStringLiteral("down"));
    const double slot = m.lay.plot.width() / m.n;
    const double body = std::max(1.0, slot * 0.66);
    const double wick = std::clamp(slot * 0.12, 0.8, 1.4);
    for (int i = 0; i < spec.ohlc.size() && i < m.n; ++i) {
        const rcs::Candle& c = spec.ohlc[i];
        if (!rcr_ok(c.open, m.y1.log) || !rcr_ok(c.close, m.y1.log) || !rcr_ok(c.high, m.y1.log) ||
            !rcr_ok(c.low, m.y1.log))
            continue;
        const QColor col = c.close >= c.open ? up : down;
        const double x = rcr_x_index(m, i);
        p->setPen(QPen(col, wick, Qt::SolidLine, Qt::FlatCap));
        p->drawLine(QPointF(x, rcr_y(m, m.y1, c.high)), QPointF(x, rcr_y(m, m.y1, c.low)));
        if (slot >= 2.5) {
            const double y0 = rcr_y(m, m.y1, std::max(c.open, c.close));
            const double y1 = rcr_y(m, m.y1, std::min(c.open, c.close));
            p->setPen(Qt::NoPen);
            p->setBrush(col);
            p->drawRect(QRectF(x - body / 2.0, y0, body, std::max(1.0, y1 - y0)));
        }
    }
}

void rcr_paint_volume(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink) {
    const QRectF& vp = m.lay.volume_plot;
    const QColor up = rcr_alpha(ink.role(QStringLiteral("up")), 0.4);
    const QColor down = rcr_alpha(ink.role(QStringLiteral("down")), 0.4);
    const double slot = vp.width() / m.n;
    const double bw = std::max(0.8, slot * 0.66);
    p->setPen(Qt::NoPen);
    for (int i = 0; i < spec.volume.size() && i < m.n; ++i) {
        const double v = spec.volume[i];
        if (!std::isfinite(v) || v <= 0.0)
            continue;
        const bool rising = i < spec.ohlc.size() && spec.ohlc[i].close >= spec.ohlc[i].open;
        const double h = v / m.vol_hi * vp.height();
        const double x = vp.left() + (i + 0.5) * slot;
        p->setBrush(rising ? up : down);
        p->drawRect(QRectF(x - bw / 2.0, vp.bottom() - h, bw, h));
    }
}

void rcr_paint_scatter(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink) {
    int total = 0;
    for (const RcrSer& s : m.ser)
        total += static_cast<int>(spec.series[s.src].values.size());
    const double r = total > 300 ? 2.0 : 2.8;
    p->setPen(Qt::NoPen);
    for (const RcrSer& s : m.ser) {
        p->setBrush(rcr_alpha(ink.role(s.role), 0.85));
        const rcs::Series& ser = spec.series[s.src];
        for (int j = 0; j < ser.values.size(); ++j) {
            const double x = rcr_scatter_x(ser, j);
            const double y = ser.values[j];
            if (!std::isfinite(x) || !rcr_ok(y, m.y1.log))
                continue;
            p->drawEllipse(QPointF(rcr_x_num(m, x), rcr_y(m, m.y1, y)), r, r);
        }
    }
}

void rcr_paint_histogram(QPainter* p, const RcrModel& m, const RcrInk& ink) {
    const Layout& L = m.lay;
    if (m.ser.isEmpty())
        return;
    const double base = rcr_y(m, m.y1, 0.0);
    p->setPen(QPen(ink.paper, 0.75));
    p->setBrush(rcr_alpha(ink.role(m.ser.first().role), 0.85));
    for (int k = 0; k < L.bin_counts.size(); ++k) {
        if (L.bin_counts[k] <= 0)
            continue;
        const double x0 = rcr_x_num(m, L.bin_edges[k]);
        const double x1 = rcr_x_num(m, L.bin_edges[k + 1]);
        const double y = rcr_y(m, m.y1, L.bin_counts[k]);
        p->drawRect(QRectF(QPointF(x0, y), QPointF(x1, base)));
    }
}

double rcr_event_x(const RcrModel& m, const rcs::ChartSpec& spec, const rcs::Event& e) {
    const QString key = e.x.trimmed();
    if (m.numeric_x) {
        bool ok = false;
        const double x = key.toDouble(&ok);
        return ok ? rcr_x_num(m, x) : kRcrNaN;
    }
    const double fi = rcr_event_index(spec, key, m.n);
    return std::isfinite(fi) ? rcr_x_index(m, fi) : kRcrNaN;
}

void rcr_paint_events(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink,
                      const RcrText& tx) {
    if (spec.events.isEmpty())
        return;
    const QRectF& pl = m.lay.plot;
    const QRectF full = m.lay.volume_plot.isEmpty() ? pl : pl.united(m.lay.volume_plot);
    p->save();
    p->setClipRect(full, Qt::IntersectClip);
    QPen dash(ink.muted, 1.0, Qt::CustomDashLine, Qt::FlatCap);
    dash.setDashPattern({4.0, 3.0});
    const double fh = std::ceil(tx.fm_annot.height()) + 2.0;
    QVector<QRectF> placed;
    p->setFont(tx.annot);
    for (const rcs::Event& e : spec.events) {
        const double x = rcr_event_x(m, spec, e);
        if (!std::isfinite(x) || x < pl.left() - 0.5 || x > pl.right() + 0.5)
            continue;
        p->setPen(dash);
        p->drawLine(QPointF(x, full.top()), QPointF(x, full.bottom()));
        const QString text = tx.fm_annot.elidedText(e.text.simplified(), Qt::ElideRight, pl.width() * 0.45);
        if (text.isEmpty())
            continue;
        const double w = std::ceil(tx.fm_annot.horizontalAdvance(text)) + 6.0;
        double lx = x + 3.0;
        if (lx + w > pl.right())
            lx = x - 3.0 - w;
        lx = std::max(lx, pl.left());
        for (int row = 0; row < 3; ++row) {
            const QRectF flag(lx, pl.top() + 2.0 + row * (fh + 2.0), w, fh);
            bool clash = false;
            for (const QRectF& q : placed)
                if (q.intersects(flag))
                    clash = true;
            if (clash)
                continue;
            placed.append(flag);
            p->setPen(Qt::NoPen);
            p->setBrush(rcr_alpha(ink.paper, 0.9));
            p->drawRect(flag);
            p->setPen(ink.ink);
            p->drawText(flag, Qt::AlignCenter | Qt::TextDontClip, text);
            break;
        }
    }
    p->restore();
}

void rcr_paint_minmax(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink,
                      const RcrText& tx) {
    if (!spec.annot_minmax || m.kind == rcs::Kind::Histogram)
        return;
    const bool candle = m.kind == rcs::Kind::Candle;
    if (!candle && m.ser.isEmpty())
        return;
    const RcrSer* s = candle ? nullptr : &m.ser.first();
    const RcrAxis& ax = (s && s->right) ? m.y2 : m.y1;
    const rcs::Series* ser = s ? &spec.series[s->src] : nullptr;
    const int n = candle ? static_cast<int>(spec.ohlc.size()) : static_cast<int>(ser->values.size());
    int imax = -1, imin = -1;
    double vmax = -kRcrInf, vmin = kRcrInf;
    for (int i = 0; i < n; ++i) {
        const double hi = candle ? spec.ohlc[i].high : ser->values[i];
        const double lo = candle ? spec.ohlc[i].low : ser->values[i];
        if (rcr_ok(hi, ax.log) && hi > vmax) {
            vmax = hi;
            imax = i;
        }
        if (rcr_ok(lo, ax.log) && lo < vmin) {
            vmin = lo;
            imin = i;
        }
    }
    if (imax < 0)
        return;
    auto x_of = [&](int i) {
        if (m.kind == rcs::Kind::Scatter)
            return rcr_x_num(m, rcr_scatter_x(*ser, i));
        return rcr_x_index(m, i);
    };
    const QRectF& pl = m.lay.plot;
    const QFontMetricsF& fm = tx.fm_annot;
    const double th = fm.height();
    const QColor mark_hi = candle ? ink.role(QStringLiteral("up")) : ink.role(s->role);
    const QColor mark_lo = candle ? ink.role(QStringLiteral("down")) : ink.role(s->role);
    p->setFont(tx.annot);
    auto draw = [&](int i, double v, bool high) {
        const double x = x_of(i);
        const double y = rcr_y(m, ax, v);
        QPolygonF tri;
        if (high)
            tri << QPointF(x, y - 3.0) << QPointF(x - 4.0, y - 9.0) << QPointF(x + 4.0, y - 9.0);
        else
            tri << QPointF(x, y + 3.0) << QPointF(x - 4.0, y + 9.0) << QPointF(x + 4.0, y + 9.0);
        p->setPen(Qt::NoPen);
        p->setBrush(high ? mark_hi : mark_lo);
        p->drawPolygon(tri);
        const QString text = ax.values.text(v, ax.fmt);
        const double w = fm.horizontalAdvance(text);
        const double lx = std::clamp(x - w / 2.0, pl.left(), std::max(pl.left(), pl.right() - w));
        const QRectF r = high ? QRectF(lx, y - 10.0 - th, w, th) : QRectF(lx, y + 10.0, w, th);
        p->setPen(ink.ink);
        p->drawText(r, Qt::AlignCenter | Qt::TextDontClip, text);
    };
    draw(imax, vmax, true);
    if (imin >= 0 && imin != imax)
        draw(imin, vmin, false);
}

void rcr_paint_tags(QPainter* p, const RcrModel& m, const RcrInk& ink, const RcrText& tx) {
    if (m.tags.isEmpty())
        return;
    p->setFont(tx.annot);
    for (const RcrTag& t : m.tags) {
        const QColor bg = ink.role(t.role);
        p->setPen(QPen(bg, 1.0));
        p->drawLine(QPointF(m.lay.plot.right(), t.target_y), QPointF(t.rect.left(), t.rect.center().y()));
        p->setPen(Qt::NoPen);
        p->setBrush(bg);
        p->drawRoundedRect(t.rect, 2.0, 2.0);
        p->setPen(ink.text_on(bg));
        p->drawText(t.rect, Qt::AlignCenter | Qt::TextDontClip, t.text);
    }
}

void rcr_paint_x_labels(QPainter* p, const RcrModel& m, const RcrInk& ink, const RcrText& tx, double baseline) {
    const Layout& L = m.lay;
    const QFontMetricsF& fa = tx.fm_axis;
    p->setPen(QPen(ink.grid, 1.0));
    for (const Tick& t : L.x_ticks)
        p->drawLine(QPointF(t.pos, baseline), QPointF(t.pos, baseline + 3.0));
    p->setFont(tx.axis);
    p->setPen(ink.muted);
    for (const Tick& t : L.x_ticks) {
        const double w = fa.horizontalAdvance(t.label);
        const double l = rcr_label_left(t.pos, w, L.outer.width());
        p->drawText(QRectF(l, baseline + 5.0, w, fa.height()), Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip,
                    t.label);
    }
}

void rcr_paint_xy(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink, const RcrText& tx) {
    const Layout& L = m.lay;
    const QRectF& pl = L.plot;
    const double lh = tx.fm_axis.height();

    // Grid and zero line.
    p->setPen(QPen(rcr_alpha(ink.grid, 0.5), 1.0));
    for (const Tick& t : L.y_ticks)
        p->drawLine(QPointF(pl.left(), t.pos), QPointF(pl.right(), t.pos));
    if (!m.y1.log && m.y1.lo < 0.0 && m.y1.hi > 0.0) {
        const double y0 = rcr_y(m, m.y1, 0.0);
        p->setPen(QPen(ink.grid, 1.0));
        p->drawLine(QPointF(pl.left(), y0), QPointF(pl.right(), y0));
    }
    if (!L.volume_plot.isEmpty()) {
        p->setPen(QPen(rcr_alpha(ink.grid, 0.5), 1.0));
        for (const Tick& t : m.vol_ticks)
            p->drawLine(QPointF(pl.left(), t.pos), QPointF(pl.right(), t.pos));
        rcr_paint_volume(p, m, spec, ink);
    }

    // Data, clipped to the plot.
    p->save();
    p->setClipRect(pl.adjusted(-2.0, -2.0, 2.0, 2.0), Qt::IntersectClip);
    if (m.kind == rcs::Kind::Candle) {
        rcr_paint_candles(p, m, spec, ink);
    } else if (m.kind == rcs::Kind::Histogram) {
        rcr_paint_histogram(p, m, ink);
    } else if (m.kind == rcs::Kind::Scatter) {
        rcr_paint_scatter(p, m, spec, ink);
    } else if (m.stacked) {
        rcr_paint_stacked(p, m, spec, ink);
    } else {
        for (const RcrSer& s : m.ser)
            if (s.mark == RcrMark::Area)
                rcr_paint_area(p, m, s.right ? m.y2 : m.y1, spec.series[s.src].values, ink.role(s.role));
        rcr_paint_bars(p, m, spec, ink);
        const double lw = m.n > 400 ? 1.2 : 1.6;
        for (const RcrSer& s : m.ser)
            if (s.mark == RcrMark::Line)
                rcr_paint_line(p, m, s.right ? m.y2 : m.y1, spec.series[s.src].values, ink.role(s.role), lw);
    }
    p->restore();

    // Frame (bottom lines) and x axis.
    p->setPen(QPen(ink.grid, 1.0));
    p->drawLine(QPointF(pl.left(), pl.bottom()), QPointF(pl.right(), pl.bottom()));
    const QRectF& base = L.volume_plot.isEmpty() ? pl : L.volume_plot;
    if (!L.volume_plot.isEmpty())
        p->drawLine(QPointF(base.left(), base.bottom()), QPointF(base.right(), base.bottom()));
    rcr_paint_x_labels(p, m, ink, tx, base.bottom());

    // y labels; with two axes each side takes the colour of its only series.
    QColor c1 = ink.muted, c2 = ink.muted;
    if (L.has_y2) {
        int n1 = 0, n2 = 0;
        for (const RcrSer& s : m.ser) {
            if (s.right) {
                ++n2;
                c2 = ink.role(s.role);
            } else {
                ++n1;
                c1 = ink.role(s.role);
            }
        }
        if (n1 != 1)
            c1 = ink.muted;
        if (n2 != 1)
            c2 = ink.muted;
    }
    p->setFont(tx.axis);
    p->setPen(c1);
    for (const Tick& t : L.y_ticks)
        p->drawText(QRectF(pl.left() - 6.0 - m.y1_col_w, t.pos - lh / 2.0, m.y1_col_w, lh),
                    Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, t.label);
    p->setPen(ink.muted);
    for (const Tick& t : m.vol_ticks)
        p->drawText(QRectF(pl.left() - 6.0 - m.y1_col_w, t.pos - lh / 2.0, m.y1_col_w, lh),
                    Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, t.label);
    if (L.has_y2) {
        p->setPen(c2);
        for (const Tick& t : L.y2_ticks)
            p->drawText(QRectF(m.y2_x, t.pos - lh / 2.0, m.y2.label_w + 1.0, lh),
                        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, t.label);
    }

    rcr_paint_events(p, m, spec, ink, tx);
    rcr_paint_minmax(p, m, spec, ink, tx);
    rcr_paint_tags(p, m, ink, tx);
}

void rcr_paint_hbar(QPainter* p, const RcrModel& m, const rcs::ChartSpec& spec, const RcrInk& ink,
                    const RcrText& tx) {
    const Layout& L = m.lay;
    const QRectF& pl = L.plot;
    const double lh = tx.fm_axis.height();
    auto x_of = [&](double v) { return pl.left() + m.hx.frac(v) * pl.width(); };

    p->setPen(QPen(rcr_alpha(ink.grid, 0.5), 1.0));
    for (const Tick& t : L.x_ticks)
        p->drawLine(QPointF(t.pos, pl.top()), QPointF(t.pos, pl.bottom()));
    const double x0 = x_of(std::clamp(0.0, m.hx.lo, m.hx.hi));

    const int nb = static_cast<int>(m.ser.size());
    const double band = pl.height() / m.n;
    const double group = band * (nb == 1 ? 0.62 : 0.78);
    const double bh = nb > 0 ? group / nb : group;
    const double gap = nb > 1 ? std::min(2.0, bh * 0.15) : 0.0;
    const QFontMetricsF& fn = tx.fm_annot;
    // Value labels only where a line of text fits beside its own bar.
    const bool labels_fit = nb == 1 ? band >= fn.height() : bh >= fn.height();
    for (int g = 0; g < nb; ++g) {
        const RcrSer& s = m.ser[g];
        const QColor c = ink.role(s.role);
        const QVector<double>& v = spec.series[s.src].values;
        for (int i = 0; i < v.size() && i < m.n; ++i) {
            if (!std::isfinite(v[i]))
                continue;
            const double y = pl.top() + (i + 0.5) * band - group / 2.0 + g * bh + gap / 2.0;
            double x1 = x_of(v[i]);
            if (v[i] != 0.0 && std::abs(x1 - x0) < 0.8)
                x1 = x0 + (v[i] > 0.0 ? 0.8 : -0.8);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawRect(QRectF(QPointF(std::min(x0, x1), y), QPointF(std::max(x0, x1), y + bh - gap)));
            if (!labels_fit)
                continue;
            const QString text = m.bar_labels.text(v[i], m.hx.fmt);
            const double w = fn.horizontalAdvance(text);
            const double cy = y + (bh - gap) / 2.0;
            const QRectF r = v[i] >= 0.0 ? QRectF(x1 + 4.0, cy - fn.height() / 2.0, w, fn.height())
                                         : QRectF(x1 - 4.0 - w, cy - fn.height() / 2.0, w, fn.height());
            p->setFont(tx.annot);
            p->setPen(ink.ink);
            p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, text);
        }
    }

    p->setPen(QPen(ink.grid, 1.0));
    p->drawLine(QPointF(x0, pl.top()), QPointF(x0, pl.bottom()));
    p->drawLine(QPointF(pl.left(), pl.bottom()), QPointF(pl.right(), pl.bottom()));
    rcr_paint_x_labels(p, m, ink, tx, pl.bottom());

    p->setFont(tx.axis);
    p->setPen(ink.ink);
    for (const Tick& t : L.y_ticks)
        p->drawText(QRectF(m.cat_x, t.pos - lh / 2.0, m.cat_w, lh), Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip,
                    t.label);
}

void rcr_paint_pie(QPainter* p, const RcrModel& m, const RcrInk& ink, const RcrText& tx) {
    const Layout& L = m.lay;
    const QPointF c = m.pie_c;
    const double r = m.pie_r;
    const QRectF outer(c.x() - r, c.y() - r, 2.0 * r, 2.0 * r);
    const bool donut = m.kind == rcs::Kind::Donut;
    const double ri = donut ? r * 0.55 : 0.0;
    const QRectF inner(c.x() - ri, c.y() - ri, 2.0 * ri, 2.0 * ri);
    const int k = static_cast<int>(L.slice_fractions.size());
    double cum = 0.0;
    for (int i = 0; i < k; ++i) {
        const double f = L.slice_fractions[i];
        const double start = 90.0 - cum * 360.0; // clockwise from 12 o'clock
        const double sweep = -f * 360.0;
        cum += f;
        QPainterPath path;
        if (donut) {
            path.arcMoveTo(outer, start);
            path.arcTo(outer, start, sweep);
            path.arcTo(inner, start + sweep, -sweep);
            path.closeSubpath();
        } else {
            path.moveTo(c);
            path.arcTo(outer, start, sweep);
            path.closeSubpath();
        }
        p->setBrush(ink.role(QStringLiteral("series:%1").arg(i)));
        if (k > 1)
            p->setPen(QPen(ink.paper, 1.2, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        else
            p->setPen(Qt::NoPen);
        p->drawPath(path);
    }
    p->setFont(tx.annot);
    for (const RcrSliceLabel& sl : m.slice_labels) {
        p->setPen(QPen(ink.muted, 0.8));
        p->setBrush(Qt::NoBrush);
        const QPointF pts[3] = {sl.edge, sl.elbow, sl.anchor};
        p->drawPolyline(pts, 3);
        p->setPen(ink.ink);
        p->drawText(sl.rect, (sl.right_side ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter | Qt::TextDontClip,
                    sl.text);
    }
}

void rcr_paint_legend(QPainter* p, const RcrModel& m, const RcrInk& ink, const RcrText& tx) {
    p->setFont(tx.legend);
    for (const RcrLegendItem& it : m.legend_items) {
        const QColor c = ink.role(it.role);
        const double cy = it.rect.center().y();
        const double x = it.rect.left();
        if (it.shape == QLatin1String("line")) {
            p->setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap));
            p->drawLine(QPointF(x + 1.0, cy), QPointF(x + 12.0, cy));
        } else if (it.shape == QLatin1String("dot")) {
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawEllipse(QPointF(x + 6.0, cy), 3.5, 3.5);
        } else {
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawRoundedRect(QRectF(x + 1.5, cy - 4.5, 9.0, 9.0), 1.5, 1.5);
        }
        p->setPen(ink.ink);
        p->drawText(QRectF(x + 17.0, it.rect.top(), std::max(0.0, it.rect.width() - 17.0), it.rect.height()),
                    Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, it.text);
    }
}

} // namespace

// ── Public API ───────────────────────────────────────────────────────────────

QFont chart_font(const QString& font_family, int base_px, double chart_width, bool bold) {
    return rcr_font(font_family, rcr_px(base_px, chart_width), bold);
}

QString resolve_color(const rcs::ChartPalette& pal, const QString& role) {
    if (role.startsWith(QLatin1Char('#')))
        return role;
    const QString accent = !pal.accent.isEmpty() ? pal.accent : (!pal.series.isEmpty() ? pal.series.first() : pal.ink);
    if (role == QLatin1String("accent"))
        return accent;
    if (role == QLatin1String("up"))
        return pal.up.isEmpty() ? accent : pal.up;
    if (role == QLatin1String("down"))
        return pal.down.isEmpty() ? pal.ink : pal.down;
    if (role == QLatin1String("paper"))
        return pal.paper;
    if (role == QLatin1String("heading"))
        return pal.heading;
    if (role == QLatin1String("grid"))
        return pal.grid;
    if (role == QLatin1String("muted"))
        return pal.muted;
    if (role.startsWith(QLatin1String("series:"))) {
        if (pal.series.isEmpty())
            return accent;
        const int i = std::abs(role.mid(7).toInt());
        return pal.series[i % pal.series.size()];
    }
    return pal.ink;
}

QString format_value(double v, const rcs::AxisFormat& fmt, int decimals) {
    if (decimals < 0)
        return rcr_value_text(v, fmt, 0);
    return rcr_format(v, fmt, decimals, rcr_unit(std::abs(v), fmt.compact));
}

QSize chart_size(const rcs::ChartSpec& spec, int width) {
    const int w = std::max(1, width);
    return QSize(w, std::max(1, static_cast<int>(std::lround(w * spec.effective_aspect()))));
}

Layout compute_layout(const rcs::ChartSpec& spec, const QSizeF& size, const QString& font_family) {
    const RcrText tx(font_family, size.width());
    return rcr_build(spec, size, tx).lay;
}

Layout compute_layout(const rcs::ChartSpec& spec, const QSizeF& size, const QString& font_family,
                      const rcs::ChartPalette& pal) {
    Layout L = compute_layout(spec, size, font_family);
    for (LegendEntry& e : L.legend)
        e.color = resolve_color(pal, e.color);
    return L;
}

void paint(QPainter* p, const QRectF& rect, const rcs::ChartSpec& spec, const rcs::ChartPalette& pal,
           const QString& font_family) {
    if (!p || !p->isActive() || !(rect.width() > 0.0) || !(rect.height() > 0.0))
        return;
    const RcrText tx(font_family, rect.width());
    const RcrModel m = rcr_build(spec, rect.size(), tx);
    const RcrInk ink(pal);
    const Layout& L = m.lay;

    p->save();
    p->translate(rect.topLeft());
    const QRectF local(0.0, 0.0, rect.width(), rect.height());
    p->setClipRect(local, p->hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->setOpacity(1.0);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->fillRect(local, ink.paper);

    if (!m.title_text.isEmpty()) {
        p->setFont(tx.title);
        p->setPen(ink.heading);
        p->drawText(L.title_rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, m.title_text);
    }
    if (L.empty) {
        p->setFont(tx.axis);
        p->setPen(ink.muted);
        p->drawText(L.plot, Qt::AlignCenter | Qt::TextDontClip, QStringLiteral("No data"));
    } else if (m.kind == rcs::Kind::Donut || m.kind == rcs::Kind::Pie) {
        rcr_paint_pie(p, m, ink, tx);
    } else if (m.kind == rcs::Kind::HBar) {
        rcr_paint_hbar(p, m, spec, ink, tx);
    } else {
        rcr_paint_xy(p, m, spec, ink, tx);
    }
    if (!L.empty)
        rcr_paint_legend(p, m, ink, tx);
    p->restore();
}

QImage render_image(const rcs::ChartSpec& spec, const rcs::ChartPalette& pal, const QString& font_family,
                    const QSize& logical, qreal scale) {
    if (logical.isEmpty())
        return {};
    const qreal s = (std::isfinite(scale) && scale > 0.0) ? std::min<qreal>(scale, 16.0) : 1.0;
    const QSize px(std::max(1, static_cast<int>(std::lround(logical.width() * s))),
                   std::max(1, static_cast<int>(std::lround(logical.height() * s))));
    QImage img(px, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull())
        return {};
    img.fill(Qt::transparent);
    img.setDotsPerMeterX(static_cast<int>(std::lround(3780.0 * s)));
    img.setDotsPerMeterY(static_cast<int>(std::lround(3780.0 * s)));
    QPainter painter(&img);
    painter.scale(s, s);
    paint(&painter, QRectF(0.0, 0.0, logical.width(), logical.height()), spec, pal, font_family);
    painter.end();
    return img;
}

QByteArray render_png(const rcs::ChartSpec& spec, const rcs::ChartPalette& pal, const QString& font_family,
                      const QSize& logical, qreal scale) {
    const QImage img = render_image(spec, pal, font_family, logical, scale);
    if (img.isNull())
        return {};
    QByteArray out;
    QBuffer buf(&out);
    if (!buf.open(QIODevice::WriteOnly) || !img.save(&buf, "PNG"))
        return {};
    return out;
}

QString cache_key(const rcs::ChartSpec& spec, const rcs::ChartPalette& pal, const QString& font_family,
                  const QSize& logical, qreal scale) {
    QString text = rcs::content_hash(spec);
    text += QLatin1Char('|') + rcs::palette_key(pal);
    text += QLatin1Char('|') + font_family;
    text += QLatin1Char('|') + QString::number(logical.width()) + QLatin1Char('x') + QString::number(logical.height());
    text += QLatin1Char('|') + QString::number(scale, 'g', 10);
    text += QLatin1Char('|') + QString::number(kRendererVersion);
    text += QLatin1Char('|') + QLocale().name();
    if (!spec.dates_ms.isEmpty()) // calendar ticks follow the local zone
        text += QLatin1Char('|') + QString::fromUtf8(QTimeZone::systemTimeZoneId());
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex());
}

} // namespace fincept::services::report_chart
