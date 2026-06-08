// src/algo_engine/DeploymentRunner.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/CandleAggregator.h"
#include "algo_engine/PositionManager.h"
#include "algo_engine/fno/FnoDataBridge.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QObject>
#include <QTimer>

#include <atomic>
#include <memory>

namespace fincept::algo {

class DeploymentRunner : public QObject {
    Q_OBJECT
public:
    explicit DeploymentRunner(const fincept::services::algo::AlgoDeployment& deployment,
                              const fincept::services::algo::AlgoStrategy& strategy,
                              QObject* parent = nullptr);
    ~DeploymentRunner() override;

    void start();
    void pause();
    void resume();
    void stop();
    bool is_running() const { return running_.load(); }
    bool is_paused() const { return paused_.load(); }
    QString deployment_id() const { return deployment_.id; }

    // Set the GUI-thread FnoDataBridge so F&O runners can request chain snapshots
    // and pin their legs. Called from AlgoEngine::start_deployment on the calling
    // thread before the runner is moved to the engine thread. Consumption is P3.3.
    void set_fno_bridge(fincept::algo::fno::FnoDataBridge* bridge) { fno_bridge_ = bridge; }

    AlgoMetrics metrics() const;
    AlgoPosition position() const;

signals:
    void trade_executed(const fincept::algo::AlgoTradeRecord& trade);
    void metrics_updated(const QString& deployment_id, const fincept::algo::AlgoMetrics& metrics);
    void status_changed(const QString& deployment_id, const QString& status);
    void order_requested(const fincept::algo::AlgoOrderSignal& signal);
    void error_occurred(const QString& deployment_id, const QString& error);
    // Real-time snapshot for the Dashboard (LTP, P&L, position, per-condition status).
    void live_update(const QString& deployment_id, const fincept::algo::AlgoLiveSnapshot& snap);

public slots:
    void on_order_filled(const QString& broker_order_id, double fill_price, double fill_qty);
    void on_order_rejected(const QString& broker_order_id, const QString& reason);
    // Multi-leg F&O basket fills (P3.4). One call per leg from AlgoEngine::execute_basket.
    // Fills accumulate; once every leg of the in-flight basket is accounted for, an
    // entry records the basket position (record_entry_legs) and an exit realizes it
    // (record_exit_legs). If any leg was rejected on entry the basket is abandoned
    // (the engine has already rolled back the filled legs at the broker).
    void on_leg_filled(int leg_index, const fincept::algo::AlgoOrderLeg& leg,
                       double fill_price, double fill_qty);
    void on_leg_rejected(int leg_index, const QString& reason);

private slots:
    void on_candle_closed(const fincept::algo::OhlcvCandle& candle);
    void on_tick_data(const QVariant& data);
    void on_heartbeat();

private:
    // Live market data: subscribe to the shared per-account quote feed via
    // DataStreamManager (the same feed the Equity watchlist uses). Quotes arrive
    // on the engine thread via on_tick_data(). No private broker polling — one
    // connection per (account, symbol) is shared across all consumers.
    void start_market_data();
    void stop_market_data();
    // Builds the candle window used for live (per-tick) evaluation: the closed
    // history plus the previous and current tick as the last two bars, so a
    // crossover is detected tick-to-tick against the live price.
    QVector<OhlcvCandle> live_eval_window(double price) const;
    // Evaluates entry/exit each tick (live timeframe only) and emits a snapshot.
    void evaluate_live(double price);
    // Pushes the real-time snapshot (LTP, P&L, position, per-condition status) to
    // the Dashboard, throttled. `note` is a short activity line.
    void emit_live_snapshot(double price, const QString& note);
    void evaluate_entry(const QVector<OhlcvCandle>& candles);
    void evaluate_exit(const QVector<OhlcvCandle>& candles);
    void emit_order_signal(const AlgoOrderSignal& signal);
    void persist_trade(const AlgoTradeRecord& trade);
    void persist_metrics();
    // True when the runner holds an open position of EITHER kind — a single-symbol
    // equity position or a multi-leg F&O basket. Entry/exit routing must treat a
    // basket as "in position" or it would keep re-entering (has_position() alone
    // only sees the single-symbol path).
    bool in_position() const;
    // Persist / clear the open F&O basket on the algo_deployments row so a
    // restarted runner reattaches to it (resolved_legs_json + resolved_expiry).
    void persist_resolved_legs();
    void clear_resolved_legs();
    // Re-seed position + metrics from algo_metrics so a resumed deployment continues
    // its open position across restarts (no-op for a fresh deploy — no row yet).
    void restore_state_from_db();
    void update_deployment_status(const QString& status);

    fincept::services::algo::AlgoDeployment deployment_;
    fincept::services::algo::AlgoStrategy strategy_;
    Timeframe timeframe_;
    fincept::algo::fno::FnoDataBridge* fno_bridge_ = nullptr; // not owned; lives on main thread
    QString resolved_expiry_; // F&O: expiry chosen at entry; used to key the chain snapshot for leg marks

    std::unique_ptr<CandleAggregator> aggregator_;
    std::unique_ptr<PositionManager> position_mgr_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    QTimer* heartbeat_timer_ = nullptr;
    int64_t last_heartbeat_ms_ = 0;
    bool first_tick_logged_ = false;    // log the first live quote once, for trackability
    bool live_mode_ = false;            // timeframe == "live" → evaluate per tick
    int64_t last_emit_ms_ = 0;          // throttle for live_update emission
    double last_tick_price_ = 0;        // previous tick price → tick-to-tick crossovers

    // Finalize the in-flight multi-leg basket once every leg has reported a
    // fill or rejection (called from on_leg_filled / on_leg_rejected).
    void finalize_basket_if_complete();

    struct PendingOrder {
        QString broker_order_id;
        AlgoOrderSignal signal;
        int64_t submitted_ms = 0;
    };
    QVector<PendingOrder> pending_orders_;

    // In-flight multi-leg basket accumulator (only one order is in flight at a
    // time — see the guard in emit_order_signal). Entry fills become open leg
    // positions; rejections are counted so a partial entry records nothing.
    QVector<AlgoLegPosition> basket_fills_;
    int basket_rejected_ = 0;
};

} // namespace fincept::algo
