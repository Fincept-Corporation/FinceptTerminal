// src/algo_engine/PositionManager.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QMutex>

#include <optional>

namespace fincept::algo {

class PositionManager {
public:
    explicit PositionManager(const QString& deployment_id,
                             double stop_loss_pct, double take_profit_pct,
                             double trailing_stop_pct, double max_order_value,
                             double max_daily_loss);

    std::optional<AlgoOrderSignal> check_risk(double current_price);

    void record_entry(PositionSide side, double qty, double price, int64_t time_ms);
    double record_exit(double qty, double price, int64_t time_ms);

    // Re-seed position + cumulative metrics after an app restart so a resumed
    // deployment continues its open position instead of starting flat.
    void restore_state(PositionSide side, double qty, double entry_price,
                       double total_pnl, int total_trades, double win_rate, double max_drawdown);

    bool has_position() const;
    bool is_paused() const;
    bool validate_order_value(double qty, double price) const;

    AlgoPosition position() const;
    AlgoMetrics metrics() const;
    RiskState risk_state() const;
    void update_price(double price);
    void reset_daily();

private:
    void update_drawdown();

    QString deployment_id_;
    AlgoPosition position_;
    RiskState risk_;
    AlgoMetrics metrics_;

    double stop_loss_pct_;
    double take_profit_pct_;
    double trailing_stop_pct_;
    double max_order_value_;
    double max_daily_loss_;

    mutable QMutex mutex_;
};

} // namespace fincept::algo
