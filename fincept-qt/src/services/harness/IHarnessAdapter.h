#pragma once
// IHarnessAdapter.h — what the terminal needs to know about one CLI agent
// ("harness": Claude Code, Codex, …): how to find and identify it, how to ask
// it for its sign-in state and models, how to launch a run and read its output.
//
// Adapters are stateless and QtCore only: they never start a process, touch the
// file system or read settings themselves. HarnessRegistry runs the probe
// commands they describe; CodingAgentService runs the LaunchPlan they build.
// Status output that may contain identity fields (e-mail, organisation) is
// handed to parse_auth() and discarded — adapters keep only the named fields.

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/harness/HarnessTypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>

namespace fincept::harness {

/// Turns one stdout line of a run into transcript events (wraps the existing
/// coding_agents parsers for the print-mode JSONL harnesses).
///
/// Contract for every implementation: never throws or asserts; a malformed,
/// truncated or unknown line becomes a status event (never dropped silently);
/// one line of any size is handled (the service caps a line at
/// LineBuffer::kMaxLineBytes). Text deltas are reported as AgentEvent::partial
/// events (see CodingAgentTypes.h) and always followed by the complete event.
class IStreamParser {
  public:
    virtual ~IStreamParser() = default;
    virtual QVector<services::coding_agents::AgentEvent> parse_line(const QString& line) = 0;
    /// The process has exited and every line was parsed: flush whatever is still
    /// buffered (a message assembled from deltas whose end was never signalled).
    virtual QVector<services::coding_agents::AgentEvent> finish() { return {}; }
    virtual const services::coding_agents::StreamFacts& facts() const = 0;
};

class IHarnessAdapter {
  public:
    virtual ~IHarnessAdapter() = default;

    virtual const HarnessDescriptor& descriptor() const = 0;

    // ── Detection ────────────────────────────────────────────────────────────
    /// Full file paths to try after PATH and before the shared per-OS list.
    virtual QStringList candidate_paths(const DetectContext& ctx) const = 0;
    virtual ProbeCommand version_command() const = 0;
    /// "MAJOR.MINOR.PATCH" from (a line of) `--version` output; empty = this
    /// binary is not this harness (e.g. Grok's `agent` is not Cursor's).
    virtual QString parse_version(const QString& output) const = 0;

    // ── Sign-in state ────────────────────────────────────────────────────────
    virtual std::optional<ProbeCommand> auth_command() const = 0;
    virtual AuthStatus parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const = 0;
    /// From environment-variable NAMES and file EXISTENCE only — nothing is read.
    virtual AuthStatus auth_hint(const DetectContext& ctx) const = 0;

    // ── Models and effort ────────────────────────────────────────────────────
    virtual std::optional<ProbeCommand> models_command() const = 0;
    virtual QVector<ModelInfo> parse_models(int exit_code, const QByteArray& out) const = 0;
    virtual QVector<ModelInfo> builtin_models(const QString& version) const = 0;
    virtual QStringList efforts(const QString& model, const QString& version) const = 0;

    // ── Runs ─────────────────────────────────────────────────────────────────
    /// The complete argument list (including resume and the per-run MCP
    /// configuration), stdin payload and temporary files of one run.
    virtual LaunchPlan build_command(const RunSpec& spec, const Installation& inst) const = 0;
    /// The MCP part of build_command(): arguments and temp files only.
    virtual LaunchPlan mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const = 0;
    virtual std::unique_ptr<IStreamParser> make_parser() const = 0;
    virtual CancelPlan cancel_plan() const = 0;
    virtual FailureKind classify_failure(int exit_code, const services::coding_agents::StreamFacts& facts,
                                         const QStringList& stderr_tail) const = 0;
    /// How the CONNECT tab adds the terminal's MCP server to the user's own copy
    /// of this harness (one-click command and/or copy-paste forms). nullopt =
    /// this harness has no documented way to register an MCP server.
    virtual std::optional<McpRegistration> mcp_registration(const McpEndpoint& endpoint) const = 0;
    /// MCP server entry NAMES this harness would load from the user's own config
    /// for a run the terminal starts (RunSpec::user_mcp_server_names). Only an
    /// adapter whose CLI merges per-run servers into the user's table needs them,
    /// to disable every entry but the terminal's. Names only — a config file is
    /// read through ctx.read_file and nothing but table names is kept.
    virtual QStringList user_mcp_server_names(const DetectContext& ctx) const {
        Q_UNUSED(ctx);
        return {};
    }
};

} // namespace fincept::harness
