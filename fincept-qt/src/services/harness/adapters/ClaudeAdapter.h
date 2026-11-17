#pragma once
// ClaudeAdapter.h — Anthropic Claude Code (`claude`) as a harness.
//
//   claude -p --output-format stream-json --verbose [--include-partial-messages]
//          --permission-mode dontAsk
//          [--permission-prompts none]                  (version >= 2.1.259)
//          [--mcp-config <temp_dir>/mcp-<run8>.json] --strict-mcp-config
//          --tools Read,Grep,Glob,WebSearch,WebFetch[,Edit,Write]
//          --allowedTools [mcp__<server>__*,]<same built-ins>
//          --append-system-prompt <text> [--model M] [--effort E]
//          [--max-budget-usd N] [--json-schema S] [--resume <session>]
//   Full access: --permission-mode bypassPermissions, no --permission-prompts,
//   no --tools / --allowedTools (the MCP config stays strict).
//   prompt on stdin. Output parsed by coding_agents::ClaudeStreamParser.
//
// Terminal-AI call (RunSpec::chat, caps.chat; version >= kChatMin*):
//   claude -p --output-format stream-json --verbose [--include-partial-messages]
//          [--no-session-persistence] --restricted --tools ""
//          [--mcp-config <temp_dir>/mcp-<run8>.json] --strict-mcp-config
//          [--allowedTools mcp__<server>__*]
//          --permission-mode dontAsk --permission-prompts none
//          --system-prompt <text> [--model M] [--effort E] [--json-schema S]
//          [--resume <session>]                          (chat_persist_session only)
//   No built-in tool at all (--tools ""), user / project settings ignored
//   (--restricted), only the terminal's MCP server; CLAUDE_CODE_OAUTH_TOKEN is
//   removed from the child (a subscription token never serves terminal AI).
//   Read-only is therefore enforced by tool removal, not an OS sandbox (which
//   native Windows does not have).
//
// Sign-in: `claude auth status --json` — only loggedIn / authMethod /
// apiProvider / subscriptionType are read; the rest (e-mail, organisation) is
// dropped unread. Models: no list command, a built-in alias list.
// CONNECT: `claude mcp add --transport http --scope user <name> <url>
// --header "Authorization: Bearer ${FINCEPT_MCP_TOKEN}"` (token-free argv).

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class ClaudeAdapter final : public IHarnessAdapter {
  public:
    ClaudeAdapter();

    /// `--permission-prompts none` exists from this version on.
    static constexpr int kPermissionPromptsMajor = 2;
    static constexpr int kPermissionPromptsMinor = 1;
    static constexpr int kPermissionPromptsPatch = 259;
    /// Terminal-AI calls need --restricted / --no-session-persistence / --tools ""
    /// as verified on this version (older or unknown versions are refused).
    static constexpr int kChatMinMajor = 2;
    static constexpr int kChatMinMinor = 1;
    static constexpr int kChatMinPatch = 325;

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
    LaunchPlan build_chat_command(const RunSpec& spec, const Installation& inst) const;

    HarnessDescriptor desc_;
};

} // namespace fincept::harness
