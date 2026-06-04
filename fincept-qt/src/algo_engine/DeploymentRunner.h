// src/algo_engine/DeploymentRunner.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/CandleAggregator.h"
#include "algo_engine/PositionManager.h"
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

private slots:
    void on_candle_closed(const fincept::algo::OhlcvCandle& candle);
    void on_tick_data(const QVariant& data);
    void on_heartbeat();

private:
    // Live market data: poll the connected broker's quote REST endpoint on a timer
    // and feed each BrokerQuote into on_tick_data(). Self-contained — does NOT touch
    // the shared AccountDataStream/WebSocket (whose single-symbol subscription is
    // owned by the Equity screen), so it can't clobber other consumers.
    void start_market_data();
    void stop_market_data();
    void poll_quote();
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
    // Re-seed position + metrics from algo_metrics so a resumed deployment continues
    // its open position across restarts (no-op for a fresh deploy — no row yet).
    void restore_state_from_db();
    void update_deployment_status(const QString& status);

    fincept::services::algo::AlgoDeployment deployment_;
    fincept::services::algo::AlgoStrategy strategy_;
    Timeframe timeframe_;

    std::unique_ptr<CandleAggregator> aggregator_;
    std::unique_ptr<PositionManager> position_mgr_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    QTimer* heartbeat_timer_ = nullptr;
    QTimer* quote_timer_ = nullptr;     // drives poll_quote()
    int64_t last_heartbeat_ms_ = 0;
    bool first_tick_logged_ = false;    // log the first live quote once, for trackability
    bool live_mode_ = false;            // timeframe == "live" → evaluate per tick
    int64_t last_emit_ms_ = 0;          // throttle for live_update emission
    double last_tick_price_ = 0;        // previous tick price → tick-to-tick crossovers

    struct PendingOrder {
        QString broker_order_id;
        AlgoOrderSignal signal;
        int64_t submitted_ms = 0;
    };
    QVector<PendingOrder> pending_orders_;
};

} // namespace fincept::algo
