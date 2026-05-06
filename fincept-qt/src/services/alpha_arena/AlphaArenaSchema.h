#pragma once
// AlphaArenaSchema — the canonical action grammar for Alpha Arena.
//
// This header is the *single source of truth* for what an LLM agent is
// allowed to emit, and how we interpret it. Any divergence between this
// file and scripts/alpha_arena/schema.json is a CI-failing bug.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §5 (Action grammar).
//
// Design notes:
//  * Four signals only — buy_to_enter / sell_to_enter / hold / close.
//    No partial close, no pyramiding, no hedging. RiskEngine enforces.
//  * One position per coin invariant — enforced server-side.
//  * Leverage is an integer in [1, 20]. Hyperliquid's S1 cap.
//  * confidence ∈ [0, 1]; risk_usd is what *the model claims* — RiskEngine
//    recomputes from |entry − stop_loss| × qty and overrides if drift > 5%.
//  * justification is free text; we cap at 1000 chars on parse.
//  * invalidation_condition is the model's externalised "exit reason" memory;
//    capped at 280 chars on parse.

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::alpha_arena {

/// Trading signals an agent may emit. Anything else is a parse error.
enum class Signal {
    BuyToEnter,
    SellToEnter,
    Hold,
    Close,
};

/// Competition modes — match Nof1 S1/S2 published modes.
/// (max_leverage from the previous prototype is dropped — it was marketing copy,
/// not a real Nof1 mode.)
enum class CompetitionMode {
    Baseline,    // S1 default ruleset.
    Monk,        // ≤ 1 entry / coin / 30 min, max position 25% of equity.
    Situational, // S2: agents see other agents' positions in their context.
};

/// The canonical perp universe (Nof1 Season 1).
/// User may extend up to 12 perps from a curated venue list, but the default
/// IS this set.
inline const QStringList& kPerpUniverse() {
    static const QStringList universe{"BTC", "ETH", "SOL", "BNB", "DOGE", "XRP"};
    return universe;
}

/// Hard caps used by RiskEngine and the action parser. Keep here so they are
/// visible alongside the schema they constrain.
namespace limits {
inline constexpr int kMinLeverage = 1;
inline constexpr int kMaxLeverage = 20;
inline constexpr int kInvalidationConditionMaxChars = 280;
inline constexpr int kJustificationMaxChars = 1000;
inline constexpr double kRiskUsdRecomputeToleranceFrac = 0.05; // 5%
inline constexpr double kMaxRiskPerTradeFrac = 0.02;           // 2% of equity
inline constexpr double kMinLiquidationBufferFrac = 0.15;      // 15% of price
} // namespace limits

/// One action proposal from a model. Validated by parse_model_response();
/// downstream code may assume fields are within their declared ranges.
struct ProposedAction {
    Signal signal = Signal::Hold;
    QString coin;                     // base symbol from kPerpUniverse(), e.g. "BTC".
    double quantity = 0.0;            // base-asset units, > 0 for entries, ignored for hold.
    int leverage = 1;                 // [1, 20]
    double profit_target = 0.0;       // absolute price; 0 = unset (not allowed for entries).
    double stop_loss = 0.0;           // absolute price; 0 = unset (not allowed for entries).
    QString invalidation_condition;   // ≤ 280 chars
    double confidence = 0.0;          // [0, 1]
    double risk_usd = 0.0;            // model-claimed; RiskEngine may override.
    QString justification;            // ≤ 1000 chars
};

/// What a model returned for one tick.
/// Either `actions` is populated and `parse_error` is empty, or `parse_error`
/// is set and we ran a no-op tick (no retry — matches Nof1 behaviour).
struct ModelResponse {
    QString raw_text;                 // verbatim model output, for ModelChat trace.
    QVector<ProposedAction> actions;
    std::optional<QString> parse_error;
};

/// Round-trip helpers — convert between Signal and the canonical wire string.
/// Wire strings are exactly what the LLM is told to emit in the system prompt;
/// changing them is a breaking schema change.
inline QString signal_to_wire(Signal s) {
    switch (s) {
    case Signal::BuyToEnter:  return QStringLiteral("buy_to_enter");
    case Signal::SellToEnter: return QStringLiteral("sell_to_enter");
    case Signal::Hold:        return QStringLiteral("hold");
    case Signal::Close:       return QStringLiteral("close");
    }
    return QStringLiteral("hold");
}

inline std::optional<Signal> signal_from_wire(const QString& s) {
    if (s == QLatin1String("buy_to_enter"))  return Signal::BuyToEnter;
    if (s == QLatin1String("sell_to_enter")) return Signal::SellToEnter;
    if (s == QLatin1String("hold"))          return Signal::Hold;
    if (s == QLatin1String("close"))         return Signal::Close;
    return std::nullopt;
}

inline QString mode_to_wire(CompetitionMode m) {
    switch (m) {
    case CompetitionMode::Baseline:    return QStringLiteral("baseline");
    case CompetitionMode::Monk:        return QStringLiteral("monk");
    case CompetitionMode::Situational: return QStringLiteral("situational");
    }
    return QStringLiteral("baseline");
}

inline std::optional<CompetitionMode> mode_from_wire(const QString& s) {
    if (s == QLatin1String("baseline"))    return CompetitionMode::Baseline;
    if (s == QLatin1String("monk"))        return CompetitionMode::Monk;
    if (s == QLatin1String("situational")) return CompetitionMode::Situational;
    return std::nullopt;
}

} // namespace fincept::services::alpha_arena
