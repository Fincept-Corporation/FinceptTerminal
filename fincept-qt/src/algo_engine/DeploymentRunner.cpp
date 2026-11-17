// src/algo_engine/DeploymentRunner.cpp
#include "algo_engine/DeploymentRunner.h"

#include "algo_engine/AlgoMarket.h"
#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ConditionEvaluator.h"
#include "algo_engine/fno/FnoExecution.h"
#include "core/logging/Logger.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "services/options/OptionChainService.h"
#include "storage/sqlite/Database.h"

using fincept::Database;
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/TradingTypes.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::algo {

// Minimum gap between a rejected order and the next order attempt (see emit_order_signal).
static constexpr int64_t kRunnerRejectBackoffMs = 5000;
// A paper order waits this long for its next quote before it expires unfilled.
static constexpr int64_t kRunnerPaperWaitMs = 15 * 60000;
// Equity snapshots: at most one per interval (plus one per fill).
static constexpr int64_t kRunnerEquitySnapMs = 60000;
// request_flatten() gives up (and reports) after this long without a flat book.
static constexpr int64_t kRunnerFlattenLimitMs = 3 * 60000;

DeploymentRunner::DeploymentRunner(const services::algo::AlgoDeployment& deployment,
                                   const services::algo::AlgoStrategy& strategy, QObject* parent)
    : QObject(parent),
      deployment_(deployment),
      strategy_(strategy),
      timeframe_(timeframe_from_string(deployment.timeframe.isEmpty() ? strategy.timeframe : deployment.timeframe)) {

    // "live" timeframe → evaluate entry/exit on every quote (tick), not on candle
    // close. (timeframe_ itself falls back to M5 for aggregation/warm-up history.)
    const QString tf_str = deployment.timeframe.isEmpty() ? strategy.timeframe : deployment.timeframe;
    live_mode_ = (tf_str.compare(QStringLiteral("live"), Qt::CaseInsensitive) == 0);

    // Exchange calendar of the feed symbol: session-aligned live bars, the exchange
    // trading date for the daily-loss rollover, entry-window checks.
    venue_ = algo_venue(feed_symbol(), deployment.exchange);
    cost_model_ = CostModel::from_json(deployment.cost_model.isEmpty() ? strategy.cost_model : deployment.cost_model);
    limits_ = DeploymentRiskLimits::from_json(deployment.risk_limits);

    // Capacity 500 so long indicators (SMA200) + a crossover lookback fit.
    // F&O deployments carry no single symbol: their conditions run on the
    // underlying's bars (feed_symbol()).
    aggregator_ = std::make_unique<CandleAggregator>(feed_symbol(), timeframe_, 500, this, venue_);
    connect(aggregator_.get(), &CandleAggregator::candle_closed, this, &DeploymentRunner::on_candle_closed);

    position_mgr_ = std::make_unique<PositionManager>(deployment.id, strategy.stop_loss, strategy.take_profit,
                                                      strategy.trailing_stop, deployment.max_order_value,
                                                      deployment.max_daily_loss);
    position_mgr_->set_max_drawdown_limit(limits_.max_drawdown);

    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setInterval(5000);
    connect(heartbeat_timer_, &QTimer::timeout, this, &DeploymentRunner::on_heartbeat);

    if (is_fno()) {
        fno_spot_timer_ = new QTimer(this);
        fno_spot_timer_->setInterval(2000);
        connect(fno_spot_timer_, &QTimer::timeout, this, &DeploymentRunner::on_fno_spot_poll);
    }
}

QString DeploymentRunner::feed_symbol() const {
    const QString sym = deployment_.symbol.trimmed();
    if (!sym.isEmpty())
        return sym;
    return deployment_.underlying.trimmed();
}

bool DeploymentRunner::is_fno() const {
    return deployment_.instrument_type != QLatin1String("equity");
}

double DeploymentRunner::current_mark() const {
    if (last_tick_price_ > 0)
        return last_tick_price_;
    return position_mgr_->metrics().current_price;
}

QString DeploymentRunner::fno_chain_expiry() {
    if (!resolved_expiry_.isEmpty() && (position_mgr_->has_legs() || !pending_orders_.isEmpty()))
        return resolved_expiry_; // open / in-flight basket: keep marking its own series
    const QDate today = QDate::currentDate();
    if (!spot_expiry_.isEmpty() && spot_expiry_day_ == today)
        return spot_expiry_;
    QString exp_mode = QStringLiteral("WEEKLY");
    QString exp_val;
    const auto rules = fincept::algo::fno::fno_legs_from_json(strategy_.legs);
    if (!rules.isEmpty()) {
        exp_mode = rules.first().expiry_mode.toUpper();
        exp_val = rules.first().expiry_value;
    }
    spot_expiry_ = fincept::algo::fno::resolve_expiry(
        exp_mode, exp_val,
        fincept::services::options::OptionChainService::instance().list_expiries(deployment_.broker_id,
                                                                                  deployment_.underlying),
        today);
    spot_expiry_day_ = spot_expiry_.isEmpty() ? QDate() : today;
    return spot_expiry_;
}

void DeploymentRunner::on_fno_spot_poll() {
    if (!running_.load() || paused_.load() || stop_requested_.load() || !fno_bridge_)
        return;
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    // A live broker quote on the underlying wins while it is flowing; the chain
    // spot only fills in when that feed is silent.
    if (last_feed_tick_ms_ > 0 && now - last_feed_tick_ms_ < 10000)
        return;
    const QString expiry = fno_chain_expiry();
    if (expiry.isEmpty())
        return; // expiry list not loaded yet — the no-data watchdog reports a lasting gap
    fno_bridge_->ensure_chain(deployment_.broker_id, deployment_.underlying, expiry);
    const auto chain = fno_bridge_->snapshot(deployment_.broker_id, deployment_.underlying, expiry);
    // Feed only a NEW and RECENT chain: re-feeding an old snapshot would hide a
    // dead data source from the 30 s no-data watchdog.
    if (chain.spot <= 0 || chain.timestamp_ms <= last_chain_spot_ts_ || now - chain.timestamp_ms > 30000)
        return;
    last_chain_spot_ts_ = chain.timestamp_ms;
    fincept::trading::BrokerQuote q;
    q.symbol = deployment_.underlying;
    q.ltp = chain.spot;
    q.timestamp = now;
    on_tick_data(QVariant::fromValue(q));
}

DeploymentRunner::~DeploymentRunner() {
    // Detach every listener first: stop() emits status_changed, and AlgoEngine's
    // handler would otherwise act on (and deleteLater) an object mid-destruction.
    QObject::disconnect(this, nullptr, nullptr, nullptr);
    if (running_.load())
        stop();
}

void DeploymentRunner::start() {
    if (running_.load() || stop_requested_.load())
        return; // a stop raced ahead of the queued start

    running_ = true;
    paused_ = false;
    last_heartbeat_ms_ = QDateTime::currentMSecsSinceEpoch();

    // Resume any open position + cumulative metrics persisted before a restart.
    restore_state_from_db();
    // Daily limits belong to the EXCHANGE trading day (not the machine's local date).
    position_mgr_->reset_daily_if_new_day(today_key());
    // Bars close on time even when the next tick is late (quiet market).
    aggregator_->start_close_timer();

    // Warm the aggregator with historical candles before going live, so indicators
    // are valid immediately. Without this the buffer starts empty and the strategy
    // can't trigger until hundreds of live bars accumulate. Live ticks are
    // subscribed only after the backfill attempt (warm_from replaces the buffer).
    const QString tf = timeframe_to_string(timeframe_);
    const int lookback = services::algo::algo_default_lookback_days(tf);
    const DataSource src = deployment_.broker_id.isEmpty() ? DataSource::YFinance : DataSource::Auto;
    QPointer<DeploymentRunner> self = this;
    // The deployment's exchange is the listing venue of its symbol (Yahoo suffix +
    // exchange calendar for the warm-up bars).
    CandleDataFetcher::instance().fetch(
        feed_symbol(), tf, lookback, src, deployment_.broker_id, deployment_.broker_account_id,
        [self](bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
            if (!self || !self->running_.load())
                return;
            if (ok && !candles.isEmpty()) {
                self->aggregator_->warm_from(candles);
                LOG_INFO("AlgoEngine", QString("Deployment %1 warmed with %2 historical candles")
                                           .arg(self->deployment_.id)
                                           .arg(candles.size()));
            } else {
                LOG_WARN("AlgoEngine", QString("Deployment %1 backfill failed (%2) — warming from live ticks only")
                                           .arg(self->deployment_.id, err));
            }
            self->start_market_data();
        },
        deployment_.exchange);

    heartbeat_timer_->start();
    if (fno_spot_timer_)
        fno_spot_timer_->start();

    update_deployment_status(QStringLiteral("running"));
    emit status_changed(deployment_.id, QStringLiteral("running"));
    LOG_INFO("AlgoEngine",
             QString("Deployment %1 started: %2 on %3").arg(deployment_.id, strategy_.name, feed_symbol()));
}

void DeploymentRunner::pause() {
    if (!running_.load())
        return;
    paused_ = true;
    // Note: the DB status column is deliberately left as 'running'. recover_orphaned()
    // only resumes rows in ('running','starting','error','crashed'), so persisting
    // 'paused' would strand the deployment permanently after a restart. Pause is an
    // in-process state; a restart resumes the deployment.
    emit status_changed(deployment_.id, QStringLiteral("paused"));
    log_event(QStringLiteral("info"), QStringLiteral("Paused — rules, stop-loss and take-profit are not monitored"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 paused").arg(deployment_.id));
}

void DeploymentRunner::resume() {
    if (!running_.load())
        return;
    paused_ = false;
    // No quotes arrive while paused, so last_heartbeat_ms_ is stale — re-stamp it
    // or the heartbeat immediately flags "no market data in 30s" and errors the
    // deployment out.
    last_heartbeat_ms_ = QDateTime::currentMSecsSinceEpoch();
    // A resume is an explicit human decision to restart a halted deployment, so it
    // also clears the daily-loss halt (which is otherwise only cleared by the day
    // rollover in on_heartbeat). Without this, resume() cannot actually resume the
    // one case that needs it. Logged at WARN because it grants a fresh daily loss
    // budget for the remainder of the day.
    if (position_mgr_->is_paused()) {
        const QString why = position_mgr_->metrics().halt_reason;
        // Clears the daily-loss latch AND a max-drawdown halt (re-basing its peak).
        position_mgr_->clear_halts();
        LOG_WARN("AlgoEngine", QString("Deployment %1: manual resume cleared the %2 halt — daily P&L reset to 0 for "
                                       "the rest of the day, drawdown measured from here")
                                   .arg(deployment_.id, why.isEmpty() ? QStringLiteral("risk") : why));
        log_event(QStringLiteral("risk"), QStringLiteral("Manual resume cleared the %1 halt").arg(why));
    }
    emit status_changed(deployment_.id, QStringLiteral("running"));
    log_event(QStringLiteral("info"), QStringLiteral("Resumed"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 resumed").arg(deployment_.id));
}

void DeploymentRunner::stop() {
    if (!running_.load())
        return;

    running_ = false;
    paused_ = false;
    heartbeat_timer_->stop();
    if (fno_spot_timer_)
        fno_spot_timer_->stop();
    aggregator_->stop_close_timer();
    stop_market_data();
    persist_metrics();
    persist_equity_snapshot(true);
    if (flatten_pending_) {
        flatten_pending_ = false;
        emit flatten_finished(deployment_.id, !in_position(), QStringLiteral("stopped"));
    }
    log_event(QStringLiteral("info"), in_position() ? QStringLiteral("Stopped with a position still open")
                                                    : QStringLiteral("Stopped"));
    update_deployment_status(QStringLiteral("stopped"));
    emit status_changed(deployment_.id, QStringLiteral("stopped"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 stopped").arg(deployment_.id));
}

void DeploymentRunner::start_market_data() {
    if (deployment_.broker_id.isEmpty() || deployment_.broker_account_id.isEmpty()) {
        LOG_WARN("AlgoEngine", QString("Deployment %1: no broker account attached — cannot source live "
                                       "market data (deploy again and pick a connected broker).")
                                   .arg(deployment_.id));
        return;
    }
    const QString sym = feed_symbol();
    if (sym.isEmpty()) {
        LOG_WARN("AlgoEngine", QString("Deployment %1: no symbol/underlying to source prices for").arg(deployment_.id));
        return;
    }
    LOG_INFO("AlgoEngine",
             QString("Deployment %1: subscribing to shared %2 feed on '%3'").arg(deployment_.id, sym, deployment_.broker_id));
    // Subscribe to the shared per-account quote feed. The symbol joins the account
    // stream (WS tick or fast REST poll), is published to broker:<id>:<acct>:quote:
    // <symbol>, and each quote is delivered here on the engine thread via
    // on_tick_data — the same entry point a WS tick used. One connection per
    // (account, symbol) is shared across the Equity watchlist and every algo.
    // F&O: this is the UNDERLYING; on_fno_spot_poll() supplies the chain's spot
    // whenever this feed is silent (e.g. an index the account stream cannot resolve).
    QPointer<DeploymentRunner> self = this;
    trading::DataStreamManager::instance().open_quote_feed(
        this, QStringLiteral("algo:") + deployment_.id, deployment_.broker_account_id, sym,
        [self](const trading::BrokerQuote& q) {
            if (self && self->running_.load() && !self->paused_.load()) {
                if (q.ltp > 0)
                    self->last_feed_tick_ms_ = QDateTime::currentMSecsSinceEpoch();
                self->on_tick_data(QVariant::fromValue(q));
            }
        });

    // F&O basket reattached across a restart: re-establish the chain stream and
    // re-pin the open legs so their LTPs flow for live marking. (On a fresh deploy
    // legs aren't open yet — evaluate_entry does the ensure_chain + pin at entry.)
    if (deployment_.instrument_type != QLatin1String("equity") && fno_bridge_ && position_mgr_->has_legs()) {
        fno_bridge_->ensure_chain(deployment_.broker_id, deployment_.underlying, resolved_expiry_);
        QStringList syms;
        for (const auto& l : position_mgr_->legs())
            syms << l.symbol;
        fno_bridge_->pin_legs(deployment_.broker_id, deployment_.underlying, resolved_expiry_, syms);
        LOG_INFO("AlgoEngine",
                 QString("Deployment %1: re-pinned %2 reattached F&O legs").arg(deployment_.id).arg(syms.size()));
    }
}

void DeploymentRunner::stop_market_data() {
    trading::DataStreamManager::instance().close_quote_feed(QStringLiteral("algo:") + deployment_.id,
                                                            deployment_.broker_account_id);
}

void DeploymentRunner::on_tick_data(const QVariant& data) {
    if (!running_.load() || paused_.load() || stop_requested_.load())
        return;

    last_heartbeat_ms_ = QDateTime::currentMSecsSinceEpoch();

    double price = 0;
    double volume = 0;
    int64_t timestamp = QDateTime::currentMSecsSinceEpoch();

    if (data.canConvert<fincept::trading::BrokerQuote>()) {
        auto quote = data.value<fincept::trading::BrokerQuote>();
        price = quote.ltp;
        volume = quote.volume;
        timestamp = quote.timestamp > 0 ? quote.timestamp : timestamp;
    } else if (data.canConvert<QVariantMap>()) {
        auto map = data.toMap();
        price = map.value("ltp", map.value("price")).toDouble();
        volume = map.value("volume").toDouble();
    } else {
        return;
    }

    if (price <= 0)
        return;

    if (!first_tick_logged_) {
        first_tick_logged_ = true;
        LOG_INFO("AlgoEngine", QString("Deployment %1: receiving live quotes (%2 = %3) from '%4'")
                                   .arg(deployment_.id, feed_symbol())
                                   .arg(price)
                                   .arg(deployment_.broker_id));
    }

    // PAPER: an order accepted by the simulator fills on this (the next) quote —
    // the next bar's open for a bar strategy, like the backtest — before anything
    // else reacts to the new price.
    fill_waiting_paper_order(price, timestamp);

    // ── F&O multi-leg basket: mark each leg from the chain snapshot, then run
    // basket risk (SL/TP/trailing). The single-symbol update_price() must be
    // SKIPPED here — it writes the single-position unrealized P&L (0 for a
    // basket) over the basket marks. ───────────────────────────────────────────
    const bool fno_basket =
        deployment_.instrument_type != QLatin1String("equity") && fno_bridge_ && position_mgr_->has_legs();
    if (fno_basket) {
        const auto chain = fno_bridge_->snapshot(deployment_.broker_id, deployment_.underlying, resolved_expiry_);
        const auto marks = fincept::algo::fno::leg_marks_from_chain(position_mgr_->legs(), chain);
        for (auto it = marks.constBegin(); it != marks.constEnd(); ++it)
            position_mgr_->update_leg_price(it.key(), it.value());

        auto risk_signal = position_mgr_->check_risk(price); // basket branch ignores price
        if (risk_signal) {
            risk_signal->account_id = deployment_.broker_account_id;
            risk_signal->symbol = deployment_.underlying;
            risk_signal->exchange = deployment_.exchange;
            risk_signal->product_type = deployment_.product_type;
            risk_signal->legs = fincept::algo::fno::build_exit_legs(position_mgr_->legs());
            emit_order_signal(*risk_signal);
        }
    } else {
        position_mgr_->update_price(price);

        auto risk_signal = position_mgr_->check_risk(price);
        if (risk_signal) {
            risk_signal->account_id = deployment_.broker_account_id;
            risk_signal->symbol = deployment_.symbol;
            risk_signal->exchange = deployment_.exchange;
            risk_signal->product_type = deployment_.product_type;
            risk_signal->price = price; // current market price for the simulated/real exit
            emit_order_signal(*risk_signal);
        }
    }

    aggregator_->on_tick(price, volume, timestamp);

    // Live timeframe: evaluate entry/exit on every tick against the live price.
    if (live_mode_)
        evaluate_live(price);

    // Always push a real-time snapshot to the Dashboard (throttled inside).
    emit_live_snapshot(price, QString());
    last_tick_price_ = price;
}

QVector<OhlcvCandle> DeploymentRunner::live_eval_window(double price) const {
    QVector<OhlcvCandle> candles = aggregator_->closed_candles();
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    // Append previous + current tick as the last two bars so a crossover is detected
    // tick-to-tick against the live price (prev < target ≤ current).
    if (last_tick_price_ > 0) {
        OhlcvCandle prev;
        prev.open = prev.high = prev.low = prev.close = last_tick_price_;
        prev.open_time = prev.close_time = now - 1;
        prev.volume = 0;
        prev.is_closed = true;
        candles.append(prev);
    }
    OhlcvCandle cur;
    cur.open = cur.high = cur.low = cur.close = price;
    cur.open_time = cur.close_time = now;
    cur.volume = 0;
    cur.is_closed = true;
    candles.append(cur);
    return candles;
}

void DeploymentRunner::evaluate_live(double price) {
    if (position_mgr_->is_paused())
        return;
    auto candles = live_eval_window(price);
    if (candles.size() < 2)
        return;
    if (in_position())
        evaluate_exit(candles);
    else if (!flatten_pending_)
        evaluate_entry(candles);
}

bool DeploymentRunner::in_position() const {
    return position_mgr_->has_position() || position_mgr_->has_legs();
}

namespace {
QString pretty_op(const QString& op) {
    if (op == "crosses_above")
        return QStringLiteral("crosses above");
    if (op == "crosses_below")
        return QStringLiteral("crosses below");
    if (op == "rising")
        return QStringLiteral("rising");
    if (op == "falling")
        return QStringLiteral("falling");
    if (op == "between")
        return QStringLiteral("between");
    return op; // >, <, >=, <=, ==
}
} // namespace

void DeploymentRunner::emit_live_snapshot(double price, const QString& note) {
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    // Throttle the periodic stream to ~1/s; event notes (fills, signals) bypass it.
    if (note.isEmpty() && now - last_emit_ms_ < 1000)
        return;
    last_emit_ms_ = now;

    AlgoLiveSnapshot snap;
    snap.deployment_id = deployment_.id;
    snap.current_price = price;
    snap.last_update_ms = now;
    auto m = position_mgr_->metrics();
    m.current_price = price;
    snap.metrics = m;
    snap.note = note;

    // Condition rows show what the ENGINE decided on. A live strategy evaluates
    // every tick on the live window; a bar strategy evaluates on the close of a
    // bar, so its rows are that evaluation (re-computing them per tick on a window
    // with synthetic tick bars showed values the engine never acted on).
    if (live_mode_) {
        snap.conditions = condition_rows(live_eval_window(price));
        snap.conditions_as_of_ms = now;
    } else {
        if (cond_rows_.isEmpty()) {
            const auto closed = aggregator_->closed_candles();
            if (closed.size() >= 2) {
                cond_rows_ = condition_rows(closed);
                cond_rows_ms_ = closed.last().close_time;
            }
        }
        snap.conditions = cond_rows_;
        snap.conditions_as_of_ms = cond_rows_ms_;
    }

    if (!note.isEmpty()) {
        const QString kind = note.contains(QLatin1String("REJECT")) ? QStringLiteral("reject")
                             : note.contains(QLatin1String("FILLED")) || note.contains(QLatin1String("filled"))
                                 ? QStringLiteral("fill")
                             : note.contains(QLatin1String("LIMIT")) || note.contains(QLatin1String("UNCONFIRMED"))
                                 ? QStringLiteral("risk")
                             : note.contains(QLatin1String("@ MARKET")) ? QStringLiteral("signal")
                                                                         : QStringLiteral("info");
        log_event(kind, note);
    }

    emit live_update(deployment_.id, snap);
}

QVector<AlgoConditionStatus> DeploymentRunner::condition_rows(const QVector<OhlcvCandle>& candles) const {
    QVector<AlgoConditionStatus> rows;
    if (candles.size() < 2)
        return rows;
    auto add_rows = [&](const QJsonArray& conds, const QString& logic, const QString& section) {
        if (conds.isEmpty())
            return;
        // full_details: list EVERY condition with its met flag — the short-circuited
        // form showed only the rules up to the first failing one, so rows appeared
        // and vanished between ticks.
        const auto res = ConditionEvaluator::evaluate_group(conds, logic, candles, /*full_details*/ true);
        for (const auto& d : res.details) {
            AlgoConditionStatus cs;
            cs.section = section;
            cs.op = d.op;
            cs.computed = d.computed_value;
            cs.target = d.target_value;
            cs.met = d.met;
            const QString fld = (d.field.isEmpty() || d.field == "value") ? QString() : (QStringLiteral(".") + d.field);
            cs.label =
                QStringLiteral("%1%2 %3 %4").arg(d.indicator, fld, pretty_op(d.op)).arg(d.target_value, 0, 'f', 2);
            rows.append(cs);
        }
    };
    add_rows(strategy_.entry_conditions, strategy_.entry_logic, QStringLiteral("entry"));
    add_rows(strategy_.exit_conditions, strategy_.exit_logic, QStringLiteral("exit"));
    return rows;
}

void DeploymentRunner::on_candle_closed(const OhlcvCandle& /*candle*/) {
    if (!running_.load() || paused_.load() || stop_requested_.load())
        return;
    if (position_mgr_->is_paused())
        return;
    if (live_mode_)
        return; // live mode evaluates per tick in on_tick_data

    auto candles = aggregator_->closed_candles();
    if (candles.size() < 20)
        return;

    // The rows the Dashboard shows are THIS evaluation (both groups, every leaf).
    cond_rows_ = condition_rows(candles);
    cond_rows_ms_ = candles.last().close_time;

    if (in_position()) {
        evaluate_exit(candles);
    } else if (!flatten_pending_) {
        evaluate_entry(candles);
    }

    auto m = position_mgr_->metrics();
    m.last_signal_time = QDateTime::currentMSecsSinceEpoch();
    emit metrics_updated(deployment_.id, m);
    emit_live_snapshot(current_mark(), QString());
}

void DeploymentRunner::evaluate_entry(const QVector<OhlcvCandle>& candles) {
    auto result = ConditionEvaluator::evaluate_group(strategy_.entry_conditions, strategy_.entry_logic, candles);

    if (!result.triggered)
        return;

    // Deployment risk limits (trades per day, entry window, regular session only):
    // they can only REFUSE an entry. Reported once per distinct reason.
    {
        QString why;
        if (!entry_gates_allow(&why)) {
            if (why != last_gate_note_) {
                last_gate_note_ = why;
                LOG_INFO("AlgoEngine", QString("Deployment %1: entry signal not taken — %2").arg(deployment_.id, why));
                emit_live_snapshot(current_mark(), QStringLiteral("Entry signal not taken — %1").arg(why));
            }
            return;
        }
        last_gate_note_.clear();
    }

    AlgoOrderSignal signal;
    signal.deployment_id = deployment_.id;
    signal.account_id = deployment_.broker_account_id;
    signal.exchange = deployment_.exchange;
    signal.product_type = deployment_.product_type;
    signal.order_type = "MARKET";
    signal.price = candles.last().close; // fill reference for paper sim / P&L
    signal.reason = "entry_signal";

    // ── F&O multi-leg branch ────────────────────────────────────────────────
    // Resolve expiry rule + chain snapshot + leg contracts, then emit a multi-
    // leg order basket. The equity single-symbol path below is left untouched.
    if (deployment_.instrument_type != QLatin1String("equity") && fno_bridge_) {
        // Expiry rule from the first leg rule (single-expiry v1), resolved against
        // today's expiry list (fno_chain_expiry() upper-cases the mode).
        const QString expiry = fno_chain_expiry();
        // Refuse the entry (never a partial basket): report the reason once per
        // distinct cause so a per-tick strategy does not flood the activity line.
        auto refuse_entry = [this](const QString& why) {
            if (why == last_entry_block_)
                return;
            last_entry_block_ = why;
            const QString msg = QStringLiteral("F&O entry NOT placed — %1").arg(why);
            LOG_WARN("AlgoEngine", QString("Deployment %1: %2").arg(deployment_.id, msg));
            emit_live_snapshot(current_mark(), msg);
            emit error_occurred(deployment_.id, msg);
        };
        if (expiry.isEmpty()) {
            refuse_entry(QStringLiteral("no expiry of %1 matches the strategy's expiry rule (expiry list not loaded?)")
                             .arg(deployment_.underlying));
            return;
        }

        fno_bridge_->ensure_chain(deployment_.broker_id, deployment_.underlying, expiry);
        const auto chain = fno_bridge_->snapshot(deployment_.broker_id, deployment_.underlying, expiry);
        if (chain.rows.isEmpty()) {
            refuse_entry(QStringLiteral("the %1 %2 option chain is not loaded yet").arg(deployment_.underlying, expiry));
            return;
        }
        QString leg_error;
        auto legs = fincept::algo::fno::resolve_entry_legs(chain, strategy_.legs, &leg_error);

        if (legs.isEmpty()) {
            refuse_entry(leg_error.isEmpty() ? QStringLiteral("no resolvable legs") : leg_error);
            return;
        }
        last_entry_block_.clear();
        resolved_expiry_ = expiry; // keep for live leg marks + restart reattach

        // Order-value ceiling. The equity path below checks this per order; the
        // basket has no single (qty × price), so sum the legs' notional and run the
        // same limit. Without this the highest-notional orders the engine places —
        // multi-leg NFO baskets — were the only ones with no ceiling at all. Checked
        // before pinning so a rejected basket leaves no leg subscriptions behind.
        double basket_notional = 0;
        for (const auto& l : legs)
            basket_notional += std::abs(l.price * l.quantity);
        if (!position_mgr_->validate_order_notional(basket_notional)) {
            refuse_entry(QStringLiteral("basket notional %1 (%2 legs) exceeds the max order value")
                             .arg(basket_notional, 0, 'f', 2)
                             .arg(legs.size()));
            return;
        }

        // Pin the resolved leg symbols into the live WS subscription window so
        // they stay fresh for mark-to-market during the life of the position.
        QStringList syms;
        for (const auto& l : legs)
            syms << l.symbol;
        fno_bridge_->pin_legs(deployment_.broker_id, deployment_.underlying, expiry, syms);

        signal.symbol = deployment_.underlying; // underlying for bookkeeping
        signal.legs = legs;
        emit_order_signal(signal);
        return; // skip the equity single-symbol path below
    }
    // ── Equity single-symbol path (unchanged) ───────────────────────────────

    signal.symbol = deployment_.symbol;
    signal.side = deployment_.entry_side;
    signal.quantity = deployment_.quantity;

    if (!position_mgr_->validate_order_value(signal.quantity, candles.last().close)) {
        // Re-evaluated on every candle while the entry signal holds: one line every 10 minutes.
        LOG_WARN_RL("AlgoEngine", QStringLiteral("order_value_limit:") + deployment_.id, 600000,
                    logf("Entry skipped, order value exceeds the deployment limit",
                         {{"deployment", deployment_.id},
                          {"symbol", signal.symbol},
                          {"qty", signal.quantity},
                          {"price", candles.last().close}}));
        return;
    }

    emit_order_signal(signal);
}

void DeploymentRunner::evaluate_exit(const QVector<OhlcvCandle>& candles) {
    auto result = ConditionEvaluator::evaluate_group(strategy_.exit_conditions, strategy_.exit_logic, candles);

    if (!result.triggered)
        return;

    AlgoOrderSignal signal;
    signal.deployment_id = deployment_.id;
    signal.account_id = deployment_.broker_account_id;
    signal.exchange = deployment_.exchange;
    signal.product_type = deployment_.product_type;
    signal.order_type = "MARKET";
    signal.price = candles.last().close;
    signal.reason = "exit_signal";

    // ── F&O multi-leg exit branch ───────────────────────────────────────────
    if (deployment_.instrument_type != QLatin1String("equity") && fno_bridge_ && position_mgr_->has_legs()) {
        signal.symbol = deployment_.underlying;
        signal.legs = fincept::algo::fno::build_exit_legs(position_mgr_->legs());
        emit_order_signal(signal);
        return; // skip the equity single-symbol exit path below
    }
    // ── Equity single-symbol exit path (unchanged) ──────────────────────────

    auto pos = position_mgr_->position();
    signal.symbol = deployment_.symbol;
    signal.side = (pos.side == PositionSide::Long) ? "SELL" : "BUY";
    signal.quantity = pos.quantity;

    emit_order_signal(signal);
}

void DeploymentRunner::emit_order_signal(const AlgoOrderSignal& signal_in) {
    // A stop was requested (possibly from another thread, before the queued stop()
    // ran): never place another order.
    if (stop_requested_.load()) {
        LOG_WARN("AlgoEngine",
                 QString("Deployment %1: %2 suppressed - stop requested").arg(deployment_.id, signal_in.reason));
        return;
    }
    // A live order's fill could not be confirmed: the broker-side position is
    // unknown, so NO further order (entry, exit, stop-loss) may be sent — an
    // "exit" for an entry that never filled would open a fresh opposite position.
    if (fill_unconfirmed_)
        return;
    // Only one order in flight at a time. Position/risk state updates only when a
    // fill arrives, so without this guard a stop-loss re-fires on every tick (and
    // entry/exit on every candle) until the fill lands — flooding duplicate orders.
    if (!pending_orders_.isEmpty())
        return;

    // Back off after a rejection. A rejected exit leaves the position open, so the very next
    // tick re-fires the same stop-loss / take-profit and the broker rejects it again — a
    // sustained rejection (market closed, margin, bad symbol) became one live order attempt
    // per tick for as long as it lasted.
    if (last_reject_ms_ > 0 && QDateTime::currentMSecsSinceEpoch() - last_reject_ms_ < kRunnerRejectBackoffMs)
        return;

    // Stamp the deployment's mode so the engine routes paper→simulated-fill and
    // live→broker. This is the single safety gate that stops a PAPER deployment
    // from ever hitting a real broker, even though it carries a real account id
    // (the account is its data source).
    AlgoOrderSignal signal = signal_in;
    signal.mode = deployment_.mode;
    signal.paper_portfolio_id = deployment_.paper_portfolio_id; // paper basket target
    if (signal.price <= 0)
        signal.price = position_mgr_->metrics().current_price;

    PendingOrder pending;
    pending.signal = signal;
    pending.submitted_ms = QDateTime::currentMSecsSinceEpoch();
    pending_orders_.append(pending);

    emit order_requested(signal);
    emit_live_snapshot(signal.price,
                       signal.legs.isEmpty()
                           ? QStringLiteral("%1: %2 %3 @ MARKET").arg(signal.reason, signal.side).arg(signal.quantity, 0,
                                                                                                         'f', 0)
                           : QStringLiteral("%1: %2-leg basket @ MARKET").arg(signal.reason).arg(signal.legs.size()));
    // The deployment's decision. The order audit trail is written where the order leaves
    // the terminal (UnifiedTrading for live, AlgoEngine::execute_order for simulated paper).
    LOG_INFO("AlgoEngine", logf("Algo order requested", {{"deployment", deployment_.id},
                                                         {"account", deployment_.broker_account_id},
                                                         {"mode", deployment_.mode},
                                                         {"symbol", signal.symbol},
                                                         {"side", signal.side},
                                                         {"type", "market"},
                                                         {"qty", signal.quantity},
                                                         {"legs", signal.legs.size()},
                                                         {"reason", signal.reason}}));
}

void DeploymentRunner::on_order_filled(const QString& broker_order_id, double fill_price, double fill_qty) {
    int idx = -1;
    for (int i = 0; i < pending_orders_.size(); ++i) {
        const PendingOrder& p = pending_orders_[i];
        // A LIVE fill is only ever confirmed against the broker order id it was
        // accepted under; the id-less match is for the paper simulator.
        const bool idless_paper = p.broker_order_id.isEmpty() && p.signal.mode != QLatin1String("live");
        if ((!broker_order_id.isEmpty() && p.broker_order_id == broker_order_id) || idless_paper) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (!broker_order_id.isEmpty())
            LOG_WARN("AlgoEngine", QString("Deployment %1: fill for order %2 matches no order in flight — ignored; "
                                           "verify the position at the broker")
                                       .arg(deployment_.id, broker_order_id));
        return;
    }

    auto pending = pending_orders_.takeAt(idx);
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    // Broker order books may omit the average price / filled quantity of a
    // completed order; fall back to the signal's reference values.
    if (fill_price <= 0)
        fill_price = pending.signal.price;
    if (fill_qty <= 0)
        fill_qty = pending.signal.quantity;

    bool is_entry = (pending.signal.reason == "entry_signal");
    double pnl = 0;
    double gross = 0;
    if (!is_entry)
        consecutive_exit_rejects_ = 0;

    // Commission + taxes per the deployment's cost model: what the paper simulator
    // charges, and for a live fill an ESTIMATE (the broker's order book reports no
    // fees) — deducting it keeps live P&L and the loss limits conservative.
    const double fees = estimate_fees(pending.signal.side, fill_qty, fill_price);

    if (is_entry) {
        PositionSide side = (pending.signal.side == "BUY") ? PositionSide::Long : PositionSide::Short;
        position_mgr_->record_entry(side, fill_qty, fill_price, now, fees);
    } else {
        pnl = position_mgr_->record_exit(fill_qty, fill_price, now, fees);
        gross = position_mgr_->last_exit_gross();
    }

    AlgoTradeRecord trade;
    trade.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    trade.deployment_id = deployment_.id;
    trade.symbol = feed_symbol();
    trade.side = pending.signal.side;
    trade.quantity = fill_qty;
    trade.price = fill_price;
    trade.pnl = pnl;
    trade.reason = pending.signal.reason;
    trade.timestamp = now;
    trade.broker_order_id = broker_order_id;
    trade.latency_ms = now - pending.submitted_ms;
    trade.fees = fees;
    trade.gross_pnl = gross;
    trade.ref_price = pending.signal.price;
    trade.mode = pending.signal.mode;

    persist_trade(trade);
    emit trade_executed(trade);
    emit metrics_updated(deployment_.id, position_mgr_->metrics());
    persist_equity_snapshot(true);
    QString fill_note = QStringLiteral("%1 FILLED %2 @ %3")
                            .arg(is_entry ? QStringLiteral("ENTRY") : QStringLiteral("EXIT"))
                            .arg(fill_qty, 0, 'f', 0)
                            .arg(fill_price, 0, 'f', 2);
    if (fees > 0)
        fill_note += QStringLiteral(" (fees %1%2)")
                         .arg(fees, 0, 'f', 2)
                         .arg(pending.signal.mode == QLatin1String("live") ? QStringLiteral(" est.") : QString());
    if (!is_entry && position_mgr_->is_paused()) {
        // The realizing exit tripped a loss limit — say so, otherwise the card just
        // stops trading with no explanation.
        const bool dd = position_mgr_->metrics().halt_reason == QLatin1String("max_drawdown_limit");
        LOG_WARN("AlgoEngine", QString("Deployment %1: %2 reached — new entries halted %3")
                                   .arg(deployment_.id,
                                        dd ? QStringLiteral("max drawdown limit") : QStringLiteral("daily loss limit"),
                                        dd ? QStringLiteral("until a manual resume")
                                           : QStringLiteral("until the next trading day")));
        fill_note += dd ? QStringLiteral(" — MAX DRAWDOWN LIMIT HIT, entries halted until resumed")
                        : QStringLiteral(" — DAILY LOSS LIMIT HIT, entries halted");
    }
    emit_live_snapshot(fill_price, fill_note);
    if (flatten_pending_ && !is_entry && !in_position()) {
        flatten_pending_ = false;
        emit flatten_finished(deployment_.id, true, QStringLiteral("position closed"));
    }
}

void DeploymentRunner::on_order_rejected(const QString& /*broker_order_id*/, const QString& reason) {
    bool was_exit = false;
    QString what = QStringLiteral("order");
    if (!pending_orders_.isEmpty()) {
        const PendingOrder p = pending_orders_.takeFirst();
        was_exit = (p.signal.reason != QLatin1String("entry_signal"));
        what = p.signal.reason;
    }
    last_reject_ms_ = QDateTime::currentMSecsSinceEpoch();
    report_rejection(was_exit, what, reason);
}

void DeploymentRunner::report_rejection(bool was_exit, const QString& what, const QString& reason) {
    LOG_ERROR("AlgoEngine", QString("Deployment %1: %2 rejected: %3").arg(deployment_.id, what, reason));
    if (was_exit)
        ++consecutive_exit_rejects_;
    // The card's activity line + the Dashboard error stream: a rejected order used
    // to leave "entry_signal: BUY 10 @ MARKET" as the last word on a RUNNING card.
    QString note = QStringLiteral("ORDER REJECTED (%1): %2").arg(what, reason);
    if (was_exit)
        note += QStringLiteral(" — position still OPEN, retrying");
    emit_live_snapshot(current_mark(), note);
    emit error_occurred(deployment_.id, QStringLiteral("Order rejected (%1): %2").arg(what, reason));
    // A protective exit that keeps failing must not stay a silent retry loop.
    static constexpr int kExitRejectAlertEvery = 3;
    if (was_exit && consecutive_exit_rejects_ > 0 && consecutive_exit_rejects_ % kExitRejectAlertEvery == 0)
        post_alert(QStringLiteral("Algo '%1': %2 exit orders in a row were rejected — the position is NOT closed. "
                                  "Last reason: %3")
                       .arg(strategy_.name)
                       .arg(consecutive_exit_rejects_)
                       .arg(reason));
}

void DeploymentRunner::post_alert(const QString& message) const {
    // ToastService is a GUI-thread object; this runner lives on the engine thread.
    if (auto* app = QCoreApplication::instance()) {
        QMetaObject::invokeMethod(
            app,
            [message]() {
                fincept::ui::ToastService::instance().post(fincept::ui::ToastService::Severity::Error, message,
                                                           QStringLiteral("algo"));
            },
            Qt::QueuedConnection);
    }
}

void DeploymentRunner::on_order_accepted(const QString& broker_order_id) {
    if (pending_orders_.isEmpty())
        return;
    PendingOrder& p = pending_orders_.first();
    p.broker_order_id = broker_order_id;
    p.awaiting_fill = true;
    const QString ref = broker_order_id.isEmpty() ? QStringLiteral("basket") : QStringLiteral("order ") + broker_order_id;
    LOG_INFO("AlgoEngine", QString("Deployment %1: %2 %3 accepted by the broker — confirming the fill")
                               .arg(deployment_.id, p.signal.reason, ref));
    emit_live_snapshot(current_mark(), QStringLiteral("%1: %2 accepted — confirming fill…").arg(p.signal.reason, ref));
}

void DeploymentRunner::on_order_unconfirmed(const QString& broker_order_id, const QString& detail) {
    fill_unconfirmed_ = true; // latch: emit_order_signal sends nothing from now on
    QString what = QStringLiteral("order");
    if (!pending_orders_.isEmpty()) {
        PendingOrder& p = pending_orders_.first();
        p.awaiting_fill = false;
        p.unconfirmed = true; // held forever — never purged by the heartbeat
        if (!broker_order_id.isEmpty())
            p.broker_order_id = broker_order_id;
        what = p.signal.reason;
    }
    // A partially-reported basket is not recorded either.
    basket_fills_.clear();
    basket_rejected_ = 0;
    const QString msg =
        QStringLiteral("FILL UNCONFIRMED (%1, order %2): %3. No position was recorded and this deployment will place "
                       "NO further orders. Check the order and position at the broker, then stop this deployment.")
            .arg(what, broker_order_id.isEmpty() ? QStringLiteral("?") : broker_order_id, detail);
    LOG_ERROR("AlgoEngine", QString("Deployment %1: %2").arg(deployment_.id, msg));
    emit_live_snapshot(current_mark(), msg);
    emit error_occurred(deployment_.id, msg);
    post_alert(QStringLiteral("Algo '%1': %2").arg(strategy_.name, msg));
    if (flatten_pending_) {
        flatten_pending_ = false;
        emit flatten_finished(deployment_.id, false, msg);
    }
}

void DeploymentRunner::on_order_queued_for_approval(const QString& approval_id) {
    if (pending_orders_.isEmpty() || approval_id.isEmpty())
        return;
    pending_orders_.first().awaiting_approval = true;
    pending_orders_.first().approval_id = approval_id;
    LOG_WARN("AlgoEngine", QString("Deployment %1: account is in Semi-Auto — order queued for approval (%2), NOT sent")
                               .arg(deployment_.id, approval_id));
    emit_live_snapshot(pending_orders_.first().signal.price, QStringLiteral("Awaiting approval in Action Center"));
}

void DeploymentRunner::on_approval_resolved(const QString& approval_id, bool approved, const QString& broker_order_id,
                                            const QString& detail) {
    if (approval_id.isEmpty())
        return; // an empty id would match a normal (non-queued) in-flight order
    int idx = -1;
    for (int i = 0; i < pending_orders_.size(); ++i) {
        if (pending_orders_[i].approval_id == approval_id) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // Already purged by the heartbeat (ActionCenter has expired it too). Do
        // NOT fall through to on_order_filled/on_order_rejected — they resolve the
        // FIRST pending order, which by now may be an unrelated later signal.
        LOG_WARN("AlgoEngine", QString("Deployment %1: approval %2 resolved but that order is no longer in flight "
                                       "— ignored")
                                   .arg(deployment_.id, approval_id));
        return;
    }

    pending_orders_[idx].awaiting_approval = false;
    pending_orders_[idx].approval_id.clear();

    if (approved) {
        // Approval = the broker ACCEPTED the order, not that it filled. Keep it in
        // flight; AlgoEngine confirms the fill from the order book and then calls
        // on_order_filled / on_order_rejected / on_order_unconfirmed.
        pending_orders_[idx].broker_order_id = broker_order_id;
        pending_orders_[idx].awaiting_fill = true;
        // The fill-confirmation window starts when the order was actually sent, not
        // when the signal first waited for approval.
        pending_orders_[idx].submitted_ms = QDateTime::currentMSecsSinceEpoch();
        emit_live_snapshot(current_mark(),
                           QStringLiteral("Approved — order %1 sent, confirming fill…").arg(broker_order_id));
    } else {
        on_order_rejected(broker_order_id, detail.isEmpty() ? QStringLiteral("rejected in Action Center") : detail);
    }
}

// ── Multi-leg F&O basket fills (P3.4) ───────────────────────────────────────
// One call per leg from AlgoEngine::execute_basket. Fills accumulate in
// basket_fills_ / basket_rejected_ until the whole in-flight basket is
// accounted for, then finalize_basket_if_complete() records or abandons it.

void DeploymentRunner::on_leg_filled(int leg_index, const AlgoOrderLeg& leg, double fill_price, double fill_qty) {
    if (pending_orders_.isEmpty())
        return;
    const PendingOrder& pending = pending_orders_.first();
    const bool is_entry = (pending.signal.reason == "entry_signal");
    const int64_t now = QDateTime::currentMSecsSinceEpoch();

    if (is_entry) {
        // Record the fill as an open leg position; P&L marks come from leg quotes.
        AlgoLegPosition lp;
        lp.symbol = leg.symbol;
        lp.instrument_token = leg.instrument_token;
        lp.is_call = leg.is_call; // a PUT leg must be marked from the chain's PE side
        lp.strike = leg.strike;
        lp.side_sign = (leg.side == QLatin1String("BUY")) ? 1 : -1;
        lp.quantity = fill_qty;
        lp.entry_price = fill_price;
        lp.current_price = fill_price;
        lp.unrealized_pnl = 0;
        basket_fills_.append(lp);
    } else {
        // Exit fill: mark the matching open leg at the exit price so the realized
        // basket P&L (computed in record_exit_legs from the open legs' marks)
        // reflects the exit fills. Count it for completion via basket_fills_.
        position_mgr_->update_leg_price(leg.symbol, fill_price);
        AlgoLegPosition lp;
        lp.symbol = leg.symbol;
        lp.quantity = fill_qty;
        basket_fills_.append(lp);
    }

    // Persist one trade row per leg (symbol = underlying for grouping; leg_symbol
    // = the concrete contract; pnl is attached to the basket at finalize).
    AlgoTradeRecord trade;
    trade.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    trade.deployment_id = deployment_.id;
    trade.symbol = deployment_.underlying.isEmpty() ? deployment_.symbol : deployment_.underlying;
    trade.side = leg.side;
    trade.quantity = fill_qty;
    trade.price = fill_price;
    trade.pnl = 0;
    trade.reason = pending.signal.reason;
    trade.timestamp = now;
    trade.broker_order_id = QString();
    trade.latency_ms = now - pending.submitted_ms;
    trade.leg_symbol = leg.symbol;
    trade.leg_index = leg_index;
    persist_trade(trade);
    emit trade_executed(trade);

    finalize_basket_if_complete();
}

void DeploymentRunner::on_leg_rejected(int leg_index, const QString& reason) {
    basket_rejected_++;
    LOG_ERROR("AlgoEngine",
              QString("Deployment %1: basket leg %2 rejected: %3").arg(deployment_.id).arg(leg_index).arg(reason));
    finalize_basket_if_complete();
}

void DeploymentRunner::finalize_basket_if_complete() {
    if (pending_orders_.isEmpty())
        return;
    const PendingOrder& pending = pending_orders_.first();
    const int expected = pending.signal.legs.size();
    if (expected <= 0)
        return;
    if (basket_fills_.size() + basket_rejected_ < expected)
        return; // still waiting on legs

    const bool is_entry = (pending.signal.reason == "entry_signal");
    const QString pending_reason = pending.signal.reason; // `pending` dangles after removeFirst()
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    pending_orders_.removeFirst(); // release the in-flight guard

    double pnl = 0;
    if (is_entry) {
        if (basket_rejected_ == 0 && !basket_fills_.isEmpty()) {
            position_mgr_->record_entry_legs(basket_fills_, now);
            persist_resolved_legs(); // so a restart reattaches to the open basket
            emit_live_snapshot(0, QStringLiteral("ENTRY basket filled (%1 legs)").arg(basket_fills_.size()));
        } else {
            // Partial entry: the engine has rolled back the filled legs at the
            // broker; record no position so we stay flat.
            LOG_WARN("AlgoEngine",
                     QString("Deployment %1: entry basket aborted (%2 filled, %3 rejected) — no position recorded")
                         .arg(deployment_.id)
                         .arg(basket_fills_.size())
                         .arg(basket_rejected_));
            emit_live_snapshot(0, QStringLiteral("ENTRY basket aborted — rolled back"));
        }
    } else if (basket_rejected_ > 0) {
        // Exit basket rejected: the legs are still open at the broker (a partial
        // exit never reaches here — the engine latches it as unconfirmed). Keep
        // the basket, back off and let the exit/stop-loss retry.
        last_reject_ms_ = now;
        const int rejected = basket_rejected_;
        basket_fills_.clear();
        basket_rejected_ = 0;
        report_rejection(true, pending_reason.isEmpty() ? QStringLiteral("exit") : pending_reason,
                         QStringLiteral("%1 of %2 exit legs rejected").arg(rejected).arg(expected));
        emit metrics_updated(deployment_.id, position_mgr_->metrics());
        return;
    } else {
        // Exit: open legs were marked at their exit fills in on_leg_filled.
        consecutive_exit_rejects_ = 0;
        pnl = position_mgr_->record_exit_legs(now);
        clear_resolved_legs(); // basket closed — nothing to reattach on restart
        resolved_expiry_.clear();
        QString exit_note = QStringLiteral("EXIT basket closed, P&L %1").arg(pnl, 0, 'f', 2);
        if (position_mgr_->is_paused()) {
            LOG_WARN("AlgoEngine", QString("Deployment %1: daily loss limit reached — new entries halted until the next "
                                           "trading day")
                                       .arg(deployment_.id));
            exit_note += QStringLiteral(" — DAILY LOSS LIMIT HIT, entries halted");
        }
        emit_live_snapshot(0, exit_note);
    }

    basket_fills_.clear();
    basket_rejected_ = 0;
    emit metrics_updated(deployment_.id, position_mgr_->metrics());
    persist_equity_snapshot(true);
    if (flatten_pending_ && !is_entry && !in_position()) {
        flatten_pending_ = false;
        emit flatten_finished(deployment_.id, true, QStringLiteral("basket closed"));
    }
}

void DeploymentRunner::on_heartbeat() {
    if (!running_.load())
        return;
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    // Day rollover: the daily loss limit latches the runner off (is_paused()
    // gates every entry/exit evaluation). Without this check the latch never
    // clears, so a "daily" limit halts the deployment for the lifetime of the
    // process. The heartbeat is the only tick that keeps running while the
    // runner is latched, so the reset has to live here.
    // The day is the EXCHANGE's trading date (MarketSessions), not the machine's
    // local date: an NSE deployment run from New York rolled over mid-session.
    if (position_mgr_->reset_daily_if_new_day(today_key())) {
        LOG_INFO("AlgoEngine", QString("Deployment %1: new trading day — daily P&L and the daily-loss halt were reset")
                                   .arg(deployment_.id));
        emit_live_snapshot(position_mgr_->metrics().current_price,
                           QStringLiteral("New trading day — daily loss limit reset"));
    }

    // Keep the DB metrics row fresh (~5s) so the summary cards and any structural
    // rebuild show current values instead of stale zeros.
    persist_metrics();
    persist_equity_snapshot(false);

    // Flatten in progress: re-send the exit when nothing is in flight (a rejected
    // exit backs off, then retries here), and give up after a bounded time.
    if (flatten_pending_) {
        if (!in_position()) {
            flatten_pending_ = false;
            emit flatten_finished(deployment_.id, true, QStringLiteral("no position"));
        } else if (now - flatten_started_ms_ > kRunnerFlattenLimitMs) {
            flatten_pending_ = false;
            emit flatten_finished(deployment_.id, false,
                                  QStringLiteral("the exit did not complete within %1 s").arg(kRunnerFlattenLimitMs / 1000));
        } else if (pending_orders_.isEmpty() && !fill_unconfirmed_) {
            AlgoOrderSignal sig;
            if (build_exit_signal(QStringLiteral("flatten_on_stop"), current_mark(), sig))
                emit_order_signal(sig);
        }
    }

    // Purge orders that never reported a fill/rejection so a lost ack doesn't
    // permanently block new signals (the in-flight guard in emit_order_signal).
    // An order parked in the Semi-Auto approval queue is NOT a lost ack — it is
    // waiting on a human — so it keeps the guard until ActionCenter's approval
    // window (ActionCenter::pending_ttl_minutes(): 5 min by default, user-
    // configurable) has certainly expired it, plus a minute of slack. Purging it
    // earlier would let the runner re-signal and stack duplicate approval requests.
    // A LIVE order that was (or may have been) sent and never reported back is NOT
    // purged either: it may still be working at the broker, and freeing the guard
    // would let the runner send another order on top of an unknown position. It
    // latches the runner instead (on_order_unconfirmed). Confirmation normally
    // resolves well inside these limits.
    const int64_t approval_limit_ms =
        (static_cast<int64_t>(fincept::trading::ActionCenter::pending_ttl_minutes()) + 1) * 60000;
    static constexpr int64_t kAwaitFillLimitMs = 5 * 60000;
    for (int i = pending_orders_.size() - 1; i >= 0; --i) {
        if (pending_orders_[i].unconfirmed)
            continue; // held until the user stops the deployment
        const int64_t age = now - pending_orders_[i].submitted_ms;
        // A paper order waits for its next quote (market closed / quiet feed) longer
        // than a lost acknowledgement would.
        const int64_t limit = pending_orders_[i].awaiting_approval ? approval_limit_ms
                              : pending_orders_[i].awaiting_fill   ? kAwaitFillLimitMs
                              : pending_orders_[i].paper_working   ? kRunnerPaperWaitMs
                                                                   : 60000;
        if (age > limit && !pending_orders_[i].awaiting_approval &&
            pending_orders_[i].signal.mode == QLatin1String("live")) {
            if (i == 0)
                on_order_unconfirmed(pending_orders_[i].broker_order_id,
                                     QStringLiteral("no fill or rejection was reported within %1 s").arg(limit / 1000));
            continue;
        }
        if (age > limit) {
            LOG_WARN("AlgoEngine", QString("Deployment %1: pending order timed out (no %2 in %3s), clearing")
                                       .arg(deployment_.id,
                                            pending_orders_[i].awaiting_approval ? QStringLiteral("approval")
                                                                                 : QStringLiteral("fill/reject"))
                                       .arg(limit / 1000));
            pending_orders_.removeAt(i);
        }
    }

    // A paused runner drops every quote by design, so the stale-data watchdog must
    // not treat that silence as a broker outage (it would error the deployment out
    // 30s into any pause, and AlgoEngine tears the runner down on 'error').
    if (!paused_.load() && !stop_requested_.load() && now - last_heartbeat_ms_ > 30000) {
        const QString msg = deployment_.broker_id.isEmpty()
                                ? QStringLiteral("No market data in 30s — no broker is attached to this deployment.")
                                : QString("No market data from broker '%1' in 30s — check the broker is connected "
                                          "and the market is open.")
                                      .arg(deployment_.broker_id);
        LOG_ERROR("AlgoEngine", QString("Deployment %1: %2").arg(deployment_.id, msg));

        auto db = Database::instance().connection();
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE algo_deployments SET status='error', error_message=?, "
                                 "updated_at=datetime('now') WHERE id=?"));
        q.addBindValue(msg);
        q.addBindValue(deployment_.id);
        // If this fails the row keeps its running status and is resumed on restart.
        if (!q.exec())
            LOG_ERROR("AlgoEngine", logf("Deployment error status could not be saved",
                                         {{"deployment", deployment_.id}, {"error", q.lastError().text()}}));

        emit status_changed(deployment_.id, QStringLiteral("error"));
        emit error_occurred(deployment_.id, msg);
    }
}

void DeploymentRunner::persist_trade(const AlgoTradeRecord& trade) {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO algo_trades (id, deployment_id, symbol, side, quantity, price, pnl, "
                             "reason, broker_order_id, latency_ms, leg_symbol, leg_index, fees, gross_pnl, ref_price, "
                             "mode, ts_ms, created_at) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))"));
    q.addBindValue(trade.id);
    q.addBindValue(trade.deployment_id);
    q.addBindValue(trade.symbol);
    q.addBindValue(trade.side);
    q.addBindValue(trade.quantity);
    q.addBindValue(trade.price);
    q.addBindValue(trade.pnl);
    q.addBindValue(trade.reason);
    q.addBindValue(trade.broker_order_id);
    q.addBindValue(QVariant::fromValue(trade.latency_ms));
    q.addBindValue(trade.leg_symbol);
    q.addBindValue(trade.leg_index);
    q.addBindValue(trade.fees);
    q.addBindValue(trade.gross_pnl);
    q.addBindValue(trade.ref_price);
    q.addBindValue(trade.mode);
    q.addBindValue(QVariant::fromValue(trade.timestamp));
    if (q.exec())
        return;
    // A database that has not run v057 has no reason / latency / cost columns (the
    // original v023 table names the reason column signal_reason): still record the
    // fill with what that schema has rather than losing the row.
    const QString first_err = q.lastError().text();
    QSqlQuery legacy(db);
    legacy.prepare(QStringLiteral("INSERT INTO algo_trades (id, deployment_id, symbol, side, quantity, price, pnl, "
                                  "signal_reason, broker_order_id, leg_symbol, leg_index, created_at) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))"));
    legacy.addBindValue(trade.id);
    legacy.addBindValue(trade.deployment_id);
    legacy.addBindValue(trade.symbol);
    legacy.addBindValue(trade.side);
    legacy.addBindValue(trade.quantity);
    legacy.addBindValue(trade.price);
    legacy.addBindValue(trade.pnl);
    legacy.addBindValue(trade.reason);
    legacy.addBindValue(trade.broker_order_id);
    legacy.addBindValue(trade.leg_symbol);
    legacy.addBindValue(trade.leg_index);
    if (!legacy.exec())
        LOG_ERROR("AlgoEngine", QString("Failed to persist trade: %1 / %2").arg(first_err, legacy.lastError().text()));
}

void DeploymentRunner::persist_metrics() {
    auto m = position_mgr_->metrics();
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO algo_metrics (deployment_id, total_pnl, unrealized_pnl, "
                             "total_trades, win_rate, max_drawdown, current_position_qty, current_position_side, "
                             "current_position_entry, current_price, updated_at) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))"));
    q.addBindValue(deployment_.id);
    q.addBindValue(m.total_pnl);
    q.addBindValue(m.unrealized_pnl);
    q.addBindValue(m.total_trades);
    q.addBindValue(m.win_rate);
    q.addBindValue(m.max_drawdown);
    q.addBindValue(m.current_position_qty);
    q.addBindValue(m.current_position_side);
    q.addBindValue(m.current_position_entry);
    q.addBindValue(m.current_price);
    if (!q.exec()) {
        LOG_ERROR("AlgoEngine", QString("Failed to persist metrics: %1").arg(q.lastError().text()));
        return;
    }
    // Risk state (v057 columns) in its own statement so a database without them
    // still persists the core metrics above. It is what lets the daily-loss limit
    // survive a restart within the same exchange trading day.
    const RiskState rs = position_mgr_->risk_state();
    QSqlQuery r(db);
    r.prepare(QStringLiteral("UPDATE algo_metrics SET daily_pnl = ?, daily_date = ?, peak_equity = ?, risk_halted = ?, "
                             "halt_reason = ?, total_fees = ?, trades_today = ? WHERE deployment_id = ?"));
    r.addBindValue(rs.daily_pnl);
    r.addBindValue(rs.day_key);
    r.addBindValue(rs.peak_equity);
    r.addBindValue((rs.paused_by_loss_limit ? 1 : 0) | (rs.halted_by_drawdown ? 2 : 0));
    r.addBindValue(rs.halt_reason);
    r.addBindValue(m.total_fees);
    r.addBindValue(rs.trades_today);
    r.addBindValue(deployment_.id);
    // Best effort (schema v057), but it carries the daily-loss limit across a restart.
    // Fails on every save on an old schema: one line per deployment every 10 minutes.
    if (!r.exec())
        LOG_WARN_RL("AlgoEngine", QStringLiteral("risk_state_save:") + deployment_.id, 600000,
                    logf("Risk state could not be saved",
                         {{"deployment", deployment_.id}, {"error", r.lastError().text()}}));
}

// ── Monitoring helpers ───────────────────────────────────────────────────────

int DeploymentRunner::today_key() const {
    return trading_day_key(venue_, QDateTime::currentMSecsSinceEpoch());
}

double DeploymentRunner::estimate_fees(const QString& side, double qty, double price) const {
    return execution::fees(cost_model_, side == QLatin1String("SELL") ? FillSide::Sell : FillSide::Buy, qty, price)
        .total();
}

double DeploymentRunner::recent_atr() const {
    const auto c = aggregator_->closed_candles();
    constexpr int kPeriod = 14;
    if (c.size() <= kPeriod)
        return std::numeric_limits<double>::quiet_NaN();
    double sum = 0;
    for (int i = c.size() - kPeriod; i < c.size(); ++i) {
        const double tr = std::max({c[i].high - c[i].low, std::abs(c[i].high - c[i - 1].close),
                                    std::abs(c[i].low - c[i - 1].close)});
        sum += tr;
    }
    return sum / kPeriod;
}

void DeploymentRunner::persist_equity_snapshot(bool force) {
    const auto m = position_mgr_->metrics();
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    const double equity = m.total_pnl + m.unrealized_pnl;
    if (!force && last_equity_snap_ms_ > 0 &&
        (now - last_equity_snap_ms_ < kRunnerEquitySnapMs ||
         (std::abs(equity - last_equity_snap_value_) < 1e-9 && now - last_equity_snap_ms_ < 10 * kRunnerEquitySnapMs)))
        return;
    last_equity_snap_ms_ = now;
    last_equity_snap_value_ = equity;
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO algo_equity_snapshots (deployment_id, ts_ms, equity, realized, "
                             "unrealized, fees, price, position_qty) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(deployment_.id);
    q.addBindValue(QVariant::fromValue(static_cast<qint64>(now)));
    q.addBindValue(equity);
    q.addBindValue(m.total_pnl);
    q.addBindValue(m.unrealized_pnl);
    q.addBindValue(m.total_fees);
    q.addBindValue(m.current_price);
    q.addBindValue(m.current_position_qty);
    if (!q.exec()) // best effort (schema v057)
        LOG_DEBUG_RL("AlgoEngine", QStringLiteral("equity_snapshot_save:") + deployment_.id, 60000,
                     logf("Equity snapshot could not be saved",
                          {{"deployment", deployment_.id}, {"error", q.lastError().text()}}));
}

void DeploymentRunner::log_event(const QString& kind, const QString& message) {
    if (message.isEmpty())
        return;
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO algo_events (deployment_id, ts_ms, kind, message) VALUES (?, ?, ?, ?)"));
    q.addBindValue(deployment_.id);
    q.addBindValue(QVariant::fromValue(static_cast<qint64>(QDateTime::currentMSecsSinceEpoch())));
    q.addBindValue(kind);
    q.addBindValue(message.left(500));
    if (!q.exec()) // best effort (schema v057)
        LOG_DEBUG_RL("AlgoEngine", QStringLiteral("event_save:") + deployment_.id, 60000,
                     logf("Deployment event could not be saved",
                          {{"deployment", deployment_.id}, {"kind", kind}, {"error", q.lastError().text()}}));
}

bool DeploymentRunner::entry_gates_allow(QString* why) const {
    if (flatten_pending_) {
        if (why)
            *why = QStringLiteral("closing the position before stopping");
        return false;
    }
    if (limits_.max_trades_per_day > 0 && position_mgr_->metrics().trades_today >= limits_.max_trades_per_day) {
        if (why)
            *why = QStringLiteral("max %1 trade(s) per day reached").arg(limits_.max_trades_per_day);
        return false;
    }
    if (venue_ && (limits_.regular_session_only || limits_.has_window())) {
        const QDateTime now_utc = QDateTime::currentDateTimeUtc();
        if (limits_.regular_session_only &&
            fincept::core::market::session_status(*venue_, now_utc).state != fincept::core::market::SessionState::Open) {
            if (why)
                *why = QStringLiteral("outside %1's regular session").arg(venue_->id);
            return false;
        }
        if (limits_.has_window()) {
            const QTime local = now_utc.toTimeZone(fincept::core::market::venue_timezone(*venue_)).time();
            if (!limits_.in_window(local)) {
                if (why)
                    *why = QStringLiteral("outside the entry window %1-%2 (%3 time)")
                               .arg(limits_.window_start, limits_.window_end, venue_->id);
                return false;
            }
        }
    }
    return true;
}

// ── Paper fills on the next quote ───────────────────────────────────────────

void DeploymentRunner::on_paper_order_accepted(const QString& sim_order_id) {
    // The only in-flight order (see the guard in emit_order_signal) is this one.
    for (auto& p : pending_orders_) {
        if (p.signal.mode == QLatin1String("live") || !p.broker_order_id.isEmpty())
            continue;
        p.broker_order_id = sim_order_id;
        p.paper_working = true;
        return;
    }
}

void DeploymentRunner::fill_waiting_paper_order(double price, int64_t quote_ms) {
    if (pending_orders_.isEmpty())
        return;
    const PendingOrder& p = pending_orders_.first();
    if (!p.paper_working || p.signal.mode == QLatin1String("live") || !p.signal.legs.isEmpty())
        return;
    // Only a quote that arrives AFTER the order was placed can fill it.
    if (quote_ms <= p.submitted_ms && QDateTime::currentMSecsSinceEpoch() <= p.submitted_ms)
        return;
    const FillSide side = p.signal.side == QLatin1String("SELL") ? FillSide::Sell : FillSide::Buy;
    const double fill_px = execution::slipped_price(cost_model_, side, price, recent_atr());
    const QString id = p.broker_order_id;
    const double qty = p.signal.quantity;
    // Simulated fill (never through PaperTrading): the order audit trail is written here
    // (the submit: AlgoEngine::execute_order).
    LOG_AUDIT("Orders", "Order filled",
              {{"venue", "paper"},
               {"account", deployment_.broker_account_id},
               {"mode", "paper"},
               {"symbol", p.signal.symbol},
               {"side", p.signal.side},
               {"qty", qty},
               {"price", fill_px},
               {"client_id", id},
               // Same provenance as the submit line: algo:<id> entry, algo_exit:<id> exit.
               {"source", (p.signal.reason == QLatin1String("entry_signal") ? QStringLiteral("algo:")
                                                                             : QStringLiteral("algo_exit:")) +
                              deployment_.id}});
    on_order_filled(id, fill_px, qty);
}

// ── Flatten (stop with a position open) ─────────────────────────────────────

bool DeploymentRunner::build_exit_signal(const QString& reason, double ref_price, AlgoOrderSignal& signal) {
    if (!in_position())
        return false;
    signal = AlgoOrderSignal{};
    signal.deployment_id = deployment_.id;
    signal.account_id = deployment_.broker_account_id;
    signal.exchange = deployment_.exchange;
    signal.product_type = deployment_.product_type;
    signal.order_type = "MARKET";
    signal.price = ref_price;
    signal.reason = reason;
    // Same construction as evaluate_exit(): F&O closes every open leg, equity
    // closes the position with the opposite side.
    if (deployment_.instrument_type != QLatin1String("equity") && fno_bridge_ && position_mgr_->has_legs()) {
        signal.symbol = deployment_.underlying;
        signal.legs = fincept::algo::fno::build_exit_legs(position_mgr_->legs());
        return !signal.legs.isEmpty();
    }
    const auto pos = position_mgr_->position();
    if (pos.side == PositionSide::None || pos.quantity <= 0)
        return false;
    signal.symbol = deployment_.symbol;
    signal.side = (pos.side == PositionSide::Long) ? "SELL" : "BUY";
    signal.quantity = pos.quantity;
    return true;
}

void DeploymentRunner::request_flatten() {
    if (!running_.load() || stop_requested_.load()) {
        emit flatten_finished(deployment_.id, !in_position(), QStringLiteral("not running"));
        return;
    }
    if (!in_position()) {
        emit flatten_finished(deployment_.id, true, QStringLiteral("no position"));
        return;
    }
    flatten_pending_ = true;
    flatten_started_ms_ = QDateTime::currentMSecsSinceEpoch();
    log_event(QStringLiteral("order"), QStringLiteral("Stop requested — closing the open position first"));
    // Through the normal order path: in-flight guard, reject back-off, paper
    // simulation or the live broker with its approval queue and fill
    // confirmation. If another order is in flight the heartbeat sends it later.
    AlgoOrderSignal sig;
    if (pending_orders_.isEmpty() && build_exit_signal(QStringLiteral("flatten_on_stop"), current_mark(), sig))
        emit_order_signal(sig);
}

void DeploymentRunner::persist_resolved_legs() {
    const QJsonArray arr = fincept::algo::fno::leg_positions_to_json(position_mgr_->legs());
    const QString json = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE algo_deployments SET resolved_legs_json = ?, resolved_expiry = ?, "
                             "underlying = ?, instrument_type = ?, updated_at = datetime('now') WHERE id = ?"));
    q.addBindValue(json);
    q.addBindValue(resolved_expiry_);
    q.addBindValue(deployment_.underlying);
    q.addBindValue(deployment_.instrument_type);
    q.addBindValue(deployment_.id);
    if (!q.exec())
        LOG_ERROR("AlgoEngine", QString("Failed to persist resolved legs: %1").arg(q.lastError().text()));
}

void DeploymentRunner::clear_resolved_legs() {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE algo_deployments SET resolved_legs_json = '[]', resolved_expiry = '', "
                             "updated_at = datetime('now') WHERE id = ?"));
    q.addBindValue(deployment_.id);
    // A stale basket would be re-attached after a restart (restore_state_from_db).
    if (!q.exec())
        LOG_ERROR("AlgoEngine", logf("Resolved legs could not be cleared",
                                     {{"deployment", deployment_.id}, {"error", q.lastError().text()}}));
}

void DeploymentRunner::restore_state_from_db() {
    auto db = Database::instance().connection();

    // ── F&O: reattach an open multi-leg basket persisted before the restart ──────
    // Done first (and independently of algo_metrics) so a basket-open deployment
    // resumes its legs even if the metrics row is missing. Marks refresh on the
    // next tick; metrics (total_pnl etc.) are restored from algo_metrics below.
    if (deployment_.instrument_type != QLatin1String("equity")) {
        QSqlQuery lq(db);
        lq.prepare(QStringLiteral("SELECT resolved_legs_json, resolved_expiry FROM algo_deployments WHERE id = ?"));
        lq.addBindValue(deployment_.id);
        const bool legs_read = lq.exec();
        if (!legs_read)
            LOG_ERROR("AlgoEngine", logf("Open F&O basket could not be read on restore",
                                         {{"deployment", deployment_.id}, {"error", lq.lastError().text()}}));
        if (legs_read && lq.next()) {
            const QString legs_json = lq.value("resolved_legs_json").toString();
            QJsonParseError legs_err{};
            const auto doc = QJsonDocument::fromJson(legs_json.toUtf8(), &legs_err);
            if (!doc.isArray() && !legs_json.trimmed().isEmpty())
                LOG_ERROR("AlgoEngine", logf("Stored F&O legs are not valid JSON, open basket not re-attached",
                                             {{"deployment", deployment_.id},
                                              {"bytes", legs_json.size()},
                                              {"error", legs_err.error != QJsonParseError::NoError
                                                            ? legs_err.errorString()
                                                            : QStringLiteral("not an array")}}));
            if (doc.isArray()) {
                const auto restored = fincept::algo::fno::leg_positions_from_json(doc.array());
                if (!restored.isEmpty()) {
                    resolved_expiry_ = lq.value("resolved_expiry").toString();
                    position_mgr_->record_entry_legs(restored, QDateTime::currentMSecsSinceEpoch());
                    LOG_INFO("AlgoEngine",
                             QString("Deployment %1: reattached open F&O basket (%2 legs, expiry %3) across restart")
                                 .arg(deployment_.id)
                                 .arg(restored.size())
                                 .arg(resolved_expiry_));
                }
            }
        }
    }

    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("SELECT total_pnl, total_trades, win_rate, max_drawdown, current_position_qty, "
                       "current_position_side, current_position_entry FROM algo_metrics WHERE deployment_id = ?"));
    q.addBindValue(deployment_.id);
    if (!q.exec()) {
        LOG_ERROR("AlgoEngine", logf("Deployment state could not be read on restore",
                                     {{"deployment", deployment_.id}, {"error", q.lastError().text()}}));
        return;
    }
    if (!q.next())
        return; // fresh deploy — no prior state

    const QString side_s = q.value("current_position_side").toString();
    const PositionSide side = (side_s == "LONG")    ? PositionSide::Long
                              : (side_s == "SHORT") ? PositionSide::Short
                                                    : PositionSide::None;
    const double qty = q.value("current_position_qty").toDouble();
    const double entry = q.value("current_position_entry").toDouble();

    position_mgr_->restore_state(side, qty, entry, q.value("total_pnl").toDouble(), q.value("total_trades").toInt(),
                                 q.value("win_rate").toDouble(), q.value("max_drawdown").toDouble());

    // Daily-loss / drawdown state (v057): it used to reset on every restart, so a
    // restart granted a fresh daily loss budget mid-day. Taken only within the
    // same exchange trading day (the drawdown halt regardless of the day).
    {
        QSqlQuery rq(db);
        rq.prepare(QStringLiteral("SELECT daily_pnl, daily_date, peak_equity, risk_halted, halt_reason, total_fees, "
                                  "trades_today FROM algo_metrics WHERE deployment_id = ?"));
        rq.addBindValue(deployment_.id);
        const bool risk_read = rq.exec();
        if (!risk_read)
            LOG_WARN("AlgoEngine", logf("Risk state could not be read on restore",
                                        {{"deployment", deployment_.id}, {"error", rq.lastError().text()}}));
        if (risk_read && rq.next()) {
            RiskState rs;
            rs.daily_pnl = rq.value(0).toDouble();
            rs.day_key = rq.value(1).toInt();
            rs.peak_equity = rq.value(2).toDouble();
            const int halted = rq.value(3).toInt();
            rs.paused_by_loss_limit = (halted & 1) != 0;
            rs.halted_by_drawdown = (halted & 2) != 0;
            rs.halt_reason = rq.value(4).toString();
            rs.trades_today = rq.value(6).toInt();
            position_mgr_->restore_risk_state(rs, today_key(), rq.value(5).toDouble());
            if (position_mgr_->is_paused())
                LOG_WARN("AlgoEngine", QString("Deployment %1: restored with entries halted (%2)")
                                           .arg(deployment_.id, rs.halt_reason));
        }
    }

    if (side != PositionSide::None)
        LOG_INFO("AlgoEngine", QString("Deployment %1: restored open %2 %3 @ %4 across restart")
                                   .arg(deployment_.id, side_s)
                                   .arg(qty, 0, 'f', 0)
                                   .arg(entry, 0, 'f', 2));
}

void DeploymentRunner::update_deployment_status(const QString& status) {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE algo_deployments SET status = ?, updated_at = datetime('now') WHERE id = ?"));
    q.addBindValue(status);
    q.addBindValue(deployment_.id);
    if (!q.exec())
        LOG_ERROR("AlgoEngine", logf("Deployment status could not be saved", {{"deployment", deployment_.id},
                                                                             {"status", status},
                                                                             {"error", q.lastError().text()}}));
}

AlgoMetrics DeploymentRunner::metrics() const {
    return position_mgr_->metrics();
}

AlgoPosition DeploymentRunner::position() const {
    return position_mgr_->position();
}

} // namespace fincept::algo
