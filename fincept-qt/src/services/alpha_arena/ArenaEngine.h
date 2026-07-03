#pragma once
// ArenaEngine — competition orchestrator (spec §3.1, §7). Singleton; runs
// while the screen is hidden; UI is a viewer over signals + ArenaStore.
#include "core/result/Result.h"
#include "services/alpha_arena/ArenaLlmClient.h"
#include "services/alpha_arena/ArenaMarketDataIface.h"
#include "services/alpha_arena/ArenaRisk.h"
#include "services/alpha_arena/ArenaStore.h"
#include "services/alpha_arena/ArenaTypes.h"
#include "services/alpha_arena/PaperExchange.h"
#include <QTimer>
#include <memory>

namespace fincept::arena {

class ArenaEngine : public QObject {
    Q_OBJECT
  public:
    static ArenaEngine& instance();
    void init();   // idempotent; crash-recovery scan (emits crash_recovery_pending)

    Result<QString> create_competition(const ArenaConfig& cfg);   // persists; status "created"
    Result<void> start(const QString& competition_id);            // load + begin ticking
    Result<void> pause();                                         // status "paused", timer stops
    Result<void> resume();                                        // status "running"
    Result<void> stop();      // status "ended"; positions stay; pending HITL rows persist
    Result<void> kill_all();  // refresh marks, close every position, reject pending HITL + stop
    /// Per-agent kill switch: close that agent's positions now, halt it
    /// ("halted_user"), snapshot its equity. Competition keeps running.
    Result<void> kill_agent(const QString& agent_id);
    Result<void> halt_agent(const QString& agent_id);
    Result<void> resume_agent(const QString& agent_id);
    Result<void> set_cadence(int seconds);                        // hot + persisted
    void force_round();                                           // immediate round if idle

    /// HITL (live mode): approve/reject a queued action by approval id.
    /// Approved actions execute at the venue's CURRENT marks; rejections are
    /// persisted as "rejected_risk" orders with reason "HITL rejected".
    /// Pending approvals persist in the store and re-surface on start().
    void resume_after_hitl(const QString& approval_id, bool approved);

    // Crash recovery: competitions found "running" at init(); UI can resume or dismiss.
    QStringList pending_crash_recoveries() const { return crash_pending_; }
    void dismiss_crash_recovery(const QString& id);               // mark "ended", don't resume

    QString active_competition_id() const { return active_id_; }
    bool is_running() const { return timer_ && timer_->isActive(); }
    int round_seq() const { return round_seq_; }
    int next_round_in_seconds() const;
    bool round_in_flight() const { return round_in_flight_; }

    /// Live account view for the active competition ({} when none / unknown agent).
    AccountView account_view(const QString& agent_id) const;
    /// Current venue marks (mid per coin); empty when no active competition.
    QHash<QString, double> current_marks() const {
        return venue_ ? venue_->marks() : QHash<QString, double>{};
    }

    // Test seams — engine takes ownership; pass BEFORE start().
    void set_market_for_test(IArenaMarketData* m);
    void set_llm_for_test(IArenaLlmClient* c);
    void set_marks_interval_for_test(int ms);   // live-marks cadence (default 10s)

  signals:
    void round_started(int seq);
    void round_completed(int seq);
    void round_failed(int seq, QString error);
    void decision_received(QString decision_id, QString agent_id, QString status);
    void order_executed(QString agent_id, QString order_id, QString status);
    void agent_status_changed(QString agent_id, QString status, QString reason);
    void competition_status_changed(QString competition_id, QString status);
    void crash_recovery_pending(QStringList competition_ids);
    /// Live mode: a risk-approved action awaits human approval (resume_after_hitl).
    void hitl_requested(QString approval_id, QString agent_id, QString summary);
    /// Between-round live marks landed (venue re-marked; equity views are fresh).
    void marks_updated();

  private:
    ArenaEngine();
    void run_round();
    void on_snapshot(quint64 epoch, int seq, Result<MarketSnapshot> r);
    void dispatch_agent(int seq, const AgentRow& agent, const MarketSnapshot& snap, bool is_retry,
                        const QString& prior_raw, const QString& prior_error);
    void finish_agent(int seq, const AgentRow& agent, const ArenaLlmResult& llm,
                      const QString& sys, const QString& user, bool was_retry);
    void agent_done(int seq);
    void settle_round(int seq);
    void record_failure(const AgentRow& agent, const QString& kind);
    bool agent_is_active(const QString& agent_id) const;   // CURRENT status in agents_
    void run_marks_tick();                                  // between-round live marks
    void refresh_marks_blocking();                          // kill paths: bounded-wait marks
                                                            // refresh + sweep → close fresh

    QString active_id_;
    CompetitionRow comp_;
    QVector<AgentRow> agents_;
    QStringList crash_pending_;               // "running" rows found at init(), not yet resumed/dismissed
    quint64 epoch_ = 0;                       // bumped on start()/stop(); stale async callbacks no-op
    QTimer* timer_ = nullptr;
    QTimer* marks_timer_ = nullptr;           // between-round live marks (Feature: realtime marks)
    int marks_interval_ms_ = 10000;
    bool inited_ = false, round_in_flight_ = false;
    int round_seq_ = 0, pending_agents_ = 0;
    qint64 last_mark_ts_ = 0;                 // ms of the PREVIOUS mark_all (round OR marks tick)
                                              // — funding accrues on elapsed-since-last-mark
    MarketSnapshot last_snapshot_;
    QHash<QString, double> round_notional_;   // agent_id → Σ new notional this round
    struct PendingApproval { QString id, agent_id; AgentAction action; int seq = 0; };
    void reject_approval(const PendingApproval& pa, const QString& reason);   // rejected order + signal
    QVector<PendingApproval> pending_hitl_;   // live mode only; mirrors arena_hitl_pending rows
                                              // (survive stop(), reload on start; kill_all rejects)
    std::unique_ptr<PaperExchange> venue_;
    IArenaMarketData* market_ = nullptr;      // owned (parent=this)
    IArenaLlmClient* llm_ = nullptr;          // owned (parent=this)
    RiskLimits limits_;
};

} // namespace fincept::arena
