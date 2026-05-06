#pragma once
// StrategyTemplates — hard-coded Sensibull-style ready-made templates and
// the instantiator that turns a (template, chain, options) tuple into a
// concrete `Strategy` with resolved tokens, strikes, and entry prices.
//
// Phase 4 scope:
//   - 20 templates spanning Bullish / Bearish / Neutral / Volatility / Others
//   - Single-expiry only (calendar / diagonals deferred)
//   - Instantiation is pure: no DB, no hub, no UI dependencies
//
// Recipe semantics
// ────────────────
// Each `StrategyLegRecipe` carries an `offset_index` measured in chain rows
// from the ATM strike at canonical width = 1. The instantiator computes:
//
//     actual_offset = recipe.offset_index * options.width + options.shift
//     target_row    = chain.rows[ atm_index + actual_offset ]
//
// So a Bull Call Spread (offsets 0 and +1) at width=3 buys ATM CE and sells
// the strike three rows above (e.g. NIFTY ATM=24000, strikes step 50 →
// sells 24150). Shift moves the whole construction up/down without changing
// width — useful when the user has a directional bias.
//
// Lots are signed at the recipe level (positive = buy, negative = sell) and
// scale by `options.default_lots` at instantiation. The recipe author picks
// the *direction* and *ratio* (1:1 vs 1:2 for ratio backspreads); the user
// picks the *size*.

#include "core/result/Result.h"
#include "services/options/OptionChainTypes.h"
#include "trading/instruments/InstrumentTypes.h"

#include <QString>
#include <QVector>

namespace fincept::services::options {

enum class StrategyCategory {
    Bullish,
    Bearish,
    Neutral,
    Volatility,
    Others,
};

inline const char* strategy_category_str(StrategyCategory c) {
    switch (c) {
        case StrategyCategory::Bullish:    return "BULLISH";
        case StrategyCategory::Bearish:    return "BEARISH";
        case StrategyCategory::Neutral:    return "NEUTRAL";
        case StrategyCategory::Volatility: return "VOLATILE";
        case StrategyCategory::Others:     return "OTHERS";
    }
    return "OTHERS";
}

/// One leg of a recipe — opaque to the chain. The instantiator resolves
/// `offset_index` against the live chain to a concrete strike + token.
struct StrategyLegRecipe {
    fincept::trading::InstrumentType type;  // CE or PE only in v1
    int offset_index;                       // 0 = ATM, ±N = chain rows from ATM
    int lots;                               // signed: + = buy, − = sell
};

/// Catalogue entry — a named, categorised composition of legs.
struct StrategyTemplate {
    QString id;                             // "long_call", "iron_condor", …
    QString name;                           // display name
    StrategyCategory category;
    QString description;                    // one-line summary
    QString outlook;                        // "Strongly bullish, expect big move up"
    QVector<StrategyLegRecipe> legs;
    /// True when the recipe authors offsets at canonical width 1 and the
    /// instantiator should scale them by `options.width`. False for single-
    /// leg or fixed-shape templates (e.g. Long Call, Synthetic Long).
    bool supports_width = false;
    /// Default width applied by the picker when the user hasn't tweaked the
    /// width control yet. Ignored for `supports_width = false`.
    int default_width = 1;
};

/// Knobs the user adjusts in the picker UI before pressing "Use".
struct StrategyInstantiationOptions {
    /// Wing width multiplier (1, 2, 3, …). Ignored when the template's
    /// `supports_width` is false.
    int width = 1;
    /// Offset applied to every leg AFTER width scaling. Lets the user move
    /// the whole strategy up/down a few strikes for a directional bias.
    int shift = 0;
    /// Per-leg lot multiplier. Recipe lots × default_lots. Default 1.
    int default_lots = 1;
};

// ── Catalogue + instantiation API ──────────────────────────────────────────

/// Hard-coded list of ~20 templates. Stable order; UI groups by category at
/// render time. Lifetime: program-static; safe to hold a reference.
const QVector<StrategyTemplate>& catalog();

/// O(N) lookup by id. Returns nullptr when not found.
const StrategyTemplate* find(const QString& id);

/// Build a concrete `Strategy` from a template + chain snapshot + user knobs.
/// Failure modes:
///   - chain has no ATM row (chain.atm_strike == 0)
///   - any leg's resolved row falls outside chain.rows bounds
///   - the chosen leg side (CE/PE) on a row has no token (token == 0)
///
/// On success, `Strategy.created` / `.modified` are set to current time and
/// `.expiry` mirrors the chain's expiry. `.notes` is initialised empty.
Result<Strategy> instantiate(const StrategyTemplate& tpl, const OptionChain& chain,
                             const StrategyInstantiationOptions& options = {});

} // namespace fincept::services::options
