// CopilotAdapter.cpp — GitHub Copilot CLI as a harness (see CopilotAdapter.h).
// Only what the command reference states is used; QtCore only.

#include "services/harness/adapters/CopilotAdapter.h"

#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/AdapterSupport.h"
#include "services/harness/parsers/PlainTextParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::harness {

namespace copilot_adapter_detail {

namespace cpa_cad = fincept::services::coding_agents;
namespace cpa_sup = fincept::harness::adapter_support;

/// `--effort=LEVEL`: "low, medium, high, xhigh, max" (command reference).
const QStringList& copilot_efforts() {
    static const QStringList kEfforts = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"),
                                         QStringLiteral("xhigh"), QStringLiteral("max")};
    return kEfforts;
}

/// Token precedence documented for headless use: COPILOT_GITHUB_TOKEN > GH_TOKEN > GITHUB_TOKEN.
const QStringList& copilot_token_vars() {
    static const QStringList kVars = {QStringLiteral("COPILOT_GITHUB_TOKEN"), QStringLiteral("GH_TOKEN"),
                                      QStringLiteral("GITHUB_TOKEN")};
    return kVars;
}

} // namespace copilot_adapter_detail

using namespace copilot_adapter_detail;

CopilotAdapter::CopilotAdapter() {
    desc_.id = QStringLiteral("copilot");
    desc_.display_name = QStringLiteral("GitHub Copilot CLI");
    desc_.vendor = QStringLiteral("GitHub");
    desc_.install_url = QStringLiteral("https://docs.github.com/en/copilot/reference/copilot-cli-reference");
#ifdef Q_OS_WIN
    desc_.install_command = QStringLiteral("winget install GitHub.Copilot");
#else
    desc_.install_command = QStringLiteral("npm install -g @github/copilot");
#endif
    desc_.docs_url = QStringLiteral("https://docs.github.com/en/copilot/reference/copilot-cli-reference/cli-command-reference");
    desc_.signin_hint = QStringLiteral("Run `copilot login` in a terminal, or set COPILOT_GITHUB_TOKEN.");
    desc_.signin_command = QStringLiteral("copilot login");
    desc_.executable_names = {QStringLiteral("copilot")};
    desc_.transport = Transport::PlainText;
    Capabilities& c = desc_.caps;
    c.structured_stream = false;
    c.text_deltas = false;
    c.thinking = false;
    c.resume = false;      // plain text carries no session id
    c.per_run_mcp = false; // --additional-mcp-config would need the token in its JSON
    c.mcp_http = false;
    c.enforced_read_only = false;
    c.model_list = false;
    c.effort = true;       // --effort low|medium|high|xhigh|max
    c.budget_usd = false;
    c.max_turns = false;
    c.json_schema = false;
    c.system_prompt = false;
    c.persistent_session = false;
    c.usage_tokens = false;
    c.cost_usd = false;
    c.full_access = false;
    desc_.api_key_env_vars = copilot_token_vars();
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = true;
    desc_.beta_note = QStringLiteral("Written from the Copilot CLI command reference; plain-text output only; not yet "
                                     "checked against a recorded run");
}

QStringList CopilotAdapter::candidate_paths(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // npm / winget / Homebrew: the shared search list covers them
    return {};
}

ProbeCommand CopilotAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString CopilotAdapter::parse_version(const QString& output) const {
    // The --version text is not documented: a line naming copilot, or a bare
    // version line, is accepted (assumption — the path override is the escape hatch).
    return cpa_sup::named_or_bare_version(output, QStringLiteral("copilot"),
                                          {QStringLiteral("grok"), QStringLiteral("claude"), QStringLiteral("codex"),
                                           QStringLiteral("gemini"), QStringLiteral("qwen"), QStringLiteral("cursor")});
}

std::optional<ProbeCommand> CopilotAdapter::auth_command() const {
    return std::nullopt; // no non-interactive status command is documented
}

AuthStatus CopilotAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    Q_UNUSED(err);
    return AuthStatus{};
}

AuthStatus CopilotAdapter::auth_hint(const DetectContext& ctx) const {
    AuthStatus a;
    for (const QString& var : copilot_token_vars()) {
        if (ctx.has_env(var)) {
            a.api_key_env_present = true;
            a.method = QStringLiteral("env");
            a.note = QStringLiteral("%1 is set.").arg(var);
            break;
        }
    }
    return a;
}

std::optional<ProbeCommand> CopilotAdapter::models_command() const {
    return std::nullopt;
}

QVector<ModelInfo> CopilotAdapter::parse_models(int exit_code, const QByteArray& out) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    return {};
}

QVector<ModelInfo> CopilotAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    // "Pass `auto` as the value to let Copilot pick the best available model."
    ModelInfo m;
    m.id = QStringLiteral("auto");
    m.label = QStringLiteral("Auto");
    m.efforts = copilot_efforts();
    return {m};
}

QStringList CopilotAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return copilot_efforts();
}

LaunchPlan CopilotAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    if (spec.chat) {
        plan.error = QStringLiteral("Chat calls through Copilot CLI are not available yet.");
        return plan;
    }
    if (spec.permission == PermissionMode::FullAccess) {
        plan.error = QStringLiteral("Full access is not available for Copilot CLI runs.");
        return plan;
    }
    if (spec.mcp) {
        plan.error = QStringLiteral("Copilot CLI can only take an MCP server for one run with the token written into "
                                    "its configuration, which the terminal does not do. Run without terminal tools.");
        return plan;
    }
    if (!spec.resume_session_id.isEmpty()) {
        plan.error = QStringLiteral("Copilot CLI runs cannot be continued: its plain-text output names no session.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Copilot CLI has no turn limit option.");
        return plan;
    }
    if (!spec.json_schema.isEmpty()) {
        plan.error = QStringLiteral("A response schema is not available for Copilot CLI runs.");
        return plan;
    }
    if (!spec.effort.isEmpty() && !copilot_efforts().contains(spec.effort)) {
        plan.error = QStringLiteral("The reasoning effort “%1” is not valid.").arg(spec.effort.left(40));
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The prompt is empty.");
        return plan;
    }
    if (!spec.model.isEmpty() && !cpa_sup::valid_model_id(spec.model)) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(60));
        return plan;
    }
    const QString prompt = cpa_cad::wrap_codex_prompt(spec.system_prompt, spec.prompt);
    if (prompt.size() > cpa_sup::kArgvTextBudgetChars) {
        plan.error = QStringLiteral("The prompt is too long for Copilot CLI's command line (%1 characters; at most %2).")
                         .arg(prompt.size())
                         .arg(cpa_sup::kArgvTextBudgetChars);
        return plan;
    }
    plan.prompt_arg = cpa_sup::argv_text(prompt);
    QStringList& args = plan.args;
    args << QStringLiteral("-p") << plan.prompt_arg << QStringLiteral("--no-ask-user");
    if (!spec.cwd.isEmpty())
        args << QStringLiteral("-C") << spec.cwd;
    if (!spec.model.isEmpty())
        args << QStringLiteral("--model") << spec.model;
    if (!spec.effort.isEmpty())
        args << QStringLiteral("--effort") << spec.effort;
    // "For multiple tools, use a quoted, comma-separated list" — one argument.
    // A bare server name denies all of that MCP server's tools ("deny rules
    // always win"): the other Fincept edition's entries never serve this run.
    QStringList deny;
    if (spec.permission == PermissionMode::WorkspaceWrite)
        args << QStringLiteral("--allow-tool=write");
    else
        deny << QStringLiteral("write");
    deny << QStringLiteral("shell");
    for (const char* name : edition::kPrivateMcpServerNames)
        deny << QString::fromLatin1(name);
    args << QStringLiteral("--deny-tool=") + deny.join(QStringLiteral(", "));
    return plan;
}

LaunchPlan CopilotAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(endpoint);
    Q_UNUSED(spec);
    LaunchPlan plan;
    plan.error = QStringLiteral("Copilot CLI runs do not take the terminal's MCP server.");
    return plan;
}

std::unique_ptr<IStreamParser> CopilotAdapter::make_parser() const {
    return std::make_unique<PlainTextParser>();
}

CancelPlan CopilotAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind CopilotAdapter::classify_failure(int exit_code, const cpa_cad::StreamFacts& facts,
                                             const QStringList& stderr_tail) const {
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(copilot login|not logged in|not authenticated|unauthori[sz]ed|\b401\b|authentication (failed|required)|no copilot (access|subscription))"));
    return cpa_sup::classify(exit_code, facts, stderr_tail, auth_rx, /*require_result=*/false);
}

std::optional<McpRegistration> CopilotAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    // docs.github.com, Copilot CLI › Add MCP servers (checked 2026-11-17):
    //   copilot mcp add --transport http NAME URL --header "H: V" --timeout MS
    //   ~/.copilot/mcp-config.json {"mcpServers":{NAME:{"type":"http","url",
    //   "headers","tools":["*"],"timeout"}}} — `tools` is required; `timeout`
    //   (default 30 000 ms) bounds discovery AND every tool call, shorter than an
    //   approval prompt or a ui_wait, so it is set to the endpoint's tool budget.
    // Tools appear to the model as "<NAME>-<tool>"; --allow-tool='<NAME>'
    // pre-approves them. No one-click add: the header would need the literal token.
    if (!cpa_sup::valid_mcp_endpoint(endpoint))
        return std::nullopt;
    const QString timeout_ms = QString::number(qMax(1, endpoint.tool_timeout_s) * 1000);
    McpRegistration r;
    r.server_name = endpoint.name;
    r.manual_args = {QStringLiteral("mcp"),      QStringLiteral("add"),         QStringLiteral("--transport"),
                     QStringLiteral("http"),     endpoint.name,                 endpoint.url,
                     QStringLiteral("--header"), cpa_sup::placeholder_bearer_header(), QStringLiteral("--timeout"),
                     timeout_ms};
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), endpoint.url},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"),
                      QStringLiteral("Bearer ") + QString::fromLatin1(kMcpTokenPlaceholder)}}},
        {QStringLiteral("tools"), QJsonArray{QStringLiteral("*")}},
        {QStringLiteral("timeout"), qMax(1, endpoint.tool_timeout_s) * 1000},
    };
    r.config_snippet = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{endpoint.name, server}}}})
            .toJson(QJsonDocument::Indented));
    r.config_path_hint = QStringLiteral("~/.copilot/mcp-config.json");
    r.note = QStringLiteral("Copilot CLI names the terminal's tools \"%1-<tool>\"; start it with --allow-tool='%1' so "
                            "they run without a prompt each time (the terminal still asks before orders, money moves "
                            "and deletes). The timeout keeps a long terminal task from being cut off at Copilot's 30 s "
                            "default.")
                 .arg(endpoint.name);
    return r;
}

} // namespace fincept::harness
