#pragma once
// CodexAdapter.h — OpenAI Codex CLI (`codex`) as a harness.
//
//   codex exec [resume] --json --skip-git-repo-check
//         (-s read-only|workspace-write|danger-full-access  |  on resume: -c sandbox_mode="…")
//         -c approval_policy="never" [-m M] [-c model_reasoning_effort="E"]
//         [-c mcp_servers.<name>.url="…" -c mcp_servers.<name>.bearer_token_env_var="…"
//          -c mcp_servers.<name>.tool_timeout_sec=N]
//         [-c mcp_servers.<other>.enabled=false  for every other server in the
//          user's config.toml — RunSpec::user_mcp_server_names]
//         [<session id>] -
//   prompt on stdin; Codex has no system-prompt flag, so the context is wrapped
//   into the prompt (not re-sent on resume). Output: coding_agents::CodexJsonParser.
//
// Terminal-started runs see the terminal's MCP server only (like Claude Code's
// --strict-mcp-config). Codex MERGES `-c mcp_servers…` into the user's table
// (checked with codex-cli 0.160.0 and a scratch CODEX_HOME: `-c mcp_servers={…}`
// keeps every user entry), so each other entry is switched off by name;
// `.enabled=false` on a name the config does not define is a config error, so
// only names read from the user-level config.toml are switched off.
// --ignore-user-config is not used: it also drops the user's model / provider /
// sign-in settings. Not covered: servers in a TRUSTED project's own
// .codex/config.toml (loaded per project; their trust is not read here).
//
// Sign-in: `codex login status` (the answer is on stderr). Models:
// `codex debug models` (a debug command — the shape may change, so a built-in
// list is the fallback).
// CONNECT: `codex mcp add <name> --url <url> --bearer-token-env-var FINCEPT_COMMUNITY_MCP_TOKEN`
// (token-free argv; Codex reads the variable when it connects).
//
// Terminal-AI call (RunSpec::chat, caps.chat — off where the read-only sandbox
// cannot be verified without running a model, i.e. Windows):
//   codex exec --json [--ephemeral] --ignore-user-config --skip-git-repo-check
//         -s read-only -c approval_policy="never" -c web_search="disabled"
//         [-m M] [-c model_reasoning_effort="E"] [-c mcp_servers.<name>.… ×3]
//         [--output-schema <temp_dir>/schema-<run8>.json] -
//   context wrapped into the stdin prompt; never resumed.

#include "services/harness/IHarnessAdapter.h"

namespace fincept::harness {

class CodexAdapter final : public IHarnessAdapter {
  public:
    CodexAdapter();

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
    /// The [mcp_servers.<name>] entries of the user-level config.toml
    /// ($CODEX_HOME/config.toml, else ~/.codex/config.toml). Names only.
    QStringList user_mcp_server_names(const DetectContext& ctx) const override;

    /// Server NAMES defined in a config.toml text: `[mcp_servers.<name>…]` table
    /// headers, `<name>.…` / `<name> = {…}` keys inside `[mcp_servers]`, top-level
    /// `mcp_servers.<name>.…` dotted keys and the keys of an inline
    /// `mcp_servers = { … }` table. Values are never kept; only names matching
    /// ^[A-Za-z0-9_-]{1,64}$ (what a -c dotted path can name) are returned.
    static QStringList parse_mcp_server_names(const QByteArray& config_toml);

  private:
    LaunchPlan build_chat_command(const RunSpec& spec, const Installation& inst) const;

    HarnessDescriptor desc_;
};

} // namespace fincept::harness
