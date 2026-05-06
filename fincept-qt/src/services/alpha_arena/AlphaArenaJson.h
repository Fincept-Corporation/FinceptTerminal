#pragma once
// AlphaArenaJson — strict parsers between LLM wire format and the
// canonical schema (AlphaArenaSchema.h).
//
// Parse failures are *expected* and benign — Nof1 spec says "no retry,
// log a no-op tick". So parse_model_response() never throws; it always
// returns a ModelResponse, with parse_error set on any defect.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §5.4 (Parse failures).

#include "services/alpha_arena/AlphaArenaSchema.h"

#include <QJsonObject>
#include <QString>

namespace fincept::services::alpha_arena {

/// Parse the model's raw completion. The model is asked to emit a JSON object
/// of the form {"actions": [...]} possibly wrapped in a fenced code block. We
/// extract the outermost {...}, validate each action against the schema, and
/// return a ModelResponse. Multiple actions per tick are allowed.
///
/// Validation rules (fail-closed — defective action drops, others survive):
///  * `signal` must round-trip through signal_from_wire().
///  * `coin` must be in kPerpUniverse() (case-insensitive; normalised upper).
///  * `leverage` is clamped into [kMinLeverage, kMaxLeverage] silently.
///  * `quantity` must be > 0 for entry signals; ignored for hold/close.
///  * `confidence` is clamped into [0, 1].
///  * `invalidation_condition` truncated to kInvalidationConditionMaxChars.
///  * `justification` truncated to kJustificationMaxChars.
///  * `profit_target` and `stop_loss` must be > 0 for entry signals;
///    if missing or zero on entries, the action is dropped with a
///    per-action note appended to parse_error (other actions still survive).
///
/// If the wrapping JSON itself is malformed, all actions are dropped and
/// `parse_error` describes the failure.
ModelResponse parse_model_response(const QString& raw_text);

/// Serialise one ProposedAction back to a JSON object — the form a model
/// would have emitted. Used in tests, ModelChat replay, and audit log.
QJsonObject action_to_json(const ProposedAction& a);

} // namespace fincept::services::alpha_arena
