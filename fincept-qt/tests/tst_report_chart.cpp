// tst_report_chart.cpp — Report Builder charts: the config contract (core/report/ReportChartSpec.h)
// and the QPainter renderer (services/report_builder/ReportChartRenderer).
//
// Golden PROPERTIES, not pixels: parsing, axis ranges (data inside, 0 for bars / areas), tick
// maths (inside the range, increasing, 1-2-5 steps, log decades), legends, histogram bins,
// stacked and dual-axis ranges, non-overlapping x labels, deterministic images and cache keys,
// a PNG for every kind, painting into an offset rect, worker threads and a vector PDF.
// Runs on the offscreen platform (QGuiApplication for fonts).
//
// Set FINCEPT_CHART_DUMP_DIR to keep every rendered PNG and the PDF for a visual check.

#include "core/report/ReportChartSpec.h"
#include "services/report_builder/ReportChartRenderer.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

using namespace fincept;
namespace rc = fincept::report::chart;
namespace rcr = fincept::services::report_chart;

class TestReportChart : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    // ReportChartSpec
    void parsesLegacySingleSeries();
    void parsesSeriesJsonWithRightAxis();
    void parsesCandlesAndVolume();
    void parsesTransforms();
    void parsesDates();
    void parsesOptions();

    // Layout
    void rangesContainData();
    void barsAndAreasIncludeZero();
    void tickMaths();
    void logScale();
    void legendEntries();
    void histogramBins();
    void stackedRangeCoversTotals();
    void dualAxisRangesIndependent();
    void datedTicksNeverOverlap();
    void categoryTicksNeverOverlap();
    void candleVolumePane();
    void donutAndPieSlices();
    void hbarLayout();
    void scatterNumericAxis();
    void annotationsReserveRoom();
    void emptySpec();
    void valueFormatting();
    void smallChartsScaleFonts();

    // Rendering
    void deterministicRender();
    void cacheKeyChanges();
    void pngForEveryKind();
    void paintRespectsRectAndState();
    void rendersOnWorkerThreads();
    void pdfOutput();
};

namespace {

using Cfg = QMap<QString, QString>;

const QString kFamily = QStringLiteral("Arial");
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

template <typename C>
int sz(const C& c) {
    return static_cast<int>(c.size());
}

rc::ChartPalette light() {
    return rc::palette_for(report::themes::light_professional());
}
rc::ChartPalette dark() {
    return rc::palette_for(report::themes::dark_corporate());
}

QString csv(const QVector<double>& v) {
    QStringList out;
    for (double x : v)
        out << (std::isfinite(x) ? QString::number(x, 'g', 12) : QStringLiteral("x"));
    return out.join(QLatin1Char(','));
}

QJsonObject ser(const QString& name, const QVector<double>& v, bool right = false, const QString& kind = {},
                const QVector<double>& x = {}) {
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    QJsonArray a;
    for (double d : v)
        a.append(std::isfinite(d) ? QJsonValue(d) : QJsonValue());
    o.insert(QStringLiteral("values"), a);
    if (right)
        o.insert(QStringLiteral("axis"), QStringLiteral("right"));
    if (!kind.isEmpty())
        o.insert(QStringLiteral("kind"), kind);
    if (!x.isEmpty()) {
        QJsonArray xa;
        for (double d : x)
            xa.append(d);
        o.insert(QStringLiteral("x"), xa);
    }
    return o;
}

QString json(const QJsonArray& a) {
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

QVector<double> wave(int n, double base = 100.0) {
    QVector<double> v;
    for (int i = 0; i < n; ++i)
        v << base + 10.0 * std::sin(i * 0.07) + 3.0 * std::sin(i * 0.31);
    return v;
}

/// Deterministic, roughly normal values (sum of uniforms from a fixed LCG).
QVector<double> normal_values(int n) {
    quint32 state = 12345u;
    auto uni = [&state]() {
        state = state * 1664525u + 1013904223u;
        return (state >> 8) / static_cast<double>(1u << 24);
    };
    QVector<double> v;
    for (int i = 0; i < n; ++i)
        v << (uni() + uni() + uni() + uni() - 2.0) * 3.0;
    return v;
}

QString business_dates(const QDate& start, int n) {
    QStringList out;
    QDate d = start;
    while (out.size() < n) {
        if (d.dayOfWeek() <= 5)
            out << d.toString(Qt::ISODate);
        d = d.addDays(1);
    }
    return out.join(QLatin1Char(','));
}

QString ohlc_json(int n, QVector<double>* volume) {
    QJsonArray a;
    double close = 50.0;
    for (int i = 0; i < n; ++i) {
        const double open = close;
        close = open + 2.0 * std::sin(i * 0.9) + 0.5 * std::cos(i * 2.3);
        const double high = std::max(open, close) + 0.8 + 0.3 * std::sin(i);
        const double low = std::min(open, close) - 0.7 - 0.2 * std::cos(i);
        a.append(QJsonArray{open, high, low, close});
        if (volume)
            *volume << 1.5e6 + 4e5 * std::sin(i * 0.5);
    }
    return json(a);
}

rcr::Layout layout(const rc::ChartSpec& s, double w = 640.0, double h = 320.0) {
    return rcr::compute_layout(s, QSizeF(w, h), kFamily);
}

double mantissa(double v) {
    return v / std::pow(10.0, std::floor(std::log10(std::abs(v)) + 1e-12));
}

bool near(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) <= tol;
}

/// "" when y ticks are well formed: >= 2, inside [lo, hi], values increasing while the
/// pixel goes up the axis, uniform 1-2-5 steps (linear) or 1/2/3/5 mantissas (log).
QString tick_problems(const QVector<rcr::Tick>& t, double lo, double hi, bool log, bool vertical = true) {
    if (t.size() < 2)
        return QStringLiteral("fewer than two ticks");
    const double eps = (hi - lo) * 1e-9 + 1e-12;
    for (int i = 0; i < t.size(); ++i) {
        if (t[i].value < lo - eps || t[i].value > hi + eps)
            return QStringLiteral("tick %1 outside [%2, %3]").arg(t[i].value).arg(lo).arg(hi);
        if (t[i].label.isEmpty())
            return QStringLiteral("empty label");
        if (i == 0)
            continue;
        if (!(t[i].value > t[i - 1].value))
            return QStringLiteral("values not increasing");
        if (vertical ? !(t[i].pos < t[i - 1].pos) : !(t[i].pos > t[i - 1].pos))
            return QStringLiteral("positions not monotonic");
        if (t[i].label == t[i - 1].label)
            return QStringLiteral("duplicate label %1").arg(t[i].label);
    }
    if (log) {
        for (const rcr::Tick& k : t) {
            const double m = mantissa(k.value);
            if (!(near(m, 1, 1e-6) || near(m, 2, 1e-6) || near(m, 3, 1e-6) || near(m, 5, 1e-6)))
                return QStringLiteral("log tick %1 is not 1/2/3/5 x 10^k").arg(k.value);
        }
        return {};
    }
    const double step = t[1].value - t[0].value;
    for (int i = 2; i < t.size(); ++i)
        if (!near(t[i].value - t[i - 1].value, step, std::abs(step) * 1e-6))
            return QStringLiteral("uneven steps");
    const double m = mantissa(step);
    if (!(near(m, 1, 1e-6) || near(m, 2, 1e-6) || near(m, 5, 1e-6) || near(m, 10, 1e-6)))
        return QStringLiteral("step %1 is not 1-2-5").arg(step);
    return {};
}

/// "" when no two x labels overlap (centred on their ticks, kept inside [0, width]).
QString overlap_problems(const QVector<rcr::Tick>& ticks, double width) {
    QImage dev(1, 1, QImage::Format_ARGB32_Premultiplied);
    dev.setDotsPerMeterX(3780);
    dev.setDotsPerMeterY(3780);
    const QFontMetricsF fm(rcr::chart_font(kFamily, 10, width), &dev);
    double last_right = -std::numeric_limits<double>::infinity();
    for (const rcr::Tick& t : ticks) {
        const double w = fm.horizontalAdvance(t.label);
        const double l = std::clamp(t.pos - w / 2.0, 0.0, std::max(0.0, width - w));
        if (l < last_right)
            return QStringLiteral("label '%1' overlaps its neighbour").arg(t.label);
        last_right = l + w;
    }
    return {};
}

QString dump_ticks(const QVector<rcr::Tick>& ticks) {
    QStringList out;
    for (const rcr::Tick& t : ticks)
        out << QStringLiteral("%1@%2").arg(t.label).arg(t.pos, 0, 'f', 1);
    return out.join(QStringLiteral(" | "));
}

rc::ChartSpec sample_spec(const QString& kind) {
    Cfg cfg{{"chart_type", kind}, {"title", QStringLiteral("Sample %1 chart").arg(kind)}};
    if (kind == QLatin1String("candle")) {
        QVector<double> vol;
        cfg.insert("ohlc", ohlc_json(60, &vol));
        cfg.insert("volume", csv(vol));
        cfg.insert("dates", business_dates(QDate(2026, 3, 2), 60));
        cfg.insert("annot_last", "1");
    } else if (kind == QLatin1String("histogram")) {
        cfg.insert("data", csv(normal_values(400)));
    } else if (kind == QLatin1String("scatter")) {
        QJsonArray a;
        QVector<double> x1, y1, x2, y2;
        for (int i = 0; i < 40; ++i) {
            x1 << i * 0.5;
            y1 << 2.0 + 0.8 * i * 0.5 + std::sin(i);
            x2 << i * 0.4 + 3.0;
            y2 << 10.0 - 0.3 * i + std::cos(i * 1.7);
        }
        a.append(ser("Fund A", y1, false, {}, x1));
        a.append(ser("Fund B", y2, false, {}, x2));
        cfg.insert("series", json(a));
    } else if (kind == QLatin1String("donut") || kind == QLatin1String("pie")) {
        cfg.insert("data", "40,25,15,10,6,2.5,1.5");
        cfg.insert("labels", "Equities,Bonds,Cash,Real estate,Commodities,Crypto,Other");
    } else if (kind == QLatin1String("hbar")) {
        QJsonArray a;
        a.append(ser("2025", {12.5, -3.2, 7.8, 4.1, 9.9, -1.5}));
        a.append(ser("2026", {10.1, 2.4, 8.8, -2.0, 11.2, 0.5}));
        cfg.insert("series", json(a));
        cfg.insert("labels", "Technology,Energy,Health care,Utilities,Financials,Materials");
    } else {
        QJsonArray a;
        a.append(ser("Portfolio", wave(24, 100.0)));
        a.append(ser("Benchmark", wave(24, 95.0)));
        a.append(ser("Cash", wave(24, 20.0)));
        cfg.insert("series", json(a));
        QStringList months;
        for (int i = 0; i < 24; ++i)
            months << QDate(2025, 1, 1).addMonths(i).toString(Qt::ISODate);
        cfg.insert("dates", months.join(QLatin1Char(',')));
        cfg.insert("annot_last", "1");
        cfg.insert("events", json(QJsonArray{QJsonObject{{"x", "2025-07-01"}, {"text", "Rebalance"}}}));
    }
    return rc::from_config(cfg);
}

rc::ChartSpec dated_line_spec(int n) {
    return rc::from_config({{"chart_type", "line"},
                            {"data", csv(wave(n))},
                            {"dates", business_dates(QDate(2025, 1, 2), n)},
                            {"title", "Price"}});
}

QString dump_dir() {
    return qEnvironmentVariable("FINCEPT_CHART_DUMP_DIR");
}

} // namespace

void TestReportChart::initTestCase() {
    // Number and month names are locale-dependent; the expectations below are en_US.
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
}

// ── ReportChartSpec ──────────────────────────────────────────────────────────

void TestReportChart::parsesLegacySingleSeries() {
    const rc::ChartSpec s = rc::from_config({{"chart_type", "bar"}, {"data", "1, 2.5,3"}, {"labels", "A,B,C"}});
    QVERIFY(s.kind == rc::Kind::Bar);
    QCOMPARE(sz(s.series), 1);
    QCOMPARE(s.series[0].values, QVector<double>({1.0, 2.5, 3.0}));
    QCOMPARE(s.labels, QStringList({"A", "B", "C"}));
    QCOMPARE(s.point_count(), 3);
    QVERIFY(!s.empty());
    QVERIFY(!s.has_right_axis());

    const rc::ChartSpec gap = rc::from_config({{"data", "1,abc,3"}});
    QVERIFY(gap.kind == rc::Kind::Line); // default kind
    QCOMPARE(sz(gap.series[0].values), 3);
    QVERIFY(std::isnan(gap.series[0].values[1]));

    const rc::ChartSpec quoted = rc::from_config({{"data", "1,2"}, {"labels", "\"A, Inc.\",B"}});
    QCOMPARE(quoted.labels, QStringList({"A, Inc.", "B"}));

    QVERIFY(rc::from_config({{"chart_type", "nonsense"}, {"data", "1"}}).kind == rc::Kind::Line);
    QVERIFY(rc::from_config({}).empty());
}

void TestReportChart::parsesSeriesJsonWithRightAxis() {
    QJsonArray a;
    a.append(ser("Price", {100, 101, kNaN, 103}));
    a.append(ser("Volume", {5, 6, 7, 8}, true, "bar"));
    a.append(ser("Empty", {}));
    QJsonObject odd = ser("Odd", {1, 2});
    odd.insert("kind", "spline"); // unknown -> the chart's kind
    a.append(odd);
    const rc::ChartSpec s = rc::from_config({{"chart_type", "line"}, {"series", json(a)}, {"data", "9,9,9"}});
    QCOMPARE(sz(s.series), 3); // the empty one is dropped, legacy data ignored
    QCOMPARE(s.series[0].name, QStringLiteral("Price"));
    QVERIFY(std::isnan(s.series[0].values[2]));
    QVERIFY(!s.series[0].right_axis);
    QVERIFY(s.series[0].kind.isEmpty());
    QVERIFY(s.series[1].right_axis);
    QCOMPARE(s.series[1].kind, QStringLiteral("bar"));
    QVERIFY(s.series[2].kind.isEmpty());
    QVERIFY(s.has_right_axis());
    QCOMPARE(s.point_count(), 4);

    const rcr::Layout L = layout(s);
    QVERIFY(L.has_y2);
    QCOMPARE(L.series_count, 3);
}

void TestReportChart::parsesCandlesAndVolume() {
    QJsonArray o;
    o.append(QJsonArray{10, 12, 9, 11});
    o.append(QJsonArray{11, 13, 10, 10.5});
    o.append(QJsonArray{1, 2}); // short rows are skipped
    const rc::ChartSpec s =
        rc::from_config({{"chart_type", "candle"}, {"ohlc", json(o)}, {"volume", "100,200"}});
    QVERIFY(s.kind == rc::Kind::Candle);
    QCOMPARE(sz(s.ohlc), 2);
    QCOMPARE(s.ohlc[1].close, 10.5);
    QCOMPARE(s.ohlc[0].high, 12.0);
    QCOMPARE(sz(s.volume), 2);
    QCOMPARE(s.point_count(), 2);
    QVERIFY(!s.empty());

    const rc::ChartSpec bad_vol = rc::from_config({{"chart_type", "candle"}, {"ohlc", json(o)}, {"volume", "100"}});
    QVERIFY(bad_vol.volume.isEmpty());

    // A missing price is a gap (NaN), not 0 — one null must not drag the axis to zero.
    QJsonArray holes = o;
    holes.append(QJsonArray{12, QJsonValue(), 11, 11.5});
    const rc::ChartSpec gap = rc::from_config({{"chart_type", "candle"}, {"ohlc", json(holes)}});
    QCOMPARE(sz(gap.ohlc), 3);
    QVERIFY(std::isnan(gap.ohlc[2].high));
    const rcr::Layout gl = layout(gap);
    QVERIFY(gl.y_min > 5.0 && gl.y_max >= 13.0);
    QVERIFY(!rcr::render_png(gap, light(), kFamily, QSize(320, 180), 1.0).isEmpty());

    const rc::ChartSpec closes = rc::from_config({{"chart_type", "candle"}, {"data", "1,2,3"}});
    QVERIFY(closes.kind == rc::Kind::Line); // no OHLC: the closes as a line
    QCOMPARE(closes.point_count(), 3);
}

void TestReportChart::parsesTransforms() {
    const rc::ChartSpec reb = rc::from_config({{"data", "50,100,75"}, {"transform", "rebased"}});
    QCOMPARE(reb.transform, QStringLiteral("rebased"));
    QCOMPARE(reb.series[0].values, QVector<double>({100.0, 200.0, 150.0}));

    const rc::ChartSpec pct = rc::from_config({{"data", "50,100,75"}, {"transform", "pct"}});
    QCOMPARE(pct.series[0].values, QVector<double>({0.0, 100.0, 50.0}));
    QCOMPARE(pct.y_fmt.suffix, QStringLiteral("%"));

    const rc::ChartSpec pct_fmt = rc::from_config(
        {{"data", "50,100"}, {"transform", "pct"}, {"y_format", "{\"suffix\":\" pp\"}"}});
    QCOMPARE(pct_fmt.y_fmt.suffix, QStringLiteral(" pp"));

    // The base is the first finite, non-zero value.
    const rc::ChartSpec zero = rc::from_config({{"data", "0,50,100"}, {"transform", "rebased"}});
    QCOMPARE(zero.series[0].values, QVector<double>({0.0, 100.0, 200.0}));

    QJsonArray a;
    a.append(ser("A", {kNaN, 20, 30}));
    a.append(ser("B", {4, 2, 8}));
    const rc::ChartSpec multi = rc::from_config({{"series", json(a)}, {"transform", "pct"}});
    QVERIFY(std::isnan(multi.series[0].values[0]));
    QCOMPARE(multi.series[0].values[2], 50.0);
    QCOMPARE(multi.series[1].values, QVector<double>({0.0, -50.0, 100.0}));

    const rc::ChartSpec hist = rc::from_config({{"chart_type", "histogram"}, {"data", "1,2,3"}, {"transform", "rebased"}});
    QCOMPARE(hist.series[0].values, QVector<double>({1.0, 2.0, 3.0}));

    QCOMPARE(rc::from_config({{"data", "1"}, {"transform", "log"}}).transform, QStringLiteral("none"));
}

void TestReportChart::parsesDates() {
    const rc::ChartSpec s =
        rc::from_config({{"data", "1,2,3"}, {"dates", "2026-01-02, 2026-01-05,2026-01-06"}});
    QCOMPARE(sz(s.dates_ms), 3);
    const QTimeZone local(QTimeZone::LocalTime);
    QCOMPARE(QDateTime::fromMSecsSinceEpoch(s.dates_ms[0], local).date(), QDate(2026, 1, 2));
    QCOMPARE(QDateTime::fromMSecsSinceEpoch(s.dates_ms[2], local).date(), QDate(2026, 1, 6));
    QVERIFY(s.dates_ms[0] < s.dates_ms[1] && s.dates_ms[1] < s.dates_ms[2]);

    const rc::ChartSpec intraday = rc::from_config({{"data", "1,2"}, {"dates", "2026-11-17T14:30,2026-11-17T15:00"}});
    QCOMPARE(sz(intraday.dates_ms), 2);
    QCOMPARE(intraday.dates_ms[1] - intraday.dates_ms[0], qint64(30 * 60 * 1000));

    QVERIFY(rc::from_config({{"data", "1,2,3"}, {"dates", "2026-01-02,2026-01-05"}}).dates_ms.isEmpty());
    QVERIFY(rc::from_config({{"data", "1,2"}, {"dates", "2026-01-02,someday"}}).dates_ms.isEmpty());

    // Candles count their OHLC rows.
    QJsonArray o;
    o.append(QJsonArray{1, 2, 0.5, 1.5});
    o.append(QJsonArray{1.5, 2, 1, 1.8});
    QCOMPARE(sz(rc::from_config({{"chart_type", "candle"}, {"ohlc", json(o)}, {"dates", "2026-01-02,2026-01-05"}})
                    .dates_ms),
             2);
}

void TestReportChart::parsesOptions() {
    QJsonArray ev;
    ev.append(QJsonObject{{"x", "2026-01-05"}, {"text", "Earnings"}});
    ev.append(QJsonObject{{"x", 3}, {"text", "Index"}});
    ev.append(QJsonObject{{"text", "no x"}});
    const rc::ChartSpec s = rc::from_config({{"data", "1,2,3,4"},
                                             {"legend", "right"},
                                             {"y_format", "{\"dec\":2,\"prefix\":\"$\",\"compact\":false}"},
                                             {"events", json(ev)},
                                             {"bins", "500"},
                                             {"annot_last", "1"},
                                             {"annot_minmax", "0"},
                                             {"y_scale", "log"},
                                             {"aspect", "0.8"}});
    QCOMPARE(s.legend, QStringLiteral("right"));
    QCOMPARE(s.y_fmt.decimals, 2);
    QCOMPARE(s.y_fmt.prefix, QStringLiteral("$"));
    QVERIFY(!s.y_fmt.compact);
    QCOMPARE(sz(s.events), 2);
    QCOMPARE(s.events[0].text, QStringLiteral("Earnings"));
    QCOMPARE(s.events[1].x, QStringLiteral("3"));
    QCOMPARE(s.bins, 100);
    QVERIFY(s.annot_last);
    QVERIFY(!s.annot_minmax);
    QVERIFY(s.log_y);
    QCOMPARE(s.effective_aspect(), 0.8);

    QCOMPARE(rc::from_config({{"data", "1"}, {"legend", "sideways"}}).legend, QStringLiteral("auto"));
    QCOMPARE(rc::from_config({{"data", "1"}, {"aspect", "5"}}).effective_aspect(), 0.5);

    // The content hash follows the content.
    const Cfg base{{"data", "1,2,3"}, {"title", "A"}};
    Cfg changed = base;
    changed["title"] = "B";
    QCOMPARE(rc::content_hash(rc::from_config(base)), rc::content_hash(rc::from_config(base)));
    QVERIFY(rc::content_hash(rc::from_config(base)) != rc::content_hash(rc::from_config(changed)));
}

// ── Layout ───────────────────────────────────────────────────────────────────

void TestReportChart::rangesContainData() {
    const rc::ChartSpec s = rc::from_config({{"data", "3,7,5,12,9"}, {"title", "Range"}});
    const rcr::Layout L = layout(s);
    QVERIFY(!L.empty);
    QVERIFY(L.y_min < 3.0 && L.y_max > 12.0); // lines get padding
    QVERIFY(L.y_min > 0.0);                   // ... which does not cross zero
    QVERIFY(L.outer.contains(L.plot));
    QVERIFY(!L.title_rect.isEmpty());
    QVERIFY(L.title_rect.bottom() <= L.plot.top());
    for (const rcr::Tick& t : L.y_ticks)
        QVERIFY(t.pos >= L.plot.top() - 0.5 && t.pos <= L.plot.bottom() + 0.5);
    for (const rcr::Tick& t : L.x_ticks)
        QVERIFY(t.pos >= L.plot.left() - 0.5 && t.pos <= L.plot.right() + 0.5);
    QCOMPARE(L.point_count, 5);
    QCOMPARE(L.series_count, 1);

    // Gaps are fine.
    const rcr::Layout g = layout(rc::from_config({{"data", "x,4,x,8,x"}}));
    QVERIFY(g.y_min < 4.0 && g.y_max > 8.0);
}

void TestReportChart::barsAndAreasIncludeZero() {
    for (const char* kind : {"bar", "area", "stacked_bar", "stacked_area"}) {
        const rcr::Layout L = layout(rc::from_config({{"chart_type", kind}, {"data", "5,8,6"}}));
        QVERIFY2(L.y_min <= 0.0 && L.y_max >= 8.0, kind);
    }
    QVERIFY(layout(rc::from_config({{"chart_type", "histogram"}, {"data", "5,8,6"}})).y_min <= 0.0); // counts
    const rcr::Layout neg = layout(rc::from_config({{"chart_type", "bar"}, {"data", "-5,-2"}}));
    QVERIFY(neg.y_max >= 0.0 && neg.y_min <= -5.0);
    const rcr::Layout mixed = layout(rc::from_config({{"chart_type", "bar"}, {"data", "-5,10"}}));
    QVERIFY(mixed.y_min <= -5.0 && mixed.y_max >= 10.0);
    const rcr::Layout hb = layout(rc::from_config({{"chart_type", "hbar"}, {"data", "3,-2,7"}}));
    QVERIFY(hb.y_min <= -2.0 && hb.y_max >= 7.0 && hb.y_min < 0.0 && hb.y_max > 0.0);
    const rcr::Layout line = layout(rc::from_config({{"chart_type", "line"}, {"data", "5,8,6"}}));
    QVERIFY(line.y_min > 0.0); // a line is not forced to zero
}

void TestReportChart::tickMaths() {
    const QVector<QVector<double>> sets = {{0.001, 0.004, 0.009},     {1, 37, 100},     {12345, 54321, 98765},
                                           {-50, 0, 50},              {5, 5, 5},        {1e9, 3e9, 2.5e9},
                                           {-0.25, -0.75, -0.5},      {99.1, 99.3, 99.2}};
    for (const QVector<double>& v : sets) {
        for (const char* kind : {"line", "bar"}) {
            const rcr::Layout L = layout(rc::from_config({{"chart_type", kind}, {"data", csv(v)}}));
            QVERIFY(!L.log_y);
            const QString err = tick_problems(L.y_ticks, L.y_min, L.y_max, false);
            QVERIFY2(err.isEmpty(), qPrintable(QStringLiteral("%1 %2: %3").arg(kind, csv(v), err)));
            QVERIFY2(L.y_ticks.size() <= 7, qPrintable(csv(v)));
        }
    }
    // A tall chart gets more ticks than a flat one, both bounded.
    const rc::ChartSpec s = rc::from_config({{"data", "0,100"}});
    const int flat = sz(layout(s, 640, 140).y_ticks);
    const int tall = sz(layout(s, 640, 600).y_ticks);
    QVERIFY(flat >= 2 && tall >= flat && tall <= 7);

    // Compact labels for large values.
    const rcr::Layout big = layout(rc::from_config({{"chart_type", "bar"}, {"data", "10000,50000"}}));
    for (const rcr::Tick& t : big.y_ticks)
        QVERIFY2(t.value == 0.0 || t.label.endsWith(QLatin1Char('K')), qPrintable(t.label));
    // Prefix / suffix / fixed decimals.
    const rcr::Layout fmt = layout(rc::from_config(
        {{"data", "1,3"}, {"y_format", "{\"dec\":2,\"prefix\":\"$\",\"suffix\":\" bn\"}"}}));
    for (const rcr::Tick& t : fmt.y_ticks) {
        QVERIFY2(t.label.startsWith(QLatin1Char('$')) && t.label.endsWith(QStringLiteral(" bn")), qPrintable(t.label));
        QVERIFY2(t.label.contains(QLatin1Char('.')) && t.label.section(QLatin1Char('.'), 1).size() == 5,
                 qPrintable(t.label)); // "00 bn"
    }
}

void TestReportChart::logScale() {
    const rc::ChartSpec s = rc::from_config({{"data", "1,10,100,1000,10000"}, {"y_scale", "log"}});
    const rcr::Layout L = layout(s, 640, 400);
    QVERIFY(L.log_y);
    QVERIFY(L.y_min <= 1.0 && L.y_max >= 10000.0);
    const QString err = tick_problems(L.y_ticks, L.y_min, L.y_max, true);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    int decades = 0;
    for (const rcr::Tick& t : L.y_ticks)
        if (near(mantissa(t.value), 1.0))
            ++decades;
    QVERIFY(decades >= 3);
    // Equal ratios get equal distances.
    double p10 = 0, p100 = 0, p1000 = 0;
    for (const rcr::Tick& t : L.y_ticks) {
        if (near(t.value, 10))
            p10 = t.pos;
        if (near(t.value, 100))
            p100 = t.pos;
        if (near(t.value, 1000))
            p1000 = t.pos;
    }
    QVERIFY(p10 > 0 && p100 > 0 && p1000 > 0);
    QVERIFY(near(p10 - p100, p100 - p1000, 0.01));

    QVERIFY(!layout(rc::from_config({{"chart_type", "bar"}, {"data", "1,10,100"}, {"y_scale", "log"}})).log_y);
    QVERIFY(!layout(rc::from_config({{"data", "-1,-2,0"}, {"y_scale", "log"}})).log_y);
    const rcr::Layout mixed = layout(rc::from_config({{"data", "-5,10,100"}, {"y_scale", "log"}}));
    QVERIFY(mixed.log_y && mixed.y_min > 0.0);
    QVERIFY(layout(rc::from_config({{"chart_type", "area"}, {"data", "1,10,100"}, {"y_scale", "log"}})).log_y);
}

void TestReportChart::legendEntries() {
    const rcr::Layout one = layout(rc::from_config({{"data", "1,2,3"}}));
    QCOMPARE(one.legend_position, QStringLiteral("none"));
    QVERIFY(one.legend.isEmpty());
    QCOMPARE(one.series_count, 1);

    QJsonArray a;
    a.append(ser("Alpha", {1, 2, 3}));
    a.append(ser("", {2, 3, 4}));
    const rc::ChartSpec two = rc::from_config({{"series", json(a)}});
    const rc::ChartPalette pal = light();
    const rcr::Layout L = rcr::compute_layout(two, QSizeF(640, 320), kFamily, pal);
    QCOMPARE(L.series_count, 2);
    QCOMPARE(L.legend_position, QStringLiteral("bottom"));
    QCOMPARE(sz(L.legend), 2);
    QCOMPARE(L.legend[0].name, QStringLiteral("Alpha"));
    QCOMPARE(L.legend[1].name, QStringLiteral("Series 2"));
    QCOMPARE(L.legend[0].shape, QStringLiteral("line"));
    QCOMPARE(L.legend[0].color, pal.series[0]);
    QCOMPARE(L.legend[1].color, pal.series[1]);
    QVERIFY(L.legend_rect.top() >= L.plot.bottom());
    // Without a palette the colours are roles.
    QCOMPARE(layout(two).legend[1].color, QStringLiteral("series:1"));
    QCOMPARE(rcr::resolve_color(pal, QStringLiteral("series:9")), pal.series[1]);
    QCOMPARE(rcr::resolve_color(pal, QStringLiteral("accent")), pal.accent);

    QJsonArray b = a;
    const rc::ChartSpec bars = rc::from_config({{"chart_type", "bar"}, {"series", json(b)}});
    QCOMPARE(layout(bars).legend[0].shape, QStringLiteral("box"));
    const rc::ChartSpec dots = rc::from_config({{"chart_type", "scatter"}, {"series", json(b)}});
    QCOMPARE(layout(dots).legend[0].shape, QStringLiteral("dot"));

    const rcr::Layout donut =
        layout(rc::from_config({{"chart_type", "donut"}, {"data", "50,30,20"}, {"labels", "Stocks,Bonds,Cash"}}));
    QCOMPARE(donut.legend_position, QStringLiteral("right"));
    QCOMPARE(sz(donut.legend), 3);
    QCOMPARE(donut.legend[1].name, QStringLiteral("Bonds"));
    QCOMPARE(donut.legend[2].shape, QStringLiteral("box"));
    QVERIFY(donut.legend_rect.left() >= donut.plot.right());

    const rcr::Layout hidden = layout(rc::from_config({{"series", json(a)}, {"legend", "none"}}));
    QCOMPARE(hidden.legend_position, QStringLiteral("none"));
    QVERIFY(hidden.legend.isEmpty());

    const rcr::Layout top = layout(rc::from_config({{"data", "1,2"}, {"legend", "top"}, {"title", "T"}}));
    QCOMPARE(top.legend_position, QStringLiteral("top"));
    QCOMPARE(sz(top.legend), 1);
    QVERIFY(top.legend_rect.bottom() <= top.plot.top());
    QVERIFY(top.legend_rect.top() >= top.title_rect.bottom());

    const rcr::Layout right = layout(rc::from_config({{"series", json(a)}, {"legend", "right"}}));
    QCOMPARE(right.legend_position, QStringLiteral("right"));
    QVERIFY(right.legend_rect.left() >= right.plot.right());
}

void TestReportChart::histogramBins() {
    QVector<double> v = normal_values(500);
    v << kNaN << kNaN;
    const rc::ChartSpec s = rc::from_config({{"chart_type", "histogram"}, {"data", csv(v)}});
    const rcr::Layout L = layout(s);
    QVERIFY(!L.empty);
    QCOMPARE(sz(L.bin_edges), sz(L.bin_counts) + 1);
    QVERIFY(L.bin_counts.size() >= 5 && L.bin_counts.size() <= 40);
    int sum = 0, maxc = 0;
    for (int c : L.bin_counts) {
        sum += c;
        maxc = std::max(maxc, c);
    }
    QCOMPARE(sum, 500);
    for (int i = 1; i < L.bin_edges.size(); ++i)
        QVERIFY(L.bin_edges[i] > L.bin_edges[i - 1]);
    double lo = 1e300, hi = -1e300;
    for (double x : s.series[0].values) // as parsed from the CSV
        if (std::isfinite(x)) {
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
    QCOMPARE(L.bin_edges.first(), lo);
    QCOMPARE(L.bin_edges.last(), hi);
    QVERIFY(L.y_min <= 0.0 && L.y_max >= maxc);
    const QString terr = tick_problems(L.y_ticks, L.y_min, L.y_max, false);
    QVERIFY2(terr.isEmpty(), qPrintable(terr));
    for (const rcr::Tick& t : L.y_ticks)
        QVERIFY(near(t.value, std::round(t.value))); // counts are whole
    QVERIFY(L.x_ticks.size() >= 2);
    for (const rcr::Tick& t : L.x_ticks)
        QVERIFY(t.value >= lo - 1e-9 && t.value <= hi + 1e-9);
    QVERIFY(overlap_problems(L.x_ticks, L.outer.width()).isEmpty());

    const rcr::Layout fixed = layout(rc::from_config({{"chart_type", "histogram"}, {"data", csv(v)}, {"bins", "12"}}));
    QCOMPARE(sz(fixed.bin_counts), 12);

    const rcr::Layout same = layout(rc::from_config({{"chart_type", "histogram"}, {"data", "3,3,3,3"}}));
    int same_sum = 0;
    for (int c : same.bin_counts)
        same_sum += c;
    QCOMPARE(same_sum, 4);
}

void TestReportChart::stackedRangeCoversTotals() {
    QJsonArray a;
    a.append(ser("A", {1, 2, 3}));
    a.append(ser("B", {4, 5, 6}));
    const rcr::Layout sb = layout(rc::from_config({{"chart_type", "stacked_bar"}, {"series", json(a)}}));
    QVERIFY(sb.y_max >= 9.0 && sb.y_min <= 0.0);
    const rcr::Layout sa = layout(rc::from_config({{"chart_type", "stacked_area"}, {"series", json(a)}}));
    QVERIFY(sa.y_max >= 9.0 && sa.y_min <= 0.0);
    const rcr::Layout grouped = layout(rc::from_config({{"chart_type", "bar"}, {"series", json(a)}}));
    QVERIFY(grouped.y_max >= 6.0 && grouped.y_max < 9.0);

    QJsonArray m;
    m.append(ser("A", {1, -2, 3}));
    m.append(ser("B", {-4, 5, -6}));
    m.append(ser("C", {2, kNaN, -1}));
    const rcr::Layout mixed = layout(rc::from_config({{"chart_type", "stacked_bar"}, {"series", json(m)}}));
    // positives per point: 3, 5, 3; negatives: -4, -2, -7
    QVERIFY(mixed.y_max >= 5.0);
    QVERIFY(mixed.y_min <= -7.0);
    QVERIFY(tick_problems(mixed.y_ticks, mixed.y_min, mixed.y_max, false).isEmpty());
}

void TestReportChart::dualAxisRangesIndependent() {
    QJsonArray a;
    a.append(ser("Index", {100, 110, 105}));
    a.append(ser("Yield", {0.01, 0.03, 0.02}, true));
    const rc::ChartSpec s = rc::from_config({{"series", json(a)}, {"y2_format", "{\"suffix\":\"%\"}"}});
    const rcr::Layout L = layout(s);
    QVERIFY(L.has_y2);
    QVERIFY(L.y_min <= 100.0 && L.y_max >= 110.0 && L.y_min > 1.0);
    QVERIFY(L.y2_min <= 0.01 && L.y2_max >= 0.03 && L.y2_max < 1.0);
    const QString e1 = tick_problems(L.y_ticks, L.y_min, L.y_max, false);
    const QString e2 = tick_problems(L.y2_ticks, L.y2_min, L.y2_max, false);
    QVERIFY2(e1.isEmpty(), qPrintable(e1));
    QVERIFY2(e2.isEmpty(), qPrintable(e2));
    for (const rcr::Tick& t : L.y2_ticks) {
        QVERIFY2(t.label.endsWith(QLatin1Char('%')), qPrintable(t.label));
        QVERIFY(t.pos >= L.plot.top() - 0.5 && t.pos <= L.plot.bottom() + 0.5);
    }

    // Combo: bars on the right include zero, the line on the left does not.
    QJsonArray c;
    c.append(ser("Price", {50, 52, 51}));
    c.append(ser("Volume", {1000, 3000, 2000}, true, "bar"));
    const rcr::Layout combo = layout(rc::from_config({{"series", json(c)}}));
    QVERIFY(combo.has_y2);
    QVERIFY(combo.y_min > 0.0);
    QVERIFY(combo.y2_min <= 0.0 && combo.y2_max >= 3000.0);
    QJsonArray big;
    big.append(ser("Price", {50, 52, 51}));
    big.append(ser("Volume", {1e6, 3e6, 2e6}, true, "bar"));
    const rcr::Layout compact = layout(rc::from_config({{"series", json(big)}}));
    QCOMPARE(compact.y2_ticks.first().value, 0.0);
    QCOMPARE(compact.y2_ticks.first().label, QStringLiteral("0")); // zero carries no unit
    QVERIFY(compact.y2_ticks.last().label.endsWith(QLatin1Char('M')));

    // Everything on the right: one (left) axis.
    QJsonArray r;
    r.append(ser("A", {1, 2}, true));
    QVERIFY(!layout(rc::from_config({{"series", json(r)}})).has_y2);
}

void TestReportChart::datedTicksNeverOverlap() {
    const rc::ChartSpec s = dated_line_spec(250);
    QCOMPARE(sz(s.dates_ms), 250);
    for (double w : {280.0, 360.0, 480.0, 640.0, 960.0, 1400.0}) {
        const rcr::Layout L = layout(s, w, w * 0.5);
        QVERIFY2(L.x_ticks.size() >= 2,
                 qPrintable(QStringLiteral("width %1, plot %2: %3").arg(w).arg(L.plot.width()).arg(dump_ticks(L.x_ticks))));
        for (int i = 0; i < L.x_ticks.size(); ++i) {
            QVERIFY(!L.x_ticks[i].label.isEmpty());
            QVERIFY(L.x_ticks[i].pos >= L.plot.left() - 0.5 && L.x_ticks[i].pos <= L.plot.right() + 0.5);
            if (i > 0) {
                QVERIFY(L.x_ticks[i].pos > L.x_ticks[i - 1].pos);
                QVERIFY(L.x_ticks[i].value > L.x_ticks[i - 1].value);
            }
        }
        const QString err = overlap_problems(L.x_ticks, w);
        QVERIFY2(err.isEmpty(), qPrintable(QStringLiteral("width %1: %2").arg(w).arg(err)));
    }
    // Some label always names the year: January as "2026", or the first tick as "Feb 2025".
    const rcr::Layout L = layout(s, 640, 320);
    QVERIFY(L.x_ticks.size() >= 4);
    bool year_label = false;
    for (const rcr::Tick& t : L.x_ticks)
        year_label = year_label || t.label.contains(QStringLiteral("2025"));
    QVERIFY2(year_label, qPrintable(L.x_ticks.first().label));
    const rcr::Layout across = layout(dated_line_spec(400), 640, 320); // runs into 2026
    bool jan = false;
    for (const rcr::Tick& t : across.x_ticks)
        jan = jan || t.label == QStringLiteral("2026");
    QVERIFY(jan);

    // Intraday minutes and multi-day hourly data.
    QStringList minutes;
    QVector<double> v;
    for (int i = 0; i < 390; ++i) {
        minutes << QDateTime(QDate(2026, 11, 16), QTime(9, 30)).addSecs(60 * i).toString(Qt::ISODate);
        v << 50 + std::sin(i * 0.05);
    }
    const rcr::Layout intra =
        layout(rc::from_config({{"data", csv(v)}, {"dates", minutes.join(QLatin1Char(','))}}), 520, 260);
    QVERIFY(intra.x_ticks.size() >= 2);
    QVERIFY(overlap_problems(intra.x_ticks, 520).isEmpty());

    QStringList hours;
    QVector<double> hv;
    for (int d = 0; d < 5; ++d)
        for (int h = 9; h <= 16; ++h) {
            hours << QDateTime(QDate(2026, 11, 16).addDays(d), QTime(h, 0)).toString(Qt::ISODate);
            hv << 10 + d + h * 0.1;
        }
    const rcr::Layout hourly =
        layout(rc::from_config({{"data", csv(hv)}, {"dates", hours.join(QLatin1Char(','))}}), 420, 220);
    QVERIFY(hourly.x_ticks.size() >= 2);
    QVERIFY(overlap_problems(hourly.x_ticks, 420).isEmpty());
}

void TestReportChart::categoryTicksNeverOverlap() {
    QStringList labels;
    QVector<double> v;
    for (int i = 0; i < 60; ++i) {
        labels << QStringLiteral("Category number %1").arg(i + 1);
        v << i % 7 + 1;
    }
    for (const char* kind : {"bar", "line"}) {
        for (double w : {320.0, 640.0, 1200.0}) {
            const rcr::Layout L =
                layout(rc::from_config({{"chart_type", kind}, {"data", csv(v)}, {"labels", labels.join(',')}}), w, 300);
            QVERIFY(!L.x_ticks.isEmpty());
            QVERIFY2(overlap_problems(L.x_ticks, w).isEmpty(), kind);
        }
    }
    // No labels: 1..n, thinned on round numbers.
    const rcr::Layout numbered = layout(rc::from_config({{"data", csv(wave(500))}}));
    QVERIFY(numbered.x_ticks.size() >= 2);
    QCOMPARE(numbered.x_ticks.first().label, QStringLiteral("1"));
    QVERIFY(overlap_problems(numbered.x_ticks, 640).isEmpty());

    // A few bars keep every category (names shortened rather than skipped).
    const rcr::Layout few = layout(
        rc::from_config({{"chart_type", "bar"},
                         {"data", "1,2,3,4,5,6"},
                         {"labels", "Information technology,Communication services,Consumer discretionary,"
                                    "Consumer staples,Health care,Industrials"}}),
        420, 240);
    QCOMPARE(sz(few.x_ticks), 6);
    QVERIFY(overlap_problems(few.x_ticks, 420).isEmpty());
}

void TestReportChart::candleVolumePane() {
    QVector<double> vol;
    const QString o = ohlc_json(30, &vol);
    const rc::ChartSpec s = rc::from_config({{"chart_type", "candle"}, {"ohlc", o}, {"volume", csv(vol)}});
    const rcr::Layout L = layout(s, 640, s.effective_aspect() * 640);
    QVERIFY(!L.volume_plot.isEmpty());
    QVERIFY(L.volume_plot.top() > L.plot.bottom());
    QCOMPARE(L.volume_plot.left(), L.plot.left());
    QCOMPARE(L.volume_plot.right(), L.plot.right());
    const double share = L.volume_plot.height() / (L.volume_plot.bottom() - L.plot.top());
    QVERIFY2(share > 0.15 && share < 0.3, qPrintable(QString::number(share)));
    for (const rc::Candle& c : s.ohlc)
        QVERIFY(L.y_min <= c.low && L.y_max >= c.high);
    QCOMPARE(L.series_count, 1);
    QCOMPARE(L.legend_position, QStringLiteral("none"));
    QVERIFY(tick_problems(L.y_ticks, L.y_min, L.y_max, false).isEmpty());
    // Candles sit in slots: no tick at the very plot edges.
    for (const rcr::Tick& t : L.x_ticks)
        QVERIFY(t.pos > L.plot.left() && t.pos < L.plot.right());

    const rcr::Layout bare = layout(rc::from_config({{"chart_type", "candle"}, {"ohlc", o}}));
    QVERIFY(bare.volume_plot.isEmpty());

    const rcr::Layout logc =
        layout(rc::from_config({{"chart_type", "candle"}, {"ohlc", o}, {"y_scale", "log"}}));
    QVERIFY(logc.log_y);
}

void TestReportChart::donutAndPieSlices() {
    const rcr::Layout d = layout(rc::from_config(
        {{"chart_type", "donut"}, {"data", "50,30,15,5,-3"}, {"labels", "A,B,C,D,E"}}), 640, 288);
    QCOMPARE(sz(d.slice_fractions), 4); // the negative value is not a slice
    double sum = 0;
    for (double f : d.slice_fractions)
        sum += f;
    QVERIFY(near(sum, 1.0, 1e-12));
    QVERIFY(near(d.slice_fractions[0], 0.5, 1e-12));
    QVERIFY(near(d.plot.width(), d.plot.height(), 1e-9));
    QVERIFY(d.outer.contains(d.plot));
    QCOMPARE(d.series_count, 1);

    QJsonArray a;
    a.append(ser("Stocks", {60}));
    a.append(ser("Bonds", {30}));
    a.append(ser("Cash", {10}));
    const rcr::Layout p = layout(rc::from_config({{"chart_type", "pie"}, {"series", json(a)}}));
    QCOMPARE(sz(p.slice_fractions), 3);
    QCOMPARE(p.legend[0].name, QStringLiteral("Stocks"));
    QVERIFY(near(p.slice_fractions[2], 0.1, 1e-12));

    QVERIFY(layout(rc::from_config({{"chart_type", "pie"}, {"data", "-1,-2"}})).empty);
}

void TestReportChart::hbarLayout() {
    const rcr::Layout L =
        layout(rc::from_config({{"chart_type", "hbar"}, {"data", "3,7,1,5"}, {"labels", "A,B,C,D"}}), 640, 300);
    QCOMPARE(sz(L.y_ticks), 4);
    QCOMPARE(L.y_ticks[0].label, QStringLiteral("A"));
    QCOMPARE(L.y_ticks[3].label, QStringLiteral("D"));
    for (int i = 1; i < 4; ++i)
        QVERIFY(L.y_ticks[i].pos > L.y_ticks[i - 1].pos); // top to bottom
    QVERIFY(L.y_min <= 0.0 && L.y_max >= 7.0);
    const QString err = tick_problems(L.x_ticks, L.y_min, L.y_max, false, false);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(overlap_problems(L.x_ticks, 640).isEmpty());

    // Many categories in a short chart: names thinned so they never stack on each other.
    QStringList names;
    QVector<double> v;
    for (int i = 0; i < 40; ++i) {
        names << QStringLiteral("Name %1").arg(i + 1);
        v << (i % 9) - 3;
    }
    const rcr::Layout crowded =
        layout(rc::from_config({{"chart_type", "hbar"}, {"data", csv(v)}, {"labels", names.join(',')}}), 480, 200);
    QVERIFY(crowded.y_ticks.size() >= 2 && crowded.y_ticks.size() < 40);
    QImage dev(1, 1, QImage::Format_ARGB32_Premultiplied);
    dev.setDotsPerMeterX(3780);
    dev.setDotsPerMeterY(3780);
    const double lh = QFontMetricsF(rcr::chart_font(kFamily, 10, 480), &dev).height();
    for (int i = 1; i < crowded.y_ticks.size(); ++i)
        QVERIFY(crowded.y_ticks[i].pos - crowded.y_ticks[i - 1].pos >= lh);
    // The natural height leaves room for every name.
    const rc::ChartSpec tall = rc::from_config({{"chart_type", "hbar"}, {"data", csv(v)}, {"labels", names.join(',')}});
    QCOMPARE(sz(layout(tall, 640, rcr::chart_size(tall, 640).height()).y_ticks), 40);
}

void TestReportChart::scatterNumericAxis() {
    QJsonArray a;
    a.append(ser("Risk/return", {3, 4, 5, 4.5}, false, {}, {1.5, 2.5, 10, 6}));
    const rcr::Layout L = layout(rc::from_config({{"chart_type", "scatter"}, {"series", json(a)}}));
    QVERIFY(L.y_min <= 3.0 && L.y_max >= 5.0);
    QVERIFY(L.x_ticks.size() >= 2);
    const double step = L.x_ticks[1].value - L.x_ticks[0].value;
    QVERIFY(near(mantissa(step), 1) || near(mantissa(step), 2) || near(mantissa(step), 5));
    QVERIFY(L.x_ticks.first().value >= 1.0 && L.x_ticks.last().value <= 10.5);
    for (int i = 1; i < L.x_ticks.size(); ++i)
        QVERIFY(L.x_ticks[i].pos > L.x_ticks[i - 1].pos);
    // Without x the index is the x.
    const rcr::Layout idx = layout(rc::from_config({{"chart_type", "scatter"}, {"data", "1,4,2,8"}}));
    QVERIFY(!idx.empty && idx.x_ticks.size() >= 2);
}

void TestReportChart::annotationsReserveRoom() {
    const Cfg base{{"data", csv(wave(50))}};
    Cfg with = base;
    with.insert("annot_last", "1");
    const rcr::Layout a = layout(rc::from_config(base));
    const rcr::Layout b = layout(rc::from_config(with));
    QVERIFY(b.plot.right() < a.plot.right() - 10.0);
    QVERIFY(b.outer.contains(b.plot));

    Cfg mm = base;
    mm.insert("annot_minmax", "1");
    const rcr::Layout c = layout(rc::from_config(mm));
    QVERIFY(c.y_max - c.y_min > a.y_max - a.y_min); // headroom for the marks
}

void TestReportChart::emptySpec() {
    const rc::ChartSpec none;
    QVERIFY(none.empty());
    const rcr::Layout L = layout(none);
    QVERIFY(L.empty);
    QVERIFY(L.outer.contains(L.plot));
    QVERIFY(!rcr::render_png(none, light(), kFamily, QSize(400, 200), 1.0).isEmpty());

    const rc::ChartSpec nans = rc::from_config({{"data", "x,x"}});
    QVERIFY(layout(nans).empty);
    QVERIFY(rcr::render_png(none, light(), kFamily, QSize(0, 0), 1.0).isEmpty()); // no size, no image
}

void TestReportChart::valueFormatting() {
    rc::AxisFormat money;
    money.prefix = QStringLiteral("$");
    QCOMPARE(rcr::format_value(12500.0, money), QStringLiteral("$12.50K"));
    QCOMPARE(rcr::format_value(-2500000.0, rc::AxisFormat()), QStringLiteral("-2.50M"));
    QCOMPARE(rcr::format_value(3.2e12, rc::AxisFormat()), QStringLiteral("3.20T"));
    QCOMPARE(rcr::format_value(152.0, rc::AxisFormat()), QStringLiteral("152"));
    QCOMPARE(rcr::format_value(152.37, rc::AxisFormat()), QStringLiteral("152.37"));
    money.compact = false;
    QCOMPARE(rcr::format_value(1234.5, money, 1), QStringLiteral("$1,234.5"));
    QCOMPARE(rcr::format_value(-0.001, rc::AxisFormat(), 2), QStringLiteral("0.00")); // never "-0.00"
    rc::AxisFormat pct;
    pct.suffix = QStringLiteral("%");
    QCOMPARE(rcr::format_value(12.5, pct, 1), QStringLiteral("12.5%"));
    QCOMPARE(rcr::format_value(-12.5, pct, 1), QStringLiteral("-12.5%"));
    QVERIFY(rcr::format_value(kNaN, pct).isEmpty());

    QCOMPARE(rcr::chart_size(rc::from_config({{"data", "1"}}), 600), QSize(600, 300));
}

void TestReportChart::smallChartsScaleFonts() {
    QCOMPARE(rcr::chart_font(kFamily, 10, 640).pixelSize(), 10);
    QCOMPARE(rcr::chart_font(kFamily, 10, 300).pixelSize(), 9);
    QCOMPARE(rcr::chart_font(kFamily, 13, 300, true).pixelSize(), 11);
    QVERIFY(rcr::chart_font(kFamily, 13, 640, true).bold());
    QVERIFY(rcr::chart_font(QStringLiteral("Some Missing Family"), 10, 640).families().contains(QStringLiteral("Arial")));
}

// ── Rendering ────────────────────────────────────────────────────────────────

void TestReportChart::deterministicRender() {
    rc::ChartSpec s = sample_spec(QStringLiteral("line"));
    s.annot_minmax = true;
    const QImage a = rcr::render_image(s, light(), kFamily, QSize(640, 320), 2.0);
    const QImage b = rcr::render_image(s, light(), kFamily, QSize(640, 320), 2.0);
    QCOMPARE(a.size(), QSize(1280, 640));
    QCOMPARE(a.format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(a.sizeInBytes(), b.sizeInBytes());
    QVERIFY(std::memcmp(a.constBits(), b.constBits(), static_cast<size_t>(a.sizeInBytes())) == 0);
    QCOMPARE(rcr::render_png(s, light(), kFamily, QSize(640, 320), 2.0),
             rcr::render_png(s, light(), kFamily, QSize(640, 320), 2.0));
    const QString k1 = rcr::cache_key(s, light(), kFamily, QSize(640, 320), 2.0);
    QCOMPARE(k1, rcr::cache_key(s, light(), kFamily, QSize(640, 320), 2.0));
    QCOMPARE(k1.size(), 40);

    // Paper everywhere at the corner, ink somewhere.
    const QColor paper = QColor::fromString(light().paper);
    QCOMPARE(a.pixelColor(1, 1), paper);
    bool drawn = false;
    for (int y = 0; y < a.height() && !drawn; y += 7)
        for (int x = 0; x < a.width() && !drawn; x += 7)
            drawn = a.pixelColor(x, y) != paper;
    QVERIFY(drawn);
}

void TestReportChart::cacheKeyChanges() {
    const rc::ChartSpec s = sample_spec(QStringLiteral("bar"));
    const QSize size(640, 320);
    const QString base = rcr::cache_key(s, light(), kFamily, size, 2.0);
    rc::ChartSpec data = s;
    data.series[0].values[0] += 1.0;
    QVERIFY(rcr::cache_key(data, light(), kFamily, size, 2.0) != base);
    rc::ChartSpec title = s;
    title.title += QStringLiteral("!");
    QVERIFY(rcr::cache_key(title, light(), kFamily, size, 2.0) != base);
    QVERIFY(rcr::cache_key(s, dark(), kFamily, size, 2.0) != base);
    QVERIFY(rcr::cache_key(s, light(), QStringLiteral("Georgia"), size, 2.0) != base);
    QVERIFY(rcr::cache_key(s, light(), kFamily, QSize(641, 320), 2.0) != base);
    QVERIFY(rcr::cache_key(s, light(), kFamily, size, 3.0) != base);
}

void TestReportChart::pngForEveryKind() {
    const QString dump = dump_dir();
    if (!dump.isEmpty())
        QDir().mkpath(dump);
    for (const auto& choice : rc::kind_choices()) {
        for (const rc::ChartPalette& pal : {light(), dark()}) {
            rc::ChartSpec s = sample_spec(choice.first);
            QCOMPARE(rc::kind_name(s.kind), choice.first);
            const rcr::Layout L = layout(s);
            QVERIFY2(!L.empty, qPrintable(choice.first));
            const QSize logical = rcr::chart_size(s, 560);
            const QByteArray png = rcr::render_png(s, pal, kFamily, logical, 1.5);
            QVERIFY2(png.startsWith("\x89PNG"), qPrintable(choice.first));
            const QImage back = QImage::fromData(png, "PNG");
            QCOMPARE(back.size(), QSize(qRound(logical.width() * 1.5), qRound(logical.height() * 1.5)));
            // Every annotation switched on still renders.
            s.annot_last = s.annot_minmax = true;
            s.events.append(rc::Event{QStringLiteral("3"), QStringLiteral("Event")});
            QVERIFY(!rcr::render_png(s, pal, kFamily, QSize(300, 180), 1.0).isEmpty());
            if (!dump.isEmpty()) {
                const QString name = QStringLiteral("%1/%2_%3.png").arg(dump, choice.first, pal.paper.mid(1));
                QFile f(name);
                if (f.open(QIODevice::WriteOnly))
                    f.write(png);
            }
        }
    }
    const QByteArray empty_png = rcr::render_png(rc::ChartSpec(), light(), kFamily, QSize(320, 160), 1.0);
    QVERIFY(empty_png.startsWith("\x89PNG"));

    if (dump.isEmpty())
        return;
    // Extra pictures for a visual check only.
    QJsonArray combo;
    QVector<double> volume;
    for (int i = 0; i < 40; ++i)
        volume << 2e6 + 1.5e6 * std::sin(i * 0.4);
    combo.append(ser("Volume", volume, true, "bar"));
    combo.append(ser("Price", wave(40, 50.0)));
    const QVector<QPair<QString, rc::ChartSpec>> extras = {
        {"combo", rc::from_config({{"series", json(combo)},
                                   {"dates", business_dates(QDate(2026, 6, 1), 40)},
                                   {"title", "Price and volume"},
                                   {"y_format", "{\"prefix\":\"$\"}"},
                                   {"annot_last", "1"}})},
        {"log", rc::from_config({{"data", "1,3,10,40,90,300,1200,5000,20000"}, {"y_scale", "log"}, {"title", "Log"}})},
        {"minmax", rc::from_config({{"chart_type", "area"},
                                    {"data", csv(wave(120))},
                                    {"dates", business_dates(QDate(2026, 1, 5), 120)},
                                    {"annot_minmax", "1"},
                                    {"annot_last", "1"},
                                    {"title", "High and low"}})},
        {"pct", rc::from_config({{"data", csv(wave(250))},
                                 {"dates", business_dates(QDate(2025, 1, 2), 250)},
                                 {"transform", "pct"},
                                 {"title", "Return since start"}})}};
    for (const auto& e : extras) {
        for (int w : {560, 300}) {
            const QSize logical = rcr::chart_size(e.second, w);
            QFile f(QStringLiteral("%1/x_%2_%3.png").arg(dump, e.first).arg(w));
            if (f.open(QIODevice::WriteOnly))
                f.write(rcr::render_png(e.second, light(), kFamily, logical, 1.5));
        }
    }
}

void TestReportChart::paintRespectsRectAndState() {
    const rc::ChartSpec s = sample_spec(QStringLiteral("area"));
    const QColor outside(1, 2, 3);
    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(outside);
    QPainter p(&img);
    const QPen pen(QColor(10, 20, 30), 3.0);
    p.setPen(pen);
    const QTransform before = p.transform();
    rcr::paint(&p, QRectF(50, 40, 300, 180), s, light(), kFamily);
    QCOMPARE(p.transform(), before);
    QCOMPARE(p.pen().color(), pen.color());
    QCOMPARE(p.pen().widthF(), 3.0);
    QVERIFY(!p.hasClipping());
    p.end();
    QCOMPARE(img.pixelColor(10, 10), outside);
    QCOMPARE(img.pixelColor(395, 295), outside);
    QCOMPARE(img.pixelColor(48, 100), outside);
    QCOMPARE(img.pixelColor(352, 100), outside);
    QCOMPARE(img.pixelColor(51, 41), QColor::fromString(light().paper));
    QCOMPARE(img.pixelColor(348, 218), QColor::fromString(light().paper));

    // Painting into a null / inactive painter or an empty rect is a no-op.
    rcr::paint(nullptr, QRectF(0, 0, 10, 10), s, light(), kFamily);
    QPainter idle;
    rcr::paint(&idle, QRectF(0, 0, 10, 10), s, light(), kFamily);
}

void TestReportChart::rendersOnWorkerThreads() {
    const rc::ChartSpec s = sample_spec(QStringLiteral("candle"));
    const QImage ref = rcr::render_image(s, light(), kFamily, QSize(480, 270), 2.0);
    const rcr::Layout ref_layout = layout(s, 480, 270);
    QImage a, b;
    rcr::Layout la;
    std::thread t1([&]() {
        a = rcr::render_image(s, light(), kFamily, QSize(480, 270), 2.0);
        la = layout(s, 480, 270);
    });
    std::thread t2([&]() { b = rcr::render_image(s, light(), kFamily, QSize(480, 270), 2.0); });
    t1.join();
    t2.join();
    QVERIFY(!a.isNull() && !b.isNull());
    QVERIFY(a == ref);
    QVERIFY(b == ref);
    QCOMPARE(la.plot, ref_layout.plot);
    QCOMPARE(sz(la.x_ticks), sz(ref_layout.x_ticks));
}

void TestReportChart::pdfOutput() {
    // Under the working directory (the test's own build folder), removed afterwards.
    QTemporaryDir dir(QDir::currentPath() + QStringLiteral("/tst_report_chart_XXXXXX"));
    QVERIFY(dir.isValid());
    QString path = dir.filePath(QStringLiteral("out.pdf"));
    if (!dump_dir().isEmpty())
        path = dump_dir() + QStringLiteral("/out.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(1200);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter p(&writer);
        QVERIFY(p.isActive());
        const double k = 1200.0 / 96.0; // 96-dpi logical px -> device
        p.scale(k, k);
        rcr::paint(&p, QRectF(48, 48, 600, 300), sample_spec(QStringLiteral("line")), light(), kFamily);
        rcr::paint(&p, QRectF(48, 370, 600, 336), sample_spec(QStringLiteral("candle")), light(), kFamily);
        rcr::paint(&p, QRectF(48, 726, 300, 160), sample_spec(QStringLiteral("donut")), dark(), kFamily);
        rcr::paint(&p, QRectF(360, 726, 288, 160), sample_spec(QStringLiteral("hbar")), light(), kFamily);
        QVERIFY(p.end());
    }
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.size() > 1000);
    QCOMPARE(f.read(5), QByteArray("%PDF-"));
}

QTEST_MAIN(TestReportChart)
#include "tst_report_chart.moc"
