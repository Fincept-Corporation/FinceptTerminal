// src/screens/algo_trading/DeploymentLogDialog.cpp
#include "screens/algo_trading/DeploymentLogDialog.h"

#include "algo_engine/PerformanceMetrics.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/MnemonicText.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace perf = fincept::algo::perf;
using fincept::services::algo::AlgoEquityPoint;
using fincept::services::algo::AlgoEventRow;
using fincept::services::algo::AlgoTradeRow;

DeploymentLogDialog::DeploymentLogDialog(const services::algo::AlgoDeployment& deployment, const QString& currency,
                                         QWidget* parent)
    : QDialog(parent), dep_(deployment), currency_(currency) {
    setWindowTitle(tr("%1 · %2 · %3")
                       .arg(dep_.strategy_name, dep_.symbol.isEmpty() ? dep_.underlying : dep_.symbol,
                            dep_.mode.toUpper()));
    resize(ui::metrics::width(960), ui::metrics::height(640));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    status_ = new QLabel(tr("Loading history…"), this);
    status_->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-2)));
    status_->setWordWrap(true);
    root->addWidget(status_);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_max_columns(5);
    root->addWidget(kpis_);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    chart_ = new ui::TimeSeriesChart(tabs_);
    chart_->set_export_name(QStringLiteral("deployment_pnl"));
    chart_->set_range_presets({QStringLiteral("1D"), QStringLiteral("1W"), QStringLiteral("1M"), QStringLiteral("ALL")});
    tabs_->addTab(chart_, fincept::ui::literal_ampersands(tr("P&L")));

    // `export_name` empty: the caller installs its own export menu.
    auto make_table = [this](const QStringList& headers, const QString& export_name) {
        auto* t = new QTableWidget(tabs_);
        t->setColumnCount(headers.size());
        t->setHorizontalHeaderLabels(headers);
        t->verticalHeader()->setVisible(false);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setSortingEnabled(true);
        t->horizontalHeader()->setStretchLastSection(true);
        // Interactive, fitted once after each fill (never ResizeToContents while rows go in).
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        ui::metrics::follow_row_height(t, 22);
        if (!export_name.isEmpty())
            ui::ChartExport::install_table_context_menu(t, export_name);
        return t;
    };
    fills_ = make_table({tr("Time"), tr("Side"), tr("Qty"), tr("Price"), tr("Signal px"), tr("Slippage"), tr("Fees"),
                         tr("Net P&L"), tr("Reason"), tr("Latency"), tr("Order id")},
                        QString());
    {
        // Right-click export of the fills: plain numbers + a currency column, like the button.
        ui::ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("deployment_fills");
        spec.csv = [this]() {
            ui::ChartExport::Table t;
            fills_export(&t.headers, &t.rows);
            return t;
        };
        ui::ChartExport::install_context_menu(fills_, spec);
    }
    tabs_->addTab(fills_, tr("Fills"));
    events_ = make_table({tr("Time"), tr("Kind"), tr("Message")}, QStringLiteral("deployment_log"));
    tabs_->addTab(events_, fincept::ui::literal_ampersands(tr("Signal & risk log")));
    root->addWidget(tabs_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* export_btn = buttons->addButton(tr("Export fills CSV…"), QDialogButtonBox::ActionRole);
    connect(export_btn, &QPushButton::clicked, this, [this]() {
        QStringList headers;
        QVector<QStringList> rows;
        fills_export(&headers, &rows);
        ui::ChartExport::save_csv(headers, rows, QStringLiteral("algo_fills_%1").arg(dep_.id.left(8)), this);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Currency / number format changed: re-render what is on screen (no reload).
    fmt::on_format_changed(this, [this]() {
        if (loaded_)
            populate(trades_, events_rows_, equity_);
    });
    load();
}

QString DeploymentLogDialog::money(double v, bool signed_value) const {
    return signed_value ? fmt::signed_money(v, currency_) : fmt::money(v, currency_);
}

void DeploymentLogDialog::fills_export(QStringList* headers, QVector<QStringList>* rows) const {
    // Machine-readable (R11): C-locale numbers without grouping or symbol, ISO 8601 UTC
    // times, and the ISO code of the price / fee / P&L columns in its own column.
    *headers = {QStringLiteral("time_utc"),  QStringLiteral("side"),         QStringLiteral("leg_symbol"),
                QStringLiteral("quantity"),  QStringLiteral("price"),        QStringLiteral("signal_price"),
                QStringLiteral("slippage_bp"), QStringLiteral("fees"),       QStringLiteral("net_pnl"),
                QStringLiteral("currency"),  QStringLiteral("reason"),       QStringLiteral("latency_ms"),
                QStringLiteral("order_id")};
    rows->clear();
    rows->reserve(trades_.size());
    for (const auto& t : trades_) {
        const double slip = t.ref_price > 0 ? (t.price - t.ref_price) / t.ref_price * 10000.0 *
                                                  (t.side == QLatin1String("BUY") ? 1 : -1)
                                            : std::numeric_limits<double>::quiet_NaN();
        rows->append({QDateTime::fromMSecsSinceEpoch(t.t_ms, QTimeZone(QTimeZone::UTC)).toString(Qt::ISODate),
                      t.side, t.leg_symbol, fmt::export_number(t.quantity), fmt::export_number(t.price),
                      t.ref_price > 0 ? fmt::export_number(t.ref_price) : QString(), fmt::export_number(slip, 2),
                      fmt::export_number(t.fees), t.is_exit() ? fmt::export_number(t.pnl) : QString(),
                      fmt::export_money(t.price, currency_).currency, t.reason,
                      t.latency_ms > 0 ? QString::number(t.latency_ms) : QString(), t.broker_order_id});
    }
}

void DeploymentLogDialog::load() {
    QPointer<DeploymentLogDialog> self = this;
    const QString id = dep_.id;
    (void)QtConcurrent::run([self, id]() {
        const auto trades = services::algo::monitor_store::trades(id);
        const auto events = services::algo::monitor_store::events(id);
        const auto curves = services::algo::monitor_store::equity_curves({id}, 0, 5000);
        const auto equity = curves.value(id);
        QMetaObject::invokeMethod(
            qApp,
            [self, trades, events, equity]() {
                if (self)
                    self->populate(trades, events, equity);
            },
            Qt::QueuedConnection);
    });
}

void DeploymentLogDialog::populate(const QVector<AlgoTradeRow>& trades, const QVector<AlgoEventRow>& events,
                                   const QVector<AlgoEquityPoint>& equity) {
    if (&trades != &trades_) { // kept for exports and the format-change re-render
        trades_ = trades;
        events_rows_ = events;
        equity_ = equity;
    }
    loaded_ = true;
    // ── Statistics (same definitions as the backtest report) ────────────────
    QVector<perf::ClosedTrade> closed;
    double slip_bps_sum = 0;
    int slip_n = 0;
    qint64 latency_sum = 0;
    int latency_n = 0;
    double fees = 0;
    for (const auto& t : trades) {
        fees += t.fees;
        if (t.ref_price > 0 && t.price > 0) {
            const double adverse = (t.price - t.ref_price) / t.ref_price * 10000.0 * (t.side == QLatin1String("BUY") ? 1 : -1);
            slip_bps_sum += adverse;
            ++slip_n;
        }
        if (t.latency_ms > 0) {
            latency_sum += t.latency_ms;
            ++latency_n;
        }
        if (t.is_exit() && t.leg_symbol.isEmpty()) {
            perf::ClosedTrade c;
            c.exit_ms = t.t_ms;
            c.net_pnl = t.pnl;
            c.gross_pnl = t.gross_pnl;
            c.fees = t.fees;
            closed.append(c);
        }
    }
    const auto ts = perf::trade_stats(closed);
    QVector<perf::Point> curve;
    curve.reserve(equity.size());
    for (const auto& p : equity)
        curve.append({p.t_ms, p.equity, 0});
    const auto dd = perf::drawdown(curve);

    kpis_->clear();
    auto tile = [this](const QString& label, const QString& value, ui::StatTile::Tone tone, const QString& tip = {}) {
        auto* t = kpis_->add_tile(label);
        t->set_compact(true);
        t->set_value(value);
        t->set_tone(tone);
        if (!tip.isEmpty())
            t->set_tooltip_text(tip);
        return t;
    };
    const double net = dep_.total_pnl + dep_.unrealized_pnl;
    auto* pnl_tile = tile(tr("NET P&L"), money(net, true),
                          net > 0   ? ui::StatTile::Tone::Positive
                          : net < 0 ? ui::StatTile::Tone::Negative
                                    : ui::StatTile::Tone::Neutral,
                          tr("Realized + unrealized, after fees."));
    if (curve.size() >= 2) {
        QVector<double> v;
        for (const auto& p : curve)
            v.append(p.v);
        pnl_tile->set_sparkline(v, 0.0);
    }
    tile(tr("CLOSED TRADES"), QString::number(ts.trades), ui::StatTile::Tone::Neutral)
        ->set_sub_label(tr("%1 W / %2 L").arg(ts.wins).arg(ts.losses));
    tile(tr("WIN RATE"), fmt::pct(ts.win_rate_pct, 1, false), ui::StatTile::Tone::Neutral);
    tile(tr("PROFIT FACTOR"),
         ts.profit_factor_infinite ? QString::fromUtf8("∞") : fmt::number(ts.profit_factor, 2),
         ui::StatTile::Tone::Neutral);
    tile(tr("EXPECTANCY"), money(ts.expectancy, true),
         ts.expectancy > 0   ? ui::StatTile::Tone::Positive
         : ts.expectancy < 0 ? ui::StatTile::Tone::Negative
                             : ui::StatTile::Tone::Neutral,
         tr("Average net P&L per closed trade."));
    tile(tr("MAX DRAWDOWN"), money(-dd.max_abs), dd.max_abs > 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral,
         tr("Largest fall of realized + unrealized P&L from its peak, from the recorded P&L history."));
    tile(dep_.is_live() ? tr("FEES (EST.)") : tr("FEES"), money(fees), ui::StatTile::Tone::Neutral,
         dep_.is_live() ? tr("Estimated from the deployment's cost model — brokers do not report fees with fills.")
                        : tr("Charged by the paper simulator per the strategy's cost model."));
    tile(tr("SLIPPAGE"), slip_n > 0 ? tr("%1 bp").arg(fmt::number(slip_bps_sum / slip_n, 1)) : fmt::placeholder(),
         ui::StatTile::Tone::Neutral, tr("Average fill price against the signal's reference price (positive = worse)."));
    tile(tr("LATENCY"), latency_n > 0 ? tr("%1 ms").arg(latency_sum / latency_n) : fmt::placeholder(),
         ui::StatTile::Tone::Neutral, tr("Average time from signal to confirmed fill."));

    // ── P&L chart ────────────────────────────────────────────────────────────
    chart_->clear();
    if (curve.size() < 2) {
        chart_->set_state(ui::TimeSeriesChart::State::Empty,
                          tr("No P&L history recorded yet (snapshots are taken while the deployment runs)."));
    } else {
        using TS = ui::TimeSeriesChart;
        chart_->set_state(TS::State::Ready);
        while (chart_->pane_count() < 2)
            chart_->add_pane(0.35);
        QVector<double> xs, ys, dds;
        double peak = curve.first().v;
        for (const auto& p : curve) {
            xs.append(static_cast<double>(p.t_ms));
            ys.append(p.v);
            peak = std::max(peak, p.v);
            dds.append(p.v - peak);
        }
        TS::SeriesSpec s;
        s.name = tr("Net P&L");
        s.xs = xs;
        s.ys = ys;
        s.style = TS::Style::Baseline;
        s.baseline = 0.0;
        s.rebase = false;
        chart_->add_series(s);
        TS::SeriesSpec u;
        u.name = tr("Drawdown");
        u.xs = xs;
        u.ys = dds;
        u.style = TS::Style::Area;
        u.color = ui::ChartColorRole::Negative;
        u.baseline = 0.0;
        u.pane = 1;
        u.rebase = false;
        chart_->add_series(u);
        const QString ccy = currency_; // abbreviated per the user's scheme (K/M/B or lakh / crore)
        chart_->set_value_formatter(TS::Axis::Left,
                                    [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); }, 0);
        chart_->set_value_formatter(TS::Axis::Left,
                                    [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); }, 1);
        chart_->set_pane_title(1, tr("Drawdown"));
    }

    // ── Fills ────────────────────────────────────────────────────────────────
    // One deployment = one currency: the price columns name it in the header and their
    // cells are bare prices in the instrument's decimals; fees / P&L carry their unit.
    const QString sym = dep_.symbol.isEmpty() ? dep_.underlying : dep_.symbol;
    for (const auto& [col, title] : {std::pair<int, QString>{3, tr("Price")}, std::pair<int, QString>{4, tr("Signal px")}}) {
        if (auto* hi = fills_->horizontalHeaderItem(col))
            hi->setText(currency_.isEmpty() ? title : fmt::header(title, currency_));
    }
    fills_->setSortingEnabled(false);
    fills_->setRowCount(trades.size());
    const QColor up(ui::colors::POSITIVE());
    const QColor down(ui::colors::NEGATIVE());
    for (int i = 0; i < trades.size(); ++i) {
        const auto& t = trades[i];
        const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(t.leg_symbol.isEmpty() ? sym : t.leg_symbol, currency_);
        auto num = [&](int col, const QString& text, double v, bool color = false) {
            auto* it = new ui::NumericTableWidgetItem(text, v);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (color && v != 0)
                it->setForeground(v > 0 ? up : down);
            fills_->setItem(i, col, it);
        };
        auto txt = [&](int col, const QString& text) { fills_->setItem(i, col, new QTableWidgetItem(text)); };
        num(0, fmt::date_time(t.t_ms / 1000, fmt::DateStyle::Short, true), static_cast<double>(t.t_ms));
        auto* side = new QTableWidgetItem(t.leg_symbol.isEmpty() ? t.side : t.side + QStringLiteral(" ") + t.leg_symbol);
        side->setForeground(t.side == QLatin1String("BUY") ? up : down);
        fills_->setItem(i, 1, side);
        num(2, fmt::quantity(t.quantity), t.quantity);
        num(3, fmt::price(t.price, inst), t.price);
        num(4, t.ref_price > 0 ? fmt::price(t.ref_price, inst) : fmt::placeholder(), t.ref_price);
        const double slip = t.ref_price > 0 ? (t.price - t.ref_price) / t.ref_price * 10000.0 *
                                                  (t.side == QLatin1String("BUY") ? 1 : -1)
                                            : 0.0;
        num(5, t.ref_price > 0 ? tr("%1 bp").arg(fmt::number(slip, 1)) : fmt::placeholder(), slip);
        num(6, money(t.fees), t.fees);
        num(7, t.is_exit() ? money(t.pnl, true) : fmt::placeholder(), t.pnl, t.is_exit());
        txt(8, t.reason);
        num(9, t.latency_ms > 0 ? tr("%1 ms").arg(t.latency_ms) : fmt::placeholder(), static_cast<double>(t.latency_ms));
        txt(10, t.broker_order_id);
    }
    fills_->setSortingEnabled(true);
    fills_->resizeColumnsToContents();

    // ── Log ──────────────────────────────────────────────────────────────────
    events_->setSortingEnabled(false);
    events_->setRowCount(events.size());
    for (int i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        auto* when = new ui::NumericTableWidgetItem(fmt::date_time(e.t_ms / 1000, fmt::DateStyle::Short, true),
                                                    static_cast<double>(e.t_ms));
        events_->setItem(i, 0, when);
        auto* kind = new QTableWidgetItem(e.kind.toUpper());
        if (e.kind == QLatin1String("reject") || e.kind == QLatin1String("error"))
            kind->setForeground(QColor(ui::colors::DANGER()));
        else if (e.kind == QLatin1String("risk"))
            kind->setForeground(QColor(ui::colors::WARNING()));
        events_->setItem(i, 1, kind);
        events_->setItem(i, 2, new QTableWidgetItem(e.message));
    }
    events_->setSortingEnabled(true);
    events_->resizeColumnsToContents();

    status_->setText(trades.isEmpty() && events.isEmpty() && equity.isEmpty()
                         ? tr("Nothing recorded for this deployment yet.")
                         : tr("%1 fills · %2 log lines · %3 P&L snapshots").arg(trades.size()).arg(events.size()).arg(
                               equity.size()));
}

} // namespace fincept::screens
