// src/screens/backtesting/BtWalkForwardView.cpp
//
// WALK-FWD view (BtAnalysisViews.h): KPI row, FOLDS timeline, stitched OUT-OF-SAMPLE EQUITY, IN vs
// OUT per window, the WINDOWS table and PARAMETER STABILITY.
//
// Thresholds of the readings (state colours, never P&L colours):
//   Walk-forward efficiency (annualised OOS return / annualised IS return): < 50 % -> warn
//   (the usual acceptance bar: at least half of the optimised performance survives out of sample).
//   IS -> OOS Sharpe (degradation = mean OOS Sharpe / mean IS Sharpe): < 0.5 -> warn.
//   Profitable windows: fewer than half -> warn.
//   Parameter stability per parameter across windows (positive values: max / min; otherwise the
//   coefficient of variation std / |mean|): max / min <= 1.5 (CV <= 0.25) stable · < 2 (CV <= 0.5)
//   moderately stable · otherwise unstable ("jumped a -> b" names the largest window-to-window move).
#include "screens/backtesting/BtAnalysisViews.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QColor>
#include <QFontMetrics>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::bt_time_ms;
using services::backtest::BtWalkForwardPtr;
using services::backtest::BtWalkForwardResult;
using services::backtest::BtWfWindow;

/// One parameter of PARAMETER STABILITY.
struct BtWfParamRow {
    QString name;
    QVector<double> values; ///< per window (NaN = not reported)
    double sd = kBtNaN;
    double lo = kBtNaN;
    double hi = kBtNaN;
    int verdict = -1; ///< 0 stable, 1 moderately stable, 2 unstable, -1 unknown
    QString reading;
};

namespace {

using WfWindowModel = ui::LiveTableModel<BtWfWindow>;
using WfParamModel = ui::LiveTableModel<BtWfParamRow>;

QString wf_window_label(int window) {
    return BtWalkForwardView::tr("W%1").arg(window);
}

QString wf_span(const QString& a, const QString& b) {
    if (a.isEmpty() && b.isEmpty())
        return dash();
    return QStringLiteral("%1 → %2").arg(date_text(a), date_text(b));
}

Tone wf_verdict_tone(int verdict) {
    switch (verdict) {
        case 0:
            return Tone::Ok;
        case 1:
            return Tone::Warn;
        case 2:
            return Tone::Danger;
        default:
            break;
    }
    return Tone::Dim;
}

BtWfParamRow wf_param_row(const QString& name, const QVector<double>& values, double sd) {
    BtWfParamRow row;
    row.name = name;
    row.values = values;
    row.sd = sd;
    QVector<double> f;
    for (double v : values)
        if (std::isfinite(v))
            f.append(v);
    if (f.isEmpty()) {
        row.reading = BtWalkForwardView::tr("%1 was not reported per window.").arg(name);
        return row;
    }
    row.lo = *std::min_element(f.cbegin(), f.cend());
    row.hi = *std::max_element(f.cbegin(), f.cend());
    double mean = 0;
    for (double v : f)
        mean += v;
    mean /= static_cast<double>(f.size());
    if (!std::isfinite(row.sd) && f.size() > 1) {
        double ss = 0;
        for (double v : f)
            ss += (v - mean) * (v - mean);
        row.sd = std::sqrt(ss / static_cast<double>(f.size() - 1));
    }
    const QString lo = bt_analysis_param_text(row.lo);
    const QString hi = bt_analysis_param_text(row.hi);
    if (row.hi - row.lo <= 1e-12 * std::max(1.0, std::fabs(row.hi))) {
        row.verdict = 0;
        row.reading = BtWalkForwardView::tr("%1 stayed at %2 in every window — stable.").arg(name, lo);
        return row;
    }
    if (row.lo > 0) {
        const double spread = row.hi / row.lo;
        row.verdict = spread <= 1.5 + 1e-9 ? 0 : (spread < 2.0 - 1e-9 ? 1 : 2);
    } else {
        const double cv = std::fabs(mean) > 1e-12 && std::isfinite(row.sd) ? row.sd / std::fabs(mean) : kBtNaN;
        row.verdict = !std::isfinite(cv) ? 2 : (cv <= 0.25 ? 0 : (cv <= 0.5 ? 1 : 2));
    }
    if (row.verdict == 0) {
        row.reading = BtWalkForwardView::tr("%1 moved between %2 and %3 — stable.").arg(name, lo, hi);
    } else if (row.verdict == 1) {
        row.reading = BtWalkForwardView::tr("%1 varied between %2 and %3 — moderately stable.").arg(name, lo, hi);
    } else {
        // The largest move between consecutive reported windows.
        double best = -1;
        double from = row.lo;
        double to = row.hi;
        double prev = kBtNaN;
        for (double v : values) {
            if (!std::isfinite(v))
                continue;
            if (std::isfinite(prev) && std::fabs(v - prev) > best) {
                best = std::fabs(v - prev);
                from = prev;
                to = v;
            }
            prev = v;
        }
        row.reading = BtWalkForwardView::tr("%1 jumped %2 → %3 — unstable.")
                          .arg(name, bt_analysis_param_text(from), bt_analysis_param_text(to));
    }
    return row;
}

QString wf_values_text(const QVector<double>& values) {
    QStringList parts;
    for (double v : values)
        parts << bt_analysis_param_text(v);
    return parts.isEmpty() ? dash() : parts.join(QStringLiteral(" → "));
}

} // namespace

BtWalkForwardView::BtWalkForwardView(QWidget* parent) : QWidget(parent) {
    build_ui();
    ui::formatting::on_format_changed(this, [this]() {
        if (result_ && !result_->windows.isEmpty())
            render();
    });
    // The fold bars' thickness and the chart heights follow the font.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refit(); });
}

BtWalkForwardView::~BtWalkForwardView() = default;

void BtWalkForwardView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    stack_ = new QStackedWidget(this);
    root->addWidget(stack_);

    empty_ = new ui::EmptyState(stack_);
    empty_->apply(ui::EmptyState::no_data_spec(
        tr("No walk-forward yet"),
        tr("Set the windows above and run the walk-forward: every window is optimised on its training span and "
           "traded on the following test span, and the out-of-sample results are chained here.")));
    stack_->addWidget(empty_);

    QVBoxLayout* page = nullptr;
    QScrollArea* scroll = bt_analysis_page(stack_, &page);
    stack_->addWidget(scroll);
    QWidget* body = scroll->widget();

    notices_ = new BtAnalysisNotices(body);
    page->addWidget(notices_);

    kpis_ = new ui::KpiStrip(body);
    t_ret_ = kpis_->add_tile(QStringLiteral("ret"), tr("OOS RETURN"));
    t_sharpe_ = kpis_->add_tile(QStringLiteral("sharpe"), tr("OOS SHARPE"));
    t_prof_ = kpis_->add_tile(QStringLiteral("prof"), tr("PROFITABLE WINDOWS"));
    t_eff_ = kpis_->add_tile(QStringLiteral("eff"), tr("WF EFFICIENCY"));
    t_drop_ = kpis_->add_tile(QStringLiteral("drop"), tr("IS → OOS SHARPE"));
    t_cov_ = kpis_->add_tile(QStringLiteral("cov"), tr("OOS COVERAGE"));
    t_mode_ = kpis_->add_tile(QStringLiteral("mode"), tr("MODE"));
    t_ret_->set_tooltip_text(tr("Out-of-sample returns of every test window, compounded end to end."));
    t_eff_->set_tooltip_text(tr("Walk-forward efficiency: annualised out-of-sample return divided by the annualised "
                                "in-sample return of the optimised sets. Below 50 % most of the optimised edge did "
                                "not survive."));
    t_drop_->set_tooltip_text(tr("Mean out-of-sample Sharpe as a share of the mean in-sample Sharpe."));
    t_cov_->set_tooltip_text(tr("Share of the period that was traded out of sample."));
    page->addWidget(kpis_);

    // FOLDS
    folds_panel_ = make_panel(tr("FOLDS"), body);
    folds_ = make_time_chart(folds_panel_.body_widget, QStringLiteral("walk_forward_folds"));
    folds_->set_crosshair_enabled(false);
    folds_->set_show_legend(true);
    bt_analysis_fit_chart(folds_, 8);
    folds_panel_.body->addWidget(folds_, 1);
    folds_panel_.body->addWidget(make_note(tr("One row per window: the optimiser saw only the muted training span; the "
                                              "accent test span was traded with the set it chose."),
                                           folds_panel_.body_widget));
    page->addWidget(folds_panel_.frame);

    // OUT-OF-SAMPLE EQUITY | IN vs OUT
    auto* row = new BtAnalysisReflow(body, 118);
    equity_panel_ = make_panel(tr("OUT-OF-SAMPLE EQUITY"), row);
    equity_ = make_time_chart(equity_panel_.body_widget, QStringLiteral("walk_forward_oos_equity"));
    bt_analysis_fit_chart(equity_, 15);
    equity_panel_.body->addWidget(equity_, 1);
    equity_panel_.body->addWidget(make_note(tr("Each test window traded with the set optimised on the window before "
                                               "it, chained end to end. Shaded: alternate windows. Select a row in "
                                               "WINDOWS to zoom to it."),
                                            equity_panel_.body_widget));
    row->add(equity_panel_.frame, 3);
    inout_panel_ = make_panel(tr("IN vs OUT"), row);
    inout_ = new ui::BarChart(inout_panel_.body_widget);
    inout_->set_mode(ui::BarChart::Mode::Grouped);
    inout_->set_show_legend(true);
    inout_->set_value_formatter([](double v) { return pct(v, 1, true); });
    bt_analysis_fit_chart(inout_, 15);
    bt_analysis_chart_menu(inout_, QStringLiteral("walk_forward_in_vs_out"),
                           [this](QStringList* h, QVector<QStringList>* rows) {
                               *h << tr("Window") << tr("In-sample return") << tr("Out-of-sample return");
                               if (!result_)
                                   return;
                               for (const BtWfWindow& w : result_->windows)
                                   rows->append({wf_window_label(w.window), QString::number(w.is_return, 'g', 10),
                                                 QString::number(w.oos_return, 'g', 10)});
                           });
    inout_panel_.body->addWidget(inout_, 1);
    inout_panel_.body->addWidget(make_note(tr("Return of each window: optimised (in-sample) vs traded (out of sample)."),
                                           inout_panel_.body_widget));
    row->add(inout_panel_.frame, 2);
    page->addWidget(row);

    // WINDOWS
    windows_panel_ = make_panel(tr("WINDOWS"), body);
    windows_view_ = new QTableView(windows_panel_.body_widget);
    windows_model_ = new WfWindowModel(this);
    windows_model_->set_headers({tr("#"), tr("TRAIN"), tr("TEST"), tr("BEST PARAMS"), tr("IS RETURN"), tr("IS SHARPE"),
                                 tr("OOS RETURN"), tr("OOS SHARPE"), tr("OOS MAX DD"), tr("OOS TRADES")});
    windows_model_->set_key([](const BtWfWindow& w) { return QString::number(w.window); });
    windows_model_->set_cell([](const BtWfWindow& w, int col, int role) -> QVariant {
        if (role == Qt::DisplayRole) {
            switch (col) {
                case 0:
                    return wf_window_label(w.window);
                case 1:
                    return wf_span(w.train_start, w.train_end);
                case 2:
                    return wf_span(w.test_start, w.test_end);
                case 3:
                    return bt_analysis_params_text(w.best_params);
                case 4:
                    return pct(w.is_return, 1, true);
                case 5:
                    return ratio(w.is_sharpe, 2);
                case 6:
                    return pct(w.oos_return, 1, true);
                case 7:
                    return ratio(w.oos_sharpe, 2);
                case 8:
                    return pct(w.oos_max_dd, 1);
                case 9:
                    return count(w.oos_trades);
                default:
                    return {};
            }
        }
        if (role == Qt::EditRole && col == 6)
            return w.oos_return;
        if (role == WfWindowModel::SortRole) {
            const auto num_or_low = [](double v) { return std::isfinite(v) ? v : -1e300; };
            switch (col) {
                case 0:
                    return w.window;
                case 1:
                    return num_or_low(bt_time_ms(w.train_start));
                case 2:
                    return num_or_low(bt_time_ms(w.test_start));
                case 3:
                    return bt_analysis_params_text(w.best_params);
                case 4:
                    return num_or_low(w.is_return);
                case 5:
                    return num_or_low(w.is_sharpe);
                case 6:
                    return num_or_low(w.oos_return);
                case 7:
                    return num_or_low(w.oos_sharpe);
                case 8:
                    return num_or_low(w.oos_max_dd);
                case 9:
                    return num_or_low(w.oos_trades);
                default:
                    return {};
            }
        }
        if (role == Qt::TextAlignmentRole)
            return (col >= 1 && col <= 3) ? QVariant(int(Qt::AlignLeft | Qt::AlignVCenter))
                                          : QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole) {
            if (col == 1)
                return QVariant::fromValue(tone_color(Tone::Dim));
            if (col == 4)
                return QVariant::fromValue(tone_color(sign_tone(w.is_return)));
            if (col == 6)
                return QVariant::fromValue(tone_color(sign_tone(w.oos_return)));
            if (col == 8 && std::isfinite(w.oos_max_dd) && w.oos_max_dd >= 0.25)
                return QVariant::fromValue(tone_color(Tone::Warn));
            return {};
        }
        if (role == Qt::ToolTipRole) {
            if (col == 1 && w.train_bars >= 0)
                return BtWalkForwardView::tr("Training span: %1").arg(bars(w.train_bars));
            if (col == 2 && w.test_bars >= 0)
                return BtWalkForwardView::tr("Test span: %1").arg(bars(w.test_bars));
            if (col == 3)
                return bt_analysis_params_text(w.best_params);
            if (col == 6 && std::isfinite(w.oos_cagr))
                return BtWalkForwardView::tr("Annualised: %1").arg(pct(w.oos_cagr, 1, true));
            return {};
        }
        return {};
    });
    windows_proxy_ = make_sort_proxy(windows_model_, this);
    windows_view_->setModel(windows_proxy_);
    setup_table(windows_view_, QStringLiteral("walk_forward_windows"));
    windows_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    windows_view_->sortByColumn(0, Qt::AscendingOrder);
    windows_view_->setItemDelegateForColumn(6, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar,
                                                                       windows_view_));
    set_column_chars(windows_view_, {5, 25, 25, 28, 10, 9, 12, 10, 11, 10});
    ui::formatting::refresh_model_on_format_change(windows_model_);
    bt_analysis_fit_table(windows_view_, 6);
    connect(windows_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { window_selected(); });
    windows_panel_.body->addWidget(windows_view_, 1);
    page->addWidget(windows_panel_.frame);

    // PARAMETER STABILITY
    stab_panel_ = make_panel(tr("PARAMETER STABILITY"), body);
    stab_chip_ = make_chip(QString(), Tone::Dim, stab_panel_.header);
    stab_panel_.header->add_control(stab_chip_);
    auto* srow = new BtAnalysisReflow(stab_panel_.body_widget, 110);
    stab_view_ = new QTableView(srow);
    stab_model_ = new WfParamModel(this);
    stab_model_->set_headers({tr("PARAMETER"), tr("BY WINDOW"), tr("RANGE"), tr("STD"), tr("READING")});
    stab_model_->set_key([](const BtWfParamRow& r) { return r.name; });
    stab_model_->set_cell([](const BtWfParamRow& r, int col, int role) -> QVariant {
        if (role == Qt::DisplayRole) {
            switch (col) {
                case 0:
                    return r.name;
                case 1:
                    return wf_values_text(r.values);
                case 2:
                    return std::isfinite(r.lo) ? QStringLiteral("%1 – %2").arg(bt_analysis_param_text(r.lo),
                                                                                    bt_analysis_param_text(r.hi))
                                               : dash();
                case 3:
                    return num(r.sd, 2);
                case 4:
                    return r.reading;
                default:
                    return {};
            }
        }
        if (role == WfParamModel::SortRole) {
            switch (col) {
                case 0:
                    return r.name;
                case 1:
                    return wf_values_text(r.values);
                case 2:
                    return std::isfinite(r.hi - r.lo) ? r.hi - r.lo : -1e300;
                case 3:
                    return std::isfinite(r.sd) ? r.sd : -1e300;
                case 4:
                    return r.verdict;
                default:
                    return {};
            }
        }
        if (role == Qt::ForegroundRole && col == 4)
            return QVariant::fromValue(tone_color(wf_verdict_tone(r.verdict)));
        if (role == Qt::ToolTipRole && (col == 1 || col == 4))
            return col == 1 ? wf_values_text(r.values) : r.reading;
        if (role == Qt::TextAlignmentRole)
            return (col == 2 || col == 3) ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter))
                                          : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
        return {};
    });
    stab_view_->setModel(stab_model_);
    setup_table(stab_view_, QStringLiteral("walk_forward_parameter_stability"));
    stab_view_->setSortingEnabled(false);
    stab_view_->horizontalHeader()->setSortIndicatorShown(false);
    stab_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    set_column_chars(stab_view_, {14, 22, 12, 8, 34});
    ui::formatting::refresh_model_on_format_change(stab_model_);
    bt_analysis_fit_table(stab_view_, 3);
    connect(stab_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { render_param_chart(); });
    srow->add(stab_view_, 3);

    auto* chart_box = new QWidget(srow);
    auto* cv = new QVBoxLayout(chart_box);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(4);
    param_title_ = new QLabel(chart_box);
    param_title_->setObjectName(QStringLiteral("btSectionTitle"));
    cv->addWidget(param_title_);
    param_chart_ = new ui::BarChart(chart_box);
    param_chart_->set_show_values(true);
    param_chart_->set_value_formatter([](double v) { return bt_analysis_param_text(v); });
    bt_analysis_fit_chart(param_chart_, 10);
    bt_analysis_chart_menu(param_chart_, QStringLiteral("walk_forward_parameter_by_window"));
    cv->addWidget(param_chart_, 1);
    srow->add(chart_box, 2);
    stab_panel_.body->addWidget(srow);
    stab_note_ = make_note(QString(), stab_panel_.body_widget, Tone::None);
    stab_panel_.body->addWidget(stab_note_);
    page->addWidget(stab_panel_.frame);

    page->addStretch(1);
    stack_->setCurrentWidget(empty_);
}

void BtWalkForwardView::show_result(const BtWalkForwardPtr& result, const QString& ccy, const QString& engine_label) {
    result_ = result;
    ccy_ = ccy;
    engine_ = engine_label.trimmed().isEmpty() ? tr("this engine") : engine_label.trimmed();
    if (!result_ || result_->windows.isEmpty()) {
        windows_model_->clear();
        stab_model_->clear();
        if (!result_) {
            empty_->apply(ui::EmptyState::no_data_spec(
                tr("No walk-forward yet"),
                tr("Set the windows above and run the walk-forward: every window is optimised on its training span "
                   "and traded on the following test span, and the out-of-sample results are chained here.")));
        } else {
            QString why = tr("The engine returned no walk-forward window: the period may be too short for the "
                             "number of windows and the warm-up of the strategy.");
            if (!result_->warnings.isEmpty())
                why += QLatin1Char(' ') + result_->warnings.first().message;
            empty_->apply(ui::EmptyState::no_data_spec(tr("No window was evaluated"), why));
        }
        stack_->setCurrentWidget(empty_);
        return;
    }
    render();
}

void BtWalkForwardView::render() {
    if (!result_ || result_->windows.isEmpty())
        return;
    if (stack_->currentIndex() != 1)
        stack_->setCurrentIndex(1);
    const BtWalkForwardResult& r = *result_;
    QVector<BtAnalysisNotice> n;
    if (r.windows.size() < 3)
        n.append({tr("Only %1 window(s): too few to judge how stable the strategy is out of sample.")
                      .arg(r.windows.size()),
                  Tone::Warn});
    if (r.oos_equity.empty())
        n.append({tr("%1 did not report the stitched out-of-sample equity curve.").arg(engine_), Tone::Dim});
    n += bt_analysis_warning_notices(r.warnings);
    notices_->set_notices(n);
    render_kpis();
    render_folds();
    render_equity();
    render_windows();
    render_stability();
}

void BtWalkForwardView::render_kpis() {
    using T = ui::StatTile::Tone;
    const BtWalkForwardResult& r = *result_;

    // OOS RETURN (compounded)
    double comp = r.compounded_oos_return;
    if (!std::isfinite(comp)) {
        double g = 1.0;
        bool any = false;
        for (const BtWfWindow& w : r.windows)
            if (std::isfinite(w.oos_return)) {
                g *= 1.0 + w.oos_return;
                any = true;
            }
        comp = any ? g - 1.0 : kBtNaN;
    }
    // (A blank tile keeps an empty sub-label: StatTile then shows the reason there.)
    set_tile(t_ret_, comp, Unit::SignedFraction, Rule::Sign, ccy_, tr("No out-of-sample return was reported"));
    t_ret_->set_sub_label(std::isfinite(comp) ? tr("avg %1 per window · σ %2")
                                                    .arg(pct(r.avg_oos_return, 1, true), pct(r.oos_return_std, 1))
                                              : QString());

    // OOS SHARPE
    const double sh = std::isfinite(r.oos_sharpe) ? r.oos_sharpe : r.avg_oos_sharpe;
    set_tile(t_sharpe_, sh, Unit::Ratio, Rule::Negative, ccy_, tr("No out-of-sample Sharpe was reported"));
    t_sharpe_->set_sub_label(std::isfinite(sh) ? tr("avg per window %1 · IS %2")
                                                     .arg(ratio(r.avg_oos_sharpe), ratio(r.avg_is_sharpe))
                                               : QString());

    // PROFITABLE WINDOWS
    int k = 0;
    int n = 0;
    for (const BtWfWindow& w : r.windows) {
        if (!std::isfinite(w.oos_return))
            continue;
        ++n;
        if (w.oos_return > 0)
            ++k;
    }
    if (n == 0) {
        set_tile(t_prof_, kBtNaN, Unit::Plain, Rule::None, ccy_, tr("No window reported a return"));
        t_prof_->set_sub_label(QString());
    } else {
        const double share = std::isfinite(r.robustness) ? r.robustness : static_cast<double>(k) / n;
        t_prof_->set_state(ui::StatTile::State::Normal);
        t_prof_->set_value(tr("%1 / %2").arg(k).arg(n));
        t_prof_->set_tone(share < 0.5 ? T::Warning : T::Neutral);
        t_prof_->set_sub_label(tr("%1 of the windows made money").arg(pct(share, 0)));
    }

    // WF EFFICIENCY
    if (!std::isfinite(r.wf_efficiency)) {
        set_tile(t_eff_, kBtNaN, Unit::Plain, Rule::None, ccy_, tr("Not reported by %1").arg(engine_));
        t_eff_->set_sub_label(tr("not reported by %1").arg(engine_));
    } else {
        t_eff_->set_state(ui::StatTile::State::Normal);
        t_eff_->set_value(pct(r.wf_efficiency, 0));
        t_eff_->set_tone(r.wf_efficiency < 0.5 ? T::Warning : T::Neutral);
        t_eff_->set_sub_label(r.wf_efficiency < 0.5 ? tr("OOS ÷ IS ann. return: below 50 %")
                                                    : tr("OOS ÷ IS annualised return"));
    }

    // IS -> OOS SHARPE DROP
    double deg = r.degradation;
    if (!std::isfinite(deg) && std::isfinite(r.avg_is_sharpe) && std::fabs(r.avg_is_sharpe) > 1e-12)
        deg = r.avg_oos_sharpe / r.avg_is_sharpe;
    if (!std::isfinite(deg)) {
        set_tile(t_drop_, kBtNaN, Unit::Plain, Rule::None, ccy_, tr("Needs the in-sample and out-of-sample Sharpe"));
        t_drop_->set_sub_label(QString());
    } else {
        const double drop = deg - 1.0;
        t_drop_->set_state(ui::StatTile::State::Normal);
        t_drop_->set_value(pct(drop, 0, true));
        t_drop_->set_tone(deg < 0.5 ? T::Warning : T::Neutral);
        t_drop_->set_sub_label(tr("OOS Sharpe = %1 of IS").arg(pct(deg, 0)));
    }

    // OOS COVERAGE
    set_tile(t_cov_, r.oos_coverage, Unit::Fraction, Rule::None, ccy_, tr("Not reported by %1").arg(engine_));
    if (std::isfinite(r.oos_coverage))
        t_cov_->set_value(pct(r.oos_coverage, 0));
    t_cov_->set_sub_label(std::isfinite(r.oos_coverage) ? tr("of the period traded out of sample") : QString());

    // MODE
    const QString mode = r.mode.isEmpty() ? QString() : r.mode.toUpper();
    t_mode_->set_state(mode.isEmpty() ? ui::StatTile::State::Error : ui::StatTile::State::Normal,
                       mode.isEmpty() ? tr("Mode not reported by %1").arg(engine_) : QString());
    t_mode_->set_value(mode.isEmpty() ? dash() : mode);
    t_mode_->set_tone(T::Neutral);
    QStringList sub;
    sub << tr("purge %1").arg(r.purge_bars >= 0 ? QString::number(r.purge_bars) : dash())
        << tr("embargo %1").arg(r.embargo_bars >= 0 ? bars(r.embargo_bars) : dash());
    t_mode_->set_sub_label(sub.join(QStringLiteral(" · ")));
    const int nw = r.n_windows > 0 ? r.n_windows : static_cast<int>(r.windows.size());
    t_mode_->set_tooltip_text(r.combinations_per_window > 0
                                  ? tr("%1 windows × %2 parameter sets = %3 optimisation runs")
                                        .arg(nw)
                                        .arg(r.combinations_per_window)
                                        .arg(count(static_cast<double>(nw) * r.combinations_per_window))
                                  : tr("%1 windows").arg(nw));
}

void BtWalkForwardView::render_folds() {
    const BtWalkForwardResult& r = *result_;
    const int n = static_cast<int>(r.windows.size());
    folds_->clear();
    const QFontMetrics fm(ui::fonts::font(0));
    const qreal thick = std::max<qreal>(4.0, fm.height() * 0.55);
    for (int i = 0; i < n; ++i) {
        const BtWfWindow& w = r.windows.at(i);
        const double y = n - i; // W1 at the top
        const double ts = bt_time_ms(w.train_start);
        const double te = bt_time_ms(w.train_end);
        const double ss = bt_time_ms(w.test_start);
        const double se = bt_time_ms(w.test_end);
        if (std::isfinite(ts) && std::isfinite(te)) {
            ui::TimeSeriesChart::SeriesSpec s;
            s.name = tr("Train (in-sample)");
            s.xs = {ts, te};
            s.ys = {y, y};
            s.color = ui::ChartColorRole::Muted;
            s.line_width = thick;
            s.rebase = false;
            s.show_in_legend = i == 0;
            s.show_in_tooltip = false;
            folds_->add_series(s);
        }
        if (std::isfinite(ss) && std::isfinite(se)) {
            ui::TimeSeriesChart::SeriesSpec s;
            s.name = tr("Test (out-of-sample)");
            s.xs = {ss, se};
            s.ys = {y, y};
            s.color = ui::ChartColorRole::Accent;
            s.line_width = thick;
            s.rebase = false;
            s.show_in_legend = i == 0;
            s.show_in_tooltip = false;
            folds_->add_series(s);
        }
    }
    folds_->set_y_range(0, ui::TimeSeriesChart::Axis::Left, 0.4, n + 0.6);
    folds_->set_value_formatter(
        ui::TimeSeriesChart::Axis::Left,
        [n](double v) {
            const double k = std::round(v);
            if (std::fabs(v - k) > 1e-6 || k < 1 || k > n)
                return QString();
            return wf_window_label(n - static_cast<int>(k) + 1);
        },
        0);
    folds_->reset_x_range();
    bt_analysis_fit_chart(folds_, 4.5 + 1.4 * n);
    QStringList badge;
    if (!r.mode.isEmpty())
        badge << r.mode;
    badge << tr("%1 windows").arg(n);
    if (r.purge_bars > 0 || r.embargo_bars > 0)
        badge << tr("purge %1 · embargo %2").arg(r.purge_bars).arg(r.embargo_bars);
    folds_panel_.header->set_badge(badge.join(QStringLiteral(" · ")));
}

void BtWalkForwardView::render_equity() {
    const BtWalkForwardResult& r = *result_;
    equity_->clear();
    if (r.oos_equity.empty()) {
        equity_->set_state(ui::TimeSeriesChart::State::Empty,
                           tr("%1 did not report the out-of-sample equity curve").arg(engine_));
    } else {
        equity_->set_state(ui::TimeSeriesChart::State::Ready);
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("Out-of-sample equity");
        s.points = to_points(r.oos_equity.t, r.oos_equity.v);
        s.style = ui::TimeSeriesChart::Style::Area;
        s.color = ui::ChartColorRole::Accent;
        s.formatter = [ccy = ccy_](double v) { return money(v, ccy); };
        equity_->add_series(s);
        equity_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, money_formatter(ccy_), 0);
        const double start = r.oos_equity.v.isEmpty() ? kBtNaN : r.oos_equity.v.first();
        if (std::isfinite(start))
            equity_->add_hline(start, tr("Start"), ui::ChartColorRole::Muted);
        for (int i = 0; i < r.windows.size(); ++i) {
            const BtWfWindow& w = r.windows.at(i);
            const double a = bt_time_ms(w.test_start);
            const double b = bt_time_ms(w.test_end);
            if (i % 2 == 1 && std::isfinite(a) && std::isfinite(b))
                equity_->add_band(a, b, ui::ChartColorRole::Muted, wf_window_label(w.window));
        }
        equity_->reset_x_range();
    }
    const double last = r.oos_equity.v.isEmpty() ? kBtNaN : r.oos_equity.v.last();
    equity_panel_.header->set_badge(std::isfinite(last) ? tr("final %1").arg(money_compact(last, ccy_)) : QString());

    QStringList cats;
    QVector<double> is, oos;
    bool any_is = false;
    for (const BtWfWindow& w : r.windows) {
        cats << wf_window_label(w.window);
        is << w.is_return;
        oos << w.oos_return;
        any_is = any_is || std::isfinite(w.is_return);
    }
    inout_->set_categories(cats);
    QVector<ui::BarSeries> series;
    if (any_is)
        series.append(ui::BarSeries{tr("In-sample"), is, QColor()});
    series.append(ui::BarSeries{tr("Out-of-sample"), oos, QColor()});
    inout_->set_series(series);
    inout_->set_show_values(cats.size() <= 8);
}

void BtWalkForwardView::render_windows() {
    const BtWalkForwardResult& r = *result_;
    windows_model_->upsert(r.windows);
    bt_analysis_fit_table(windows_view_, std::clamp(static_cast<int>(r.windows.size()), 3, 12));
    windows_panel_.header->set_badge(tr("%1 windows").arg(r.windows.size()));
}

void BtWalkForwardView::render_stability() {
    const BtWalkForwardResult& r = *result_;
    QStringList names = r.param_stability.keys();
    std::sort(names.begin(), names.end());
    QVector<BtWfParamRow> rows;
    int worst = -1;
    int unstable = 0;
    for (const QString& name : names) {
        const auto& ps = r.param_stability.value(name);
        BtWfParamRow row = wf_param_row(name, ps.first, ps.second);
        worst = std::max(worst, row.verdict);
        if (row.verdict == 2)
            ++unstable;
        rows.append(row);
    }
    const QString prev = stab_view_->selectionModel() && !stab_view_->selectionModel()->selectedRows().isEmpty()
                             ? stab_view_->selectionModel()->selectedRows().first().data(Qt::DisplayRole).toString()
                             : QString();
    stab_model_->upsert(rows);
    bt_analysis_fit_table(stab_view_, std::clamp(static_cast<int>(rows.size()), 2, 8));
    if (rows.isEmpty()) {
        stab_chip_->setText(tr("NOT REPORTED"));
        set_tone(stab_chip_, Tone::Dim);
        stab_note_->setText(tr("%1 did not report the best parameters per window.").arg(engine_));
    } else {
        stab_chip_->setText(worst == 0 ? tr("STABLE") : (worst == 1 ? tr("MIXED") : tr("UNSTABLE")));
        set_tone(stab_chip_, wf_verdict_tone(worst));
        QStringList lines;
        for (const BtWfParamRow& row : rows)
            lines << row.reading;
        QString tail;
        if (unstable > 0)
            tail = tr("Parameters that jump between windows suggest the optimiser is fitting each period's noise; "
                      "prefer a fixed value from the middle of the stable range.");
        else if (worst <= 0)
            tail = tr("The optimiser picked similar values in every window: the edge does not depend on re-tuning.");
        stab_note_->setText(lines.join(QLatin1Char('\n')) + (tail.isEmpty() ? QString() : QStringLiteral("\n") + tail));
    }
    // Keep (or make) a selection so the chart shows one parameter.
    int sel = 0;
    for (int i = 0; i < rows.size(); ++i)
        if (rows.at(i).name == prev)
            sel = i;
    if (!rows.isEmpty()) {
        const QModelIndex idx = stab_model_->index(sel, 0);
        stab_view_->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    render_param_chart();
}

void BtWalkForwardView::render_param_chart() {
    if (!result_)
        return;
    const QModelIndexList sel = stab_view_->selectionModel() ? stab_view_->selectionModel()->selectedRows()
                                                             : QModelIndexList();
    const BtWfParamRow* row = sel.isEmpty() ? nullptr : stab_model_->row_at(sel.first());
    if (!row) {
        param_title_->setText(tr("VALUE BY WINDOW"));
        param_chart_->clear();
        return;
    }
    QStringList cats;
    for (int i = 0; i < row->values.size(); ++i)
        cats << wf_window_label(i < result_->windows.size() ? result_->windows.at(i).window : i + 1);
    param_title_->setText(tr("%1 BY WINDOW").arg(row->name.toUpper()));
    param_chart_->set_data(cats, row->values, row->name);
    param_chart_->set_show_values(cats.size() <= 16);
}

void BtWalkForwardView::window_selected() {
    if (!result_)
        return;
    const QModelIndexList sel = windows_view_->selectionModel()->selectedRows();
    if (sel.isEmpty()) {
        equity_->reset_x_range();
        return;
    }
    const BtWfWindow* w = windows_model_->row_at(windows_proxy_->mapToSource(sel.first()));
    if (!w)
        return;
    const double a = bt_time_ms(w->test_start);
    const double b = bt_time_ms(w->test_end);
    if (std::isfinite(a) && std::isfinite(b) && b > a)
        equity_->set_x_range(a, b);
}

void BtWalkForwardView::refit() {
    // The fold bars' stroke width is set when a series is added: re-add them at the new font size
    // (2 series per window, cheap).
    if (result_ && !result_->windows.isEmpty())
        render_folds();
}

} // namespace fincept::screens::bt
