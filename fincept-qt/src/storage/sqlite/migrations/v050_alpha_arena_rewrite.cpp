// v050_alpha_arena_rewrite — ground-up Alpha Arena rebuild.
//
// Drops every v024 aa_* table (the feature was non-functional: model picker
// never saw the Providers config layer, paper venue had no market data) and
// creates the new arena_* schema. Old competition data is intentionally not
// migrated. Spec: docs/superpowers/specs/2026-06-10-alpha-arena-rewrite-design.md §4.
//
// NOTE: assigned version 50 (not 49 as in the spec draft) because the
// concurrently-developed v049_order_baskets migration already occupies version
// 49 and the register_migration_v049() symbol in this tree.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v050(QSqlDatabase& db) {
    for (const char* stmt : {
        // Drop the old aa_* schema (children before parents). v024 data is not migrated.
        "DROP TABLE IF EXISTS aa_hitl_approvals",
        "DROP TABLE IF EXISTS aa_events",
        "DROP TABLE IF EXISTS aa_pnl_snapshots",
        "DROP TABLE IF EXISTS aa_positions",
        "DROP TABLE IF EXISTS aa_fills",
        "DROP TABLE IF EXISTS aa_orders",
        "DROP TABLE IF EXISTS aa_decisions",
        "DROP TABLE IF EXISTS aa_ticks",
        "DROP TABLE IF EXISTS aa_prompts",
        "DROP TABLE IF EXISTS aa_agents",
        "DROP TABLE IF EXISTS aa_competitions",
    }) {
        auto r = sql(db, stmt);
        if (r.is_err()) return r;
    }
    for (const char* stmt : {
        "CREATE TABLE IF NOT EXISTS arena_competitions ("
        "  id TEXT PRIMARY KEY, name TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'created',"
        "  venue TEXT NOT NULL DEFAULT 'paper', instruments_json TEXT NOT NULL DEFAULT '[]',"
        "  capital_per_agent REAL NOT NULL DEFAULT 10000, cadence_seconds INTEGER NOT NULL DEFAULT 180,"
        "  max_leverage REAL NOT NULL DEFAULT 10, system_prompt_override TEXT NOT NULL DEFAULT '',"
        "  live_mode INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL DEFAULT 0, started_at INTEGER NOT NULL DEFAULT 0,"
        "  ended_at INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS arena_agents ("
        "  id TEXT PRIMARY KEY, competition_id TEXT NOT NULL, provider TEXT NOT NULL,"
        "  model_id TEXT NOT NULL, display_name TEXT NOT NULL, color_hex TEXT NOT NULL DEFAULT '',"
        "  source_kind TEXT NOT NULL DEFAULT 'provider', source_ref TEXT NOT NULL DEFAULT '',"
        "  status TEXT NOT NULL DEFAULT 'active', halt_reason TEXT NOT NULL DEFAULT '',"
        "  consecutive_failures INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS arena_rounds ("
        "  competition_id TEXT NOT NULL, seq INTEGER NOT NULL, started_at INTEGER NOT NULL DEFAULT 0,"
        "  completed_at INTEGER NOT NULL DEFAULT 0, market_snapshot_json TEXT NOT NULL DEFAULT '',"
        "  status TEXT NOT NULL DEFAULT 'running', PRIMARY KEY (competition_id, seq))",
        "CREATE TABLE IF NOT EXISTS arena_decisions ("
        "  id TEXT PRIMARY KEY, competition_id TEXT NOT NULL, agent_id TEXT NOT NULL,"
        "  round_seq INTEGER NOT NULL, system_prompt TEXT NOT NULL DEFAULT '',"
        "  user_prompt TEXT NOT NULL DEFAULT '', raw_response TEXT NOT NULL DEFAULT '',"
        "  actions_json TEXT NOT NULL DEFAULT '[]', parse_error TEXT NOT NULL DEFAULT '',"
        "  status TEXT NOT NULL DEFAULT 'ok', latency_ms INTEGER NOT NULL DEFAULT 0,"
        "  prompt_tokens INTEGER NOT NULL DEFAULT 0, completion_tokens INTEGER NOT NULL DEFAULT 0,"
        "  ts INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS arena_orders ("
        "  id TEXT PRIMARY KEY, competition_id TEXT NOT NULL, agent_id TEXT NOT NULL,"
        "  round_seq INTEGER NOT NULL, coin TEXT NOT NULL, side TEXT NOT NULL DEFAULT '',"
        "  action TEXT NOT NULL, qty REAL NOT NULL DEFAULT 0, price REAL NOT NULL DEFAULT 0,"
        "  notional_usd REAL NOT NULL DEFAULT 0, leverage REAL NOT NULL DEFAULT 1,"
        "  fee REAL NOT NULL DEFAULT 0, realized_pnl REAL NOT NULL DEFAULT 0,"
        "  status TEXT NOT NULL DEFAULT 'filled', reject_reason TEXT NOT NULL DEFAULT '',"
        "  ts INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS arena_positions ("
        "  competition_id TEXT NOT NULL, agent_id TEXT NOT NULL, coin TEXT NOT NULL,"
        "  side TEXT NOT NULL, qty REAL NOT NULL DEFAULT 0, entry_price REAL NOT NULL DEFAULT 0,"
        "  leverage REAL NOT NULL DEFAULT 1, funding_accrued REAL NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (competition_id, agent_id, coin))",
        "CREATE TABLE IF NOT EXISTS arena_accounts ("
        "  competition_id TEXT NOT NULL, agent_id TEXT NOT NULL, balance REAL NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (competition_id, agent_id))",
        "CREATE TABLE IF NOT EXISTS arena_equity_snapshots ("
        "  competition_id TEXT NOT NULL, agent_id TEXT NOT NULL, round_seq INTEGER NOT NULL,"
        "  ts INTEGER NOT NULL DEFAULT 0, equity REAL NOT NULL DEFAULT 0,"
        "  balance REAL NOT NULL DEFAULT 0, unrealized_pnl REAL NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (competition_id, agent_id, round_seq))",
        "CREATE TABLE IF NOT EXISTS arena_events ("
        "  seq INTEGER PRIMARY KEY AUTOINCREMENT, competition_id TEXT NOT NULL DEFAULT '',"
        "  agent_id TEXT NOT NULL DEFAULT '', type TEXT NOT NULL, payload_json TEXT NOT NULL DEFAULT '{}',"
        "  ts INTEGER NOT NULL DEFAULT 0)",
        "CREATE INDEX IF NOT EXISTS idx_arena_agents_comp     ON arena_agents(competition_id)",
        "CREATE INDEX IF NOT EXISTS idx_arena_decisions_comp  ON arena_decisions(competition_id, agent_id, round_seq)",
        "CREATE INDEX IF NOT EXISTS idx_arena_orders_comp     ON arena_orders(competition_id, agent_id, round_seq)",
        "CREATE INDEX IF NOT EXISTS idx_arena_equity_comp     ON arena_equity_snapshots(competition_id, round_seq)",
        "CREATE INDEX IF NOT EXISTS idx_arena_events_comp     ON arena_events(competition_id, seq)",
    }) {
        auto r = sql(db, stmt);
        if (r.is_err()) return r;
    }
    return Result<void>::ok();
}

} // namespace

void register_migration_v050() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({50, "alpha_arena_rewrite", apply_v050});
}

} // namespace fincept
