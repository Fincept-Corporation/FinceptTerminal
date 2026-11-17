// src/algo_engine/DeploymentRunner.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/CandleAggregator.h"
#include "algo_engine/ExecutionModel.h"
#include "algo_engine/PositionManager.h"
#include "algo_engine/RiskLimits.h"
#include "algo_engine/fno/FnoDataBridge.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QDate>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <memory>

namespace fincept::core::market {
struct Venue;
}

namespace fincept::algo {

class DeploymentRunner : public QObject {
    Q_OBJECT
  public:
    explicit DeploymentRunner(const fincept::services::algo::AlgoDeployment& deployment,
                              const fincept::services::algo::AlgoStrategy& strategy, QObject* parent = nullptr);
    ~DeploymentRunner() override;

    void start();
    void pause();
    void resume();
    void stop();
    bool is_running() const { return running_.load(); }
    bool is_paused() const { return paused_.load(); }
    QString deployment_id() const { return deployment_.id; }
    /// Immutable after construction - safe to read from any thread.
    const fincept::services::algo::AlgoDeployment& deployment() const { return deployment_; }

    /// Kill switch, callable from ANY thread: from this moment the runner emits no
    /// further order signal (entries, exits, stop-loss/take-profit), even if the
    /// engine thread has not yet processed the queued stop(). AlgoEngine sets it
    /// first on every stop so a busy or hung engine thread cannot keep trading.
    void request_stop() { stop_requested_.store(true); }
    bool stop_requested() const { return stop_requested_.load(); }

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
    // Result of request_flatten(): `flat` = no position is open any more (closed
    // through the normal exit path, or there was none).
    void flatten_finished(const QString& deployment_id, bool flat, const QString& detail);

  public slots:
    // PAPER: the simulator accepted the order. It fills on the NEXT quote — the
    // next bar's open for a bar strategy, as in the backtest — at that price moved
    // adversely by the cost model's slippage, with the model's fees deducted.
    void on_paper_order_accepted(const QString& sim_order_id);
    // Close the open position (equity or F&O basket) through the normal exit path —
    // emit_order_signal → AlgoEngine::execute_order, i.e. paper simulation or the
    // live broker with its approval queue and fill confirmation — then report
    // flatten_finished. No new entries are taken meanwhile. Engine thread.
    void request_flatten();
    // A CONFIRMED fill (paper simulation, or the broker order book reporting the
    // order complete). fill_price <= 0 / fill_qty <= 0 fall back to the signal's
    // reference price / quantity.
    void on_order_filled(const QString& broker_order_id, double fill_price, double fill_qty);
    void on_order_rejected(const QString& broker_order_id, const QString& reason);
    // LIVE: the broker ACCEPTED the order (acceptance is not a fill). The order
    // stays in flight — no position is recorded — until the engine's order-book
    // check reports it filled (on_order_filled), dead (on_order_rejected) or
    // unknown (on_order_unconfirmed).
    void on_order_accepted(const QString& broker_order_id);
    // LIVE: the fill could not be confirmed (order still open / missing / book
    // unreadable after the bounded check). Fail safe: no position is recorded and
    // the deployment latches — it places NO further order (entry, exit or
    // stop-loss) until the user stops it and checks the broker.
    void on_order_unconfirmed(const QString& broker_order_id, const QString& detail);
    // Semi-Auto approval gate (ActionCenter). The engine calls the first when it
    // parks the in-flight order in the approval queue instead of sending it, and
    // the second when the user approves or rejects it. The in-flight guard is
    // held throughout, so a deployment cannot stack duplicate approval requests
    // while one is waiting on a human.
    void on_order_queued_for_approval(const QString& approval_id);
    void on_approval_resolved(const QString& approval_id, bool approved, const QString& broker_order_id,
                              const QString& detail);
    // Multi-leg F&O basket fills (P3.4). One call per leg from AlgoEngine::execute_basket.
    // Fills accumulate; once every leg of the in-flight basket is accounted for, an
    // entry records the basket position (record_entry_legs) and an exit realizes it
    // (record_exit_legs). If any leg was rejected on entry the basket is abandoned
    // (the engine has already rolled back the filled legs at the broker).
    void on_leg_filled(int leg_index, const fincept::algo::AlgoOrderLeg& leg, double fill_price, double fill_qty);
    void on_leg_rejected(int leg_index, const QString& reason);

  private slots:
    void on_candle_closed(const fincept::algo::OhlcvCandle& candle);
    void on_tick_data(const QVariant& data);
    void on_heartbeat();
    // F&O: feeds the underlying's spot from the option-chain snapshot whenever the
    // broker quote feed on the underlying is silent (index underlyings such as
    // NIFTY are not resolvable on every broker's account quote stream).
    void on_fno_spot_poll();

  private:
    // Symbol whose price drives the deployment: the traded symbol for equity, the
    // UNDERLYING for F&O (an F&O deployment carries no single symbol).
    QString feed_symbol() const;
    bool is_fno() const;
    // Expiry whose chain the runner reads: the open basket's expiry, else the
    // strategy's expiry rule resolved against today's expiry list.
    QString fno_chain_expiry();
    // Best current price for an activity snapshot.
    double current_mark() const;
    // Surfaces a rejected order on the Dashboard card + error stream; alerts after
    // repeated EXIT rejections (the position is still open).
    void report_rejection(bool was_exit, const QString& what, const QString& reason);
    // Error toast on the GUI thread.
    void post_alert(const QString& message) const;
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
    // Builds the exit order for the open position (equity side/qty or the F&O
    // basket's closing legs). False when nothing is open.
    bool build_exit_signal(const QString& reason, double ref_price, AlgoOrderSignal& out);
    // The deployment's extra entry limits (trades per day, entry window, regular
    // session only, flatten in progress). Only ever BLOCKS entries; `why` = reason.
    bool entry_gates_allow(QString* why) const;
    // Fills a paper order waiting for its next quote (see on_paper_order_accepted).
    void fill_waiting_paper_order(double price, int64_t quote_ms);
    // Commission + taxes of a fill per the cost model (paper: what the simulator
    // charges; live: an ESTIMATE — brokers do not report fees with the fill).
    double estimate_fees(const QString& side, double qty, double price) const;
    // ATR(14) of the closed bars (slippage scaling); NaN when not enough bars.
    double recent_atr() const;
    // Every entry + exit leaf evaluated on `candles` (full details, stable rows).
    QVector<AlgoConditionStatus> condition_rows(const QVector<OhlcvCandle>& candles) const;
    // Exchange trading date of now (yyyymmdd) — daily-limit rollover.
    int today_key() const;
    // Realized + unrealized P&L into algo_equity_snapshots (throttled unless forced).
    void persist_equity_snapshot(bool force);
    // One line into algo_events (signal / order / fill / reject / risk / info).
    void log_event(const QString& kind, const QString& message);
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

    const fincept::core::market::Venue* venue_ = nullptr; // exchange calendar of the feed symbol
    CostModel cost_model_;          // paper fill realism + estimated live fees
    DeploymentRiskLimits limits_;   // extra (more conservative) entry / drawdown limits
    QVector<AlgoConditionStatus> cond_rows_; // last closed-bar evaluation (bar strategies)
    int64_t cond_rows_ms_ = 0;
    int64_t last_equity_snap_ms_ = 0;
    double last_equity_snap_value_ = 0;
    bool flatten_pending_ = false;  // request_flatten() in progress
    int64_t flatten_started_ms_ = 0;
    QString last_gate_note_;        // last entry-gate refusal (de-dupes the activity line)

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stop_requested_{false};
    QTimer* heartbeat_timer_ = nullptr;
    int64_t last_heartbeat_ms_ = 0;
    bool first_tick_logged_ = false; // log the first live quote once, for trackability
    bool live_mode_ = false;         // timeframe == "live" → evaluate per tick
    int64_t last_emit_ms_ = 0;       // throttle for live_update emission
    double last_tick_price_ = 0;     // previous tick price → tick-to-tick crossovers
    int64_t last_reject_ms_ = 0;     // when the last order was rejected (retry back-off)
    int consecutive_exit_rejects_ = 0; // exit/stop-loss rejections in a row (alert threshold)
    // Latched when a LIVE order's fill could not be confirmed: no further order of
    // any kind leaves this runner (see on_order_unconfirmed). Cleared only by stop.
    bool fill_unconfirmed_ = false;
    QString last_entry_block_; // last "F&O entry refused" reason (de-dupes the note)

    // F&O spot-from-chain feed (see on_fno_spot_poll).
    QTimer* fno_spot_timer_ = nullptr;
    qint64 last_chain_spot_ts_ = 0;  // chain timestamp_ms last fed as a tick
    int64_t last_feed_tick_ms_ = 0;  // last quote from the broker quote feed
    QString spot_expiry_;            // cached rule-resolved expiry ...
    QDate spot_expiry_day_;          // ... and the day it was resolved for

    // Finalize the in-flight multi-leg basket once every leg has reported a
    // fill or rejection (called from on_leg_filled / on_leg_rejected).
    void finalize_basket_if_complete();

    struct PendingOrder {
        QString broker_order_id;
        AlgoOrderSignal signal;
        int64_t submitted_ms = 0;
        // Set when the order was parked in the Semi-Auto approval queue instead of
        // being sent. approval_id is ActionCenter's pending id; it keys the
        // approve/reject callback back to this exact order.
        bool awaiting_approval = false;
        QString approval_id;
        // LIVE: accepted by the broker, fill confirmation pending (never purged
        // as a "lost ack" — the engine's bounded order-book check resolves it).
        bool awaiting_fill = false;
        // LIVE: fill could not be confirmed — held forever (blocks every order).
        bool unconfirmed = false;
        // PAPER: accepted by the simulator, fills on the next quote.
        bool paper_working = false;
    };
    QVector<PendingOrder> pending_orders_;

    // In-flight multi-leg basket accumulator (only one order is in flight at a
    // time — see the guard in emit_order_signal). Entry fills become open leg
    // positions; rejections are counted so a partial entry records nothing.
    QVector<AlgoLegPosition> basket_fills_;
    int basket_rejected_ = 0;
};

} // namespace fincept::algo
