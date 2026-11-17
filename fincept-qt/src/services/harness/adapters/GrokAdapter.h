#pragma once
// GrokAdapter.h — xAI Grok Build CLI (`grok`) as a harness. BETA: written from
// `grok --help` / `grok models` of grok 1.0.41 on a Windows machine; no run has
// been recorded yet.
//
//   grok -p <prompt> --output-format streaming-json
//        --permission-mode dontAsk|acceptEdits        (read-only | workspace edits)
//        --rules <system prompt> [-m M] [--max-turns N] [-r <session id>]
//   A prompt that would make the command line too long (or that has to pass
//   through a cmd.exe launcher) goes through `--prompt-file <temp_dir>/prompt-<run8>.txt`.
//   Output: "NDJSON: one ACP session update per line" → AcpUpdateParser.
//
// Not offered (help lists no values or no flag): reasoning effort values,
// per-run MCP servers, full access, a JSON schema in streaming mode.
// Sign-in: `grok models` prints "You are not authenticated." when signed out
// (recorded); a signed-in state is inferred from the model list.
// Install: %USERPROFILE%\.grok\bin\grok.exe; the same folder holds agent.exe, the
// same binary — accepted only because its `--version` names grok (Cursor's CLI
// is also called `agent`).

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class GrokAdapter final : public IHarnessAdapter {
  public:
    GrokAdapter();

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
