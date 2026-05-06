#pragma once
// AlphaArenaEngine — singleton owning the live competition runtime.
//
// Constructed once at app startup (like ExchangeService). Owns:
//   * TickClock              (cadence)
//   * ModelDispatcher        (context build + LLM fan-out + persistence)
//   * OrderRouter            (RiskEngine + HITL + venue routing)
//   * IExchangeVenue         (PaperVenue or HyperliquidVenue)
//
// v1 supports a single active competition. The engine continues running
// even when the AlphaArenaScreen is hidden — competitions are live state,
// not screen-bound state. The screen is a viewer.
//
// Reference: fincept-qt/.grill-me/alpha-arena-production-refactor.md §Phase 6.

#include "core/result/Result.h"
#include "services/alpha_arena/AlphaArenaRepo.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "services/alpha_arena/IExchangeVenue.h"
#include "services/alpha_arena/ModelDispatcher.h"
#include "services/alpha_arena/OrderRouter.h"
#include "services/alpha_arena/PaperVenue.h"
#include "services/alpha_arena/TickClock.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::alpha_arena {

/// One agent slot the user wants to enroll. The engine converts this into
/// an AgentRow + a SecureStorage entry for the API key.
struct AgentSpec {
    QString provider;       // openai|anthropic|google|deepseek|xai|qwen
    QString model_id;
    QString display_name;
    QString color_hex;
    QString api_key;        // engine stores this in SecureStorage; field is wiped after.
    QString base_url;       // optional override
};

/// User-facing config for create_competition().
struct NewCompetitionConfig {
    QString name;
    CompetitionMode mode = CompetitionMode::Baseline;
    QString venue_kind = QStringLiteral("paper");   // "paper" | "hyperliquid"
    QStringList instruments;                        // subset of kPerpUniverse(); empty = all
    double initial_capital_per_agent = 10000.0;
    int cadence_seconds = 180;
    QVector<AgentSpec> agents;
    bool live_mode = false;                         // gated by UI; engine just records
};

class AlphaArenaEngine : public QObject {
    Q_OBJECT
  public:
    static AlphaArenaEngine& instance();

    /// One-time init at app startup. Idempotent. Safe to call before the
    /// user has navigated to the Alpha Arena screen.
    void init();

    // ── User-facing actions ────────────────────────────────────────────────

    /// Create a competition (status = "created"; not yet started).
    Result<QString> create_competition(NewCompetitionConfig cfg);

    /// Start ticking (status = "running").
    Result<void> start(const QString& competition_id);

    /// Stop ticking (status = "halted_by_user"); does NOT close positions.
    /// Use kill_all() for the panic-button semantics.
    Result<void> stop();

    /// Stop + close every agent's open positions (market IOC).
    Result<void> kill_all();

    /// Pause an individual agent. No further actions routed; existing
    /// positions stay open.
    Result<void> halt_agent(const QString& agent_id);

    /// Resume after circuit_open or manual halt.
    Result<void> resume_agent(const QString& agent_id);

    /// User UI -> approve/reject a pending HITL request.
    void resume_after_hitl(const QString& approval_id, bool approved);

    // ── Status / introspection ────────────────────────────────────────────

    QString active_competition_id() const { return active_competition_id_; }
    bool is_running() const;
    int last_tick_seq() const { return clock_ ? clock_->last_seq() : 0; }
    int next_tick_in_seconds() const;
    QString venue_kind() const { return venue_ ? venue_->venue_kind() : QString(); }
    IExchangeVenue::ConnectionState venue_connection_state() const;

  signals:
    void tick_fired(int seq);
    void tick_skipped(int seq);
    void decision_received(QString decision_id, QString agent_id);
    void action_evaluated(QString agent_id, QString coin, int signal,
                          int risk_outcome, QString reason);
    void order_placed(QString agent_id, QString order_id);
    void hitl_requested(QString approval_id, QString agent_id, QString summary);
    void agent_circuit_open(QString agent_id, QString reason);
    void competition_status_changed(QString competition_id, QString status);
    /// Emitted after init() finds competitions in `running` status. UI must
    /// surface a Resume / Halt / Reconcile prompt to the user.
    void crash_recovery_pending(QStringList competition_ids);

  private slots:
    void on_tick(fincept::services::alpha_arena::Tick t);
    void on_tick_skipped(int seq);
    void on_dispatch_complete(int seq);
    void on_decision_received(QString decision_id, QString agent_id,
                              QVector<fincept::services::alpha_arena::ProposedAction> actions,
                              QString parse_error);
    void on_circuit_open(QString agent_id, QString reason);

  private:
    AlphaArenaEngine();
    ~AlphaArenaEngine() override;
    AlphaArenaEngine(const AlphaArenaEngine&) = delete;
    AlphaArenaEngine& operator=(const AlphaArenaEngine&) = delete;

    /// Snapshot market + per-agent state for the next tick. Reads from venue
    /// for marks and from repo for positions/equity.
    void prepare_tick_inputs(const Tick& t,
                             TickContext& ctx_out,
                             QHash<QString, AgentAccountState>& per_agent_out,
                             std::optional<SituationalContext>& situational_out);

    /// Detach the active competition (clears pointers, state).
    void detach_active();

    /// Pick a venue impl. v1: paper for venue_kind=="paper"; "hyperliquid"
    /// returns nullptr until Phase 5c lands.
    IExchangeVenue* select_venue_for_kind(const QString& venue_kind);

    static QString deterministic_agent_id(const QString& competition_id, int slot);

    bool inited_ = false;
    QString active_competition_id_;
    CompetitionMode active_mode_ = CompetitionMode::Baseline;

    TickClock* clock_ = nullptr;
    ModelDispatcher* dispatcher_ = nullptr;
    OrderRouter* router_ = nullptr;

    PaperVenue* paper_venue_ = nullptr;
    IExchangeVenue* venue_ = nullptr;  // alias to active venue
};

} // namespace fincept::services::alpha_arena
