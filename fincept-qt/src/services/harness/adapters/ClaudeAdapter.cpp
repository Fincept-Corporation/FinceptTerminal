// ClaudeAdapter.cpp — Claude Code as a harness (see ClaudeAdapter.h). QtCore only.

#include "services/harness/adapters/ClaudeAdapter.h"

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace fincept::harness {

namespace claude_adapter_detail {

namespace cad = fincept::services::coding_agents;

class ClaudeParser final : public IStreamParser {
  public:
    QVector<cad::AgentEvent> parse_line(const QString& line) override { return parser_.parse_line(line); }
    QVector<cad::AgentEvent> finish() override { return parser_.finish(); }
    const cad::StreamFacts& facts() const override { return parser_.facts(); }

  private:
    cad::ClaudeStreamParser parser_;
};

/// The endpoint can be put on a command line / into a config entry as it is:
/// a plain server name, a URL without quotes, backslashes, spaces or line
/// breaks, and a valid environment-variable name.
bool claude_endpoint_ok(const McpEndpoint& endpoint) {
    // A name starting with '-' would be read as a flag by `claude mcp get <name>`.
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$"));
    static const QRegularExpression env_rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    static const QRegularExpression url_rx(QStringLiteral(R"(^https?://[^\s"'\\]{1,500}$)"));
    return name_rx.match(endpoint.name).hasMatch() && url_rx.match(endpoint.url).hasMatch() &&
           env_rx.match(endpoint.token_env_var).hasMatch();
}

const QStringList& claude_efforts() {
    static const QStringList kEfforts = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"),
                                         QStringLiteral("xhigh"), QStringLiteral("max")};
    return kEfforts;
}

/// A short status value that is safe to show and cache. Anything longer or
/// with unusual characters is dropped rather than risk carrying identity.
QString safe_status_token(const QString& value) {
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z0-9 ._:+-]{1,40}$)"));
    const QString t = value.trimmed();
    return rx.match(t).hasMatch() ? t : QString();
}

/// A boolean switch variable (not a credential) is on.
bool switch_on(const DetectContext& ctx, const char* name) {
    const QString v = ctx.env_value(QString::fromLatin1(name)).trimmed().toLower();
    return !v.isEmpty() && v != QLatin1String("0") && v != QLatin1String("false");
}

QJsonObject json_object_in(const QByteArray& bytes) {
    const QByteArray trimmed = bytes.trimmed();
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(trimmed, &pe);
    if (!doc.isObject()) {
        // Tolerate a warning line before or after the JSON document.
        const qsizetype b = trimmed.indexOf('{');
        const qsizetype e = trimmed.lastIndexOf('}');
        if (b >= 0 && e > b)
            doc = QJsonDocument::fromJson(trimmed.mid(b, e - b + 1), &pe);
    }
    return doc.isObject() ? doc.object() : QJsonObject();
}

} // namespace claude_adapter_detail

using namespace claude_adapter_detail;

ClaudeAdapter::ClaudeAdapter() {
    desc_.id = QStringLiteral("claude");
    desc_.display_name = QStringLiteral("Claude Code");
    desc_.vendor = QStringLiteral("Anthropic");
    desc_.install_url = QStringLiteral("https://code.claude.com/docs/en/setup");
#ifdef Q_OS_WIN
    desc_.install_command = QStringLiteral("winget install Anthropic.ClaudeCode");
#else
    desc_.install_command = QStringLiteral("npm install -g @anthropic-ai/claude-code");
#endif
    desc_.docs_url = QStringLiteral("https://code.claude.com/docs/en/headless");
    desc_.auth_docs_url = QStringLiteral("https://code.claude.com/docs/en/authentication");
    desc_.signin_hint = QStringLiteral("Run `claude` in a terminal and sign in, or `claude auth login`.");
    desc_.signin_command = QStringLiteral("claude auth login");
    desc_.executable_names = {QStringLiteral("claude")};
    desc_.transport = Transport::PrintJsonl;
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = true;
    c.thinking = true;
    c.resume = true;
    c.per_run_mcp = true;
    c.mcp_http = true;
    c.enforced_read_only = true; // read-only = built-in write/exec tools removed (no OS sandbox on Windows)
    c.model_list = false;
    c.effort = true;
    c.budget_usd = true;
    c.max_turns = false; // not offered by `claude --help` (2.1.325)
    c.json_schema = true;
    c.system_prompt = true;
    c.persistent_session = false;
    c.usage_tokens = true;
    c.cost_usd = true;
    c.full_access = true; // --permission-mode bypassPermissions
    c.chat = true;        // --restricted --tools "" (no built-in tool), the terminal's MCP server only
    desc_.api_key_env_vars = {QStringLiteral("ANTHROPIC_API_KEY"), QStringLiteral("ANTHROPIC_AUTH_TOKEN")};
    // DOCS (headless): in -p mode the key is always used when present.
    desc_.terminal_key_env_vars = desc_.api_key_env_vars;
    desc_.terminal_unset_env_vars = {QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN")};
    desc_.api_key_docs_url = QStringLiteral("https://code.claude.com/docs/en/authentication");
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = false;
    desc_.tested_version = QStringLiteral("2.1.325");
}

QStringList ClaudeAdapter::candidate_paths(const DetectContext& ctx) const {
    if (ctx.windows || ctx.home.isEmpty())
        return {}; // Windows locations are all in the shared list
    return {ctx.home + QStringLiteral("/.claude/local/claude")}; // the old "local" npm install
}

ProbeCommand ClaudeAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString ClaudeAdapter::parse_version(const QString& output) const {
    // "2.1.325 (Claude Code)"
    if (!output.contains(QLatin1String("claude"), Qt::CaseInsensitive))
        return {};
    return cad::parse_cli_version(output);
}

std::optional<ProbeCommand> ClaudeAdapter::auth_command() const {
    return ProbeCommand{{QStringLiteral("auth"), QStringLiteral("status"), QStringLiteral("--json")}, 20000};
}

AuthStatus ClaudeAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    Q_UNUSED(err); // may carry identity; never looked at
    AuthStatus a;
    // Only the four named fields are read; e-mail / organisation fields are
    // never touched and the document dies with this scope.
    const QJsonObject o = json_object_in(out);
    if (o.isEmpty()) {
        a.state = exit_code == 0 ? AuthState::Unknown : AuthState::Error;
        a.note = exit_code == 0 ? QStringLiteral("The sign-in status could not be read.")
                                : QStringLiteral("`claude auth status` exited with code %1.").arg(exit_code);
        return a;
    }
    const QJsonValue logged_in = o.value(QStringLiteral("loggedIn"));
    if (logged_in.isBool())
        a.state = logged_in.toBool() ? AuthState::SignedIn : AuthState::SignedOut;
    else
        a.state = exit_code == 0 ? AuthState::Unknown : AuthState::Error;
    if (a.state == AuthState::SignedOut)
        return a;

    const QString method = o.value(QStringLiteral("authMethod")).toString().trimmed().toLower();
    const QString provider = o.value(QStringLiteral("apiProvider")).toString().trimmed();
    const QString provider_l = provider.toLower();
    const bool first_party = provider_l.isEmpty() || provider_l == QLatin1String("firstparty") ||
                             provider_l == QLatin1String("first_party") || provider_l == QLatin1String("anthropic");
    if (!first_party) {
        a.method = QStringLiteral("cloud");
        a.note = safe_status_token(provider);
    } else if (method.contains(QLatin1String("claude.ai")) || method.contains(QLatin1String("oauth")) ||
               method.contains(QLatin1String("subscription"))) {
        a.method = QStringLiteral("subscription");
    } else if (method.contains(QLatin1String("api")) || method.contains(QLatin1String("key")) ||
               method.contains(QLatin1String("console"))) {
        a.method = QStringLiteral("api_key");
    } else if (method.contains(QLatin1String("token")) || method.contains(QLatin1String("env"))) {
        a.method = QStringLiteral("env");
    }
    a.plan = safe_status_token(o.value(QStringLiteral("subscriptionType")).toString());
    if (a.method.isEmpty() && !a.plan.isEmpty())
        a.method = QStringLiteral("subscription");
    return a;
}

AuthStatus ClaudeAdapter::auth_hint(const DetectContext& ctx) const {
    AuthStatus a;
    a.api_key_env_present =
        ctx.has_env(QStringLiteral("ANTHROPIC_API_KEY")) || ctx.has_env(QStringLiteral("ANTHROPIC_AUTH_TOKEN"));
    if (switch_on(ctx, "CLAUDE_CODE_USE_BEDROCK")) {
        a.method = QStringLiteral("cloud");
        a.note = QStringLiteral("bedrock");
    } else if (switch_on(ctx, "CLAUDE_CODE_USE_VERTEX")) {
        a.method = QStringLiteral("cloud");
        a.note = QStringLiteral("vertex");
    } else if (switch_on(ctx, "CLAUDE_CODE_USE_FOUNDRY")) {
        a.method = QStringLiteral("cloud");
        a.note = QStringLiteral("foundry");
    } else if (a.api_key_env_present) {
        a.method = QStringLiteral("api_key");
    } else if (ctx.has_env(QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN"))) {
        a.method = QStringLiteral("env");
    }
    // A saved sign-in — existence only, the file is never opened.
    QString dir = ctx.env_value(QStringLiteral("CLAUDE_CONFIG_DIR")).trimmed();
    if (dir.isEmpty() && !ctx.home.isEmpty())
        dir = ctx.home + QStringLiteral("/.claude");
    if (!dir.isEmpty() && ctx.is_file && ctx.is_file(dir + QStringLiteral("/.credentials.json")) && a.note.isEmpty())
        a.note = QStringLiteral("A saved sign-in was found.");
    return a;
}

std::optional<ProbeCommand> ClaudeAdapter::models_command() const {
    return std::nullopt; // no list command; see builtin_models()
}

QVector<ModelInfo> ClaudeAdapter::parse_models(int exit_code, const QByteArray& out) const {
    Q_UNUSED(exit_code);
    Q_UNUSED(out);
    return {};
}

QVector<ModelInfo> ClaudeAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version);
    struct Alias {
        const char* id;
        const char* label;
        const char* description;
    };
    static const Alias kAliases[] = {
        {"opus", "Opus", "Latest Opus model"},
        {"sonnet", "Sonnet", "Latest Sonnet model"},
        {"haiku", "Haiku", "Latest Haiku model"},
        {"fable", "Fable", "Latest Fable model"},
        {"opusplan", "Opus plan", "Opus while planning, Sonnet while executing"},
        {"sonnet[1m]", "Sonnet 1M", "Sonnet with the 1M-token context window"},
        {"opus[1m]", "Opus 1M", "Opus with the 1M-token context window"},
        {"best", "Best", "The most capable model available to the account"},
    };
    QVector<ModelInfo> out;
    for (const Alias& a : kAliases) {
        ModelInfo m;
        m.id = QString::fromLatin1(a.id);
        m.label = QString::fromLatin1(a.label);
        m.description = QString::fromLatin1(a.description);
        m.efforts = claude_efforts();
        out.append(m);
    }
    return out;
}

QStringList ClaudeAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return claude_efforts(); // `--effort low|medium|high|xhigh|max`
}

LaunchPlan ClaudeAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    LaunchPlan plan;
    static const QRegularExpression effort_rx(QStringLiteral("^[a-z]{1,16}$"));
    static const QRegularExpression session_rx(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    const QString& resume = spec.resume_session_id;
    if (spec.chat)
        return build_chat_command(spec, inst);
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Claude Code has no turn limit option.");
        return plan;
    }
    const bool full_access = spec.permission == PermissionMode::FullAccess;
    if (!spec.effort.isEmpty() && !effort_rx.match(spec.effort).hasMatch()) {
        plan.error = QStringLiteral("The reasoning effort “%1” is not valid.").arg(spec.effort.left(40));
        return plan;
    }
    if (!resume.isEmpty() && (!session_rx.match(resume).hasMatch() || resume.startsWith(QLatin1Char('-')))) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }

    QStringList& args = plan.args;
    args << QStringLiteral("-p") << QStringLiteral("--output-format") << QStringLiteral("stream-json")
         << QStringLiteral("--verbose");
    if (spec.want_deltas) // stream_event lines → partial text (needs stream-json + --verbose)
        args << QStringLiteral("--include-partial-messages");
    if (full_access) {
        // Every tool runs without asking (nothing is ever prompted for).
        args << QStringLiteral("--permission-mode") << QStringLiteral("bypassPermissions");
    } else {
        // Unattended: anything not pre-approved below is denied instead of waiting on a prompt.
        args << QStringLiteral("--permission-mode") << QStringLiteral("dontAsk");
        if (cad::cli_version_at_least(inst.version, kPermissionPromptsMajor, kPermissionPromptsMinor,
                                      kPermissionPromptsPatch))
            args << QStringLiteral("--permission-prompts") << QStringLiteral("none");
    }
    QStringList servers;
    if (spec.mcp) {
        const LaunchPlan mcp = mcp_config_for_run(*spec.mcp, spec);
        if (!mcp.error.isEmpty()) {
            plan.error = mcp.error;
            return plan;
        }
        args << mcp.args;
        plan.temp_files << mcp.temp_files;
        servers << spec.mcp->name;
    }
    args << QStringLiteral("--strict-mcp-config"); // only the terminal's server, never the user's others
    if (!full_access) {
        QStringList builtins = {QStringLiteral("Read"), QStringLiteral("Grep"), QStringLiteral("Glob"),
                                QStringLiteral("WebSearch"), QStringLiteral("WebFetch")};
        if (spec.permission == PermissionMode::WorkspaceWrite)
            builtins << QStringLiteral("Edit") << QStringLiteral("Write");
        args << QStringLiteral("--tools") << builtins.join(QLatin1Char(','));
        QStringList allowed;
        for (const QString& name : std::as_const(servers))
            allowed << QStringLiteral("mcp__%1__*").arg(name);
        allowed << builtins;
        args << QStringLiteral("--allowedTools") << allowed.join(QLatin1Char(','));
    }
    // Through cmd.exe (an npm shim that could not be resolved to node + script)
    // the prompt loses the characters cmd.exe would interpret.
    plan.system_prompt_arg =
        inst.via == Installation::CmdShim ? cad::cmd_safe_text(spec.system_prompt) : spec.system_prompt;
    args << QStringLiteral("--append-system-prompt") << plan.system_prompt_arg;
    if (!spec.model.isEmpty())
        args << QStringLiteral("--model") << spec.model;
    if (!spec.effort.isEmpty())
        args << QStringLiteral("--effort") << spec.effort;
    if (spec.max_budget_usd > 0.0)
        args << QStringLiteral("--max-budget-usd") << QString::number(spec.max_budget_usd, 'f', 2);
    if (!spec.json_schema.isEmpty())
        args << QStringLiteral("--json-schema") << spec.json_schema;
    if (!resume.isEmpty())
        args << QStringLiteral("--resume") << resume;
    // The prompt travels over stdin: no quoting or length limits, never in the process list.
    plan.stdin_payload = spec.prompt.toUtf8();
    return plan;
}

LaunchPlan ClaudeAdapter::build_chat_command(const RunSpec& spec, const Installation& inst) const {
    LaunchPlan plan;
    static const QRegularExpression effort_rx(QStringLiteral("^[a-z]{1,16}$"));
    static const QRegularExpression session_rx(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    static const QRegularExpression model_rx(QStringLiteral(R"(^[A-Za-z0-9._:/@\[\]-]{1,100}$)"));
    const QString& resume = spec.resume_session_id;
    if (inst.version.isEmpty() ||
        !cad::cli_version_at_least(inst.version, kChatMinMajor, kChatMinMinor, kChatMinPatch)) {
        plan.error = QStringLiteral("Terminal AI needs Claude Code %1.%2.%3 or newer (installed: %4).")
                         .arg(kChatMinMajor)
                         .arg(kChatMinMinor)
                         .arg(kChatMinPatch)
                         .arg(inst.version.isEmpty() ? QStringLiteral("unknown version") : inst.version);
        return plan;
    }
    if (spec.permission != PermissionMode::ReadOnly) {
        plan.error = QStringLiteral("Terminal AI calls are read-only.");
        return plan;
    }
    if (spec.max_turns > 0) {
        plan.error = QStringLiteral("Claude Code has no turn limit option.");
        return plan;
    }
    if (!spec.model.isEmpty() && (!model_rx.match(spec.model).hasMatch() || spec.model.startsWith(QLatin1Char('-')))) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(40));
        return plan;
    }
    if (!spec.effort.isEmpty() && !effort_rx.match(spec.effort).hasMatch()) {
        plan.error = QStringLiteral("The reasoning effort “%1” is not valid.").arg(spec.effort.left(40));
        return plan;
    }
    if (!resume.isEmpty() && (!spec.chat_persist_session || !session_rx.match(resume).hasMatch() ||
                              resume.startsWith(QLatin1Char('-')))) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The message is empty.");
        return plan;
    }

    QStringList& args = plan.args;
    args << QStringLiteral("-p") << QStringLiteral("--output-format") << QStringLiteral("stream-json")
         << QStringLiteral("--verbose");
    if (spec.want_deltas)
        args << QStringLiteral("--include-partial-messages");
    if (!spec.chat_persist_session)
        args << QStringLiteral("--no-session-persistence"); // stateless: nothing lands in the user's own session list
    // No built-in tool at all and none of the user's settings, hooks or MCP
    // servers: the only thing this call can touch is the terminal's MCP server.
    args << QStringLiteral("--restricted") << QStringLiteral("--tools") << QString();
    QString server;
    if (spec.mcp) {
        const LaunchPlan mcp = mcp_config_for_run(*spec.mcp, spec);
        if (!mcp.error.isEmpty()) {
            plan.error = mcp.error;
            return plan;
        }
        args << mcp.args;
        plan.temp_files << mcp.temp_files;
        server = spec.mcp->name;
    }
    args << QStringLiteral("--strict-mcp-config");
    if (!server.isEmpty())
        args << QStringLiteral("--allowedTools") << QStringLiteral("mcp__%1__*").arg(server);
    // Unattended: anything not pre-approved is denied instead of waiting on a prompt.
    args << QStringLiteral("--permission-mode") << QStringLiteral("dontAsk") << QStringLiteral("--permission-prompts")
         << QStringLiteral("none");
    plan.system_prompt_arg =
        inst.via == Installation::CmdShim ? cad::cmd_safe_text(spec.system_prompt) : spec.system_prompt;
    args << QStringLiteral("--system-prompt") << plan.system_prompt_arg;
    if (!spec.model.isEmpty())
        args << QStringLiteral("--model") << spec.model;
    if (!spec.effort.isEmpty())
        args << QStringLiteral("--effort") << spec.effort;
    if (spec.max_budget_usd > 0.0)
        args << QStringLiteral("--max-budget-usd") << QString::number(spec.max_budget_usd, 'f', 2);
    if (!spec.json_schema.isEmpty())
        args << QStringLiteral("--json-schema") << spec.json_schema;
    if (!resume.isEmpty())
        args << QStringLiteral("--resume") << resume;
    // A personal subscription token never serves terminal AI (owner decision):
    // with ANTHROPIC_API_KEY in the environment, -p authenticates with the key.
    plan.env_unset = desc_.terminal_unset_env_vars;
    plan.stdin_payload = spec.prompt.toUtf8();
    return plan;
}

LaunchPlan ClaudeAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    LaunchPlan plan;
    static const QRegularExpression env_rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    if (endpoint.name.isEmpty() || endpoint.url.isEmpty()) {
        plan.error = QStringLiteral("The terminal's MCP server returned no client configuration.");
        return plan;
    }
    if (!env_rx.match(endpoint.token_env_var).hasMatch()) {
        plan.error = QStringLiteral("The MCP token variable name is not valid.");
        return plan;
    }
    if (spec.temp_dir.isEmpty()) {
        plan.error = QStringLiteral("No folder was given for the run's MCP configuration.");
        return plan;
    }
    // The header names the variable; Claude Code expands ${VAR} from the
    // child's environment, so the token itself never reaches the file.
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), endpoint.url},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ${%1}").arg(endpoint.token_env_var)}}},
    };
    TempFile file;
    file.name = QStringLiteral("mcp-") + spec.run_id.left(8) + QStringLiteral(".json");
    file.content = QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{endpoint.name, server}}}})
                       .toJson(QJsonDocument::Compact);
    plan.args << QStringLiteral("--mcp-config") << spec.temp_dir + QLatin1Char('/') + file.name;
    plan.temp_files << file;
    return plan;
}

std::unique_ptr<IStreamParser> ClaudeAdapter::make_parser() const {
    return std::make_unique<ClaudeParser>();
}

CancelPlan ClaudeAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind ClaudeAdapter::classify_failure(int exit_code, const cad::StreamFacts& facts,
                                            const QStringList& stderr_tail) const {
    if (exit_code == 0 && facts.saw_result && !facts.result_is_error)
        return FailureKind::None;
    const QString text = (facts.result_error + QLatin1Char('\n') + facts.last_error + QLatin1Char('\n') +
                          stderr_tail.join(QLatin1Char('\n')))
                             .toLower();
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(not logged in|please run /login|invalid api key|authentication_failed|authentication failed|unauthori[sz]ed|\b401\b|token has expired)"));
    static const QRegularExpression rate_rx(
        QStringLiteral(R"(rate.?limit|\b429\b|usage limit|overloaded|too many requests)"));
    static const QRegularExpression model_rx(QStringLiteral(
        R"(model[^\n]{0,80}(not found|not available|does not exist|is not supported)|unknown model|invalid model)"));
    if (auth_rx.match(text).hasMatch())
        return FailureKind::Auth;
    if (rate_rx.match(text).hasMatch())
        return FailureKind::RateLimit;
    if (model_rx.match(text).hasMatch())
        return FailureKind::Model;
    return FailureKind::Other;
}

std::optional<McpRegistration> ClaudeAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    if (!claude_endpoint_ok(endpoint))
        return std::nullopt;
    const QString& name = endpoint.name;
    McpRegistration r;
    r.server_name = name;
    r.check = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("get"), name}, 20000};
    r.remove = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("remove"), name, QStringLiteral("--scope"),
                             QStringLiteral("user")},
                            20000};
    // The header names the variable instead of carrying the token, so nothing
    // secret is on the command line or in ~/.claude.json. Claude Code documents
    // ${VAR} / ${VAR:-default} expansion in `url` and `headers` of MCP server
    // entries (code.claude.com/docs/en/mcp, "Environment variable expansion in
    // .mcp.json", checked 2026-11-17); the page does not say in so many words
    // that user-scope entries and --mcp-config files get the same treatment —
    // confirm with a live run. If not, the user's own sessions send the literal
    // text and the server answers 401 (the copy-paste form carries the real token).
    r.add = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("add"), QStringLiteral("--transport"),
                          QStringLiteral("http"), QStringLiteral("--scope"), QStringLiteral("user"), name, endpoint.url,
                          QStringLiteral("--header"),
                          QStringLiteral("Authorization: Bearer ${%1}").arg(endpoint.token_env_var)},
                         20000};
    r.token_from_env = true;
    r.manual_args = {QStringLiteral("mcp"),  QStringLiteral("add"),  QStringLiteral("--transport"),
                     QStringLiteral("http"), name,                   endpoint.url,
                     QStringLiteral("-H"),
                     QStringLiteral("Authorization: Bearer ") + QString::fromLatin1(kMcpTokenPlaceholder),
                     QStringLiteral("-s"),   QStringLiteral("user")};
    r.note = QStringLiteral("Claude Code reads the token from the %1 environment variable when it connects, so that "
                            "variable must be set in the environment you start Claude Code from (the terminal sets it "
                            "only for the runs it launches itself). The copy-paste command instead stores the token in "
                            "your Claude Code settings.")
                 .arg(endpoint.token_env_var);
    return r;
}

} // namespace fincept::harness
