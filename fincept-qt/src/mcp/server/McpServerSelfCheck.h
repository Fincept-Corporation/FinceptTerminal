#pragma once
// McpServerSelfCheck.h — an in-terminal check the user can run from the MCP
// settings / diagnostics surface: does every supported harness get a working
// session from the terminal's MCP server? Calls no model and spends nothing.
//
// It first asks GET /health (no token) which Fincept edition answers on the
// port, and stops there unless it is this one — a token is never sent to the
// other edition. Then it talks to the RUNNING server over loopback exactly like
// each harness does:
// for every client profile (Claude Code, Codex, Gemini CLI, Qwen Code, Cursor,
// Copilot, Grok, VS Code, Claude Desktop, generic) it sends that client's
// `initialize` (its real clientInfo name and protocol version), the
// `initialized` notification, tools/list (checking names, schemas and size
// against the profile), one harmless tools/call (ui_overview, read-only), ping
// and DELETE of the session. It also probes the MCP 2026-07-28 fallback path,
// and — when Python and the bridge script are available — the stdio bridge
// (scripts/fincept_mcp/fincept_mcp_stdio.py) end to end.
//
// Hook: Settings › AI Agents (MCP) (src/screens/settings/AgentConnectSection)
// can call run_mcp_self_check_async(this, …) from a "Check connections" button
// and show report.lines(); nothing here touches widgets.

#include "core/config/EditionIdentity.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::mcp {

struct SelfCheckStep {
    QString profile; ///< "claude-code", "codex", … or "stdio-bridge" / "protocol"
    QString step;    ///< "initialize", "tools/list", …
    bool ok = false;
    QString detail;
    qint64 ms = 0;
};

struct SelfCheckReport {
    bool ok = false;
    QList<SelfCheckStep> steps;
    /// One line per step ("[PASS] codex · tools/list — 11 tools, 9.8 KB (12 ms)").
    QStringList lines() const;
};

struct SelfCheckOptions {
    QString url;           ///< http://127.0.0.1:<port>/mcp
    QString token;         ///< bearer token (never logged, never shown)
    QString python;        ///< interpreter for the stdio bridge check; empty = skip it
    QString bridge_script; ///< fincept_mcp_stdio.py; empty = skip it
    int timeout_ms = 15000;
    bool call_tool = true; ///< run ui_overview once per profile
    /// The server must report this edition in GET /health before any token is
    /// sent (the other Fincept edition may listen on the port).
    QString expected_edition = QString::fromLatin1(edition::kId);
};

/// Blocking; run it on a worker thread, never the GUI thread.
SelfCheckReport run_mcp_self_check(const SelfCheckOptions& options);

/// Runs on its own short-lived thread against McpHttpServer::instance() (which
/// must be running) and delivers the report on `context`'s thread. GUI-thread
/// entry point. Defined in McpHttpServer.cpp (it reads the running server).
void run_mcp_self_check_async(QObject* context, std::function<void(const SelfCheckReport&)> done);

} // namespace fincept::mcp
