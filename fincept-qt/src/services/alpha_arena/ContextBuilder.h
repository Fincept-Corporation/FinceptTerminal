#pragma once
// ContextBuilder — assembles the system prompt + user prompt that every
// model sees on every tick.
//
// The byte-equal-across-agents invariant is the whole point of Alpha Arena.
// Two agents in the same competition, on the same tick, in non-situational
// mode, MUST receive byte-identical prompt strings. (Situational mode is the
// one exception — peers vary per agent by design.)
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §4 (Context builder).

#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"

#include <QString>

#include <optional>

namespace fincept::services::alpha_arena {

/// Render the static system prompt. This is constant for the lifetime of a
/// competition — it depends only on the policy/risk constants and the mode.
/// Cached by the engine.
QString build_system_prompt(CompetitionMode mode);

/// Render the dynamic user prompt for one agent on one tick.
///
/// Determinism contract:
///   For two calls with the same TickContext + same AgentAccountState +
///   same CompetitionMode + same SituationalContext (or both nullopt),
///   the returned string is byte-identical. This is enforced by a unit test
///   in Phase 6.
///
/// Float formatting is fixed:
///   * prices, equity, USD: 6 decimals
///   * indicators (EMA, MACD, RSI, ATR): 4 decimals
///   * quantities, volumes: 8 decimals
///   * leverage, integer counts: %d
QString build_user_prompt(const TickContext& ctx,
                          const AgentAccountState& account,
                          CompetitionMode mode,
                          const std::optional<SituationalContext>& situational);

} // namespace fincept::services::alpha_arena
