#pragma once
// Domain types for the Alpha Arena rewrite. Namespace fincept::arena.
// Spec: docs/superpowers/specs/2026-06-10-alpha-arena-rewrite-design.md

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::arena {

struct ArenaAgentSpec {
    QString provider, model_id, display_name, color_hex;
    QString source_kind;  // "provider" | "model" | "profile"
    QString source_ref;   // row id in the source table ("" for provider — provider name is the key)
};

struct ArenaConfig {
    QString name;
    QStringList instruments;             // e.g. {"BTC","ETH"}
    double capital_per_agent = 10000.0;
    int cadence_seconds = 180;
    double max_leverage = 10.0;
    QString system_prompt_override;      // empty = default prompt
    QString venue = QStringLiteral("paper");  // "paper" | "hyperliquid"
    bool live_mode = false;
    QVector<ArenaAgentSpec> agents;
};

// ── DB row mirrors (1:1 with arena_* tables) ────────────────────────────────
struct CompetitionRow {
    QString id, name, status, venue, system_prompt_override;
    QStringList instruments;
    double capital_per_agent = 0, max_leverage = 10;
    int cadence_seconds = 180;
    bool live_mode = false;
    qint64 created_at = 0, started_at = 0, ended_at = 0;
};
struct AgentRow {
    QString id, competition_id, provider, model_id, display_name, color_hex,
            source_kind, source_ref, status, halt_reason;
    int consecutive_failures = 0;
};
struct DecisionRow {
    QString id, competition_id, agent_id, system_prompt, user_prompt, raw_response,
            actions_json, parse_error, status;
    int round_seq = 0, prompt_tokens = 0, completion_tokens = 0;
    qint64 latency_ms = 0, ts = 0;
};
struct OrderRow {
    QString id, competition_id, agent_id, coin, side, action, status, reject_reason;
    int round_seq = 0;
    double qty = 0, price = 0, notional_usd = 0, leverage = 1, fee = 0, realized_pnl = 0;
    qint64 ts = 0;
};
struct PositionRow {
    QString competition_id, agent_id, coin, side;  // side: "long" | "short"
    double qty = 0, entry_price = 0, leverage = 1, funding_accrued = 0;
};
struct EquitySnapshotRow {
    QString competition_id, agent_id;
    int round_seq = 0;
    qint64 ts = 0;
    double equity = 0, balance = 0, unrealized_pnl = 0;
};

// ── Market data ─────────────────────────────────────────────────────────────
struct Candle { qint64 t = 0; double o = 0, h = 0, l = 0, c = 0, v = 0; };
struct CoinSnapshot {
    QString coin;
    double mid = 0, open_interest = 0, day_change_pct = 0;
    double funding_rate = 0;               // per-hour fractional rate (HL convention)
    QVector<Candle> candles_1h;            // oldest → newest
    double ema20 = 0, ema50 = 0, rsi14 = 0, atr14 = 0;
};
struct MarketSnapshot {
    qint64 ts = 0;
    QHash<QString, CoinSnapshot> coins;
};

// ── Decisions ───────────────────────────────────────────────────────────────
enum class ActionKind { Open, Close, Hold };
struct AgentAction {
    ActionKind kind = ActionKind::Hold;
    QString coin, side;          // side: "long"|"short" (open only)
    double size_usd = 0;         // open: notional; close: 0 = full close, >0 = reduce
    double leverage = 1;
    QString reason;
};

} // namespace fincept::arena
