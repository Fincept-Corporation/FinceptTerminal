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

public slots:
    void on_order_filled(const QString& broker_order_id, double fill_price, double fill_qty);
    void on_order_rejected(const QString& broker_order_id, const QString& reason);

private slots:
    void on_candle_closed(const fincept::algo::OhlcvCandle& candle);
    void on_tick_data(const QVariant& data);
    void on_heartbeat();

private:
    void subscribe_tick_topic();
    void unsubscribe_tick_topic();
    void evaluate_entry(const QVector<OhlcvCandle>& candles);
    void evaluate_exit(const QVector<OhlcvCandle>& candles);
    void emit_order_signal(const AlgoOrderSignal& signal);
    void persist_trade(const AlgoTradeRecord& trade);
    void persist_metrics();
    void update_deployment_status(const QString& status);

    fincept::services::algo::AlgoDeployment deployment_;
    fincept::services::algo::AlgoStrategy strategy_;
    Timeframe timeframe_;

    std::unique_ptr<CandleAggregator> aggregator_;
    std::unique_ptr<PositionManager> position_mgr_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    QMetaObject::Connection tick_connection_;
    QTimer* heartbeat_timer_ = nullptr;
    int64_t last_heartbeat_ms_ = 0;

    struct PendingOrder {
        QString broker_order_id;
        AlgoOrderSignal signal;
        int64_t submitted_ms = 0;
    };
    QVector<PendingOrder> pending_orders_;
};

} // namespace fincept::algo
