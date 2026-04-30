#pragma once
// SchemaValidator.h — Validate + normalize tool args against a ToolSchema.
//
// Phase 3 of the MCP refactor (see plans/mcp-refactor-phase-3-schema-validation.md).
//
// Validates a QJsonObject against ToolSchema (typed `params` first, then the
// legacy `properties` JSON fragment). On success, mutates `args` in place to
// inject schema defaults so handlers receive normalised input. On failure,
// returns a Result error naming the offending parameter.
//
// This is the single validator path. It's invoked by McpProvider::call_tool
// before the handler runs, replacing the previously-orphaned
// McpService::validate_params (deleted in this phase).

#include "core/result/Result.h"
#include "mcp/McpTypes.h"

namespace fincept::mcp {

/// Validate `args` against `schema`. On success, inject defaults into `args`
/// for any param with a default that wasn't supplied by the caller. The
/// mutated args object is what handlers should subsequently read.
///
/// Errors fail fast with a descriptive message: "Invalid argument: <param>:
/// <reason>".
Result<void> validate_args(const ToolSchema& schema, QJsonObject& args);

} // namespace fincept::mcp
