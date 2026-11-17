#pragma once
// CursorAdapter.h — Cursor's agent CLI (`agent`) as a harness. BETA: written
// from cursor.com/docs/cli; not installed on the development machine.
//
//   agent -p <context + prompt> --output-format stream-json --trust
//         [--workspace <cwd>] [--model M] [--mode ask] [--resume <chat id>]
//   --mode ask for read-only analysis; workspace edits need no flag (print mode
//   writes inside the workspace). No system-prompt flag: the context is wrapped
//   into the first prompt. Output: CursorStreamParser.
//
// IDENTITY: the binary is called `agent`, and Grok Build ships an `agent.exe`
// too (on the development machine `agent --version` prints "grok 1.0.41 …").
// parse_version() therefore rejects any line naming grok and accepts only one
// naming cursor. Cursor's exact --version text is unconfirmed: if it does not
// name cursor, detection reports the binary as unrecognised.
//
// Not used (output shapes or values not documented): `agent status`, `agent
// models`, a reasoning-effort option, per-run MCP (print mode reads only the
// project's .cursor/mcp.json), full access.

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class CursorAdapter final : public IHarnessAdapter {
  public:
    CursorAdapter();

    const HarnessDescriptor& descriptor() const override { return desc_; }
    QStringList candidate_paths(const DetectContext& ctx) const override;
    ProbeCommand version_command() const override;
    QString parse_version(const QString& output) const override;
    std::optional<ProbeCommand> auth_command() const override;
    AuthStatus parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const override;
    AuthStatus auth_hint(const DetectContext& ctx) const override;
    std::optional<ProbeCommand> models_command() const override;
    QVector<ModelInfo> parse_models(int exit_code, const QByteArray& out) const override;
    QVector<ModelInfo> builtin_models(const QString& version) const override;
    QStringList efforts(const QString& model, const QString& version) const override;
    LaunchPlan build_command(const RunSpec& spec, const Installation& inst) const override;
    LaunchPlan mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const override;
    std::unique_ptr<IStreamParser> make_parser() const override;
    CancelPlan cancel_plan() const override;
    FailureKind classify_failure(int exit_code, const services::coding_agents::StreamFacts& facts,
                                 const QStringList& stderr_tail) const override;
    std::optional<McpRegistration> mcp_registration(const McpEndpoint& endpoint) const override;

  private:
    HarnessDescriptor desc_;
};

} // namespace fincept::harness
