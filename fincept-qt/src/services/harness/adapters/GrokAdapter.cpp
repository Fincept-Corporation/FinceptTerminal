// GrokAdapter.cpp — Grok Build as a harness (see GrokAdapter.h). Every flag used
// here is listed by `grok --help` of grok 1.0.41; QtCore only.

#include "services/harness/adapters/GrokAdapter.h"

#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/AdapterSupport.h"
#include "services/harness/parsers/AcpUpdateParser.h"

#include <QRegularExpression>

namespace fincept::harness {

namespace grok_adapter_detail {

namespace gad_cad = fincept::services::coding_agents;
namespace gad_sup = fincept::harness::adapter_support;

/// `grok models` of grok 1.0.41 (signed out), used when the list cannot be read.
constexpr const char* kGrokRecordedVersion = "1.0.41";

} // namespace grok_adapter_detail

using namespace grok_adapter_detail;

GrokAdapter::GrokAdapter() {
    desc_.id = QStringLiteral("grok");
    desc_.display_name = QStringLiteral("Grok Build");
    desc_.vendor = QStringLiteral("xAI");
    desc_.install_url = QStringLiteral("https://x.ai");
    desc_.docs_url = QStringLiteral("https://x.ai");
    desc_.signin_hint = QStringLiteral("Run `grok login` in a terminal (on a remote or headless machine: "
                                       "`grok login --device-auth`).");
    desc_.signin_command = QStringLiteral("grok login");
    desc_.executable_names = {QStringLiteral("grok")};
    desc_.transport = Transport::PrintJsonl; // one process per run; each line is an ACP session update
    Capabilities& c = desc_.caps;
    c.structured_stream = true;
    c.text_deltas = true;          // agent_message_chunk updates
    c.thinking = false;            // not documented for print mode
    c.resume = true;               // -r <session id>
    c.per_run_mcp = false;         // no per-run flag in --help
    c.mcp_http = true;             // `grok mcp add --transport http`
    c.enforced_read_only = false;  // what dontAsk denies is not documented
    c.model_list = true;           // `grok models`
    c.effort = false;              // --reasoning-effort exists, its values are not listed
    c.budget_usd = false;
    c.max_turns = true;            // --max-turns N
    c.json_schema = false;         // --json-schema implies --output-format json (no streaming)
    c.system_prompt = true;        // --rules "Extra rules to append to the system prompt"
    c.persistent_session = false;
    c.usage_tokens = false;        // only if the stream carries ACP usage (not verified)
    c.cost_usd = false;
    c.full_access = false;
    desc_.run_mcp_server_name = QString::fromLatin1(kMcpServerName); // "fincept_community" (EditionIdentity.h)
    desc_.beta = true;
    desc_.beta_note = QStringLiteral("Written from grok --help; not yet checked against a recorded run");
}

QStringList GrokAdapter::candidate_paths(const DetectContext& ctx) const {
    if (ctx.home.isEmpty())
        return {};
    const QString dir = ctx.home + QStringLiteral("/.grok/bin/");
    const QString ext = ctx.windows ? QStringLiteral(".exe") : QString();
    // agent(.exe) is the same binary; check_version() accepts it only when its
    // --version line names grok.
    return {dir + QStringLiteral("grok") + ext, dir + QStringLiteral("agent") + ext};
}

ProbeCommand GrokAdapter::version_command() const {
    return ProbeCommand{{QStringLiteral("--version")}, 20000};
}

QString GrokAdapter::parse_version(const QString& output) const {
    // "grok 1.0.41 (4220f3b224a6) [stable]" — from grok.exe and agent.exe alike.
    const QString line = output.trimmed();
    if (!line.startsWith(QLatin1String("grok"), Qt::CaseInsensitive))
        return {};
    return gad_cad::parse_cli_version(line);
}

std::optional<ProbeCommand> GrokAdapter::auth_command() const {
    return ProbeCommand{{QStringLiteral("models")}, 20000};
}

AuthStatus GrokAdapter::parse_auth(int exit_code, const QByteArray& out, const QByteArray& err) const {
    // Only fixed phrases are looked for; nothing of the output is kept.
    const QString text = QString::fromUtf8(out) + QLatin1Char('\n') + QString::fromUtf8(err);
    AuthStatus a;
    if (text.contains(QLatin1String("not authenticated"), Qt::CaseInsensitive)) {
        a.state = AuthState::SignedOut; // "You are not authenticated." (grok 1.0.41, exit code 0)
    } else if (exit_code != 0) {
        a.state = AuthState::Error;
        a.note = QStringLiteral("`grok models` exited with code %1.").arg(exit_code);
    } else if (text.contains(QLatin1String("Available models:"), Qt::CaseInsensitive)) {
        a.state = AuthState::SignedIn;
        a.note = QStringLiteral("inferred from `grok models`");
    } else {
        a.state = AuthState::Unknown;
        a.note = QStringLiteral("The sign-in status could not be read.");
    }
    return a;
}

AuthStatus GrokAdapter::auth_hint(const DetectContext& ctx) const {
    Q_UNUSED(ctx); // no API-key variable or credential file of Grok Build is documented
    return AuthStatus{};
}

std::optional<ProbeCommand> GrokAdapter::models_command() const {
    return ProbeCommand{{QStringLiteral("models")}, 20000};
}

QVector<ModelInfo> GrokAdapter::parse_models(int exit_code, const QByteArray& out) const {
    // grok 1.0.41:
    //   Default model: grok-4.6
    //
    //   Available models:
    //     * grok-4.6 (default)
    //     - grok-4.5
    if (exit_code != 0)
        return {};
    static const QRegularExpression default_rx(QStringLiteral(R"(^Default model:\s*(\S+)\s*$)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression item_rx(QStringLiteral(R"(^\s*([*-])\s+(\S+)(\s+\(default\))?\s*$)"));
    QString default_id;
    bool in_list = false;
    QVector<ModelInfo> list;
    for (QString line : QString::fromUtf8(out).split(QLatin1Char('\n'))) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QString t = line.trimmed();
        const auto dm = default_rx.match(t);
        if (dm.hasMatch()) {
            if (gad_sup::valid_model_id(dm.captured(1)))
                default_id = dm.captured(1);
            continue;
        }
        if (t.compare(QLatin1String("Available models:"), Qt::CaseInsensitive) == 0) {
            in_list = true;
            continue;
        }
        if (!in_list)
            continue;
        const auto im = item_rx.match(line);
        if (!im.hasMatch())
            continue;
        const QString id = im.captured(2);
        if (!gad_sup::valid_model_id(id) || list.size() >= 100)
            continue;
        bool dup = false;
        for (const ModelInfo& m : std::as_const(list))
            dup = dup || m.id == id;
        if (dup)
            continue;
        ModelInfo m;
        m.id = id;
        m.label = id;
        m.is_default = !im.captured(3).isEmpty() || im.captured(1) == QLatin1String("*");
        list.append(m);
    }
    bool has_default = false;
    for (const ModelInfo& m : std::as_const(list))
        has_default = has_default || m.is_default;
    if (!has_default && !default_id.isEmpty()) {
        for (ModelInfo& m : list)
            m.is_default = m.id == default_id;
    }
    return list;
}

QVector<ModelInfo> GrokAdapter::builtin_models(const QString& version) const {
    Q_UNUSED(version); // only 1.0.41 has been seen
    QVector<ModelInfo> out;
    ModelInfo a;
    a.id = QStringLiteral("grok-4.6");
    a.label = a.id;
    a.is_default = true;
    a.description = QStringLiteral("Listed by grok %1").arg(QLatin1String(kGrokRecordedVersion));
    ModelInfo b;
    b.id = QStringLiteral("grok-4.5");
    b.label = b.id;
    b.description = a.description;
    out << a << b;
    return out;
}

QStringList GrokAdapter::efforts(const QString& model, const QString& version) const {
    Q_UNUSED(model);
    Q_UNUSED(version);
    return {}; // --reasoning-effort exists but `grok --help` lists no values
}

LaunchPlan GrokAdapter::build_command(const RunSpec& spec, const Installation& inst) const {
    LaunchPlan plan;
    if (spec.chat) {
        plan.error = QStringLiteral("Chat calls through Grok Build are not available yet.");
        return plan;
    }
    if (spec.permission == PermissionMode::FullAccess) {
        plan.error = QStringLiteral("Full access is not available for Grok Build runs.");
        return plan;
    }
    if (spec.mcp) {
        plan.error = QStringLiteral("Grok Build has no per-run MCP option. Add the terminal's MCP server to Grok "
                                    "Build under CONNECT, or run without terminal tools.");
        return plan;
    }
    if (!spec.effort.isEmpty()) {
        plan.error = QStringLiteral("Grok Build does not list its reasoning-effort values, so none can be chosen.");
        return plan;
    }
    if (!spec.json_schema.isEmpty()) {
        plan.error = QStringLiteral("A response schema is not available for Grok Build runs.");
        return plan;
    }
    if (spec.prompt.trimmed().isEmpty()) {
        plan.error = QStringLiteral("The prompt is empty.");
        return plan;
    }
    if (!spec.model.isEmpty() && !gad_sup::valid_model_id(spec.model)) {
        plan.error = QStringLiteral("The model name “%1” is not valid.").arg(spec.model.left(60));
        return plan;
    }
    if (!spec.resume_session_id.isEmpty() && !gad_sup::valid_session_id(spec.resume_session_id)) {
        plan.error = QStringLiteral("The session id to resume is not valid.");
        return plan;
    }
    if (spec.max_turns < 0 || spec.max_turns > gad_cad::kRunMaxTurnsLimit) {
        plan.error = QStringLiteral("The turn limit must be between 1 and %1.").arg(gad_cad::kRunMaxTurnsLimit);
        return plan;
    }

    const bool through_cmd = inst.via == Installation::CmdShim;
    QString rules = through_cmd ? gad_cad::cmd_safe_text(spec.system_prompt) : spec.system_prompt;
    if (rules.size() > gad_sup::kArgvTextBudgetChars) {
        plan.error = QStringLiteral("The run's system prompt is too long for a command line.");
        return plan;
    }
    rules = gad_sup::argv_text(rules);

    QStringList& args = plan.args;
    // Through cmd.exe nothing free-form may be on the line; otherwise only a
    // prompt that would push the line past the budget goes through a file.
    // (--prompt-file = "Single-turn prompt from a file" per --help; not yet
    // exercised in a recorded run.)
    const bool via_file = through_cmd || spec.prompt.size() + rules.size() > gad_sup::kArgvTextBudgetChars;
    if (via_file) {
        if (spec.temp_dir.isEmpty()) {
            plan.error = QStringLiteral("No folder was given for the run's prompt file.");
            return plan;
        }
        TempFile file;
        file.name = QStringLiteral("prompt-") + spec.run_id.left(8) + QStringLiteral(".txt");
        file.content = spec.prompt.toUtf8();
        args << QStringLiteral("--prompt-file") << spec.temp_dir + QLatin1Char('/') + file.name;
        plan.temp_files << file;
    } else {
        plan.prompt_arg = gad_sup::argv_text(spec.prompt);
        args << QStringLiteral("-p") << plan.prompt_arg;
    }
    args << QStringLiteral("--output-format") << QStringLiteral("streaming-json");
    // dontAsk / acceptEdits: listed values of --permission-mode; neither waits on
    // a prompt in a headless run (assumed from their names — unverified).
    args << QStringLiteral("--permission-mode")
         << (spec.permission == PermissionMode::WorkspaceWrite ? QStringLiteral("acceptEdits") : QStringLiteral("dontAsk"));
    if (!rules.trimmed().isEmpty()) {
        plan.system_prompt_arg = rules;
        args << QStringLiteral("--rules") << rules;
    }
    if (!spec.model.isEmpty())
        args << QStringLiteral("-m") << spec.model;
    if (spec.max_turns > 0)
        args << QStringLiteral("--max-turns") << QString::number(spec.max_turns);
    if (!spec.resume_session_id.isEmpty())
        args << QStringLiteral("-r") << spec.resume_session_id;
    return plan; // nothing on stdin
}

LaunchPlan GrokAdapter::mcp_config_for_run(const McpEndpoint& endpoint, const RunSpec& spec) const {
    Q_UNUSED(endpoint);
    Q_UNUSED(spec);
    LaunchPlan plan;
    plan.error = QStringLiteral("Grok Build has no per-run MCP option.");
    return plan;
}

std::unique_ptr<IStreamParser> GrokAdapter::make_parser() const {
    return std::make_unique<AcpUpdateParser>();
}

CancelPlan GrokAdapter::cancel_plan() const {
    return CancelPlan{true, false, 3000};
}

FailureKind GrokAdapter::classify_failure(int exit_code, const gad_cad::StreamFacts& facts,
                                          const QStringList& stderr_tail) const {
    static const QRegularExpression auth_rx(QStringLiteral(
        R"(not authenticated|grok login|unauthori[sz]ed|\b401\b|invalid api key|authentication (failed|required)|token (has )?expired)"));
    return gad_sup::classify(exit_code, facts, stderr_tail, auth_rx, /*require_result=*/false);
}

std::optional<McpRegistration> GrokAdapter::mcp_registration(const McpEndpoint& endpoint) const {
    if (!gad_sup::valid_mcp_endpoint(endpoint))
        return std::nullopt;
    McpRegistration r;
    r.server_name = endpoint.name;
    // `grok mcp add` takes headers only as literal values (-H "NAME: VALUE"), so
    // there is no token-free form: no one-click add, only the copy-paste command.
    r.remove = ProbeCommand{{QStringLiteral("mcp"), QStringLiteral("remove"), endpoint.name}, 20000};
    r.manual_args = {QStringLiteral("mcp"),        QStringLiteral("add"), QStringLiteral("--transport"),
                     QStringLiteral("http"),       endpoint.name,         endpoint.url,
                     QStringLiteral("-H"),         gad_sup::placeholder_bearer_header()};
    r.token_from_env = false;
    r.note = QStringLiteral("Grok Build stores the token as a literal header, so the terminal cannot add it for you "
                            "without putting the token on a command line. Run the command yourself (copying it fills "
                            "in the token) and run it again whenever the token changes.");
    return r;
}

} // namespace fincept::harness
