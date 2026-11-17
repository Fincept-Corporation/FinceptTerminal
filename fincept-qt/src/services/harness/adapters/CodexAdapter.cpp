// CodexAdapter.cpp — OpenAI Codex CLI as a harness (see CodexAdapter.h). QtCore only.

#include "services/harness/adapters/CodexAdapter.h"

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

#include <algorithm>
#include <climits>

namespace fincept::harness {

namespace codex_adapter_detail {

namespace cxd = fincept::services::coding_agents;

class CodexParser final : public IStreamParser {
  public:
    QVector<cxd::AgentEvent> parse_line(const QString& line) override { return parser_.parse_line(line); }
    const cxd::StreamFacts& facts() const override { return parser_.facts(); }

  private:
    cxd::CodexJsonParser parser_;
};

const QRegularExpression& codex_effort_rx() {
    static const QRegularExpression rx(QStringLiteral("^[a-z]{1,16}$"));
    return rx;
}

const QRegularExpression& codex_model_rx() {
    // Same alphabet the service accepts for a model name (no quotes, no spaces).
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z0-9._:/@\[\]-]{1,100}$)"));
    return rx;
}

struct BuiltinModel {
    const char* id;
    const char* label;
    bool ultra;
};

// `codex debug models` on codex-cli 0.160.0 (visible models), used when the
// list cannot be fetched.
constexpr BuiltinModel kCodexBuiltinModels[] = {
    {"gpt-6-luna", "GPT-6-Luna", false},
    {"gpt-5.6-terra", "GPT-5.6-Terra", true},
    {"gpt-5.6-luna", "GPT-5.6-Luna", false},
};

/// TOML basic string (same escaping as McpHttpServer::codex_config_toml()).
QString codex_toml_string(const QString& s) {
    QString out;
    out.reserve(s.size() + 2);
    out += QLatin1Char('"');
    for (const QChar c : s) {
        switch (c.unicode()) {
            case u'\\':
                out += QStringLiteral("\\\\");
                break;
            case u'"':
                out += QStringLiteral("\\\"");
                break;
            case u'\n':
                out += QStringLiteral("\\n");
                break;
            case u'\r':
                out += QStringLiteral("\\r");
                break;
            case u'\t':
                out += QStringLiteral("\\t");
                break;
            default:
                if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                    out += QStringLiteral("\\u%1").arg(static_cast<int>(c.unicode()), 4, 16, QLatin1Char('0'));
                else
                    out += c;
        }
    }
    out += QLatin1Char('"');
    return out;
}

/// The endpoint can go into a -c override, an argv and a TOML table header as it is.
bool codex_endpoint_ok(const McpEndpoint& endpoint) {
    // A name starting with '-' would be read as a flag by `codex mcp get <name>`.
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$"));
    static const QRegularExpression env_rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    static const QRegularExpression bad_url_rx(QStringLiteral(R"(["\\\s])"));
    return name_rx.match(endpoint.name).hasMatch() && !endpoint.url.isEmpty() && endpoint.url.size() <= 500 &&
           !endpoint.url.contains(bad_url_rx) && endpoint.url.startsWith(QLatin1String("http")) &&
           env_rx.match(endpoint.token_env_var).hasMatch();
}

QStringList builtin_efforts(bool ultra) {
    QStringList e = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"), QStringLiteral("xhigh"),
                     QStringLiteral("max")};
    if (ultra)
        e << QStringLiteral("ultra");
    return e;
}

} // namespace codex_adapter_detail

using namespace codex_adapter_detail;

CodexAdapter::CodexAdapter() {
    desc_.id = QStringLiteral("codex");
    desc_.display_name = QStringLiteral("Codex");
    desc_.vendor = QStringLiteral("OpenAI");
    desc_.install_url = QStringLiteral("https://developers.openai.com/codex/cli");
    desc_.install_command = QStringLiteral("npm install -g @openai/codex");
    desc_.docs_url = QStringLiteral("https://developers.openai.com/codex/cli");
    desc_.auth_docs_url = QStringLiteral("https://learn.chatgpt.com/docs/auth");
    desc_.signin_hint = QStringLiteral("Run `codex login` in a terminal.");
    desc_.signin_command = QStringLiteral("codex login");
    desc_.executable_names = {QStringLiteral("codex")};
    desc_.transport = Transport::PrintJsonl;
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = false;
    c.thinking = true;
    c.resume = true;
    c.per_run_mcp = true;
    c.mcp_http = true;
#ifdef Q_OS_WIN
    c.enforced_read_only = false; // depends on the Windows sandbox mode of the machine — unverified
#else
    c.enforced_read_only = true;
#endif
    c.model_list = true;
    c.effort = true;
    c.budget_usd = false;
    c.max_turns = false;
    c.json_schema = true;
    c.system_prompt = false;
    c.persistent_session = false;
    c.usage_tokens = true;
    c.cost_usd = false;
    c.full_access = true; // -s danger-full-access
    // Terminal-AI calls rely on the read-only sandbox; where it cannot be
    // verified without running a model (Windows) they are refused.
    c.chat = c.enforced_read_only;
    desc_.api_key_env_vars = {QStringLiteral("OPENAI_API_KEY"), QStringLiteral("CODEX_API_KEY")};
    // DOCS (non-interactive mode): `codex exec` authenticates with CODEX_API_KEY when it is set.
    desc_.terminal_key_env_vars = {QStringLiteral("CODEX_API_KEY")};
    desc_.api_key_docs_url = QStringLiteral("https://learn.chatgpt.com/docs/auth");
    // A user's own `[mcp_servers.fincept]` entry (e.g. the Enterprise server, or
    // the stdio bridge from the CONNECT tab) must not merge with ours.
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = false;
    desc_.tested_version = QStringLiteral("0.160.0");
}

QStringList CodexAdapter::candidate_paths(const DetectContext& ctx) const {
    if (!ctx.windows)
        return {};
    const QString local = ctx.env_value(QStringLiteral("LOCALAPPDATA")).trimmed();
    if (local.isEmpty())
        return {};
    return {local + QStringLiteral("/Programs/OpenAI/Codex/bin/codex.exe")}; // the official installer
}

ProbeCommand CodexAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString CodexAdapter::parse_version(const QString& output) const {
    // "codex-cli 0.160.0"
    if (!output.contains(QLatin1String("codex"), Qt::CaseInsensitive))
        return {};
    return cxd::parse_cli_version(output);
}

std::optional<ProbeCommand> CodexAdapter::auth_command() const {
    return ProbeCommand{{QStringLiteral("login"), QStringLiteral("status")}, 20000};
}

AuthStatus CodexAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    // The answer is on stderr ("Logged in using ChatGPT"). An API-key answer can
    // carry a masked key: only the phrases below are looked for and nothing of
    // the text is kept.
    const QString text = (QString::fromUtf8(out) + QLatin1Char('\n') + QString::fromUtf8(err)).toLower();
    AuthStatus a;
    if (text.contains(QLatin1String("not logged in")) || text.contains(QLatin1String("logged out"))) {
        a.state = AuthState::SignedOut;
    } else if (text.contains(QLatin1String("logged in"))) {
        a.state = AuthState::SignedIn;
        if (text.contains(QLatin1String("chatgpt")))
            a.method = QStringLiteral("subscription");
        else if (text.contains(QLatin1String("api key")))
            a.method = QStringLiteral("api_key");
        else if (text.contains(QLatin1String("access token")))
            a.method = QStringLiteral("env");
    } else {
        a.state = exit_code == 0 ? AuthState::Unknown : AuthState::Error;
        a.note = exit_code == 0 ? QStringLiteral("The sign-in status could not be read.")
                                : QStringLiteral("`codex login status` exited with code %1.").arg(exit_code);
    }
    return a;
}

AuthStatus CodexAdapter::auth_hint(const DetectContext& ctx) const {
    AuthStatus a;
    a.api_key_env_present = ctx.has_env(QStringLiteral("OPENAI_API_KEY")) || ctx.has_env(QStringLiteral("CODEX_API_KEY"));
    if (a.api_key_env_present)
        a.method = QStringLiteral("api_key");
    // A saved sign-in — existence only, the file is never opened.
    QString dir = ctx.env_value(QStringLiteral("CODEX_HOME")).trimmed();
    if (dir.isEmpty() && !ctx.home.isEmpty())
        dir = ctx.home + QStringLiteral("/.codex");
    if (!dir.isEmpty() && ctx.is_file && ctx.is_file(dir + QStringLiteral("/auth.json")))
        a.note = QStringLiteral("A saved sign-in was found.");
    return a;
}

std::optional<ProbeCommand> CodexAdapter::models_command() const {
    return ProbeCommand{{QStringLiteral("debug"), QStringLiteral("models")}, 20000};
}

QVector<ModelInfo> CodexAdapter::parse_models(int exit_code, const QByteArray& out) const {
    if (exit_code != 0)
        return {};
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(out, &pe);
    if (!doc.isObject())
        return {};
    const QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();
    struct Ranked {
        int priority;
        int index;
        ModelInfo info;
    };
    QVector<Ranked> ranked;
    for (int i = 0; i < models.size(); ++i) {
        const QJsonObject o = models.at(i).toObject();
        const QString slug = o.value(QStringLiteral("slug")).toString().trimmed();
        if (!codex_model_rx().match(slug).hasMatch())
            continue;
        const QJsonValue visibility = o.value(QStringLiteral("visibility"));
        if (visibility.isString() && visibility.toString() != QLatin1String("list"))
            continue; // "hide" = internal helper models
        ModelInfo m;
        m.id = slug;
        m.label = o.value(QStringLiteral("display_name")).toString().trimmed().left(80);
        if (m.label.isEmpty())
            m.label = slug;
        m.description = o.value(QStringLiteral("description")).toString().trimmed().left(200);
        const QJsonArray levels = o.value(QStringLiteral("supported_reasoning_levels")).toArray();
        for (const QJsonValue& lv : levels) {
            const QString e = lv.isObject() ? lv.toObject().value(QStringLiteral("effort")).toString() : lv.toString();
            if (codex_effort_rx().match(e).hasMatch() && !m.efforts.contains(e))
                m.efforts << e;
        }
        const QString def = o.value(QStringLiteral("default_reasoning_level")).toString();
        if (codex_effort_rx().match(def).hasMatch())
            m.default_effort = def;
        const double ctx_window = o.value(QStringLiteral("context_window")).toDouble();
        m.context_window = ctx_window > 0 ? static_cast<qint64>(ctx_window) : 0;
        const QJsonValue prio = o.value(QStringLiteral("priority"));
        ranked.append(Ranked{prio.isDouble() ? prio.toInt() : INT_MAX, i, m});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        return a.priority != b.priority ? a.priority < b.priority : a.index < b.index;
    });
    QVector<ModelInfo> list;
    list.reserve(ranked.size());
    for (const Ranked& r : std::as_const(ranked))
        list.append(r.info);
    return list;
}

QVector<ModelInfo> CodexAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    QVector<ModelInfo> out;
    for (const BuiltinModel& b : kCodexBuiltinModels) {
        ModelInfo m;
        m.id = QString::fromLatin1(b.id);
        m.label = QString::fromLatin1(b.label);
        m.efforts = builtin_efforts(b.ultra);
        m.default_effort = QStringLiteral("medium");
        out.append(m);
    }
    return out;
}

QStringList CodexAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(version);
    for (const BuiltinModel& b : kCodexBuiltinModels) {
        if (model == QLatin1String(b.id))
            return builtin_efforts(b.ultra);
    }
    // Unknown model: the levels every reasoning model accepts.
    return {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high")};
}

LaunchPlan CodexAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    static const QRegularExpression session_rx(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    const QString& session = spec.resume_session_id;
    const bool resume = !session.isEmpty();
    if (spec.chat)
        return build_chat_command(spec, inst);
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Codex has no turn limit option.");
        return plan;
    }
    if (!spec.effort.isEmpty() && !codex_effort_rx().match(spec.effort).hasMatch()) {
        plan.error = QStringLiteral("The reasoning effort “%1” is not valid.").arg(spec.effort.left(40));
        return plan;
    }
    if (resume && (!session_rx.match(session).hasMatch() || session.startsWith(QLatin1Char('-')))) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    if (!spec.json_schema.isEmpty() && (resume || spec.temp_dir.isEmpty())) {
        plan.error = QStringLiteral("A response schema needs a new Codex session.");
        return plan;
    }

    const QString sandbox = spec.permission == PermissionMode::FullAccess       ? QStringLiteral("danger-full-access")
                            : spec.permission == PermissionMode::WorkspaceWrite ? QStringLiteral("workspace-write")
                                                                                : QStringLiteral("read-only");
    QStringList& args = plan.args;
    args << QStringLiteral("exec");
    if (resume)
        args << QStringLiteral("resume");
    args << QStringLiteral("--json") << QStringLiteral("--skip-git-repo-check");
    if (resume) // `exec resume` has no -s/-C; the sandbox goes through config
        args << QStringLiteral("-c") << QStringLiteral("sandbox_mode=\"%1\"").arg(sandbox);
    else
        args << QStringLiteral("-s") << sandbox;
    args << QStringLiteral("-c") << QStringLiteral("approval_policy=\"never\"");
    if (!spec.model.isEmpty())
        args << QStringLiteral("-m") << spec.model;
    if (!spec.effort.isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("model_reasoning_effort=\"%1\"").arg(spec.effort);
    if (spec.mcp) {
        const LaunchPlan mcp = mcp_config_for_run(*spec.mcp, spec);
        if (!mcp.error.isEmpty()) {
            plan.error = mcp.error;
            return plan;
        }
        args << mcp.args;
        plan.temp_files << mcp.temp_files;
    }
    // The terminal's MCP server only (Claude Code: --strict-mcp-config). Codex
    // merges -c servers into the user's table, so every other entry of the
    // user's config.toml — the private edition's, a pre-split "fincept_terminal",
    // third-party ones — is switched off by name for this run. Without terminal
    // tools the run gets no MCP server at all, the same as Claude.
    {
        static const QRegularExpression other_rx(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
        const QString own = spec.mcp ? spec.mcp->name : QString();
        QStringList seen;
        for (const QString& n : spec.user_mcp_server_names) {
            if (n == own || seen.contains(n) || !other_rx.match(n).hasMatch())
                continue;
            seen << n;
            args << QStringLiteral("-c") << QStringLiteral("mcp_servers.%1.enabled=false").arg(n);
        }
    }
    if (!spec.json_schema.isEmpty()) {
        TempFile schema;
        schema.name = QStringLiteral("schema-") + spec.run_id.left(8) + QStringLiteral(".json");
        schema.content = spec.json_schema.toUtf8();
        args << QStringLiteral("--output-schema") << spec.temp_dir + QLatin1Char('/') + schema.name;
        plan.temp_files << schema;
    }
    if (resume)
        args << session;
    args << QStringLiteral("-"); // prompt from stdin
    // No system-prompt flag: the context is wrapped into the first prompt only
    // (the resumed session already has it).
    plan.stdin_payload = (resume ? spec.prompt : cxd::wrap_codex_prompt(spec.system_prompt, spec.prompt)).toUtf8();
    return plan;
}

LaunchPlan CodexAdapter::build_chat_command(const RunSpec& spec, const Installation& inst) const {
    Q_UNUSED(inst);
    LaunchPlan plan;
    if (!desc_.caps.chat) {
        plan.error = QStringLiteral("Codex's read-only sandbox cannot be verified on this system, so it is not used for "
                                    "terminal AI here (it stays available in the Agent Workbench).");
        return plan;
    }
    if (spec.permission != PermissionMode::ReadOnly) {
        plan.error = QStringLiteral("Terminal AI calls are read-only.");
        return plan;
    }
    if (!spec.resume_session_id.isEmpty()) {
        // `exec resume` has no -s and would load the user's config again: a
        // terminal-AI session is never resumed.
        plan.error = QStringLiteral("Codex terminal-AI sessions cannot be resumed.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Codex has no turn limit option.");
        return plan;
    }
    if (!spec.model.isEmpty() && (!codex_model_rx().match(spec.model).hasMatch() || spec.model.startsWith(QLatin1Char('-')))) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(40));
        return plan;
    }
    if (!spec.effort.isEmpty() && !codex_effort_rx().match(spec.effort).hasMatch()) {
        plan.error = QStringLiteral("The reasoning effort “%1” is not valid.").arg(spec.effort.left(40));
        return plan;
    }
    if (!spec.json_schema.isEmpty() && spec.temp_dir.isEmpty()) {
        plan.error = QStringLiteral("No folder was given for the response schema.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The message is empty.");
        return plan;
    }

    QStringList& args = plan.args;
    args << QStringLiteral("exec") << QStringLiteral("--json");
    if (!spec.chat_persist_session)
        args << QStringLiteral("--ephemeral"); // no rollout files: nothing lands in the user's session list
    // None of the user's config.toml (other MCP servers, profiles, a looser
    // sandbox): the read-only sandbox and the terminal's MCP server only.
    args << QStringLiteral("--ignore-user-config") << QStringLiteral("--skip-git-repo-check") << QStringLiteral("-s")
         << QStringLiteral("read-only") << QStringLiteral("-c") << QStringLiteral("approval_policy=\"never\"")
         << QStringLiteral("-c") << QStringLiteral("web_search=\"disabled\"");
    if (!spec.model.isEmpty())
        args << QStringLiteral("-m") << spec.model;
    if (!spec.effort.isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("model_reasoning_effort=\"%1\"").arg(spec.effort);
    if (spec.mcp) {
        const LaunchPlan mcp = mcp_config_for_run(*spec.mcp, spec);
        if (!mcp.error.isEmpty()) {
            plan.error = mcp.error;
            return plan;
        }
        args << mcp.args;
        plan.temp_files << mcp.temp_files;
    }
    if (!spec.json_schema.isEmpty()) {
        TempFile schema;
        schema.name = QStringLiteral("schema-") + spec.run_id.left(8) + QStringLiteral(".json");
        schema.content = spec.json_schema.toUtf8();
        args << QStringLiteral("--output-schema") << spec.temp_dir + QLatin1Char('/') + schema.name;
        plan.temp_files << schema;
    }
    args << QStringLiteral("-"); // prompt from stdin
    plan.env_unset = desc_.terminal_unset_env_vars;
    plan.stdin_payload = cxd::wrap_codex_prompt(spec.system_prompt, spec.prompt).toUtf8();
    return plan;
}

LaunchPlan CodexAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(spec);
    LaunchPlan plan;
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    static const QRegularExpression env_rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    static const QRegularExpression bad_url_rx(QStringLiteral(R"(["\\\r\n])"));
    if (!name_rx.match(endpoint.name).hasMatch() || endpoint.url.isEmpty()) {
        plan.error = QStringLiteral("The terminal's MCP server returned no Codex configuration.");
        return plan;
    }
    if (endpoint.url.contains(bad_url_rx) || !env_rx.match(endpoint.token_env_var).hasMatch()) {
        plan.error = QStringLiteral("The terminal's MCP server address is not valid.");
        return plan;
    }
    // TOML values through -c; the token stays in the environment variable named here.
    plan.args << QStringLiteral("-c") << QStringLiteral("mcp_servers.%1.url=\"%2\"").arg(endpoint.name, endpoint.url)
              << QStringLiteral("-c")
              << QStringLiteral("mcp_servers.%1.bearer_token_env_var=\"%2\"").arg(endpoint.name, endpoint.token_env_var)
              << QStringLiteral("-c")
              << QStringLiteral("mcp_servers.%1.tool_timeout_sec=%2")
                     .arg(endpoint.name, QString::number(qMax(1, endpoint.tool_timeout_s)));
    return plan;
}

std::unique_ptr<IStreamParser> CodexAdapter::make_parser() const {
    return std::make_unique<CodexParser>();
}

CancelPlan CodexAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind CodexAdapter::classify_failure(int exit_code, const cxd::StreamFacts& facts,
                                           const QStringList& stderr_tail) const {
    if (exit_code == 0 && !facts.result_is_error && (facts.saw_result || !facts.final_text.isEmpty()))
        return FailureKind::None;
    const QString text = (facts.result_error + QLatin1Char('\n') + facts.last_error + QLatin1Char('\n') +
                          stderr_tail.join(QLatin1Char('\n')))
                             .toLower();
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(not logged in|please (log|sign) ?in|codex login|unauthori[sz]ed|\b401\b|invalid api key|token (has )?expired)"));
    static const QRegularExpression rate_rx(
        QStringLiteral(R"(rate.?limit|\b429\b|usage limit|quota|too many requests)"));
    static const QRegularExpression model_rx(QStringLiteral(
        R"(model[^\n]{0,80}(not found|not supported|does not exist|is not available)|unknown model|invalid model)"));
    if (auth_rx.match(text).hasMatch())
        return FailureKind::Auth;
    if (rate_rx.match(text).hasMatch())
        return FailureKind::RateLimit;
    if (model_rx.match(text).hasMatch())
        return FailureKind::Model;
    return FailureKind::Other;
}

std::optional<McpRegistration> CodexAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    if (!codex_endpoint_ok(endpoint))
        return std::nullopt;
    const QString& name = endpoint.name;
    McpRegistration r;
    r.server_name = name;
    r.check = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("get"), name}, 20000};
    r.remove = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("remove"), name}, 20000};
    // --bearer-token-env-var (codex-cli 0.160.0 `mcp add --help`): the config
    // stores the variable's NAME; Codex reads the token from it when it connects.
    r.add = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("add"), name, QStringLiteral("--url"), endpoint.url,
                          QStringLiteral("--bearer-token-env-var"), endpoint.token_env_var},
                         20000};
    r.token_from_env = true;
    r.manual_args = r.add->args;
    // tool_timeout_sec: an approval or a ui_wait can outlast Codex's 60 s default.
    r.config_snippet = QStringLiteral("[mcp_servers.%1]\nurl = %2\nbearer_token_env_var = %3\ntool_timeout_sec = %4\n")
                           .arg(name, codex_toml_string(endpoint.url), codex_toml_string(endpoint.token_env_var),
                                QString::number(qMax(1, endpoint.tool_timeout_s)));
    r.config_path_hint = QStringLiteral("~/.codex/config.toml");
    r.note = QStringLiteral("Codex reads the token from the %1 environment variable when it connects, so that "
                            "variable must be set in the environment you start Codex from (the terminal sets it only "
                            "for the runs it launches itself).")
                 .arg(endpoint.token_env_var);
    return r;
}

namespace codex_adapter_detail {

/// "#" comment removed (outside quoted strings).
QString cfg_strip_comment(const QString& line) {
    QChar quote;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (!quote.isNull()) {
            if (c == QLatin1Char('\\') && quote == QLatin1Char('"')) {
                ++i;
                continue;
            }
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('#')) {
            return line.left(i);
        }
    }
    return line;
}

/// A TOML key path ("a.\"b.c\".d") split into unquoted segments.
QStringList cfg_split_key(const QString& key) {
    QStringList out;
    QString cur;
    QChar quote;
    for (const QChar c : key) {
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            else
                cur += c;
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('.')) {
            out << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur.trimmed();
    return out;
}

/// Index of the first '=' outside quotes, -1 if none.
qsizetype cfg_assignment(const QString& line) {
    QChar quote;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('=')) {
            return i;
        }
    }
    return -1;
}

/// Brace depth change of `text` (braces inside quotes ignored).
int cfg_brace_delta(const QString& text) {
    int d = 0;
    QChar quote;
    for (const QChar c : text) {
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('{')) {
            ++d;
        } else if (c == QLatin1Char('}')) {
            --d;
        }
    }
    return d;
}

/// Keys at depth 1 of an inline table text "{ a = {…}, \"b\" = {…} }".
QStringList cfg_inline_keys(const QString& text) {
    QStringList keys;
    int depth = 0;
    QChar quote;
    QString cur;
    bool reading_key = false;
    for (const QChar c : text) {
        if (!quote.isNull()) {
            if (depth == 1 && reading_key)
                cur += c;
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
            if (depth == 1 && reading_key)
                cur += c;
            continue;
        }
        if (c == QLatin1Char('{') || c == QLatin1Char('[')) {
            ++depth;
            if (depth == 1) {
                reading_key = true;
                cur.clear();
            }
            continue;
        }
        if (c == QLatin1Char('}') || c == QLatin1Char(']')) {
            --depth;
            continue;
        }
        if (depth != 1)
            continue;
        if (c == QLatin1Char(',')) {
            reading_key = true;
            cur.clear();
        } else if (c == QLatin1Char('=') && reading_key) {
            keys << cfg_split_key(cur).value(0);
            reading_key = false;
        } else if (reading_key) {
            cur += c;
        }
    }
    return keys;
}

} // namespace codex_adapter_detail

QStringList CodexAdapter::parse_mcp_server_names(const QByteArray& config_toml) {
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    QStringList names;
    auto add = [&names](const QString& raw) {
        const QString n = raw.trimmed();
        if (name_rx.match(n).hasMatch() && !names.contains(n))
            names << n;
    };
    const QStringList lines = QString::fromUtf8(config_toml.left(1024 * 1024)).split(QLatin1Char('\n'));
    QStringList table; // current [table] path
    QString inline_text;
    int inline_depth = 0;
    for (const QString& raw : lines) {
        const QString line = cfg_strip_comment(raw).trimmed();
        if (line.isEmpty())
            continue;
        if (inline_depth > 0) { // a multi-line inline `mcp_servers = { … }`
            inline_text += QLatin1Char(' ') + line;
            inline_depth += cfg_brace_delta(line);
            if (inline_depth <= 0) {
                for (const QString& k : cfg_inline_keys(inline_text))
                    add(k);
                inline_text.clear();
            }
            continue;
        }
        if (line.startsWith(QLatin1Char('['))) {
            QString inner = line;
            while (inner.startsWith(QLatin1Char('[')))
                inner.remove(0, 1);
            const qsizetype close = inner.lastIndexOf(QLatin1Char(']'));
            if (close >= 0)
                inner.truncate(close);
            while (inner.endsWith(QLatin1Char(']')))
                inner.chop(1);
            table = cfg_split_key(inner);
            if (table.size() >= 2 && table.at(0) == QLatin1String("mcp_servers"))
                add(table.at(1));
            continue;
        }
        const qsizetype eq = cfg_assignment(line);
        if (eq < 0)
            continue;
        const QStringList full = table + cfg_split_key(line.left(eq));
        if (full.size() >= 2 && full.at(0) == QLatin1String("mcp_servers")) {
            add(full.at(1));
            continue;
        }
        if (full.size() == 1 && full.at(0) == QLatin1String("mcp_servers")) {
            inline_text = line.mid(eq + 1);
            inline_depth = cfg_brace_delta(inline_text);
            if (inline_depth <= 0) {
                for (const QString& k : cfg_inline_keys(inline_text))
                    add(k);
                inline_text.clear();
                inline_depth = 0;
            }
        }
    }
    return names;
}

QStringList CodexAdapter::user_mcp_server_names(const DetectContext& ctx) const {
    if (!ctx.read_file)
        return {};
    QString dir = ctx.env_value(QStringLiteral("CODEX_HOME")).trimmed();
    if (dir.isEmpty()) {
        if (ctx.home.isEmpty())
            return {};
        dir = ctx.home + QStringLiteral("/.codex");
    }
    // Names only: the parser keeps nothing but [mcp_servers.<name>] keys.
    return parse_mcp_server_names(ctx.read_file(dir + QStringLiteral("/config.toml"), 1024 * 1024));
}

} // namespace fincept::harness
