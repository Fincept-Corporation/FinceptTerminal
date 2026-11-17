// src/screens/backtesting/BtExposureView.cpp
//
// EXPOSURE sub-tab (spec 2.6): exposure KPIs · gross / net exposure over time with cash in a second
// pane (or, when the engine reports no exposure series, the equity curve with the in-market spans
// shaded from the trades, so the area is never blank) · positions at the end of the run + a CASH row.
// A KPI the engine did not report is computed from the series / trades and says so in its sub-label.
#include "screens/backtesting/BtRunViews.h"

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
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPair>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::BtPosition;
using services::backtest::BtTrade;

namespace {

using ExTs = ui::TimeSeriesChart;

constexpr int kExMaxBands = 400;

enum ExCol { kExSymbol, kExQty, kExAvg, kExLast, kExValue, kExWeight, kExUnreal };

struct ExPosRow {
    BtPosition p;
    bool cash = false;
    bool derived = false; ///< built from an open trade (the engine sent no positions[])
};
using ExPosModel = ui::LiveTableModel<ExPosRow>;

QVariant ex_right() {
    return QVariant((Qt::AlignRight | Qt::AlignVCenter).toInt());
}

QVariant ex_pos_cell(const ExPosRow& row, int col, int role, const QString& ccy) {
    const BtPosition& p = row.p;
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case kExSymbol:
                    return row.cash ? BtExposureView::tr("CASH") : p.symbol;
                case kExQty:
                    return row.cash ? QString() : quantity(p.quantity);
                case kExAvg:
                    return row.cash ? QString() : price(p.avg_price, ccy);
                case kExLast:
                    return row.cash ? QString() : price(p.last_price, ccy);
                case kExValue:
                    return money(p.market_value, ccy);
                case kExWeight:
                    return pct(p.weight, 1, !row.cash);
                case kExUnreal:
                    return row.cash ? QString() : money(p.unrealized, ccy, true);
                default:
                    break;
            }
            break;
        case Qt::EditRole:
            if (col == kExWeight && std::isfinite(p.weight))
                return p.weight;
            break;
        case ExPosModel::SortRole:
            switch (col) {
                case kExSymbol:
                    return row.cash ? QStringLiteral("~") : p.symbol;
                case kExQty:
                    return std::isfinite(p.quantity) ? p.quantity : 0.0;
                case kExAvg:
                    return std::isfinite(p.avg_price) ? p.avg_price : 0.0;
                case kExLast:
                    return std::isfinite(p.last_price) ? p.last_price : 0.0;
                case kExValue:
                    return std::isfinite(p.market_value) ? p.market_value : 0.0;
                case kExWeight:
                    return std::isfinite(p.weight) ? p.weight : 0.0;
                case kExUnreal:
                    return std::isfinite(p.unrealized) ? p.unrealized : 0.0;
                default:
                    break;
            }
            break;
        case Qt::ForegroundRole:
            if (col == kExUnreal && !row.cash) {
                const Tone tn = sign_tone(p.unrealized);
                if (tn != Tone::None)
                    return tone_color(tn);
            }
            if (row.cash && col == kExSymbol)
                return tone_color(Tone::Accent);
            break;
        case Qt::TextAlignmentRole:
            if (col != kExSymbol)
                return ex_right();
            break;
        case Qt::ToolTipRole:
            if (row.cash)
                return BtExposureView::tr("Final equity minus the market value of the positions");
            if (row.derived)
                return BtExposureView::tr("Derived from the open trade (the engine did not report positions)");
            if (col == kExQty && std::isfinite(p.quantity) && p.quantity < 0)
                return BtExposureView::tr("Short position");
            break;
        default:
            break;
    }
    return {};
}

double ex_mean(const QVector<double>& v, bool absolute) {
    double s = 0;
    int n = 0;
    for (double x : v)
        if (std::isfinite(x)) {
            s += absolute ? std::fabs(x) : x;
            ++n;
        }
    return n > 0 ? s / n : kBtNaN;
}

double ex_max_abs(const QVector<double>& v) {
    double m = kBtNaN;
    for (double x : v)
        if (std::isfinite(x) && (!std::isfinite(m) || std::fabs(x) > m))
            m = std::fabs(x);
    return m;
}

/// End time of a trade for spans: its exit, or the last bar while it is open.
double ex_trade_end(const BtRun& r, const BtTrade& t) {
    const double last = r.t.isEmpty() ? t.entry_t : r.t.last();
    return std::isfinite(t.exit_t) && !t.open ? t.exit_t : last;
}

/// Open-position count over time from the trades: (time, count) steps.
void ex_position_steps(const BtRun& r, QVector<double>* xs, QVector<double>* ys, double* avg, double* max_n) {
    QVector<QPair<double, int>> ev;
    ev.reserve(r.trades.size() * 2);
    for (const BtTrade& t : r.trades) {
        if (!std::isfinite(t.entry_t))
            continue;
        ev.append({t.entry_t, 1});
        ev.append({std::max(ex_trade_end(r, t), t.entry_t), -1});
    }
    *avg = kBtNaN;
    *max_n = kBtNaN;
    if (ev.isEmpty() || r.t.size() < 2)
        return;
    std::sort(ev.begin(), ev.end(), [](const QPair<double, int>& a, const QPair<double, int>& b) {
        return a.first != b.first ? a.first < b.first : a.second < b.second; // exits before entries
    });
    const double t0 = r.t.first(), t1 = r.t.last();
    int live = 0, peak = 0;
    double area = 0, prev_t = t0;
    xs->append(t0);
    ys->append(0);
    for (const auto& e : ev) {
        const double at = std::clamp(e.first, t0, t1);
        area += live * (at - prev_t);
        prev_t = at;
        live += e.second;
        peak = std::max(peak, live);
        if (!xs->isEmpty() && xs->last() == at) {
            ys->last() = live;
        } else {
            xs->append(at);
            ys->append(live);
        }
    }
    area += live * (t1 - prev_t);
    if (xs->last() < t1) {
        xs->append(t1);
        ys->append(live);
    }
    *avg = t1 > t0 ? area / (t1 - t0) : kBtNaN;
    *max_n = peak;
}

/// Merged [entry, exit] spans of the trades, at most kExMaxBands (short gaps closed first).
QVector<QPair<double, double>> ex_spans(const BtRun& r) {
    QVector<QPair<double, double>> raw;
    raw.reserve(r.trades.size());
    for (const BtTrade& t : r.trades)
        if (std::isfinite(t.entry_t))
            raw.append({t.entry_t, std::max(ex_trade_end(r, t), t.entry_t)});
    std::sort(raw.begin(), raw.end());
    const int n = static_cast<int>(r.t.size());
    double gap = n >= 2 ? (r.t.last() - r.t.first()) / (n - 1) : 86400000.0;
    QVector<QPair<double, double>> out;
    for (int pass = 0; pass < 40; ++pass) {
        out.clear();
        for (const auto& s : raw) {
            if (!out.isEmpty() && s.first <= out.last().second + gap)
                out.last().second = std::max(out.last().second, s.second);
            else
                out.append(s);
        }
        if (out.size() <= kExMaxBands)
            break;
        gap *= 2.0;
    }
    return out;
}

/// Fill a tile from the engine's metric, else `computed` with `note` as its sub-label, else "—".
void ex_tile(ui::StatTile* tile, const BtRun& r, const QString& path, double computed, Unit unit,
             const QString& note, const QString& tip) {
    const double v = r.metric(path);
    tile->set_tooltip_text(tip);
    if (std::isfinite(v)) {
        set_tile(tile, v, unit, Rule::None, r.currency);
        tile->set_sub_label(QString());
    } else if (std::isfinite(computed)) {
        set_tile(tile, computed, unit, Rule::None, r.currency);
        tile->set_sub_label(note);
    } else {
        set_tile(tile, kBtNaN, unit, Rule::None, r.currency, r.why_blank(path));
        tile->set_sub_label(QString());
    }
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

BtExposureView::BtExposureView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("btScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll_);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    scroll_->setWidget(content);
    root->addWidget(scroll_);

    kpis_ = new ui::KpiStrip(content);
    const QList<QPair<QString, QString>> tiles = {
        {QStringLiteral("time"), tr("TIME IN MARKET")}, {QStringLiteral("avg_gross"), tr("AVG GROSS")},
        {QStringLiteral("max_gross"), tr("MAX GROSS")}, {QStringLiteral("avg_net"), tr("AVG NET")},
        {QStringLiteral("avg_pos"), tr("AVG POSITIONS")}, {QStringLiteral("max_pos"), tr("MAX POSITIONS")},
        {QStringLiteral("long"), tr("LONG SHARE")},      {QStringLiteral("turnover"), tr("ANNUAL TURNOVER")},
    };
    for (const auto& t : tiles)
        kpis_->add_tile(t.first, t.second)->set_compact(true);
    v->addWidget(kpis_);

    notice_ = new QFrame(content);
    notice_->setObjectName(QStringLiteral("btPanel"));
    notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* nl = new QHBoxLayout(notice_);
    nl->setContentsMargins(8, 4, 8, 4);
    nl->setSpacing(8);
    nl->addWidget(make_chip(tr("NOT REPORTED"), Tone::Warn, notice_), 0, Qt::AlignVCenter);
    notice_text_ = make_note(QString(), notice_, Tone::None);
    nl->addWidget(notice_text_, 1);
    notice_->setVisible(false);
    v->addWidget(notice_);

    chart_panel_ = make_panel(tr("EXPOSURE · % OF EQUITY"), content, 4);
    chart_ = make_time_chart(chart_panel_.body_widget, QStringLiteral("exposure"));
    chart_->set_range_presets({QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("ALL")});
    pane2_ = chart_->add_pane(0.45);
    chart_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    chart_panel_.body->addWidget(chart_, 1);
    connect(chart_, &ExTs::crosshair_moved, this, [this](double x) {
        if (selection_)
            emit selection_->crosshair_moved(x, this);
    });
    v->addWidget(chart_panel_.frame, 3);

    pos_panel_ = make_panel(tr("POSITIONS AT END OF RUN"), content, 0);
    pos_table_ = new QTableView(pos_panel_.body_widget);
    setup_table(pos_table_, QStringLiteral("positions"));
    auto* pm = new ExPosModel(this);
    pm->set_headers({tr("SYMBOL"), tr("QTY"), tr("AVG PRICE"), tr("LAST"), tr("MARKET VALUE"), tr("WEIGHT"),
                     tr("UNREALISED P&L")});
    pm->set_cell([this](const ExPosRow& row, int c, int role) {
        return ex_pos_cell(row, c, role, run_ ? run_->currency : QString());
    });
    pos_model_ = pm;
    pos_proxy_ = make_sort_proxy(pm, this);
    pos_table_->setModel(pos_proxy_);
    weight_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, pos_table_);
    weight_delegate_->set_range(0.0, 1.0);
    pos_table_->setItemDelegateForColumn(kExWeight, weight_delegate_);
    set_column_chars(pos_table_, {12, 11, 11, 11, 15, 14, 16});
    pos_table_->sortByColumn(kExValue, Qt::DescendingOrder);
    ui::formatting::refresh_model_on_format_change(pm);
    pos_panel_.body->addWidget(pos_table_, 1);
    pos_note_ = make_note(QString(), pos_panel_.body_widget);
    pos_note_->setContentsMargins(8, 2, 8, 4);
    pos_panel_.body->addWidget(pos_note_);
    v->addWidget(pos_panel_.frame, 2);

    if (selection_) {
        connect(selection_, &BtSelection::time_range_selected, this, [this](double a, double b, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (std::isfinite(a) && std::isfinite(b))
                chart_->set_x_range(a, b);
            else
                chart_->reset_x_range();
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
        render_kpis();
        render_chart();
        if (zoomed)
            chart_->set_x_range(a, b);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_to_font(); });
    fit_to_font();
}

void BtExposureView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    chart_->setMinimumHeight(fm.height() * 18);
    const int row_h = pos_table_->verticalHeader()->defaultSectionSize();
    pos_table_->setMinimumHeight(pos_table_->horizontalHeader()->sizeHint().height() + row_h * 5);
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtExposureView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    render();
}

void BtExposureView::render() {
    render_kpis();
    render_chart();
    render_positions();
}

void BtExposureView::render_kpis() {
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
    const bool series = r->has_exposure();
    const QString from_series = tr("from the exposure series");
    const QString from_trades = tr("from the trades");
    QVector<double> xs, ys;
    double avg_pos = kBtNaN, max_pos = kBtNaN;
    ex_position_steps(*r, &xs, &ys, &avg_pos, &max_pos);

    double long_t = 0, all_t = 0;
    for (const BtTrade& t : r->trades) {
        if (!std::isfinite(t.entry_t))
            continue;
        const double d = std::max(0.0, ex_trade_end(*r, t) - t.entry_t);
        all_t += d;
        long_t += t.side == QLatin1String("short") ? 0.0 : d;
    }

    ex_tile(kpis_->tile(QStringLiteral("time")), *r, QStringLiteral("exposure.timeInMarket"), kBtNaN, Unit::Fraction,
            QString(), tr("Share of the bars with a position open"));
    ex_tile(kpis_->tile(QStringLiteral("avg_gross")), *r, QStringLiteral("exposure.avgGrossExposure"),
            series ? ex_mean(r->exposure, true) : kBtNaN, Unit::Fraction, from_series,
            tr("Average gross exposure (long + short market value) as a share of equity"));
    ex_tile(kpis_->tile(QStringLiteral("max_gross")), *r, QStringLiteral("exposure.maxGrossExposure"),
            series ? ex_max_abs(r->exposure) : kBtNaN, Unit::Fraction, from_series,
            tr("Highest gross exposure as a share of equity (above 100 % = leverage)"));
    ex_tile(kpis_->tile(QStringLiteral("avg_net")), *r, QStringLiteral("exposure.avgNetExposure"),
            r->net_exposure.isEmpty() ? kBtNaN : ex_mean(r->net_exposure, false), Unit::SignedFraction, from_series,
            tr("Average net exposure (long − short) as a share of equity"));
    ex_tile(kpis_->tile(QStringLiteral("avg_pos")), *r, QStringLiteral("exposure.avgPositions"), avg_pos, Unit::Plain,
            from_trades, tr("Average number of open positions over the run (time-weighted)"));
    ex_tile(kpis_->tile(QStringLiteral("max_pos")), *r, QStringLiteral("exposure.maxPositions"), max_pos, Unit::Count,
            from_trades, tr("Most positions open at the same time"));
    ex_tile(kpis_->tile(QStringLiteral("long")), *r, QStringLiteral("exposure.longShare"),
            all_t > 0 ? long_t / all_t : kBtNaN, Unit::Fraction, from_trades,
            tr("Share of the time in the market spent long"));

    ui::StatTile* to = kpis_->tile(QStringLiteral("turnover"));
    to->set_tooltip_text(tr("Traded notional per year, in multiples of the average equity"));
    const double turnover = r->costs.annual_turnover;
    if (std::isfinite(turnover)) {
        to->set_state(ui::StatTile::State::Normal);
        to->set_tone(ui::StatTile::Tone::Neutral);
        to->set_value(tr("%1×").arg(ratio(turnover, 1)));
        to->set_sub_label(tr("of equity a year"));
    } else {
        set_tile(to, kBtNaN, Unit::Ratio, Rule::None, r->currency, r->why_blank(QStringLiteral("costs.annualTurnover")));
        to->set_sub_label(QString());
    }
}

void BtExposureView::render_chart() {
    chart_->clear();
    const BtRun* r = run_.get();
    if (!r || r->t.size() < 2) {
        chart_->set_state(ui::ChartState::Empty, r ? tr("This run has no equity curve") : tr("No backtest yet"));
        notice_->setVisible(false);
        chart_panel_.header->set_badge(QString());
        return;
    }
    chart_->set_state(ui::ChartState::Ready);
    const QString ccy = r->currency;
    const auto pct_series = [](double v) { return ui::formatting::pct(v, 1, false); };

    if (r->has_exposure()) {
        notice_->setVisible(false);
        chart_panel_.header->set_title(tr("EXPOSURE · % OF EQUITY"));
        chart_->set_pane_title(0, tr("Exposure"));
        chart_->set_value_formatter(ExTs::Axis::Left, [](double v) { return ui::formatting::pct(v, 0, false); }, 0);
        ExTs::SeriesSpec g;
        g.name = tr("Gross");
        g.points = to_points(r->t, r->exposure, 100.0);
        g.style = ExTs::Style::Step;
        g.color = ui::ChartColorRole::Accent;
        g.rebase = false;
        g.formatter = pct_series;
        chart_->add_series(g);
        if (!r->net_exposure.isEmpty()) {
            ExTs::SeriesSpec n;
            n.name = tr("Net");
            n.points = to_points(r->t, r->net_exposure, 100.0);
            n.style = ExTs::Style::Step;
            n.color = ui::ChartColorRole::Info;
            n.rebase = false;
            n.formatter = pct_series;
            chart_->add_series(n);
        }
        chart_->add_hline(100.0, tr("100 %"), ui::ChartColorRole::Muted);
        chart_->add_hline(0.0, QString(), ui::ChartColorRole::Muted);
        chart_panel_.header->set_badge(tr("max %1").arg(pct(ex_max_abs(r->exposure), 0)));
    } else {
        const QString engine = !r->engine_implementation.isEmpty() ? r->engine_implementation
                                                                     : (r->engine.isEmpty() ? tr("This engine") : r->engine);
        notice_text_->setText(tr("%1 does not report exposure over time. The equity curve is shown with the periods a "
                                 "position was open shaded (from the trades); time in market comes from the metrics.")
                                  .arg(engine));
        notice_->setVisible(true);
        chart_panel_.header->set_title(tr("EQUITY · IN-MARKET SPANS"));
        chart_->set_pane_title(0, tr("Equity"));
        chart_->set_value_formatter(ExTs::Axis::Left, money_formatter(ccy), 0);
        ExTs::SeriesSpec e;
        e.name = tr("Equity");
        e.points = to_points(r->t, r->equity);
        e.color = ui::ChartColorRole::Accent;
        e.formatter = [ccy](double x) { return money(x, ccy); };
        chart_->add_series(e);
        const auto spans = ex_spans(*r);
        for (const auto& s : spans)
            chart_->add_band(s.first, std::max(s.second, s.first + 1.0), ui::ChartColorRole::Info);
        chart_panel_.header->set_badge(spans.isEmpty() ? tr("never in the market")
                                                       : tr("%1 in-market spans").arg(count(static_cast<double>(spans.size()))));
    }

    // Second pane: cash, else the open-position count, else the drawdown.
    if (!r->cash.isEmpty()) {
        chart_->set_pane_title(pane2_, tr("Cash"));
        chart_->set_value_formatter(ExTs::Axis::Left, money_formatter(ccy), pane2_);
        ExTs::SeriesSpec c;
        c.name = tr("Cash");
        c.points = to_points(r->t, r->cash);
        c.style = ExTs::Style::Area;
        c.pane = pane2_;
        c.color = ui::ChartColorRole::Cyan;
        c.baseline = 0.0;
        c.rebase = false;
        c.formatter = [ccy](double x) { return money(x, ccy); };
        chart_->add_series(c);
    } else if (!r->trades.isEmpty()) {
        QVector<double> xs, ys;
        double avg = 0, mx = 0;
        ex_position_steps(*r, &xs, &ys, &avg, &mx);
        chart_->set_pane_title(pane2_, tr("Open positions (from the trades)"));
        chart_->set_value_formatter(ExTs::Axis::Left, number_formatter(0), pane2_);
        ExTs::SeriesSpec c;
        c.name = tr("Open positions");
        c.points = to_points(xs, ys);
        c.style = ExTs::Style::Step;
        c.pane = pane2_;
        c.color = ui::ChartColorRole::Cyan;
        c.rebase = false;
        c.formatter = number_formatter(0);
        chart_->add_series(c);
    } else {
        chart_->set_pane_title(pane2_, tr("Drawdown"));
        chart_->set_value_formatter(ExTs::Axis::Left, [](double v) { return ui::formatting::pct(v, 0, false); }, pane2_);
        ExTs::SeriesSpec c;
        c.name = tr("Drawdown");
        c.points = to_points(r->t, r->drawdown, 100.0);
        c.style = ExTs::Style::Area;
        c.pane = pane2_;
        c.color = ui::ChartColorRole::Warning;
        c.baseline = 0.0;
        c.rebase = false;
        c.formatter = pct_series;
        chart_->add_series(c);
    }
}

void BtExposureView::render_positions() {
    auto* pm = static_cast<ExPosModel*>(pos_model_);
    const BtRun* r = run_.get();
    if (!r) {
        pm->upsert({});
        pos_note_->setText(QString());
        pos_panel_.header->set_badge(QString());
        return;
    }
    const double fin = r->final_equity();
    QVector<ExPosRow> rows;
    bool derived = false;
    for (const BtPosition& p : r->positions)
        rows.append(ExPosRow{p, false, false});
    if (rows.isEmpty()) {
        // No positions[] from the engine: the open trades are the positions.
        for (const BtTrade& t : r->trades) {
            if (!t.open)
                continue;
            ExPosRow row;
            row.derived = true;
            const double sign = t.side == QLatin1String("short") ? -1.0 : 1.0;
            row.p.symbol = t.symbol;
            row.p.quantity = std::isfinite(t.quantity) ? sign * std::fabs(t.quantity) : kBtNaN;
            row.p.avg_price = t.entry_price;
            row.p.last_price = std::isfinite(t.mark_price) ? t.mark_price : t.exit_price;
            row.p.market_value = std::isfinite(row.p.quantity) && std::isfinite(row.p.last_price)
                                     ? row.p.quantity * row.p.last_price
                                     : kBtNaN;
            row.p.unrealized = t.pnl;
            rows.append(row);
            derived = true;
        }
    }
    double mv_sum = 0;
    for (ExPosRow& row : rows) {
        if (std::isfinite(row.p.market_value))
            mv_sum += row.p.market_value;
        if (!std::isfinite(row.p.weight) && std::isfinite(row.p.market_value) && std::isfinite(fin) && fin != 0)
            row.p.weight = row.p.market_value / fin;
    }
    const int n_pos = static_cast<int>(rows.size());
    ExPosRow cash;
    cash.cash = true;
    cash.p.symbol = tr("CASH");
    cash.p.market_value = std::isfinite(fin) ? fin - mv_sum : kBtNaN;
    cash.p.weight = (std::isfinite(fin) && fin != 0 && std::isfinite(cash.p.market_value)) ? cash.p.market_value / fin
                                                                                          : kBtNaN;
    rows.append(cash);
    double lo = 0, hi = 0;
    for (const ExPosRow& row : rows)
        if (std::isfinite(row.p.weight)) {
            lo = std::min(lo, row.p.weight);
            hi = std::max(hi, row.p.weight);
        }
    weight_delegate_->set_range(lo < hi ? lo : 0.0, lo < hi ? hi : 1.0);
    pm->upsert(rows);
    pos_panel_.header->set_badge(tr("%1 open · equity %2").arg(n_pos).arg(money(fin, r->currency)));
    if (n_pos == 0)
        pos_note_->setText(tr("Flat at the end of the run: everything is in cash."));
    else if (derived)
        pos_note_->setText(tr("Derived from the open trades: the engine did not report end-of-run positions."));
    else
        pos_note_->setText(tr("Weights are shares of the final equity; the CASH row is equity minus the positions."));
}

} // namespace fincept::screens::bt
