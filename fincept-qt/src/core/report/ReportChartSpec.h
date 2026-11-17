#pragma once
// ReportChartSpec.h — what a Report Builder chart block draws, parsed from its config.
//
// A chart is document content: it is drawn by services/report_builder/ReportChartRenderer
// (QPainter only — no widgets, no QtCharts — so it runs on a worker thread into a QImage,
// and straight into a QPdfWriter for vector PDF), in the REPORT's paper palette.
//
// Config keys of a "chart" component (all optional except the data):
//   chart_type   line | area | stacked_area | bar (grouped when several series) | stacked_bar |
//                hbar (horizontal bars) | candle | donut | pie | scatter | histogram
//   title, caption, width (px at 96 dpi), sample ("1" = template placeholder numbers)
//   data         CSV numbers — one series (written by every earlier build; still read)
//   labels       category / x labels (ReportTableCodec list)
//   dates        CSV of ISO dates ("2026-11-17" or "2026-11-17T14:30") — a calendar x axis
//   series       JSON [{"name","values":[n|null...],"axis":"right"?,"kind":"line|bar|area"?,
//                       "x":[n...]? (scatter)}]
//   ohlc         JSON [[open,high,low,close],...]; volume: CSV numbers (candles: volume pane)
//   y_scale      linear | log
//   transform    none | rebased (first value = 100) | pct (% change from the first value)
//   legend       auto | top | bottom | right | none
//   y_format / y2_format  JSON {"dec":-1,"prefix":"","suffix":"","compact":true}
//   annot_last   "1": label each series' last value at the right edge
//   annot_minmax "1": mark the high and low of the first series
//   events       JSON [{"x":"<label | ISO date | index>","text":"Q3 results"}]
//   bins         histogram bins (0 = automatic)
//   aspect       height / width (0.3..1.2; 0 = the kind's default)
//
// Pure, Qt Core only, header-only. The hash is the chart's identity for the image cache:
// identical blocks (same data and options) in the same palette and size draw the same pixels.

#include "core/report/ReportDocument.h"
#include "core/report/ReportTableCodec.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::report::chart {

enum class Kind { Line, Area, StackedArea, Bar, StackedBar, HBar, Candle, Donut, Pie, Scatter, Histogram };

inline Kind kind_from_name(const QString& name) {
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("area"))
        return Kind::Area;
    if (n == QLatin1String("stacked_area"))
        return Kind::StackedArea;
    if (n == QLatin1String("bar"))
        return Kind::Bar;
    if (n == QLatin1String("stacked_bar"))
        return Kind::StackedBar;
    if (n == QLatin1String("hbar"))
        return Kind::HBar;
    if (n == QLatin1String("candle"))
        return Kind::Candle;
    if (n == QLatin1String("donut"))
        return Kind::Donut;
    if (n == QLatin1String("pie"))
        return Kind::Pie;
    if (n == QLatin1String("scatter"))
        return Kind::Scatter;
    if (n == QLatin1String("histogram"))
        return Kind::Histogram;
    return Kind::Line;
}

inline QString kind_name(Kind k) {
    switch (k) {
        case Kind::Line:
            return QStringLiteral("line");
        case Kind::Area:
            return QStringLiteral("area");
        case Kind::StackedArea:
            return QStringLiteral("stacked_area");
        case Kind::Bar:
            return QStringLiteral("bar");
        case Kind::StackedBar:
            return QStringLiteral("stacked_bar");
        case Kind::HBar:
            return QStringLiteral("hbar");
        case Kind::Candle:
            return QStringLiteral("candle");
        case Kind::Donut:
            return QStringLiteral("donut");
        case Kind::Pie:
            return QStringLiteral("pie");
        case Kind::Scatter:
            return QStringLiteral("scatter");
        case Kind::Histogram:
            return QStringLiteral("histogram");
    }
    return QStringLiteral("line");
}

/// Every kind, with the label the properties panel shows.
inline QVector<QPair<QString, QString>> kind_choices() {
    return {{QStringLiteral("line"), QStringLiteral("Line")},
            {QStringLiteral("area"), QStringLiteral("Area")},
            {QStringLiteral("stacked_area"), QStringLiteral("Stacked area")},
            {QStringLiteral("bar"), QStringLiteral("Bars (grouped)")},
            {QStringLiteral("stacked_bar"), QStringLiteral("Bars (stacked)")},
            {QStringLiteral("hbar"), QStringLiteral("Horizontal bars")},
            {QStringLiteral("candle"), QStringLiteral("Candles + volume")},
            {QStringLiteral("donut"), QStringLiteral("Donut")},
            {QStringLiteral("pie"), QStringLiteral("Pie")},
            {QStringLiteral("scatter"), QStringLiteral("Scatter")},
            {QStringLiteral("histogram"), QStringLiteral("Histogram")}};
}

struct Series {
    QString name;
    QVector<double> values; ///< NaN = gap
    QVector<double> x;      ///< scatter x values (empty = 0, 1, 2 ...)
    bool right_axis = false;
    QString kind;           ///< "" (the chart's kind) | line | bar | area — combo charts
};

struct Candle {
    double open = 0, high = 0, low = 0, close = 0;
};

struct Event {
    QString x;    ///< a label, an ISO date or an index
    QString text;
};

struct AxisFormat {
    int decimals = -1; ///< -1 = from the tick step
    QString prefix;
    QString suffix;
    bool compact = true; ///< 1.2K / 3.4M / 5.6B
    bool operator==(const AxisFormat& o) const {
        return decimals == o.decimals && prefix == o.prefix && suffix == o.suffix && compact == o.compact;
    }
};

/// The report's paper colours for a chart ("#rrggbb"). Document ink, never the terminal's.
struct ChartPalette {
    QString paper, ink, heading, grid, muted, accent, up, down;
    QStringList series; ///< categorical colours, in order
};

inline ChartPalette palette_for(const ReportTheme& t) {
    ChartPalette p;
    p.paper = t.page_bg;
    p.ink = t.text_color;
    p.heading = t.heading_color;
    p.grid = t.divider_color;
    p.muted = t.meta_color;
    p.accent = t.accent_color;
    p.up = QStringLiteral("#16a34a");
    p.down = QStringLiteral("#dc2626");
    // The accent first, then hues that read on light and dark paper alike.
    p.series = {t.accent_color,         QStringLiteral("#3b82f6"), QStringLiteral("#16a34a"), QStringLiteral("#dc2626"),
                QStringLiteral("#7c3aed"), QStringLiteral("#0891b2"), QStringLiteral("#ea580c"), QStringLiteral("#65a30d")};
    return p;
}

inline QString palette_key(const ChartPalette& p) {
    return QStringList{p.paper, p.ink, p.heading, p.grid, p.muted, p.accent, p.up, p.down, p.series.join(QLatin1Char(','))}
        .join(QLatin1Char('|'));
}

struct ChartSpec {
    Kind kind = Kind::Line;
    QString title;
    QStringList labels;
    QVector<qint64> dates_ms;  ///< epoch ms per point (empty = category axis)
    QVector<Series> series;
    QVector<Candle> ohlc;
    QVector<double> volume;
    bool log_y = false;
    QString transform = QStringLiteral("none");
    QString legend = QStringLiteral("auto");
    AxisFormat y_fmt, y2_fmt;
    bool annot_last = false;
    bool annot_minmax = false;
    QVector<Event> events;
    int bins = 0;
    double aspect = 0;

    /// Points on the x axis (the longest series, or the candles).
    int point_count() const {
        if (kind == Kind::Candle)
            return static_cast<int>(ohlc.size());
        int n = 0;
        for (const Series& s : series)
            n = std::max(n, static_cast<int>(s.values.size()));
        return n;
    }
    bool has_right_axis() const {
        for (const Series& s : series)
            if (s.right_axis)
                return true;
        return false;
    }
    /// Nothing to draw.
    bool empty() const {
        if (kind == Kind::Candle)
            return ohlc.isEmpty();
        for (const Series& s : series)
            for (double v : s.values)
                if (std::isfinite(v))
                    return false;
        return true;
    }
    /// Height / width the chart is drawn at.
    double effective_aspect() const {
        if (aspect >= 0.3 && aspect <= 1.2)
            return aspect;
        if (kind == Kind::HBar) {
            const int n = point_count();
            return std::clamp(0.12 + n * 0.055, 0.4, 1.1);
        }
        if (kind == Kind::Donut || kind == Kind::Pie)
            return 0.45;
        if (kind == Kind::Candle && !volume.isEmpty())
            return 0.56;
        return 0.5;
    }
};

// ── Parsing ─────────────────────────────────────────────────────────────────

/// "1,,3" keeps the gap (NaN in the middle); a trailing comma adds nothing.
inline QVector<double> parse_csv_numbers(const QString& csv) {
    QVector<double> out;
    if (csv.trimmed().isEmpty())
        return out;
    for (const QString& tok : csv.split(QLatin1Char(','))) {
        bool ok = false;
        const double v = tok.trimmed().toDouble(&ok);
        out << (ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN());
    }
    while (!out.isEmpty() && !std::isfinite(out.last()))
        out.removeLast();
    return out;
}

inline AxisFormat parse_axis_format(const QString& text) {
    AxisFormat f;
    if (text.trimmed().isEmpty())
        return f;
    const QJsonObject o = QJsonDocument::fromJson(text.toUtf8()).object();
    f.decimals = std::clamp(o.value(QStringLiteral("dec")).toInt(-1), -1, 8);
    f.prefix = o.value(QStringLiteral("prefix")).toString().left(8);
    f.suffix = o.value(QStringLiteral("suffix")).toString().left(8);
    f.compact = o.value(QStringLiteral("compact")).toBool(true);
    return f;
}

inline QString axis_format_text(const AxisFormat& f) {
    if (f == AxisFormat())
        return {};
    QJsonObject o;
    if (f.decimals >= 0)
        o.insert(QStringLiteral("dec"), f.decimals);
    if (!f.prefix.isEmpty())
        o.insert(QStringLiteral("prefix"), f.prefix);
    if (!f.suffix.isEmpty())
        o.insert(QStringLiteral("suffix"), f.suffix);
    if (!f.compact)
        o.insert(QStringLiteral("compact"), false);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/// Display transform of a series: rebased (first finite value = 100) or % change from it.
inline void apply_transform(QVector<double>& v, const QString& transform) {
    if (transform != QLatin1String("rebased") && transform != QLatin1String("pct"))
        return;
    double base = std::numeric_limits<double>::quiet_NaN();
    for (double x : v)
        if (std::isfinite(x) && x != 0) {
            base = x;
            break;
        }
    if (!std::isfinite(base))
        return;
    for (double& x : v) {
        if (!std::isfinite(x))
            continue;
        x = transform == QLatin1String("rebased") ? x / base * 100.0 : (x / base - 1.0) * 100.0;
    }
}

inline ChartSpec from_config(const QMap<QString, QString>& cfg) {
    ChartSpec s;
    s.kind = kind_from_name(cfg.value(QStringLiteral("chart_type"), QStringLiteral("line")));
    s.title = cfg.value(QStringLiteral("title"));
    s.labels = table::parse_list(cfg.value(QStringLiteral("labels")));
    s.log_y = cfg.value(QStringLiteral("y_scale")) == QLatin1String("log");
    const QString tr = cfg.value(QStringLiteral("transform"), QStringLiteral("none"));
    s.transform = (tr == QLatin1String("rebased") || tr == QLatin1String("pct")) ? tr : QStringLiteral("none");
    const QString lg = cfg.value(QStringLiteral("legend"), QStringLiteral("auto"));
    static const QStringList kLegends = {"auto", "top", "bottom", "right", "none"};
    s.legend = kLegends.contains(lg) ? lg : QStringLiteral("auto");
    s.y_fmt = parse_axis_format(cfg.value(QStringLiteral("y_format")));
    s.y2_fmt = parse_axis_format(cfg.value(QStringLiteral("y2_format")));
    s.annot_last = cfg.value(QStringLiteral("annot_last")) == QLatin1String("1");
    s.annot_minmax = cfg.value(QStringLiteral("annot_minmax")) == QLatin1String("1");
    s.bins = std::clamp(cfg.value(QStringLiteral("bins")).toInt(), 0, 100);
    s.aspect = cfg.value(QStringLiteral("aspect")).toDouble();

    for (const QString& d : cfg.value(QStringLiteral("dates")).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QDateTime dt = QDateTime::fromString(d.trimmed(), Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime(QDate::fromString(d.trimmed().left(10), Qt::ISODate), QTime(0, 0));
        if (!dt.isValid()) {
            s.dates_ms.clear();
            break;
        }
        s.dates_ms << dt.toMSecsSinceEpoch();
    }

    const QString series_raw = cfg.value(QStringLiteral("series"));
    if (!series_raw.trimmed().isEmpty()) {
        for (const QJsonValue& v : QJsonDocument::fromJson(series_raw.toUtf8()).array()) {
            const QJsonObject o = v.toObject();
            Series ser;
            ser.name = o.value(QStringLiteral("name")).toString();
            for (const QJsonValue& x : o.value(QStringLiteral("values")).toArray())
                ser.values << (x.isDouble() ? x.toDouble() : std::numeric_limits<double>::quiet_NaN());
            for (const QJsonValue& x : o.value(QStringLiteral("x")).toArray())
                ser.x << (x.isDouble() ? x.toDouble() : std::numeric_limits<double>::quiet_NaN());
            ser.right_axis = o.value(QStringLiteral("axis")).toString() == QLatin1String("right");
            const QString k = o.value(QStringLiteral("kind")).toString();
            ser.kind = (k == QLatin1String("line") || k == QLatin1String("bar") || k == QLatin1String("area")) ? k : QString();
            if (!ser.values.isEmpty())
                s.series.append(ser);
        }
    }
    if (s.series.isEmpty()) {
        const QVector<double> v = parse_csv_numbers(cfg.value(QStringLiteral("data")));
        if (!v.isEmpty())
            s.series.append(Series{QString(), v, {}, false, {}});
    }

    if (s.kind == Kind::Candle) {
        for (const QJsonValue& v : QJsonDocument::fromJson(cfg.value(QStringLiteral("ohlc")).toUtf8()).array()) {
            const QJsonArray a = v.toArray();
            if (a.size() < 4)
                continue;
            // A missing price (null / text) is a gap, not a price of 0.
            auto num = [&a](int i) {
                return a[i].isDouble() ? a[i].toDouble() : std::numeric_limits<double>::quiet_NaN();
            };
            s.ohlc.append(Candle{num(0), num(1), num(2), num(3)});
        }
        s.volume = parse_csv_numbers(cfg.value(QStringLiteral("volume")));
        if (s.volume.size() != s.ohlc.size())
            s.volume.clear();
        if (s.ohlc.isEmpty()) // no OHLC: draw the closes as a line
            s.kind = Kind::Line;
    }

    if (s.transform != QLatin1String("none") && s.kind != Kind::Candle && s.kind != Kind::Donut &&
        s.kind != Kind::Pie && s.kind != Kind::Histogram && s.kind != Kind::Scatter) {
        for (Series& ser : s.series)
            apply_transform(ser.values, s.transform);
        if (s.transform == QLatin1String("pct") && s.y_fmt.suffix.isEmpty())
            s.y_fmt.suffix = QStringLiteral("%");
    }
    if (s.dates_ms.size() != s.point_count())
        s.dates_ms.clear();

    const QString events_raw = cfg.value(QStringLiteral("events"));
    if (!events_raw.trimmed().isEmpty()) {
        for (const QJsonValue& v : QJsonDocument::fromJson(events_raw.toUtf8()).array()) {
            const QJsonObject o = v.toObject();
            Event e;
            const QJsonValue x = o.value(QStringLiteral("x"));
            e.x = x.isDouble() ? QString::number(qRound(x.toDouble())) : x.toString();
            e.text = o.value(QStringLiteral("text")).toString().left(60);
            if (!e.x.isEmpty())
                s.events.append(e);
        }
    }
    return s;
}

/// Canonical text of everything that changes the pixels (stable across runs and platforms).
inline QString canonical(const ChartSpec& s) {
    QJsonObject o;
    o.insert(QStringLiteral("k"), kind_name(s.kind));
    o.insert(QStringLiteral("t"), s.title);
    o.insert(QStringLiteral("l"), QJsonArray::fromStringList(s.labels));
    QJsonArray dates;
    for (qint64 d : s.dates_ms)
        dates.append(static_cast<double>(d));
    o.insert(QStringLiteral("d"), dates);
    QJsonArray ser;
    auto nums = [](const QVector<double>& v) {
        QJsonArray a;
        for (double x : v)
            a.append(std::isfinite(x) ? QJsonValue(x) : QJsonValue());
        return a;
    };
    for (const Series& x : s.series)
        ser.append(QJsonObject{{QStringLiteral("n"), x.name},
                               {QStringLiteral("v"), nums(x.values)},
                               {QStringLiteral("x"), nums(x.x)},
                               {QStringLiteral("r"), x.right_axis},
                               {QStringLiteral("k"), x.kind}});
    o.insert(QStringLiteral("s"), ser);
    QJsonArray ohlc;
    for (const Candle& c : s.ohlc)
        ohlc.append(QJsonArray{c.open, c.high, c.low, c.close});
    o.insert(QStringLiteral("o"), ohlc);
    o.insert(QStringLiteral("vol"), nums(s.volume));
    o.insert(QStringLiteral("log"), s.log_y);
    o.insert(QStringLiteral("tr"), s.transform);
    o.insert(QStringLiteral("lg"), s.legend);
    o.insert(QStringLiteral("f1"), axis_format_text(s.y_fmt));
    o.insert(QStringLiteral("f2"), axis_format_text(s.y2_fmt));
    o.insert(QStringLiteral("al"), s.annot_last);
    o.insert(QStringLiteral("am"), s.annot_minmax);
    QJsonArray ev;
    for (const Event& e : s.events)
        ev.append(QJsonArray{e.x, e.text});
    o.insert(QStringLiteral("e"), ev);
    o.insert(QStringLiteral("b"), s.bins);
    o.insert(QStringLiteral("a"), s.aspect);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/// Hex SHA-1 of the chart content (palette, size and renderer version are added by the
/// renderer's cache key).
inline QString content_hash(const ChartSpec& s) {
    return QString::fromLatin1(QCryptographicHash::hash(canonical(s).toUtf8(), QCryptographicHash::Sha1).toHex());
}

} // namespace fincept::report::chart
