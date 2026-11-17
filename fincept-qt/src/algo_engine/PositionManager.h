// src/algo_engine/PositionManager.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QMutex>

#include <optional>

namespace fincept::algo {

class PositionManager {
  public:
    explicit PositionManager(const QString& deployment_id, double stop_loss_pct, double take_profit_pct,
                             double trailing_stop_pct, double max_order_value, double max_daily_loss);

    std::optional<AlgoOrderSignal> check_risk(double current_price);

    /// `fees` = commission + taxes of the fill (paper: simulated; live: estimated
    /// from the cost model). They are carried with the position and deducted from
    /// the trade's P&L when it closes.
    void record_entry(PositionSide side, double qty, double price, int64_t time_ms, double fees = 0);
    /// Returns the NET realized P&L (price move minus entry and exit fees).
    double record_exit(double qty, double price, int64_t time_ms, double fees = 0);
    /// Price-move P&L of the last record_exit (before fees).
    double last_exit_gross() const;

    // Re-seed position + cumulative metrics after an app restart so a resumed
    // deployment continues its open position instead of starting flat.
    void restore_state(PositionSide side, double qty, double entry_price, double total_pnl, int total_trades,
                       double win_rate, double max_drawdown);
    /// Restore the persisted daily / drawdown risk state (v057). The daily part is
    /// only taken when `state.day_key` is the current exchange trading day.
    void restore_risk_state(const RiskState& state, int today_key, double total_fees);

    /// Max-drawdown limit in currency (0 = off): realized + unrealized P&L falling
    /// this far from its peak exits the position and halts until clear_halts().
    void set_max_drawdown_limit(double limit);

    bool has_position() const;
    /// Entries halted by the daily-loss limit or the max-drawdown limit.
    bool is_paused() const;
    bool validate_order_value(double qty, double price) const;
    // Same ceiling as validate_order_value, for a pre-computed notional. The
    // multi-leg F&O basket path has no single (qty × price) — its notional is a
    // sum over the legs — so it checks the limit through here.
    bool validate_order_notional(double notional) const;

    AlgoPosition position() const;
    AlgoMetrics metrics() const;
    RiskState risk_state() const;
    void update_price(double price);
    void reset_daily();
    /// Manual resume: clears the daily-loss latch AND the drawdown halt, and
    /// re-bases the drawdown peak on the current P&L (or it would trip again at once).
    void clear_halts();
    // Clears the daily P&L and the loss-limit latch once the local trading date
    // has moved past day_start_epoch; returns true when that rollover happened.
    // Without a caller for this the "daily" loss limit latches for the lifetime
    // of the process — a lifetime limit, not a daily one. The date is taken in
    // local time, matching how day_start_epoch is stamped in the constructor.
    bool reset_daily_if_new_day();
    /// Same, on the EXCHANGE trading date (yyyymmdd, from MarketSessions) — the
    /// day a venue's session belongs to, not the machine's local date. The
    /// drawdown halt is not a daily limit and is not cleared here.
    bool reset_daily_if_new_day(int exchange_day_key);

    // Multi-leg (F&O basket) mode — parallel to the single-position equity path.
    void record_entry_legs(const QVector<fincept::algo::AlgoLegPosition>& legs, int64_t time_ms);
    double record_exit_legs(int64_t time_ms);                 // realizes basket P&L, clears legs
    void update_leg_price(const QString& symbol, double ltp); // marks one leg, recomputes basket unrealized
    bool has_legs() const;                                    // multi_leg_ && !legs_.isEmpty()
    QVector<fincept::algo::AlgoLegPosition> legs() const;     // mutex copy

  private:
    void update_drawdown();
    // Mirror the risk state into metrics_ (daily P&L, halt flags). Caller holds mutex_.
    void sync_risk_metrics_locked();
    // Raises paused_by_loss_limit once realized daily P&L breaches max_daily_loss_.
    // Assumes mutex_ is already held.
    void latch_daily_loss_if_breached();
    // Body of reset_daily(); assumes mutex_ is already held (QMutex is not
    // recursive, so reset_daily_if_new_day() cannot call reset_daily()).
    void reset_daily_locked();

    QString deployment_id_;
    AlgoPosition position_;
    RiskState risk_;
    AlgoMetrics metrics_;

    double stop_loss_pct_;
    double take_profit_pct_;
    double trailing_stop_pct_;
    double max_order_value_;
    double max_daily_loss_;
    double max_drawdown_limit_ = 0;
    double last_exit_gross_ = 0;

    // Multi-leg basket state (P3)
    QVector<fincept::algo::AlgoLegPosition> legs_;
    bool multi_leg_ = false;
    double basket_entry_value_ = 0; // Σ |entry_price * quantity| at entry
    double basket_peak_pnl_ = 0;    // trailing-stop high-water mark (basket P&L)

    mutable QMutex mutex_;
};

} // namespace fincept::algo
