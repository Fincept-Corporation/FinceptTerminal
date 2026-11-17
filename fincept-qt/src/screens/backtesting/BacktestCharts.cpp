// src/screens/backtesting/BacktestCharts.cpp
//
// The CHART tab of every Backtesting command, the equity chart shared with the
// tearsheet, and the trade price chart of the DETAILS tab. See BacktestViews.h.

#include "screens/backtesting/BacktestViews.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/CandleChart.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/FanChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMap>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::screens::btview {

namespace {

namespace fmt = fincept::ui::formatting;
using TS = ui::TimeSeriesChart;

/// Translation context "BacktestCharts".
class BtChartsText {
    Q_DECLARE_TR_FUNCTIONS(BacktestCharts)
};
using CText = BtChartsText;

/// Most trades marked on a chart; beyond this the markers hide the curve.
constexpr int kMaxTradeMarkers = 500;

/// Money axis tick. `compact` is decided once per axis (ch_compact_axis), never per
/// tick: deciding per tick put "$98,000" and "$100.0K" on the same axis.
QString ch_money_tick(double v, const FmtCtx& ctx, bool compact) {
    if (ctx.ccy.isEmpty())
        return fmt::compact(v, compact ? fmt::Compact::FromSettings : fmt::Compact::None);
    return fmt::money(v, ctx.ccy, compact ? fmt::Compact::FromSettings : fmt::Compact::None, compact ? -1 : 0);
}

/// True when an axis over these values should abbreviate (largest magnitude >= 100k).
bool ch_compact_axis(std::initializer_list<const QVector<double>*> series) {
    double hi = 0.0;
    for (const QVector<double>* s : series)
        if (s)
            for (double v : *s)
                if (std::isfinite(v))
                    hi = std::max(hi, std::abs(v));
    return hi >= 1e5;
}

QString ch_param_label(double v) {
    return fmt::number(v, v == std::floor(v) ? 0 : (std::abs(v) < 1 ? 4 : 2));
}

QWidget* ch_wrap(QWidget* parent, const QString& title, QWidget* body) {
    auto* box = new QWidget(parent);
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    auto* t = new QLabel(title, box);
    t->setObjectName(QStringLiteral("btResultSection"));
    vl->addWidget(t);
    body->setParent(box);
    vl->addWidget(body, 1);
    return box;
}

QLabel* ch_caption(QWidget* parent, const QString& text) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("btChartCaption"));
    l->setWordWrap(true);
    return l;
}

// ── Trade markers ───────────────────────────────────────────────────────────

QString ch_trade_tooltip(const QJsonObject& t, bool entry, const FmtCtx& ctx) {
    const bool is_short = t.value(QStringLiteral("side")).toString() == QLatin1String("short");
    const fmt::InstrumentFmt inst{ctx.ccy, -1, 0, false};
    QStringList lines;
    const QString sym = t.value(QStringLiteral("symbol")).toString();
    if (entry) {
        lines << (is_short ? CText::tr("SHORT %1").arg(sym) : CText::tr("BUY %1").arg(sym));
        lines << CText::tr("%1 @ %2")
                     .arg(fmt::quantity(t.value(QStringLiteral("quantity")).toDouble()),
                          fmt::price(t.value(QStringLiteral("entryPrice")).toDouble(kNaN), inst));
    } else {
        lines << (is_short ? CText::tr("COVER %1").arg(sym) : CText::tr("SELL %1").arg(sym));
        lines << CText::tr("@ %1").arg(fmt::price(t.value(QStringLiteral("exitPrice")).toDouble(kNaN), inst));
        const QJsonValue pnl = t.value(QStringLiteral("pnl"));
        if (pnl.isDouble())
            lines << CText::tr("P&L %1 (%2)")
                         .arg(format_value(pnl.toDouble(), Unit::SignedMoney, ctx),
                              format_json(t.value(QStringLiteral("pnlPercent")), Unit::SignedFraction, ctx));
        const QString reason = t.value(QStringLiteral("exitReason")).toString();
        if (!reason.isEmpty())
            lines << CText::tr("Exit: %1").arg(reason);
    }
    return lines.join(QLatin1Char('\n'));
}

// ── Optimisation view ───────────────────────────────────────────────────────

struct OptRow {
    QMap<QString, double> params;
    double objective = kNaN;
    QJsonObject perf;
};

struct OptMetric {
    QString key; ///< "objectiveValue" or a performance key
    QString label;
    bool fraction = false;
    bool lower_better = false;
};

class OptimizationView : public QWidget {
  public:
    OptimizationView(QWidget* parent, const QJsonObject& payload) : QWidget(parent) {
        for (const QJsonValue& v : payload.value(QStringLiteral("allResults")).toArray()) {
            const QJsonObject o = v.toObject();
            OptRow r;
            const QJsonObject params = o.value(QStringLiteral("parameters")).toObject();
            for (auto it = params.begin(); it != params.end(); ++it)
                if (it.value().isDouble())
                    r.params.insert(it.key(), it.value().toDouble());
            r.objective = o.value(QStringLiteral("objectiveValue")).toDouble(o.value(QStringLiteral("objective")).toDouble(kNaN));
            r.perf = o.value(QStringLiteral("performance")).toObject();
            if (!r.params.isEmpty() && std::isfinite(r.objective))
                rows_.append(r);
        }
        for (const QJsonValue& v : payload.value(QStringLiteral("paramNames")).toArray())
            if (!v.toString().isEmpty())
                names_ << v.toString();
        if (names_.isEmpty() && !rows_.isEmpty())
            names_ = rows_.first().params.keys();
        objective_name_ = payload.value(QStringLiteral("objective")).toString();
        sensitivity_ = payload.value(QStringLiteral("parameterSensitivity")).toObject();
        spread_ = payload.value(QStringLiteral("parameterSpread")).toObject();
        build();
    }

    bool empty() const { return rows_.isEmpty(); }

  private:
    void build() {
        metrics_.append({QStringLiteral("objectiveValue"),
                         CText::tr("Objective (%1)").arg(objective_name_.isEmpty() ? CText::tr("score") : objective_name_),
                         objective_name_ == QLatin1String("return"), false});
        const QJsonObject p0 = rows_.isEmpty() ? QJsonObject() : rows_.first().perf;
        const QVector<OptMetric> known = {
            {QStringLiteral("totalReturn"), CText::tr("Total return"), true, false},
            {QStringLiteral("sharpeRatio"), CText::tr("Sharpe"), false, false},
            {QStringLiteral("sortinoRatio"), CText::tr("Sortino"), false, false},
            {QStringLiteral("calmarRatio"), CText::tr("Calmar"), false, false},
            {QStringLiteral("maxDrawdown"), CText::tr("Max drawdown"), true, true},
            {QStringLiteral("winRate"), CText::tr("Win rate"), true, false},
            {QStringLiteral("totalTrades"), CText::tr("Trades"), false, false},
        };
        for (const OptMetric& m : known)
            if (p0.value(m.key).isDouble())
                metrics_.append(m);

        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(6);

        auto* bar = new QHBoxLayout;
        bar->setSpacing(8);
        auto add_combo = [&](const QString& label, QComboBox*& combo) {
            auto* l = new QLabel(label, this);
            l->setObjectName(QStringLiteral("btFieldLabel"));
            combo = new QComboBox(this);
            combo->setObjectName(QStringLiteral("btChartCombo"));
            bar->addWidget(l);
            bar->addWidget(combo);
        };
        add_combo(CText::tr("METRIC"), metric_combo_);
        for (const OptMetric& m : metrics_)
            metric_combo_->addItem(m.label, m.key);
        if (names_.size() >= 2) {
            add_combo(CText::tr("X"), x_combo_);
            add_combo(CText::tr("Y"), y_combo_);
            x_combo_->addItems(names_);
            y_combo_->addItems(names_);
            x_combo_->setCurrentIndex(0);
            y_combo_->setCurrentIndex(1);
        }
        bar->addStretch(1);
        vl->addLayout(bar);

        heat_ = new ui::MatrixHeatmap(this);
        bars_ = new ui::BarChart(this);
        heat_->setMinimumHeight(260);
        bars_->setMinimumHeight(260);
        caption_ = ch_caption(this, {});
        vl->addWidget(heat_, 3);
        vl->addWidget(bars_, 3);
        vl->addWidget(caption_);

        // Parameter sensitivity: correlation of each parameter with the objective.
        if (!sensitivity_.isEmpty()) {
            QStringList cats;
            QVector<double> vals;
            QStringList spread_txt;
            for (auto it = sensitivity_.begin(); it != sensitivity_.end(); ++it) {
                cats << it.key();
                vals << it.value().toDouble();
                if (spread_.value(it.key()).isDouble())
                    spread_txt << CText::tr("%1: %2").arg(it.key(), fmt::number(spread_.value(it.key()).toDouble(), 3));
            }
            auto* sens = new ui::BarChart(this);
            sens->set_data(cats, vals, CText::tr("Correlation with objective"));
            sens->set_orientation(Qt::Horizontal);
            sens->set_signed_colors(true);
            sens->set_show_values(true);
            sens->set_value_formatter([](double v) { return fmt::number(v, 2); });
            sens->setMinimumHeight(std::max(110, 36 + 26 * static_cast<int>(cats.size())));
            vl->addWidget(ch_wrap(this, CText::tr("PARAMETER SENSITIVITY (CORRELATION WITH THE OBJECTIVE)"), sens), 1);
            if (!spread_txt.isEmpty())
                vl->addWidget(ch_caption(this, CText::tr("Spread of the average objective across each parameter's "
                                                          "values (captures non-linear effects): %1")
                                                   .arg(spread_txt.join(QStringLiteral(" · ")))));
        }

        auto refresh = [this]() { render(); };
        QObject::connect(metric_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
        if (x_combo_) {
            QObject::connect(x_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
            QObject::connect(y_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
        }
        render();
    }

    double metric_of(const OptRow& r, const OptMetric& m) const {
        if (m.key == QLatin1String("objectiveValue"))
            return r.objective;
        return r.perf.value(m.key).toDouble(kNaN);
    }

    std::function<QString(double)> formatter(const OptMetric& m) const {
        if (m.fraction)
            return [](double v) { return fmt::fraction_pct(v, 1, true); };
        if (m.key == QLatin1String("totalTrades"))
            return [](double v) { return fmt::number(v, 0); };
        return [](double v) { return fmt::number(v, 2); };
    }

    void render() {
        const int mi = std::clamp(metric_combo_->currentIndex(), 0, static_cast<int>(metrics_.size()) - 1);
        const OptMetric m = metrics_.at(mi);
        const auto f = formatter(m);
        const bool two_d = x_combo_ && y_combo_ && x_combo_->currentText() != y_combo_->currentText();
        heat_->setVisible(two_d);
        bars_->setVisible(!two_d);
        if (two_d) {
            const QString xn = x_combo_->currentText();
            const QString yn = y_combo_->currentText();
            QVector<double> xs, ys;
            for (const OptRow& r : rows_) {
                if (r.params.contains(xn) && !xs.contains(r.params.value(xn)))
                    xs << r.params.value(xn);
                if (r.params.contains(yn) && !ys.contains(r.params.value(yn)))
                    ys << r.params.value(yn);
            }
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());
            // Cell = the metric of the combination with the best OBJECTIVE among those
            // sharing this (x, y) - the other parameters at their best values.
            QVector<QVector<double>> vals(ys.size(), QVector<double>(xs.size(), kNaN));
            QVector<QVector<double>> best(ys.size(), QVector<double>(xs.size(), -1e300));
            for (const OptRow& r : rows_) {
                if (!r.params.contains(xn) || !r.params.contains(yn))
                    continue;
                const int c = static_cast<int>(xs.indexOf(r.params.value(xn)));
                const int rr = static_cast<int>(ys.indexOf(r.params.value(yn)));
                if (c < 0 || rr < 0 || r.objective <= best[rr][c])
                    continue;
                best[rr][c] = r.objective;
                vals[rr][c] = metric_of(r, m);
            }
            QStringList xl, yl;
            for (double x : xs)
                xl << ch_param_label(x);
            for (double y : ys)
                yl << ch_param_label(y);
            heat_->set_matrix(yl, xl, vals);
            const bool magnitudes = m.key == QLatin1String("maxDrawdown") || m.key == QLatin1String("totalTrades") ||
                                    m.key == QLatin1String("winRate");
            if (magnitudes)
                heat_->set_sequential_scale();
            else
                heat_->set_diverging_scale();
            heat_->set_value_formatter(f);
            heat_->set_show_values(xs.size() <= 25 && ys.size() <= 25);
            const int filled = static_cast<int>(std::count_if(rows_.cbegin(), rows_.cend(), [&](const OptRow& r) {
                return r.params.contains(xn) && r.params.contains(yn);
            }));
            caption_->setText(CText::tr("%1 (columns) × %2 (rows) · %3 combinations · %4")
                                  .arg(xn, yn)
                                  .arg(filled)
                                  .arg(names_.size() > 2 ? CText::tr("each cell shows the best combination of the "
                                                                     "other parameters")
                                                         : CText::tr("blank cells were not tested")));
        } else {
            // One parameter (or X == Y): the metric along it.
            const QString pn = x_combo_ ? x_combo_->currentText() : (names_.isEmpty() ? QString() : names_.first());
            QMap<double, double> by_val;
            QMap<double, double> best;
            for (const OptRow& r : rows_) {
                if (!r.params.contains(pn))
                    continue;
                const double x = r.params.value(pn);
                if (!best.contains(x) || r.objective > best.value(x)) {
                    best[x] = r.objective;
                    by_val[x] = metric_of(r, m);
                }
            }
            QStringList cats;
            QVector<double> vals;
            for (auto it = by_val.cbegin(); it != by_val.cend(); ++it) {
                cats << ch_param_label(it.key());
                vals << it.value();
            }
            bars_->set_data(cats, vals, m.label);
            bars_->set_signed_colors(!m.lower_better && m.key != QLatin1String("totalTrades") &&
                                     m.key != QLatin1String("winRate"));
            bars_->set_value_formatter(f);
            caption_->setText(CText::tr("%1 by %2 · %3 values").arg(m.label, pn).arg(cats.size()));
        }
    }

    QVector<OptRow> rows_;
    QStringList names_;
    QString objective_name_;
    QJsonObject sensitivity_;
    QJsonObject spread_;
    QVector<OptMetric> metrics_;
    QComboBox* metric_combo_ = nullptr;
    QComboBox* x_combo_ = nullptr;
    QComboBox* y_combo_ = nullptr;
    ui::MatrixHeatmap* heat_ = nullptr;
    ui::BarChart* bars_ = nullptr;
    QLabel* caption_ = nullptr;
};

// ── Other commands ──────────────────────────────────────────────────────────

QWidget* ch_walk_forward(QWidget* parent, const QJsonObject& p, const FmtCtx& ctx) {
    const QJsonArray windows = p.value(QStringLiteral("windows")).toArray();
    if (windows.isEmpty())
        return nullptr;
    auto* host = new QWidget(parent);
    auto* vl = new QVBoxLayout(host);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);

    QStringList cats;
    ui::BarSeries is{CText::tr("In-sample"), {}, {}};
    ui::BarSeries oos{CText::tr("Out-of-sample"), {}, {}};
    bool any_is = false;
    for (const QJsonValue& v : windows) {
        const QJsonObject w = v.toObject();
        cats << CText::tr("W%1").arg(w.value(QStringLiteral("window")).toInt());
        const QJsonValue a = w.value(QStringLiteral("inSampleReturn"));
        any_is = any_is || a.isDouble();
        is.values << (a.isDouble() ? a.toDouble() * 100.0 : kNaN);
        oos.values << w.value(QStringLiteral("outOfSampleReturn")).toDouble(kNaN) * 100.0;
    }
    auto* bars = new ui::BarChart(host);
    bars->set_categories(cats);
    if (any_is)
        bars->set_series({is, oos});
    else
        bars->set_series({oos});
    bars->set_mode(ui::BarChart::Mode::Grouped);
    bars->set_show_values(windows.size() <= 12);
    bars->set_value_formatter([](double v) { return fmt::pct(v, 1, true); });
    bars->setMinimumHeight(240);
    vl->addWidget(ch_wrap(host, CText::tr("RETURN PER WINDOW: IN-SAMPLE (OPTIMISED) VS OUT-OF-SAMPLE"), bars), 1);

    const QVector<QPointF> oos_eq = dated_points(p.value(QStringLiteral("oosEquity")).toArray(),
                                                 QStringLiteral("equity"));
    if (oos_eq.size() >= 2) {
        auto* c = new TS(host);
        c->set_utc(true);
        c->set_title(CText::tr("STITCHED OUT-OF-SAMPLE EQUITY"));
        c->set_export_name(QStringLiteral("walk_forward_oos_equity"));
        TS::SeriesSpec s;
        s.name = CText::tr("Out-of-sample equity");
        s.points = oos_eq;
        s.style = TS::Style::Area;
        s.color = ui::ChartColorRole::Accent;
        s.formatter = [ctx](double v) { return format_value(v, Unit::Money, ctx); };
        c->add_series(s);
        QVector<double> ys;
        ys.reserve(oos_eq.size());
        for (const QPointF& q : oos_eq)
            ys << q.y();
        const bool compact = ch_compact_axis({&ys});
        c->set_value_formatter(TS::Axis::Left, [ctx, compact](double v) { return ch_money_tick(v, ctx, compact); }, 0);
        for (const QJsonValue& v : windows) {
            const QJsonObject w = v.toObject();
            const double a = parse_time_ms(w.value(QStringLiteral("testStart")).toString());
            const double b = parse_time_ms(w.value(QStringLiteral("testEnd")).toString());
            if (std::isfinite(a) && std::isfinite(b) && w.value(QStringLiteral("window")).toInt() % 2 == 0)
                c->add_band(a, b, ui::ChartColorRole::Muted);
        }
        c->setMinimumHeight(240);
        vl->addWidget(c, 1);
        vl->addWidget(ch_caption(host, CText::tr("Each out-of-sample window traded with the parameters optimised on "
                                                  "the window before it, chained end to end. Shaded: alternate "
                                                  "windows. Best parameters per window are in DETAILS.")));
    }
    return host;
}

QWidget* ch_indicator(QWidget* parent, const QJsonObject& p) {
    const QJsonValue sv = p.value(QStringLiteral("series"));
    QVector<QPair<QString, QVector<QPointF>>> comps;
    if (sv.isArray()) {
        comps.append({p.value(QStringLiteral("indicator")).toString().toUpper(), dated_points(sv.toArray())});
    } else if (sv.isObject()) {
        const QJsonObject so = sv.toObject();
        for (auto it = so.begin(); it != so.end(); ++it)
            if (it.value().isArray())
                comps.append({it.key(), dated_points(it.value().toArray())});
    }
    comps.erase(std::remove_if(comps.begin(), comps.end(), [](const auto& c) { return c.second.size() < 2; }),
                comps.end());
    if (comps.isEmpty())
        return nullptr;
    // Components on very different scales (Bollinger bands vs %B) get their own pane.
    auto typical = [](const QVector<QPointF>& pts) {
        QVector<double> a;
        a.reserve(pts.size());
        for (const QPointF& q : pts)
            a << std::abs(q.y());
        std::nth_element(a.begin(), a.begin() + a.size() / 2, a.end());
        return a.at(a.size() / 2);
    };
    const double ref = typical(comps.first().second);
    auto* c = new TS(parent);
    c->set_utc(true);
    c->set_title(CText::tr("%1 (%2) · %3")
                     .arg(p.value(QStringLiteral("indicator")).toString().toUpper(),
                          fmt::number(p.value(QStringLiteral("period")).toDouble(), 0),
                          p.value(QStringLiteral("symbol")).toString()));
    c->set_export_name(QStringLiteral("indicator_") + p.value(QStringLiteral("indicator")).toString());
    QMap<int, int> pane_for_bucket;
    int palette = 0;
    for (const auto& comp : comps) {
        const double t = typical(comp.second);
        const bool same = ref <= 0 || t <= 0 ? t == ref : (t / ref < 5.0 && ref / t < 5.0);
        int pane = 0;
        if (!same) {
            const int bucket = static_cast<int>(std::round(std::log10(std::max(t, 1e-12))));
            if (!pane_for_bucket.contains(bucket) && c->pane_count() < 3)
                pane_for_bucket[bucket] = c->add_pane(0.45);
            pane = pane_for_bucket.value(bucket, c->pane_count() - 1);
        }
        TS::SeriesSpec s;
        s.name = comp.first;
        s.points = comp.second;
        s.pane = pane;
        s.color = ui::ChartColorRole::Palette;
        s.palette_index = palette++;
        s.rebase = false;
        c->add_series(s);
    }
    const QString id = p.value(QStringLiteral("indicator")).toString().toLower();
    if (id.contains(QLatin1String("rsi")) || id.contains(QLatin1String("stoch")) ||
        id.contains(QLatin1String("mfi"))) {
        c->add_hline(70.0, QStringLiteral("70"));
        c->add_hline(30.0, QStringLiteral("30"));
    }
    return c;
}

/// Entries / exits as a 0/1 "in position" step line with markers (signal commands
/// carry dates, not prices).
QWidget* ch_signals(QWidget* parent, const QJsonObject& p, const QString& title) {
    struct Ev {
        double t;
        bool entry;
        QString tip;
    };
    QVector<Ev> ev;
    auto collect = [&](const QJsonArray& arr, bool entry) {
        for (const QJsonValue& v : arr) {
            QString date;
            QString tip;
            if (v.isString()) {
                date = v.toString();
            } else {
                const QJsonObject o = v.toObject();
                date = o.value(QStringLiteral("date")).toString();
                if (o.value(QStringLiteral("price")).isDouble())
                    tip = CText::tr("Price %1").arg(fmt::price(o.value(QStringLiteral("price")).toDouble()));
                if (o.value(QStringLiteral("indicatorValue")).isDouble())
                    tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) +
                           CText::tr("Indicator %1").arg(fmt::number(o.value(QStringLiteral("indicatorValue")).toDouble(), 2));
            }
            const double t = parse_time_ms(date);
            if (std::isfinite(t))
                ev.append({t, entry, tip});
        }
    };
    collect(p.value(QStringLiteral("entrySignals")).toArray(), true);
    collect(p.value(QStringLiteral("exitSignals")).toArray(), false);
    if (ev.isEmpty()) {
        collect(p.value(QStringLiteral("entries")).toArray(), true);
        collect(p.value(QStringLiteral("exits")).toArray(), false);
    }
    if (ev.isEmpty())
        return nullptr;
    std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t || (a.t == b.t && !a.entry); });
    QVector<QPointF> pos;
    double state = 0.0;
    pos.append(QPointF(ev.first().t - 86400000.0, 0.0));
    for (const Ev& e : ev) {
        state = e.entry ? 1.0 : 0.0;
        pos.append(QPointF(e.t, state));
    }
    auto* c = new TS(parent);
    c->set_utc(true);
    c->set_title(title);
    c->set_export_name(QStringLiteral("signals"));
    TS::SeriesSpec s;
    s.name = CText::tr("In position");
    s.points = pos;
    s.style = TS::Style::Step;
    s.color = ui::ChartColorRole::Accent;
    s.rebase = false;
    const int sid = c->add_series(s);
    c->set_y_range(0, TS::Axis::Left, -0.1, 1.3);
    c->set_value_formatter(TS::Axis::Left, [](double v) {
        return v >= 0.5 ? CText::tr("IN") : CText::tr("OUT");
    }, 0);
    int n = 0;
    for (const Ev& e : ev) {
        if (++n > kMaxTradeMarkers)
            break;
        TS::MarkerSpec m;
        m.x = e.t;
        m.series = sid;
        m.kind = e.entry ? ui::ChartMarkerKind::Buy : ui::ChartMarkerKind::Sell;
        m.tooltip = (e.entry ? CText::tr("Entry signal") : CText::tr("Exit signal")) +
                    (e.tip.isEmpty() ? QString() : QStringLiteral("\n") + e.tip);
        c->add_marker(m);
    }
    return c;
}

QWidget* ch_labels(QWidget* parent, const QJsonObject& p) {
    const QJsonObject dist = p.value(QStringLiteral("distribution")).toObject();
    if (dist.isEmpty())
        return nullptr;
    QStringList keys = dist.keys();
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return a.toInt() < b.toInt(); });
    QStringList cats;
    QVector<double> vals;
    for (const QString& k : keys) {
        cats << CText::tr("Class %1").arg(k);
        vals << dist.value(k).toDouble();
    }
    auto* bars = new ui::BarChart(parent);
    bars->set_data(cats, vals, CText::tr("Bars"));
    bars->set_show_values(true);
    bars->set_value_formatter([](double v) { return fmt::number(v, 0); });
    return ch_wrap(parent, CText::tr("CLASS DISTRIBUTION (%1)").arg(p.value(QStringLiteral("labelType")).toString()),
                   bars);
}

QWidget* ch_returns(QWidget* parent, const QJsonObject& p) {
    struct Src {
        const char* key;
        QString name;
        double scale;
        bool area;
        bool pct;
    };
    const QString metric = p.value(QStringLiteral("metric")).toString();
    const QVector<Src> srcs = {
        {"cumulativeReturns", CText::tr("Cumulative return"), 100.0, false, true},
        {"drawdownSeries", CText::tr("Drawdown"), 100.0, true, true},
        {"series", metric.isEmpty() ? CText::tr("Value") : CText::tr("Rolling %1").arg(metric), 1.0, false, false},
    };
    auto* c = new TS(parent);
    c->set_utc(true);
    c->set_export_name(QStringLiteral("returns_analysis"));
    int added = 0;
    for (const Src& s : srcs) {
        QVector<QPointF> pts = dated_points(p.value(QLatin1String(s.key)).toArray(), QStringLiteral("value"), s.scale);
        if (pts.size() < 2)
            continue;
        if (s.area) {
            // Drawdowns are reported as positive depths by some engines; draw them below zero.
            bool all_pos = std::all_of(pts.cbegin(), pts.cend(), [](const QPointF& q) { return q.y() >= 0; });
            if (all_pos)
                for (QPointF& q : pts)
                    q.setY(-q.y());
        }
        const int pane = added == 0 ? 0 : c->add_pane(0.6);
        TS::SeriesSpec spec;
        spec.name = s.name;
        spec.points = pts;
        spec.pane = pane;
        spec.style = s.area ? TS::Style::Area : TS::Style::Baseline;
        spec.baseline = 0.0;
        spec.color = s.area ? ui::ChartColorRole::Negative : ui::ChartColorRole::Auto;
        spec.rebase = false;
        c->add_series(spec);
        if (s.pct)
            c->set_value_formatter(TS::Axis::Left, [](double v) { return fmt::pct(v, 1, true); }, pane);
        else
            c->set_value_formatter(TS::Axis::Left, [](double v) { return fmt::number(v, 2); }, pane);
        if (pane > 0)
            c->set_pane_title(pane, s.name);
        ++added;
    }
    if (added == 0) {
        delete c;
        return nullptr;
    }
    c->set_title(CText::tr("RETURNS ANALYSIS · %1").arg(p.value(QStringLiteral("analysisType")).toString()));
    return c;
}

QWidget* ch_sweep(QWidget* parent, const QJsonObject& p) {
    const QJsonArray results = p.value(QStringLiteral("results")).toArray();
    if (results.isEmpty())
        return nullptr;
    QStringList cats;
    ui::BarSeries mean{CText::tr("Mean"), {}, {}};
    ui::BarSeries last{CText::tr("Last"), {}, {}};
    for (const QJsonValue& v : results) {
        const QJsonObject o = v.toObject();
        QStringList parts;
        const QJsonObject params = o.value(QStringLiteral("params")).toObject();
        for (auto it = params.begin(); it != params.end(); ++it)
            parts << (it.value().isDouble() ? ch_param_label(it.value().toDouble()) : it.value().toString());
        cats << parts.join(QStringLiteral(" / "));
        mean.values << o.value(QStringLiteral("mean")).toDouble(kNaN);
        last.values << o.value(QStringLiteral("last")).toDouble(kNaN);
    }
    auto* bars = new ui::BarChart(parent);
    bars->set_categories(cats);
    bars->set_series({mean, last});
    bars->set_value_formatter([](double v) { return fmt::number(v, 2); });
    return ch_wrap(parent,
                   CText::tr("%1 ACROSS PARAMETER VALUES").arg(p.value(QStringLiteral("indicator")).toString().toUpper()),
                   bars);
}

QWidget* ch_monte_carlo(QWidget* parent, const QJsonObject& p, const FmtCtx& ctx) {
    QVector<double> x;
    for (const QJsonValue& v : p.value(QStringLiteral("x")).toArray())
        x << v.toDouble();
    if (x.size() < 2)
        return nullptr;
    auto to_vec = [](const QJsonArray& a) {
        QVector<double> out;
        out.reserve(a.size());
        for (const QJsonValue& v : a)
            out << v.toDouble(kNaN);
        return out;
    };
    QVector<ui::FanBand> bands;
    for (const QJsonValue& v : p.value(QStringLiteral("bands")).toArray()) {
        const QJsonObject b = v.toObject();
        ui::FanBand fb;
        fb.lower_pct = b.value(QStringLiteral("lower")).toDouble();
        fb.upper_pct = b.value(QStringLiteral("upper")).toDouble();
        fb.lower = to_vec(b.value(QStringLiteral("lowerValues")).toArray());
        fb.upper = to_vec(b.value(QStringLiteral("upperValues")).toArray());
        if (fb.lower.size() == x.size() && fb.upper.size() == x.size())
            bands << fb;
    }
    const QVector<double> median = to_vec(p.value(QStringLiteral("median")).toArray());

    auto* host = new QWidget(parent);
    auto* vl = new QVBoxLayout(host);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);

    auto* fan = new ui::FanChart(host);
    fan->set_fan(x, bands, median.size() == x.size() ? median : QVector<double>{});
    const double init = p.value(QStringLiteral("initialCapital")).toDouble(kNaN);
    if (std::isfinite(init))
        fan->add_reference_line(init, CText::tr("Initial capital"));
    bool fan_compact = ch_compact_axis({&median});
    for (const ui::FanBand& b : bands)
        fan_compact = fan_compact || ch_compact_axis({&b.lower, &b.upper});
    fan->set_value_formatter([ctx, fan_compact](double v) { return ch_money_tick(v, ctx, fan_compact); });
    fan->set_x_formatter([](double v) { return CText::tr("Trade %1").arg(fmt::number(v, 0)); });
    fan->setMinimumHeight(280);
    vl->addWidget(ch_wrap(host, CText::tr("EQUITY AFTER EACH TRADE · 5–95% AND 25–75% OF PATHS, MEDIAN"), fan), 3);

    auto* row = new QWidget(host);
    auto* grid = new QHBoxLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(12);
    const QJsonObject st = p.value(QStringLiteral("stats")).toObject();
    auto* fin = new ui::DistributionChart(row);
    const QVector<double> finals = to_vec(p.value(QStringLiteral("finalEquity")).toArray());
    fin->set_samples(finals);
    const bool fin_compact = ch_compact_axis({&finals});
    fin->set_value_formatter([ctx, fin_compact](double v) { return ch_money_tick(v, ctx, fin_compact); });
    if (std::isfinite(init))
        fin->add_marker(init, CText::tr("Start"), ui::DistributionChart::MarkerRole::Neutral);
    fin->add_marker(st.value(QStringLiteral("finalP5")).toDouble(), CText::tr("P5"),
                    ui::DistributionChart::MarkerRole::VaR);
    fin->add_marker(st.value(QStringLiteral("finalP50")).toDouble(), CText::tr("Median"),
                    ui::DistributionChart::MarkerRole::Median);
    grid->addWidget(ch_wrap(row, CText::tr("FINAL EQUITY"), fin), 1);
    QVector<double> dd = to_vec(p.value(QStringLiteral("maxDrawdown")).toArray());
    for (double& d : dd)
        d *= -100.0;
    auto* ddc = new ui::DistributionChart(row);
    ddc->set_samples(dd);
    ddc->set_value_formatter([](double v) { return fmt::pct(v, 1, false); });
    ddc->add_marker(-st.value(QStringLiteral("ddP95")).toDouble() * 100.0, CText::tr("P95"),
                    ui::DistributionChart::MarkerRole::CVaR);
    ddc->add_marker(-st.value(QStringLiteral("originalMaxDrawdown")).toDouble() * 100.0, CText::tr("Actual"),
                    ui::DistributionChart::MarkerRole::Accent);
    grid->addWidget(ch_wrap(row, CText::tr("MAXIMUM DRAWDOWN"), ddc), 1);
    row->setMinimumHeight(240);
    vl->addWidget(row, 2);
    return host;
}

} // namespace

// ── Public ──────────────────────────────────────────────────────────────────

ui::TimeSeriesChart* make_equity_chart(QWidget* parent, const QJsonObject& payload, const FmtCtx& ctx,
                                       bool interactive) {
    const EquityData eq = parse_equity(payload.value(QStringLiteral("equity")).toArray());
    if (eq.empty())
        return nullptr;
    const QString bsym = payload.value(QStringLiteral("benchmark")).toObject().value(QStringLiteral("symbol")).toString();

    auto* c = new TS(parent);
    c->set_utc(true); // provider stamps are exchange wall time, read as UTC
    c->set_zoom_enabled(interactive);
    c->set_show_mode_buttons(true);
    c->set_export_name(QStringLiteral("equity_curve"));
    c->set_title(!eq.benchmark.isEmpty() && !bsym.isEmpty() ? CText::tr("EQUITY VS %1").arg(bsym.toUpper())
                                                            : CText::tr("EQUITY"));
    c->set_range_presets(eq.intraday ? QStringList{QStringLiteral("1D"), QStringLiteral("1W"), QStringLiteral("1M"),
                                                   QStringLiteral("3M"), QStringLiteral("ALL")}
                                     : QStringList{QStringLiteral("3M"), QStringLiteral("6M"), QStringLiteral("YTD"),
                                                   QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("5Y"),
                                                   QStringLiteral("ALL")});
    const auto money_full = [ctx](double v) { return format_value(v, Unit::Money, ctx); };

    TS::SeriesSpec es;
    es.name = CText::tr("Strategy");
    es.xs = eq.t;
    es.ys = eq.equity;
    es.style = TS::Style::Line;
    es.color = ui::ChartColorRole::Accent;
    es.line_width = 1.8;
    es.id = QStringLiteral("equity");
    es.formatter = money_full;
    const int eq_id = c->add_series(es);

    if (!eq.benchmark.isEmpty()) {
        TS::SeriesSpec bs;
        bs.name = bsym.isEmpty() ? CText::tr("Benchmark") : bsym.toUpper();
        bs.xs = eq.t;
        bs.ys = eq.benchmark;
        bs.style = TS::Style::Line;
        bs.color = ui::ChartColorRole::Muted;
        bs.dashed = true;
        bs.line_width = 1.2;
        bs.id = QStringLiteral("benchmark");
        bs.formatter = money_full;
        c->add_series(bs);
    }
    const bool compact = ch_compact_axis({&eq.equity, &eq.benchmark});
    c->set_value_formatter(TS::Axis::Left, [ctx, compact](double v) { return ch_money_tick(v, ctx, compact); }, 0);
    if (std::isfinite(eq.equity.first()))
        c->add_hline(eq.equity.first(), CText::tr("Start"), ui::ChartColorRole::Muted);

    const int dd_pane = c->add_pane(0.32);
    c->set_pane_title(dd_pane, CText::tr("Drawdown"));
    TS::SeriesSpec ds;
    ds.name = CText::tr("Drawdown");
    ds.xs = eq.t;
    ds.ys.reserve(eq.drawdown.size());
    for (double d : eq.drawdown)
        ds.ys << d * 100.0;
    ds.style = TS::Style::Area;
    ds.pane = dd_pane;
    ds.color = ui::ChartColorRole::Negative;
    ds.baseline = 0.0;
    ds.rebase = false;
    ds.formatter = [](double v) { return fmt::pct(v, 2, false); };
    c->add_series(ds);
    c->set_value_formatter(TS::Axis::Left, [](double v) { return fmt::pct(v, 0, false); }, dd_pane);

    // Trade entries / exits on the equity line.
    const QJsonArray trades = payload.value(QStringLiteral("trades")).toArray();
    int n = 0;
    for (const QJsonValue& v : trades) {
        if (n >= kMaxTradeMarkers)
            break;
        const QJsonObject t = v.toObject();
        const bool is_short = t.value(QStringLiteral("side")).toString() == QLatin1String("short");
        const double te = parse_time_ms(t.value(QStringLiteral("entryDate")).toString());
        if (std::isfinite(te)) {
            c->add_marker(te, eq_id, is_short ? ui::ChartMarkerKind::Sell : ui::ChartMarkerKind::Buy, {},
                          ch_trade_tooltip(t, true, ctx));
            ++n;
        }
        const double tx = parse_time_ms(t.value(QStringLiteral("exitDate")).toString());
        if (std::isfinite(tx)) {
            c->add_marker(tx, eq_id, is_short ? ui::ChartMarkerKind::Buy : ui::ChartMarkerKind::Sell, {},
                          ch_trade_tooltip(t, false, ctx));
            ++n;
        }
    }
    return c;
}

QWidget* make_command_chart(QWidget* parent, const QString& command, const QJsonObject& payload, const FmtCtx& ctx) {
    if (command == QLatin1String("backtest"))
        return make_equity_chart(parent, payload, ctx, /*interactive=*/true);
    if (command == QLatin1String("optimize")) {
        auto* v = new OptimizationView(parent, payload);
        if (v->empty()) {
            delete v;
            return nullptr;
        }
        return v;
    }
    if (command == QLatin1String("walk_forward"))
        return ch_walk_forward(parent, payload, ctx);
    if (command == QLatin1String("indicator"))
        return ch_indicator(parent, payload);
    if (command == QLatin1String("indicator_signals"))
        return ch_signals(parent, payload,
                          CText::tr("%1 SIGNALS · %2")
                              .arg(payload.value(QStringLiteral("indicator")).toString().toUpper(),
                                   payload.value(QStringLiteral("mode")).toString()));
    if (command == QLatin1String("signals"))
        return ch_signals(parent, payload,
                          CText::tr("RANDOM SIGNALS · %1").arg(payload.value(QStringLiteral("generatorType")).toString()));
    if (command == QLatin1String("labels_to_signals"))
        return ch_signals(parent, payload,
                          CText::tr("SIGNALS FROM %1 LABELS").arg(payload.value(QStringLiteral("labelType")).toString()));
    if (command == QLatin1String("labels"))
        return ch_labels(parent, payload);
    if (command == QLatin1String("returns"))
        return ch_returns(parent, payload);
    if (command == QLatin1String("indicator_sweep"))
        return ch_sweep(parent, payload);
    if (command == QLatin1String("monte_carlo"))
        return ch_monte_carlo(parent, payload, ctx);
    return nullptr;
}

QStringList price_symbols(const QJsonObject& payload) {
    QStringList out;
    const QJsonObject pd = payload.value(QStringLiteral("priceData")).toObject();
    for (auto it = pd.begin(); it != pd.end(); ++it)
        if (it.value().toObject().value(QStringLiteral("t")).toArray().size() > 1)
            out << it.key();
    return out;
}

bool load_trade_chart(ui::CandleChart* chart, const QJsonObject& payload, const QString& symbol, const FmtCtx& ctx) {
    if (!chart)
        return false;
    chart->clear_annotations();
    const QJsonObject d = payload.value(QStringLiteral("priceData")).toObject().value(symbol).toObject();
    const QJsonArray t = d.value(QStringLiteral("t")).toArray();
    const QJsonArray o = d.value(QStringLiteral("o")).toArray();
    const QJsonArray h = d.value(QStringLiteral("h")).toArray();
    const QJsonArray l = d.value(QStringLiteral("l")).toArray();
    const QJsonArray c = d.value(QStringLiteral("c")).toArray();
    const QJsonArray v = d.value(QStringLiteral("v")).toArray();
    QVector<ui::CandleChart::Bar> bars;
    bars.reserve(t.size());
    for (int i = 0; i < t.size(); ++i) {
        ui::CandleChart::Bar b;
        b.time_ms = parse_time_ms(t.at(i).toString());
        b.close = c.at(i).toDouble(kNaN);
        b.open = o.at(i).toDouble(b.close);
        b.high = h.at(i).toDouble(std::max(b.open, b.close));
        b.low = l.at(i).toDouble(std::min(b.open, b.close));
        b.volume = v.at(i).toDouble(0.0);
        if (std::isfinite(b.time_ms) && std::isfinite(b.close))
            bars.append(b);
    }
    if (bars.size() < 2) {
        chart->clear();
        chart->set_state(ui::ChartState::Empty, CText::tr("No price data for %1").arg(symbol));
        return false;
    }
    chart->set_state(ui::ChartState::Ready);
    chart->set_utc(true);
    chart->set_title(symbol);
    chart->set_export_name(symbol + QStringLiteral("_trades"));
    const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, ctx.ccy);
    chart->set_price_formatter([inst](double x) { return fmt::price(x, inst); });
    chart->set_bars(bars);
    chart->set_default_visible_bars(std::min(300, static_cast<int>(bars.size())));
    int n = 0;
    for (const QJsonValue& tv : payload.value(QStringLiteral("trades")).toArray()) {
        const QJsonObject tr = tv.toObject();
        if (tr.value(QStringLiteral("symbol")).toString().compare(symbol, Qt::CaseInsensitive) != 0)
            continue;
        if (++n > kMaxTradeMarkers)
            break;
        const bool is_short = tr.value(QStringLiteral("side")).toString() == QLatin1String("short");
        const double te = parse_time_ms(tr.value(QStringLiteral("entryDate")).toString());
        const double pe = tr.value(QStringLiteral("entryPrice")).toDouble(kNaN);
        if (std::isfinite(te) && std::isfinite(pe))
            chart->add_trade_marker(te, pe, is_short ? ui::CandleChart::TradeSide::Sell : ui::CandleChart::TradeSide::Buy,
                                    is_short ? CText::tr("S") : CText::tr("B"), ch_trade_tooltip(tr, true, ctx));
        const double tx = parse_time_ms(tr.value(QStringLiteral("exitDate")).toString());
        const double px = tr.value(QStringLiteral("exitPrice")).toDouble(kNaN);
        if (std::isfinite(tx) && std::isfinite(px))
            chart->add_trade_marker(tx, px, is_short ? ui::CandleChart::TradeSide::Buy : ui::CandleChart::TradeSide::Sell,
                                    CText::tr("X"), ch_trade_tooltip(tr, false, ctx));
    }
    chart->reset_view();
    return true;
}

} // namespace fincept::screens::btview
