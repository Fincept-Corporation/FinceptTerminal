#pragma once
// CopilotAdapter.h — GitHub Copilot CLI (`copilot`) as a harness. BETA: written
// from docs.github.com (Copilot CLI command reference); not installed on the
// development machine.
//
//   copilot -p <context + prompt> --no-ask-user [-C <cwd>] [--model M] [--effort E]
//           read-only:       "--deny-tool=write, shell"
//           workspace edits: --allow-tool=write --deny-tool=shell
//   Plain-text output (Transport::PlainText → PlainTextParser): the JSONL
//   envelope of --output-format=json is not documented, so it is not used —
//   and without it there is no session id, hence no resume.
//
// Not used: per-run MCP (--additional-mcp-config takes the server JSON with a
// literal token), a model list command (none documented), full access
// (--allow-all-tools is not offered). Windows needs PowerShell 6+.

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class CopilotAdapter final : public IHarnessAdapter {
  public:
    CopilotAdapter();

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
