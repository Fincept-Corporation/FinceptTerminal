#pragma once
// OrderRouter — runs RiskEngine on each ProposedAction and routes accepted
// (or amended) actions to the venue. Persists every step of the journey to
// the repo: risk verdict, order row, fill row, position row.
//
// HITL gate is implemented inline here (rather than its own QObject) because
// the only state it carries is the pending-approval queue, which is also
// persisted in `aa_hitl_approvals`. The C++-side queue is just a fast path
// for the UI; the DB row is the source of truth.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §6 (Risk engine), §7
// (Order router), §6.6 (HITL).

#include "services/alpha_arena/AlphaArenaRepo.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "services/alpha_arena/IExchangeVenue.h"
#include "services/alpha_arena/RiskEngine.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace fincept::services::alpha_arena {

/// Per-agent state the router keeps in memory. Persisted derivatives are in
/// aa_agents / aa_positions; this is the hot copy used by RiskEngine on the
/// next decision.
struct AgentRuntimeState {
    QString agent_id;
    QString competition_id;
    double equity = 0.0;
    QHash<QString /*coin*/, double /*signed qty*/> open_positions;
    QHash<QString /*coin*/, qint64 /*utc_ms*/> last_entry_per_coin;
    CircuitState circuit;
    bool live_mode = false;
};

class OrderRouter : public QObject {
    Q_OBJECT
  public:
    explicit OrderRouter(QObject* parent = nullptr);

    /// Configure the router for the active competition. Must be called
    /// before any decision_received signal can be processed.
    void configure(const QString& competition_id,
                   CompetitionMode mode,
                   IExchangeVenue* venue);

    /// Engine pushes the latest agent state on each tick (after building the
    /// account view from the venue + repo). Replaces internal state.
    void update_agent_state(const QString& agent_id, const AgentRuntimeState& state);

    /// HITL approvals pending in the C++ runtime. UI subscribes to this.
    int pending_hitl_count() const { return pending_hitl_.size(); }

  public slots:
    /// Called by ModelDispatcher when an agent's tick completes.
    /// `actions` may be empty (parse error, hold-only, etc.).
    void on_decision(QString decision_id, QString agent_id,
                     QVector<fincept::services::alpha_arena::ProposedAction> actions,
                     QString parse_error);

    /// User resolves a HITL approval from the UI.
    void resolve_hitl(QString approval_id, bool approved);

  signals:
    /// One verdict per action emitted; carries enough info for the UI to
    /// render the RISK and AUDIT panels live.
    void action_evaluated(QString agent_id, QString coin,
                          fincept::services::alpha_arena::Signal signal,
                          int risk_outcome,           // RiskVerdict::Outcome cast to int
                          QString reason);

    /// Order placed (after risk + HITL). The fill arrives via the venue.
    void order_placed(QString agent_id, QString order_id);

    /// Per-agent kill-switch tripped. Engine should halt the agent on receipt.
    void agent_circuit_open(QString agent_id, QString reason);

    /// HITL approval requested; UI surfaces a modal/tray notification.
    void hitl_requested(QString approval_id, QString agent_id, QString summary);

  private:
    /// Decide whether an action needs HITL approval.
    /// Rule (live mode only): notional ≥ $1000 OR leverage ≥ 10×.
    bool needs_hitl(const ProposedAction& a, const AgentRuntimeState& agent) const;

    /// Place an order with the venue, persist the order row, wire fill cb.
    void route_to_venue(const QString& decision_id,
                        const ProposedAction& a,
                        const AgentRuntimeState& agent);

    QString competition_id_;
    CompetitionMode mode_ = CompetitionMode::Baseline;
    IExchangeVenue* venue_ = nullptr;     // not owned — Engine owns

    QHash<QString /*agent_id*/, AgentRuntimeState> agents_;

    /// Pending HITL approvals: approval_id -> (decision_id, action, agent_id).
    /// Resolved by user via resolve_hitl().
    struct PendingHitl {
        QString decision_id;
        ProposedAction action;
        QString agent_id;
    };
    QHash<QString, PendingHitl> pending_hitl_;
};

} // namespace fincept::services::alpha_arena
