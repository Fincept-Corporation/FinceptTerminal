// src/screens/backtesting/BtMonteCarloView.cpp
//
// MONTE CARLO view (BtAnalysisViews.h): KPI row, the FAN (ui::FanChart: 5-95 and 25-75 bands,
// median, the original run's own path as a line over the fan, reference lines at the start capital and
// the original run's final equity) with a PATHS alternative (ui::TimeSeriesChart: the original run's
// path highlighted over the percentile lines and the sample paths), PERCENTILES table + simulation facts,
// and the
// FINAL EQUITY / MAX DRAWDOWN distributions.
//
// trade_shuffle permutes the same trades, so every path ends at the same final equity: the final
// percentiles are identical by construction and the view says so instead of drawing them.
//
// Thresholds (state colours): probability of loss > 25 % warn; probability of ruin > 1 % warn;
// drawdown tiles by BtTabKit's Rule::Drawdown; original run at or above the 90th percentile of the
// resamples warn ("the realised path was lucky").
#include "screens/backtesting/BtAnalysisViews.h"

#include "ui/charts/DistributionChart.h"
#include "ui/charts/FanChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::bt_time_ms;
using services::backtest::BtMonteCarloPtr;
using services::backtest::BtMonteCarloResult;

/// One row of PERCENTILES (P5 P25 P50 P75 P95).
struct BtMcPctRow {
    QString key;
    QString label;
    Unit unit = Unit::Money;
    QString ccy;
    QVector<double> values; ///< 5
    QStringList tips;       ///< 5 (why a value is computed here / blank)
};

namespace {

using McPctModel = ui::LiveTableModel<BtMcPctRow>;

constexpr double kMcQs[5] = {0.05, 0.25, 0.50, 0.75, 0.95};

QVector<double> mc_sorted(const QVector<double>& v) {
    QVector<double> out;
    out.reserve(v.size());
    for (double x : v)
        if (std::isfinite(x))
            out.append(x);
    std::sort(out.begin(), out.end());
    return out;
}

double mc_quantile(const QVector<double>& sorted, double q) {
    if (sorted.isEmpty())
        return kBtNaN;
    const double pos = std::clamp(q, 0.0, 1.0) * static_cast<double>(sorted.size() - 1);
    const int lo = static_cast<int>(std::floor(pos));
    const int hi = static_cast<int>(std::ceil(pos));
    return sorted.at(lo) + (sorted.at(hi) - sorted.at(lo)) * (pos - lo);
}

QString mc_method_text(const BtMonteCarloResult& r) {
    const QString& m = r.method_id;
    if (m == QLatin1String("trade_bootstrap"))
        return BtMonteCarloView::tr("Trade bootstrap (with replacement)");
    if (m == QLatin1String("trade_shuffle"))
        return BtMonteCarloView::tr("Trade shuffle (same trades, new order)");
    if (m == QLatin1String("returns_bootstrap"))
        return BtMonteCarloView::tr("Return bootstrap (per bar)");
    if (m == QLatin1String("block_bootstrap"))
        return std::isfinite(r.block_size)
                   ? BtMonteCarloView::tr("Block bootstrap (mean block %1)").arg(bars(r.block_size))
                   : BtMonteCarloView::tr("Block bootstrap");
    return r.method.isEmpty() ? dash() : r.method;
}

bool mc_trade_based(const BtMonteCarloResult& r) {
    return r.method_id.startsWith(QLatin1String("trade"));
}

} // namespace

BtMonteCarloView::BtMonteCarloView(QWidget* parent) : QWidget(parent) {
    build_ui();
    ui::formatting::on_format_changed(this, [this]() {
        if (result_ && !result_->p50.isEmpty())
            render();
    });
}

BtMonteCarloView::~BtMonteCarloView() = default;

void BtMonteCarloView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    stack_ = new QStackedWidget(this);
    root->addWidget(stack_);

    empty_ = new ui::EmptyState(stack_);
    empty_->apply(ui::EmptyState::no_data_spec(
        tr("No Monte Carlo yet"),
        tr("Choose a resampling method above and run it: the active run's trades or returns are resampled into "
           "many alternative paths, and the spread of outcomes appears here.")));
    stack_->addWidget(empty_);

    QVBoxLayout* page = nullptr;
    QScrollArea* scroll = bt_analysis_page(stack_, &page);
    stack_->addWidget(scroll);
    QWidget* body = scroll->widget();

    notices_ = new BtAnalysisNotices(body);
    page->addWidget(notices_);

    kpis_ = new ui::KpiStrip(body);
    t_med_ = kpis_->add_tile(QStringLiteral("med"), tr("MEDIAN FINAL"));
    t_p5_ = kpis_->add_tile(QStringLiteral("p5"), tr("P5 FINAL"));
    t_p95_ = kpis_->add_tile(QStringLiteral("p95"), tr("P95 FINAL"));
    t_loss_ = kpis_->add_tile(QStringLiteral("loss"), tr("PROB. OF LOSS"));
    t_ruin_ = kpis_->add_tile(QStringLiteral("ruin"), tr("PROB. OF RUIN"));
    t_dd50_ = kpis_->add_tile(QStringLiteral("dd50"), tr("MAX DD P50"));
    t_dd95_ = kpis_->add_tile(QStringLiteral("dd95"), tr("MAX DD P95"));
    t_orig_ = kpis_->add_tile(QStringLiteral("orig"), tr("ORIGINAL RUN"));
    t_p5_->set_tooltip_text(tr("5 % of the simulated paths ended below this."));
    t_p95_->set_tooltip_text(tr("5 % of the simulated paths ended above this."));
    t_loss_->set_tooltip_text(tr("Share of the paths that ended below the starting capital."));
    t_ruin_->set_tooltip_text(tr("Share of the paths that fell below the ruin level at any point."));
    t_orig_->set_tooltip_text(tr("Where the realised run's final equity falls among the simulated finals."));
    page->addWidget(kpis_);

    // FAN | PERCENTILES
    auto* row1 = new BtAnalysisReflow(body, 118);
    fan_panel_ = make_panel(tr("FAN"), row1);
    fan_seg_ = make_segmented({tr("FAN"), tr("PATHS")}, 0,
                              [this](int i) {
                                  fan_mode_ = i;
                                  fan_stack_->setCurrentIndex(i);
                              },
                              fan_panel_.header);
    fan_seg_->setToolTip(tr("FAN: percentile bands. PATHS: the original run's path over the percentile lines and "
                            "sample paths."));
    fan_panel_.header->add_control(fan_seg_);
    fan_stack_ = new QStackedWidget(fan_panel_.body_widget);
    fan_ = new ui::FanChart(fan_stack_);
    bt_analysis_chart_menu(fan_, QStringLiteral("monte_carlo_fan"), [this](QStringList* h, QVector<QStringList>* rows) {
        *h << tr("Step") << tr("Date") << QStringLiteral("P5") << QStringLiteral("P25") << QStringLiteral("P50")
           << QStringLiteral("P75") << QStringLiteral("P95") << tr("Original");
        if (!result_)
            return;
        const BtMonteCarloResult& r = *result_;
        auto at = [](const QVector<double>& v, int i) {
            return i < v.size() && std::isfinite(v.at(i)) ? QString::number(v.at(i), 'g', 12) : QString();
        };
        for (int i = 0; i < r.x.size(); ++i)
            rows->append({QString::number(r.x.at(i), 'g', 12), i < r.x_dates.size() ? r.x_dates.at(i) : QString(),
                          at(r.p5, i), at(r.p25, i), at(r.p50, i), at(r.p75, i), at(r.p95, i), at(r.original, i)});
    });
    fan_stack_->addWidget(fan_);
    paths_ = make_time_chart(fan_stack_, QStringLiteral("monte_carlo_paths"));
    paths_->set_show_legend(true);
    fan_stack_->addWidget(paths_);
    bt_analysis_fit_chart(fan_, 17);
    bt_analysis_fit_chart(paths_, 17);
    fan_panel_.body->addWidget(fan_stack_, 1);
    fan_note_ = make_note(QString(), fan_panel_.body_widget);
    fan_panel_.body->addWidget(fan_note_);
    row1->add(fan_panel_.frame, 3);

    pct_panel_ = make_panel(tr("PERCENTILES"), row1);
    pct_view_ = new QTableView(pct_panel_.body_widget);
    pct_model_ = new McPctModel(this);
    pct_model_->set_headers({QString(), QStringLiteral("P5"), QStringLiteral("P25"), QStringLiteral("P50"),
                             QStringLiteral("P75"), QStringLiteral("P95")});
    pct_model_->set_key([](const BtMcPctRow& r) { return r.key; });
    pct_model_->set_cell([](const BtMcPctRow& r, int col, int role) -> QVariant {
        const double v = col >= 1 && col <= r.values.size() ? r.values.at(col - 1) : kBtNaN;
        if (role == Qt::DisplayRole) {
            if (col == 0)
                return r.label;
            switch (r.unit) {
                case Unit::Money:
                case Unit::SignedMoney:
                    return money_compact(v, r.ccy);
                case Unit::SignedFraction:
                    return pct(v, 1, true);
                case Unit::Fraction:
                case Unit::PctPoints:
                case Unit::Ratio:
                case Unit::Count:
                case Unit::Bars:
                case Unit::Plain:
                    break;
            }
            return pct(v, 1);
        }
        if (role == McPctModel::SortRole)
            return col == 0 ? QVariant(r.label) : QVariant(std::isfinite(v) ? v : -1e300);
        if (role == Qt::TextAlignmentRole)
            return col == 0 ? QVariant(int(Qt::AlignLeft | Qt::AlignVCenter))
                            : QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole) {
            if (col == 0)
                return QVariant::fromValue(tone_color(Tone::Dim));
            if (r.key == QLatin1String("return") || r.key == QLatin1String("cagr"))
                return QVariant::fromValue(tone_color(sign_tone(v)));
            if (r.key == QLatin1String("dd") && std::isfinite(v) && v >= 0.25)
                return QVariant::fromValue(tone_color(Tone::Warn));
            return {};
        }
        if (role == Qt::ToolTipRole && col >= 1 && col <= r.tips.size())
            return r.tips.at(col - 1);
        return {};
    });
    pct_view_->setModel(pct_model_);
    setup_table(pct_view_, QStringLiteral("monte_carlo_percentiles"));
    pct_view_->setSortingEnabled(false);
    pct_view_->horizontalHeader()->setSortIndicatorShown(false);
    set_column_chars(pct_view_, {13, 9, 9, 9, 9, 9});
    ui::formatting::refresh_model_on_format_change(pct_model_);
    bt_analysis_fit_table(pct_view_, 4);
    pct_panel_.body->addWidget(pct_view_);
    auto* sim_title = new QLabel(tr("SIMULATION"), pct_panel_.body_widget);
    sim_title->setObjectName(QStringLiteral("btSectionTitle"));
    pct_panel_.body->addWidget(sim_title);
    sim_kv_ = new BtKvGrid(pct_panel_.body_widget, 1);
    pct_panel_.body->addWidget(sim_kv_);
    pct_panel_.body->addStretch(1);
    row1->add(pct_panel_.frame, 2);
    page->addWidget(row1);

    // FINAL EQUITY | MAX DRAWDOWN
    auto* row2 = new BtAnalysisReflow(body, 100);
    final_panel_ = make_panel(tr("FINAL EQUITY"), row2);
    final_stack_ = new QStackedWidget(final_panel_.body_widget);
    final_chart_ = new ui::DistributionChart(final_stack_);
    bt_analysis_chart_menu(final_chart_, QStringLiteral("monte_carlo_final_equity"),
                           [this](QStringList* h, QVector<QStringList>* rows) {
                               *h << tr("Final equity");
                               if (result_)
                                   for (double v : result_->final_values)
                                       rows->append({QString::number(v, 'g', 12)});
                           });
    final_stack_->addWidget(final_chart_);
    final_note_ = make_note(QString(), final_stack_, Tone::None);
    final_note_->setAlignment(Qt::AlignCenter);
    final_stack_->addWidget(final_note_);
    bt_analysis_fit_chart(final_chart_, 13);
    final_panel_.body->addWidget(final_stack_, 1);
    row2->add(final_panel_.frame, 1);

    dd_panel_ = make_panel(tr("MAX DRAWDOWN"), row2);
    dd_chart_ = new ui::DistributionChart(dd_panel_.body_widget);
    dd_chart_->set_value_formatter([](double v) { return pct(v, 1); });
    bt_analysis_chart_menu(dd_chart_, QStringLiteral("monte_carlo_max_drawdown"),
                           [this](QStringList* h, QVector<QStringList>* rows) {
                               *h << tr("Max drawdown");
                               if (result_)
                                   for (double v : result_->drawdown_values)
                                       rows->append({QString::number(v, 'g', 12)});
                           });
    bt_analysis_fit_chart(dd_chart_, 13);
    dd_panel_.body->addWidget(dd_chart_, 1);
    row2->add(dd_panel_.frame, 1);
    page->addWidget(row2);

    page->addStretch(1);
    stack_->setCurrentWidget(empty_);
}

void BtMonteCarloView::show_result(const BtMonteCarloPtr& result, const QString& ccy, const QString& engine_label) {
    result_ = result;
    ccy_ = ccy;
    engine_ = engine_label.trimmed().isEmpty() ? tr("this engine") : engine_label.trimmed();
    if (!result_ || result_->p50.isEmpty()) {
        pct_model_->clear();
        if (!result_) {
            empty_->apply(ui::EmptyState::no_data_spec(
                tr("No Monte Carlo yet"),
                tr("Choose a resampling method above and run it: the active run's trades or returns are resampled "
                   "into many alternative paths, and the spread of outcomes appears here.")));
        } else {
            QString why = tr("The simulation returned no percentile bands: the run may have too few trades or bars "
                             "to resample.");
            if (!result_->warnings.isEmpty())
                why += QLatin1Char(' ') + result_->warnings.first().message;
            empty_->apply(ui::EmptyState::no_data_spec(tr("No simulated paths"), why));
        }
        stack_->setCurrentWidget(empty_);
        return;
    }
    render();
}

void BtMonteCarloView::render() {
    if (!result_ || result_->p50.isEmpty())
        return;
    if (stack_->currentIndex() != 1)
        stack_->setCurrentIndex(1);
    render_notices();
    render_kpis();
    render_fan();
    render_paths();
    render_percentiles();
    render_distributions();
}

double BtMonteCarloView::years() const {
    if (!result_)
        return kBtNaN;
    auto span = [](const QString& a, const QString& b) {
        const double ta = bt_time_ms(a);
        const double tb = bt_time_ms(b);
        return std::isfinite(ta) && std::isfinite(tb) && tb > ta ? (tb - ta) / (365.25 * 86400000.0) : kBtNaN;
    };
    const QStringList& d = result_->x_dates;
    if (d.size() >= 2) {
        const double y = span(d.first(), d.last());
        if (std::isfinite(y))
            return y;
    }
    return span(result_->provenance.first_bar, result_->provenance.last_bar);
}

void BtMonteCarloView::render_notices() {
    const BtMonteCarloResult& r = *result_;
    QVector<BtAnalysisNotice> n;
    if (r.method_id == QLatin1String("block_bootstrap"))
        n.append({tr("Block bootstrap keeps the dependence inside blocks of about %1; longer memory and regime "
                     "changes are not captured.")
                      .arg(std::isfinite(r.block_size) ? bars(r.block_size) : tr("the block length")),
                  Tone::Warn});
    else
        n.append({tr("Resampling assumes trades / returns are independent; serial correlation and regime change are "
                     "not captured (use BLOCK BOOTSTRAP to keep short-range dependence)."),
                  Tone::Warn});
    if (r.method_id == QLatin1String("trade_shuffle"))
        n.append({tr("Trade shuffle reorders the same trades: every path ends at the same final equity "
                     "(multiplication commutes), so the final-equity percentiles are identical by construction. "
                     "Only the path, the drawdowns and the risk of ruin vary."),
                  Tone::Dim});
    if (r.n_simulations > 0 && r.n_simulations < 500)
        n.append({tr("Only %1 paths: the tail percentiles (P5, P95) are noisy.").arg(count(r.n_simulations)),
                  Tone::Dim});
    n += bt_analysis_warning_notices(r.warnings);
    notices_->set_notices(n);
}

void BtMonteCarloView::render_kpis() {
    using T = ui::StatTile::Tone;
    const BtMonteCarloResult& r = *result_;
    const bool shuffle = r.method_id == QLatin1String("trade_shuffle");
    const double initial = r.initial_capital;

    auto final_tile = [&](ui::StatTile* tile, const char* key) {
        const double v = r.stat(QString::fromLatin1(key));
        if (shuffle) {
            const QString why = tr("Identical in every path (trade shuffle)");
            set_tile(tile, kBtNaN, Unit::Money, Rule::None, ccy_, why);
            tile->set_sub_label(why);
            return;
        }
        set_tile(tile, v, Unit::Money, Rule::None, ccy_, tr("Not reported by %1").arg(engine_));
        if (std::isfinite(v) && std::isfinite(initial) && initial > 0) {
            const double rel = v / initial - 1.0;
            tile->set_tone(rel > 0 ? T::Positive : (rel < 0 ? T::Negative : T::Neutral));
            tile->set_sub_label(tr("%1 vs start").arg(pct(rel, 1, true)));
        } else {
            tile->set_sub_label(QString());
        }
    };
    final_tile(t_med_, "finalP50");
    final_tile(t_p5_, "finalP5");
    final_tile(t_p95_, "finalP95");

    const double loss = r.stat(QStringLiteral("probLoss"));
    set_tile(t_loss_, loss, Unit::Fraction, Rule::None, ccy_, tr("Not reported by %1").arg(engine_));
    // (A blank tile keeps an empty sub-label: StatTile then shows the reason there.)
    if (std::isfinite(loss)) {
        t_loss_->set_value(pct(loss, 1));
        t_loss_->set_tone(loss > 0.25 ? T::Warning : T::Neutral);
    }
    t_loss_->set_sub_label(std::isfinite(loss) ? tr("paths ending below the start") : QString());

    const double ruin = r.stat(QStringLiteral("probRuin"));
    const double level = r.stat(QStringLiteral("ruinLevel"));
    set_tile(t_ruin_, ruin, Unit::Fraction, Rule::None, ccy_, tr("Not reported by %1").arg(engine_));
    if (std::isfinite(ruin)) {
        t_ruin_->set_value(pct(ruin, 1));
        t_ruin_->set_tone(ruin > 0.01 ? T::Warning : T::Neutral);
    }
    t_ruin_->set_sub_label(!std::isfinite(ruin)   ? QString()
                           : std::isfinite(level) ? tr("falls below %1 of the start").arg(pct(level, 0))
                                                  : tr("ruin level not reported"));

    const double dd50 = r.stat(QStringLiteral("ddP50"));
    const double dd95 = r.stat(QStringLiteral("ddP95"));
    set_tile(t_dd50_, dd50, Unit::Fraction, Rule::Drawdown, ccy_, tr("Not reported by %1").arg(engine_));
    t_dd50_->set_sub_label(std::isfinite(dd50) ? tr("half of the paths go deeper") : QString());
    set_tile(t_dd95_, dd95, Unit::Fraction, Rule::Drawdown, ccy_, tr("Not reported by %1").arg(engine_));
    t_dd95_->set_sub_label(std::isfinite(dd95) ? tr("1 path in 20 goes deeper") : QString());

    const double op = r.stat(QStringLiteral("originalPercentile"));
    const double of = r.stat(QStringLiteral("originalFinal"));
    if (!std::isfinite(op)) {
        const QString why = shuffle ? tr("Undefined for a trade shuffle (every path ends the same)")
                                    : tr("Not reported by %1").arg(engine_);
        set_tile(t_orig_, kBtNaN, Unit::Plain, Rule::None, ccy_, why);
        t_orig_->set_sub_label(std::isfinite(of) ? tr("final %1").arg(money_compact(of, ccy_)) : why);
    } else {
        t_orig_->set_state(ui::StatTile::State::Normal);
        t_orig_->set_value(tr("P%1").arg(static_cast<int>(std::lround(op * 100.0))));
        t_orig_->set_tone(op >= 0.9 ? T::Warning : T::Neutral);
        const QString where = op >= 0.9   ? tr("luckier than 90 % of resamples")
                              : op <= 0.1 ? tr("unluckier than 90 % of resamples")
                                          : tr("a typical path");
        t_orig_->set_sub_label(std::isfinite(of) ? tr("%1 · %2").arg(money_compact(of, ccy_), where) : where);
    }
}

void BtMonteCarloView::render_fan() {
    const BtMonteCarloResult& r = *result_;
    const int n = static_cast<int>(r.x.size());
    QVector<ui::FanBand> bands;
    if (r.p5.size() == n && r.p95.size() == n)
        bands.append(ui::FanBand{5, 95, r.p5, r.p95});
    if (r.p25.size() == n && r.p75.size() == n)
        bands.append(ui::FanBand{25, 75, r.p25, r.p75});
    fan_->clear_reference_lines();
    fan_->clear_lines(); // lines are kept across set_fan(): drop the previous result's
    fan_->set_fan(r.x, bands, r.p50.size() == n ? r.p50 : QVector<double>());
    if (r.original.size() == n) // the realised path over the bands (legend + hover read-out)
        fan_->add_line(r.original, tr("Original run"), ui::ChartColorRole::Accent);
    const QString ccy = ccy_;
    fan_->set_value_formatter([ccy](double v) { return money_compact(v, ccy); });

    const bool trades = mc_trade_based(r) && r.x_dates.isEmpty();
    const QVector<double> xs = r.x;
    const QStringList dates = r.x_dates;
    fan_->set_x_formatter([xs, dates, trades](double v) {
        if (dates.size() == xs.size() && !xs.isEmpty()) {
            const auto it = std::lower_bound(xs.cbegin(), xs.cend(), v);
            int i = static_cast<int>(it - xs.cbegin());
            if (i >= xs.size())
                i = static_cast<int>(xs.size()) - 1;
            if (i > 0 && std::fabs(xs.at(i - 1) - v) < std::fabs(xs.at(i) - v))
                --i;
            return date_text(dates.at(i)).left(10);
        }
        const QString k = num(v, 0);
        return trades ? BtMonteCarloView::tr("trade %1").arg(k) : BtMonteCarloView::tr("bar %1").arg(k);
    });

    const double initial = r.initial_capital;
    const double of = r.stat(QStringLiteral("originalFinal"));
    if (std::isfinite(initial))
        fan_->add_reference_line(initial, tr("Start"));
    if (std::isfinite(of))
        fan_->add_reference_line(of, tr("Original final"));
    // The ruin line only when it is near the fan (it would otherwise squash the bands).
    const double level = r.stat(QStringLiteral("ruinLevel"));
    if (std::isfinite(level) && std::isfinite(initial) && !r.p5.isEmpty() && !r.p95.isEmpty()) {
        const double lo = *std::min_element(r.p5.cbegin(), r.p5.cend());
        const double hi = *std::max_element(r.p95.cbegin(), r.p95.cend());
        const double ruin_v = level * initial;
        if (std::isfinite(lo) && std::isfinite(hi) && ruin_v >= lo - 0.25 * (hi - lo))
            fan_->add_reference_line(ruin_v, tr("Ruin"));
    }

    QStringList badge;
    badge << tr("%1 paths").arg(count(r.n_simulations)) << mc_method_text(r);
    fan_panel_.header->set_badge(badge.join(QStringLiteral(" · ")));
    QString note = r.original.size() == n
                       ? tr("Bands: 5–95 and 25–75 percentiles of equity at each step; lines: the median and the "
                            "original run.")
                       : tr("Bands: 5–95 and 25–75 percentiles of equity at each step; line: the median.");
    if (std::isfinite(of))
        note += QLatin1Char(' ') + tr("The original run ended at %1 (dashed line).").arg(money_compact(of, ccy_));
    note += QLatin1Char(' ') + (r.original.size() == n ? tr("PATHS shows the original run's whole path.")
                                                       : tr("%1 did not report the original path.").arg(engine_));
    fan_note_->setText(note);
    fan_stack_->setCurrentIndex(fan_mode_);
    set_segment(fan_seg_, fan_mode_);
}

void BtMonteCarloView::render_paths() {
    const BtMonteCarloResult& r = *result_;
    const int n = static_cast<int>(r.x.size());
    paths_->clear();

    // Time axis when every step has a date, else the step index.
    QVector<double> xt;
    bool timed = r.x_dates.size() == n && n >= 2;
    if (timed) {
        xt.reserve(n);
        for (const QString& d : r.x_dates) {
            const double t = bt_time_ms(d);
            if (!std::isfinite(t)) {
                timed = false;
                break;
            }
            xt.append(t);
        }
    }
    if (!timed)
        xt = r.x;
    paths_->set_x_is_time(timed);

    auto add = [&](const QVector<double>& ys, const QString& name, ui::ChartColorRole role, int palette, qreal width,
                   bool dashed, bool legend) {
        if (ys.size() != n)
            return;
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = name;
        s.points = to_points(xt, ys);
        s.color = role;
        s.palette_index = palette;
        s.line_width = width;
        s.dashed = dashed;
        s.rebase = false;
        s.show_in_legend = legend;
        s.show_in_tooltip = legend;
        paths_->add_series(s);
    };
    const int samples = std::min(20, static_cast<int>(r.sample_paths.size()));
    for (int i = 0; i < samples; ++i)
        add(r.sample_paths.at(i), tr("Sample path"), ui::ChartColorRole::Muted, -1, 1.0, false, false);
    add(r.p95, QStringLiteral("P95"), ui::ChartColorRole::Info, -1, 1.2, true, true);
    add(r.p75, QStringLiteral("P75"), ui::ChartColorRole::Info, -1, 1.0, false, true);
    add(r.p50, tr("Median"), ui::ChartColorRole::Palette, 1, 2.0, false, true);
    add(r.p25, QStringLiteral("P25"), ui::ChartColorRole::Info, -1, 1.0, false, true);
    add(r.p5, QStringLiteral("P5"), ui::ChartColorRole::Info, -1, 1.2, true, true);
    add(r.original, tr("Original run"), ui::ChartColorRole::Accent, -1, 2.4, false, true);
    paths_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, money_formatter(ccy_), 0);
    if (!timed) {
        const bool trades = mc_trade_based(r);
        paths_->set_time_formatter([trades](double v) {
            const QString k = num(v, 0);
            return trades ? BtMonteCarloView::tr("trade %1").arg(k) : BtMonteCarloView::tr("bar %1").arg(k);
        });
    } else {
        paths_->set_time_formatter({});
    }
    if (std::isfinite(r.initial_capital))
        paths_->add_hline(r.initial_capital, tr("Start"), ui::ChartColorRole::Muted);
    paths_->reset_x_range();
}

void BtMonteCarloView::render_percentiles() {
    const BtMonteCarloResult& r = *result_;
    const bool shuffle = r.method_id == QLatin1String("trade_shuffle");
    const double initial = r.initial_capital;
    const double yrs = years();
    const QVector<double> finals = mc_sorted(r.final_values);
    const QVector<double> dds = mc_sorted(r.drawdown_values);
    const QString sampled = tr("Computed here from the %1 sampled values").arg(count(static_cast<double>(finals.size())));
    const QString sampled_dd = tr("Computed here from the %1 sampled drawdowns").arg(count(static_cast<double>(dds.size())));

    BtMcPctRow fin{QStringLiteral("final"), tr("Final equity"), Unit::Money, ccy_, QVector<double>(5, kBtNaN),
                   QStringList(5, QString())};
    BtMcPctRow ret{QStringLiteral("return"), tr("Total return"), Unit::SignedFraction, ccy_, QVector<double>(5, kBtNaN),
                   QStringList(5, QString())};
    BtMcPctRow cagr{QStringLiteral("cagr"), tr("CAGR"), Unit::SignedFraction, ccy_, QVector<double>(5, kBtNaN),
                    QStringList(5, QString())};
    BtMcPctRow dd{QStringLiteral("dd"), tr("Max drawdown"), Unit::Fraction, ccy_, QVector<double>(5, kBtNaN),
                  QStringList(5, QString())};
    static const char* const fin_keys[5] = {"finalP5", "finalP25", "finalP50", "finalP75", "finalP95"};
    static const char* const cagr_keys[5] = {"cagrP5", nullptr, "cagrP50", nullptr, "cagrP95"};
    const double of = r.stat(QStringLiteral("originalFinal"));
    for (int i = 0; i < 5; ++i) {
        if (shuffle) {
            fin.values[i] = of;
            fin.tips[i] = tr("Identical in every path by construction (trade shuffle)");
        } else {
            fin.values[i] = r.stat(QString::fromLatin1(fin_keys[i]));
            if (!std::isfinite(fin.values[i]) && !finals.isEmpty()) {
                fin.values[i] = mc_quantile(finals, kMcQs[i]);
                fin.tips[i] = sampled;
            }
        }
        if (std::isfinite(fin.values[i]) && std::isfinite(initial) && initial > 0)
            ret.values[i] = fin.values[i] / initial - 1.0;
        ret.tips[i] = fin.tips[i];
        if (cagr_keys[i] && !shuffle)
            cagr.values[i] = r.stat(QString::fromLatin1(cagr_keys[i]));
        if (!std::isfinite(cagr.values[i]) && std::isfinite(fin.values[i]) && std::isfinite(yrs) && yrs > 0 &&
            std::isfinite(initial) && initial > 0 && fin.values[i] > 0) {
            cagr.values[i] = std::pow(fin.values[i] / initial, 1.0 / yrs) - 1.0;
            cagr.tips[i] = tr("Computed here from the final-equity percentile over %1 years").arg(num(yrs, 2));
        }
        if (!std::isfinite(cagr.values[i]) && cagr.tips[i].isEmpty())
            cagr.tips[i] = tr("Not reported (no dated horizon)");
    }
    // The engine reports the drawdown P50 and P95; the other columns come from the sampled drawdowns.
    dd.values[2] = r.stat(QStringLiteral("ddP50"));
    dd.values[4] = r.stat(QStringLiteral("ddP95"));
    for (int i = 0; i < 5; ++i) {
        if (!std::isfinite(dd.values[i]) && !dds.isEmpty()) {
            dd.values[i] = mc_quantile(dds, kMcQs[i]);
            dd.tips[i] = sampled_dd;
        }
    }
    dd.tips[0] = dd.tips[0].isEmpty() ? tr("The shallowest 5 % of paths") : dd.tips[0] + QLatin1Char('\n') +
                                                                                 tr("The shallowest 5 % of paths");
    dd.tips[4] = dd.tips[4].isEmpty() ? tr("1 path in 20 goes deeper") : dd.tips[4];
    pct_model_->upsert({fin, ret, cagr, dd});
    pct_panel_.header->set_badge(shuffle ? tr("finals identical") : QString());

    QVector<BtKv> kv;
    kv.append({tr("Method"), mc_method_text(r), Tone::None, r.method});
    kv.append({tr("Paths"), count(r.n_simulations), Tone::None, {}});
    kv.append({tr("Seed"), std::isfinite(r.seed) ? num(r.seed, 0) : dash(), Tone::None,
               tr("The same seed reproduces the same paths")});
    if (r.method_id == QLatin1String("block_bootstrap"))
        kv.append({tr("Mean block"), bars(r.block_size), Tone::None, {}});
    if (r.horizon_bars > 0)
        kv.append({tr("Horizon"), bars(r.horizon_bars), Tone::None, {}});
    if (mc_trade_based(r) || r.n_trades > 0)
        kv.append({tr("Trades resampled"), count(r.n_trades), r.n_trades >= 0 && r.n_trades < 30 ? Tone::Warn : Tone::None,
                   r.n_trades >= 0 && r.n_trades < 30 ? tr("Fewer than 30 trades: the resampled spread is unreliable")
                                                      : QString()});
    kv.append({tr("Start capital"), money(initial, ccy_), Tone::None, {}});
    const double level = r.stat(QStringLiteral("ruinLevel"));
    kv.append({tr("Ruin level"), std::isfinite(level) ? tr("below %1 of the start").arg(pct(level, 0)) : dash(),
               Tone::None, {}});
    kv.append({tr("Original final"), money(of, ccy_), Tone::None, {}});
    kv.append({tr("Original max DD"), pct(r.stat(QStringLiteral("originalMaxDrawdown")), 1), Tone::None, {}});
    const double dbl = r.stat(QStringLiteral("probDoubling"));
    if (std::isfinite(dbl))
        kv.append({tr("Prob. of doubling"), pct(dbl, 1), Tone::None, {}});
    const double dd20 = r.stat(QStringLiteral("probDdOver20"));
    if (std::isfinite(dd20))
        kv.append({tr("Prob. max DD > 20 %"), pct(dd20, 1), dd20 > 0.25 ? Tone::Warn : Tone::None, {}});
    const double dd99 = r.stat(QStringLiteral("ddP99"));
    if (std::isfinite(dd99))
        kv.append({tr("Max DD P99"), pct(dd99, 1), dd99 >= 0.25 ? Tone::Warn : Tone::None, {}});
    sim_kv_->set_rows(kv);
}

void BtMonteCarloView::render_distributions() {
    const BtMonteCarloResult& r = *result_;
    const bool shuffle = r.method_id == QLatin1String("trade_shuffle");
    const double initial = r.initial_capital;
    const double of = r.stat(QStringLiteral("originalFinal"));

    final_chart_->clear_markers();
    if (shuffle) {
        final_note_->setText(tr("Every reshuffled path ends at %1: the trades are the same, only their order changes, "
                                "and the compounded result does not depend on the order. The drawdown distribution "
                                "on the right is what this method measures.")
                                 .arg(money(of, ccy_)));
        final_stack_->setCurrentWidget(final_note_);
        final_panel_.header->set_badge(tr("identical by construction"));
    } else {
        final_stack_->setCurrentWidget(final_chart_);
        const QString ccy = ccy_;
        final_chart_->set_value_formatter([ccy](double v) { return money_compact(v, ccy); });
        final_chart_->set_samples(r.final_values);
        if (r.final_values.isEmpty())
            final_chart_->set_state(ui::PaintedChart::Empty, tr("Final values not reported by %1").arg(engine_));
        else
            final_chart_->set_state(ui::PaintedChart::Ready);
        if (std::isfinite(initial))
            final_chart_->add_marker(initial, tr("Start"), ui::DistributionChart::MarkerRole::Neutral);
        const double med = r.stat(QStringLiteral("finalP50"));
        if (std::isfinite(med))
            final_chart_->add_marker(med, tr("Median"), ui::DistributionChart::MarkerRole::Median);
        if (std::isfinite(of))
            final_chart_->add_marker(of, tr("Original"), ui::DistributionChart::MarkerRole::Accent);
        final_panel_.header->set_badge(tr("%1 values").arg(count(static_cast<double>(r.final_values.size()))));
    }

    dd_chart_->clear_markers();
    dd_chart_->set_samples(r.drawdown_values);
    if (r.drawdown_values.isEmpty())
        dd_chart_->set_state(ui::PaintedChart::Empty, tr("Drawdowns not reported by %1").arg(engine_));
    else
        dd_chart_->set_state(ui::PaintedChart::Ready);
    const double dd50 = r.stat(QStringLiteral("ddP50"));
    const double dd95 = r.stat(QStringLiteral("ddP95"));
    const double odd = r.stat(QStringLiteral("originalMaxDrawdown"));
    if (std::isfinite(dd50))
        dd_chart_->add_marker(dd50, tr("Median"), ui::DistributionChart::MarkerRole::Median);
    if (std::isfinite(dd95))
        dd_chart_->add_marker(dd95, QStringLiteral("P95"), ui::DistributionChart::MarkerRole::Neutral);
    if (std::isfinite(odd))
        dd_chart_->add_marker(odd, tr("Original"), ui::DistributionChart::MarkerRole::Accent);
    dd_panel_.header->set_badge(tr("%1 values").arg(count(static_cast<double>(r.drawdown_values.size()))));
}

} // namespace fincept::screens::bt
