// v024_alpha_arena — Alpha Arena production schema.
//
// Full reference: fincept-qt/.grill-me/alpha-arena-production-refactor.md §Phase 4.
//
// All tables are prefixed `aa_` so they coexist cleanly with the dozen other
// feature surfaces in the unified DB. Composite (competition_id, seq) keys
// drive replayability — given a competition + a tick seq we can reconstruct
// the exact prompt, market snapshot and per-agent response.
//
// `aa_prompts` is content-addressed by sha256 so identical prompts (and they
// often are — non-situational mode produces byte-equal prompts across all N
// agents in a tick) only store one row.
//
// `aa_events` is append-only with a monotonic seq. Use it for the AUDIT tab
// and crash recovery.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v024_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v024(QSqlDatabase& db) {
    // ── Competitions ────────────────────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_competitions (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                mode TEXT NOT NULL DEFAULT 'baseline',         -- baseline|monk|situational
                venue TEXT NOT NULL DEFAULT 'paper',           -- paper|hyperliquid
                instruments_json TEXT NOT NULL DEFAULT '[]',   -- ["BTC","ETH",...]
                initial_capital REAL NOT NULL DEFAULT 10000,
                cadence_seconds INTEGER NOT NULL DEFAULT 180,
                live_mode INTEGER NOT NULL DEFAULT 0,          -- 0=paper, 1=live (one-way)
                live_mode_acked_at TEXT DEFAULT NULL,          -- ISO ts of typed ack (live only)
                live_mode_acked_host TEXT DEFAULT '',          -- hostname recorded at ack
                status TEXT NOT NULL DEFAULT 'created',        -- created|running|halted_by_user|halted_by_crash|completed|failed
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                started_at TEXT DEFAULT NULL,
                ended_at TEXT DEFAULT NULL,
                cycle_count INTEGER NOT NULL DEFAULT 0
            )
        )sql");
        if (r.is_err()) return r;
    }

    // ── Agents (one row per agent slot in a competition) ────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_agents (
                id TEXT PRIMARY KEY,
                competition_id TEXT NOT NULL,
                slot INTEGER NOT NULL,
                provider TEXT NOT NULL,                        -- openai|anthropic|google|deepseek|xai|qwen|...
                model_id TEXT NOT NULL,
                display_name TEXT NOT NULL,
                color_hex TEXT NOT NULL DEFAULT '#888888',
                api_secret_handle TEXT DEFAULT '',             -- opaque handle into SecureStorage
                state TEXT NOT NULL DEFAULT 'active',          -- active|paused|circuit_open|halted
                consecutive_parse_failures INTEGER NOT NULL DEFAULT 0,
                consecutive_risk_rejects INTEGER NOT NULL DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(competition_id, slot)
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_agents_comp "
                              "ON aa_agents(competition_id)");
        if (r.is_err()) return r;
    }

    // ── Prompts (content-addressed) ─────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_prompts (
                sha256 TEXT PRIMARY KEY,
                length INTEGER NOT NULL,
                text TEXT NOT NULL,
                first_seen_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err()) return r;
    }

    // ── Ticks ───────────────────────────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_ticks (
                id TEXT PRIMARY KEY,
                competition_id TEXT NOT NULL,
                seq INTEGER NOT NULL,
                utc_ms INTEGER NOT NULL,
                system_prompt_sha256 TEXT DEFAULT '',
                market_snapshot_json TEXT NOT NULL DEFAULT '{}',
                status TEXT NOT NULL DEFAULT 'open',           -- open|closed|skipped
                closed_at TEXT DEFAULT NULL,
                UNIQUE(competition_id, seq)
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_ticks_comp_seq "
                              "ON aa_ticks(competition_id, seq DESC)");
        if (r.is_err()) return r;
    }

    // ── Decisions (one row per agent per tick) ──────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_decisions (
                id TEXT PRIMARY KEY,
                tick_id TEXT NOT NULL,
                agent_id TEXT NOT NULL,
                user_prompt_sha256 TEXT NOT NULL,
                raw_response TEXT NOT NULL DEFAULT '',
                parsed_actions_json TEXT NOT NULL DEFAULT '[]',
                parse_error TEXT DEFAULT '',
                risk_verdict_json TEXT NOT NULL DEFAULT '[]',  -- one verdict per parsed action
                latency_ms INTEGER DEFAULT 0,
                tokens_in INTEGER DEFAULT 0,
                tokens_out INTEGER DEFAULT 0,
                cost_usd REAL DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(tick_id, agent_id)
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_decisions_agent "
                              "ON aa_decisions(agent_id, created_at DESC)");
        if (r.is_err()) return r;
    }

    // ── Orders ──────────────────────────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_orders (
                id TEXT PRIMARY KEY,
                decision_id TEXT NOT NULL,
                agent_id TEXT NOT NULL,
                competition_id TEXT NOT NULL,
                coin TEXT NOT NULL,
                side TEXT NOT NULL,                            -- buy|sell
                qty REAL NOT NULL,
                leverage INTEGER NOT NULL DEFAULT 1,
                type TEXT NOT NULL DEFAULT 'market',           -- market|limit
                tif TEXT NOT NULL DEFAULT 'ioc',
                price REAL DEFAULT NULL,
                status TEXT NOT NULL DEFAULT 'pending',        -- pending|sent|filled|partial|rejected|cancelled
                venue_order_id TEXT DEFAULT '',
                error TEXT DEFAULT '',
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                resolved_at TEXT DEFAULT NULL
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_orders_agent "
                              "ON aa_orders(agent_id, created_at DESC)");
        if (r.is_err()) return r;
    }

    // ── Fills ───────────────────────────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_fills (
                id TEXT PRIMARY KEY,
                order_id TEXT NOT NULL,
                qty REAL NOT NULL,
                price REAL NOT NULL,
                fee REAL NOT NULL DEFAULT 0,
                ts TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_fills_order "
                              "ON aa_fills(order_id)");
        if (r.is_err()) return r;
    }

    // ── Positions (one open row per (agent_id, coin); closed positions kept) ─
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_positions (
                id TEXT PRIMARY KEY,
                agent_id TEXT NOT NULL,
                competition_id TEXT NOT NULL,
                coin TEXT NOT NULL,
                qty REAL NOT NULL,                              -- signed
                entry REAL NOT NULL,
                mark REAL NOT NULL DEFAULT 0,
                liq_price REAL NOT NULL DEFAULT 0,
                leverage INTEGER NOT NULL DEFAULT 1,
                profit_target REAL NOT NULL DEFAULT 0,
                stop_loss REAL NOT NULL DEFAULT 0,
                invalidation_condition TEXT NOT NULL DEFAULT '',
                opened_at TEXT DEFAULT CURRENT_TIMESTAMP,
                closed_at TEXT DEFAULT NULL,
                close_reason TEXT DEFAULT ''                    -- model_close|stop|target|liquidation|halt
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_aa_positions_open "
                              "ON aa_positions(agent_id, coin) WHERE closed_at IS NULL");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_positions_agent "
                              "ON aa_positions(agent_id, opened_at DESC)");
        if (r.is_err()) return r;
    }

    // ── PnL snapshots (one row per agent per tick) ──────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_pnl_snapshots (
                id TEXT PRIMARY KEY,
                tick_id TEXT NOT NULL,
                agent_id TEXT NOT NULL,
                equity REAL NOT NULL,
                cash REAL NOT NULL,
                return_pct REAL NOT NULL DEFAULT 0,
                sharpe REAL NOT NULL DEFAULT 0,
                max_drawdown REAL NOT NULL DEFAULT 0,
                fees_paid REAL NOT NULL DEFAULT 0,
                win_rate REAL NOT NULL DEFAULT 0,
                trades_count INTEGER NOT NULL DEFAULT 0,
                avg_leverage REAL NOT NULL DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(tick_id, agent_id)
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_pnl_agent "
                              "ON aa_pnl_snapshots(agent_id, created_at DESC)");
        if (r.is_err()) return r;
    }

    // ── Events (append-only audit) ──────────────────────────────────────────
    // Strict monotonic seq via INTEGER PRIMARY KEY AUTOINCREMENT.
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_events (
                seq INTEGER PRIMARY KEY AUTOINCREMENT,
                competition_id TEXT NOT NULL,
                agent_id TEXT DEFAULT NULL,
                type TEXT NOT NULL,
                payload_json TEXT NOT NULL DEFAULT '{}',
                ts TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_events_comp "
                              "ON aa_events(competition_id, seq)");
        if (r.is_err()) return r;
    }

    // ── HITL approvals ──────────────────────────────────────────────────────
    {
        auto r = v024_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS aa_hitl_approvals (
                id TEXT PRIMARY KEY,
                decision_id TEXT NOT NULL,
                agent_id TEXT NOT NULL,
                action_json TEXT NOT NULL,
                prompt_text TEXT NOT NULL DEFAULT '',
                status TEXT NOT NULL DEFAULT 'pending',          -- pending|approved|rejected|timeout
                requested_at TEXT DEFAULT CURRENT_TIMESTAMP,
                resolved_at TEXT DEFAULT NULL,
                resolved_by TEXT DEFAULT ''
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v024_sql(db, "CREATE INDEX IF NOT EXISTS idx_aa_hitl_status "
                              "ON aa_hitl_approvals(status, requested_at)");
        if (r.is_err()) return r;
    }

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v024() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({24, "alpha_arena", apply_v024});
}

} // namespace fincept
