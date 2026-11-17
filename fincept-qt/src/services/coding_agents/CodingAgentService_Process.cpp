// CodingAgentService_Process.cpp — launching a run (two phases: the command
// line on the GUI thread, then its folder and per-run files on the file thread,
// then the process), stream handling, failure classification, process-tree
// stop/kill, and the CONNECT tab's one-click registration. Part of the
// CodingAgentService partial-class split.
//
// Where a CLI is, which version, the argument lists, the stream parser, the
// failure classes, the child environment and the process-tree mechanics all
// come from the harness layer (src/services/harness): HarnessRegistry holds
// the detected installation and the user's settings, the adapter builds the
// LaunchPlan and the parser, HarnessLaunch starts it. Nothing below names a
// particular harness.

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/process/HostExec.h"
#include "mcp/server/McpHttpServer.h"
#include "services/coding_agents/CodingAgentLiveRun.h"
#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessDetect.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/harness/McpLegacyEntry.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include <limits>
#include <string_view>

namespace fincept::services::coding_agents {

namespace coding_agent_process_detail {

// The harness layer is QtCore-only and names the token variable itself.
static_assert(std::string_view(mcp::McpHttpServer::kTokenEnvVar) == std::string_view(harness::kMcpTokenEnvVar),
              "harness::kMcpTokenEnvVar must match McpHttpServer::kTokenEnvVar");

constexpr const char* kCodingAgentProcTag = "CodingAgents";
constexpr int kCliCommandTimeoutMs = 60000;
constexpr int kMcpToolTimeoutSec = 300; // Codex's 60 s default is shorter than ui_act / ui_wait
// Name used when registering this terminal with the user's own harness config:
// edition-specific ("fincept_community", EditionIdentity.h), so it never
// replaces or removes the private build's entries ("fincept", "fincept-cloud", …).
constexpr const char* kClientServerName = harness::kMcpServerName;

QString mask_secrets(QString text, const QString& token) {
    if (!token.isEmpty())
        text.replace(token, QStringLiteral("••••"));
    static const QRegularExpression bearer(QStringLiteral(R"((Bearer\s+)[A-Za-z0-9._~+/=-]{6,})"));
    text.replace(bearer, QStringLiteral("\\1••••"));
    // Either edition's token variable, as an assignment in a CLI's output.
    static const QRegularExpression env_token(
        QStringLiteral("((?:%1|%2)\\s*[=:]\\s*)[^\\s$]\\S*")
            .arg(QRegularExpression::escape(QString::fromLatin1(edition::kMcpTokenEnvVar)),
                 QRegularExpression::escape(QString::fromLatin1(edition::kForeignMcpTokenEnvVar))));
    text.replace(env_token, QStringLiteral("\\1••••"));
    return text;
}

/// Notes about the one-time rename of a pre-split "fincept_terminal" entry,
/// shown with the registration result (GUI thread only).
QHash<QString, QString>& migration_notes() {
    static QHash<QString, QString> notes;
    return notes;
}

QString with_migration_note(const QString& harness_id, const QString& message) {
    const QString note = migration_notes().take(harness_id);
    return note.isEmpty() ? message : note + QStringLiteral("\n\n") + message;
}

bool stderr_looks_like_error(const QString& line) {
    static const QRegularExpression rx(
        QStringLiteral(R"(\b(error|failed|fatal|exception|denied|unauthori[sz]ed|forbidden|invalid api key|not logged in|panic)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(line).hasMatch();
}

/// The command line as shown in the transcript and the run header: the
/// prompts as their lengths, the adapter's redact list and the token masked.
QString shown_command(const QString& program, const harness::LaunchPlan& plan, const QString& token) {
    QStringList parts{QDir::toNativeSeparators(program)};
    for (const auto& a : plan.args) {
        if (!plan.system_prompt_arg.isEmpty() && a == plan.system_prompt_arg) {
            parts << QStringLiteral("<system prompt: %1 chars>").arg(a.size());
            continue;
        }
        if (!plan.prompt_arg.isEmpty() && a == plan.prompt_arg) {
            parts << QStringLiteral("<prompt: %1 chars>").arg(a.size());
            continue;
        }
        QString shown = a;
        for (const QString& secret : plan.redact) {
            if (!secret.isEmpty())
                shown.replace(secret, QStringLiteral("••••"));
        }
        parts << (shown.contains(QLatin1Char(' ')) || shown.isEmpty() ? QLatin1Char('"') + shown + QLatin1Char('"') : shown);
    }
    return mask_secrets(parts.join(QLatin1Char(' ')), token);
}

/// The plain sentence that leads a failed run's error.
QString failure_sentence(harness::FailureKind kind, const harness::HarnessDescriptor& d, const StreamFacts& f,
                         const QString& model) {
    switch (kind) {
        case harness::FailureKind::Auth:
            return QStringLiteral("Signed out of %1. %2").arg(d.display_name, d.signin_hint);
        case harness::FailureKind::RateLimit:
            if (f.rate_limit_resets_at_ms > QDateTime::currentMSecsSinceEpoch())
                return QStringLiteral("Rate limited until %1 — %2 reported the limit.")
                    .arg(QDateTime::fromMSecsSinceEpoch(f.rate_limit_resets_at_ms).toString(QStringLiteral("ddd HH:mm")),
                         d.display_name);
            return QStringLiteral("Rate limited by %1 — try again later.").arg(d.display_name);
        case harness::FailureKind::Model:
            return model.isEmpty() ? QStringLiteral("%1 rejected its default model.").arg(d.display_name)
                                   : QStringLiteral("%1 rejected the model “%2”.").arg(d.display_name, model);
        case harness::FailureKind::NotInstalled:
            return QStringLiteral("%1 is not installed (or could not be started).").arg(d.display_name);
        case harness::FailureKind::Timeout:
        case harness::FailureKind::Other:
        case harness::FailureKind::None:
            break;
    }
    return {};
}

/// Worker (file) thread: create the run's folders and write the adapter's
/// per-run files. Returns the error text, or an empty string.
QString prepare_run_files(const QString& workdir, const QString& temp_dir, const QVector<harness::TempFile>& files,
                          QStringList* written) {
    // Flatpak: a folder of the user's system may be out of the sandbox's reach;
    // the CLI's host-side start creates it there (HostExec.h), and reports it if it cannot.
    if (!QDir().mkpath(workdir) && !process::host::in_flatpak())
        return QStringLiteral("Cannot create the working folder %1").arg(QDir::toNativeSeparators(workdir));
    if (files.isEmpty())
        return {};
    if (!QDir().mkpath(temp_dir))
        return QStringLiteral("Cannot create %1").arg(QDir::toNativeSeparators(temp_dir));
    for (const harness::TempFile& tf : files) {
        const QString path = temp_dir + QLatin1Char('/') + tf.name;
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(tf.content) != tf.content.size() || !f.commit()) {
            for (const QString& p : std::as_const(*written))
                QFile::remove(p);
            written->clear();
            return QStringLiteral("Cannot write the run file %1: %2").arg(tf.name, f.errorString());
        }
        *written << path;
    }
    return {};
}

} // namespace coding_agent_process_detail

using namespace coding_agent_process_detail;

// ── Launch ───────────────────────────────────────────────────────────────────

bool CodingAgentService::launch(const QString& run_id, QString* error, QString* failure) {
    auto fail = [error, failure](const QString& message, harness::FailureKind kind = harness::FailureKind::Other) {
        if (error)
            *error = message;
        if (failure)
            *failure = harness::failure_id(kind);
        return false;
    };
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return fail(QStringLiteral("Run not found."));

    // Everything comes from the registry's cached state — no file-system search here.
    auto& registry = harness::HarnessRegistry::instance();
    const QString harness_id = it->harness;
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_id);
    if (!adapter)
        return fail(QStringLiteral("%1 is not available in this build.").arg(registry.display_name(harness_id)),
                    harness::FailureKind::NotInstalled);
    const harness::HarnessDescriptor& desc = adapter->descriptor();
    const harness::HarnessStatus st = registry.status(harness_id);
    if (!st.installation) {
        if (st.host_blocked) // Flatpak without the host-command permission: say that, not "not installed"
            return fail(st.detect_error, harness::FailureKind::NotInstalled);
        registry.report_failure(harness_id, harness::FailureKind::NotInstalled);
        return fail(QStringLiteral("%1 is not installed on this computer. Install it (%2) or set its path in Agent "
                                   "Workbench › HARNESSES.")
                        .arg(desc.display_name, desc.install_url),
                    harness::FailureKind::NotInstalled);
    }
    const harness::Installation cli = *st.installation;
    // Flatpak: the CLI runs on the user's system; the folders and files it is
    // handed must be spelled so the host finds them (~/.local/share/… of the
    // sandbox is ~/.var/app/<id>/.local/share/… there). Identity elsewhere.
    const QString run_dir = process::host::shared_path(it->workdir);
    const bool resume = !it->parent_run_id.isEmpty() && !it->session_id.isEmpty();

    // ── Terminal tools: the external MCP server must be up ───────────────────
    QString token;
    QString mcp_url;
    const QString server_name = desc.run_mcp_server_name.isEmpty() ? QStringLiteral("fincept") : desc.run_mcp_server_name;
    std::optional<harness::McpEndpoint> endpoint;
    if (it->use_fincept_tools && desc.caps.per_run_mcp) {
        auto& srv = mcp::McpHttpServer::instance();
        if (!srv.is_running() && !srv.start())
            return fail(QStringLiteral("The terminal's MCP server could not start (%1). Open Agent Workbench › CONNECT, "
                                       "or untick “Use Fincept terminal tools”.")
                            .arg(srv.last_error().isEmpty() ? QStringLiteral("unknown error") : srv.last_error()));
        token = srv.token();
        mcp_url = srv.url();
        if (token.isEmpty())
            return fail(QStringLiteral("The terminal's MCP server has no access token."));
        if (mcp_url.isEmpty())
            return fail(QStringLiteral("The terminal's MCP server returned no client configuration."));
        // The token itself never goes into the endpoint: the child reads it from
        // the variable named here.
        harness::McpEndpoint ep;
        ep.name = server_name;
        ep.url = mcp_url;
        ep.token_env_var = QString::fromLatin1(mcp::McpHttpServer::kTokenEnvVar);
        ep.tool_timeout_s = kMcpToolTimeoutSec;
        endpoint = ep;
    }

    // ── System context ───────────────────────────────────────────────────────
    SystemPromptContext ctx;
    ctx.harness = harness_id;
    ctx.permission = it->permission;
    ctx.tools_enabled = endpoint.has_value();
    ctx.mcp_server_name = server_name;
    ctx.workdir = QDir::toNativeSeparators(run_dir);
    ctx.now = QDateTime::currentDateTime();

    // ── Command line (the adapter) ───────────────────────────────────────────
    harness::RunSpec spec;
    spec.prompt = it->prompt;
    spec.system_prompt = build_agent_system_prompt(ctx);
    spec.model = it->model;
    spec.effort = desc.caps.effort ? it->effort : QString();
    spec.cwd = run_dir;
    spec.resume_session_id = resume ? it->session_id : QString();
    spec.permission = it->permission;
    spec.mcp = endpoint;
    spec.max_budget_usd = desc.caps.budget_usd ? it->max_budget_usd : 0.0;
    spec.max_turns = desc.caps.max_turns ? it->max_turns : 0;
    spec.want_deltas = desc.caps.text_deltas;
    spec.run_id = run_id;
    spec.temp_dir = process::host::shared_path(AppPaths::cache() + QStringLiteral("/coding_agents"));
    spec.api_key_env_present = registry.api_key_reaches_child(harness_id);
    // Entry NAMES of the user's own MCP config that the CLI would merge into this
    // run (Codex): the adapter switches them off so the run sees the terminal's
    // server only — never the other Fincept edition's. A small local file (in
    // the Flatpak: read on the host by the registry, see user_mcp_server_names()).
    spec.user_mcp_server_names = registry.user_mcp_server_names(harness_id);
    const harness::LaunchPlan plan = adapter->build_command(spec, cli);
    if (!plan.error.isEmpty())
        return fail(plan.error);
    const harness::LaunchSpec launch_spec = harness::make_launch_spec(cli, plan.args);
    if (!launch_spec.error.isEmpty())
        return fail(launch_spec.error);

    // ── The live run (the process starts once its files exist) ───────────────
    auto* live = new LiveRun;
    live->run_id = run_id;
    live->harness = harness_id;
    live->parser = adapter->make_parser();
    live->cancel = adapter->cancel_plan();
    live->spec = launch_spec;
    // THE child environment: registry policy (API-key stripping), the adapter's
    // env_set / env_unset and the MCP token — the only place the token goes.
    live->env = registry.child_environment(harness_id, cli, &plan, token);
    live->host_env_names = harness::host_env_names(desc, &plan);
    live->workdir = run_dir;
    live->display_name = desc.display_name;
    live->version = cli.version;
    live->resume = resume;
    live->stdin_payload = plan.stdin_payload;
    live->shown_command = shown_command(cli.path, plan, token);
    live->tools_note = endpoint ? QStringLiteral("%1 (server `%2`)").arg(mcp_url, server_name) : QStringLiteral("off");
    const qint64 limit_ms = it->time_limit_s > 0 ? static_cast<qint64>(it->time_limit_s) * 1000 : kCodingAgentMaxRunMs;
    live->time_limit_ms = qMin(limit_ms, kCodingAgentMaxRunMs);
    if (!live->parser) {
        delete live;
        return fail(QStringLiteral("%1 has no output parser.").arg(desc.display_name));
    }
    live_.insert(run_id, live); // occupies a slot from now on

    it->status = RunStatus::Running;
    it->started_at_ms = QDateTime::currentMSecsSinceEpoch();
    it->harness_version = cli.version;
    it->error.clear();
    it->failure.clear();
    command_lines_.insert(run_id, live->shown_command);
    persist_run(run_id);
    // `it` is not used past this point: slots of the signals below may touch runs_.
    emit run_updated(run_id);

    // Folder + per-run files on the file thread; back on the GUI thread to start.
    const QString workdir = live->workdir;
    const QString temp_dir = spec.temp_dir;
    const QVector<harness::TempFile> files = plan.temp_files;
    QPointer<CodingAgentService> self(this);
    submit_file_job([self, run_id, workdir, temp_dir, files]() {
        QStringList written;
        const QString err = prepare_run_files(workdir, temp_dir, files, &written);
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, run_id, err, written]() {
                if (self)
                    self->on_launch_prepared(run_id, err.isEmpty(), err, written);
                else
                    for (const QString& p : written)
                        QFile::remove(p); // shutdown race: tiny files, the app is ending
            },
            Qt::QueuedConnection);
    });
    return true;
}

void CodingAgentService::on_launch_prepared(const QString& run_id, bool ok, const QString& error,
                                            const QStringList& temp_paths) {
    LiveRun* live = live_.value(run_id);
    if (!live || !live->preparing || shutting_down_) {
        // Stopped (or the app is closing) while its files were being prepared.
        if (!temp_paths.isEmpty())
            submit_file_job([temp_paths]() {
                for (const QString& p : temp_paths)
                    QFile::remove(p);
            });
        return;
    }
    live->preparing = false;
    live->temp_paths = temp_paths;
    if (!ok) {
        finish_run(run_id, RunStatus::Failed, error, harness::failure_id(harness::FailureKind::Other));
        return;
    }
    const auto it = runs_.constFind(run_id);
    if (it == runs_.cend())
        return;

    // Windows: kill-on-close job; the child is created suspended and joins it in
    // on_proc_started() before it runs (nothing it spawns can escape). POSIX: own session.
    live->job = harness::create_kill_on_close_job();
    auto* proc = new QProcess(this);
    live->proc = proc;
    proc->setWorkingDirectory(live->workdir);
    proc->setProcessEnvironment(live->env);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    // The launch boundary (Flatpak: the CLI runs on the user's system).
    harness::apply_launch_spec(proc, live->spec, /*own_tree=*/true, live->host_env_names);

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

    // The run's time limit, for every harness alike.
    auto* watchdog = new QTimer(this);
    watchdog->setSingleShot(true);
    live->watchdog = watchdog;
    connect(watchdog, &QTimer::timeout, this, [this, run_id]() {
        LiveRun* l = live_.value(run_id);
        if (!l)
            return;
        l->timed_out = true;
        record_status(run_id, QStringLiteral("Time limit reached"),
                      QStringLiteral("Stopping the agent after %1 minutes.").arg(l->time_limit_ms / 60000), true);
        if (l->proc && l->cancel.close_stdin)
            l->proc->closeWriteChannel();
        kill_tree(l);
    });

    write_prompt_file(*it);
    const QString version = live->version.isEmpty() ? QString() : QLatin1Char(' ') + live->version;
    record_status(run_id,
                  QStringLiteral("Starting %1%2%3").arg(live->display_name, version,
                                                         live->resume ? QStringLiteral(" (resume)") : QString()),
                  QStringLiteral("Command: %1\nWorking folder: %2\nTerminal tools: %3")
                      .arg(live->shown_command, QDir::toNativeSeparators(live->workdir), live->tools_note));
    LOG_INFO(kCodingAgentProcTag,
             QStringLiteral("Run %1: starting %2 in %3").arg(run_id.left(8), live->display_name, live->workdir));

    watchdog->start(static_cast<int>(qMin<qint64>(live->time_limit_ms, std::numeric_limits<int>::max())));
    proc->start(); // may fail synchronously → on_proc_failed_to_start → finish_run; `live` must not be touched after this
}

void CodingAgentService::on_proc_started(const QString& run_id) {
    LiveRun* live = live_.value(run_id);
    if (!live || !live->proc)
        return;
    live->pid = live->proc->processId();
    // Windows: the child was created suspended — join the job, then resume.
    const harness::TreeStart tree = harness::adopt_started_process(live->proc, live->job);
    live->job_assigned = tree.assigned;
    if (!live->job_assigned)
        LOG_WARN(kCodingAgentProcTag, QStringLiteral("Run %1: job object assignment failed — Stop kills only the CLI "
                                                     "process, not its children")
                                          .arg(run_id.left(8)));
    if (!tree.resumed) {
        LOG_WARN(kCodingAgentProcTag, QStringLiteral("Run %1: the agent process could not be resumed").arg(run_id.left(8)));
        record_status(run_id, QStringLiteral("Could not start the agent"),
                      QStringLiteral("The agent process was created but could not be resumed; it is being stopped."),
                      true);
        kill_tree(live);
        return;
    }
    // The prompt travels over stdin when the adapter puts it there: no quoting
    // or length limits, never in the process list. Closing stdin tells the CLI
    // the prompt is complete (an adapter that keeps a session open says so).
    if (!live->stdin_payload.isEmpty())
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
    if (!live || it == runs_.end() || !live->parser)
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
    if (f.input_tokens != live->synced_in || f.output_tokens != live->synced_out ||
        f.cached_tokens != live->synced_cached) {
        live->synced_in = f.input_tokens;
        live->synced_out = f.output_tokens;
        live->synced_cached = f.cached_tokens;
        it->input_tokens = f.input_tokens;
        it->output_tokens = f.output_tokens;
        it->cached_tokens = f.cached_tokens;
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
    // Drain the pipes, including an unterminated last line, then let the parser
    // flush whatever it still assembles from deltas.
    on_proc_stdout(run_id);
    on_proc_stderr(run_id);
    const QByteArray out_rest = live->out_buf.take_rest();
    if (!out_rest.trimmed().isEmpty()) {
        for (const auto& ev : live->parse(QString::fromUtf8(out_rest)))
            record_event(run_id, ev);
    }
    for (const auto& ev : live->parser->finish())
        record_event(run_id, ev);
    const QString err_rest = QString::fromUtf8(live->err_buf.take_rest()).trimmed();
    if (!err_rest.isEmpty()) {
        live->stderr_tail.append(err_rest.left(2000));
        record_status(run_id, QStringLiteral("stderr"), err_rest, stderr_looks_like_error(err_rest));
    }
    sync_facts(run_id);

    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    auto& registry = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* adapter = registry.adapter(it->harness);
    const StreamFacts& f = live->facts();
    const QString name = live->display_name;
    it->exit_code = exit_code;
    if (!f.final_text.isEmpty())
        it->final_text = f.final_text;

    RunStatus status = RunStatus::Failed;
    QString err;
    harness::FailureKind kind = harness::FailureKind::Other;
    if (live->stop_requested) {
        status = RunStatus::Cancelled;
        err = QStringLiteral("Stopped by the user.");
        kind = harness::FailureKind::None;
    } else if (live->timed_out) {
        kind = harness::FailureKind::Timeout;
        err = QStringLiteral("Timed out — stopped after the %1-minute time limit.").arg(live->time_limit_ms / 60000);
    } else {
        kind = adapter ? adapter->classify_failure(crashed ? -1 : exit_code, f, live->stderr_tail)
                       : harness::FailureKind::Other;
        if (!crashed && kind == harness::FailureKind::None) {
            status = RunStatus::Succeeded;
        } else {
            if (kind == harness::FailureKind::None)
                kind = harness::FailureKind::Other; // crashed after a clean-looking stream
            QString detail;
            if (!f.result_error.isEmpty())
                detail = f.result_error;
            else if (!f.last_error.isEmpty())
                detail = f.last_error;
            else if (crashed)
                detail = QStringLiteral("%1 crashed.").arg(name);
            else if (exit_code != 0)
                detail = QStringLiteral("%1 exited with code %2.").arg(name).arg(exit_code);
            else
                detail = QStringLiteral("%1 exited without producing a result.").arg(name);
            if (f.result_error.isEmpty() && !live->stderr_tail.isEmpty()) {
                const qsizetype from = qMax<qsizetype>(0, live->stderr_tail.size() - 8);
                detail += QStringLiteral("\n\nLast stderr output:\n") + live->stderr_tail.mid(from).join(QLatin1Char('\n'));
            }
            const QString lead = adapter ? failure_sentence(kind, adapter->descriptor(), f, it->model) : QString();
            err = lead.isEmpty() ? detail : lead + QStringLiteral("\n\n") + detail;
        }
    }
    // Feed the registry: a sign-in re-check, a cooldown, a re-detection — or clear them.
    const QString harness_id = it->harness;
    if (status == RunStatus::Succeeded)
        registry.report_success(harness_id);
    else if (kind == harness::FailureKind::Auth || kind == harness::FailureKind::RateLimit ||
             kind == harness::FailureKind::NotInstalled || kind == harness::FailureKind::Model)
        registry.report_failure(harness_id, kind, f.rate_limit_resets_at_ms);
    finish_run(run_id, status, err, status == RunStatus::Succeeded ? QString() : harness::failure_id(kind));
}

void CodingAgentService::on_proc_failed_to_start(const QString& run_id, const QString& message) {
    const CodingAgentRun* run = find_run(run_id);
    if (!run)
        return;
    auto& registry = harness::HarnessRegistry::instance();
    const QString harness_id = run->harness;
    const harness::HarnessStatus st = registry.status(harness_id);
    const QString path = st.installation ? st.installation->path : QString();
    registry.report_failure(harness_id, harness::FailureKind::NotInstalled); // re-detects
    finish_run(run_id, RunStatus::Failed,
               QStringLiteral("%1 could not be started (%2): %3")
                   .arg(registry.display_name(harness_id), QDir::toNativeSeparators(path), message),
               harness::failure_id(harness::FailureKind::NotInstalled));
}

// ── Stop / kill ──────────────────────────────────────────────────────────────

void CodingAgentService::terminate_tree(LiveRun* live) {
    if (!live)
        return;
    // POSIX: SIGTERM to the process group (Flatpak: also the CLI's group on the
    // host). Windows: console processes have no window to receive WM_CLOSE — the
    // graceful step is the stdin close the caller already did; kill_tree follows.
    harness::signal_tree(live->proc.data(), live->pid, /*force=*/false);
}

void CodingAgentService::kill_tree(LiveRun* live) {
    if (!live)
        return;
    if (live->job && live->job_assigned)
        harness::terminate_job(live->job); // Windows: the whole tree, npm shim's node included
    // POSIX: SIGKILL to the process group (Flatpak: the host group first).
    harness::signal_tree(live->proc.data(), live->pid, /*force=*/true);
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
    harness::close_job(live->job); // kill-on-close: reaps anything the agent left behind
    live->job = nullptr;
    if (!live->temp_paths.isEmpty()) {
        const QStringList paths = live->temp_paths;
        if (immediate || shutting_down_) {
            for (const QString& p : paths)
                QFile::remove(p); // teardown: a few small files in the app's own cache
        } else {
            submit_file_job([paths]() {
                for (const QString& p : paths)
                    QFile::remove(p);
            });
        }
    }
    stderr_tails_.insert(run_id, live->stderr_tail);
    delete live;
}

// ── External clients (CONNECT) ───────────────────────────────────────────────

void CodingAgentService::run_cli_async(const QString& harness_id, const QStringList& args, bool with_token,
                                       std::function<void(int, const QString&)> done) {
    auto& registry = harness::HarnessRegistry::instance();
    const harness::HarnessStatus st = registry.status(harness_id);
    const std::optional<harness::Installation> cli = st.installation;
    if (!cli) {
        const QString why = st.host_blocked ? st.detect_error
                                            : QStringLiteral("%1 is not installed on this computer.")
                                                  .arg(registry.display_name(harness_id));
        QTimer::singleShot(0, this, [done, why]() { done(-1, why); });
        return;
    }
    const harness::LaunchSpec spec = harness::make_launch_spec(*cli, args);
    if (!spec.error.isEmpty()) {
        QTimer::singleShot(0, this, [done, err = spec.error]() { done(-1, err); });
        return;
    }
    auto* proc = new QProcess(this);
    // The token only for a command that connects to the server (a health check);
    // registration itself writes the variable's NAME, never its value.
    const QString token = with_token ? mcp::McpHttpServer::instance().token() : QString();
    proc->setProcessEnvironment(registry.child_environment(harness_id, *cli, nullptr, token));
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setStandardInputFile(QProcess::nullDevice());
    // Flatpak: `claude mcp add` & co. change the user's config on the host, where the CLI is.
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_id);
    harness::apply_launch_spec(proc, spec, /*own_tree=*/false,
                               adapter ? harness::host_env_names(adapter->descriptor()) : QStringList{});
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

namespace coding_agent_process_detail {

std::optional<harness::McpRegistration> client_registration(const QString& harness_id,
                                                            const char* server_name = kClientServerName) {
    const harness::IHarnessAdapter* adapter = harness::HarnessRegistry::instance().adapter(harness_id);
    if (!adapter)
        return std::nullopt;
    auto& srv = mcp::McpHttpServer::instance();
    harness::McpEndpoint ep;
    ep.name = QString::fromLatin1(server_name);
    ep.url = srv.url();
    ep.token_env_var = QString::fromLatin1(mcp::McpHttpServer::kTokenEnvVar);
    ep.tool_timeout_s = kMcpToolTimeoutSec;
    return adapter->mcp_registration(ep);
}

} // namespace coding_agent_process_detail

void CodingAgentService::register_with_client(const QString& harness_id, bool replace_existing) {
    auto& registry = harness::HarnessRegistry::instance();
    const QString name = registry.display_name(harness_id);
    if (const harness::HarnessStatus st = registry.status(harness_id); !st.installation) {
        emit client_registration_finished(harness_id, false, false,
                                          st.host_blocked ? st.detect_error
                                                          : QStringLiteral("%1 is not installed on this computer.").arg(name));
        return;
    }
    auto& srv = mcp::McpHttpServer::instance();
    if (!srv.is_running() && !srv.start()) {
        emit client_registration_finished(harness_id, false, false,
                                          QStringLiteral("The terminal's MCP server is not running: %1").arg(srv.last_error()));
        return;
    }
    const std::optional<harness::McpRegistration> reg = client_registration(harness_id);
    if (!reg || !reg->add) {
        emit client_registration_finished(harness_id, false, false,
                                          QStringLiteral("%1 has no one-click registration that keeps the token off its "
                                                         "command line — use the command shown instead.")
                                              .arg(name));
        return;
    }

    // The registration under the edition's name (after the rename step below).
    auto proceed = [this, harness_id, replace_existing]() {
        const std::optional<harness::McpRegistration> current = client_registration(harness_id);
        if (!current || !current->add) {
            emit client_registration_finished(harness_id, false, false,
                                              with_migration_note(harness_id, QStringLiteral("Nothing to run.")));
            return;
        }
        if (replace_existing) {
            if (current->remove)
                run_cli_async(harness_id, current->remove->args, false,
                              [this, harness_id](int, const QString&) { add_to_client(harness_id); });
            else
                add_to_client(harness_id);
            return;
        }
        if (!current->check) {
            add_to_client(harness_id);
            return;
        }
        const QString token = mcp::McpHttpServer::instance().token();
        run_cli_async(harness_id, current->check->args, false, [this, harness_id, token](int code, const QString& out) {
            if (code == 0) {
                emit client_registration_finished(
                    harness_id, false, true,
                    with_migration_note(harness_id, mask_secrets(out, token).trimmed().left(2000)));
                return;
            }
            add_to_client(harness_id);
        });
    };

    // One-time rename of this edition's pre-split entry ("fincept_terminal"): only
    // when the harness reports that it points at THIS edition's loopback port —
    // an entry pointing anywhere else (the private build listens on 47821) is
    // left untouched. The output is parsed for its URL only and never logged.
    const std::optional<harness::McpRegistration> legacy =
        client_registration(harness_id, edition::kLegacyMcpServerName);
    if (!legacy || !legacy->check || !legacy->remove) {
        proceed();
        return;
    }
    const quint16 own_port = srv.port() != 0 ? srv.port() : srv.configured_port();
    const QStringList remove_args = legacy->remove->args;
    run_cli_async(harness_id, legacy->check->args, false,
                  [this, harness_id, own_port, remove_args, proceed](int code, const QString& out) {
                      const QString old_name = QString::fromLatin1(edition::kLegacyMcpServerName);
                      const QString new_name = QString::fromLatin1(kClientServerName);
                      switch (harness::classify_legacy_entry(code, out, own_port)) {
                          case harness::LegacyEntryAction::Absent:
                              proceed();
                              return;
                          case harness::LegacyEntryAction::LeaveForeign:
                              migration_notes().insert(
                                  harness_id, QStringLiteral("An older entry named “%1” points somewhere other than this "
                                                             "terminal, so it was left as it is.")
                                                  .arg(old_name));
                              LOG_INFO(kCodingAgentProcTag,
                                       logf("Pre-split MCP entry left untouched", {{"harness", harness_id},
                                                                                   {"reason", "not this edition's port"}}));
                              proceed();
                              return;
                          case harness::LegacyEntryAction::Migrate:
                              break;
                      }
                      run_cli_async(harness_id, remove_args, false,
                                    [harness_id, old_name, new_name, proceed](int rc, const QString&) {
                                        migration_notes().insert(
                                            harness_id,
                                            rc == 0 ? QStringLiteral("Renamed this terminal's older entry “%1” to “%2”.")
                                                          .arg(old_name, new_name)
                                                    : QStringLiteral("Could not remove this terminal's older entry “%1”; "
                                                                     "remove it yourself.")
                                                          .arg(old_name));
                                        LOG_INFO(kCodingAgentProcTag,
                                                 logf("Pre-split MCP entry renamed",
                                                      {{"harness", harness_id}, {"ok", rc == 0}}));
                                        proceed();
                                    });
                  });
}

void CodingAgentService::add_to_client(const QString& harness_id) {
    const std::optional<harness::McpRegistration> reg = client_registration(harness_id);
    if (!reg || !reg->add) {
        emit client_registration_finished(harness_id, false, false,
                                          with_migration_note(harness_id, QStringLiteral("Nothing to run.")));
        return;
    }
    const QString token = mcp::McpHttpServer::instance().token();
    const QString note = reg->note;
    const std::optional<harness::ProbeCommand> check = reg->check;
    run_cli_async(harness_id, reg->add->args, false,
                  [this, harness_id, token, note, check](int code, const QString& out) {
                      QString msg = mask_secrets(out, token).trimmed();
                      const bool ok = code == 0;
                      if (ok && msg.isEmpty())
                          msg = QStringLiteral("Added the `%1` MCP server.").arg(QString::fromLatin1(kClientServerName));
                      else if (!ok && msg.isEmpty())
                          msg = QStringLiteral("Exit code %1.").arg(code);
                      if (ok && !note.isEmpty())
                          msg += QLatin1Char('\n') + note;
                      msg = with_migration_note(harness_id, msg);
                      const bool exists = !ok && msg.contains(QLatin1String("already exists"), Qt::CaseInsensitive);
                      LOG_INFO(kCodingAgentProcTag, QStringLiteral("Register with %1: %2")
                                                        .arg(harness_id, ok ? QStringLiteral("ok") : QStringLiteral("failed")));
                      if (!ok || !check) {
                          emit client_registration_finished(harness_id, ok, exists, msg.left(2000));
                          return;
                      }
                      // Show what the harness itself now reports for the entry (a
                      // connection check where it has one), with the token in its
                      // environment the way the user's own sessions will need it.
                      run_cli_async(harness_id, check->args, true,
                                    [this, harness_id, msg, token](int, const QString& check_out) {
                                        QString full = msg;
                                        const QString shown = mask_secrets(check_out, token).trimmed();
                                        if (!shown.isEmpty())
                                            full += QStringLiteral("\n\nCheck:\n") + shown.left(1200);
                                        emit client_registration_finished(harness_id, true, false, full.left(2400));
                                    });
                  });
}

} // namespace fincept::services::coding_agents
