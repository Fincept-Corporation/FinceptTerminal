// CursorAdapter.cpp — Cursor's agent CLI as a harness (see CursorAdapter.h).
// Only what the documentation states is used; QtCore only.

#include "services/harness/adapters/CursorAdapter.h"

#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/AdapterSupport.h"
#include "services/harness/parsers/CursorStreamParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::harness {

namespace cursor_adapter_detail {

namespace cua_cad = fincept::services::coding_agents;
namespace cua_sup = fincept::harness::adapter_support;

const char* const kCursorKeyVar = "CURSOR_API_KEY";

} // namespace cursor_adapter_detail

using namespace cursor_adapter_detail;

CursorAdapter::CursorAdapter() {
    desc_.id = QStringLiteral("cursor");
    desc_.display_name = QStringLiteral("Cursor CLI");
    desc_.vendor = QStringLiteral("Cursor");
    desc_.install_url = QStringLiteral("https://cursor.com/docs/cli");
#ifndef Q_OS_WIN
    desc_.install_command = QStringLiteral("curl https://cursor.com/install | bash");
#endif
    desc_.docs_url = QStringLiteral("https://cursor.com/docs/cli");
    desc_.signin_hint = QStringLiteral("Run `agent login` in a terminal, or set CURSOR_API_KEY.");
    desc_.signin_command = QStringLiteral("agent login");
    desc_.executable_names = {QStringLiteral("agent")};
    desc_.transport = Transport::PrintJsonl;
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = false; // --stream-partial-output is not requested
    c.thinking = false;    // suppressed in print mode
    c.resume = true;       // --resume <chat id>
    c.per_run_mcp = false;
    c.mcp_http = true;     // ~/.cursor/mcp.json
    c.enforced_read_only = false;
    c.model_list = false;  // `agent models` output is not documented
    c.effort = false;
    c.budget_usd = false;
    c.max_turns = false;
    c.json_schema = false;
    c.system_prompt = false;
    c.persistent_session = false;
    c.usage_tokens = false;
    c.cost_usd = false;
    c.full_access = false;
    desc_.api_key_env_vars = {QString::fromLatin1(kCursorKeyVar)};
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = true;
    desc_.beta_note = QStringLiteral("Written from Cursor's CLI documentation; not yet checked against a recorded run");
}

QStringList CursorAdapter::candidate_paths(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // the installer's target folder is not documented: PATH and the shared list only
    return {};
}

ProbeCommand CursorAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString CursorAdapter::parse_version(const QString& output) const {
    // Grok Build's agent.exe answers "grok 1.0.41 (…) [stable]": never Cursor.
    // Cursor's own --version text is unconfirmed, so only a line naming cursor
    // is accepted; a Cursor build that prints a bare version is reported as
    // unrecognised until its output is known.
    const QString line = output.trimmed();
    if (line.contains(QLatin1String("grok"), Qt::CaseInsensitive))
        return {};
    if (!line.contains(QLatin1String("cursor"), Qt::CaseInsensitive))
        return {};
    return cua_cad::parse_cli_version(line);
}

std::optional<ProbeCommand> CursorAdapter::auth_command() const {
    return std::nullopt; // `agent status --format json` exists; its output shape is not documented
}

AuthStatus CursorAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    Q_UNUSED(err);
    return AuthStatus{};
}

AuthStatus CursorAdapter::auth_hint(const DetectContext& ctx) const {
    AuthStatus a;
    a.api_key_env_present = ctx.has_env(QString::fromLatin1(kCursorKeyVar));
    if (a.api_key_env_present)
        a.method = QStringLiteral("api_key");
    return a;
}

std::optional<ProbeCommand> CursorAdapter::models_command() const {
    return std::nullopt; // `agent models` exists; its output is not documented
}

QVector<ModelInfo> CursorAdapter::parse_models(int exit_code, const QByteArray& out) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    return {};
}

QVector<ModelInfo> CursorAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    return {}; // no documented list: "CLI default" or a typed model id
}

QStringList CursorAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return {};
}

LaunchPlan CursorAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    if (spec.chat) {
        plan.error = QStringLiteral("Chat calls through Cursor CLI are not available yet.");
        return plan;
    }
    if (spec.permission == PermissionMode::FullAccess) {
        plan.error = QStringLiteral("Full access is not available for Cursor CLI runs.");
        return plan;
    }
    if (spec.mcp) {
        plan.error = QStringLiteral("Cursor CLI has no per-run MCP option in print mode. Add the terminal's MCP server "
                                    "to Cursor under CONNECT, or run without terminal tools.");
        return plan;
    }
    if (!spec.effort.isEmpty()) {
        plan.error = QStringLiteral("Cursor CLI has no documented reasoning-effort option.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Cursor CLI has no turn limit option.");
        return plan;
    }
    if (!spec.json_schema.isEmpty()) {
        plan.error = QStringLiteral("A response schema is not available for Cursor CLI runs.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The prompt is empty.");
        return plan;
    }
    if (!spec.model.isEmpty() && !cua_sup::valid_model_id(spec.model)) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(60));
        return plan;
    }
    const bool resume = !spec.resume_session_id.isEmpty();
    if (resume && !cua_sup::valid_session_id(spec.resume_session_id)) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    const QString prompt = resume ? spec.prompt : cua_cad::wrap_codex_prompt(spec.system_prompt, spec.prompt);
    if (prompt.size() > cua_sup::kArgvTextBudgetChars) {
        plan.error = QStringLiteral("The prompt is too long for Cursor CLI's command line (%1 characters; at most %2).")
                         .arg(prompt.size())
                         .arg(cua_sup::kArgvTextBudgetChars);
        return plan;
    }
    plan.prompt_arg = cua_sup::argv_text(prompt);
    QStringList& args = plan.args;
    args << QStringLiteral("-p") << plan.prompt_arg << QStringLiteral("--output-format") << QStringLiteral("stream-json")
         << QStringLiteral("--trust");
    if (!spec.cwd.isEmpty())
        args << QStringLiteral("--workspace") << spec.cwd;
    if (!spec.model.isEmpty())
        args << QStringLiteral("--model") << spec.model;
    if (spec.permission == PermissionMode::ReadOnly)
        args << QStringLiteral("--mode") << QStringLiteral("ask");
    if (resume)
        args << QStringLiteral("--resume") << spec.resume_session_id;
    return plan;
}

LaunchPlan CursorAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(endpoint);
    Q_UNUSED(spec);
    LaunchPlan plan;
    plan.error = QStringLiteral("Cursor CLI has no per-run MCP option in print mode.");
    return plan;
}

std::unique_ptr<IStreamParser> CursorAdapter::make_parser() const {
    return std::make_unique<CursorStreamParser>();
}

CancelPlan CursorAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind CursorAdapter::classify_failure(int exit_code, const cua_cad::StreamFacts& facts,
                                            const QStringList& stderr_tail) const {
    // On an error the stream may end without a result event: success needs one.
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(agent login|not logged in|not authenticated|unauthori[sz]ed|\b401\b|invalid api key|cursor_api_key)"));
    return cua_sup::classify(exit_code, facts, stderr_tail, auth_rx, /*require_result=*/true);
}

std::optional<McpRegistration> CursorAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    if (!cua_sup::valid_mcp_endpoint(endpoint))
        return std::nullopt;
    McpRegistration r;
    r.server_name = endpoint.name;
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), endpoint.url},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"),
                      QStringLiteral("Bearer ") + QString::fromLatin1(kMcpTokenPlaceholder)}}},
    };
    r.config_snippet = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{endpoint.name, server}}}})
            .toJson(QJsonDocument::Indented));
    r.config_path_hint = QStringLiteral("~/.cursor/mcp.json");
    r.token_from_env = false;
    r.note = QStringLiteral("Merge this entry into \"mcpServers\" in Cursor's mcp.json (copying fills in the token) "
                            "and update it whenever the token changes.");
    return r;
}

} // namespace fincept::harness
