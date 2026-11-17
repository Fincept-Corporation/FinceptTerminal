// src/screens/backtesting/BtRiskView.cpp
//
// RISK sub-tab (spec 2.6): tail / downside KPI row · underwater curve (selected drawdown banded) ·
// top-10 drawdowns table · tail distribution of the worst 10 % of bar returns with the always-visible
// "historical, not a forecast" note. A KPI the engine did not report is computed here from the bar
// returns when possible and says so in its sub-label.
#include "screens/backtesting/BtRunViews.h"

#include "ui/charts/ChartExport.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::BtDrawdown;

namespace {

using RkTs = ui::TimeSeriesChart;
using RkDdModel = ui::LiveTableModel<BtDrawdown>;

constexpr int kRkTwoColumnChars = 100;
constexpr int kRkDepthRole = Qt::EditRole;

enum RkCol { kRkRank, kRkDepth, kRkPeak, kRkTrough, kRkRecovery, kRkToTrough, kRkToRecover, kRkTotal, kRkColCount };

QVariant rk_right() {
    return QVariant((Qt::AlignRight | Qt::AlignVCenter).toInt());
}

/// numpy-style linear-interpolation quantile of an ascending vector.
double rk_quantile(const QVector<double>& sorted, double q) {
    const int n = static_cast<int>(sorted.size());
    if (n == 0)
        return kBtNaN;
    if (n == 1)
        return sorted.first();
    const double pos = q * (n - 1);
    const int i = static_cast<int>(std::floor(pos));
    const double f = pos - i;
    return i + 1 < n ? sorted[i] + f * (sorted[i + 1] - sorted[i]) : sorted[n - 1];
}

/// Mean of the values at or below `cut` (CVaR of an ascending vector).
double rk_tail_mean(const QVector<double>& sorted, double cut) {
    double sum = 0;
    int n = 0;
    for (double x : sorted) {
        if (x > cut)
            break;
        sum += x;
        ++n;
    }
    return n > 0 ? sum / n : kBtNaN;
}

QVariant rk_dd_cell(const BtDrawdown& d, int col, int role) {
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case kRkRank:
                    return QString::number(d.rank);
                case kRkDepth:
                    return pct(std::isfinite(d.depth) ? -d.depth : kBtNaN, 2, true);
                case kRkPeak:
                    return date_text(d.peak_date);
                case kRkTrough:
                    return date_text(d.trough_date);
                case kRkRecovery:
                    return d.recovered() ? date_text(d.recovery_date) : BtRiskView::tr("ONGOING");
                case kRkToTrough:
                    return bars(d.to_trough_bars);
                case kRkToRecover:
                    return d.recovered() ? bars(d.recovery_bars) : dash();
                case kRkTotal: {
                    const QString b = bars(d.duration_bars);
                    return d.duration_days >= 0 ? BtRiskView::tr("%1 · %2 d").arg(b, count(d.duration_days)) : b;
                }
                default:
                    break;
            }
            break;
        case kRkDepthRole:
            if (col == kRkDepth && std::isfinite(d.depth))
                return d.depth;
            break;
        case RkDdModel::SortRole:
            switch (col) {
                case kRkRank:
                    return d.rank;
                case kRkDepth:
                    return std::isfinite(d.depth) ? d.depth : 0.0;
                case kRkPeak:
                    return d.peak_t;
                case kRkTrough:
                    return d.trough_t;
                case kRkRecovery:
                    return d.recovered() ? d.recovery_t : 1e300;
                case kRkToTrough:
                    return d.to_trough_bars;
                case kRkToRecover:
                    return d.recovered() ? d.recovery_bars : 1 << 30;
                case kRkTotal:
                    return d.duration_bars;
                default:
                    break;
            }
            break;
        case Qt::ForegroundRole:
            if (col == kRkRecovery && !d.recovered())
                return tone_color(Tone::Warn);
            break;
        case Qt::TextAlignmentRole:
            if (col == kRkRank || col == kRkDepth || col >= kRkToTrough)
                return rk_right();
            break;
        case Qt::ToolTipRole:
            if (col == kRkRecovery && !d.recovered())
                return BtRiskView::tr("Not recovered by the end of the run");
            if (col == kRkDepth)
                return BtRiskView::tr("Peak equity %1 → trough %2").arg(num(d.peak_equity, 2), num(d.trough_equity, 2));
            return BtRiskView::tr("Select to zoom the charts to this drawdown and filter TRADES to it");
        default:
            break;
    }
    return {};
}

/// Fill `tile` from the engine's metric, else from `computed` (sub-label "computed here"), else "—".
void rk_tile(ui::StatTile* tile, const BtRun& r, const QString& path, double computed, Unit unit, Rule rule,
             const QString& tip) {
    const double v = r.metric(path);
    tile->set_tooltip_text(tip);
    if (std::isfinite(v)) {
        set_tile(tile, v, unit, rule, r.currency);
        tile->set_sub_label(QString());
    } else if (std::isfinite(computed)) {
        set_tile(tile, computed, unit, rule, r.currency);
        tile->set_sub_label(BtRiskView::tr("computed here"));
    } else {
        set_tile(tile, kBtNaN, unit, rule, r.currency, r.why_blank(path));
        tile->set_sub_label(QString());
    }
}

} // namespace

// ── Static helpers ──────────────────────────────────────────────────────────

bool BtRiskView::drawdown_window(const BtRun& run, const BtDrawdown& dd, double* from_ms, double* to_ms) {
    if (!std::isfinite(dd.peak_t) || !from_ms || !to_ms)
        return false;
    double end = dd.recovered() && std::isfinite(dd.recovery_t) ? dd.recovery_t
                                                                : (run.t.isEmpty() ? dd.trough_t : run.t.last());
    if (!std::isfinite(end) || end < dd.peak_t)
        end = dd.peak_t;
    const int n = static_cast<int>(run.t.size());
    const double bar = n >= 2 ? (run.t.last() - run.t.first()) / (n - 1) : 86400000.0;
    const double span = std::max(end - dd.peak_t, bar);
    *from_ms = dd.peak_t - 0.1 * span;
    *to_ms = end + 0.1 * span;
    return true;
}

void BtRiskView::select_drawdown(BtSelection* bus, const BtRun& run, int rank, QObject* origin) {
    if (!bus)
        return;
    emit bus->drawdown_selected(rank, origin);
    for (const BtDrawdown& d : run.drawdowns) {
        if (d.rank != rank)
            continue;
        double from = 0, to = 0;
        if (!drawdown_window(run, d, &from, &to))
            return;
        emit bus->time_range_selected(from, to, origin);
        const double end = d.recovered() && std::isfinite(d.recovery_t) ? d.recovery_t
                                                                        : (run.t.isEmpty() ? d.trough_t : run.t.last());
        emit bus->trades_filter_requested(d.peak_t, end, tr("during drawdown #%1").arg(rank), origin);
        return;
    }
}

// ── Construction ────────────────────────────────────────────────────────────

BtRiskView::BtRiskView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("btScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content_ = new QWidget(scroll_);
    grid_ = new QGridLayout(content_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(6);
    grid_->setVerticalSpacing(6);
    scroll_->setWidget(content_);
    root->addWidget(scroll_);

    // ── KPI row ──
    kpis_ = new ui::KpiStrip(content_);
    const QList<QPair<QString, QString>> tiles = {
        {QStringLiteral("var95"), tr("VAR 95")},          {QStringLiteral("cvar95"), tr("CVAR 95")},
        {QStringLiteral("var99"), tr("VAR 99")},          {QStringLiteral("cvar99"), tr("CVAR 99")},
        {QStringLiteral("ddev"), tr("DOWNSIDE DEV")},     {QStringLiteral("ulcer"), tr("ULCER INDEX")},
        {QStringLiteral("skew"), tr("SKEW")},             {QStringLiteral("kurt"), tr("EXCESS KURTOSIS")},
        {QStringLiteral("tail"), tr("TAIL RATIO")},       {QStringLiteral("worst"), tr("WORST BAR")},
    };
    for (const auto& t : tiles)
        kpis_->add_tile(t.first, t.second)->set_compact(true);

    // ── Underwater ──
    uw_panel_ = make_panel(tr("UNDERWATER"), content_, 4);
    underwater_ = make_time_chart(uw_panel_.body_widget, QStringLiteral("underwater"));
    underwater_->set_range_presets({QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("ALL")});
    underwater_->set_value_formatter(RkTs::Axis::Left, [](double v) { return ui::formatting::pct(v, 0, false); }, 0);
    underwater_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    uw_panel_.body->addWidget(underwater_, 1);
    connect(underwater_, &RkTs::crosshair_moved, this, [this](double x) {
        if (selection_)
            emit selection_->crosshair_moved(x, this);
    });

    // ── Drawdowns table ──
    dd_panel_ = make_panel(tr("DRAWDOWNS"), content_, 0);
    dd_table_ = new QTableView(dd_panel_.body_widget);
    setup_table(dd_table_, QStringLiteral("drawdowns"));
    dd_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* model = new RkDdModel(this);
    model->set_headers({tr("#"), tr("DEPTH"), tr("PEAK"), tr("TROUGH"), tr("RECOVERY"), tr("TO TROUGH"),
                        tr("TO RECOVER"), tr("TOTAL")});
    model->set_cell([](const BtDrawdown& d, int c, int role) { return rk_dd_cell(d, c, role); });
    dd_model_ = model;
    dd_proxy_ = make_sort_proxy(model, this);
    dd_table_->setModel(dd_proxy_);
    depth_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, dd_table_);
    depth_delegate_->set_heat_sequential(true);
    depth_delegate_->set_value_role(kRkDepthRole);
    depth_delegate_->set_range(0.0, 0.25);
    dd_table_->setItemDelegateForColumn(kRkDepth, depth_delegate_);
    set_column_chars(dd_table_, {3, 9, 11, 11, 11, 10, 11, 16});
    dd_table_->sortByColumn(kRkRank, Qt::AscendingOrder);
    ui::formatting::refresh_model_on_format_change(model);
    dd_panel_.body->addWidget(dd_table_, 1);
    dd_empty_ = make_note(tr("No drawdown: the equity curve never fell below a previous peak."), dd_panel_.body_widget);
    dd_empty_->setAlignment(Qt::AlignCenter);
    dd_empty_->setVisible(false);
    dd_panel_.body->addWidget(dd_empty_, 1);
    connect(dd_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (syncing_ || !run_ || !current.isValid())
                    return;
                const auto* m = static_cast<RkDdModel*>(dd_model_);
                const BtDrawdown* d = m->row_at(dd_proxy_->mapToSource(current));
                if (!d)
                    return;
                mark_drawdown(d->rank, true);
                select_drawdown(selection_, *run_, d->rank, this);
            });

    // ── Tail ──
    tail_panel_ = make_panel(tr("TAIL · WORST 10 % OF BARS"), content_, 6);
    tail_ = new ui::DistributionChart(tail_panel_.body_widget);
    tail_->set_signed_colors(true);
    tail_->set_value_formatter([](double v) { return ui::formatting::pct(v, 2, true); });
    {
        ui::ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("return_tail");
        ui::ChartExport::install_context_menu(tail_, spec);
    }
    tail_panel_.body->addWidget(tail_, 1);
    tail_kv_ = new BtKvGrid(tail_panel_.body_widget, 2);
    tail_panel_.body->addWidget(tail_kv_);
    tail_note_ = make_note(QString(), tail_panel_.body_widget, Tone::Warn);
    tail_panel_.body->addWidget(tail_note_);

    // ── Cross-highlight ──
    if (selection_) {
        connect(selection_, &BtSelection::time_range_selected, this, [this](double a, double b, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (std::isfinite(a) && std::isfinite(b))
                underwater_->set_x_range(a, b);
            else
                underwater_->reset_x_range();
        });
        connect(selection_, &BtSelection::drawdown_selected, this, [this](int rank, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            mark_drawdown(rank, false);
            // Select the row without re-emitting.
            const auto* m = static_cast<RkDdModel*>(dd_model_);
            const QSignalBlocker block(dd_table_->selectionModel());
            syncing_ = true;
            dd_table_->clearSelection();
            for (int i = 0; i < m->rows().size(); ++i) {
                if (m->rows().at(i).rank != rank)
                    continue;
                const QModelIndex pi = dd_proxy_->mapFromSource(m->index(i, 0));
                dd_table_->selectionModel()->setCurrentIndex(pi, QItemSelectionModel::ClearAndSelect |
                                                                     QItemSelectionModel::Rows);
                dd_table_->scrollTo(pi);
                break;
            }
            syncing_ = false;
            dd_table_->viewport()->update();
        });
        connect(selection_, &BtSelection::crosshair_moved, this, [this](double x, QObject* origin) {
            if (!bt_view_owns_origin(this, origin))
                underwater_->set_linked_crosshair(x);
        });
    }

    // A format change re-renders the text; the underwater chart keeps the user's zoom.
    ui::formatting::on_format_changed(this, [this]() {
        render_kpis();
        render_tail();
        mark_drawdown(sel_rank_, false);
        underwater_->set_value_formatter(RkTs::Axis::Left, [](double v) { return ui::formatting::pct(v, 0, false); }, 0);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        fit_to_font();
        relayout(width(), true);
    });
    fit_to_font();
    relayout(width(), true);
}

void BtRiskView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(e->size().width(), false);
}

void BtRiskView::relayout(int width, bool force) {
    const QFontMetrics fm(ui::fonts::font(0));
    const int mode = width >= kRkTwoColumnChars * std::max(5, fm.averageCharWidth()) ? 1 : 0;
    if (!force && mode == layout_mode_)
        return;
    layout_mode_ = mode;
    for (QWidget* w : {static_cast<QWidget*>(kpis_), static_cast<QWidget*>(uw_panel_.frame),
                       static_cast<QWidget*>(dd_panel_.frame), static_cast<QWidget*>(tail_panel_.frame)})
        grid_->removeWidget(w);
    for (int i = 0; i < 5; ++i) {
        grid_->setRowStretch(i, 0);
        grid_->setColumnStretch(i, 0);
    }
    if (mode == 1) {
        grid_->addWidget(kpis_, 0, 0, 1, 2);
        grid_->addWidget(uw_panel_.frame, 1, 0, 1, 2);
        grid_->addWidget(dd_panel_.frame, 2, 0);
        grid_->addWidget(tail_panel_.frame, 2, 1);
        grid_->setColumnStretch(0, 3);
        grid_->setColumnStretch(1, 2);
        grid_->setRowStretch(1, 4);
        grid_->setRowStretch(2, 5);
    } else {
        grid_->addWidget(kpis_, 0, 0);
        grid_->addWidget(uw_panel_.frame, 1, 0);
        grid_->addWidget(dd_panel_.frame, 2, 0);
        grid_->addWidget(tail_panel_.frame, 3, 0);
        grid_->setColumnStretch(0, 1);
        grid_->setRowStretch(1, 4);
        grid_->setRowStretch(2, 4);
        grid_->setRowStretch(3, 4);
    }
}

void BtRiskView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    underwater_->setMinimumHeight(fm.height() * 13);
    const int row_h = dd_table_->verticalHeader()->defaultSectionSize();
    dd_table_->setMinimumHeight(dd_table_->horizontalHeader()->sizeHint().height() + row_h * 10 + 4);
    tail_->setMinimumHeight(fm.height() * 11);
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtRiskView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    sel_rank_ = 0;
    sorted_returns_.clear();
    if (rp) {
        const int n = static_cast<int>(rp->returns.size());
        sorted_returns_.reserve(n);
        for (int i = 1; i < n; ++i)
            if (std::isfinite(rp->returns[i]))
                sorted_returns_.append(rp->returns[i]);
        std::sort(sorted_returns_.begin(), sorted_returns_.end());
    }
    render();
}

void BtRiskView::render() {
    render_kpis();
    render_underwater();
    render_table();
    render_tail();
}

void BtRiskView::render_kpis() {
    const BtRun* r = run_.get();
    if (!r) {
        for (int i = 0; i < kpis_->count(); ++i) {
            ui::StatTile* t = kpis_->tile(i);
            t->set_value(dash());
            t->set_sub_label(QString());
            t->set_tone(ui::StatTile::Tone::Neutral);
            t->set_state(ui::StatTile::State::Normal);
        }
        return;
    }
    const QVector<double>& s = sorted_returns_;
    const int n = static_cast<int>(s.size());
    const double var95 = rk_quantile(s, 0.05), var99 = rk_quantile(s, 0.01);
    const double cvar95 = rk_tail_mean(s, var95), cvar99 = rk_tail_mean(s, var99);
    const double ppy = std::isfinite(r->periods_per_year) && r->periods_per_year > 0 ? r->periods_per_year : 252.0;
    double dsum = 0, mean = 0, m2 = 0, m3 = 0, m4 = 0;
    for (double x : s) {
        mean += x;
        const double neg = std::min(x, 0.0);
        dsum += neg * neg;
    }
    mean = n > 0 ? mean / n : kBtNaN;
    for (double x : s) {
        const double d = x - mean;
        m2 += d * d;
        m3 += d * d * d;
        m4 += d * d * d * d;
    }
    double ddev = kBtNaN, skew = kBtNaN, kurt = kBtNaN;
    if (n >= 3) {
        ddev = std::sqrt(dsum / n) * std::sqrt(ppy);
        m2 /= n;
        m3 /= n;
        m4 /= n;
        if (m2 > 0) {
            skew = m3 / std::pow(m2, 1.5);
            kurt = m4 / (m2 * m2) - 3.0;
        }
    }
    const double p95 = rk_quantile(s, 0.95);
    const double tail = (std::isfinite(var95) && std::fabs(var95) > 1e-10) ? std::fabs(p95 / var95) : kBtNaN;
    double ulcer = kBtNaN;
    {
        double sq = 0;
        int k = 0;
        for (double d : r->drawdown)
            if (std::isfinite(d)) {
                sq += d * d;
                ++k;
            }
        if (k > 0)
            ulcer = std::sqrt(sq / k);
    }
    const double ulcer_metric = r->metric(QStringLiteral("advancedMetrics.ulcerIndexFraction"));
    const double ulcer_pp = r->metric(QStringLiteral("advancedMetrics.ulcerIndex"));
    const QString bar = r->intraday ? tr("bar") : tr("day");

    rk_tile(kpis_->tile(QStringLiteral("var95")), *r, QStringLiteral("advancedMetrics.var95"), var95,
            Unit::SignedFraction, Rule::None, tr("1-bar historical Value at Risk: the 5th percentile of bar returns"));
    rk_tile(kpis_->tile(QStringLiteral("cvar95")), *r, QStringLiteral("advancedMetrics.cvar95"), cvar95,
            Unit::SignedFraction, Rule::None, tr("Mean of the bar returns at or below VaR 95 (expected shortfall)"));
    rk_tile(kpis_->tile(QStringLiteral("var99")), *r, QStringLiteral("advancedMetrics.var99"), var99,
            Unit::SignedFraction, Rule::None, tr("1-bar historical Value at Risk: the 1st percentile of bar returns"));
    rk_tile(kpis_->tile(QStringLiteral("cvar99")), *r, QStringLiteral("advancedMetrics.cvar99"), cvar99,
            Unit::SignedFraction, Rule::None, tr("Mean of the bar returns at or below VaR 99"));
    rk_tile(kpis_->tile(QStringLiteral("ddev")), *r, QStringLiteral("performance.downsideDeviation"), ddev,
            Unit::Fraction, Rule::None, tr("Annualised deviation of the negative bar returns"));
    rk_tile(kpis_->tile(QStringLiteral("ulcer")), *r, QStringLiteral("advancedMetrics.ulcerIndexFraction"),
            std::isfinite(ulcer_metric) ? ulcer_metric : (std::isfinite(ulcer_pp) ? ulcer_pp / 100.0 : ulcer),
            Unit::Fraction, Rule::None, tr("Root-mean-square drawdown: depth and duration of the losses together"));
    if (!std::isfinite(ulcer_metric) && std::isfinite(ulcer_pp))
        kpis_->tile(QStringLiteral("ulcer"))->set_sub_label(QString());
    rk_tile(kpis_->tile(QStringLiteral("skew")), *r, QStringLiteral("advancedMetrics.skewness"), skew, Unit::Ratio,
            Rule::None, tr("Skewness of the bar returns (negative = a longer left tail)"));
    rk_tile(kpis_->tile(QStringLiteral("kurt")), *r, QStringLiteral("advancedMetrics.kurtosis"), kurt, Unit::Ratio,
            Rule::None, tr("Excess kurtosis of the bar returns (0 = normal; higher = fatter tails)"));
    rk_tile(kpis_->tile(QStringLiteral("tail")), *r, QStringLiteral("advancedMetrics.tailRatio"), tail, Unit::Ratio,
            Rule::BelowOne, tr("|95th percentile| / |5th percentile| of the bar returns (< 1: losses outsize gains)"));
    rk_tile(kpis_->tile(QStringLiteral("worst")), *r, QStringLiteral("periodStats.worstDay"),
            s.isEmpty() ? kBtNaN : s.first(), Unit::SignedFraction, Rule::Sign,
            tr("Worst single %1 return of the run").arg(bar));
}

void BtRiskView::render_underwater() {
    underwater_->clear();
    const BtRun* r = run_.get();
    if (!r || r->t.size() < 2 || r->drawdown.isEmpty()) {
        underwater_->set_state(ui::ChartState::Empty, r ? tr("This run has no equity curve") : tr("No backtest yet"));
        uw_panel_.header->set_badge(QString());
        return;
    }
    underwater_->set_state(ui::ChartState::Ready);
    RkTs::SeriesSpec s;
    s.name = tr("Drawdown");
    s.points = to_points(r->t, r->drawdown, 100.0);
    s.style = RkTs::Style::Area;
    s.color = ui::ChartColorRole::Warning;
    s.baseline = 0.0;
    s.rebase = false;
    s.formatter = [](double v) { return ui::formatting::pct(v, 2, false); };
    underwater_->add_series(s);
    double below = 0;
    int k = 0;
    for (double d : r->drawdown)
        if (std::isfinite(d)) {
            ++k;
            below += d < -1e-9 ? 1 : 0;
        }
    uw_panel_.header->set_badge(k > 0 ? tr("%1 of bars below a peak").arg(pct(below / k, 0)) : QString());
    mark_drawdown(sel_rank_, false);
}

void BtRiskView::mark_drawdown(int rank, bool zoom) {
    sel_rank_ = rank;
    underwater_->clear_annotations();
    const BtRun* r = run_.get();
    if (!r)
        return;
    const double mdd = r->metric(QStringLiteral("performance.maxDrawdown"));
    if (std::isfinite(mdd))
        underwater_->add_hline(-std::fabs(mdd) * 100.0, tr("Max DD %1").arg(pct(-std::fabs(mdd), 1, true)),
                               ui::ChartColorRole::Muted);
    if (rank <= 0)
        return;
    for (const BtDrawdown& d : r->drawdowns) {
        if (d.rank != rank)
            continue;
        const double end = d.recovered() && std::isfinite(d.recovery_t) ? d.recovery_t
                                                                        : (r->t.isEmpty() ? d.trough_t : r->t.last());
        if (std::isfinite(d.peak_t) && std::isfinite(end))
            underwater_->add_band(d.peak_t, std::max(end, d.peak_t + 1.0), ui::ChartColorRole::Warning,
                                  tr("#%1 · %2").arg(d.rank).arg(pct(-d.depth, 1, true)));
        double from = 0, to = 0;
        if (zoom && drawdown_window(*r, d, &from, &to))
            underwater_->set_x_range(from, to);
        return;
    }
}

void BtRiskView::render_table() {
    auto* m = static_cast<RkDdModel*>(dd_model_);
    const BtRun* r = run_.get();
    syncing_ = true;
    m->upsert(r ? r->drawdowns : QVector<BtDrawdown>());
    syncing_ = false;
    double deepest = 0;
    if (r)
        for (const BtDrawdown& d : r->drawdowns)
            if (std::isfinite(d.depth))
                deepest = std::max(deepest, d.depth);
    depth_delegate_->set_range(0.0, deepest > 0 ? deepest : 0.25);
    const bool none = r && r->drawdowns.isEmpty();
    dd_table_->setVisible(!none);
    dd_empty_->setVisible(none);
    if (!r) {
        dd_panel_.header->set_badge(QString());
        return;
    }
    int ongoing = 0;
    for (const BtDrawdown& d : r->drawdowns)
        ongoing += d.recovered() ? 0 : 1;
    QString badge = tr("top %1 by depth").arg(r->drawdowns.size());
    if (ongoing > 0)
        badge += tr(" · %1 ongoing").arg(ongoing);
    if (r->drawdowns_computed)
        badge += tr(" · from the equity curve");
    dd_panel_.header->set_badge(badge);
}

void BtRiskView::render_tail() {
    tail_->clear_markers();
    const BtRun* r = run_.get();
    const QVector<double>& s = sorted_returns_;
    const int n = static_cast<int>(s.size());
    if (!r || n < 10) {
        tail_->clear();
        tail_->set_state(ui::PaintedChart::Empty, r ? tr("Too few bar returns for a tail") : tr("No backtest yet"));
        tail_kv_->set_rows({});
        tail_note_->setText(r ? tr("1-bar historical VaR / CVaR from %1 returns; not a forecast.").arg(count(n))
                              : QString());
        return;
    }
    const int k = std::max(2, static_cast<int>(std::ceil(n * 0.10)));
    QVector<double> worst = s.mid(0, k);
    double tail_sum = 0;
    for (double& x : worst) {
        tail_sum += x;
        x *= 100.0;
    }
    tail_->set_state(ui::PaintedChart::Ready);
    tail_->set_samples(worst);
    const auto pick = [r](const char* path, double computed) {
        const double v = r->metric(QString::fromLatin1(path));
        return std::isfinite(v) ? v : computed;
    };
    const double var95 = pick("advancedMetrics.var95", rk_quantile(s, 0.05));
    const double cvar95 = pick("advancedMetrics.cvar95", rk_tail_mean(s, rk_quantile(s, 0.05)));
    const double var99 = pick("advancedMetrics.var99", rk_quantile(s, 0.01));
    if (std::isfinite(var95))
        tail_->add_marker(var95 * 100.0, tr("VaR 95"), ui::DistributionChart::MarkerRole::VaR);
    if (std::isfinite(cvar95))
        tail_->add_marker(cvar95 * 100.0, tr("CVaR 95"), ui::DistributionChart::MarkerRole::CVaR);
    if (std::isfinite(var99))
        tail_->add_marker(var99 * 100.0, tr("VaR 99"), ui::DistributionChart::MarkerRole::VaR);

    // Worst bar and its date.
    int wi = -1;
    for (int i = 1; i < r->returns.size(); ++i)
        if (std::isfinite(r->returns[i]) && (wi < 0 || r->returns[i] < r->returns[wi]))
            wi = i;
    const QString worst_text = wi >= 0 ? tr("%1 · %2").arg(pct(r->returns[wi], 2, true), date_text(r->dates.value(wi)))
                                       : dash();
    int below = 0;
    for (double x : s) {
        if (!std::isfinite(var95) || x > var95)
            break;
        ++below;
    }
    tail_kv_->set_rows({
        {tr("Worst bar"), worst_text, sign_tone(s.first()), QString()},
        {tr("Mean of worst 10 %"), pct(tail_sum / k, 2, true), sign_tone(tail_sum), QString()},
        {tr("1 % quantile"), pct(rk_quantile(s, 0.01), 2, true), Tone::None, QString()},
        {tr("5 % quantile"), pct(rk_quantile(s, 0.05), 2, true), Tone::None, QString()},
        {tr("Bars at or below VaR 95"), count(below), Tone::None, QString()},
        {tr("Bars in the tail"), count(k), Tone::None, QString()},
    });
    tail_panel_.header->set_badge(tr("%1 of %2 bars").arg(count(k), count(n)));
    tail_note_->setText(tr("1-bar historical VaR / CVaR from %1 returns; not a forecast.").arg(count(n)));
}

} // namespace fincept::screens::bt
