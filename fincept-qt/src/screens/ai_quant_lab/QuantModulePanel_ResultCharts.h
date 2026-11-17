// src/screens/ai_quant_lab/QuantModulePanel_ResultCharts.h
//
// Private header — included only by QuantModulePanel*.cpp translation units.
// Chart helpers shared by the AI Quant Lab result displays, built on the
// terminal's chart kit (TimeSeriesChart, FanChart, DonutChart, QtCharts via
// ChartFactory), so every module draws its series, forecasts, equity curves,
// frontiers and weights the same way:
//
//   * qc_series_chart   — the whole series (one or more lines, optional extra
//                          panes) instead of a 12-15 row sample;
//   * qc_forecast_chart — history + forecast with its interval (TimeSeriesChart
//                          when the history is known, FanChart otherwise);
//   * qc_equity_chart   — equity / growth curve, dashed benchmark, drawdown pane;
//   * qc_frontier_chart — risk / return frontier with the max-Sharpe and
//                          min-variance points;
//   * qc_weights_donut  — long-only weights as a donut;
//   * qc_latest_rows / qc_caption — the "latest N rows, newest first" table
//                          that sits under a series chart.
//
// Every colour comes from the theme at paint time (the kit widgets) or is a
// theme token that QuantModulePanel::refresh_theme() re-maps (QtCharts).
#pragma once

#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/charts/ChartFactory.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/FanChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QChartView>
#include <QCoreApplication>
#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QLocale>
#include <QPainter>
#include <QPair>
#include <QPen>
#include <QRegularExpression>
#include <QScatterSeries>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace fincept::screens::quant_charts {

using ValueFmt = std::function<QString(double)>;

inline constexpr double kQcNaN = std::numeric_limits<double>::quiet_NaN();

inline QString qc_tr(const char* text) {
    return QCoreApplication::translate("QuantModulePanel", text);
}

// ── Value formatters (locale grouping; NaN → em dash) ───────────────────────

/// Plain number with `decimals`.
inline ValueFmt qc_fmt_num(int decimals) {
    return [decimals](double v) {
        return std::isfinite(v) ? QLocale().toString(v, 'f', decimals) : QString::fromUtf8("—");
    };
}
/// Decimals that suit the magnitude (prices 2, small ratios more, K/M/B above
/// ten million) — for series whose unit the script does not state.
inline ValueFmt qc_fmt_auto() {
    return [](double v) { return ui::chartkit::format_auto(v); };
}
/// A value that is already in percent: 12.5 → "12.50%".
inline ValueFmt qc_fmt_pct(int decimals) {
    return [decimals](double v) {
        return std::isfinite(v) ? QLocale().toString(v, 'f', decimals) + QLatin1Char('%') : QString::fromUtf8("—");
    };
}

// ── X axis ──────────────────────────────────────────────────────────────────

/// x of a series: epoch ms (UTC) when every label is an ISO date, otherwise
/// the number in each label ("obs 12" → 12, "t+3" → 3, "120" → 120),
/// otherwise the row position.
struct XAxis {
    QVector<double> xs;
    bool time = false;
};

/// "2024-01-31", "2024-01-31 15:30:00", "2024-01-31T15:30" → epoch ms (UTC
/// wall clock; charts using it call set_utc(true) so dates never shift).
inline bool qc_date_ms(const QString& label, double& ms) {
    const QString t = label.trimmed();
    if (t.size() < 10 || t.at(4) != QLatin1Char('-') || t.at(7) != QLatin1Char('-'))
        return false;
    const QDate d = QDate::fromString(t.left(10), Qt::ISODate);
    if (!d.isValid())
        return false;
    double day_ms = 0.0;
    if (t.size() >= 16 && (t.at(10) == QLatin1Char(' ') || t.at(10) == QLatin1Char('T'))) {
        const QTime tm = QTime::fromString(t.mid(11, 8), Qt::ISODate);
        if (tm.isValid())
            day_ms = tm.msecsSinceStartOfDay();
    }
    ms = static_cast<double>(d.toJulianDay() - 2440588) * 86400000.0 + day_ms; // 2440588 = 1970-01-01
    return true;
}

/// The number in a label: "12" → 12, "obs 12" → 12, "t+3" → 3.
inline bool qc_label_number(const QString& label, double& v) {
    bool ok = false;
    v = label.trimmed().toDouble(&ok);
    if (ok)
        return true;
    static const QRegularExpression re(QStringLiteral("(-?\\d+(?:\\.\\d+)?)"));
    const QRegularExpressionMatch m = re.match(label);
    if (!m.hasMatch())
        return false;
    v = m.captured(1).toDouble(&ok);
    return ok;
}

/// 0, 1, 2, … n-1.
inline XAxis qc_x_positions(qsizetype n) {
    XAxis out;
    out.xs.reserve(n);
    for (qsizetype i = 0; i < n; ++i)
        out.xs.append(static_cast<double>(i));
    return out;
}

/// x of every row of `rows` (objects) from the first of `keys` each row has.
inline XAxis qc_x_axis(const QJsonArray& rows, const QStringList& keys) {
    const qsizetype n = rows.size();
    QVector<double> dates;
    QVector<double> nums;
    bool all_dates = n > 0;
    bool all_nums = n > 0;
    for (qsizetype i = 0; i < n && (all_dates || all_nums); ++i) {
        const QJsonObject o = rows.at(i).toObject();
        QJsonValue v;
        for (const QString& k : keys) {
            if (o.contains(k)) {
                v = o.value(k);
                break;
            }
        }
        if (v.isDouble()) {
            all_dates = false;
            nums.append(v.toDouble());
            continue;
        }
        const QString s = v.toString();
        double d = 0.0;
        double x = 0.0;
        if (all_dates && qc_date_ms(s, d))
            dates.append(d);
        else
            all_dates = false;
        if (all_nums && qc_label_number(s, x))
            nums.append(x);
        else
            all_nums = false;
    }
    XAxis out;
    if (all_dates && dates.size() == n) {
        out.xs = dates;
        out.time = true;
    } else if (all_nums && nums.size() == n) {
        out.xs = nums;
    } else {
        out = qc_x_positions(n);
    }
    return out;
}

/// Values of `key` in every row (NaN where missing or not a number).
inline QVector<double> qc_column(const QJsonArray& rows, const QString& key) {
    QVector<double> out;
    out.reserve(rows.size());
    for (const QJsonValue& r : rows) {
        const QJsonValue v = r.toObject().value(key);
        out.append(v.isDouble() ? v.toDouble() : kQcNaN);
    }
    return out;
}

/// A scalar array as doubles (null / text → NaN).
inline QVector<double> qc_values(const QJsonArray& arr) {
    QVector<double> out;
    out.reserve(arr.size());
    for (const QJsonValue& v : arr)
        out.append(v.isDouble() ? v.toDouble() : kQcNaN);
    return out;
}

// ── Series chart ────────────────────────────────────────────────────────────

struct QcLine {
    QString name;
    QVector<double> xs; ///< empty = the chart's shared x
    QVector<double> ys; ///< NaN = gap
    ui::ChartColorRole color = ui::ChartColorRole::Auto;
    int palette = -1;
    ui::TimeSeriesChart::Style style = ui::TimeSeriesChart::Style::Line;
    bool dashed = false;
    qreal width = 1.5;
    int pane = 0; ///< 0 = main pane; 1, 2, … = panes below, in order
    bool has_baseline = false;
    double baseline = 0.0;
    bool visible = true;
};

struct QcChartSpec {
    QString title;
    bool x_time = false;
    QVector<double> xs; ///< shared x of lines without their own
    QVector<QcLine> lines;
    ValueFmt fmt;                ///< pane 0 values
    QStringList pane_titles;     ///< panes 1, 2, …
    QVector<ValueFmt> pane_fmts; ///< panes 1, 2, …
    /// Crosshair / tooltip title of an x value on a numeric axis ("obs 12").
    std::function<QString(double)> x_label;
    int height = 260; ///< at the default font size; scales with it
};

/// The whole series as a TimeSeriesChart: crosshair tooltip with every line's
/// value, clickable legend, right-click copy / PNG / CSV. Wheel zoom is off so
/// the result page keeps scrolling under the mouse.
inline ui::TimeSeriesChart* qc_series_chart(const QcChartSpec& s, QWidget* parent) {
    using TSC = ui::TimeSeriesChart;
    auto* chart = new TSC(parent);
    chart->set_x_is_time(s.x_time);
    chart->set_utc(true);
    chart->set_show_mode_buttons(false);
    chart->set_zoom_enabled(false);
    chart->set_title(s.title);
    if (!s.title.isEmpty())
        chart->set_export_name(s.title);
    int panes = 0;
    for (const QcLine& l : s.lines)
        panes = std::max(panes, l.pane);
    for (int p = 1; p <= panes; ++p) {
        const int idx = chart->add_pane(0.45);
        if (p - 1 < s.pane_titles.size())
            chart->set_pane_title(idx, s.pane_titles.at(p - 1));
    }
    chart->setFixedHeight(quant_styles::row_px(s.height + 90 * panes));

    int shown = 0;
    for (const QcLine& l : s.lines) {
        const QVector<double>& xs = l.xs.isEmpty() ? s.xs : l.xs;
        const qsizetype n = std::min(xs.size(), l.ys.size());
        if (n == 0)
            continue;
        TSC::SeriesSpec spec;
        spec.name = l.name;
        spec.id = l.name;
        spec.xs = xs.mid(0, n);
        spec.ys = l.ys.mid(0, n);
        spec.style = l.style;
        spec.pane = l.pane;
        spec.color = l.color;
        spec.palette_index = l.palette;
        spec.dashed = l.dashed;
        spec.line_width = l.width;
        if (l.has_baseline)
            spec.baseline = l.baseline;
        spec.visible = l.visible;
        chart->add_series(spec);
        ++shown;
    }
    if (s.fmt)
        chart->set_value_formatter(TSC::Axis::Left, s.fmt, 0);
    for (int p = 1; p <= panes && p - 1 < s.pane_fmts.size(); ++p) {
        if (s.pane_fmts.at(p - 1))
            chart->set_value_formatter(TSC::Axis::Left, s.pane_fmts.at(p - 1), p);
    }
    if (!s.x_time && s.x_label)
        chart->set_time_formatter(s.x_label);
    chart->set_show_legend(shown > 1);
    if (shown == 0)
        chart->set_state(TSC::State::Empty, qc_tr("No data points in this result."));
    return chart;
}

/// A small caption line above a table or a chart without its own title.
inline QLabel* qc_caption(const QString& text) {
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) +
                             "px; font-weight:700; padding:4px 0 0 2px; background:transparent;")
                         .arg(ui::colors::TEXT_TERTIARY()));
    return l;
}

/// Indexes of the `n` most recent rows of a series of `size`, newest first —
/// the table under a series chart always ends on (starts with) the final row.
inline QVector<int> qc_latest_rows(qsizetype size, int n = 10) {
    QVector<int> out;
    for (qsizetype i = size - 1; i >= 0 && out.size() < n; --i)
        out.append(static_cast<int>(i));
    return out;
}

/// "LATEST 10 OF 252 POINTS · NEWEST FIRST — FULL SERIES IN THE CHART"
inline QString qc_latest_caption(int shown, qsizetype total) {
    if (shown >= total)
        return QCoreApplication::translate("QuantModulePanel", "ALL %1 POINTS  ·  NEWEST FIRST").arg(total);
    return QCoreApplication::translate("QuantModulePanel", "LATEST %1 OF %2 POINTS  ·  NEWEST FIRST  ·  FULL SERIES IN THE CHART")
        .arg(shown)
        .arg(total);
}

// ── Forecast chart ──────────────────────────────────────────────────────────

struct QcBand {
    double lower_pct = 5;
    double upper_pct = 95;
    QVector<double> lower; ///< one per forecast step
    QVector<double> upper;
    QString lower_name; ///< legend name (default "P<lower_pct>")
    QString upper_name;
};

struct QcForecast {
    QString title;
    /// Observed history (optional) on the same x unit as the forecast.
    XAxis history_x;
    QVector<double> history;
    /// In-sample fit (optional), hidden until its legend entry is clicked.
    QVector<double> fitted_x;
    QVector<double> fitted;
    /// Forecast steps: x of each step (steps 1..N when there is no history).
    QVector<double> fc_x;
    QVector<double> point;
    QString point_name;
    QVector<QcBand> bands;
    /// The last actual observation the forecast continues from (NaN = unknown).
    double anchor_x = 0.0;
    double anchor_y = kQcNaN;
    QString anchor_name; ///< reference-line label (default "Last actual")
    ValueFmt fmt;
    std::function<QString(double)> x_label;
};

/// "t+3" for a step axis (0 = the last actual); blank between whole steps.
inline std::function<QString(double)> qc_step_label() {
    return [](double x) {
        const double r = std::round(x);
        if (std::abs(x - r) > 1e-9)
            return QString();
        if (r == 0.0)
            return QCoreApplication::translate("QuantModulePanel", "last");
        return QStringLiteral("t+%1").arg(static_cast<qlonglong>(r));
    };
}

/// History + forecast with its interval, continuing from the last actual
/// value. With a history: TimeSeriesChart (actual, forecast, band edges, the
/// forecast region shaded). Without one but with bands: FanChart (nested
/// shaded bands opening from the last actual, median line, "Last actual"
/// reference line). Point forecast without history: TimeSeriesChart with the
/// last actual as a reference line.
inline QWidget* qc_forecast_chart(const QcForecast& f, QWidget* parent) {
    const bool anchored = std::isfinite(f.anchor_y);
    const QString anchor_name = f.anchor_name.isEmpty() ? qc_tr("Last actual") : f.anchor_name;
    const auto band_name = [](double pct) { return QStringLiteral("P%1").arg(pct, 0, 'g', 3); };

    if (f.history.isEmpty() && !f.bands.isEmpty()) {
        auto* box = new QWidget(parent);
        auto* vl = new QVBoxLayout(box);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(2);
        if (!f.title.isEmpty())
            vl->addWidget(qc_caption(f.title));
        auto* fan = new ui::FanChart(box);
        QVector<double> x;
        if (anchored)
            x.append(f.anchor_x);
        x += f.fc_x;
        QVector<ui::FanBand> bands;
        for (const QcBand& b : f.bands) {
            ui::FanBand fb;
            fb.lower_pct = b.lower_pct;
            fb.upper_pct = b.upper_pct;
            if (anchored) {
                fb.lower.append(f.anchor_y);
                fb.upper.append(f.anchor_y);
            }
            fb.lower += b.lower;
            fb.upper += b.upper;
            bands.append(fb);
        }
        QVector<double> median;
        if (!f.point.isEmpty()) {
            if (anchored)
                median.append(f.anchor_y);
            median += f.point;
        }
        fan->set_fan(x, bands, median);
        if (anchored)
            fan->add_reference_line(f.anchor_y, anchor_name);
        fan->set_value_formatter(f.fmt ? f.fmt : qc_fmt_auto());
        fan->set_x_formatter(f.x_label ? f.x_label : qc_step_label());
        fan->setFixedHeight(quant_styles::row_px(260));
        vl->addWidget(fan);
        return box;
    }

    QcChartSpec s;
    s.title = f.title;
    s.x_time = f.history_x.time;
    s.fmt = f.fmt ? f.fmt : qc_fmt_auto();
    s.x_label = f.x_label ? f.x_label : (f.history.isEmpty() ? qc_step_label() : std::function<QString(double)>{});
    s.height = 280;
    if (!f.history.isEmpty()) {
        QcLine a;
        a.name = qc_tr("Actual");
        a.xs = f.history_x.xs;
        a.ys = f.history;
        a.color = ui::ChartColorRole::Text;
        a.width = 1.25;
        s.lines.append(a);
    }
    if (!f.fitted.isEmpty() && f.fitted.size() == f.fitted_x.size()) {
        QcLine fit;
        fit.name = qc_tr("Fitted (in-sample)");
        fit.xs = f.fitted_x;
        fit.ys = f.fitted;
        fit.color = ui::ChartColorRole::Muted;
        fit.dashed = true;
        fit.width = 1.0;
        fit.visible = false;
        s.lines.append(fit);
    }
    const auto anchored_line = [&](const QVector<double>& ys) {
        QVector<double> xs;
        QVector<double> out;
        if (anchored) {
            xs.append(f.anchor_x);
            out.append(f.anchor_y);
        }
        xs += f.fc_x;
        out += ys;
        return qMakePair(xs, out);
    };
    for (int i = 0; i < f.bands.size(); ++i) {
        const QcBand& b = f.bands.at(i);
        const auto lo = anchored_line(b.lower);
        const auto hi = anchored_line(b.upper);
        QcLine l;
        l.name = b.lower_name.isEmpty() ? band_name(b.lower_pct) : b.lower_name;
        l.xs = lo.first;
        l.ys = lo.second;
        l.color = ui::ChartColorRole::Info;
        l.dashed = true;
        l.width = 1.0;
        QcLine u = l;
        u.name = b.upper_name.isEmpty() ? band_name(b.upper_pct) : b.upper_name;
        u.xs = hi.first;
        u.ys = hi.second;
        s.lines.append(l);
        s.lines.append(u);
    }
    if (!f.point.isEmpty()) {
        const auto pt = anchored_line(f.point);
        QcLine p;
        p.name = f.point_name.isEmpty() ? qc_tr("Forecast") : f.point_name;
        p.xs = pt.first;
        p.ys = pt.second;
        p.color = ui::ChartColorRole::Accent;
        p.width = 2.0;
        s.lines.append(p);
    }
    auto* chart = qc_series_chart(s, parent);
    // Shade the forecast span only when there is a history to tell it apart from.
    if (!f.history.isEmpty() && !f.fc_x.isEmpty()) {
        const double from = anchored ? f.anchor_x : f.fc_x.first();
        if (f.fc_x.last() > from)
            chart->add_band(from, f.fc_x.last(), ui::ChartColorRole::Muted, qc_tr("Forecast"));
    }
    if (f.history.isEmpty() && anchored)
        chart->add_hline(f.anchor_y, anchor_name, ui::ChartColorRole::Muted);
    return chart;
}

// ── Equity curve ────────────────────────────────────────────────────────────

/// Equity (or growth-of-1) curve, an optional dashed benchmark on the same
/// axis, and below it the drawdown of the plotted curve in percent.
inline ui::TimeSeriesChart* qc_equity_chart(const QString& title, const XAxis& x, const QVector<double>& equity,
                                            const QVector<double>& benchmark, const ValueFmt& fmt,
                                            const QString& benchmark_name, QWidget* parent) {
    QcChartSpec s;
    s.title = title;
    s.x_time = x.time;
    s.xs = x.xs;
    s.fmt = fmt;
    s.height = 260;
    QcLine port;
    port.name = qc_tr("Portfolio");
    port.ys = equity;
    port.color = ui::ChartColorRole::Accent;
    port.width = 2.0;
    s.lines.append(port);
    if (!benchmark.isEmpty()) {
        QcLine b;
        b.name = benchmark_name.isEmpty() ? qc_tr("Benchmark") : benchmark_name;
        b.ys = benchmark;
        b.color = ui::ChartColorRole::Muted;
        b.dashed = true;
        s.lines.append(b);
    }
    QVector<double> dd(equity.size(), kQcNaN);
    double peak = kQcNaN;
    for (int i = 0; i < equity.size(); ++i) {
        const double v = equity.at(i);
        if (!std::isfinite(v))
            continue;
        if (!std::isfinite(peak) || v > peak)
            peak = v;
        if (peak > 0.0)
            dd[i] = (v / peak - 1.0) * 100.0;
    }
    QcLine ddl;
    ddl.name = qc_tr("Drawdown");
    ddl.ys = dd;
    ddl.pane = 1;
    ddl.style = ui::TimeSeriesChart::Style::Area;
    ddl.color = ui::ChartColorRole::Negative;
    ddl.has_baseline = true;
    ddl.baseline = 0.0;
    s.lines.append(ddl);
    s.pane_titles.append(qc_tr("DRAWDOWN"));
    s.pane_fmts.append(qc_fmt_pct(1));
    return qc_series_chart(s, parent);
}

// ── Risk / return frontier (QtCharts, themed via ChartFactory) ──────────────

/// Frontier line through (volatility %, return %) in the given order, the
/// max-Sharpe point and the min-variance point highlighted. Indexes < 0 skip
/// the highlight. Empty axis titles = "Volatility (%)" / "Return (%)".
inline QChartView* qc_frontier_chart(const QVector<double>& vol_pct, const QVector<double>& ret_pct, int max_sharpe_idx,
                                     int min_var_idx, QWidget* parent, const QString& x_title = {},
                                     const QString& y_title = {}) {
    auto* line = new QLineSeries;
    line->setName(qc_tr("Efficient frontier"));
    QPen pen{QColor(ui::colors::INFO())};
    pen.setWidthF(2.0);
    line->setPen(pen);
    double min_x = std::numeric_limits<double>::max(), max_x = std::numeric_limits<double>::lowest();
    double min_y = min_x, max_y = max_x;
    const qsizetype n = std::min(vol_pct.size(), ret_pct.size());
    for (qsizetype i = 0; i < n; ++i) {
        const double x = vol_pct.at(i);
        const double y = ret_pct.at(i);
        if (!std::isfinite(x) || !std::isfinite(y))
            continue;
        line->append(x, y);
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
    auto* chart = new QChart;
    chart->addSeries(line);
    const auto add_point = [&](int idx, const QString& name, const char* token) -> QScatterSeries* {
        if (idx < 0 || idx >= n || !std::isfinite(vol_pct.at(idx)) || !std::isfinite(ret_pct.at(idx)))
            return nullptr;
        auto* pt = new QScatterSeries;
        pt->setName(name);
        pt->setColor(QColor(token));
        pt->setBorderColor(QColor(token));
        pt->setMarkerSize(11);
        pt->append(vol_pct.at(idx), ret_pct.at(idx));
        chart->addSeries(pt);
        return pt;
    };
    QScatterSeries* best = add_point(max_sharpe_idx, qc_tr("Max Sharpe"), ui::colors::POSITIVE());
    QScatterSeries* minv = add_point(min_var_idx, qc_tr("Min variance"), ui::colors::WARNING());

    if (min_x > max_x) {
        min_x = 0;
        max_x = 1;
        min_y = 0;
        max_y = 1;
    }
    const double pad_x = std::max((max_x - min_x) * 0.08, 0.1);
    const double pad_y = std::max((max_y - min_y) * 0.08, 0.1);
    auto* x_axis = new QValueAxis;
    x_axis->setTitleText(x_title.isEmpty() ? qc_tr("Volatility (%)") : x_title);
    x_axis->setRange(std::max(0.0, min_x - pad_x), max_x + pad_x);
    x_axis->setTickCount(6);
    x_axis->setLabelFormat("%.1f%%");
    auto* y_axis = new QValueAxis;
    y_axis->setTitleText(y_title.isEmpty() ? qc_tr("Return (%)") : y_title);
    y_axis->setRange(min_y - pad_y, max_y + pad_y);
    y_axis->setTickCount(6);
    y_axis->setLabelFormat("%.1f%%");
    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    for (QAbstractSeries* sr : chart->series()) {
        sr->attachAxis(x_axis);
        sr->attachAxis(y_axis);
    }
    chart->setAnimationOptions(QChart::NoAnimation);
    ui::ChartFactory::apply_theme(chart);
    // apply_theme hides the legend; this chart needs it to name the two points.
    chart->legend()->setVisible(best != nullptr || minv != nullptr);
    chart->legend()->setAlignment(Qt::AlignTop);
    for (QValueAxis* ax : {x_axis, y_axis}) {
        ax->setTitleBrush(QColor(ui::colors::TEXT_TERTIARY()));
        ax->setTitleFont(ui::fonts::font(-3));
    }
    auto* view = new QChartView(chart, parent);
    view->setRenderHint(QPainter::Antialiasing);
    view->setFixedHeight(quant_styles::row_px(280));
    return view;
}

// ── Weights donut ───────────────────────────────────────────────────────────

/// Long-only weights (fractions) as a donut; nullptr when there are no
/// assets, a short (negative) weight, or more than `max_assets` assets — a
/// donut cannot show those honestly and the table's in-cell bars do.
inline ui::DonutChart* qc_weights_donut(const QStringList& labels, const QVector<double>& weights, QWidget* parent,
                                        int max_assets = 12) {
    const qsizetype n = std::min(labels.size(), weights.size());
    if (n == 0 || n > max_assets)
        return nullptr;
    QVector<ui::DonutSlice> slices;
    for (qsizetype i = 0; i < n; ++i) {
        const double w = weights.at(i);
        if (!std::isfinite(w) || w < -1e-9)
            return nullptr;
        slices.append({labels.at(i), labels.at(i), w * 100.0, QColor()});
    }
    auto* donut = new ui::DonutChart(parent);
    donut->set_slices(slices);
    donut->set_max_slices(0);
    donut->set_center_text(qc_tr("ASSETS"), QString::number(n));
    donut->set_value_formatter(qc_fmt_pct(2));
    donut->setMinimumHeight(quant_styles::row_px(200));
    return donut;
}

/// `table` with the donut (when there is one) to its right.
inline QWidget* qc_side_by_side(QWidget* table, QWidget* chart, QWidget* parent) {
    if (!chart)
        return table;
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(table, 3);
    hl->addWidget(chart, 2);
    return row;
}

} // namespace fincept::screens::quant_charts
