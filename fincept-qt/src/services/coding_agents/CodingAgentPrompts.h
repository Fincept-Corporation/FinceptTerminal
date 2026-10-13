#pragma once
// CodingAgentPrompts.h — the context appended to every Agent Workbench run.
//
// Claude Code receives it through --append-system-prompt; Codex has no such
// flag, so it is prepended to the stdin prompt inside a delimited <context>
// block. QtCore only.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QDateTime>
#include <QString>

namespace fincept::services::coding_agents {

struct SystemPromptContext {
    AgentKind agent = AgentKind::Claude;
    RunMode mode = RunMode::Analysis;
    bool tools_enabled = true;
    QString mcp_server_name = QStringLiteral("fincept"); // the name the agent sees the terminal's MCP server under
    QString workdir;
    QDateTime now; // local time; invalid → current time
};

/// Who the user is, the clock, how to use the terminal's MCP tools (tool_list /
/// tool_describe / tool_invoke, job_* for long tools, result_fetch for large
/// results), where deliverables go and what never to do.
///
/// Written with no % ^ & | < > ! or double-quote characters so that, after
/// cmd_safe_text(), it survives a cmd.exe command line unchanged in meaning.
QString build_agent_system_prompt(const SystemPromptContext& ctx);

/// Codex has no --append-system-prompt: wrap the context and the request in
/// clearly delimited blocks.
QString wrap_codex_prompt(const QString& system_prompt, const QString& user_prompt);

/// Strip the characters cmd.exe interprets (% ^ & | < > ! ") and fold newlines
/// into spaces. Only used when a CLI must be launched through an npm .cmd shim.
QString cmd_safe_text(const QString& text);

} // namespace fincept::services::coding_agents
