// src/screens/backtesting/BtOverviewView.cpp
//
// OVERVIEW sub-tab (spec 2.6): notice strip · equity vs benchmark with a drawdown pane, the top-5
// drawdowns as bands and entry / exit markers · snapshot panels · monthly mini heat map, top drawdowns
// and last trades. Built once; show_run() only refills.
#include "screens/backtesting/BtRunViews.h"

#include "ui/charts/ChartExport.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QDate>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens::bt {

using services::backtest::BtDrawdown;
using services::backtest::BtTrade;
using services::backtest::BtWarning;

namespace {

using OvTs = ui::TimeSeriesChart;
using OvDdModel = ui::LiveTableModel<BtDrawdown>;
using OvTradeModel = ui::LiveTableModel<BtTrade>;

constexpr int kOvMarkerAutoCap = 400;  // markers start ON up to this many trades
constexpr int kOvMarkerMax = 250;      // above it, at most the newest 250 trades get markers
constexpr int kOvTopBands = 5;
constexpr int kOvTopDrawdowns = 5;
constexpr int kOvLastTrades = 8;
constexpr int kOvHeatYears = 5;
constexpr int kOvWideChars = 150;      // view width (in average characters) for the side snapshot column
constexpr double kOvDayMs = 86400000.0;
constexpr qint64 kOvUnixJulian = 2440588;

QVariant ov_right() {
    return QVariant((Qt::AlignRight | Qt::AlignVCenter).toInt());
}

/// Epoch ms (UTC) of the first instant of year / month (month 1..12).
double ov_month_start(int year, int month) {
    return static_cast<double>(QDate(year, month, 1).toJulianDay() - kOvUnixJulian) * kOvDayMs;
}

/// [first ms, last ms] of a month (month 1..12) or of the whole year (month 0).
void ov_period_window(int year, int month, double* from, double* to) {
    if (month <= 0) {
        *from = ov_month_start(year, 1);
        *to = ov_month_start(year + 1, 1) - 1.0;
        return;
    }
    *from = ov_month_start(year, month);
    *to = (month == 12 ? ov_month_start(year + 1, 1) : ov_month_start(year, month + 1)) - 1.0;
}

QString ov_period_label(int year, int month) {
    if (month <= 0)
        return QString::number(year);
    return QStringLiteral("%1-%2").arg(year).arg(month, 2, 10, QLatin1Char('0'));
}

/// The "active" property of a #btToolButton (accent look), re-polished only when it changes.
void ov_set_active(QPushButton* b, bool on) {
    if (!b || b->property("active").toBool() == on)
        return;
    b->setProperty("active", on);
    b->style()->unpolish(b);
    b->style()->polish(b);
}

bool ov_is_short(const BtTrade& t) {
    return t.side == QLatin1String("short");
}

QString ov_trade_tip(const BtTrade& t, bool entry, const QString& ccy) {
    QStringList lines;
    if (entry) {
        lines << (ov_is_short(t) ? BtOverviewView::tr("SHORT %1") : BtOverviewView::tr("BUY %1")).arg(t.symbol);
        lines << BtOverviewView::tr("%1 @ %2").arg(quantity(t.quantity), price(t.entry_price, ccy));
        lines << date_text(t.entry_date);
    } else {
        lines << (ov_is_short(t) ? BtOverviewView::tr("COVER %1") : BtOverviewView::tr("SELL %1")).arg(t.symbol);
        lines << BtOverviewView::tr("@ %1 · %2").arg(price(t.exit_price, ccy), date_text(t.exit_date));
        lines << BtOverviewView::tr("P&L %1 (%2)").arg(money(t.pnl, ccy, true), pct(t.pnl_pct, 2, true));
        lines << BtOverviewView::tr("Exit: %1").arg(exit_reason_text(t.exit_reason));
    }
    return lines.join(QLatin1Char('\n'));
}

/// One snapshot row from a metric: "—" with the reason when the engine did not supply it.
BtKv ov_metric(const BtRun& r, const QString& key, const char* path, Unit unit, bool signed_tone) {
    const QString p = QString::fromLatin1(path);
    const double v = r.metric(p);
    BtKv kv{key, format_value(v, unit, r.currency), Tone::None, QString()};
    if (!std::isfinite(v))
        kv.tip = r.why_blank(p);
    else if (signed_tone)
        kv.tone = sign_tone(v);
    return kv;
}

QVector<BtKv> ov_returns_rows(const BtRun& r) {
    QVector<BtKv> rows;
    rows << ov_metric(r, BtOverviewView::tr("Total return"), "performance.totalReturn", Unit::SignedFraction, true);
    rows << ov_metric(r, BtOverviewView::tr("CAGR"), "performance.cagr", Unit::SignedFraction, true);
    const double fin = r.final_equity();
    rows << BtKv{BtOverviewView::tr("Final equity"), money(fin, r.currency), Tone::None,
                 BtOverviewView::tr("Started with %1").arg(money(r.initial_capital(), r.currency))};
    int best_y = 0, worst_y = 0;
    double best = kBtNaN, worst = kBtNaN;
    for (const auto& y : r.yearly) {
        if (!std::isfinite(y.strategy))
            continue;
        if (!std::isfinite(best) || y.strategy > best) {
            best = y.strategy;
            best_y = y.year;
        }
        if (!std::isfinite(worst) || y.strategy < worst) {
            worst = y.strategy;
            worst_y = y.year;
        }
    }
    const QString none = BtOverviewView::tr("No calendar-year returns in this run");
    rows << BtKv{BtOverviewView::tr("Best year"),
                 std::isfinite(best) ? BtOverviewView::tr("%1 (%2)").arg(pct(best, 1, true)).arg(best_y) : dash(),
                 sign_tone(best), std::isfinite(best) ? QString() : none};
    rows << BtKv{BtOverviewView::tr("Worst year"),
                 std::isfinite(worst) ? BtOverviewView::tr("%1 (%2)").arg(pct(worst, 1, true)).arg(worst_y) : dash(),
                 sign_tone(worst), std::isfinite(worst) ? QString() : none};
    double pos = r.metric(QStringLiteral("periodStats.positiveMonths"));
    if (!std::isfinite(pos)) {
        int n = 0, up = 0;
        for (const auto& m : r.monthly)
            for (double v : m.months)
                if (std::isfinite(v)) {
                    ++n;
                    up += v > 0 ? 1 : 0;
                }
        pos = n > 0 ? static_cast<double>(up) / n : kBtNaN;
    }
    rows << BtKv{BtOverviewView::tr("Positive months"), pct(pos, 0), Tone::None,
                 std::isfinite(pos) ? QString() : BtOverviewView::tr("No monthly returns in this run")};
    return rows;
}

QVector<BtKv> ov_risk_rows(const BtRun& r) {
    QVector<BtKv> rows;
    const double mdd = r.metric(QStringLiteral("performance.maxDrawdown"));
    QString mdd_text = pct(std::isfinite(mdd) ? -std::fabs(mdd) : kBtNaN, 2, true);
    if (std::isfinite(mdd) && !r.drawdowns.isEmpty() && !r.drawdowns.first().trough_date.isEmpty())
        mdd_text = BtOverviewView::tr("%1 · %2").arg(mdd_text, date_text(r.drawdowns.first().trough_date));
    rows << BtKv{BtOverviewView::tr("Max drawdown"), mdd_text,
                 std::isfinite(mdd) && std::fabs(mdd) >= 0.10 ? Tone::Warn : Tone::None,
                 std::isfinite(mdd) ? BtOverviewView::tr("Deepest fall from a peak (trough date)")
                                    : r.why_blank(QStringLiteral("performance.maxDrawdown"))};
    double longest = r.metric(QStringLiteral("performance.maxDrawdownDuration"));
    if (!std::isfinite(longest) || longest < 0) {
        longest = kBtNaN;
        for (const BtDrawdown& d : r.drawdowns)
            if (d.duration_bars >= 0 && (!std::isfinite(longest) || d.duration_bars > longest))
                longest = d.duration_bars;
    }
    rows << BtKv{BtOverviewView::tr("Longest drawdown"), bars(longest), Tone::None, QString()};
    rows << ov_metric(r, BtOverviewView::tr("Volatility"), "performance.volatility", Unit::Fraction, false);
    BtKv sharpe = ov_metric(r, BtOverviewView::tr("Sharpe"), "performance.sharpeRatio", Unit::Ratio, false);
    const double sv = r.metric(QStringLiteral("performance.sharpeRatio"));
    sharpe.tone = std::isfinite(sv) && sv < 0 ? Tone::Warn : Tone::None;
    rows << sharpe;
    rows << ov_metric(r, BtOverviewView::tr("Sortino"), "performance.sortinoRatio", Unit::Ratio, false);
    rows << ov_metric(r, BtOverviewView::tr("VaR 95 (1 bar)"), "advancedMetrics.var95", Unit::SignedFraction, false);
    rows << ov_metric(r, BtOverviewView::tr("CVaR 95 (1 bar)"), "advancedMetrics.cvar95", Unit::SignedFraction, false);
    return rows;
}

QVector<BtKv> ov_trade_rows(const BtRun& r) {
    QVector<BtKv> rows;
    QString closed = count(r.closed_trades());
    if (r.open_trades > 0)
        closed = BtOverviewView::tr("%1 + %2 open").arg(closed, count(r.open_trades));
    rows << BtKv{BtOverviewView::tr("Closed trades"), closed, Tone::None, QString()};
    rows << ov_metric(r, BtOverviewView::tr("Win rate"), "tradeStats.winRate", Unit::Fraction, false);
    if (r.metric(QStringLiteral("tradeStats.profitFactorInfinite")) > 0.5) {
        rows << BtKv{BtOverviewView::tr("Profit factor"), QStringLiteral("∞"), Tone::None,
                     r.why_blank(QStringLiteral("tradeStats.profitFactor"))};
    } else {
        BtKv pf = ov_metric(r, BtOverviewView::tr("Profit factor"), "tradeStats.profitFactor", Unit::Ratio, false);
        const double v = r.metric(QStringLiteral("tradeStats.profitFactor"));
        pf.tone = std::isfinite(v) && v < 1.0 ? Tone::Warn : Tone::None;
        rows << pf;
    }
    rows << ov_metric(r, BtOverviewView::tr("Expectancy"), "tradeStats.expectancy", Unit::SignedMoney, true);
    rows << ov_metric(r, BtOverviewView::tr("Avg hold"), "tradeStats.avgHoldingBars", Unit::Bars, false);
    rows << ov_metric(r, BtOverviewView::tr("Payoff ratio"), "tradeStats.payoffRatio", Unit::Ratio, false);
    return rows;
}

QVector<BtKv> ov_bench_rows(const BtRun& r) {
    QVector<BtKv> rows;
    const QString sym = r.benchmark_symbol.isEmpty() ? BtOverviewView::tr("Benchmark") : r.benchmark_symbol.toUpper();
    rows << ov_metric(r, BtOverviewView::tr("%1 return").arg(sym), "benchmark.totalReturn", Unit::SignedFraction, true);
    const double tr_s = r.metric(QStringLiteral("performance.totalReturn"));
    const double tr_b = r.metric(QStringLiteral("benchmark.totalReturn"));
    const double ex = (std::isfinite(tr_s) && std::isfinite(tr_b)) ? tr_s - tr_b : kBtNaN;
    rows << BtKv{BtOverviewView::tr("Excess return"), pp(ex), sign_tone(ex),
                 std::isfinite(ex) ? BtOverviewView::tr("Total return minus the benchmark's, percentage points")
                                   : r.why_blank(QStringLiteral("benchmark.totalReturn"))};
    rows << ov_metric(r, BtOverviewView::tr("Alpha"), "performance.alpha", Unit::SignedFraction, true);
    rows << ov_metric(r, BtOverviewView::tr("Beta"), "performance.beta", Unit::Ratio, false);
    rows << ov_metric(r, BtOverviewView::tr("Information ratio"), "performance.informationRatio", Unit::Ratio, false);
    rows << ov_metric(r, BtOverviewView::tr("Correlation"), "advancedMetrics.benchmarkCorrelation", Unit::Ratio, false);
    return rows;
}

QVector<BtKv> ov_exposure_rows(const BtRun& r) {
    QVector<BtKv> rows;
    rows << ov_metric(r, BtOverviewView::tr("Time in market"), "exposure.timeInMarket", Unit::Fraction, false);
    rows << ov_metric(r, BtOverviewView::tr("Avg gross exposure"), "exposure.avgGrossExposure", Unit::Fraction, false);
    const QString ccy = r.currency;
    rows << BtKv{BtOverviewView::tr("Total costs"), money(r.costs.total_costs, ccy), Tone::None,
                 std::isfinite(r.costs.total_costs) ? QString() : r.why_blank(QStringLiteral("costs.totalCosts"))};
    rows << BtKv{BtOverviewView::tr("Cost drag / yr"), pct(r.costs.cost_drag, 2), Tone::None,
                 std::isfinite(r.costs.cost_drag) ? BtOverviewView::tr("Annual return lost to costs")
                                                  : r.why_blank(QStringLiteral("costs.costDrag"))};
    const double to = r.costs.annual_turnover;
    rows << BtKv{BtOverviewView::tr("Annual turnover"),
                 std::isfinite(to) ? BtOverviewView::tr("%1×").arg(ratio(to, 1)) : dash(), Tone::None,
                 std::isfinite(to) ? BtOverviewView::tr("Traded notional per year, in multiples of equity")
                                   : r.why_blank(QStringLiteral("costs.annualTurnover"))};
    return rows;
}

QVariant ov_dd_cell(const BtDrawdown& d, int col, int role) {
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case 0:
                    return QString::number(d.rank);
                case 1:
                    return pct(std::isfinite(d.depth) ? -d.depth : kBtNaN, 1, true);
                case 2:
                    return date_text(d.peak_date);
                case 3:
                    return d.recovered() ? date_text(d.recovery_date) : BtOverviewView::tr("ongoing");
                case 4:
                    return bars(d.duration_bars);
                default:
                    break;
            }
            break;
        case Qt::ForegroundRole:
            if (col == 1 && std::isfinite(d.depth) && d.depth >= 0.10)
                return tone_color(Tone::Warn);
            if (col == 3 && !d.recovered())
                return tone_color(Tone::Warn);
            break;
        case Qt::TextAlignmentRole:
            if (col == 0 || col == 1 || col == 4)
                return ov_right();
            break;
        case Qt::ToolTipRole:
            return BtOverviewView::tr("Drawdown #%1: %2 from the %3 peak to the %4 trough · click for RISK")
                .arg(d.rank)
                .arg(pct(d.depth, 2), date_text(d.peak_date), date_text(d.trough_date));
        default:
            break;
    }
    return {};
}

QVariant ov_trade_cell(const BtTrade& t, int col, int role, const QString& ccy) {
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case 0:
                    return t.symbol;
                case 1:
                    return ov_is_short(t) ? BtOverviewView::tr("SHORT") : BtOverviewView::tr("LONG");
                case 2:
                    return t.open ? BtOverviewView::tr("OPEN") : date_text(t.exit_date);
                case 3:
                    return money(t.pnl, ccy, true);
                case 4:
                    return pct(t.pnl_pct, 2, true);
                default:
                    break;
            }
            break;
        case Qt::ForegroundRole:
            if (col == 2 && t.open)
                return tone_color(Tone::Warn);
            if (col == 3 || col == 4) {
                const Tone tn = sign_tone(col == 3 ? t.pnl : t.pnl_pct);
                if (tn != Tone::None)
                    return tone_color(tn);
            }
            break;
        case Qt::TextAlignmentRole:
            if (col >= 3)
                return ov_right();
            break;
        case Qt::ToolTipRole:
            if (t.open)
                return BtOverviewView::tr("%1 open since %2: unrealised P&L marked at %3 · click for TRADES")
                    .arg(t.symbol, date_text(t.entry_date), price(t.mark_price, ccy));
            return BtOverviewView::tr("%1 %2 → %3 · exit: %4 · click for TRADES")
                .arg(t.symbol, date_text(t.entry_date), date_text(t.exit_date), exit_reason_text(t.exit_reason));
        default:
            break;
    }
    return {};
}

void ov_fit_table(QTableView* view, int rows) {
    if (!view)
        return;
    const int row_h = view->verticalHeader()->defaultSectionSize();
    const int head = view->horizontalHeader()->sizeHint().height();
    view->setMinimumHeight(head + row_h * rows + 2 * view->frameWidth() + 2);
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

BtOverviewView::BtOverviewView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    // Notice strip (warn / error items of warnings[] and data disclosures).
    notice_ = new QFrame(this);
    notice_->setObjectName(QStringLiteral("btPanel"));
    notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* nl = new QHBoxLayout(notice_);
    nl->setContentsMargins(8, 4, 8, 4);
    nl->setSpacing(8);
    notice_chip_ = make_chip(QString(), Tone::Warn, notice_);
    nl->addWidget(notice_chip_, 0, Qt::AlignVCenter);
    notice_text_ = make_note(QString(), notice_, Tone::None);
    nl->addWidget(notice_text_, 1, Qt::AlignVCenter);
    auto* view_btn = make_tool_button(tr("VIEW"), tr("Open the assumptions and data behind these results"), notice_);
    connect(view_btn, &QPushButton::clicked, this, [this]() {
        if (selection_)
            emit selection_->tab_requested(QStringLiteral("assumptions"));
    });
    nl->addWidget(view_btn, 0, Qt::AlignVCenter);
    notice_->setVisible(false);
    root->addWidget(notice_);

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
    root->addWidget(scroll_, 1);

    // ── Equity chart ──
    chart_panel_ = make_panel(tr("EQUITY"), content_, 4);
    trades_btn_ = make_tool_button(tr("TRADES"),
                                   tr("Entry / exit markers on the equity line. Off by default above %1 trades; "
                                      "then the newest %2 trades are marked.")
                                       .arg(kOvMarkerAutoCap)
                                       .arg(kOvMarkerMax),
                                   chart_panel_.header);
    trades_btn_->setCheckable(true);
    chart_panel_.header->add_control(trades_btn_);
    connect(trades_btn_, &QPushButton::toggled, this, [this](bool on) {
        ov_set_active(trades_btn_, on);
        rebuild_annotations();
    });
    chart_ = make_time_chart(chart_panel_.body_widget, QStringLiteral("equity_curve"));
    chart_->set_show_mode_buttons(true);
    chart_->set_range_presets({QStringLiteral("1M"), QStringLiteral("3M"), QStringLiteral("6M"), QStringLiteral("YTD"),
                               QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("ALL")});
    dd_pane_ = chart_->add_pane(0.30);
    chart_->set_pane_title(dd_pane_, tr("Drawdown"));
    chart_->set_value_formatter(OvTs::Axis::Left, [](double v) { return ui::formatting::pct(v, 0, false); }, dd_pane_);
    chart_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    chart_panel_.body->addWidget(chart_, 1);
    connect(chart_, &OvTs::crosshair_moved, this, [this](double x) {
        if (selection_)
            emit selection_->crosshair_moved(x, this);
    });
    connect(chart_, &OvTs::marker_clicked, this, [this](int id) {
        const int ti = marker_trade_.value(id, -1);
        if (!run_ || ti < 0 || ti >= run_->trades.size())
            return;
        sel_trade_ = run_->trades.at(ti).id;
        rebuild_annotations();
        if (selection_)
            emit selection_->trade_selected(sel_trade_, this);
    });

    // ── Snapshot panels ──
    snapshot_ = new QWidget(content_);
    snapshot_grid_ = new QGridLayout(snapshot_);
    snapshot_grid_->setContentsMargins(0, 0, 0, 0);
    snapshot_grid_->setHorizontalSpacing(6);
    snapshot_grid_->setVerticalSpacing(6);
    const QStringList titles = {tr("RETURNS"), tr("RISK"), tr("TRADES"), tr("VS BENCHMARK"), tr("EXPOSURE · COSTS")};
    for (const QString& t : titles) {
        BtPanel p = make_panel(t, snapshot_, 8);
        auto* kv = new BtKvGrid(p.body_widget, 1);
        p.body->addWidget(kv);
        p.body->addStretch(1);
        snap_panels_.append(p);
        snap_kv_.append(kv);
    }

    // ── Bottom row ──
    bottom_ = new QWidget(content_);
    bottom_grid_ = new QGridLayout(bottom_);
    bottom_grid_->setContentsMargins(0, 0, 0, 0);
    bottom_grid_->setHorizontalSpacing(6);
    bottom_grid_->setVerticalSpacing(6);

    BtPanel mp = make_panel(tr("MONTHLY · LAST %1 YEARS").arg(kOvHeatYears), bottom_, 4);
    mini_heat_ = new ui::MonthlyReturnsHeatmap(mp.body_widget);
    mini_heat_->set_show_year_total(true);
    mini_heat_->setToolTip(tr("Click a month for PERFORMANCE; the charts zoom to it and TRADES is filtered to it"));
    {
        ui::ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("monthly_returns_recent");
        ui::ChartExport::install_context_menu(mini_heat_, spec);
    }
    mp.body->addWidget(mini_heat_, 1);
    connect(mini_heat_, &ui::MonthlyReturnsHeatmap::cell_clicked, this, [this](int y, int m) { open_month(y, m); });
    bottom_panels_.append(mp);

    BtPanel dp = make_panel(tr("TOP DRAWDOWNS"), bottom_, 0);
    dd_table_ = new QTableView(dp.body_widget);
    setup_table(dd_table_, QStringLiteral("top_drawdowns"));
    dd_table_->setSortingEnabled(false);
    dd_table_->horizontalHeader()->setSortIndicatorShown(false);
    dd_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* ddm = new OvDdModel(this);
    ddm->set_headers({tr("#"), tr("DEPTH"), tr("PEAK"), tr("RECOVERY"), tr("LENGTH")});
    ddm->set_cell([](const BtDrawdown& d, int c, int role) { return ov_dd_cell(d, c, role); });
    dd_model_ = ddm;
    dd_table_->setModel(ddm);
    set_column_chars(dd_table_, {2, 7, 10, 10, 9});
    ui::formatting::refresh_model_on_format_change(ddm);
    const auto dd_open = [this](const QModelIndex& idx) {
        const auto* m = static_cast<OvDdModel*>(dd_model_);
        if (const BtDrawdown* d = m->row_at(idx))
            open_drawdown(d->rank);
    };
    connect(dd_table_, &QTableView::clicked, this, dd_open);
    connect(dd_table_, &QTableView::activated, this, dd_open);
    dp.body->addWidget(dd_table_, 1);
    bottom_panels_.append(dp);

    BtPanel lp = make_panel(tr("LAST TRADES"), bottom_, 0);
    last_table_ = new QTableView(lp.body_widget);
    setup_table(last_table_, QStringLiteral("last_trades"));
    last_table_->setSortingEnabled(false);
    last_table_->horizontalHeader()->setSortIndicatorShown(false);
    last_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* ltm = new OvTradeModel(this);
    ltm->set_headers({tr("SYMBOL"), tr("SIDE"), tr("EXIT"), tr("P&L"), tr("P&L %")});
    ltm->set_cell([this](const BtTrade& t, int c, int role) {
        return ov_trade_cell(t, c, role, run_ ? run_->currency : QString());
    });
    last_model_ = ltm;
    last_table_->setModel(ltm);
    set_column_chars(last_table_, {7, 5, 10, 11, 7});
    ui::formatting::refresh_model_on_format_change(ltm);
    const auto trade_open = [this](const QModelIndex& idx) {
        const auto* m = static_cast<OvTradeModel*>(last_model_);
        if (const BtTrade* t = m->row_at(idx))
            open_trade(t->id);
    };
    connect(last_table_, &QTableView::clicked, this, trade_open);
    connect(last_table_, &QTableView::activated, this, trade_open);
    lp.body->addWidget(last_table_, 1);
    bottom_panels_.append(lp);

    // ── Cross-highlight ──
    if (selection_) {
        connect(selection_, &BtSelection::time_range_selected, this, [this](double a, double b, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (std::isfinite(a) && std::isfinite(b))
                chart_->set_x_range(a, b);
            else
                chart_->reset_x_range();
        });
        connect(selection_, &BtSelection::trade_selected, this, [this](const QString& id, QObject* origin) {
            if (bt_view_owns_origin(this, origin) || id == sel_trade_)
                return;
            sel_trade_ = id;
            rebuild_annotations();
            if (!id.isEmpty())
                zoom_to_trade(id);
        });
        connect(selection_, &BtSelection::drawdown_selected, this, [this](int rank, QObject* origin) {
            if (bt_view_owns_origin(this, origin) || rank == sel_dd_)
                return;
            sel_dd_ = rank;
            rebuild_annotations();
        });
        connect(selection_, &BtSelection::crosshair_moved, this, [this](double x, QObject* origin) {
            if (!bt_view_owns_origin(this, origin))
                chart_->set_linked_crosshair(x);
        });
    }

    // A format change re-renders the text; the chart keeps the user's zoom.
    ui::formatting::on_format_changed(this, [this]() {
        const double a = chart_->x_from(), b = chart_->x_to();
        const bool zoomed = run_ && run_->t.size() >= 2 && std::isfinite(a) && std::isfinite(b) &&
                            (a > run_->t.first() || b < run_->t.last());
        render();
        if (zoomed)
            chart_->set_x_range(a, b);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        fit_to_font();
        relayout(width(), true);
    });
    fit_to_font();
    relayout(width(), true);
}

// ── Layout ──────────────────────────────────────────────────────────────────

void BtOverviewView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(e->size().width(), false);
}

void BtOverviewView::relayout(int width, bool force) {
    const QFontMetrics fm(ui::fonts::font(0));
    const int ch = std::max(5, fm.averageCharWidth());
    const bool wide = width >= kOvWideChars * ch;
    const int mode = wide ? 1 : 0;
    const int panels = static_cast<int>(snap_panels_.size());
    const int snap_cols = wide ? 1 : std::clamp(width / (ch * 34), 1, panels);
    const int snap_w = wide ? width * 32 / 100 : width / std::max(1, snap_cols);
    const int kv_cols = snap_w >= ch * 58 ? 2 : 1;
    const int bottom_cols = (wide || width >= ch * 120) ? 3 : (width >= ch * 76 ? 2 : 1);
    if (!force && mode == layout_mode_ && snap_cols == snapshot_cols_ && bottom_cols == bottom_cols_ &&
        kv_cols == kv_cols_)
        return;

    if (force || mode != layout_mode_) {
        grid_->removeWidget(chart_panel_.frame);
        grid_->removeWidget(snapshot_);
        grid_->removeWidget(bottom_);
        for (int i = 0; i < 3; ++i) {
            grid_->setRowStretch(i, 0);
            grid_->setColumnStretch(i, 0);
        }
        if (wide) {
            grid_->addWidget(chart_panel_.frame, 0, 0);
            grid_->addWidget(bottom_, 1, 0);
            grid_->addWidget(snapshot_, 0, 1, 2, 1);
            grid_->setColumnStretch(0, 68);
            grid_->setColumnStretch(1, 32);
        } else {
            grid_->addWidget(chart_panel_.frame, 0, 0);
            grid_->addWidget(snapshot_, 1, 0);
            grid_->addWidget(bottom_, 2, 0);
            grid_->setColumnStretch(0, 1);
        }
        grid_->setRowStretch(0, 1);
        layout_mode_ = mode;
    }

    if (force || snap_cols != snapshot_cols_) {
        for (const BtPanel& p : snap_panels_)
            snapshot_grid_->removeWidget(p.frame);
        for (int i = 0; i < 6; ++i) {
            snapshot_grid_->setRowStretch(i, 0);
            snapshot_grid_->setColumnStretch(i, 0);
        }
        for (int i = 0; i < panels; ++i) {
            // The last panel spans what is left of its row, so a short last row leaves no blank cell.
            const int span = i == panels - 1 ? snap_cols - i % snap_cols : 1;
            snapshot_grid_->addWidget(snap_panels_[i].frame, i / snap_cols, i % snap_cols, 1, span);
        }
        for (int c = 0; c < snap_cols; ++c)
            snapshot_grid_->setColumnStretch(c, 1);
        const int rows = (panels + snap_cols - 1) / snap_cols;
        for (int r = 0; r < rows; ++r)
            snapshot_grid_->setRowStretch(r, 1);
        snapshot_cols_ = snap_cols;
    }

    if (force || kv_cols != kv_cols_) {
        for (BtKvGrid* kv : snap_kv_)
            kv->set_columns(kv_cols);
        kv_cols_ = kv_cols;
    }

    if (force || bottom_cols != bottom_cols_) {
        for (const BtPanel& p : bottom_panels_)
            bottom_grid_->removeWidget(p.frame);
        for (int i = 0; i < 3; ++i) {
            bottom_grid_->setRowStretch(i, 0);
            bottom_grid_->setColumnStretch(i, 0);
        }
        const int n = static_cast<int>(bottom_panels_.size());
        for (int i = 0; i < n; ++i) {
            if (bottom_cols == 2 && i == n - 1 && n % 2 == 1)
                bottom_grid_->addWidget(bottom_panels_[i].frame, i / 2, 0, 1, 2); // the odd one spans: no blank half
            else
                bottom_grid_->addWidget(bottom_panels_[i].frame, i / bottom_cols, i % bottom_cols);
        }
        for (int c = 0; c < bottom_cols; ++c)
            bottom_grid_->setColumnStretch(c, 1);
        bottom_cols_ = bottom_cols;
    }
}

void BtOverviewView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    chart_->setMinimumHeight(fm.height() * 20);
    ov_fit_table(dd_table_, kOvTopDrawdowns + 1);
    ov_fit_table(last_table_, kOvLastTrades);
    layout_mode_ = -1;
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtOverviewView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    sel_trade_.clear();
    sel_dd_ = 0;
    marker_order_.clear();
    {
        const QSignalBlocker block(trades_btn_);
        const bool on = rp && rp->trades.size() <= kOvMarkerAutoCap;
        trades_btn_->setChecked(on);
        ov_set_active(trades_btn_, on);
    }
    render();
}

void BtOverviewView::render() {
    render_notice();
    render_chart();
    render_snapshot();
    render_bottom();
}

void BtOverviewView::render_notice() {
    const BtRun* r = run_.get();
    if (!r) {
        notice_->setVisible(false);
        return;
    }
    int serious = 0;
    bool error = false;
    QString first;
    for (const BtWarning& w : r->warnings) {
        const int rank = services::backtest::bt_warning_rank(w.severity);
        if (rank > 1)
            continue;
        ++serious;
        error = error || rank == 0;
        if (first.isEmpty())
            first = w.symbol.isEmpty() ? w.message : tr("%1: %2").arg(w.symbol, w.message);
    }
    const int disclosures = static_cast<int>(r->data_disclosures.size());
    if (serious == 0 && disclosures == 0) {
        notice_->setVisible(false);
        return;
    }
    if (first.isEmpty())
        first = r->data_disclosures.first();
    const int total = serious + disclosures;
    if (serious > 0) {
        notice_chip_->setText(serious == 1 ? tr("1 WARNING") : tr("%1 WARNINGS").arg(serious));
        set_tone(notice_chip_, error ? Tone::Danger : Tone::Warn);
    } else {
        notice_chip_->setText(tr("DATA NOTE"));
        set_tone(notice_chip_, Tone::Accent);
    }
    notice_text_->setText(total > 1 ? tr("%1  ·  +%2 more").arg(first).arg(total - 1) : first);
    notice_->setVisible(true);
}

void BtOverviewView::render_chart() {
    chart_->clear();
    marker_trade_.clear();
    eq_series_ = -1;
    const BtRun* r = run_.get();
    const bool bench = r && r->has_benchmark();
    const QString bsym = r ? r->benchmark_symbol.toUpper() : QString();
    chart_panel_.header->set_title(bench && !bsym.isEmpty() ? tr("EQUITY VS %1").arg(bsym) : tr("EQUITY"));
    if (!r || r->t.size() < 2 || r->equity.size() < 2) {
        chart_panel_.header->set_badge(QString());
        chart_->set_state(ui::ChartState::Empty, r ? tr("This run has no equity curve") : tr("No backtest yet"));
        return;
    }
    chart_->set_state(ui::ChartState::Ready);
    chart_panel_.header->set_badge(tr("%1 → %2 · %3 bars")
                                       .arg(date_text(r->dates.value(0)), date_text(r->dates.value(r->dates.size() - 1)),
                                            count(static_cast<double>(r->t.size()))));
    chart_->set_range_presets(
        r->intraday ? QStringList{QStringLiteral("1D"), QStringLiteral("1W"), QStringLiteral("1M"), QStringLiteral("3M"),
                                  QStringLiteral("ALL")}
                    : QStringList{QStringLiteral("1M"), QStringLiteral("3M"), QStringLiteral("6M"), QStringLiteral("YTD"),
                                  QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("ALL")});
    const QString ccy = r->currency;
    const auto money_fmt = [ccy](double v) { return money(v, ccy); };
    chart_->set_value_formatter(OvTs::Axis::Left, money_formatter(ccy), 0);

    OvTs::SeriesSpec es;
    es.name = tr("Strategy");
    es.points = to_points(r->t, r->equity);
    es.color = ui::ChartColorRole::Accent;
    es.line_width = 1.8;
    es.id = QStringLiteral("equity");
    es.formatter = money_fmt;
    eq_series_ = chart_->add_series(es);

    if (bench) {
        OvTs::SeriesSpec bs;
        bs.name = bsym.isEmpty() ? tr("Benchmark") : bsym;
        bs.points = to_points(r->t, r->benchmark);
        bs.color = ui::ChartColorRole::Muted;
        bs.dashed = true;
        bs.line_width = 1.2;
        bs.id = QStringLiteral("benchmark");
        bs.formatter = money_fmt;
        chart_->add_series(bs);
    }

    OvTs::SeriesSpec ds;
    ds.name = tr("Drawdown");
    ds.points = to_points(r->t, r->drawdown, 100.0);
    ds.style = OvTs::Style::Area;
    ds.pane = dd_pane_;
    ds.color = ui::ChartColorRole::Warning;
    ds.baseline = 0.0;
    ds.rebase = false;
    ds.id = QStringLiteral("drawdown");
    ds.formatter = [](double v) { return ui::formatting::pct(v, 2, false); };
    chart_->add_series(ds);

    rebuild_annotations();
}

void BtOverviewView::rebuild_annotations() {
    chart_->clear_annotations();
    marker_trade_.clear();
    const BtRun* r = run_.get();
    if (!r || eq_series_ < 0 || r->t.isEmpty())
        return;
    const double last_t = r->t.last();
    const double start = r->initial_capital();
    if (std::isfinite(start))
        chart_->add_hline(start, tr("Start"), ui::ChartColorRole::Muted);

    // Top drawdowns as faint bands; the selected one stands out.
    const int nd = std::min(kOvTopBands, static_cast<int>(r->drawdowns.size()));
    for (int i = 0; i < nd; ++i) {
        const BtDrawdown& d = r->drawdowns.at(i);
        if (d.rank == sel_dd_ || !std::isfinite(d.peak_t))
            continue;
        const double end = d.recovered() && std::isfinite(d.recovery_t) ? d.recovery_t : last_t;
        chart_->add_band(d.peak_t, end, ui::ChartColorRole::Muted, tr("#%1").arg(d.rank));
    }
    if (sel_dd_ > 0) {
        for (const BtDrawdown& d : r->drawdowns) {
            if (d.rank != sel_dd_ || !std::isfinite(d.peak_t))
                continue;
            const double end = d.recovered() && std::isfinite(d.recovery_t) ? d.recovery_t : last_t;
            chart_->add_band(d.peak_t, end, ui::ChartColorRole::Warning,
                             tr("Drawdown #%1 · %2").arg(d.rank).arg(pct(-d.depth, 1, true)));
            break;
        }
    }

    // The selected trade.
    if (!sel_trade_.isEmpty()) {
        for (const BtTrade& t : r->trades) {
            if (t.id != sel_trade_)
                continue;
            if (std::isfinite(t.entry_t)) {
                const double end = std::isfinite(t.exit_t) && !t.open ? t.exit_t : last_t;
                chart_->add_band(t.entry_t, std::max(end, t.entry_t + 1.0), ui::ChartColorRole::Accent,
                                 tr("%1 %2").arg(t.symbol, ov_is_short(t) ? tr("SHORT") : tr("LONG")));
            }
            break;
        }
    }

    // Entry / exit markers.
    if (!trades_btn_->isChecked() || r->trades.isEmpty())
        return;
    const int n = static_cast<int>(r->trades.size());
    QVector<int> picks;
    if (n <= kOvMarkerAutoCap) {
        picks.resize(n);
        std::iota(picks.begin(), picks.end(), 0);
    } else {
        if (marker_order_.size() != n) {
            marker_order_.resize(n);
            std::iota(marker_order_.begin(), marker_order_.end(), 0);
            const auto& tr_ = r->trades;
            std::sort(marker_order_.begin(), marker_order_.end(), [&tr_](int a, int b) {
                const double ta = tr_.at(a).entry_t, tb = tr_.at(b).entry_t;
                if (!std::isfinite(ta))
                    return false;
                if (!std::isfinite(tb))
                    return true;
                return ta > tb;
            });
        }
        picks = marker_order_.mid(0, kOvMarkerMax);
    }
    const QString ccy = r->currency;
    for (int i : picks) {
        const BtTrade& t = r->trades.at(i);
        const bool shorted = ov_is_short(t);
        if (std::isfinite(t.entry_t)) {
            const int id = chart_->add_marker(t.entry_t, eq_series_,
                                              shorted ? ui::ChartMarkerKind::Sell : ui::ChartMarkerKind::Buy, QString(),
                                              ov_trade_tip(t, true, ccy));
            marker_trade_.insert(id, i);
        }
        if (!t.open && std::isfinite(t.exit_t)) {
            const int id = chart_->add_marker(t.exit_t, eq_series_,
                                              shorted ? ui::ChartMarkerKind::Buy : ui::ChartMarkerKind::Sell, QString(),
                                              ov_trade_tip(t, false, ccy));
            marker_trade_.insert(id, i);
        }
    }
}

void BtOverviewView::render_snapshot() {
    const BtRun* r = run_.get();
    if (!r) {
        for (BtKvGrid* kv : snap_kv_)
            kv->set_rows({});
        snap_panels_[3].header->set_title(tr("VS BENCHMARK"));
        for (const BtPanel& p : snap_panels_)
            p.header->set_badge(QString());
        return;
    }
    snap_kv_[0]->set_rows(ov_returns_rows(*r));
    snap_kv_[1]->set_rows(ov_risk_rows(*r));
    snap_kv_[2]->set_rows(ov_trade_rows(*r));
    snap_kv_[3]->set_rows(ov_bench_rows(*r));
    snap_kv_[4]->set_rows(ov_exposure_rows(*r));
    const double yrs = r->years();
    snap_panels_[0].header->set_badge(std::isfinite(yrs) ? tr("%1 y").arg(num(yrs, 1)) : QString());
    snap_panels_[2].header->set_badge(count(static_cast<double>(r->trades.size())));
    snap_panels_[3].header->set_title(r->benchmark_symbol.isEmpty() ? tr("VS BENCHMARK")
                                                                    : tr("VS %1").arg(r->benchmark_symbol.toUpper()));
    snap_panels_[3].header->set_badge(r->has_benchmark() ? QString() : tr("none"));
}

void BtOverviewView::render_bottom() {
    const BtRun* r = run_.get();
    auto* ddm = static_cast<OvDdModel*>(dd_model_);
    auto* ltm = static_cast<OvTradeModel*>(last_model_);
    if (!r) {
        mini_heat_->clear();
        mini_heat_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
        ddm->upsert({});
        ltm->upsert({});
        bottom_panels_[1].header->set_badge(QString());
        bottom_panels_[2].header->set_badge(QString());
        return;
    }

    // Monthly mini heat map: the last five calendar years, percent units.
    ui::MonthlyReturnsHeatmap::MonthlyReturns heat;
    int last_year = 0;
    for (const auto& m : r->monthly)
        last_year = std::max(last_year, m.year);
    for (const auto& m : r->monthly) {
        if (m.year <= last_year - kOvHeatYears)
            continue;
        for (int i = 0; i < 12; ++i)
            if (std::isfinite(m.months[i]))
                heat[m.year][i + 1] = m.months[i] * 100.0;
    }
    if (heat.isEmpty()) {
        mini_heat_->clear();
        mini_heat_->set_state(ui::PaintedChart::Empty, tr("No monthly returns in this run"));
    } else {
        mini_heat_->set_state(ui::PaintedChart::Ready);
        mini_heat_->set_monthly_returns(heat);
    }

    // Top drawdowns.
    ddm->upsert(r->drawdowns.mid(0, kOvTopDrawdowns));
    bottom_panels_[1].header->set_badge(r->drawdowns_computed ? tr("from the equity curve") : QString());

    // Last trades: newest entries first, open positions marked.
    const int n = static_cast<int>(r->trades.size());
    QVector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    const int take = std::min(kOvLastTrades, n);
    const auto& trs = r->trades;
    std::partial_sort(idx.begin(), idx.begin() + take, idx.end(), [&trs](int a, int b) {
        const double ta = trs.at(a).entry_t, tb = trs.at(b).entry_t;
        if (!std::isfinite(ta))
            return false;
        if (!std::isfinite(tb))
            return true;
        return ta > tb;
    });
    QVector<BtTrade> last;
    last.reserve(take);
    for (int i = 0; i < take; ++i)
        last.append(trs.at(idx[i]));
    ltm->upsert(last);
    bottom_panels_[2].header->set_badge(r->open_trades > 0 ? tr("%1 open").arg(r->open_trades) : QString());
}

// ── Actions ─────────────────────────────────────────────────────────────────

void BtOverviewView::open_month(int year, int month) {
    if (year <= 0)
        return;
    double from = 0, to = 0;
    ov_period_window(year, month, &from, &to);
    chart_->set_x_range(from, to);
    if (!selection_)
        return;
    emit selection_->month_selected(year, month, this);
    emit selection_->time_range_selected(from, to, this);
    emit selection_->trades_filter_requested(from, to, tr("in %1").arg(ov_period_label(year, month)), this);
    emit selection_->tab_requested(QStringLiteral("performance"));
}

void BtOverviewView::open_drawdown(int rank) {
    if (!run_)
        return;
    sel_dd_ = rank;
    rebuild_annotations();
    for (const BtDrawdown& d : run_->drawdowns) {
        if (d.rank != rank)
            continue;
        double from = 0, to = 0;
        if (BtRiskView::drawdown_window(*run_, d, &from, &to))
            chart_->set_x_range(from, to);
        break;
    }
    BtRiskView::select_drawdown(selection_, *run_, rank, this);
    if (selection_)
        emit selection_->tab_requested(QStringLiteral("risk"));
}

void BtOverviewView::open_trade(const QString& trade_id) {
    if (!run_ || trade_id.isEmpty())
        return;
    sel_trade_ = trade_id;
    rebuild_annotations();
    zoom_to_trade(trade_id);
    if (!selection_)
        return;
    emit selection_->trade_selected(trade_id, this);
    emit selection_->tab_requested(QStringLiteral("trades"));
}

void BtOverviewView::zoom_to_trade(const QString& trade_id) {
    if (!run_)
        return;
    for (const BtTrade& t : run_->trades) {
        if (t.id != trade_id)
            continue;
        double from = 0, to = 0;
        if (BtTradesView::trade_window(*run_, t, &from, &to))
            chart_->set_x_range(from, to);
        return;
    }
}

} // namespace fincept::screens::bt
