#pragma once
// EssentialTools.h — the curated "essentials" toolset shared by every agent surface
// that cannot take the whole ~900-tool catalogue:
//   * McpHttpServer (external agents: Claude Code, Codex, Cursor…) in its
//     ExternalToolset::Essentials mode, and
//   * the Python agent bridge (TerminalMcpBridge / finagent_core) when an agent
//     config does not name its tools explicitly.
//
// The list holds (1) the discovery + job meta tools — tool_list, tool_describe,
// tool_invoke (generic dispatcher for any tool found via tool_list), job_status,
// job_result, job_cancel, job_list, result_fetch — and (2) a hand-picked set of
// high-value, read-only research tools (quotes, history, fundamentals, news,
// filings, macro series, portfolio/watchlist reads…).
//
// Names are bare internal tool names. Callers MUST tolerate names that are not
// registered in the running build (skip them) — the list is curated by hand and a
// renamed tool must never break a surface.

#include <QStringList>

namespace fincept::mcp {

/// Ordered: meta tools first, then research tools by priority.
QStringList essential_tool_names();

/// The meta subset only (discovery + generic dispatch + jobs + result paging).
QStringList essential_meta_tool_names();

} // namespace fincept::mcp
