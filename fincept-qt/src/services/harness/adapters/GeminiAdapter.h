#pragma once
// GeminiAdapter.h — Google Gemini CLI (`gemini`) as a harness. BETA: written
// from the Gemini CLI documentation (geminicli.com/docs) and its source
// (packages/core/src/output/types.ts); not installed on the development machine.
//
//   gemini -p <context + prompt> -o stream-json --approval-mode plan|auto_edit
//          [-m M] [-r <session uuid>]
//   No system-prompt flag: the context is wrapped into the first prompt (not
//   re-sent on resume). Output: GeminiStreamParser.
//
// API-KEY ONLY: since 2026-06-18 Gemini CLI serves paid API keys only, and
// Google's terms say third-party software using Gemini CLI's OAuth sign-in "is a
// violation of applicable terms". A run is refused unless GEMINI_API_KEY is set
// in the child's environment. No sign-in status command, no model list command
// (built-in aliases), no reasoning-effort flag, no per-run MCP option.

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class GeminiAdapter final : public IHarnessAdapter {
  public:
    GeminiAdapter();

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
