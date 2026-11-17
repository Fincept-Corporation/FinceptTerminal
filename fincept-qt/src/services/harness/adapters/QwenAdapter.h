#pragma once
// QwenAdapter.h — Qwen Code (`qwen`) as a harness. BETA: written from the Qwen
// Code headless documentation (qwenlm.github.io/qwen-code-docs); not installed
// on the development machine.
//
//   qwen -p <context + prompt> -o stream-json [--include-partial-messages]
//        --approval-mode plan|auto-edit [-m M] [--resume <session id>]
//   Claude-like stream-json (its init message is system/session_start), parsed
//   by coding_agents::ClaudeStreamParser. No system-prompt flag is documented:
//   the context is wrapped into the first prompt.
//
// Not used: per-run MCP (no flag documented), a sign-in status or model list
// command (none documented), reasoning effort, full access (yolo).

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class QwenAdapter final : public IHarnessAdapter {
  public:
    QwenAdapter();

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
