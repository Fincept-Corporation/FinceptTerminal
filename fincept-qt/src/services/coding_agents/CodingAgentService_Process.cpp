// CodingAgentService_Process.cpp — CLI detection, command lines, the QProcess
// lifecycle of a run (stdin prompt, stdout/stderr streaming, exit
// classification), process-tree stop/kill, and `claude|codex mcp add` for the
// Connect tab. Part of the CodingAgentService partial-class split.

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer.h"
#include "services/coding_agents/CodingAgentLiveRun.h"
#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#    include <windows.h>
#else
#    include <signal.h>
#    include <sys/types.h>
#endif

namespace fincept::services::coding_agents {

namespace coding_agent_process_detail {

constexpr const char* kCodingAgentProcTag = "CodingAgents";
constexpr int kVersionProbeTimeoutMs = 20000;
constexpr int kCliCommandTimeoutMs = 60000;
constexpr const char* kTerminalServerName = "fincept";
// Name used when registering this terminal with the user's own Claude Code /
// Codex config. Distinct from "fincept", which Enterprise users already map to
// the Enterprise MCP server.
constexpr const char* kClientServerName = "fincept_terminal";

struct LaunchSpec {
    QString program;
    QStringList args;
    QString native_args; // Windows cmd.exe shim only
    bool use_native = false;
    QString error;
};

[[maybe_unused]] bool is_cmd_shim(const QString& path) {
#ifdef Q_OS_WIN
    return path.endsWith(QLatin1String(".cmd"), Qt::CaseInsensitive) ||
           path.endsWith(QLatin1String(".bat"), Qt::CaseInsensitive);
#else
    Q_UNUSED(path);
    return false;
#endif
}

/// MSVC / CommandLineToArgvW quoting (what node and Rust parse on Windows).
[[maybe_unused]] QString msvc_quote(const QString& arg) {
    static const QRegularExpression needs_quotes(QStringLiteral("[\\s\"]"));
    if (!arg.isEmpty() && !arg.contains(needs_quotes))
        return arg;
    QString out = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar c : arg) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (c == QLatin1Char('"')) {
            out += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            out += QLatin1Char('"');
            backslashes = 0;
            continue;
        }
        out += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        out += c;
    }
    out += QString(backslashes * 2, QLatin1Char('\\'));
    out += QLatin1Char('"');
    return out;
}

/// cmd.exe expands %VAR% (and !VAR! with delayed expansion) even inside quotes
/// and treats ^ & | < > as operators outside them; a line break ends the command.
[[maybe_unused]] bool cmd_arg_safe(const QString& arg) {
    static const QRegularExpression bad(QStringLiteral("[%!^&|<>\\r\\n]"));
    return !arg.contains(bad);
}

LaunchSpec make_launch_spec(const CliInfo& cli, const QStringList& args) {
    LaunchSpec s;
#ifdef Q_OS_WIN
    if (cli.via_cmd_shim) {
        if (!cmd_arg_safe(cli.path)) {
            s.error = QStringLiteral("The CLI path contains characters cmd.exe cannot pass safely: %1").arg(cli.path);
            return s;
        }
        QString line = msvc_quote(QDir::toNativeSeparators(cli.path));
        for (const auto& a : args) {
            if (!cmd_arg_safe(a)) {
                s.error = QStringLiteral("An argument contains %^&|<>! or a line break, which cannot pass through the "
                                         "npm .cmd launcher. Install the native %1 binary or point Agent Workbench "
                                         "settings at it.")
                              .arg(QFileInfo(cli.path).completeBaseName());
                return s;
            }
            line += QLatin1Char(' ') + msvc_quote(a);
        }
        s.program = qEnvironmentVariable("ComSpec", QStringLiteral("cmd.exe"));
        s.native_args = QStringLiteral("/d /s /c \"%1\"").arg(line);
        s.use_native = true;
        return s;
    }
#endif
    s.program = cli.path;
    s.args = args;
    return s;
}

void apply_launch_spec(QProcess* proc, const LaunchSpec& spec) {
    proc->setProgram(spec.program);
#ifdef Q_OS_WIN
    // No console window flashing up for a console-subsystem child of a GUI app.
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= CREATE_NO_WINDOW;
    });
    if (spec.use_native) {
        proc->setNativeArguments(spec.native_args);
        return;
    }
#endif
    proc->setArguments(spec.args);
}

/// Environment for every CLI child: drop Claude Code's nested-session markers
/// (a terminal launched from inside Claude Code must still be able to run it),
/// never inherit a stale token, and on POSIX make sure node-based launchers
/// find `node` even though GUI apps start with a minimal PATH.
QProcessEnvironment child_environment(const CliInfo& cli, const QString& token) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("CLAUDECODE"));
    env.remove(QStringLiteral("CLAUDE_CODE_ENTRYPOINT"));
    env.remove(QString::fromLatin1(mcp::McpHttpServer::kTokenEnvVar));
    if (!token.isEmpty())
        env.insert(QString::fromLatin1(mcp::McpHttpServer::kTokenEnvVar), token);
#ifndef Q_OS_WIN
    const QString home = QDir::homePath();
    const QStringList extra = {QFileInfo(cli.path).absolutePath(),
                               home + QStringLiteral("/.local/bin"),
                               QStringLiteral("/opt/homebrew/bin"),
                               QStringLiteral("/usr/local/bin"),
                               home + QStringLiteral("/.npm-global/bin"),
                               home + QStringLiteral("/.bun/bin")};
    QStringList path = env.value(QStringLiteral("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (auto it = extra.crbegin(); it != extra.crend(); ++it) {
        if (!it->isEmpty() && !path.contains(*it))
            path.prepend(*it);
    }
    env.insert(QStringLiteral("PATH"), path.join(QLatin1Char(':')));
#else
    Q_UNUSED(cli);
#endif
    return env;
}

QString detect_cli_path(AgentKind kind) {
    const QString name = kind == AgentKind::Codex ? QStringLiteral("codex") : QStringLiteral("claude");
    const QString home = QDir::homePath();
    QStringList candidates;
    const QString on_path = QStandardPaths::findExecutable(name);
    if (!on_path.isEmpty())
        candidates << on_path;
#ifdef Q_OS_WIN
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    const QString roaming = qEnvironmentVariable("APPDATA");
    candidates << home + QStringLiteral("/.local/bin/") + name + QStringLiteral(".exe");
    if (kind == AgentKind::Codex && !local.isEmpty())
        candidates << local + QStringLiteral("/Programs/OpenAI/Codex/bin/codex.exe");
    if (!local.isEmpty())
        candidates << local + QStringLiteral("/Microsoft/WinGet/Links/") + name + QStringLiteral(".exe");
    candidates << home + QStringLiteral("/.bun/bin/") + name + QStringLiteral(".exe");
    if (!roaming.isEmpty())
        candidates << roaming + QStringLiteral("/npm/") + name + QStringLiteral(".cmd");
#else
    candidates << home + QStringLiteral("/.local/bin/") + name;
    if (kind == AgentKind::Claude)
        candidates << home + QStringLiteral("/.claude/local/claude");
    candidates << QStringLiteral("/opt/homebrew/bin/") + name << QStringLiteral("/usr/local/bin/") + name
               << home + QStringLiteral("/.npm-global/bin/") + name << home + QStringLiteral("/.bun/bin/") + name
               << QStringLiteral("/usr/bin/") + name;
#endif
    // Prefer a native executable: a .cmd shim needs cmd.exe and restricts arguments.
    QString shim;
    for (const auto& c : candidates) {
        const QFileInfo fi(c);
        if (!fi.isFile())
            continue;
#ifndef Q_OS_WIN
        if (!fi.isExecutable())
            continue;
#endif
        const QString abs = fi.absoluteFilePath();
        if (is_cmd_shim(abs)) {
            if (shim.isEmpty())
                shim = abs;
            continue;
        }
        return abs;
    }
    return shim;
}

QString mask_secrets(QString text, const QString& token) {
    if (!token.isEmpty())
        text.replace(token, QStringLiteral("••••"));
    static const QRegularExpression bearer(QStringLiteral(R"((Bearer\s+)[A-Za-z0-9._~+/=-]{6,})"));
    text.replace(bearer, QStringLiteral("\\1••••"));
    static const QRegularExpression env_token(QStringLiteral(R"((FINCEPT_MCP_TOKEN\s*[=:]\s*)\S+)"));
    text.replace(env_token, QStringLiteral("\\1••••"));
    return text;
}

bool stderr_looks_like_error(const QString& line) {
    static const QRegularExpression rx(
        QStringLiteral(R"(\b(error|failed|fatal|exception|denied|unauthori[sz]ed|forbidden|invalid api key|not logged in|panic)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(line).hasMatch();
}

QString shown_command(const QString& program, const QStringList& args, const QString& system_prompt) {
    QStringList parts{QDir::toNativeSeparators(program)};
    for (const auto& a : args) {
        if (!system_prompt.isEmpty() && a == system_prompt) {
            parts << QStringLiteral("<system prompt: %1 chars>").arg(a.size());
            continue;
        }
        parts << (a.contains(QLatin1Char(' ')) || a.isEmpty() ? QLatin1Char('"') + a + QLatin1Char('"') : a);
    }
    return parts.join(QLatin1Char(' '));
}

#ifdef Q_OS_WIN
void* create_kill_on_close_job() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return nullptr;
    }
    return job;
}

bool assign_to_job(void* job, qint64 pid) {
    if (!job || pid <= 0)
        return false;
    HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    const BOOL ok = AssignProcessToJobObject(static_cast<HANDLE>(job), h);
    CloseHandle(h);
    return ok != FALSE;
}
#endif

} // namespace coding_agent_process_detail

using namespace coding_agent_process_detail;

// ── CLI detection ────────────────────────────────────────────────────────────

CliInfo CodingAgentService::cli_info(AgentKind kind) const {
    return kind == AgentKind::Codex ? codex_cli_ : claude_cli_;
}

void CodingAgentService::resolve_cli_paths() {
    for (AgentKind kind : {AgentKind::Claude, AgentKind::Codex}) {
        CliInfo& current = kind == AgentKind::Codex ? codex_cli_ : claude_cli_;
        CliInfo fresh;
        fresh.kind = kind;
        const QString override_path = cli_path_override(kind);
        if (!override_path.isEmpty()) {
            const QFileInfo fi(override_path);
            if (fi.isFile()) {
                fresh.path = fi.absoluteFilePath();
                fresh.path_overridden = true;
            }
        }
        if (fresh.path.isEmpty())
            fresh.path = detect_cli_path(kind);
        fresh.via_cmd_shim = is_cmd_shim(fresh.path);
        if (!fresh.path.isEmpty() && fresh.path == current.path) {
            // Same binary — keep what the last probe learned.
            fresh.version = current.version;
            fresh.version_line = current.version_line;
            fresh.probed = current.probed;
            fresh.probing = current.probing;
            fresh.probe_error = current.probe_error;
        }
        current = fresh;
    }
    cli_resolved_ = true;
}

void CodingAgentService::refresh_cli_detection() {
    resolve_cli_paths();
    emit cli_status_changed();
    probe_version(AgentKind::Claude);
    probe_version(AgentKind::Codex);
}

void CodingAgentService::probe_version(AgentKind kind) {
    CliInfo& ci = kind == AgentKind::Codex ? codex_cli_ : claude_cli_;
    if (!ci.installed() || ci.probing)
        return;
    const LaunchSpec spec = make_launch_spec(ci, {QStringLiteral("--version")});
    if (!spec.error.isEmpty()) {
        ci.probed = true;
        ci.probe_error = spec.error;
        emit cli_status_changed();
        return;
    }
    ci.probing = true;
    const QString path = ci.path;
    auto* proc = new QProcess(this);
    proc->setProcessEnvironment(child_environment(ci, {}));
    apply_launch_spec(proc, spec);
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, proc, [proc]() { proc->kill(); });
    connect(proc, &QProcess::finished, this, [this, proc, kind, path](int code, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        CliInfo& c = kind == AgentKind::Codex ? codex_cli_ : claude_cli_;
        if (c.path != path)
            return; // re-resolved to another binary meanwhile
        c.probing = false;
        c.probed = true;
        c.version_line = out.section(QLatin1Char('\n'), 0, 0).trimmed();
        c.version = parse_cli_version(c.version_line);
        if (status == QProcess::NormalExit && code == 0 && !c.version.isEmpty())
            c.probe_error.clear();
        else
            c.probe_error = err.isEmpty() ? QStringLiteral("`--version` exited with code %1").arg(code) : err.left(300);
        emit cli_status_changed();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, kind, path](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        const QString msg = proc->errorString();
        proc->deleteLater();
        CliInfo& c = kind == AgentKind::Codex ? codex_cli_ : claude_cli_;
        if (c.path != path)
            return;
        c.probing = false;
        c.probed = true;
        c.probe_error = msg;
        emit cli_status_changed();
    });
    timer->start(kVersionProbeTimeoutMs);
    proc->start();
}

// ── Launch ───────────────────────────────────────────────────────────────────

bool CodingAgentService::launch(const QString& run_id, QString* error) {
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return fail(QStringLiteral("Run not found."));

    resolve_cli_paths();
    const CliInfo cli = cli_info(it->agent);
    const QString agent_name = agent_display_name(it->agent);
    if (!cli.installed())
        return fail(QStringLiteral("%1 CLI not found. Install it (%2) or set its path in Agent Workbench settings.")
                        .arg(agent_name, install_guide_url(it->agent)));
    if (!QDir().mkpath(it->workdir))
        return fail(QStringLiteral("Cannot create the working folder %1").arg(QDir::toNativeSeparators(it->workdir)));

    const bool resume = !it->parent_run_id.isEmpty() && !it->session_id.isEmpty();
    static const QRegularExpression session_rx(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    if (resume && !session_rx.match(it->session_id).hasMatch())
        return fail(QStringLiteral("The session id to resume is not valid."));

    // ── Terminal tools: the external MCP server must be up ───────────────────
    QString token;
    QString server_name = it->agent == AgentKind::Codex ? QStringLiteral("fincept_terminal")
                                                        : QString::fromLatin1(kTerminalServerName);
    QString mcp_url;
    QJsonObject claude_servers;
    QStringList codex_overrides;
    if (it->use_fincept_tools) {
        auto& srv = mcp::McpHttpServer::instance();
        if (!srv.is_running() && !srv.start())
            return fail(QStringLiteral("The terminal's MCP server could not start (%1). Open Agent Workbench › CONNECT, "
                                       "or untick “Use Fincept terminal tools”.")
                            .arg(srv.last_error().isEmpty() ? QStringLiteral("unknown error") : srv.last_error()));
        token = srv.token();
        mcp_url = srv.url();
        if (token.isEmpty())
            return fail(QStringLiteral("The terminal's MCP server has no access token."));
        if (it->agent == AgentKind::Claude) {
            const QJsonObject cfg = srv.claude_mcp_config_env();
            claude_servers = cfg.value(QStringLiteral("mcpServers")).toObject();
            if (claude_servers.isEmpty() && (cfg.contains(QStringLiteral("url")) || cfg.contains(QStringLiteral("type"))))
                claude_servers.insert(QString::fromLatin1(kTerminalServerName), cfg);
            if (claude_servers.isEmpty())
                return fail(QStringLiteral("The terminal's MCP server returned no client configuration."));
            server_name = claude_servers.keys().constFirst();
        } else {
            // A user's own `[mcp_servers.fincept]` entry (e.g. the Enterprise
            // server, or the stdio bridge from the CONNECT tab) must not merge
            // with ours — use a distinct name for in-app runs.
            static const QRegularExpression rename(QStringLiteral(R"(^mcp_servers\.fincept(?=[.=]))"));
            for (QString o : srv.codex_config_overrides()) {
                o.replace(rename, QStringLiteral("mcp_servers.fincept_terminal"));
                codex_overrides << o;
            }
            if (codex_overrides.isEmpty())
                return fail(QStringLiteral("The terminal's MCP server returned no Codex configuration."));
        }
    }

    // ── System context ───────────────────────────────────────────────────────
    SystemPromptContext ctx;
    ctx.agent = it->agent;
    ctx.mode = it->mode;
    ctx.tools_enabled = it->use_fincept_tools;
    ctx.mcp_server_name = server_name;
    ctx.workdir = QDir::toNativeSeparators(it->workdir);
    ctx.now = QDateTime::currentDateTime();
    const QString system_prompt = build_agent_system_prompt(ctx);

    // ── Command line ─────────────────────────────────────────────────────────
    QStringList args;
    QString mcp_config_path;
    QString shown_system_prompt;
    QByteArray stdin_payload;
    if (it->agent == AgentKind::Claude) {
        args << QStringLiteral("-p") << QStringLiteral("--output-format") << QStringLiteral("stream-json")
             << QStringLiteral("--verbose");
        // Unattended: anything not pre-approved below is denied instead of waiting on a prompt.
        args << QStringLiteral("--permission-mode") << QStringLiteral("dontAsk");
        if (cli_version_at_least(cli.version, 2, 1, 259))
            args << QStringLiteral("--permission-prompts") << QStringLiteral("none");
        if (!claude_servers.isEmpty()) {
            const QString dir = AppPaths::cache() + QStringLiteral("/coding_agents");
            QDir().mkpath(dir);
            mcp_config_path = dir + QStringLiteral("/mcp-") + run_id.left(8) + QStringLiteral(".json");
            QSaveFile f(mcp_config_path);
            const QByteArray json =
                QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), claude_servers}}).toJson(QJsonDocument::Compact);
            if (!f.open(QIODevice::WriteOnly) || f.write(json) != json.size() || !f.commit())
                return fail(QStringLiteral("Cannot write the MCP configuration file: %1").arg(f.errorString()));
            args << QStringLiteral("--mcp-config") << mcp_config_path;
        }
        args << QStringLiteral("--strict-mcp-config"); // only the terminal's server, never the user's others
        QStringList builtins = {QStringLiteral("Read"), QStringLiteral("Grep"), QStringLiteral("Glob"),
                                QStringLiteral("WebSearch"), QStringLiteral("WebFetch")};
        if (it->mode == RunMode::WorkspaceEdits)
            builtins << QStringLiteral("Edit") << QStringLiteral("Write");
        args << QStringLiteral("--tools") << builtins.join(QLatin1Char(','));
        QStringList allowed;
        for (const auto& name : claude_servers.keys())
            allowed << QStringLiteral("mcp__%1__*").arg(name);
        allowed << builtins;
        args << QStringLiteral("--allowedTools") << allowed.join(QLatin1Char(','));
        shown_system_prompt = cli.via_cmd_shim ? cmd_safe_text(system_prompt) : system_prompt;
        args << QStringLiteral("--append-system-prompt") << shown_system_prompt;
        if (!it->model.isEmpty())
            args << QStringLiteral("--model") << it->model;
        if (it->max_budget_usd > 0.0)
            args << QStringLiteral("--max-budget-usd") << QString::number(it->max_budget_usd, 'f', 2);
        if (resume)
            args << QStringLiteral("--resume") << it->session_id;
        stdin_payload = it->prompt.toUtf8();
    } else {
        const QString sandbox =
            it->mode == RunMode::WorkspaceEdits ? QStringLiteral("workspace-write") : QStringLiteral("read-only");
        args << QStringLiteral("exec");
        if (resume)
            args << QStringLiteral("resume");
        args << QStringLiteral("--json") << QStringLiteral("--skip-git-repo-check");
        if (resume) // `exec resume` has no -s/-C; the sandbox goes through config
            args << QStringLiteral("-c") << QStringLiteral("sandbox_mode=\"%1\"").arg(sandbox);
        else
            args << QStringLiteral("-s") << sandbox;
        args << QStringLiteral("-c") << QStringLiteral("approval_policy=\"never\"");
        if (!it->model.isEmpty())
            args << QStringLiteral("-m") << it->model;
        args << codex_overrides;
        if (resume)
            args << it->session_id;
        args << QStringLiteral("-"); // prompt from stdin
        stdin_payload = (resume ? it->prompt : wrap_codex_prompt(system_prompt, it->prompt)).toUtf8();
    }

    const LaunchSpec spec = make_launch_spec(cli, args);
    if (!spec.error.isEmpty()) {
        if (!mcp_config_path.isEmpty())
            QFile::remove(mcp_config_path);
        return fail(spec.error);
    }

    // ── Process ──────────────────────────────────────────────────────────────
    auto* live = new LiveRun;
    live->run_id = run_id;
    live->agent = it->agent;
    live->stdin_payload = stdin_payload;
    live->mcp_config_path = mcp_config_path;
#ifdef Q_OS_WIN
    live->job = create_kill_on_close_job();
#endif
    auto* proc = new QProcess(this);
    live->proc = proc;
    proc->setWorkingDirectory(it->workdir);
    proc->setProcessEnvironment(child_environment(cli, token));
    proc->setProcessChannelMode(QProcess::SeparateChannels);
#ifndef Q_OS_WIN
    // Own process group ⇒ Stop can signal the agent AND everything it spawned.
    proc->setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    apply_launch_spec(proc, spec);

    connect(proc, &QProcess::started, this, [this, run_id]() { on_proc_started(run_id); });
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, run_id]() { on_proc_stdout(run_id); });
    connect(proc, &QProcess::readyReadStandardError, this, [this, run_id]() { on_proc_stderr(run_id); });
    connect(proc, &QProcess::finished, this, [this, run_id](int code, QProcess::ExitStatus status) {
        on_proc_finished(run_id, code, status == QProcess::CrashExit);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, run_id, proc](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            on_proc_failed_to_start(run_id, proc->errorString());
    });

    auto* watchdog = new QTimer(this);
    watchdog->setSingleShot(true);
    live->watchdog = watchdog;
    connect(watchdog, &QTimer::timeout, this, [this, run_id]() {
        LiveRun* l = live_.value(run_id);
        if (!l)
            return;
        l->timed_out = true;
        record_status(run_id, QStringLiteral("Run limit reached"),
                      QStringLiteral("Stopping the agent after %1 hours.").arg(kCodingAgentMaxRunMs / 3600000), true);
        kill_tree(l);
    });

    live_.insert(run_id, live);
    it->status = RunStatus::Running;
    it->started_at_ms = QDateTime::currentMSecsSinceEpoch();
    it->error.clear();
    const QString workdir = it->workdir;
    const bool tools = it->use_fincept_tools;
    persist_run(run_id);
    write_prompt_file(*it);
    // `it` is not used past this point: slots of the signals below may touch runs_.
    emit run_updated(run_id);

    const QString version = cli.version.isEmpty() ? QString() : QLatin1Char(' ') + cli.version;
    record_status(run_id,
                  QStringLiteral("Starting %1%2%3").arg(agent_name, version, resume ? QStringLiteral(" (resume)") : QString()),
                  QStringLiteral("Command: %1\nWorking folder: %2\nTerminal tools: %3")
                      .arg(shown_command(cli.path, args, shown_system_prompt), QDir::toNativeSeparators(workdir),
                           tools ? QStringLiteral("%1 (server `%2`)").arg(mcp_url, server_name) : QStringLiteral("off")));
    LOG_INFO(kCodingAgentProcTag, QStringLiteral("Run %1: starting %2 in %3").arg(run_id.left(8), agent_name, workdir));

    watchdog->start(static_cast<int>(kCodingAgentMaxRunMs));
    proc->start(); // may fail synchronously → on_proc_failed_to_start → finish_run; `live` must not be touched after this
    return true;
}

void CodingAgentService::on_proc_started(const QString& run_id) {
    LiveRun* live = live_.value(run_id);
    if (!live || !live->proc)
        return;
    live->pid = live->proc->processId();
#ifdef Q_OS_WIN
    live->job_assigned = assign_to_job(live->job, live->pid);
    if (!live->job_assigned)
        LOG_WARN(kCodingAgentProcTag, QStringLiteral("Run %1: job object assignment failed — Stop kills only the CLI "
                                                     "process, not its children")
                                          .arg(run_id.left(8)));
#endif
    // The prompt travels over stdin: no quoting or length limits, never in the
    // process list. Closing stdin tells the CLI the prompt is complete.
    live->proc->write(live->stdin_payload);
    live->proc->closeWriteChannel();
    live->stdin_payload.clear();
}

void CodingAgentService::on_proc_stdout(const QString& run_id) {
    LiveRun* live = live_.value(run_id);
    if (!live || !live->proc)
        return;
    const QList<QByteArray> lines = live->out_buf.feed(live->proc->readAllStandardOutput());
    for (const auto& line : lines) {
        const QVector<AgentEvent> events = live->parse(QString::fromUtf8(line));
        for (const auto& ev : events)
            record_event(run_id, ev);
    }
    if (!lines.isEmpty())
        sync_facts(run_id);
}

void CodingAgentService::on_proc_stderr(const QString& run_id) {
    LiveRun* live = live_.value(run_id);
    if (!live || !live->proc)
        return;
    const QList<QByteArray> lines = live->err_buf.feed(live->proc->readAllStandardError());
    for (const auto& raw : lines) {
        const QString text = QString::fromUtf8(raw).trimmed();
        if (text.isEmpty())
            continue;
        live->stderr_tail.append(text.left(2000));
        while (live->stderr_tail.size() > kCodingAgentStderrTailLines)
            live->stderr_tail.removeFirst();
        if (live->stderr_events < kCodingAgentStderrEventLimit) {
            ++live->stderr_events;
            const bool err = stderr_looks_like_error(text);
            record_status(run_id, err ? QStringLiteral("stderr · error") : QStringLiteral("stderr"), text, err);
        } else if (live->stderr_events == kCodingAgentStderrEventLimit) {
            ++live->stderr_events;
            record_status(run_id, QStringLiteral("stderr suppressed"),
                          QStringLiteral("Further stderr output is kept only for failure diagnostics."));
        }
    }
}

void CodingAgentService::sync_facts(const QString& run_id) {
    LiveRun* live = live_.value(run_id);
    auto it = runs_.find(run_id);
    if (!live || it == runs_.end())
        return;
    const StreamFacts& f = live->facts();
    bool changed = false;
    bool persist = false;
    if (!f.session_id.isEmpty() && f.session_id != live->synced_session) {
        live->synced_session = f.session_id;
        it->session_id = f.session_id;
        changed = persist = true;
    }
    if (it->model.isEmpty() && !f.model.isEmpty()) {
        it->model = f.model; // the model actually used — follow-ups keep it
        changed = persist = true;
    }
    if (f.input_tokens != live->synced_in || f.output_tokens != live->synced_out) {
        live->synced_in = f.input_tokens;
        live->synced_out = f.output_tokens;
        it->input_tokens = f.input_tokens;
        it->output_tokens = f.output_tokens;
        changed = true;
    }
    if (f.cost_usd.has_value() && (!it->cost_usd.has_value() || *it->cost_usd != *f.cost_usd)) {
        it->cost_usd = f.cost_usd;
        changed = true;
    }
    if (persist)
        persist_run(run_id);
    if (changed)
        emit run_updated(run_id);
}

void CodingAgentService::on_proc_finished(const QString& run_id, int exit_code, bool crashed) {
    LiveRun* live = live_.value(run_id);
    if (!live || !runs_.contains(run_id))
        return;
    // Drain the pipes, including an unterminated last line.
    on_proc_stdout(run_id);
    on_proc_stderr(run_id);
    const QByteArray out_rest = live->out_buf.take_rest();
    if (!out_rest.trimmed().isEmpty()) {
        for (const auto& ev : live->parse(QString::fromUtf8(out_rest)))
            record_event(run_id, ev);
    }
    const QString err_rest = QString::fromUtf8(live->err_buf.take_rest()).trimmed();
    if (!err_rest.isEmpty()) {
        live->stderr_tail.append(err_rest.left(2000));
        record_status(run_id, QStringLiteral("stderr"), err_rest, stderr_looks_like_error(err_rest));
    }
    sync_facts(run_id);

    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    const StreamFacts& f = live->facts();
    const QString agent_name = agent_display_name(it->agent);
    it->exit_code = exit_code;
    if (!f.final_text.isEmpty())
        it->final_text = f.final_text;

    RunStatus status = RunStatus::Failed;
    QString err;
    if (live->stop_requested) {
        status = RunStatus::Cancelled;
        err = QStringLiteral("Stopped by the user.");
    } else if (live->timed_out) {
        err = QStringLiteral("Stopped after exceeding the %1-hour run limit.").arg(kCodingAgentMaxRunMs / 3600000);
    } else {
        const bool clean_exit = !crashed && exit_code == 0;
        const bool ok = it->agent == AgentKind::Claude
                            ? (clean_exit && f.saw_result && !f.result_is_error)
                            : (clean_exit && !f.result_is_error && (f.saw_result || !f.final_text.isEmpty()));
        if (ok) {
            status = RunStatus::Succeeded;
        } else {
            if (!f.result_error.isEmpty())
                err = f.result_error;
            else if (!f.last_error.isEmpty())
                err = f.last_error;
            else if (crashed)
                err = QStringLiteral("%1 crashed.").arg(agent_name);
            else if (exit_code != 0)
                err = QStringLiteral("%1 exited with code %2.").arg(agent_name).arg(exit_code);
            else
                err = QStringLiteral("%1 exited without producing a result.").arg(agent_name);
            if (f.result_error.isEmpty() && !live->stderr_tail.isEmpty()) {
                const qsizetype from = qMax<qsizetype>(0, live->stderr_tail.size() - 8);
                err += QStringLiteral("\n\nLast stderr output:\n") + live->stderr_tail.mid(from).join(QLatin1Char('\n'));
            }
        }
    }
    finish_run(run_id, status, err);
}

void CodingAgentService::on_proc_failed_to_start(const QString& run_id, const QString& message) {
    const CodingAgentRun* run = find_run(run_id);
    const QString path = run ? cli_info(run->agent).path : QString();
    finish_run(run_id, RunStatus::Failed,
               QStringLiteral("Could not start %1: %2").arg(QDir::toNativeSeparators(path), message));
}

// ── Stop / kill ──────────────────────────────────────────────────────────────

void CodingAgentService::terminate_tree(LiveRun* live) {
    if (!live)
        return;
#ifdef Q_OS_WIN
    // Console processes have no window to receive WM_CLOSE: the graceful step on
    // Windows is the stdin close the caller already did; kill_tree follows.
    Q_UNUSED(live);
#else
    if (live->pid > 0)
        ::kill(-static_cast<pid_t>(live->pid), SIGTERM);
#endif
}

void CodingAgentService::kill_tree(LiveRun* live) {
    if (!live)
        return;
#ifdef Q_OS_WIN
    if (live->job && live->job_assigned)
        TerminateJobObject(static_cast<HANDLE>(live->job), 1);
#else
    if (live->pid > 0)
        ::kill(-static_cast<pid_t>(live->pid), SIGKILL);
#endif
    if (live->proc && live->proc->state() != QProcess::NotRunning)
        live->proc->kill();
}

void CodingAgentService::cleanup_live(const QString& run_id, bool immediate) {
    LiveRun* live = live_.take(run_id);
    if (!live)
        return;
    if (live->watchdog) {
        live->watchdog->stop();
        live->watchdog->deleteLater();
    }
    if (live->kill_timer) {
        live->kill_timer->stop();
        live->kill_timer->deleteLater();
    }
    if (QProcess* proc = live->proc) {
        disconnect(proc, nullptr, this, nullptr);
        if (proc->state() != QProcess::NotRunning) {
            kill_tree(live);
            if (immediate)
                proc->waitForFinished(1500); // teardown only (P1 exception)
        }
        if (immediate)
            delete proc;
        else
            proc->deleteLater();
    }
#ifdef Q_OS_WIN
    if (live->job)
        CloseHandle(static_cast<HANDLE>(live->job)); // kill-on-close: reaps anything the agent left behind
#endif
    if (!live->mcp_config_path.isEmpty())
        QFile::remove(live->mcp_config_path);
    stderr_tails_.insert(run_id, live->stderr_tail);
    delete live;
}

// ── External clients (Connect tab) ───────────────────────────────────────────

void CodingAgentService::run_cli_async(AgentKind kind, const QStringList& args,
                                       std::function<void(int, const QString&)> done) {
    resolve_cli_paths();
    const CliInfo cli = cli_info(kind);
    if (!cli.installed()) {
        QTimer::singleShot(0, this, [done, kind]() {
            done(-1, QStringLiteral("%1 is not installed on this computer.").arg(agent_display_name(kind)));
        });
        return;
    }
    const LaunchSpec spec = make_launch_spec(cli, args);
    if (!spec.error.isEmpty()) {
        QTimer::singleShot(0, this, [done, err = spec.error]() { done(-1, err); });
        return;
    }
    auto* proc = new QProcess(this);
    proc->setProcessEnvironment(child_environment(cli, {}));
    proc->setProcessChannelMode(QProcess::MergedChannels);
    apply_launch_spec(proc, spec);
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, proc, [proc]() { proc->kill(); });
    connect(proc, &QProcess::finished, this, [proc, done](int code, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(proc->readAll());
        proc->deleteLater();
        done(status == QProcess::CrashExit ? -1 : code, out);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, done](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        const QString msg = proc->errorString();
        proc->deleteLater();
        done(-1, msg);
    });
    timer->start(kCliCommandTimeoutMs);
    proc->start();
}

void CodingAgentService::register_with_client(AgentKind kind, bool replace_existing) {
    const QString agent = agent_id(kind);
    resolve_cli_paths();
    if (!cli_info(kind).installed()) {
        emit client_registration_finished(agent, false, false,
                                          QStringLiteral("%1 is not installed on this computer.").arg(agent_display_name(kind)));
        return;
    }
    auto& srv = mcp::McpHttpServer::instance();
    if (!srv.is_running() && !srv.start()) {
        emit client_registration_finished(agent, false, false,
                                          QStringLiteral("The terminal's MCP server is not running: %1").arg(srv.last_error()));
        return;
    }
    const QString server = QString::fromLatin1(kClientServerName);
    if (replace_existing) {
        run_cli_async(kind, {QStringLiteral("mcp"), QStringLiteral("remove"), server},
                      [this, kind](int, const QString&) { add_to_client(kind); });
        return;
    }
    const QString token = srv.token();
    run_cli_async(kind, {QStringLiteral("mcp"), QStringLiteral("get"), server},
                  [this, kind, agent, token](int code, const QString& out) {
                      if (code == 0) {
                          emit client_registration_finished(agent, false, true,
                                                            mask_secrets(out, token).trimmed().left(2000));
                          return;
                      }
                      add_to_client(kind);
                  });
}

void CodingAgentService::add_to_client(AgentKind kind) {
    auto& srv = mcp::McpHttpServer::instance();
    const QString url = srv.url();
    const QString token = srv.token();
    const QString server = QString::fromLatin1(kClientServerName);
    const QString env_var = QString::fromLatin1(mcp::McpHttpServer::kTokenEnvVar);
    QStringList args;
    QString note;
    if (kind == AgentKind::Claude) {
        // --header is variadic in the claude CLI: it must come last.
        args = {QStringLiteral("mcp"), QStringLiteral("add"), QStringLiteral("--transport"), QStringLiteral("http"),
                QStringLiteral("--scope"), QStringLiteral("user"), server, url, QStringLiteral("--header"),
                QStringLiteral("Authorization: Bearer ") + token};
    } else {
        const QString python = srv.python_for_bridge();
        const QString bridge = srv.bridge_script_path();
        if (!python.isEmpty() && QFileInfo::exists(bridge)) {
            args = {QStringLiteral("mcp"), QStringLiteral("add"), server, QStringLiteral("--env"),
                    QStringLiteral("%1=%2").arg(env_var, token), QStringLiteral("--"), python, bridge,
                    QStringLiteral("--url"), url};
        } else {
            args = {QStringLiteral("mcp"), QStringLiteral("add"), server, QStringLiteral("--url"), url,
                    QStringLiteral("--bearer-token-env-var"), env_var};
            note = QStringLiteral("No Python interpreter was found for the stdio bridge, so Codex reads the token from "
                                  "the %1 environment variable — set it to the token shown on this tab.")
                       .arg(env_var);
        }
    }
    const QString agent = agent_id(kind);
    run_cli_async(kind, args, [this, agent, token, note](int code, const QString& out) {
        QString msg = mask_secrets(out, token).trimmed();
        if (code == 0) {
            if (msg.isEmpty())
                msg = QStringLiteral("Added the `fincept` MCP server.");
            if (!note.isEmpty())
                msg += QLatin1Char('\n') + note;
        } else if (msg.isEmpty()) {
            msg = QStringLiteral("Exit code %1.").arg(code);
        }
        const bool exists = code != 0 && msg.contains(QLatin1String("already exists"), Qt::CaseInsensitive);
        LOG_INFO(kCodingAgentProcTag,
                 QStringLiteral("Register with %1: %2").arg(agent, code == 0 ? QStringLiteral("ok") : QStringLiteral("failed")));
        emit client_registration_finished(agent, code == 0, exists, msg.left(2000));
    });
}

} // namespace fincept::services::coding_agents
