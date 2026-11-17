// src/screens/backtesting/BtCostsView.cpp
//
// COSTS sub-tab (spec 2.6): GROSS RESULT notice when nothing was charged · cost KPIs · gross -> net
// waterfall (gross P&L, −commission, −slippage, −borrow, other, = net) · by year [COSTS | TURNOVER |
// NOTIONAL] (costs.byYear, or computed from the trades) · the cost model with what is not modelled.
// Turnover and traded notional are always shown, so a zero-cost run still says what costs act on.
#include "screens/backtesting/BtRunViews.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QDate>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::BtCostModel;
using services::backtest::BtCostYear;
using services::backtest::BtCosts;
using services::backtest::BtTrade;

namespace {

constexpr double kCoDayMs = 86400000.0;
constexpr qint64 kCoUnixJulian = 2440588;
constexpr int kCoTwoColumnChars = 110;

int co_year(double t_ms) {
    return QDate::fromJulianDay(static_cast<qint64>(std::floor(t_ms / kCoDayMs)) + kCoUnixJulian).year();
}

double co_or0(double v) {
    return std::isfinite(v) ? v : 0.0;
}

/// Notional traded by a trade: |qty| x (entry + exit) prices (one leg while open).
double co_trade_notional(const BtTrade& t) {
    if (!std::isfinite(t.quantity) || !std::isfinite(t.entry_price))
        return kBtNaN;
    const double q = std::fabs(t.quantity);
    double n = q * std::fabs(t.entry_price);
    if (!t.open && std::isfinite(t.exit_price))
        n += q * std::fabs(t.exit_price);
    return n;
}

double co_traded_notional(const BtRun& r) {
    if (std::isfinite(r.costs.traded_notional))
        return r.costs.traded_notional;
    double sum = 0;
    bool any = false;
    for (const BtTrade& t : r.trades) {
        const double n = co_trade_notional(t);
        if (std::isfinite(n)) {
            sum += n;
            any = true;
        }
    }
    return any ? sum : kBtNaN;
}

double co_avg_equity(const BtRun& r) {
    double s = 0;
    int n = 0;
    for (double e : r.equity)
        if (std::isfinite(e)) {
            s += e;
            ++n;
        }
    return n > 0 ? s / n : kBtNaN;
}

/// Per calendar year from the trades (exit year; entry year while open), turnover against the year's
/// average equity.
QVector<BtCostYear> co_years_from_trades(const BtRun& r) {
    QMap<int, BtCostYear> by;
    for (const BtTrade& t : r.trades) {
        const double at = (!t.open && std::isfinite(t.exit_t)) ? t.exit_t : t.entry_t;
        if (!std::isfinite(at))
            continue;
        BtCostYear& y = by[co_year(at)];
        y.year = co_year(at);
        if (std::isfinite(t.commission))
            y.commission = co_or0(y.commission) + t.commission;
        if (std::isfinite(t.slippage))
            y.slippage = co_or0(y.slippage) + t.slippage;
        const double n = co_trade_notional(t);
        if (std::isfinite(n))
            y.notional = co_or0(y.notional) + n;
    }
    QMap<int, QPair<double, int>> eq;
    const int m = static_cast<int>(std::min(r.t.size(), r.equity.size()));
    for (int i = 0; i < m; ++i) {
        if (!std::isfinite(r.t[i]) || !std::isfinite(r.equity[i]))
            continue;
        auto& e = eq[co_year(r.t[i])];
        e.first += r.equity[i];
        e.second += 1;
    }
    QVector<BtCostYear> out;
    for (auto it = by.begin(); it != by.end(); ++it) {
        BtCostYear y = it.value();
        const auto e = eq.value(y.year, {0.0, 0});
        if (std::isfinite(y.notional) && e.second > 0 && e.first != 0)
            y.turnover = y.notional / (e.first / e.second);
        out.append(y);
    }
    return out;
}

QString co_commission_text(const BtCostModel& m, const QString& ccy) {
    if (!std::isfinite(m.commission_value))
        return m.commission_type.isEmpty() ? dash() : m.commission_type;
    if (m.commission_type == QLatin1String("pct_notional"))
        return BtCostsView::tr("%1 of notional per fill").arg(ui::formatting::bp(m.commission_value * 1e4, 1, false));
    if (m.commission_type == QLatin1String("per_share"))
        return BtCostsView::tr("%1 per share").arg(ui::formatting::money_precise(m.commission_value, ccy));
    if (m.commission_type == QLatin1String("per_trade"))
        return BtCostsView::tr("%1 per fill").arg(money(m.commission_value, ccy));
    return BtCostsView::tr("%1 (%2)").arg(num(m.commission_value, 4), m.commission_type);
}

QString co_slippage_text(const BtCostModel& m) {
    if (m.slippage_type == QLatin1String("none"))
        return BtCostsView::tr("none");
    if (!std::isfinite(m.slippage_value))
        return m.slippage_type.isEmpty() ? dash() : m.slippage_type;
    if (m.slippage_type == QLatin1String("pct_price"))
        return BtCostsView::tr("%1 of the price per fill").arg(ui::formatting::bp(m.slippage_value * 1e4, 1, false));
    if (m.slippage_type == QLatin1String("vol_scaled"))
        return BtCostsView::tr("%1 × daily volatility (20 bars)").arg(num(m.slippage_value, 2));
    return BtCostsView::tr("%1 (%2)").arg(num(m.slippage_value, 4), m.slippage_type);
}

QString co_fill_text(const BtRun& r) {
    const auto& a = r.assumptions;
    if (!a.fill_description.isEmpty())
        return a.fill_description;
    if (a.fill_at == QLatin1String("next_open"))
        return BtCostsView::tr("next bar's open");
    if (a.fill_at == QLatin1String("next_close"))
        return BtCostsView::tr("next bar's close");
    if (a.fill_at == QLatin1String("same_close"))
        return BtCostsView::tr("the signal bar's close");
    return dash();
}

/// A tile with custom text: `text` when `v` is finite, else "—" with `why`.
void co_text_tile(ui::StatTile* tile, double v, const QString& text, const QString& sub, const QString& why) {
    if (std::isfinite(v)) {
        tile->set_state(ui::StatTile::State::Normal);
        tile->set_tone(ui::StatTile::Tone::Neutral);
        tile->set_value(text);
        tile->set_sub_label(sub);
    } else {
        set_tile(tile, kBtNaN, Unit::Plain, Rule::None, QString(), why);
        tile->set_sub_label(QString());
    }
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

BtCostsView::BtCostsView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
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

    notice_ = new QFrame(content_);
    notice_->setObjectName(QStringLiteral("btPanel"));
    notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* nl = new QHBoxLayout(notice_);
    nl->setContentsMargins(8, 4, 8, 4);
    nl->setSpacing(8);
    nl->addWidget(make_chip(tr("GROSS RESULT"), Tone::Warn, notice_), 0, Qt::AlignVCenter);
    notice_text_ = make_note(QString(), notice_, Tone::None);
    nl->addWidget(notice_text_, 1);
    notice_->setVisible(false);

    kpis_ = new ui::KpiStrip(content_);
    const QList<QPair<QString, QString>> tiles = {
        {QStringLiteral("total"), tr("TOTAL COSTS")},       {QStringLiteral("commission"), tr("COMMISSION")},
        {QStringLiteral("slippage"), tr("SLIPPAGE")},       {QStringLiteral("share"), tr("COSTS / GROSS PROFIT")},
        {QStringLiteral("drag"), tr("COST DRAG")},          {QStringLiteral("turnover"), tr("ANNUAL TURNOVER")},
        {QStringLiteral("notional"), tr("TRADED NOTIONAL")}, {QStringLiteral("fills"), tr("FILLS")},
    };
    for (const auto& t : tiles)
        kpis_->add_tile(t.first, t.second)->set_compact(true);

    // Gross -> net
    wf_panel_ = make_panel(tr("GROSS → NET"), content_, 4);
    waterfall_ = new ui::BarChart(wf_panel_.body_widget);
    waterfall_->set_mode(ui::BarChart::Mode::Waterfall);
    waterfall_->set_show_values(true);
    waterfall_->set_waterfall_total(true, tr("Net P&L"));
    waterfall_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
    wf_panel_.body->addWidget(waterfall_, 1);
    wf_note_ = make_note(QString(), wf_panel_.body_widget);
    wf_panel_.body->addWidget(wf_note_);

    // By year
    year_panel_ = make_panel(tr("BY YEAR"), content_, 4);
    year_seg_ = make_segmented({tr("COSTS"), tr("TURNOVER"), tr("NOTIONAL")}, 0,
                               [this](int i) {
                                   year_mode_ = i;
                                   render_by_year();
                               },
                               year_panel_.header);
    year_panel_.header->add_control(year_seg_);
    by_year_ = new ui::BarChart(year_panel_.body_widget);
    by_year_->set_show_values(true);
    by_year_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
    year_panel_.body->addWidget(by_year_, 1);
    year_note_ = make_note(QString(), year_panel_.body_widget);
    year_panel_.body->addWidget(year_note_);

    // Cost model
    model_panel_ = make_panel(tr("COST MODEL"), content_, 8);
    model_kv_ = new BtKvGrid(model_panel_.body_widget, 1);
    model_panel_.body->addWidget(model_kv_);
    model_source_ = make_note(QString(), model_panel_.body_widget);
    model_panel_.body->addWidget(model_source_);
    not_modelled_title_ = new QLabel(tr("NOT MODELLED"), model_panel_.body_widget);
    not_modelled_title_->setObjectName(QStringLiteral("btSectionTitle"));
    model_panel_.body->addWidget(not_modelled_title_);
    not_modelled_ = make_note(QString(), model_panel_.body_widget, Tone::Warn);
    model_panel_.body->addWidget(not_modelled_);
    model_panel_.body->addStretch(1);

    // Export menus of the painted charts.
    {
        ui::ChartExport::MenuSpec wf_spec;
        wf_spec.suggested_name = QStringLiteral("gross_to_net");
        wf_spec.csv = [this]() {
            ui::ChartExport::Table t;
            t.headers = QStringList{tr("Item"), tr("Amount")};
            if (!run_)
                return t;
            const BtCosts& c = run_->costs;
            const auto add = [&t](const QString& k, double v) {
                t.rows.append(QStringList{k, ui::formatting::export_number(v, 2)});
            };
            add(tr("Gross P&L"), c.gross_pnl);
            add(tr("Commission"), c.total_commission);
            add(tr("Slippage"), c.total_slippage);
            add(tr("Borrow"), c.total_borrow);
            add(tr("Total costs"), c.total_costs);
            add(tr("Net P&L"), c.net_pnl);
            return t;
        };
        ui::ChartExport::install_context_menu(waterfall_, wf_spec);

        ui::ChartExport::MenuSpec year_spec;
        year_spec.suggested_name = QStringLiteral("costs_by_year");
        year_spec.csv = [this]() {
            ui::ChartExport::Table t;
            t.headers = QStringList{tr("Year"), tr("Commission"), tr("Slippage"), tr("Traded notional"), tr("Turnover")};
            for (const BtCostYear& y : years_)
                t.rows.append(QStringList{QString::number(y.year), ui::formatting::export_number(y.commission, 2),
                                          ui::formatting::export_number(y.slippage, 2),
                                          ui::formatting::export_number(y.notional, 2),
                                          ui::formatting::export_number(y.turnover, 4)});
            return t;
        };
        ui::ChartExport::install_context_menu(by_year_, year_spec);
    }

    ui::formatting::on_format_changed(this, [this]() { render(); });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        fit_to_font();
        relayout(width(), true);
    });
    fit_to_font();
    relayout(width(), true);
}

void BtCostsView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(e->size().width(), false);
}

void BtCostsView::relayout(int width, bool force) {
    const QFontMetrics fm(ui::fonts::font(0));
    const int mode = width >= kCoTwoColumnChars * std::max(5, fm.averageCharWidth()) ? 1 : 0;
    if (!force && mode == layout_mode_)
        return;
    layout_mode_ = mode;
    for (QWidget* w : {static_cast<QWidget*>(notice_), static_cast<QWidget*>(kpis_),
                       static_cast<QWidget*>(wf_panel_.frame), static_cast<QWidget*>(year_panel_.frame),
                       static_cast<QWidget*>(model_panel_.frame)})
        grid_->removeWidget(w);
    for (int i = 0; i < 6; ++i) {
        grid_->setRowStretch(i, 0);
        grid_->setColumnStretch(i, 0);
    }
    if (mode == 1) {
        grid_->addWidget(notice_, 0, 0, 1, 2);
        grid_->addWidget(kpis_, 1, 0, 1, 2);
        grid_->addWidget(wf_panel_.frame, 2, 0);
        grid_->addWidget(year_panel_.frame, 3, 0);
        grid_->addWidget(model_panel_.frame, 2, 1, 2, 1);
        grid_->setColumnStretch(0, 62);
        grid_->setColumnStretch(1, 38);
        grid_->setRowStretch(2, 1);
        grid_->setRowStretch(3, 1);
    } else {
        grid_->addWidget(notice_, 0, 0);
        grid_->addWidget(kpis_, 1, 0);
        grid_->addWidget(wf_panel_.frame, 2, 0);
        grid_->addWidget(year_panel_.frame, 3, 0);
        grid_->addWidget(model_panel_.frame, 4, 0);
        grid_->setColumnStretch(0, 1);
        grid_->setRowStretch(2, 1);
        grid_->setRowStretch(3, 1);
    }
}

void BtCostsView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    waterfall_->setMinimumHeight(fm.height() * 13);
    by_year_->setMinimumHeight(fm.height() * 12);
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtCostsView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    years_.clear();
    years_computed_ = false;
    if (rp) {
        years_ = rp->costs.by_year;
        if (years_.isEmpty() && !rp->trades.isEmpty()) {
            years_ = co_years_from_trades(*rp);
            years_computed_ = !years_.isEmpty();
        }
    }
    render();
}

void BtCostsView::render() {
    const BtRun* r = run_.get();
    if (r) {
        const BtCosts& c = r->costs;
        const bool zero = (std::isfinite(c.total_costs) && std::fabs(c.total_costs) < 1e-9) ||
                          c.model.preset == QLatin1String("none");
        if (zero) {
            notice_text_->setText(tr("GROSS RESULT — no commission or slippage was applied. Turnover and traded notional "
                                     "below show what costs would act on."));
            notice_->setVisible(true);
        } else if (!std::isfinite(c.total_costs) && !c.present) {
            const QString engine = r->engine_implementation.isEmpty() ? r->engine : r->engine_implementation;
            notice_text_->setText(tr("%1 does not itemise its costs: the P&L is net of whatever the engine charged.")
                                      .arg(engine.isEmpty() ? tr("This engine") : engine));
            notice_->setVisible(true);
        } else {
            notice_->setVisible(false);
        }
    } else {
        notice_->setVisible(false);
    }
    render_kpis();
    render_waterfall();
    render_by_year();
    render_model();
}

void BtCostsView::render_kpis() {
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
    const BtCosts& c = r->costs;
    const QString ccy = r->currency;
    const double cap = r->initial_capital();
    const double yrs = r->years();

    ui::StatTile* total = kpis_->tile(QStringLiteral("total"));
    set_tile(total, c.total_costs, Unit::Money, Rule::None, ccy, r->why_blank(QStringLiteral("costs.totalCosts")));
    total->set_sub_label(std::isfinite(c.total_costs) && std::isfinite(cap) && cap > 0
                             ? tr("%1 of capital").arg(pct(c.total_costs / cap, 2))
                             : QString());
    total->set_tooltip_text(tr("Commission + slippage + borrow over the run"));

    ui::StatTile* comm = kpis_->tile(QStringLiteral("commission"));
    set_tile(comm, c.total_commission, Unit::Money, Rule::None, ccy, r->why_blank(QStringLiteral("costs.totalCommission")));
    comm->set_sub_label(QString());

    ui::StatTile* slip = kpis_->tile(QStringLiteral("slippage"));
    const QString slip_why = c.model.itemised_slippage
                                 ? r->why_blank(QStringLiteral("costs.totalSlippage"))
                                 : tr("Inside the fill prices: this engine does not itemise slippage");
    set_tile(slip, c.total_slippage, Unit::Money, Rule::None, ccy, slip_why);
    slip->set_sub_label(c.model.itemised_slippage ? QString() : tr("inside fill prices"));
    slip->set_tooltip_text(tr("Adverse fill-price difference (half the spread included), in money"));

    ui::StatTile* share = kpis_->tile(QStringLiteral("share"));
    double sh = c.pct_of_gross_profit;
    const double gp = r->metric(QStringLiteral("tradeStats.grossProfit"));
    const bool sh_computed = !std::isfinite(sh) && std::isfinite(c.total_costs) && std::isfinite(gp) && gp > 0;
    if (sh_computed)
        sh = c.total_costs / gp;
    set_tile(share, sh, Unit::Fraction, Rule::None, ccy, r->why_blank(QStringLiteral("costs.costsPctOfGrossProfit")));
    share->set_sub_label(sh_computed ? tr("computed here") : QString());
    share->set_tooltip_text(tr("Total costs as a share of the winning trades' gross profit"));

    double drag = c.cost_drag;
    const bool drag_approx = !std::isfinite(drag) && std::isfinite(c.total_costs) && std::isfinite(cap) && cap > 0 &&
                             std::isfinite(yrs) && yrs > 0;
    if (drag_approx)
        drag = c.total_costs / cap / yrs;
    co_text_tile(kpis_->tile(QStringLiteral("drag")), drag, pp(std::isfinite(drag) ? -drag : kBtNaN, 2),
                 drag_approx ? tr("a year · approx.") : tr("a year"), r->why_blank(QStringLiteral("costs.costDrag")));
    kpis_->tile(QStringLiteral("drag"))->set_tooltip_text(tr("Annual return lost to costs, percentage points"));

    const double notional = co_traded_notional(*r);
    double turnover = c.annual_turnover;
    const double avg_eq = co_avg_equity(*r);
    const bool to_computed = !std::isfinite(turnover) && std::isfinite(notional) && std::isfinite(avg_eq) &&
                             avg_eq > 0 && std::isfinite(yrs) && yrs > 0;
    if (to_computed)
        turnover = notional / avg_eq / yrs;
    co_text_tile(kpis_->tile(QStringLiteral("turnover")), turnover, tr("%1×").arg(ratio(turnover, 1)),
                 to_computed ? tr("of equity a year · computed") : tr("of equity a year"),
                 r->why_blank(QStringLiteral("costs.annualTurnover")));
    kpis_->tile(QStringLiteral("turnover"))->set_tooltip_text(tr("Traded notional per year, in multiples of equity"));

    ui::StatTile* nt = kpis_->tile(QStringLiteral("notional"));
    set_tile(nt, notional, Unit::Money, Rule::None, ccy, r->why_blank(QStringLiteral("costs.tradedNotional")));
    nt->set_sub_label(!std::isfinite(c.traded_notional) && std::isfinite(notional) ? tr("from the trades") : QString());
    nt->set_tooltip_text(tr("Sum of |quantity × price| over every fill"));

    ui::StatTile* fills = kpis_->tile(QStringLiteral("fills"));
    double nf = c.fills >= 0 ? static_cast<double>(c.fills) : kBtNaN;
    const bool nf_computed = !std::isfinite(nf) && !r->trades.isEmpty();
    if (nf_computed)
        nf = static_cast<double>(r->trades.size() * 2 - r->open_trades);
    set_tile(fills, nf, Unit::Count, Rule::None, ccy, r->why_blank(QStringLiteral("costs.fills")));
    fills->set_sub_label(nf_computed ? tr("2 per round trip") : QString());
}

void BtCostsView::render_waterfall() {
    const BtRun* r = run_.get();
    if (!r) {
        waterfall_->clear();
        waterfall_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
        wf_note_->setText(QString());
        wf_panel_.header->set_badge(QString());
        return;
    }
    const BtCosts& c = r->costs;
    const QString ccy = r->currency;
    const double comm = c.total_commission;
    const double slip = c.total_slippage;
    const double borrow = c.total_borrow;
    double net = c.net_pnl;
    double gross = c.gross_pnl;
    const double charged = co_or0(comm) + co_or0(slip) + co_or0(borrow);
    if (!std::isfinite(gross) && std::isfinite(net))
        gross = net + charged;
    if (!std::isfinite(net) && std::isfinite(gross))
        net = gross - charged;
    if (!std::isfinite(gross)) {
        waterfall_->clear();
        waterfall_->set_state(ui::PaintedChart::Empty, tr("The engine reported no gross or net P&L"));
        wf_note_->setText(QString());
        wf_panel_.header->set_badge(QString());
        return;
    }
    QStringList cats{tr("Gross P&L")};
    QVector<double> vals{gross};
    cats << tr("Commission");
    vals << -co_or0(comm);
    if (std::isfinite(slip)) {
        cats << tr("Slippage");
        vals << -slip;
    }
    if (std::isfinite(borrow) && borrow > 0) {
        cats << tr("Borrow");
        vals << -borrow;
    }
    const double residual = net - (gross - charged);
    if (std::isfinite(residual) && std::fabs(residual) > std::max(1e-6, std::fabs(gross) * 0.001)) {
        cats << tr("Other");
        vals << residual;
    }
    waterfall_->set_state(ui::PaintedChart::Ready);
    waterfall_->set_value_formatter([ccy](double v) { return money_compact(v, ccy, true); });
    waterfall_->set_data(cats, vals, tr("P&L"));
    QStringList notes;
    if (!c.model.itemised_slippage || !std::isfinite(slip))
        notes << tr("Slippage is inside the fill prices, so the gross P&L already includes it.");
    if (cats.contains(tr("Other")))
        notes << tr("Other: P&L the engine booked outside commission, slippage and borrow (financing, dividends, "
                    "rounding).");
    if (std::isfinite(gross) && gross != 0 && charged > 0)
        notes << tr("Costs took %1 of the gross result.").arg(pct(charged / std::fabs(gross), 1));
    wf_note_->setText(notes.join(QLatin1Char(' ')));
    wf_panel_.header->set_badge(tr("net %1").arg(money_compact(net, ccy, true)));
}

void BtCostsView::render_by_year() {
    const BtRun* r = run_.get();
    if (!r || years_.isEmpty()) {
        by_year_->clear();
        by_year_->set_state(ui::PaintedChart::Empty, r ? tr("No costs by year in this run") : tr("No backtest yet"));
        year_note_->setText(QString());
        year_panel_.header->set_badge(QString());
        return;
    }
    const QString ccy = r->currency;
    QStringList cats;
    ui::BarSeries comm{tr("Commission"), {}, {}};
    ui::BarSeries slip{tr("Slippage"), {}, {}};
    ui::BarSeries turn{tr("Turnover"), {}, {}};
    ui::BarSeries notional{tr("Traded notional"), {}, {}};
    bool any_slip = false;
    for (const BtCostYear& y : years_) {
        cats << QString::number(y.year);
        comm.values << y.commission;
        slip.values << y.slippage;
        any_slip = any_slip || std::isfinite(y.slippage);
        turn.values << y.turnover;
        notional.values << y.notional;
    }
    by_year_->set_state(ui::PaintedChart::Ready);
    by_year_->set_categories(cats);
    if (year_mode_ == 1) {
        by_year_->set_mode(ui::BarChart::Mode::Grouped);
        by_year_->set_value_formatter([](double v) { return BtCostsView::tr("%1×").arg(ratio(v, 1)); });
        by_year_->set_series({turn});
        year_note_->setText(tr("Traded notional of the year in multiples of its average equity."));
    } else if (year_mode_ == 2) {
        by_year_->set_mode(ui::BarChart::Mode::Grouped);
        by_year_->set_value_formatter([ccy](double v) { return money_compact(v, ccy); });
        by_year_->set_series({notional});
        year_note_->setText(tr("Sum of |quantity × price| over the year's fills."));
    } else {
        by_year_->set_mode(ui::BarChart::Mode::Stacked);
        by_year_->set_value_formatter([ccy](double v) { return money_compact(v, ccy); });
        if (any_slip)
            by_year_->set_series({comm, slip});
        else
            by_year_->set_series({comm});
        year_note_->setText(any_slip ? tr("Commission and slippage stacked per calendar year.")
                                     : tr("Commission per calendar year (slippage is not itemised)."));
    }
    year_panel_.header->set_badge(years_computed_ ? tr("computed from the trades") : QString());
}

void BtCostsView::render_model() {
    const BtRun* r = run_.get();
    if (!r) {
        model_kv_->set_rows({});
        model_source_->setText(QString());
        not_modelled_title_->setVisible(false);
        not_modelled_->setVisible(false);
        model_panel_.header->set_badge(QString());
        return;
    }
    const BtCostModel& m = r->costs.model;
    const QString ccy = r->currency;
    const QString why = m.present ? QString() : tr("This engine did not report its cost model");
    QString preset = m.preset;
    preset.replace(QLatin1Char('_'), QLatin1Char(' '));
    const auto itemised = [](bool on) { return on ? BtCostsView::tr("itemised") : BtCostsView::tr("in fills"); };
    QVector<BtKv> rows;
    rows << BtKv{tr("Preset"), preset.isEmpty() ? (m.present ? tr("custom") : dash()) : preset, Tone::None,
                 m.source.isEmpty() ? why : m.source};
    rows << BtKv{tr("Commission"), m.present ? co_commission_text(m, ccy) : dash(), Tone::None, why};
    rows << BtKv{tr("Min commission"),
                 std::isfinite(m.min_commission) && m.min_commission > 0 ? money(m.min_commission, ccy)
                                                                          : (m.present ? tr("none") : dash()),
                 Tone::None, why};
    rows << BtKv{tr("Slippage"), m.present ? co_slippage_text(m) : dash(), Tone::None, why};
    rows << BtKv{tr("Spread"),
                 std::isfinite(m.spread_bps) ? tr("%1 (half paid per fill)").arg(ui::formatting::bp(m.spread_bps, 1, false))
                                             : dash(),
                 Tone::None, why};
    rows << BtKv{tr("Borrow rate"), std::isfinite(m.borrow_rate) ? tr("%1 a year").arg(pct(m.borrow_rate, 2)) : dash(),
                 Tone::None, tr("Annual fee on the value of short positions")};
    rows << BtKv{tr("Financing"),
                 std::isfinite(m.financing_rate) && m.financing_rate > 0 ? tr("%1 a year").arg(pct(m.financing_rate, 2))
                                                                         : (m.present ? tr("off") : dash()),
                 Tone::None, why};
    rows << BtKv{tr("Day count"), m.day_count.isEmpty() ? dash() : m.day_count, Tone::None, why};
    rows << BtKv{tr("Fills at"), co_fill_text(*r), Tone::None, r->assumptions.execution};
    rows << BtKv{tr("Reported"),
                 tr("commission %1 · slippage %2 · borrow %3")
                     .arg(itemised(m.itemised_commission), itemised(m.itemised_slippage), itemised(m.itemised_borrow)),
                 m.itemised_slippage ? Tone::None : Tone::Warn, QString()};
    model_kv_->set_rows(rows);

    QStringList source;
    if (!m.source.isEmpty())
        source << m.source;
    if (!r->assumptions.costs.isEmpty() && r->assumptions.costs != m.source)
        source << r->assumptions.costs;
    model_source_->setText(source.join(QLatin1Char(' ')));
    model_source_->setVisible(!source.isEmpty());

    QStringList nm;
    for (const QString& s : r->assumptions.not_modelled)
        nm << QStringLiteral("• ") + s;
    not_modelled_->setText(nm.join(QLatin1Char('\n')));
    not_modelled_title_->setVisible(!nm.isEmpty());
    not_modelled_->setVisible(!nm.isEmpty());
    model_panel_.header->set_badge(m.present ? QString() : tr("not reported"));
}

} // namespace fincept::screens::bt
