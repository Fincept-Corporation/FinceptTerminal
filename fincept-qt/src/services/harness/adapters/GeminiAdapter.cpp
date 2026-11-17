// GeminiAdapter.cpp — Gemini CLI as a harness (see GeminiAdapter.h). Only what
// the documentation states is used; QtCore only.

#include "services/harness/adapters/GeminiAdapter.h"

#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/AdapterSupport.h"
#include "services/harness/parsers/GeminiStreamParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::harness {

namespace gemini_adapter_detail {

namespace gma_cad = fincept::services::coding_agents;
namespace gma_sup = fincept::harness::adapter_support;

const char* const kGeminiKeyVar = "GEMINI_API_KEY";

} // namespace gemini_adapter_detail

using namespace gemini_adapter_detail;

GeminiAdapter::GeminiAdapter() {
    desc_.id = QStringLiteral("gemini");
    desc_.display_name = QStringLiteral("Gemini CLI");
    desc_.vendor = QStringLiteral("Google");
    desc_.install_url = QStringLiteral("https://geminicli.com/docs/");
    desc_.install_command = QStringLiteral("npm install -g @google/gemini-cli");
    desc_.docs_url = QStringLiteral("https://geminicli.com/docs/");
    desc_.signin_hint = QStringLiteral("Set GEMINI_API_KEY to a paid Gemini API key in the environment the terminal "
                                       "starts from. The terminal never uses Gemini CLI's Google sign-in: Google's "
                                       "terms do not allow third-party software to use it.");
    desc_.executable_names = {QStringLiteral("gemini")};
    desc_.transport = Transport::PrintJsonl;
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = true;  // message events with delta:true
    c.thinking = false;    // no thinking events in stream-json
    c.resume = true;       // -r <uuid>
    c.per_run_mcp = false; // settings.json only
    c.mcp_http = true;     // settings.json httpUrl
    c.enforced_read_only = false;
    c.model_list = false;  // no list command: built-in aliases
    c.effort = false;
    c.budget_usd = false;
    c.max_turns = false;
    c.json_schema = false;
    c.system_prompt = false;
    c.persistent_session = false;
    c.usage_tokens = true; // result.stats
    c.cost_usd = false;
    c.full_access = false;
    desc_.api_key_env_vars = {QString::fromLatin1(kGeminiKeyVar)};
    desc_.api_key_only = true;
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = true;
    desc_.beta_note = QStringLiteral("Written from the Gemini CLI documentation and source; not yet checked against a "
                                     "recorded run");
}

QStringList GeminiAdapter::candidate_paths(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // an npm / Homebrew install: the shared search list covers it
    return {};
}

ProbeCommand GeminiAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString GeminiAdapter::parse_version(const QString& output) const {
    // Gemini CLI prints its version through yargs: assumed to be a bare "0.x.y"
    // line (unverified here); a line naming gemini is accepted too. A bare
    // version cannot prove the identity, so the path override stays the way to
    // point the terminal at the right binary.
    return gma_sup::named_or_bare_version(output, QStringLiteral("gemini"),
                                          {QStringLiteral("grok"), QStringLiteral("claude"), QStringLiteral("codex"),
                                           QStringLiteral("qwen"), QStringLiteral("cursor"), QStringLiteral("copilot")});
}

std::optional<ProbeCommand> GeminiAdapter::auth_command() const {
    return std::nullopt; // no status command
}

AuthStatus GeminiAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    Q_UNUSED(err);
    return AuthStatus{};
}

AuthStatus GeminiAdapter::auth_hint(const DetectContext& ctx) const {
    AuthStatus a;
    a.api_key_env_present = ctx.has_env(QString::fromLatin1(kGeminiKeyVar));
    if (a.api_key_env_present) {
        a.method = QStringLiteral("api_key");
        a.note = QStringLiteral("GEMINI_API_KEY is set.");
    } else {
        a.note = QStringLiteral("GEMINI_API_KEY is not set — runs need a paid Gemini API key.");
    }
    return a;
}

std::optional<ProbeCommand> GeminiAdapter::models_command() const {
    return std::nullopt;
}

QVector<ModelInfo> GeminiAdapter::parse_models(int exit_code, const QByteArray& out) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    return {};
}

QVector<ModelInfo> GeminiAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    // The model aliases the documentation lists for -m.
    struct Alias {
        const char* id;
        const char* label;
    };
    static const Alias kAliases[] = {{"auto", "Auto"}, {"pro", "Pro"}, {"flash", "Flash"}, {"flash-lite", "Flash-Lite"}};
    QVector<ModelInfo> out;
    for (const Alias& a : kAliases) {
        ModelInfo m;
        m.id = QString::fromLatin1(a.id);
        m.label = QString::fromLatin1(a.label);
        out.append(m);
    }
    return out;
}

QStringList GeminiAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return {}; // no reasoning-effort flag
}

LaunchPlan GeminiAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    if (spec.chat) {
        plan.error = QStringLiteral("Chat calls through Gemini CLI are not available yet.");
        return plan;
    }
    if (!spec.api_key_env_present) {
        plan.error = QStringLiteral("Gemini CLI runs need a paid Gemini API key: set GEMINI_API_KEY. Google's terms do "
                                    "not allow third-party software to use Gemini CLI's Google sign-in, so the "
                                    "terminal never runs it that way.");
        return plan;
    }
    if (spec.permission == PermissionMode::FullAccess) {
        plan.error = QStringLiteral("Full access is not available for Gemini CLI runs.");
        return plan;
    }
    if (spec.mcp) {
        plan.error = QStringLiteral("Gemini CLI has no per-run MCP option. Add the terminal's MCP server to Gemini "
                                    "CLI's settings under CONNECT, or run without terminal tools.");
        return plan;
    }
    if (!spec.effort.isEmpty()) {
        plan.error = QStringLiteral("Gemini CLI has no reasoning-effort option.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Gemini CLI has no turn limit option.");
        return plan;
    }
    if (!spec.json_schema.isEmpty()) {
        plan.error = QStringLiteral("A response schema is not available for Gemini CLI runs.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The prompt is empty.");
        return plan;
    }
    if (!spec.model.isEmpty() && !gma_sup::valid_model_id(spec.model)) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(60));
        return plan;
    }
    const bool resume = !spec.resume_session_id.isEmpty();
    if (resume && !gma_sup::valid_uuid(spec.resume_session_id)) {
        // -r also takes "latest" or an index; only a session id is ever passed.
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    // No system-prompt flag: the context is wrapped into the first prompt only.
    const QString prompt = resume ? spec.prompt : gma_cad::wrap_codex_prompt(spec.system_prompt, spec.prompt);
    if (prompt.size() > gma_sup::kArgvTextBudgetChars) {
        plan.error = QStringLiteral("The prompt is too long for Gemini CLI's command line (%1 characters; at most %2).")
                         .arg(prompt.size())
                         .arg(gma_sup::kArgvTextBudgetChars);
        return plan;
    }
    plan.prompt_arg = gma_sup::argv_text(prompt);
    QStringList& args = plan.args;
    args << QStringLiteral("-p") << plan.prompt_arg << QStringLiteral("-o") << QStringLiteral("stream-json");
    // plan = read-only; auto_edit = edits approved, anything else denied ("ask"
    // is treated as deny in non-interactive mode).
    args << QStringLiteral("--approval-mode")
         << (spec.permission == PermissionMode::WorkspaceWrite ? QStringLiteral("auto_edit") : QStringLiteral("plan"));
    // Only the terminal's own MCP server may load (documented flag): never the
    // other Fincept edition's entries or anything else in the user's settings.
    args << QStringLiteral("--allowed-mcp-server-names") << QString::fromLatin1(kMcpServerName);
    if (!spec.model.isEmpty())
        args << QStringLiteral("-m") << spec.model;
    if (resume)
        args << QStringLiteral("-r") << spec.resume_session_id;
    return plan;
}

LaunchPlan GeminiAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(endpoint);
    Q_UNUSED(spec);
    LaunchPlan plan;
    plan.error = QStringLiteral("Gemini CLI has no per-run MCP option.");
    return plan;
}

std::unique_ptr<IStreamParser> GeminiAdapter::make_parser() const {
    return std::make_unique<GeminiStreamParser>();
}

CancelPlan GeminiAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind GeminiAdapter::classify_failure(int exit_code, const gma_cad::StreamFacts& facts,
                                            const QStringList& stderr_tail) const {
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(gemini_api_key|api key not valid|api_key_invalid|invalid api key|unauthenticated|unauthori[sz]ed|\b401\b|permission denied)"));
    return gma_sup::classify(exit_code, facts, stderr_tail, auth_rx, /*require_result=*/false);
}

std::optional<McpRegistration> GeminiAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    if (!gma_sup::valid_mcp_endpoint(endpoint))
        return std::nullopt;
    McpRegistration r;
    r.server_name = endpoint.name;
    r.manual_args = {QStringLiteral("mcp"),  QStringLiteral("add"),  QStringLiteral("--transport"),
                     QStringLiteral("http"), endpoint.name,          endpoint.url,
                     QStringLiteral("-H"),   gma_sup::placeholder_bearer_header(),
                     QStringLiteral("-s"),   QStringLiteral("user")};
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
    r.config_path_hint = QStringLiteral("~/.gemini/settings.json");
    r.token_from_env = false;
    r.note = QStringLiteral("Gemini CLI stores the token as a literal header, so the terminal does not add the server "
                            "for you. Run the command yourself, or merge the entry into \"mcpServers\" in your "
                            "settings.json (copying fills in the token); repeat it whenever the token changes.");
    return r;
}

} // namespace fincept::harness
