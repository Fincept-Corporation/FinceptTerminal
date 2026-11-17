#pragma once
// HarnessChatRun.h — run ONE terminal-AI call through a harness CLI and stream
// its answer: the process runner behind HarnessChatBackend.
//
//   * a slot from the chat pool (max 2 processes; the rest queue)
//   * a fresh empty working folder <scratch_root>/<uuid>, removed afterwards
//   * the adapter's chat command line (RunSpec::chat: no built-in tools,
//     read-only), the conversation flattened into the prompt (stateless)
//   * stdout parsed line by line by the adapter's parser; text and thinking
//     deltas, tool calls and results go to the sink as they arrive
//   * timeouts: first event, total; cancel = the adapter's cancel plan, then
//     the whole process tree (Windows job object / POSIX process group)
//   * the outcome classified by the adapter (auth, rate limit, timeout, …)
//
// Blocking: runs a local event loop on the calling thread — a worker thread,
// never the GUI thread. Everything outside (registry, MCP server, settings,
// paths) is injected through HarnessChatEnvironment, so a test drives a fake
// CLI. QtCore only (+ HarnessLaunch's Win32 job API).

#include "services/harness/HarnessChatTypes.h"
#include "services/harness/HarnessTypes.h"

#include <QProcessEnvironment>
#include <QString>

#include <functional>
#include <optional>

namespace fincept::harness {

class IHarnessAdapter;
class HarnessProcessPool;

struct HarnessChatEnvironment {
    const IHarnessAdapter* adapter = nullptr;
    Installation installation;
    /// The child's environment for `plan` (applies its env_set / env_unset).
    /// Empty = the system environment with the plan applied.
    std::function<QProcessEnvironment(const LaunchPlan& plan)> environment;
    /// The terminal's MCP server for this call (its token travels in the
    /// environment, never argv). Used only when the request allows tools.
    std::optional<McpEndpoint> mcp;
    /// A fresh <scratch_root>/<uuid> folder is the call's working directory.
    QString scratch_root;
    HarnessProcessPool* pool = nullptr; // null = no limit
    int pool_wait_ms = 2 * 60 * 1000;
    /// Checked on every stdout line before it is parsed: non-empty = stop the
    /// call now as an Auth failure with this sentence (subscription session).
    std::function<QString(const QString& line)> line_guard;
    /// Diagnostics (never prompt or answer text).
    std::function<void(const QString& message)> log;
};

/// The prompt a stateless call sends: earlier user / assistant turns as a
/// transcript, then the new message. System turns go to chat_system_prompt().
QString flatten_chat_prompt(const QVector<ChatTurn>& history, const QString& user_message);
/// `base` plus the system turns of the history (e.g. a node's instructions).
QString chat_system_prompt(const QString& base, const QVector<ChatTurn>& history);

HarnessChatResult run_harness_chat(const HarnessChatEnvironment& env, const HarnessChatRequest& req,
                                   const HarnessChatSink& sink);

} // namespace fincept::harness
