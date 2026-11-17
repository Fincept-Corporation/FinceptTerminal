// src/algo_engine/PositionManager.cpp
#include "algo_engine/PositionManager.h"

#include "core/logging/Logger.h"

#include <QDate>
#include <QDateTime>
#include <QMutexLocker>
#include <QUuid>

#include <cmath>

namespace fincept::algo {

PositionManager::PositionManager(const QString& deployment_id, double stop_loss_pct, double take_profit_pct,
                                 double trailing_stop_pct, double max_order_value, double max_daily_loss)
    : deployment_id_(deployment_id),
      stop_loss_pct_(stop_loss_pct),
      take_profit_pct_(take_profit_pct),
      trailing_stop_pct_(trailing_stop_pct),
      max_order_value_(max_order_value),
      max_daily_loss_(max_daily_loss) {
    risk_.day_start_epoch = QDateTime::currentMSecsSinceEpoch();
}

std::optional<AlgoOrderSignal> PositionManager::check_risk(double current_price) {
    QMutexLocker lock(&mutex_);

    // ── Multi-leg basket branch (P3) ────────────────────────────────────────
    // Entered ONLY when legs are active. Single-position logic below is UNCHANGED.
    if (multi_leg_) {
        if (legs_.isEmpty())
            return std::nullopt;

        // Compute current basket P&L from cached unrealized_pnl on each leg.
        double basket_pnl = 0;
        for (const auto& leg : std::as_const(legs_))
            basket_pnl += leg.unrealized_pnl;

        double basket_pnl_pct = basket_entry_value_ > 1e-10 ? basket_pnl / basket_entry_value_ * 100.0 : 0;

        // Drawdown tracking includes the open basket's marks.
        {
            const double equity = metrics_.total_pnl + basket_pnl;
            if (equity > risk_.peak_equity)
                risk_.peak_equity = equity;
            risk_.max_drawdown = std::max(risk_.max_drawdown, risk_.peak_equity - equity);
            metrics_.max_drawdown = risk_.max_drawdown;
        }

        // Daily loss limit — realized today PLUS the open basket's unrealized loss
        // (it used to wait for the loss to be realized, so an open position could run
        // far past the limit before anything happened).
        if (max_daily_loss_ > 0 && risk_.daily_pnl + basket_pnl <= -std::abs(max_daily_loss_)) {
            risk_.paused_by_loss_limit = true;
            risk_.halt_reason = QStringLiteral("daily_loss_limit");
            sync_risk_metrics_locked();
            AlgoOrderSignal sig;
            sig.deployment_id = deployment_id_;
            sig.reason = "daily_loss_limit";
            return sig;
        }

        // Max drawdown limit (realized + unrealized from the running peak).
        if (max_drawdown_limit_ > 0 && risk_.peak_equity - (metrics_.total_pnl + basket_pnl) >= max_drawdown_limit_) {
            risk_.halted_by_drawdown = true;
            risk_.halt_reason = QStringLiteral("max_drawdown_limit");
            sync_risk_metrics_locked();
            AlgoOrderSignal sig;
            sig.deployment_id = deployment_id_;
            sig.reason = "max_drawdown_limit";
            return sig;
        }

        // Stop loss
        if (stop_loss_pct_ > 0 && basket_pnl_pct <= -std::abs(stop_loss_pct_)) {
            AlgoOrderSignal sig;
            sig.deployment_id = deployment_id_;
            sig.reason = "stop_loss";
            return sig;
        }

        // Take profit
        if (take_profit_pct_ > 0 && basket_pnl_pct >= std::abs(take_profit_pct_)) {
            AlgoOrderSignal sig;
            sig.deployment_id = deployment_id_;
            sig.reason = "take_profit";
            return sig;
        }

        // Trailing stop — track peak basket P&L
        if (trailing_stop_pct_ > 0) {
            if (basket_pnl > basket_peak_pnl_)
                basket_peak_pnl_ = basket_pnl;
            double peak_pct = basket_entry_value_ > 1e-10 ? basket_peak_pnl_ / basket_entry_value_ * 100.0 : 0;
            if (peak_pct - basket_pnl_pct >= trailing_stop_pct_) {
                AlgoOrderSignal sig;
                sig.deployment_id = deployment_id_;
                sig.reason = "trailing_stop";
                return sig;
            }
        }

        return std::nullopt;
    }
    // ── End multi-leg branch ────────────────────────────────────────────────

    if (position_.side == PositionSide::None)
        return std::nullopt;

    // Daily loss limit — realized today PLUS the open position's unrealized loss
    // (net of its entry fees). It used to count realized P&L only, so an open
    // position could lose far beyond the limit before the check could see it.
    if (max_daily_loss_ > 0 && risk_.daily_pnl + position_.unrealized_pnl <= -std::abs(max_daily_loss_)) {
        risk_.paused_by_loss_limit = true;
        risk_.halt_reason = QStringLiteral("daily_loss_limit");
        sync_risk_metrics_locked();
        AlgoOrderSignal sig;
        sig.deployment_id = deployment_id_;
        sig.side = (position_.side == PositionSide::Long) ? "SELL" : "BUY";
        sig.quantity = position_.quantity;
        sig.reason = "daily_loss_limit";
        return sig;
    }

    // Max drawdown limit: realized + unrealized P&L this far below its running peak.
    if (max_drawdown_limit_ > 0 &&
        risk_.peak_equity - (metrics_.total_pnl + position_.unrealized_pnl) >= max_drawdown_limit_) {
        risk_.halted_by_drawdown = true;
        risk_.halt_reason = QStringLiteral("max_drawdown_limit");
        sync_risk_metrics_locked();
        AlgoOrderSignal sig;
        sig.deployment_id = deployment_id_;
        sig.side = (position_.side == PositionSide::Long) ? "SELL" : "BUY";
        sig.quantity = position_.quantity;
        sig.reason = "max_drawdown_limit";
        return sig;
    }

    double pnl_pct = 0;
    if (position_.side == PositionSide::Long && position_.entry_price > 1e-10)
        pnl_pct = (current_price - position_.entry_price) / position_.entry_price * 100.0;
    else if (position_.side == PositionSide::Short && position_.entry_price > 1e-10)
        pnl_pct = (position_.entry_price - current_price) / position_.entry_price * 100.0;

    // Stop loss
    if (stop_loss_pct_ > 0 && pnl_pct <= -std::abs(stop_loss_pct_)) {
        AlgoOrderSignal sig;
        sig.deployment_id = deployment_id_;
        sig.side = (position_.side == PositionSide::Long) ? "SELL" : "BUY";
        sig.quantity = position_.quantity;
        sig.reason = "stop_loss";
        return sig;
    }

    // Take profit
    if (take_profit_pct_ > 0 && pnl_pct >= std::abs(take_profit_pct_)) {
        AlgoOrderSignal sig;
        sig.deployment_id = deployment_id_;
        sig.side = (position_.side == PositionSide::Long) ? "SELL" : "BUY";
        sig.quantity = position_.quantity;
        sig.reason = "take_profit";
        return sig;
    }

    // Trailing stop
    if (trailing_stop_pct_ > 0) {
        if (position_.side == PositionSide::Long) {
            if (current_price > position_.highest_since_entry)
                position_.highest_since_entry = current_price;
            double peak_pnl = (position_.highest_since_entry - position_.entry_price) / position_.entry_price * 100.0;
            if (peak_pnl - pnl_pct >= trailing_stop_pct_) {
                AlgoOrderSignal sig;
                sig.deployment_id = deployment_id_;
                sig.side = "SELL";
                sig.quantity = position_.quantity;
                sig.reason = "trailing_stop";
                return sig;
            }
        } else {
            if (current_price < position_.lowest_since_entry || position_.lowest_since_entry < 1e-10)
                position_.lowest_since_entry = current_price;
            double peak_pnl = (position_.entry_price - position_.lowest_since_entry) / position_.entry_price * 100.0;
            if (peak_pnl - pnl_pct >= trailing_stop_pct_) {
                AlgoOrderSignal sig;
                sig.deployment_id = deployment_id_;
                sig.side = "BUY";
                sig.quantity = position_.quantity;
                sig.reason = "trailing_stop";
                return sig;
            }
        }
    }

    return std::nullopt;
}

void PositionManager::record_entry(PositionSide side, double qty, double price, int64_t time_ms, double fees) {
    QMutexLocker lock(&mutex_);
    Q_ASSERT(!multi_leg_); // invariant: single-leg path must not be called while a basket is active
    position_.side = side;
    position_.quantity = qty;
    position_.entry_price = price;
    position_.entry_time = time_ms;
    position_.entry_fees = std::max(0.0, fees);
    // The entry fees are already spent: the position starts that far under water.
    position_.unrealized_pnl = -position_.entry_fees;
    position_.highest_since_entry = price;
    position_.lowest_since_entry = price;
    metrics_.last_trade_time = time_ms;
    metrics_.unrealized_pnl = position_.unrealized_pnl;
    metrics_.total_fees += position_.entry_fees;
    ++risk_.trades_today;
    sync_risk_metrics_locked();
}

double PositionManager::record_exit(double qty, double price, int64_t time_ms, double fees) {
    QMutexLocker lock(&mutex_);
    double gross = 0;
    if (position_.side == PositionSide::Long)
        gross = (price - position_.entry_price) * qty;
    else if (position_.side == PositionSide::Short)
        gross = (position_.entry_price - price) * qty;
    // Entry fees in proportion to the quantity closed (a partial fill closes part).
    const double share = position_.quantity > 0 ? std::min(1.0, qty / position_.quantity) : 1.0;
    const double exit_fees = std::max(0.0, fees);
    const double pnl = gross - position_.entry_fees * share - exit_fees;
    last_exit_gross_ = gross;

    metrics_.total_pnl += pnl;
    metrics_.total_fees += exit_fees;
    metrics_.total_trades++;
    if (pnl > 0)
        metrics_.winning_trades++;
    else if (pnl < 0)
        metrics_.losing_trades++;
    metrics_.win_rate =
        metrics_.total_trades > 0 ? static_cast<double>(metrics_.winning_trades) / metrics_.total_trades * 100.0 : 0;
    metrics_.last_trade_time = time_ms;

    risk_.daily_pnl += pnl;

    position_.side = PositionSide::None;
    position_.quantity = 0;
    position_.entry_price = 0;
    position_.entry_time = 0;
    position_.unrealized_pnl = 0;
    position_.entry_fees = 0;
    metrics_.unrealized_pnl = 0;

    update_drawdown();
    latch_daily_loss_if_breached();
    sync_risk_metrics_locked();
    return pnl;
}

double PositionManager::last_exit_gross() const {
    QMutexLocker lock(&mutex_);
    return last_exit_gross_;
}

void PositionManager::set_max_drawdown_limit(double limit) {
    QMutexLocker lock(&mutex_);
    max_drawdown_limit_ = std::max(0.0, limit);
}

void PositionManager::sync_risk_metrics_locked() {
    metrics_.daily_pnl = risk_.daily_pnl;
    metrics_.trades_today = risk_.trades_today;
    metrics_.risk_halted = risk_.paused_by_loss_limit || risk_.halted_by_drawdown;
    metrics_.halt_reason = metrics_.risk_halted ? risk_.halt_reason : QString();
    metrics_.max_drawdown = risk_.max_drawdown;
}

void PositionManager::restore_risk_state(const RiskState& state, int today_key, double total_fees) {
    QMutexLocker lock(&mutex_);
    metrics_.total_fees = std::max(0.0, total_fees);
    if (state.peak_equity > risk_.peak_equity)
        risk_.peak_equity = state.peak_equity;
    risk_.halted_by_drawdown = state.halted_by_drawdown;
    // The daily figures only carry over within the same exchange trading day; a
    // restart on a new day starts clean (as the day rollover would have).
    if (state.day_key > 0 && today_key > 0 && state.day_key == today_key) {
        risk_.day_key = state.day_key;
        risk_.daily_pnl = state.daily_pnl;
        risk_.trades_today = state.trades_today;
        risk_.paused_by_loss_limit = state.paused_by_loss_limit;
    } else {
        risk_.day_key = today_key;
    }
    if (risk_.halted_by_drawdown || risk_.paused_by_loss_limit)
        risk_.halt_reason = state.halt_reason;
    latch_daily_loss_if_breached();
    sync_risk_metrics_locked();
}

void PositionManager::clear_halts() {
    QMutexLocker lock(&mutex_);
    reset_daily_locked();
    risk_.halted_by_drawdown = false;
    risk_.halt_reason.clear();
    // Re-base the drawdown peak on where the P&L is now, or the limit trips again
    // on the next tick.
    risk_.peak_equity = metrics_.total_pnl + (multi_leg_ ? metrics_.unrealized_pnl : position_.unrealized_pnl);
    sync_risk_metrics_locked();
}

// The daily-loss halt used to be raised only from check_risk() while a position was
// open. A realized loss that breaches the limit closes the position, so check_risk()
// then returns early (no position) and never sees the breach — the runner stayed
// un-latched, took one MORE entry, and only halted when that fresh position's next
// tick tripped the check (an extra round trip beyond the configured loss limit).
// Latching on the realizing exit itself closes that hole. Caller holds mutex_.
void PositionManager::latch_daily_loss_if_breached() {
    if (max_daily_loss_ > 0 && risk_.daily_pnl <= -std::abs(max_daily_loss_)) {
        risk_.paused_by_loss_limit = true;
        if (risk_.halt_reason.isEmpty())
            risk_.halt_reason = QStringLiteral("daily_loss_limit");
    }
}

void PositionManager::restore_state(PositionSide side, double qty, double entry_price, double total_pnl,
                                    int total_trades, double win_rate, double max_drawdown) {
    QMutexLocker lock(&mutex_);
    position_.side = side;
    position_.quantity = qty;
    position_.entry_price = entry_price;
    position_.highest_since_entry = entry_price; // trailing-stop baseline resets on resume
    position_.lowest_since_entry = entry_price;

    metrics_.total_pnl = total_pnl;
    metrics_.total_trades = total_trades;
    metrics_.win_rate = win_rate;
    metrics_.winning_trades = static_cast<int>(std::lround(win_rate * total_trades / 100.0));
    metrics_.losing_trades = total_trades - metrics_.winning_trades;
    if (metrics_.losing_trades < 0)
        metrics_.losing_trades = 0;
    metrics_.max_drawdown = max_drawdown;
    metrics_.current_position_qty = qty;
    metrics_.current_position_side = position_side_to_string(side);
    metrics_.current_position_entry = entry_price;

    risk_.max_drawdown = max_drawdown;
    risk_.peak_equity = total_pnl > 0 ? total_pnl : 0;
}

void PositionManager::update_price(double price) {
    QMutexLocker lock(&mutex_);
    metrics_.current_price = price;
    // Mark-to-market net of the entry fees already paid.
    if (position_.side == PositionSide::Long)
        position_.unrealized_pnl = (price - position_.entry_price) * position_.quantity - position_.entry_fees;
    else if (position_.side == PositionSide::Short)
        position_.unrealized_pnl = (position_.entry_price - price) * position_.quantity - position_.entry_fees;
    metrics_.unrealized_pnl = position_.unrealized_pnl;
    metrics_.current_position_qty = position_.quantity;
    metrics_.current_position_side = position_side_to_string(position_.side);
    metrics_.current_position_entry = position_.entry_price;
    if (position_.side != PositionSide::None)
        update_drawdown();
}

// Drawdown of realized + UNREALIZED P&L from its running peak. It used to be
// measured on realized P&L at exits only, so an open position's slide never
// showed up until (and unless) it was closed.
void PositionManager::update_drawdown() {
    const double unrealized = multi_leg_ ? metrics_.unrealized_pnl : position_.unrealized_pnl;
    const double equity = metrics_.total_pnl + unrealized;
    if (equity > risk_.peak_equity)
        risk_.peak_equity = equity;
    const double dd = risk_.peak_equity - equity;
    if (dd > risk_.max_drawdown)
        risk_.max_drawdown = dd;
    metrics_.max_drawdown = risk_.max_drawdown;
}

bool PositionManager::has_position() const {
    QMutexLocker lock(&mutex_);
    return position_.side != PositionSide::None;
}

bool PositionManager::is_paused() const {
    QMutexLocker lock(&mutex_);
    return risk_.paused_by_loss_limit || risk_.halted_by_drawdown;
}

bool PositionManager::validate_order_value(double qty, double price) const {
    return validate_order_notional(qty * price);
}

bool PositionManager::validate_order_notional(double notional) const {
    if (max_order_value_ <= 0)
        return true;
    return std::abs(notional) <= max_order_value_;
}

AlgoPosition PositionManager::position() const {
    QMutexLocker lock(&mutex_);
    return position_;
}

AlgoMetrics PositionManager::metrics() const {
    QMutexLocker lock(&mutex_);
    return metrics_;
}

RiskState PositionManager::risk_state() const {
    QMutexLocker lock(&mutex_);
    return risk_;
}

void PositionManager::reset_daily_locked() {
    risk_.daily_pnl = 0;
    risk_.paused_by_loss_limit = false;
    risk_.trades_today = 0;
    if (!risk_.halted_by_drawdown)
        risk_.halt_reason.clear();
    risk_.day_start_epoch = QDateTime::currentMSecsSinceEpoch();
    sync_risk_metrics_locked();
}

bool PositionManager::reset_daily_if_new_day(int exchange_day_key) {
    QMutexLocker lock(&mutex_);
    if (exchange_day_key <= 0)
        return false;
    if (risk_.day_key <= 0) {
        risk_.day_key = exchange_day_key; // first stamp: today is day one
        return false;
    }
    if (exchange_day_key == risk_.day_key)
        return false;
    risk_.day_key = exchange_day_key;
    reset_daily_locked();
    return true;
}

void PositionManager::reset_daily() {
    QMutexLocker lock(&mutex_);
    reset_daily_locked();
}

bool PositionManager::reset_daily_if_new_day() {
    QMutexLocker lock(&mutex_);
    if (risk_.day_start_epoch <= 0) {
        // Never stamped (restored state / defensive) — adopt today as day one.
        risk_.day_start_epoch = QDateTime::currentMSecsSinceEpoch();
        return false;
    }
    // day_start_epoch is stamped with currentMSecsSinceEpoch(), so the matching
    // trading date is the LOCAL date of that instant. Compare dates, not a fixed
    // 24h window, so the reset lands on the day boundary regardless of when the
    // deployment was started.
    const QDate start_day = QDateTime::fromMSecsSinceEpoch(risk_.day_start_epoch).date();
    if (start_day >= QDate::currentDate())
        return false;
    reset_daily_locked();
    return true;
}

// ── Multi-leg basket methods (P3) ───────────────────────────────────────────

void PositionManager::record_entry_legs(const QVector<fincept::algo::AlgoLegPosition>& legs, int64_t time_ms) {
    QMutexLocker lock(&mutex_);
    Q_ASSERT(position_.side ==
             PositionSide::None); // invariant: multi-leg path must not be called while a single-leg position is open
    legs_ = legs;
    multi_leg_ = true;
    basket_peak_pnl_ = 0;

    // Compute entry value: Σ |entry_price * quantity| (unsigned, used as denominator)
    basket_entry_value_ = 0;
    double total_qty = 0;
    for (const auto& leg : std::as_const(legs_)) {
        basket_entry_value_ += std::abs(leg.entry_price * leg.quantity);
        total_qty += leg.quantity;
    }

    metrics_.current_position_side = QStringLiteral("BASKET");
    metrics_.current_position_qty = total_qty;
    metrics_.last_trade_time = time_ms;
    ++risk_.trades_today;
    sync_risk_metrics_locked();
}

double PositionManager::record_exit_legs(int64_t time_ms) {
    QMutexLocker lock(&mutex_);
    double pnl = 0;
    for (const auto& leg : std::as_const(legs_))
        pnl += leg.unrealized_pnl;

    metrics_.total_pnl += pnl;
    metrics_.total_trades++;
    if (pnl > 0)
        metrics_.winning_trades++;
    else if (pnl < 0)
        metrics_.losing_trades++;
    metrics_.win_rate =
        metrics_.total_trades > 0 ? static_cast<double>(metrics_.winning_trades) / metrics_.total_trades * 100.0 : 0;
    metrics_.last_trade_time = time_ms;
    metrics_.unrealized_pnl = 0;

    risk_.daily_pnl += pnl;
    update_drawdown();
    latch_daily_loss_if_breached();
    sync_risk_metrics_locked();

    legs_.clear();
    multi_leg_ = false;
    basket_entry_value_ = 0;
    basket_peak_pnl_ = 0;

    metrics_.current_position_side = QString();
    metrics_.current_position_qty = 0;

    return pnl;
}

void PositionManager::update_leg_price(const QString& symbol, double ltp) {
    QMutexLocker lock(&mutex_);
    bool found = false;
    for (auto& leg : legs_) {
        if (leg.symbol == symbol) {
            leg.current_price = ltp;
            leg.unrealized_pnl = (ltp - leg.entry_price) * leg.quantity * leg.side_sign;
            found = true;
            break;
        }
    }
    if (!found) // per tick: one line per symbol every 5 minutes
        LOG_WARN_RL("AlgoEngine", QStringLiteral("leg_price:") + symbol, 300000,
                    logf("Leg price update ignored, symbol not in basket", {{"symbol", symbol}}));
    // Recompute basket unrealized P&L
    double basket_pnl = 0;
    for (const auto& leg : std::as_const(legs_))
        basket_pnl += leg.unrealized_pnl;
    metrics_.unrealized_pnl = basket_pnl;
}

bool PositionManager::has_legs() const {
    QMutexLocker lock(&mutex_);
    return multi_leg_ && !legs_.isEmpty();
}

QVector<fincept::algo::AlgoLegPosition> PositionManager::legs() const {
    QMutexLocker lock(&mutex_);
    return legs_;
}

} // namespace fincept::algo
