// src/screens/algo_trading/DeploymentDashboard.cpp
#include "screens/algo_trading/DeploymentDashboard.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/AlgoMarket.h"
#include "algo_engine/RiskLimits.h"
#include "core/agent/AgentRiskMarkers.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/DeploymentLogDialog.h"
#include "screens/algo_trading/DeploymentsView.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/markets/FxRateService.h"
#include "trading/AccountManager.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QMap>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTime>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <cmath>
#include <iterator>

namespace {

/// The PAUSE ↔ RESUME toggle's agent marker: only the starting state commits something.
/// PAUSE (stops evaluating) carries none; RESUME restarts trading, `order`.
void dd_mark_pause_toggle(QPushButton* b, bool paused) {
    if (!b)
        return;
    if (paused)
        b->setProperty("fincept_agent_risk", "order");
    else
        b->setProperty(fincept::core::agent::kAgentRiskProperty, QVariant());
}

// P&L history shown on the dashboard (chart + trends).
constexpr qint64 kDashEquityWindowMs = 30LL * 24 * 3600 * 1000;
// Recent activity notes kept per deployment (the full, persisted log is under LOG).
constexpr int kDashActivityLines = 6;

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;
namespace algo_ns = fincept::algo;
namespace fmt = fincept::ui::formatting;
namespace kit = fincept::screens::algo_kit;

namespace {

/// A price in the deployment's quote currency ("$189.84", "117.70p"); "—" when there is none.
QString dash_price(double v, const QString& symbol, const QString& ccy) {
    if (!(std::isfinite(v) && v > 0))
        return fmt::placeholder();
    return fmt::price_with_symbol(v, fmt::instrument_for_symbol(symbol, ccy));
}

/// When a deployment row was created (epoch ms; 0 = unknown).
qint64 dash_started_ms(const QString& created_at) {
    if (created_at.isEmpty())
        return 0;
    QDateTime t = QDateTime::fromString(created_at, Qt::ISODate);
    if (!t.isValid()) {
        t = QDateTime::fromString(created_at, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (t.isValid())
            t.setTimeZone(QTimeZone::UTC); // SQLite CURRENT_TIMESTAMP is UTC
    }
    return t.isValid() ? t.toMSecsSinceEpoch() : 0;
}

} // namespace

// ── Constructor ─────────────────────────────────────────────────────────────

DeploymentDashboard::DeploymentDashboard(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    view_ = new DeploymentsView(this);
    root->addWidget(view_);

    // STOP ALL stops every running deployment — trading state, so an order-class action.
    stop_all_btn_ = view_->stop_all_button();
    stop_all_btn_->setProperty("fincept_agent_risk", "order");
    // REMOVE deletes a stopped deployment's whole record (the confirm carries it too).
    QPushButton* remove_btn = view_->remove_button();
    remove_btn->setProperty("fincept_agent_risk", "delete");
    // PAUSE carries no marker; RESUME (paused) is `order` — dd_mark_pause_toggle().
    dd_mark_pause_toggle(view_->pause_button(), false);

    connect(view_, &DeploymentsView::refresh_requested, this, [this]() {
        algo_ns::AlgoEngine::instance().list_deployments();
        refresh_equity();
        if (!history_for_.isEmpty())
            load_history(history_for_);
    });
    connect(view_, &DeploymentsView::selection_changed, this, &DeploymentDashboard::on_selection);
    connect(view_, &DeploymentsView::log_requested, this, &DeploymentDashboard::open_log);
    connect(view_, &DeploymentsView::pause_toggle_requested, this, &DeploymentDashboard::toggle_pause);
    connect(view_, &DeploymentsView::stop_requested, this, &DeploymentDashboard::request_stop);
    connect(view_, &DeploymentsView::remove_requested, this, &DeploymentDashboard::request_remove);
    connect(view_, &DeploymentsView::stop_all_requested, this, &DeploymentDashboard::request_stop_all);
    connect(view_, &DeploymentsView::open_strategies_requested, this, &DeploymentDashboard::open_strategies_requested);
    connect(view_, &DeploymentsView::open_builder_requested, this, &DeploymentDashboard::open_builder_requested);

    connect_service();
    // Currency / number format changed: re-render the totals, rows and chart from what is
    // already loaded (no DB read, no engine call).
    fmt::on_format_changed(this, [this]() {
        update_summary(deployments_);
        push_rows();
        view_->set_curves(last_curves_);
    });
    LOG_DEBUG("AlgoTrading", "DeploymentDashboard constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void DeploymentDashboard::connect_service() {
    auto& engine = algo_ns::AlgoEngine::instance();
    connect(&engine, &algo_ns::AlgoEngine::deployments_loaded, this, &DeploymentDashboard::on_deployments_loaded);
    // Real-time per-tick updates — the rows refresh in place from these, not the 5 s DB poll
    // (which only fixes the deployment list structure).
    connect(&engine, &algo_ns::AlgoEngine::live_update, this, &DeploymentDashboard::on_live_update);
    connect(&engine, &algo_ns::AlgoEngine::deployment_started, this,
            [](const QString&) { algo_ns::AlgoEngine::instance().list_deployments(); });
    connect(&engine, &algo_ns::AlgoEngine::deployment_stopped, this, [](const QString& id) {
        LOG_INFO("AlgoTrading", QString("Deployment stopped: %1").arg(id));
        algo_ns::AlgoEngine::instance().list_deployments();
    });
    // Pause / resume are in-process states (the DB row stays 'running').
    connect(&engine, &algo_ns::AlgoEngine::deployment_state_changed, this,
            [this](const QString& id, const QString& status) {
                if (status == QLatin1String("paused"))
                    paused_.insert(id);
                else
                    paused_.remove(id);
                pause_pending_.remove(id);
                push_rows();
            });
    connect(&engine, &algo_ns::AlgoEngine::error_occurred, this, &DeploymentDashboard::on_error);
    connect(&AlgoTradingService::instance(), &AlgoTradingService::error_occurred, this, &DeploymentDashboard::on_error);

    // Cross-currency totals re-price when FX rates arrive.
    connect(&services::FxRateService::instance(), &services::FxRateService::rates_changed, this,
            [this]() { update_summary(deployments_); });

    // Ages ("updated 3 s ago", NO DATA) once a second; showEvent / hideEvent own start / stop.
    age_timer_ = new QTimer(this);
    age_timer_->setInterval(1000);
    connect(age_timer_, &QTimer::timeout, this, &DeploymentDashboard::refresh_ages);
    // P&L history (snapshots are written about once a minute per deployment).
    equity_timer_ = new QTimer(this);
    equity_timer_->setInterval(60000);
    connect(equity_timer_, &QTimer::timeout, this, &DeploymentDashboard::refresh_equity);
    // Arrow keys through the table: one fills / log read once the selection rests.
    history_timer_ = new QTimer(this);
    history_timer_->setSingleShot(true);
    history_timer_->setInterval(150);
    connect(history_timer_, &QTimer::timeout, this, [this]() { load_history(history_for_); });
}

void DeploymentDashboard::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    age_timer_->start();
    equity_timer_->start();
    refresh_ages();
    refresh_equity();
    if (!history_for_.isEmpty())
        history_timer_->start();
}

void DeploymentDashboard::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    age_timer_->stop();
    equity_timer_->stop();
    history_timer_->stop();
}

void DeploymentDashboard::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        push_rows(); // row texts (FLAT, states) come from this side too
    QWidget::changeEvent(event);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

QString DeploymentDashboard::currency_for(const AlgoDeployment& d) const {
    // P&L is the instrument's price move × quantity: its QUOTE currency. Unknown -> empty (no
    // unit printed), not the display currency: that would label it wrongly.
    const QString sym = d.symbol.isEmpty() ? d.underlying : d.symbol;
    return fincept::ui::algo::algo_currency_for(sym, d.exchange, d.broker_id);
}

QString DeploymentDashboard::alert_text(const AlgoDeployment& d) const {
    // A stopped or errored deployment that still holds a position is the most dangerous state
    // on this screen: nothing manages its stop-loss any more.
    const bool active_status = (d.status == "running" || d.status == "starting");
    QString alert = d.error_message;
    if (!active_status && d.position_qty != 0) {
        const QString qty = fmt::quantity(d.position_qty);
        const QString entry = dash_price(d.position_entry, d.symbol.isEmpty() ? d.underlying : d.symbol, currency_for(d));
        const QString unmanaged =
            d.mode == QLatin1String("live")
                ? tr("OPEN LIVE POSITION NO LONGER MANAGED: %1 %2 @ %3 - stop-loss/take-profit are not monitored. "
                     "Close or protect it at the broker.")
                      .arg(qty, d.position_side.toUpper(), entry)
                : tr("Paper position left open when the deployment stopped: %1 %2 @ %3.")
                      .arg(qty, d.position_side.toUpper(), entry);
        alert = alert.isEmpty() ? unmanaged : alert + QLatin1Char('\n') + unmanaged;
    }
    return alert;
}

const AlgoDeployment* DeploymentDashboard::find(const QString& id) const {
    for (const auto& d : deployments_)
        if (d.id == id)
            return &d;
    return nullptr;
}

bool DeploymentDashboard::has_position(const QString& id) const {
    if (live_.contains(id))
        return live_.value(id).metrics.current_position_qty != 0;
    const AlgoDeployment* d = find(id);
    return d && d->position_qty != 0;
}

// ── Rows ────────────────────────────────────────────────────────────────────

void DeploymentDashboard::push_rows() {
    auto& accts = fincept::trading::AccountManager::instance();
    QVector<DeploymentsViewRow> rows;
    rows.reserve(deployments_.size());
    for (const auto& d : deployments_) {
        DeploymentsViewRow r;
        r.id = d.id;
        r.strategy = d.strategy_name;
        r.symbol = d.symbol.isEmpty() ? d.underlying : d.symbol;
        r.timeframe = d.timeframe;
        r.mode = d.mode;
        r.side = d.entry_side.isEmpty() ? QStringLiteral("BUY") : d.entry_side;
        r.status = d.status;
        r.currency = currency_for(d);
        const bool active = d.status == QLatin1String("running") || d.status == QLatin1String("starting");
        r.paused = active && paused_.contains(d.id);
        r.pause_pending = pause_pending_.contains(d.id);
        r.stop_pending = stop_pending_.contains(d.id);
        // Figures: the live snapshot is fresher than the DB poll.
        r.ltp = d.current_price;
        r.net_pnl = d.total_pnl + d.unrealized_pnl;
        r.unrealized = d.unrealized_pnl;
        r.today_pnl = d.daily_pnl;
        r.fees = d.total_fees;
        r.max_dd = d.max_drawdown;
        r.win_rate = d.win_rate;
        r.trades = d.total_trades;
        r.pos_qty = d.position_qty;
        r.pos_side = d.position_side;
        r.pos_entry = d.position_entry;
        r.risk_halted = d.risk_halted;
        r.halt_reason = d.halt_reason;
        if (live_.contains(d.id)) {
            const auto& snap = live_.value(d.id);
            const auto& m = snap.metrics;
            r.ltp = snap.current_price;
            r.net_pnl = m.total_pnl + m.unrealized_pnl;
            r.unrealized = m.unrealized_pnl;
            r.today_pnl = m.daily_pnl;
            r.fees = m.total_fees;
            r.max_dd = m.max_drawdown;
            r.win_rate = m.win_rate;
            r.trades = m.total_trades;
            r.trades_today = m.trades_today;
            r.pos_qty = m.current_position_qty;
            r.pos_side = m.current_position_side;
            r.pos_entry = m.current_position_entry;
            r.risk_halted = m.risk_halted;
            r.halt_reason = m.halt_reason;
            r.last_update_ms = snap.last_update_ms;
            r.last_signal_ms = m.last_signal_time;
        }
        r.started_ms = dash_started_ms(d.created_at);
        r.spark = spark_.value(d.id);
        r.alert = alert_text(d);
        r.broker_id = d.broker_id;
        r.broker_connected = !d.broker_account_id.isEmpty() &&
                             accts.connection_state(d.broker_account_id) == fincept::trading::ConnectionState::Connected;
        r.quantity = d.quantity;
        r.max_order_value = d.max_order_value;
        r.max_daily_loss = d.max_daily_loss;
        r.risk_limits = d.risk_limits;
        rows.append(r);
    }
    view_->set_rows(rows);
    // The one PAUSE / RESUME button follows the selected deployment's state.
    const QString sel = view_->selected_id();
    dd_mark_pause_toggle(view_->pause_button(), paused_.contains(sel));
}

void DeploymentDashboard::refresh_ages() {
    view_->tick();
}

// ── Summary ribbon ──────────────────────────────────────────────────────────

void DeploymentDashboard::update_summary(const QVector<AlgoDeployment>& deployments) {
    ui::KpiStrip* ribbon = view_->ribbon();
    int active = 0, active_live = 0, halted = 0, positions = 0, orders_today = 0;
    // Paper P&L is simulated money and live P&L is real money: never one total. Within each,
    // amounts are kept per currency and only converted (FX) when several are involved.
    QMap<QString, double> live_pnl, paper_pnl, paper_today, fees;
    long long trades = 0;
    double wins = 0;
    for (const auto& d0 : deployments) {
        AlgoDeployment d = d0;
        int today_trades = 0;
        if (live_.contains(d.id)) { // fresher than the DB poll
            const auto& m = live_.value(d.id).metrics;
            d.total_pnl = m.total_pnl;
            d.unrealized_pnl = m.unrealized_pnl;
            d.total_trades = m.total_trades;
            d.win_rate = m.win_rate;
            d.total_fees = m.total_fees;
            d.risk_halted = m.risk_halted;
            d.daily_pnl = m.daily_pnl;
            d.position_qty = m.current_position_qty;
            today_trades = m.trades_today;
        }
        const bool running = (d.status == "running" || d.status == "starting");
        if (running) {
            ++active;
            if (d.is_live())
                ++active_live;
            orders_today += today_trades;
        }
        if (d.risk_halted && running)
            ++halted;
        if (d.position_qty != 0)
            ++positions;
        const QString ccy = currency_for(d);
        (d.is_live() ? live_pnl : paper_pnl)[ccy] += d.total_pnl + d.unrealized_pnl;
        if (!d.is_live())
            paper_today[ccy] += d.daily_pnl;
        fees[ccy] += d.total_fees;
        trades += d.total_trades;
        wins += std::round(d.win_rate * d.total_trades / 100.0);
    }

    // A per-currency sum as one figure. One currency: that amount, unconverted. Several: each
    // converted to the display currency, the rate and its time in the tooltip; a currency with
    // no rate yet (or an unknown one) is left OUT of the total — never taken 1:1 — and named
    // under the figure. rates_changed() re-renders when it lands.
    const QString base = fmt::display_currency();
    struct DashTotal {
        QString text;
        double sign = 0;  // for the tone
        QString tooltip;  // one line per currency
        QString excluded; // sub-label naming what the total leaves out; empty = complete
    };
    auto total_of = [&](const QMap<QString, double>& by_ccy, bool signed_) -> DashTotal {
        DashTotal t;
        auto own = [signed_](double v, const QString& ccy) {
            return signed_ ? fmt::signed_money(v, ccy) : fmt::money(v, ccy);
        };
        if (by_ccy.isEmpty()) {
            t.text = fmt::money(0, base);
            return t;
        }
        if (by_ccy.size() == 1) {
            t.sign = by_ccy.first();
            t.text = own(by_ccy.first(), by_ccy.firstKey());
            if (by_ccy.firstKey().isEmpty())
                t.tooltip = tr("The currency of these deployments is not known: shown without a unit.");
            return t;
        }
        double sum = 0;
        bool any = false;
        fmt::ConvertedMoney agg; // the total's "≈" marker follows its stale / approximate legs
        agg.currency = base;
        agg.converted = true;
        QStringList lines, missing;
        for (auto it = by_ccy.constBegin(); it != by_ccy.constEnd(); ++it) {
            const QString& ccy = it.key();
            if (ccy.isEmpty()) {
                missing << tr("unknown currency");
                lines << tr("%1 (currency unknown) — not in the total").arg(own(it.value(), ccy));
                continue;
            }
            const fmt::ConvertedMoney c = services::fx::convert_for_display(it.value(), ccy, base);
            if (!c.ok()) {
                missing << ccy;
                lines << tr("%1 — no FX rate yet, not in the total").arg(own(it.value(), ccy));
                continue;
            }
            sum += c.amount;
            any = true;
            agg.stale = agg.stale || c.stale;
            agg.approximate = agg.approximate || c.approximate;
            const QString note = fmt::fx_note(c);
            lines << (note.isEmpty() ? own(it.value(), ccy)
                                     : tr("%1 = %2 (%3)").arg(own(it.value(), ccy),
                                                              fmt::converted_money(c, fmt::Compact::None, signed_), note));
        }
        t.tooltip = lines.join(QLatin1Char('\n'));
        if (!missing.isEmpty())
            t.excluded = tr("excl. %1").arg(missing.join(QStringLiteral(", ")));
        if (!any) {
            t.text = fmt::placeholder(); // nothing could be converted: no number at all
            return t;
        }
        agg.amount = sum;
        t.sign = sum;
        t.text = fmt::converted_money(agg, fmt::Compact::None, signed_);
        return t;
    };
    auto tone_of = [](double v) {
        return v > 0 ? ui::StatTile::Tone::Positive : v < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral;
    };

    if (auto* t = ribbon->tile(QStringLiteral("running"))) {
        t->set_value(active_live > 0 ? tr("%1  ·  %2 LIVE").arg(active).arg(active_live) : QString::number(active));
        t->set_tone(active_live > 0 ? ui::StatTile::Tone::Danger : ui::StatTile::Tone::Neutral);
    }
    auto pnl_tile = [&](const QString& key, const QMap<QString, double>& by_ccy, const QString& head) {
        if (auto* t = ribbon->tile(key)) {
            const DashTotal tot = total_of(by_ccy, true);
            t->set_value(tot.text);
            t->set_tone(tone_of(tot.sign));
            t->set_sub_label(tot.excluded);
            t->set_tooltip_text(tot.tooltip.isEmpty() ? head : head + QLatin1Char('\n') + tot.tooltip);
        }
    };
    pnl_tile(QStringLiteral("live"), live_pnl, tr("Realized + unrealized, net of estimated fees — real money"));
    pnl_tile(QStringLiteral("paper"), paper_pnl, tr("Realized + unrealized, net of simulated fees"));
    pnl_tile(QStringLiteral("paper_today"), paper_today,
             tr("Realized P&L of paper deployments on the current exchange trading day"));
    if (auto* t = ribbon->tile(QStringLiteral("positions")))
        t->set_value(QString::number(positions));
    if (auto* t = ribbon->tile(QStringLiteral("orders")))
        t->set_value(QString::number(orders_today));
    if (auto* t = ribbon->tile(QStringLiteral("winrate"))) {
        // Trade-weighted: total winning trades over total trades (an average of per-deployment
        // rates let a 1-trade deployment weigh as much as a 500-trade one).
        t->set_value(trades > 0 ? fmt::pct(100.0 * wins / trades, 1, false) : fmt::placeholder());
        t->set_sub_label(tr("%n trade(s)", nullptr, static_cast<int>(trades)));
    }
    if (auto* t = ribbon->tile(QStringLiteral("fees"))) {
        const DashTotal tot = total_of(fees, false);
        t->set_value(tot.text);
        t->set_sub_label(tot.excluded);
        t->set_tooltip_text(tr("Paper: simulated fees. Live: ESTIMATED from the cost model.") +
                            (tot.tooltip.isEmpty() ? QString() : QLatin1Char('\n') + tot.tooltip));
    }
    if (auto* t = ribbon->tile(QStringLiteral("halted"))) {
        t->set_value(QString::number(halted));
        t->set_tone(halted > 0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
    }
}

// ── P&L history ─────────────────────────────────────────────────────────────

void DeploymentDashboard::refresh_equity() {
    if (deployments_.isEmpty() || !isVisible())
        return;
    QStringList ids;
    for (const auto& d : deployments_)
        ids << d.id;
    const int seq = ++equity_seq_;
    QPointer<DeploymentDashboard> self = this;
    const qint64 since = QDateTime::currentMSecsSinceEpoch() - kDashEquityWindowMs;
    (void)QtConcurrent::run([self, ids, since, seq]() {
        const auto curves = monitor_store::equity_curves(ids, since, 1500);
        QMetaObject::invokeMethod(
            qApp,
            [self, curves, seq]() {
                if (self && seq == self->equity_seq_)
                    self->apply_equity(curves);
            },
            Qt::QueuedConnection);
    });
}

void DeploymentDashboard::apply_equity(const QHash<QString, QVector<AlgoEquityPoint>>& curves) {
    if (&curves != &last_curves_)
        last_curves_ = curves; // a format change redraws from this
    spark_.clear();
    for (auto it = curves.constBegin(); it != curves.constEnd(); ++it) {
        QVector<double> v;
        v.reserve(it.value().size());
        for (const auto& p : it.value())
            v.append(p.equity);
        spark_.insert(it.key(), v);
    }
    push_rows();
    view_->set_curves(last_curves_);
}

// ── Selection: fills, log, rule status ──────────────────────────────────────

void DeploymentDashboard::on_selection(const QString& id) {
    dd_mark_pause_toggle(view_->pause_button(), paused_.contains(id));
    push_conditions(id);
    history_for_ = id;
    if (id.isEmpty())
        return;
    view_->set_history(id, {}, {}, true);
    if (isVisible())
        history_timer_->start();
}

void DeploymentDashboard::load_history(const QString& id) {
    if (id.isEmpty())
        return;
    const int seq = ++history_seq_;
    QPointer<DeploymentDashboard> self = this;
    (void)QtConcurrent::run([self, id, seq]() {
        const auto trades = monitor_store::trades(id, 500);
        const auto events = monitor_store::events(id, 500);
        QMetaObject::invokeMethod(
            qApp,
            [self, id, seq, trades, events]() {
                if (self && seq == self->history_seq_ && id == self->history_for_)
                    self->view_->set_history(id, trades, events, false);
            },
            Qt::QueuedConnection);
    });
}

void DeploymentDashboard::push_conditions(const QString& id) {
    QVector<DeploymentsCondRow> rows;
    qint64 as_of = 0;
    if (live_.contains(id)) {
        const auto& snap = live_.value(id);
        as_of = snap.conditions_as_of_ms;
        // Entry rows stay green once a position is open (they matched and fired) even when the
        // price has since moved off the level.
        const bool in_position = snap.metrics.current_position_qty != 0;
        for (int i = 0; i < snap.conditions.size(); ++i) {
            const auto& c = snap.conditions[i];
            DeploymentsCondRow r;
            r.key = QStringLiteral("%1|%2").arg(c.section).arg(i);
            r.section = c.section;
            r.label = c.label;
            r.op = c.op;
            r.computed = c.computed;
            r.target = c.target;
            r.met = c.met;
            r.executed = c.section == QLatin1String("entry") && in_position;
            rows.append(r);
        }
    }
    view_->set_conditions(id, rows, as_of, activity_.value(id));
}

// ── Slots ───────────────────────────────────────────────────────────────────

void DeploymentDashboard::on_deployments_loaded(QVector<AlgoDeployment> deployments) {
    // The STRUCTURAL refresh (DB poll): rows are upserted by id, so live values stay on
    // screen between snapshots (no blink) and the selection / sort / scroll survive.
    const bool first = !loaded_once_;
    QStringList old_ids, new_ids;
    for (const auto& d : deployments_)
        old_ids << d.id;
    deployments_ = deployments;
    loaded_once_ = true;
    for (const auto& d : deployments)
        new_ids << d.id;
    // A confirmed STOP is done once the engine reports the row as no longer running.
    for (const auto& d : deployments) {
        const bool active = d.status == QLatin1String("running") || d.status == QLatin1String("starting");
        if (!active) {
            stop_pending_.remove(d.id);
            paused_.remove(d.id);
            pause_pending_.remove(d.id);
        }
    }
    for (auto it = stop_pending_.begin(); it != stop_pending_.end();)
        it = new_ids.contains(*it) ? std::next(it) : stop_pending_.erase(it);

    update_summary(deployments);
    view_->set_page(deployments.isEmpty() ? DeploymentsView::Page::Empty : DeploymentsView::Page::Table);
    push_rows();
    kit::set_state(view_->status_bar(), kit::RunState::Ready);
    view_->status_bar()->set_text(QStringLiteral("asof"),
                                  tr("as of %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    if ((first || old_ids != new_ids) && isVisible())
        refresh_equity();
    LOG_DEBUG("AlgoTrading", QString("Dashboard rows: %1 deployments").arg(deployments.size()));
}

void DeploymentDashboard::on_live_update(const QString& deployment_id, const fincept::algo::AlgoLiveSnapshot& snap) {
    live_.insert(deployment_id, snap);
    if (!snap.note.isEmpty()) {
        QStringList& lines = activity_[deployment_id];
        lines.prepend(QStringLiteral("%1  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), snap.note));
        while (lines.size() > kDashActivityLines)
            lines.removeLast();
    }
    if (!find(deployment_id))
        return; // structure not loaded yet — the next list_deployments() adds it
    push_rows();
    if (view_->selected_id() == deployment_id)
        push_conditions(deployment_id);
}

void DeploymentDashboard::on_error(const QString& context, const QString& msg) {
    kit::set_state(view_->status_bar(), kit::RunState::Failed, tr("ERROR"));
    view_->status_bar()->show_notice(tr("Error [%1]: %2").arg(context, msg.left(200)), QString(), {}, 9000,
                                     ui::ScreenStatusBar::Tone::Danger);
}

// ── Actions (every one confirmed where it can trade or delete) ──────────────

void DeploymentDashboard::toggle_pause(const QString& dep_id) {
    const AlgoDeployment* d = find(dep_id);
    if (!d)
        return;
    if (paused_.contains(dep_id)) {
        algo_ns::AlgoEngine::instance().resume_deployment(dep_id);
    } else {
        if (has_position(dep_id)) {
            const auto ans = QMessageBox::warning(
                this, tr("Pause deployment"),
                tr("\"%1\" holds an open position. While paused, its stop-loss and take-profit are NOT "
                   "monitored.\n\nPause anyway?")
                    .arg(d->strategy_name),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (ans != QMessageBox::Yes)
                return;
        }
        algo_ns::AlgoEngine::instance().pause_deployment(dep_id);
    }
    pause_pending_.insert(dep_id); // cleared by deployment_state_changed
    push_rows();
}

void DeploymentDashboard::request_stop(const QString& dep_id) {
    const AlgoDeployment* found = find(dep_id);
    if (!found || stop_pending_.contains(dep_id))
        return; // gone, or a stop is already on its way
    const AlgoDeployment d = *found;
    const bool live = d.is_live();
    const auto limits = algo_ns::DeploymentRiskLimits::from_json(d.risk_limits);
    const bool has_pos = has_position(dep_id);

    // A live strategy that is stopped mid-position leaves that position unmanaged at the
    // broker, so confirm — say whether it is live — and offer to close it first through the
    // normal order path.
    QMessageBox box(this);
    box.setIcon(live ? QMessageBox::Warning : QMessageBox::Question);
    box.setWindowTitle(tr("Stop deployment"));
    box.setText(live ? tr("Stop LIVE deployment \"%1\" on %2?\n\n"
                          "The runner will stop placing orders. Unless you close it below, any position it holds "
                          "at the broker is NOT closed — you would have to flatten it yourself.")
                           .arg(d.strategy_name, d.symbol)
                     : tr("Stop paper deployment \"%1\" on %2?").arg(d.strategy_name, d.symbol));
    QCheckBox* flatten = nullptr;
    if (has_pos) {
        flatten = new QCheckBox(live ? tr("Close the open position first (a market order through the broker — "
                                          "approval rules apply)")
                                     : tr("Close the open position first"),
                                &box);
        flatten->setChecked(limits.flatten_on_stop);
        // Ticking it makes the stop send a closing order.
        flatten->setProperty("fincept_agent_risk", "order");
        box.setCheckBox(flatten);
    }
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    // STOP itself carries no marker; with an open position the Yes can close it (an order).
    if (flatten)
        if (QAbstractButton* yes = box.button(QMessageBox::Yes))
            yes->setProperty("fincept_agent_risk", "order");
    if (box.exec() != QMessageBox::Yes)
        return;
    const bool close_first = flatten && flatten->isChecked();
    stop_pending_.insert(dep_id);
    push_rows();
    if (close_first)
        algo_ns::AlgoEngine::instance().flatten_and_stop(dep_id);
    else
        algo_ns::AlgoEngine::instance().stop_deployment(dep_id);
    LOG_INFO("AlgoTrading", QString("Stop requested: %1 (flatten=%2)").arg(dep_id).arg(close_first));
}

void DeploymentDashboard::request_remove(const QString& dep_id) {
    const AlgoDeployment* found = find(dep_id);
    if (!found)
        return;
    const QString name = found->strategy_name;
    const QString symbol = found->symbol.isEmpty() ? found->underlying : found->symbol;
    const bool live = found->is_live();
    const double position_qty = found->position_qty;
    // Deletes the deployment's whole record at once: ask first, saying what goes.
    QString text = (live ? tr("Remove the LIVE deployment \"%1\" on %2?") : tr("Remove the paper deployment \"%1\" on %2?"))
                       .arg(name, symbol) +
                   QStringLiteral("\n\n") +
                   tr("This permanently deletes its trade history, P&L metrics, order signals, equity curve and event "
                      "log. The strategy itself stays in My Strategies.");
    if (position_qty != 0)
        text += QStringLiteral("\n\n") +
                (live ? tr("It still records an open position of %1. Removing it does not close that position at the "
                           "broker.")
                      : tr("It still records an open position of %1. Removing it does not close that position."))
                    .arg(fmt::quantity(position_qty));
    auto* box = new QMessageBox(QMessageBox::Warning, tr("Remove deployment"), text, QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    auto* confirm = box->addButton(tr("Remove deployment"), QMessageBox::DestructiveRole);
    confirm->setProperty(fincept::core::agent::kAgentRiskProperty, "delete"); // as REMOVE
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    connect(box, &QMessageBox::buttonClicked, this, [dep_id, confirm](QAbstractButton* clicked) {
        if (clicked != confirm)
            return;
        algo_ns::AlgoEngine::instance().remove_deployment(dep_id);
        LOG_INFO("AlgoTrading", QString("Remove requested: %1").arg(dep_id));
    });
    box->open();
}

void DeploymentDashboard::request_stop_all() {
    // Count what is actually at risk so the prompt is concrete, and never fire a blanket
    // stop without confirmation.
    int running = 0;
    int live_running = 0;
    for (const auto& d : deployments_) {
        if (d.status != QLatin1String("running") && d.status != QLatin1String("starting"))
            continue;
        ++running;
        if (d.is_live())
            ++live_running;
    }
    if (running == 0) {
        QMessageBox::information(this, tr("Stop all"), tr("No deployments are running."));
        return;
    }
    const QString body = live_running > 0 ? tr("Stop ALL %1 deployment(s), including %2 LIVE one(s)?\n\n"
                                               "Open positions are NOT closed — flatten them yourself in Equity "
                                               "Trading.")
                                                .arg(running)
                                                .arg(live_running)
                                          : tr("Stop ALL %1 deployment(s)?").arg(running);
    const auto ans =
        QMessageBox::question(this, tr("Stop all"), body, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ans != QMessageBox::Yes)
        return;
    for (const auto& d : deployments_)
        if (d.status == QLatin1String("running") || d.status == QLatin1String("starting"))
            stop_pending_.insert(d.id);
    push_rows();
    algo_ns::AlgoEngine::instance().stop_all();
    LOG_INFO("AlgoTrading", "Stop all deployments requested");
}

void DeploymentDashboard::open_log(const QString& deployment_id) {
    const AlgoDeployment* found = find(deployment_id);
    if (!found)
        return;
    AlgoDeployment dep = *found;
    if (live_.contains(dep.id)) {
        const auto& m = live_.value(dep.id).metrics;
        dep.total_pnl = m.total_pnl;
        dep.unrealized_pnl = m.unrealized_pnl;
    }
    auto* dlg = new DeploymentLogDialog(dep, currency_for(dep), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

} // namespace fincept::screens
