#pragma once
// ArenaStore — all SQLite access for the Alpha Arena rewrite (arena_* tables).

#include "core/result/Result.h"
#include "services/alpha_arena/ArenaTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept::arena {

class ArenaStore : public BaseRepository<CompetitionRow> {
  public:
    static ArenaStore& instance();

    // Competitions
    Result<QString> insert_competition(const ArenaConfig& cfg);           // returns new id
    Result<CompetitionRow> competition(const QString& id);
    Result<QVector<CompetitionRow>> list_competitions();                  // newest first
    Result<void> set_competition_status(const QString& id, const QString& status);
    Result<void> set_competition_timestamps(const QString& id, qint64 started_at, qint64 ended_at);
    Result<void> set_cadence(const QString& id, int seconds);
    Result<void> delete_competition_cascade(const QString& id);           // all 9 tables
    Result<QStringList> competitions_with_status(const QString& status);

    // Agents
    Result<QString> insert_agent(const QString& competition_id, const ArenaAgentSpec& spec);
    Result<QVector<AgentRow>> agents_for(const QString& competition_id);
    Result<void> set_agent_status(const QString& agent_id, const QString& status, const QString& reason);
    Result<void> set_agent_failures(const QString& agent_id, int count);

    // Rounds / decisions / orders
    Result<void> insert_round(const QString& competition_id, int seq, qint64 started_at, const QString& snapshot_json);
    Result<void> complete_round(const QString& competition_id, int seq, qint64 completed_at, const QString& status);
    Result<int> last_round_seq(const QString& competition_id);
    Result<QString> insert_decision(DecisionRow d);                       // fills id+ts, returns id
    Result<QVector<DecisionRow>> recent_decisions(const QString& competition_id, const QString& agent_id, int limit);
    /// Newest decisions across all agents (for the MODEL CHAT feed). agent_id empty = all.
    Result<QVector<DecisionRow>> latest_decisions(const QString& competition_id,
                                                  const QString& agent_id, int limit);
    /// Aggregate token spend per agent: agent_id → (prompt+completion) tokens.
    Result<QHash<QString, qint64>> token_totals(const QString& competition_id);
    Result<QVector<DecisionRow>> decisions_for_round(const QString& competition_id, int seq);
    Result<QString> insert_order(OrderRow o);                             // fills id+ts, returns id
    Result<QVector<OrderRow>> orders_for(const QString& competition_id, const QString& agent_id /*empty=all*/, int limit);

    // Positions / accounts / equity
    Result<void> upsert_position(const PositionRow& p);
    Result<void> delete_position(const QString& competition_id, const QString& agent_id, const QString& coin);
    Result<QVector<PositionRow>> positions_for(const QString& competition_id, const QString& agent_id /*empty=all*/);
    Result<void> upsert_account(const QString& competition_id, const QString& agent_id, double balance);
    Result<double> account_balance(const QString& competition_id, const QString& agent_id);
    Result<void> insert_equity_snapshot(const EquitySnapshotRow& s);
    Result<QVector<EquitySnapshotRow>> equity_series(const QString& competition_id);

    // Events
    Result<void> insert_event(const QString& competition_id, const QString& agent_id,
                              const QString& type, const QString& payload_json);
    Result<QVector<QString>> recent_events(const QString& competition_id, int limit); // "ts|agent|type|payload"
};

} // namespace fincept::arena
