#pragma once
// ModelDispatcher — turns a Tick into per-agent decisions.
//
// On every tick:
//   1. Build the system prompt (cached, mode-dependent only) and per-agent
//      user prompts (canonical, byte-equal across agents in non-situational
//      mode).
//   2. Persist tick metadata + content-addressed prompt rows.
//   3. For each agent, write a request file (path-only argv; no secret) and
//      spawn `scripts/alpha_arena/llm_call.py` via PythonRunner. The
//      subprocess redeems the API key from the SecretRedemptionServer.
//   4. Parse the response into ProposedActions, persist the decision row,
//      and emit `decision_received(decision_id, agent_id, actions)` for the
//      OrderRouter to consume. Parse failures land as a no-op decision row
//      with parse_error set (per Nof1 spec — no retry).
//
// The dispatcher does NOT call the venue. It is a pure prompt-and-parse
// stage. OrderRouter is downstream.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §4 (Context builder)
// and §12 (Backend topology).

#include "services/alpha_arena/AlphaArenaRepo.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "services/alpha_arena/SecretRedemptionServer.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <optional>

namespace fincept::services::alpha_arena {

struct DispatchAgent {
    QString agent_id;
    QString provider;       // openai|anthropic|google|deepseek|xai|qwen
    QString model_id;
    QString base_url;       // optional override
    QString api_secret_handle;   // SecureStorage key
    QString display_name;
};

class ModelDispatcher : public QObject {
    Q_OBJECT
  public:
    explicit ModelDispatcher(QObject* parent = nullptr);

    /// Configure dispatch for the active competition. Replaces any prior
    /// state. Pass an empty list to halt dispatch.
    void configure(const QString& competition_id,
                   CompetitionMode mode,
                   const QVector<DispatchAgent>& agents);

  public slots:
    /// Triggered by AlphaArenaEngine on TickClock::tick. Per-agent state
    /// (positions, equity, etc.) and the market snapshot are supplied by
    /// the engine, which has read them from the repo / venue immediately
    /// before this call.
    void dispatch_tick(fincept::services::alpha_arena::Tick t,
                       fincept::services::alpha_arena::TickContext ctx,
                       QHash<QString, fincept::services::alpha_arena::AgentAccountState> per_agent_state,
                       std::optional<fincept::services::alpha_arena::SituationalContext> situational);

  signals:
    /// Emitted once per agent per tick after the response (or timeout/parse
    /// error) is persisted. `decision_id` is the aa_decisions row id; the
    /// row carries parsed_actions_json and parse_error.
    void decision_received(QString decision_id, QString agent_id,
                           QVector<fincept::services::alpha_arena::ProposedAction> actions,
                           QString parse_error);

    /// All agents have responded (or timed out) for this tick. The engine
    /// listens for this to call TickClock::tick_complete().
    void tick_dispatch_complete(int seq);

  private:
    void start_secret_server_for_tick(int seq);
    void close_secret_server_when_done();

    QString competition_id_;
    CompetitionMode mode_ = CompetitionMode::Baseline;
    QVector<DispatchAgent> agents_;

    SecretRedemptionServer* secrets_ = nullptr;

    /// Per-tick state — number of outstanding agent calls. When it hits 0,
    /// we close the SecretRedemptionServer and emit tick_dispatch_complete.
    QHash<int /*seq*/, int> outstanding_;
    QHash<int /*seq*/, QString /*tick_id*/> tick_ids_;
};

} // namespace fincept::services::alpha_arena
