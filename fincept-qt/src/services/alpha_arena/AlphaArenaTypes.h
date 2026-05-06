#pragma once
// AlphaArenaTypes — POD value types used across the Alpha Arena engine.
//
// These describe what flows from the venue into the ContextBuilder, and from
// the OrderRouter back into persistence. Nothing in here is Qt::Object — they
// are pure data, copyable, persistable, and trivially serialisable.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §4 (Context builder)
//            and §6 (Risk engine).

#include "services/alpha_arena/AlphaArenaSchema.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>
#include <optional>

namespace fincept::services::alpha_arena {

/// One scheduler tick. seq is monotonic per competition starting at 1.
struct Tick {
    qint64 utc_ms = 0;
    int seq = 0;
};

/// One candle. Used for both 3-minute intraday and 4-hour context bars.
struct Bar {
    qint64 utc_ms = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

/// Per-coin market state captured at decision time.
/// Series ordered OLDEST → NEWEST; ContextBuilder relies on this ordering.
struct MarketSnapshot {
    QString coin;                    // base symbol, e.g. "BTC"
    double mid = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double open_interest = 0.0;
    double funding_rate = 0.0;       // current 1h funding rate, fractional
    double ema20 = 0.0;
    double rsi7 = 0.0;
    double macd = 0.0;
    double macd_signal = 0.0;
    QVector<Bar> bars_3m_24h;        // up to 480 bars
    QVector<Bar> bars_4h_30d;        // up to 180 bars
};

/// One open position. Mirrors the position-table view the model sees.
struct Position {
    QString coin;
    double qty = 0.0;                // signed: positive = long, negative = short
    double entry = 0.0;
    double mark = 0.0;
    double liq_price = 0.0;
    double unrealized_pnl = 0.0;
    int leverage = 1;
    double profit_target = 0.0;
    double stop_loss = 0.0;
    QString invalidation_condition;
    QStringList order_ids;           // venue order ids associated with this position
};

/// What an agent's prompt sees about its own account at tick time.
struct AgentAccountState {
    double equity = 0.0;             // mark-to-market account value, USD
    double cash = 0.0;
    double return_pct = 0.0;         // since inception
    double sharpe = 0.0;             // rolling Sharpe
    double max_drawdown = 0.0;       // worst peak-to-trough so far
    double fees_paid = 0.0;
    double win_rate = 0.0;           // closed trades only
    int trades_count = 0;
    QVector<Position> positions;
};

/// Optional — only populated in CompetitionMode::Situational. Lists the other
/// agents' currently-open positions (no PnL, no equity — just exposure).
struct PeerPositionView {
    QString agent_display_name;
    QString coin;
    double qty = 0.0;
    int leverage = 1;
};

struct SituationalContext {
    QVector<PeerPositionView> peers;
};

/// Output of RiskEngine::evaluate(). Pure value type — no I/O.
struct RiskVerdict {
    enum class Outcome {
        Accept,   // pass to OrderRouter as-is
        Reject,   // do not place; record reason
        Amend,    // place using `amended` instead of original
    };
    Outcome outcome = Outcome::Reject;
    QString reason;
    std::optional<ProposedAction> amended;
};

/// Per-tick aggregate the engine assembles before fanning out to models.
/// All agents in a competition see the same MarketSnapshot vector and the
/// same Tick — only AgentAccountState varies (and SituationalContext in
/// situational mode).
struct TickContext {
    Tick tick;
    QVector<MarketSnapshot> markets;
};

} // namespace fincept::services::alpha_arena
