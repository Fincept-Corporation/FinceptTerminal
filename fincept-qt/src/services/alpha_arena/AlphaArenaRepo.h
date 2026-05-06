#pragma once
// AlphaArenaRepo — single facade over the aa_* tables.
//
// Unlike most fincept repos, this one is a *facade* — one class with methods
// for every aa_* table — rather than one class per table. The Alpha Arena
// engine writes coherent groups (tick + N decisions + M orders + fills +
// position deltas + pnl snapshots) within tight time windows, and a single
// facade keeps the call graph readable and groups indexes by purpose.
//
// All write methods return Result<QString> with the inserted id (TEXT PK)
// for new rows, or Result<void> for updates.
//
// Reference: fincept-qt/.grill-me/alpha-arena-production-refactor.md §Phase 4
// and the v024_alpha_arena migration for column definitions.

#include "core/result/Result.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::alpha_arena {

// ─── Read-side row structs ─────────────────────────────────────────────────

struct CompetitionRow {
    QString id;
    QString name;
    CompetitionMode mode = CompetitionMode::Baseline;
    QString venue;                // "paper" | "hyperliquid"
    QStringList instruments;
    double initial_capital = 10000.0;
    int cadence_seconds = 180;
    bool live_mode = false;
    QString status;               // see migration enum comment
    int cycle_count = 0;
    QString created_at;
    QString started_at;
    QString ended_at;
};

struct AgentRow {
    QString id;
    QString competition_id;
    int slot = 0;
    QString provider;
    QString model_id;
    QString display_name;
    QString color_hex;
    QString api_secret_handle;
    QString state;                // active|paused|circuit_open|halted
    int consecutive_parse_failures = 0;
    int consecutive_risk_rejects = 0;
};

struct LeaderboardRow {
    QString agent_id;
    QString display_name;
    QString color_hex;
    int rank = 0;
    double equity = 0.0;
    double return_pct = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
    double fees_paid = 0.0;
    double avg_leverage = 0.0;
    int trades_count = 0;
    double win_rate = 0.0;
};

struct ModelChatRow {
    QString decision_id;
    QString agent_id;
    QString tick_id;
    int tick_seq = 0;
    qint64 tick_utc_ms = 0;
    QString user_prompt_sha256;
    QString raw_response;
    QString parsed_actions_json;
    QString parse_error;
    QString risk_verdict_json;
    int latency_ms = 0;
    int tokens_in = 0;
    int tokens_out = 0;
    double cost_usd = 0.0;
};

struct EventRow {
    qint64 seq = 0;
    QString competition_id;
    QString agent_id;
    QString type;
    QString payload_json;
    QString ts;
};

// ─── Repo facade ────────────────────────────────────────────────────────────

class AlphaArenaRepo {
  public:
    static AlphaArenaRepo& instance();

    // ── Competitions ────────────────────────────────────────────────────────
    /// Persist a new competition. Caller supplies the id (Qt QUuid).
    /// `instruments` is the perp universe for this run (subset of kPerpUniverse()).
    Result<void> insert_competition(const CompetitionRow& comp);
    Result<void> update_competition_status(const QString& id, const QString& status);
    Result<void> mark_live_mode_ack(const QString& id, const QString& host);
    Result<void> increment_cycle_count(const QString& id);
    std::optional<CompetitionRow> find_competition(const QString& id);
    Result<QVector<CompetitionRow>> list_competitions(int limit);
    /// Returns competitions that were running when the app was last killed.
    Result<QVector<CompetitionRow>> find_orphaned_running();

    // ── Agents ─────────────────────────────────────────────────────────────
    Result<void> insert_agent(const AgentRow& agent);
    Result<void> update_agent_state(const QString& id, const QString& state);
    Result<void> bump_parse_failures(const QString& agent_id, int new_value);
    Result<void> bump_risk_rejects(const QString& agent_id, int new_value);
    Result<QVector<AgentRow>> agents_for(const QString& competition_id);
    std::optional<AgentRow> find_agent(const QString& id);

    // ── Prompts (content-addressed) ────────────────────────────────────────
    /// INSERT OR IGNORE — returns ok regardless. Pass the canonical sha256 of
    /// the prompt text in lowercase hex (caller computes it).
    Result<void> upsert_prompt(const QString& sha256, const QString& text);

    // ── Ticks ───────────────────────────────────────────────────────────────
    /// Returns the inserted tick id (TEXT PK).
    Result<QString> insert_tick(const QString& competition_id,
                                int seq,
                                qint64 utc_ms,
                                const QString& system_prompt_sha256,
                                const QString& market_snapshot_json);
    Result<void> close_tick(const QString& tick_id);
    Result<void> mark_tick_skipped(const QString& tick_id);

    // ── Decisions ───────────────────────────────────────────────────────────
    struct DecisionInsert {
        QString tick_id;
        QString agent_id;
        QString user_prompt_sha256;
        QString raw_response;
        QString parsed_actions_json;
        QString parse_error;
        QString risk_verdict_json;
        int latency_ms = 0;
        int tokens_in = 0;
        int tokens_out = 0;
        double cost_usd = 0.0;
    };
    Result<QString> insert_decision(const DecisionInsert& d);
    Result<QVector<ModelChatRow>> modelchat_for(const QString& competition_id,
                                                const QString& agent_id,
                                                int limit,
                                                int offset);

    // ── Orders ──────────────────────────────────────────────────────────────
    struct OrderInsert {
        QString decision_id;
        QString agent_id;
        QString competition_id;
        QString coin;
        QString side;
        double qty = 0.0;
        int leverage = 1;
        QString type = QStringLiteral("market");
        QString tif = QStringLiteral("ioc");
        std::optional<double> price;
    };
    Result<QString> insert_order(const OrderInsert& o);
    Result<void> mark_order_status(const QString& order_id,
                                   const QString& status,
                                   const QString& venue_order_id,
                                   const QString& error);

    // ── Fills ───────────────────────────────────────────────────────────────
    Result<QString> insert_fill(const QString& order_id, double qty, double price, double fee);

    // ── Positions ──────────────────────────────────────────────────────────
    Result<QString> open_position(const QString& agent_id,
                                  const QString& competition_id,
                                  const Position& p);
    Result<void> update_position_marks(const QString& position_id, double mark, double liq_price);
    Result<void> close_position(const QString& position_id, const QString& reason);
    Result<QVector<Position>> open_positions_for(const QString& agent_id);

    // ── PnL snapshots ──────────────────────────────────────────────────────
    Result<void> insert_pnl_snapshot(const QString& tick_id,
                                     const QString& agent_id,
                                     const AgentAccountState& s,
                                     double avg_leverage);
    Result<QVector<LeaderboardRow>> leaderboard(const QString& competition_id);

    // ── Events ─────────────────────────────────────────────────────────────
    /// Append-only audit. agent_id may be empty for competition-level events.
    Result<void> append_event(const QString& competition_id,
                              const QString& agent_id,
                              const QString& type,
                              const QJsonObject& payload);
    Result<QVector<EventRow>> events_since(const QString& competition_id, qint64 since_seq, int limit);

    // ── HITL approvals ──────────────────────────────────────────────────────
    Result<QString> request_hitl(const QString& decision_id,
                                 const QString& agent_id,
                                 const QJsonObject& action,
                                 const QString& prompt_text);
    Result<void> resolve_hitl(const QString& approval_id,
                              const QString& status,    // approved|rejected|timeout
                              const QString& resolved_by);

  private:
    AlphaArenaRepo() = default;
};

} // namespace fincept::services::alpha_arena
