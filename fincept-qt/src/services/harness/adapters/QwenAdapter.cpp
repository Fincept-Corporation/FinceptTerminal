// QwenAdapter.cpp — Qwen Code as a harness (see QwenAdapter.h). Only what the
// headless documentation states is used; QtCore only.

#include "services/harness/adapters/QwenAdapter.h"

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/AdapterSupport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::harness {

namespace qwen_adapter_detail {

namespace qwa_cad = fincept::services::coding_agents;
namespace qwa_sup = fincept::harness::adapter_support;

/// Qwen Code's stream-json has Claude Code's shapes (init = system/session_start).
class QwenParser final : public IStreamParser {
  public:
    QVector<qwa_cad::AgentEvent> parse_line(const QString& line) override { return parser_.parse_line(line); }
    QVector<qwa_cad::AgentEvent> finish() override { return parser_.finish(); }
    const qwa_cad::StreamFacts& facts() const override { return parser_.facts(); }

  private:
    qwa_cad::ClaudeStreamParser parser_;
};

} // namespace qwen_adapter_detail

using namespace qwen_adapter_detail;

QwenAdapter::QwenAdapter() {
    desc_.id = QStringLiteral("qwen");
    desc_.display_name = QStringLiteral("Qwen Code");
    desc_.vendor = QStringLiteral("Qwen");
    desc_.install_url = QStringLiteral("https://qwenlm.github.io/qwen-code-docs/");
    desc_.docs_url = QStringLiteral("https://qwenlm.github.io/qwen-code-docs/en/users/features/headless/");
    desc_.signin_hint = QStringLiteral("Run `qwen` in a terminal and sign in (Qwen OAuth now needs a paid plan).");
    desc_.executable_names = {QStringLiteral("qwen")};
    desc_.transport = Transport::PrintJsonl;
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = true;  // --include-partial-messages
    c.thinking = false;
    c.resume = true;       // --resume <session id> (headless documentation)
    c.per_run_mcp = false;
    c.mcp_http = false;
    c.enforced_read_only = false;
    c.model_list = false;
    c.effort = false;
    c.budget_usd = false;
    c.max_turns = false;
    c.json_schema = false;
    c.system_prompt = false;
    c.persistent_session = false;
    c.usage_tokens = true; // result.usage
    c.cost_usd = false;
    c.full_access = false;
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = true;
    desc_.beta_note = QStringLiteral("Written from the Qwen Code headless documentation; not yet checked against a "
                                     "recorded run");
}

QStringList QwenAdapter::candidate_paths(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // npm install: the shared search list covers it
    return {};
}

ProbeCommand QwenAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString QwenAdapter::parse_version(const QString& output) const {
    // Qwen Code (a Gemini CLI fork) prints its version through yargs: assumed to
    // be a bare "0.x.y" line (unverified); a line naming qwen is accepted too.
    return qwa_sup::named_or_bare_version(output, QStringLiteral("qwen"),
                                          {QStringLiteral("grok"), QStringLiteral("claude"), QStringLiteral("codex"),
                                           QStringLiteral("gemini"), QStringLiteral("cursor"), QStringLiteral("copilot")});
}

std::optional<ProbeCommand> QwenAdapter::auth_command() const {
    return std::nullopt; // no status command
}

AuthStatus QwenAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    Q_UNUSED(err);
    return AuthStatus{};
}

AuthStatus QwenAdapter::auth_hint(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // its API-key variables are not documented in what was read
    return AuthStatus{};
}

std::optional<ProbeCommand> QwenAdapter::models_command() const {
    return std::nullopt;
}

QVector<ModelInfo> QwenAdapter::parse_models(int exit_code, const QByteArray& out) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    return {};
}

QVector<ModelInfo> QwenAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    return {};
}

QStringList QwenAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return {};
}

LaunchPlan QwenAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    if (spec.chat) {
        plan.error = QStringLiteral("Chat calls through Qwen Code are not available yet.");
        return plan;
    }
    if (spec.permission == PermissionMode::FullAccess) {
        plan.error = QStringLiteral("Full access is not available for Qwen Code runs.");
        return plan;
    }
    if (spec.mcp) {
        plan.error = QStringLiteral("Qwen Code has no documented per-run MCP option. Run without terminal tools.");
        return plan;
    }
    if (!spec.effort.isEmpty()) {
        plan.error = QStringLiteral("Qwen Code has no reasoning-effort option.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Qwen Code's turn limit is not offered by the terminal.");
        return plan;
    }
    if (!spec.json_schema.isEmpty()) {
        plan.error = QStringLiteral("A response schema is not available for Qwen Code runs.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The prompt is empty.");
        return plan;
    }
    if (!spec.model.isEmpty() && !qwa_sup::valid_model_id(spec.model)) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(60));
        return plan;
    }
    const bool resume = !spec.resume_session_id.isEmpty();
    if (resume && !qwa_sup::valid_session_id(spec.resume_session_id)) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    const QString prompt = resume ? spec.prompt : qwa_cad::wrap_codex_prompt(spec.system_prompt, spec.prompt);
    if (prompt.size() > qwa_sup::kArgvTextBudgetChars) {
        plan.error = QStringLiteral("The prompt is too long for Qwen Code's command line (%1 characters; at most %2).")
                         .arg(prompt.size())
                         .arg(qwa_sup::kArgvTextBudgetChars);
        return plan;
    }
    plan.prompt_arg = qwa_sup::argv_text(prompt);
    QStringList& args = plan.args;
    args << QStringLiteral("-p") << plan.prompt_arg << QStringLiteral("-o") << QStringLiteral("stream-json");
    if (spec.want_deltas)
        args << QStringLiteral("--include-partial-messages");
    args << QStringLiteral("--approval-mode")
         << (spec.permission == PermissionMode::WorkspaceWrite ? QStringLiteral("auto-edit") : QStringLiteral("plan"));
    if (!spec.model.isEmpty())
        args << QStringLiteral("-m") << spec.model;
    if (resume)
        args << QStringLiteral("--resume") << spec.resume_session_id;
    return plan;
}

LaunchPlan QwenAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(endpoint);
    Q_UNUSED(spec);
    LaunchPlan plan;
    plan.error = QStringLiteral("Qwen Code has no documented per-run MCP option.");
    return plan;
}

std::unique_ptr<IStreamParser> QwenAdapter::make_parser() const {
    return std::make_unique<QwenParser>();
}

CancelPlan QwenAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind QwenAdapter::classify_failure(int exit_code, const qwa_cad::StreamFacts& facts,
                                          const QStringList& stderr_tail) const {
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(not logged in|not authenticated|unauthori[sz]ed|\b401\b|invalid api key|authentication (failed|required)|oauth)"));
    return qwa_sup::classify(exit_code, facts, stderr_tail, auth_rx, /*require_result=*/false);
}

std::optional<McpRegistration> QwenAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    // qwenlm.github.io/qwen-code-docs (MCP servers, settings; checked 2026-11-17):
    //   qwen mcp add --transport http NAME URL --header "H: V"
    //   ~/.qwen/settings.json {"mcpServers":{NAME:{"httpUrl","headers","timeout"}}}
    // (the Gemini CLI settings shape; timeout in ms, default 600 000). Tools appear
    // to the model as "mcp__<NAME>__<tool>". `--scope user` follows Gemini CLI,
    // whose `mcp add` writes the PROJECT settings by default — assumed, not
    // stated for Qwen; the settings snippet is the documented alternative.
    if (!qwa_sup::valid_mcp_endpoint(endpoint))
        return std::nullopt;
    McpRegistration r;
    r.server_name = endpoint.name;
    r.manual_args = {QStringLiteral("mcp"),  QStringLiteral("add"),  QStringLiteral("--transport"),
                     QStringLiteral("http"), endpoint.name,          endpoint.url,
                     QStringLiteral("--header"), qwa_sup::placeholder_bearer_header(), QStringLiteral("--scope"),
                     QStringLiteral("user")};
    const QJsonObject server{
        {QStringLiteral("httpUrl"), endpoint.url},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"),
                      QStringLiteral("Bearer ") + QString::fromLatin1(kMcpTokenPlaceholder)}}},
        {QStringLiteral("timeout"), qMax(1, endpoint.tool_timeout_s) * 1000},
    };
    r.config_snippet = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{endpoint.name, server}}}})
            .toJson(QJsonDocument::Indented));
    r.config_path_hint = QStringLiteral("~/.qwen/settings.json");
    r.note = QStringLiteral("Merge this entry into \"mcpServers\" in Qwen Code's settings.json (copying fills in the "
                            "token), or run the command. Add \"trust\": true only if Qwen Code may call the terminal's "
                            "tools without asking (it applies in trusted folders; the terminal still asks before "
                            "orders, money moves and deletes).");
    return r;
}

} // namespace fincept::harness
