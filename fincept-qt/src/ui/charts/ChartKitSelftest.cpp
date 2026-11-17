// ChartKitSelftest — headless verification of the shared chart kit.
//
// Also the compile-and-construct check for every kit widget: each one is
// built, fed a 10k-point data set, laid out and rendered once, so a widget
// nobody has adopted yet still gets exercised at smoke time.
//
// Run: FinceptTerminal --selftest-chart-kit

#include "ui/charts/ChartKitSelftest.h"

#include "ui/charts/CandleChart.h"
#include "ui/charts/ChartAxisTicks.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/ChartKit.h"
#include "ui/charts/Sparkline.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QImage>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextStream>
#include <QTimeZone>

#include <cmath>

namespace fincept::ui {
namespace {

int cks_failures = 0;

void cks_check(bool ok, const QString& what) {
    QTextStream out(stdout);
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++cks_failures;
}

void cks_note(const QString& what) {
    QTextStream out(stdout);
    out << "  INFO  " << what << "\n";
}

/// Pixels that differ from the top-left pixel (the canvas background).
int cks_painted_pixels(const QPixmap& pm) {
    if (pm.isNull())
        return 0;
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    const QRgb bg = img.pixel(0, 0);
    int n = 0;
    for (int y = 0; y < img.height(); y += 2)
        for (int x = 0; x < img.width(); x += 2)
            if (img.pixel(x, y) != bg)
                ++n;
    return n;
}

void cks_realize(QWidget& w, int width, int height) {
    w.setAttribute(Qt::WA_DontShowOnScreen, true);
    w.resize(width, height);
    w.show();
    QCoreApplication::processEvents();
}

/// Daily closes as a random-ish walk with one spike, epoch ms x.
QVector<QPointF> cks_walk(int n, double start, double seed) {
    QVector<QPointF> pts;
    pts.reserve(n);
    const double t0 = static_cast<double>(QDate(1990, 1, 1).startOfDay(QTimeZone::UTC).toMSecsSinceEpoch());
    double v = start;
    for (int i = 0; i < n; ++i) {
        v *= 1.0 + 0.01 * std::sin(i * 0.37 + seed) + 0.0004;
        pts.append({t0 + i * 86400000.0, v});
    }
    return pts;
}

void cks_ticks() {
    using namespace chart_ticks;
    const auto t = nice_ticks(0.0, 100.0, 5);
    cks_check(t.step == 20.0 && t.values.size() == 6 && t.values.first() == 0.0 && t.values.last() == 100.0,
              "nice_ticks(0,100,5) → 0,20,…,100");
    const auto f = nice_ticks(0.013, 0.087, 5);
    cks_check(std::abs(f.step - 0.02) < 1e-12 && f.decimals == 2, "nice_ticks on 0.013..0.087 steps by 0.02 with 2 decimals");
    const auto neg = nice_ticks(-37.0, 12.0, 6);
    cks_check(neg.values.contains(0.0) && neg.values.first() >= -37.0 && neg.values.last() <= 12.0,
              "nice_ticks across zero includes an exact 0 and stays inside the range");
    const auto lg = log_ticks(1.0, 1000.0, 6);
    cks_check(lg.values.contains(1.0) && lg.values.contains(10.0) && lg.values.contains(100.0) &&
                  lg.values.contains(1000.0),
              "log_ticks(1,1000) has every decade");

    const double from = static_cast<double>(QDate(2024, 1, 15).startOfDay().toMSecsSinceEpoch());
    const double to = static_cast<double>(QDate(2025, 1, 15).startOfDay().toMSecsSinceEpoch());
    const auto tt = time_ticks(from, to, 8);
    bool ascending = true, inside = true, month_starts = true;
    for (int i = 0; i < tt.values.size(); ++i) {
        if (i > 0 && tt.values[i] <= tt.values[i - 1])
            ascending = false;
        if (tt.values[i] < from || tt.values[i] > to)
            inside = false;
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(tt.values[i]));
        if (dt.date().day() != 1 || dt.time() != QTime(0, 0))
            month_starts = false;
    }
    cks_check(tt.step.unit == TimeUnit::Month && !tt.values.isEmpty() && tt.values.size() <= 8,
              "time_ticks over a year steps by months, within the tick budget");
    cks_check(ascending && inside && month_starts, "time ticks are ascending, in range and on local month starts");

    const auto hours = time_ticks(from, from + 6 * 3600000.0, 7);
    cks_check(hours.step.unit == TimeUnit::Hour && hours.values.size() >= 6, "time_ticks over 6 hours steps by hours");
}

void cks_decimation() {
    const int n = 100000;
    QVector<double> xs(n), ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = i;
        ys[i] = std::sin(i * 0.001);
    }
    ys[54321] = 50.0;  // spike up
    ys[12345] = -50.0; // spike down
    ys[70000] = std::numeric_limits<double>::quiet_NaN();
    QVector<int> idx;
    const int cols = 800;
    chartkit::decimate_minmax(xs.constData(), ys.constData(), 0, n, 0.0, static_cast<double>(cols) / n, idx);
    cks_check(idx.size() <= cols * 4 + 4, QStringLiteral("100k points decimate to %1 (<= 4 per column)").arg(idx.size()));
    cks_check(idx.contains(54321) && idx.contains(12345), "decimation keeps both spikes");
    cks_check(idx.contains(70000), "decimation keeps the gap marker");
    cks_check(std::is_sorted(idx.begin(), idx.end()), "decimated indices stay in order");
}

void cks_format_and_csv() {
    cks_check(chartkit::format_number(-0.0001, 2) == chartkit::format_number(0.0, 2), "format_number never prints -0.00");
    cks_check(chartkit::format_signed_percent(1.234, 2).startsWith(QLocale().positiveSign()),
              "format_signed_percent prefixes the plus sign");
    cks_check(chartkit::format_compact(2'500'000.0, 1).endsWith(chartkit::ChartKitText::tr("M", "millions suffix")),
              "format_compact uses the millions suffix");

    cks_check(ChartExport::csv_cell(QStringLiteral("=1+2")) == QStringLiteral("'=1+2"), "CSV neutralises formulas");
    cks_check(ChartExport::csv_cell(QStringLiteral("-1.5")) == QStringLiteral("-1.5"), "CSV keeps negative numbers as-is");
    cks_check(ChartExport::csv_cell(QStringLiteral("a,\"b\"")) == QStringLiteral("\"a,\"\"b\"\"\""),
              "CSV quotes commas and doubles quotes");
    const QString csv = ChartExport::to_csv({QStringLiteral("A"), QStringLiteral("B")}, {{QStringLiteral("1"), QStringLiteral("2")}});
    cks_check(csv == QStringLiteral("A,B\r\n1,2\r\n"), "to_csv writes CRLF rows");
}

void cks_time_series_chart() {
    TimeSeriesChart chart;
    cks_realize(chart, 900, 560);

    const auto a = cks_walk(10000, 100.0, 0.0);
    const auto b = cks_walk(10000, 80.0, 1.3);
    TimeSeriesChart::SeriesSpec sa;
    sa.name = QStringLiteral("A");
    sa.points = a;
    sa.style = TimeSeriesChart::Style::Area;
    const int ia = chart.add_series(sa);
    const int ib = chart.add_series({.name = QStringLiteral("B"), .points = b});
    const int pane = chart.add_pane(0.4);
    QVector<QPointF> dd;
    dd.reserve(a.size());
    double peak = 0.0;
    for (const auto& p : a) {
        peak = std::max(peak, p.y());
        dd.append({p.x(), (p.y() / peak - 1.0) * 100.0});
    }
    chart.add_series({.name = QStringLiteral("DD"), .points = dd, .style = TimeSeriesChart::Style::Area, .pane = pane,
                      .color = TimeSeriesChart::ColorRole::Negative, .baseline = 0.0});
    const int vol_pane = chart.add_pane(0.3);
    QVector<QPointF> rets;
    for (int i = 1; i < a.size(); ++i)
        rets.append({a[i].x(), a[i].y() / a[i - 1].y() - 1.0});
    chart.add_series({.name = QStringLiteral("R"), .points = rets, .style = TimeSeriesChart::Style::Bars,
                      .pane = vol_pane, .color_by_sign = true});
    chart.add_series({.name = QStringLiteral("S"), .points = cks_walk(500, 1.0, 2.0), .style = TimeSeriesChart::Style::Step,
                      .axis = TimeSeriesChart::Axis::Right, .dashed = true});
    chart.add_series({.name = QStringLiteral("BL"), .points = rets, .style = TimeSeriesChart::Style::Baseline,
                      .pane = vol_pane, .show_in_legend = false});
    chart.set_title(QStringLiteral("Selftest"));
    chart.set_range_presets({QStringLiteral("1M"), QStringLiteral("1Y"), QStringLiteral("YTD"), QStringLiteral("ALL")});
    chart.set_show_mode_buttons(true);
    chart.add_marker(a[5000].x(), ia, TimeSeriesChart::MarkerKind::Buy, QStringLiteral("B"), QStringLiteral("buy"));
    chart.add_marker(a[6000].x(), ia, TimeSeriesChart::MarkerKind::Sell, QStringLiteral("S"));
    chart.add_marker({.x = a[7000].x(), .kind = TimeSeriesChart::MarkerKind::Earnings, .tooltip = QStringLiteral("Q3")});
    chart.add_price_marker(a[8000].x(), a[8000].y(), TimeSeriesChart::MarkerKind::Dividend);
    chart.add_hline(150.0, QStringLiteral("Target"), TimeSeriesChart::ColorRole::Accent);
    chart.add_band(a[2000].x(), a[2500].x(), TimeSeriesChart::ColorRole::Info, QStringLiteral("Recession"));

    QElapsedTimer timer;
    timer.start();
    const QPixmap full = chart.render_image();
    const qint64 ms_full = timer.elapsed();
    cks_check(!full.isNull() && cks_painted_pixels(full) > 2000, "TimeSeriesChart renders 6 series / 3 panes");
    // The first render in the process also pays for font-database setup; the
    // warm figure is what a pan / zoom / data update costs.
    chart.set_title(QStringLiteral("Selftest (warm)"));
    timer.restart();
    chart.render_image();
    cks_note(QStringLiteral("TimeSeriesChart render (~41k points): cold %1 ms, warm %2 ms")
                 .arg(ms_full)
                 .arg(timer.elapsed()));

    int ranges = 0;
    QObject::connect(&chart, &TimeSeriesChart::range_changed, [&ranges](double, double) { ++ranges; });
    chart.set_active_preset(QStringLiteral("1Y"));
    const double span_days = (chart.x_to() - chart.x_from()) / 86400000.0;
    cks_check(span_days > 360.0 && span_days < 370.0, QStringLiteral("1Y preset shows ~365 days (%1)").arg(span_days));
    cks_check(ranges == 1, "range_changed fires once for a preset");
    cks_check(chart.active_preset() == QStringLiteral("1Y"), "active_preset reports the preset");

    chart.set_x_range(a[100].x(), a[400].x());
    cks_check(std::abs(chart.x_from() - a[100].x()) < 1.0 && chart.active_preset().isEmpty(),
              "set_x_range shows the requested window and clears the preset");
    chart.set_value_mode(TimeSeriesChart::ValueMode::Percent);
    chart.set_log_scale(true);
    cks_check(cks_painted_pixels(chart.render_image()) > 2000, "renders in Percent mode with log scale requested");
    chart.set_value_mode(TimeSeriesChart::ValueMode::Rebased100);
    cks_check(cks_painted_pixels(chart.render_image()) > 2000, "renders in Rebased100 mode");
    chart.set_value_mode(TimeSeriesChart::ValueMode::Absolute);
    chart.reset_x_range();
    cks_check(chart.x_from() <= a.first().x() + 1.0, "reset_x_range returns to the full data");

    chart.set_series_visible(ib, false);
    cks_check(!chart.series_visible(ib), "set_series_visible hides a series");
    chart.append_point(ia, a.last().x() + 86400000.0, 123.0);
    cks_check(chart.point_count(ia) == 10001, "append_point adds a point");
    chart.append_point(ia, a.last().x() + 86400000.0, 124.0);
    cks_check(chart.point_count(ia) == 10001, "append_point at the same x replaces it");

    QStringList headers;
    QVector<QStringList> rows;
    chart.csv_table(headers, rows);
    cks_check(!headers.isEmpty() && rows.size() >= 10000, QStringLiteral("csv_table exports %1 rows").arg(rows.size()));

    chart.set_state(TimeSeriesChart::State::Error, QStringLiteral("boom"));
    cks_check(cks_painted_pixels(chart.render_image()) > 50, "Error state renders a message");
    chart.clear();
    chart.set_state(TimeSeriesChart::State::Ready);
    cks_check(chart.series_ids().isEmpty() && cks_painted_pixels(chart.render_image()) > 50,
              "clear() empties the chart and the empty state renders");
}

void cks_candle_chart() {
    CandleChart chart;
    cks_realize(chart, 900, 520);
    QVector<CandleChart::Bar> bars;
    const auto walk = cks_walk(12000, 50.0, 0.4);
    bars.reserve(walk.size());
    double prev = walk.first().y();
    for (const auto& p : walk) {
        const double o = prev;
        const double c = p.y();
        bars.append({p.x(), o, std::max(o, c) * 1.01, std::min(o, c) * 0.99, c, 1e6 + std::fmod(p.x(), 7e5)});
        prev = c;
    }
    chart.set_bars(bars);
    QVector<double> ma(bars.size(), std::numeric_limits<double>::quiet_NaN());
    double sum = 0.0;
    for (int i = 0; i < bars.size(); ++i) {
        sum += bars[i].close;
        if (i >= 50)
            sum -= bars[i - 50].close;
        if (i >= 49)
            ma[i] = sum / 50.0;
    }
    chart.add_overlay_series(QStringLiteral("MA 50"), ma);
    chart.add_trade_marker(bars[bars.size() - 20].time_ms, bars[bars.size() - 20].low, CandleChart::TradeSide::Buy,
                           QStringLiteral("B"), QStringLiteral("bought"));
    chart.add_trade_marker(bars[bars.size() - 5].time_ms, bars[bars.size() - 5].high, CandleChart::TradeSide::Sell);
    chart.add_hline(bars.last().close * 0.98, QStringLiteral("Stop"), ChartColorRole::Negative);

    QElapsedTimer timer;
    timer.start();
    const QPixmap pm = chart.render_image();
    cks_note(QStringLiteral("CandleChart render (120 of 12k bars): %1 ms").arg(timer.elapsed()));
    cks_check(cks_painted_pixels(pm) > 2000, "CandleChart renders candles, volume, overlay and markers");

    chart.set_default_visible_bars(20000);
    chart.reset_view();
    timer.restart();
    cks_check(cks_painted_pixels(chart.render_image()) > 2000, "CandleChart renders all 12k bars (aggregated columns)");
    cks_note(QStringLiteral("CandleChart render (12k bars): %1 ms").arg(timer.elapsed()));

    chart.set_log_scale(true);
    cks_check(cks_painted_pixels(chart.render_image()) > 2000, "CandleChart renders on a log scale");

    CandleChart::Bar next = bars.last();
    next.time_ms += 86400000.0;
    chart.update_bar(next);
    cks_check(chart.bar_count() == bars.size() + 1, "update_bar appends a newer bar");
    next.close += 1.0;
    chart.update_bar(next);
    cks_check(chart.bar_count() == bars.size() + 1 && chart.bars().last().close == next.close,
              "update_bar replaces the last bar");

    QStringList headers;
    QVector<QStringList> rows;
    chart.csv_table(headers, rows);
    cks_check(headers.size() == 7 && rows.size() == chart.bar_count(), "CandleChart csv_table has OHLCV + overlay");
}

void cks_small_widgets() {
    Sparkline spark;
    cks_realize(spark, 160, 28);
    QVector<double> v(10000);
    for (int i = 0; i < v.size(); ++i)
        v[i] = std::sin(i * 0.01) + i * 0.0001;
    spark.set_data(v);
    spark.set_min_max_dots(true);
    spark.set_fill(true);
    cks_check(cks_painted_pixels(spark.grab()) > 20, "Sparkline paints 10k values");
    cks_check(Sparkline::resolve_color({1.0, 2.0}, {}) == chartkit::up() &&
                  Sparkline::resolve_color({2.0, 1.0}, {}) == chartkit::down(),
              "Sparkline auto colour follows first → last");
    SparklineStyle inverted;
    inverted.invert = true;
    cks_check(Sparkline::resolve_color({1.0, 2.0}, inverted) == chartkit::down(), "inverted sparkline swaps colours");

    cks_check(SparklineDelegate::values_from(QVariant::fromValue(QVector<double>{1.0, 2.0, 3.0})).size() == 3,
              "SparklineDelegate reads QVector<double>");
    cks_check(SparklineDelegate::values_from(QVariantList{1, 2.5, QStringLiteral("3")}).size() == 3,
              "SparklineDelegate reads QVariantList");
    QStandardItemModel model(4, 2);
    for (int r = 0; r < 4; ++r) {
        model.setData(model.index(r, 0), QStringLiteral("ROW %1").arg(r));
        model.setData(model.index(r, 1), QVariant::fromValue(QVector<double>{1.0, 3.0 - r, 2.0, 4.0 + r}),
                      SparklineDelegate::kDefaultRole);
    }
    QTableView view;
    view.setModel(&model);
    view.setItemDelegateForColumn(1, new SparklineDelegate(&view));
    cks_realize(view, 400, 200);
    cks_check(!view.grab().isNull(), "SparklineDelegate paints inside a QTableView");

    KpiStrip strip;
    for (int i = 0; i < 7; ++i) {
        StatTile* t = strip.add_tile(QStringLiteral("k%1").arg(i), QStringLiteral("METRIC %1").arg(i));
        t->set_number(1234.5678 * (i + 1), 2);
        t->set_delta(i % 2 ? -0.42 : 1.37);
        t->set_sub_label(QStringLiteral("sub"));
        t->set_sparkline(v.mid(0, 200));
    }
    strip.tile(QStringLiteral("k1"))->set_tone(StatTile::Tone::Positive);
    strip.tile(QStringLiteral("k2"))->set_state(StatTile::State::Error, QStringLiteral("timeout"));
    strip.tile(QStringLiteral("k3"))->set_state(StatTile::State::Stale, QStringLiteral("10 min old"));
    strip.tile(QStringLiteral("k4"))->set_state(StatTile::State::Loading);
    cks_check(strip.count() == 7 && strip.tile(QStringLiteral("k6")) != nullptr, "KpiStrip holds 7 keyed tiles");

    const int one_row = strip.heightForWidth(10000);
    const int narrow = strip.heightForWidth(200);
    cks_check(narrow > one_row, "KpiStrip wraps into more rows when narrow");
    cks_realize(strip, 700, strip.heightForWidth(700));
    QCoreApplication::processEvents();
    bool laid_out = true;
    for (int i = 0; i < strip.count(); ++i)
        if (strip.tile(i)->width() <= 0)
            laid_out = false;
    cks_check(laid_out, "KpiStrip gives every tile a geometry");
    cks_check(cks_painted_pixels(strip.grab()) > 200, "StatTiles paint (normal, tone, error, stale, loading)");
    strip.remove_tile(strip.tile(0));
    cks_check(strip.count() == 6, "KpiStrip remove_tile");
}

} // namespace

int run_chart_kit_selftest() {
    QTextStream out(stdout);
    out << "=== Chart kit selftest ===\n";
    out.flush();
    cks_failures = 0;
    cks_ticks();
    cks_decimation();
    cks_format_and_csv();
    cks_time_series_chart();
    cks_candle_chart();
    cks_small_widgets();
    out << (cks_failures == 0 ? QStringLiteral("=== Chart kit: ALL PASS ===\n")
                              : QStringLiteral("=== Chart kit: %1 FAILURE(S) ===\n").arg(cks_failures));
    return cks_failures == 0 ? 0 : 1;
}

} // namespace fincept::ui
