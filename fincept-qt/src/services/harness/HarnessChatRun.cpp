// HarnessChatRun.cpp — one terminal-AI call through a harness CLI (see the header).

#include "services/harness/HarnessChatRun.h"

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/HarnessProcessPool.h"
#include "services/harness/IHarnessAdapter.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QTimer>
#include <QUuid>

#include <memory>

namespace fincept::harness {

namespace harness_chat_run_detail {

namespace hcr = fincept::services::coding_agents;

constexpr int kChatPollMs = 100;
constexpr int kChatStderrTailLines = 40;
constexpr int kChatHistoryTurnMaxChars = 24000; // one earlier turn inside the flattened prompt

QString hcr_role_label(const QString& role) {
    if (role == QLatin1String("assistant"))
        return QStringLiteral("Assistant");
    return QStringLiteral("User");
}

/// Appends one streamed message (Text or Thinking) to its channel: partial
/// events carry the message so far, the complete event the whole message —
/// only what was not sent yet goes out.
class HcrStreamChannel {
  public:
    /// Returns the delta to emit ("" = nothing new).
    QString take(const hcr::AgentEvent& ev) {
        const QString& body = ev.body;
        QString delta;
        const auto it = ev.ref_id.isEmpty() ? sent_.end() : sent_.find(ev.ref_id);
        if (it != sent_.end()) {
            // More of a message already streaming on this channel.
            const qsizetype already = it.value();
            if (body.size() <= already)
                return {};
            it.value() = body.size();
            delta = body.mid(already);
        } else {
            // A new message: separated from the previous one on this channel.
            if (body.trimmed().isEmpty())
                return {};
            if (!ev.ref_id.isEmpty())
                sent_.insert(ev.ref_id, body.size());
            delta = (!any_ || last_ends_with_newline_) ? body : QStringLiteral("\n\n") + body;
        }
        any_ = true;
        last_ends_with_newline_ = delta.endsWith(QLatin1Char('\n'));
        return delta;
    }
    void reset() {
        any_ = false;
        last_ends_with_newline_ = false;
    }

  private:
    QHash<QString, qsizetype> sent_;
    bool any_ = false;
    bool last_ends_with_newline_ = false;
};

QString hcr_failure_sentence(FailureKind kind, const QString& name, const QString& model) {
    switch (kind) {
        case FailureKind::Auth:
            return QStringLiteral("%1 is not signed in, or its API key was refused.").arg(name);
        case FailureKind::RateLimit:
            return QStringLiteral("%1 is rate limited.").arg(name);
        case FailureKind::Model:
            return model.isEmpty() ? QStringLiteral("%1 does not offer the requested model.").arg(name)
                                   : QStringLiteral("%1 does not offer the model “%2”.").arg(name, model);
        case FailureKind::NotInstalled:
            return QStringLiteral("%1 could not be started.").arg(name);
        case FailureKind::Timeout:
            return QStringLiteral("%1 did not answer in time.").arg(name);
        case FailureKind::Other:
        case FailureKind::None:
            break;
    }
    return QStringLiteral("%1 could not answer.").arg(name);
}

} // namespace harness_chat_run_detail

using namespace harness_chat_run_detail;

QString flatten_chat_prompt(const QVector<ChatTurn>& history, const QString& user_message) {
    QStringList turns;
    for (const ChatTurn& t : history) {
        if (t.role == QLatin1String("system") || t.content.trimmed().isEmpty())
            continue;
        QString text = t.content;
        if (text.size() > kChatHistoryTurnMaxChars)
            text = text.left(kChatHistoryTurnMaxChars) + QStringLiteral(" …[shortened]");
        turns << QStringLiteral("%1: %2").arg(hcr_role_label(t.role), text);
    }
    if (turns.isEmpty())
        return user_message;
    return QStringLiteral("Conversation so far (for context; answer only the new message):\n\n%1\n\n"
                          "New message from the user:\n%2")
        .arg(turns.join(QStringLiteral("\n\n")), user_message);
}

QString chat_system_prompt(const QString& base, const QVector<ChatTurn>& history) {
    QStringList parts;
    if (!base.trimmed().isEmpty())
        parts << base.trimmed();
    for (const ChatTurn& t : history) {
        if (t.role == QLatin1String("system") && !t.content.trimmed().isEmpty())
            parts << t.content.trimmed();
    }
    return parts.join(QStringLiteral("\n\n"));
}

HarnessChatResult run_harness_chat(const HarnessChatEnvironment& env, const HarnessChatRequest& req,
                                   const HarnessChatSink& sink) {
    HarnessChatResult r;
    QElapsedTimer clock;
    clock.start();
    const auto log = [&env](const QString& m) {
        if (env.log)
            env.log(m);
    };
    const auto is_cancelled = [&sink]() { return sink.cancelled && sink.cancelled(); };

    if (!env.adapter) {
        r.failure = FailureKind::NotInstalled;
        r.error = QStringLiteral("This harness is not available in this build.");
        return r;
    }
    const HarnessDescriptor& desc = env.adapter->descriptor();
    const QString name = desc.display_name;

    // ── A process slot (terminal AI never runs more than the pool allows) ──
    std::unique_ptr<HarnessProcessPool::Slot> slot;
    if (env.pool) {
        slot = std::make_unique<HarnessProcessPool::Slot>(*env.pool, sink.cancelled, env.pool_wait_ms);
        if (!slot->held()) {
            if (is_cancelled()) {
                r.cancelled = true;
                r.error = QStringLiteral("Request cancelled");
            } else {
                r.failure = FailureKind::Timeout;
                r.error = QStringLiteral("%1 is busy with other terminal-AI requests.").arg(name);
            }
            r.duration_ms = clock.elapsed();
            return r;
        }
    }
    if (is_cancelled()) {
        r.cancelled = true;
        r.error = QStringLiteral("Request cancelled");
        return r;
    }

    // ── A fresh, empty working folder — never a folder of the user's ──
    const QString run_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = QDir::cleanPath(env.scratch_root) + QLatin1Char('/') + run_id;
    if (env.scratch_root.isEmpty() || !QDir().mkpath(dir)) {
        r.failure = FailureKind::Other;
        r.error = QStringLiteral("The working folder for %1 could not be created.").arg(name);
        return r;
    }
    struct DirGuard {
        QString path;
        ~DirGuard() { QDir(path).removeRecursively(); }
    } dir_guard{dir};

    // ── The command line (the adapter refuses what it cannot keep read-only) ──
    RunSpec spec;
    spec.prompt = flatten_chat_prompt(req.history, req.user_message);
    spec.system_prompt = chat_system_prompt(req.system_prompt, req.history);
    spec.model = req.model;
    spec.effort = desc.caps.effort ? req.effort : QString();
    spec.cwd = dir;
    spec.permission = PermissionMode::ReadOnly;
    if (req.tools != ChatTools::None && env.mcp)
        spec.mcp = env.mcp;
    spec.chat = true;
    spec.chat_persist_session = req.persist_session;
    spec.resume_session_id = req.persist_session ? req.resume_session_id : QString();
    spec.want_deltas = desc.caps.text_deltas;
    spec.run_id = run_id;
    spec.temp_dir = dir;
    spec.json_schema = req.json_schema;
    spec.api_key_env_present = true;
    const LaunchPlan plan = env.adapter->build_command(spec, env.installation);
    if (!plan.error.isEmpty()) {
        r.failure = FailureKind::Other;
        r.error = plan.error;
        return r;
    }
    for (const TempFile& tf : plan.temp_files) {
        QFile f(dir + QLatin1Char('/') + tf.name);
        if (!f.open(QIODevice::WriteOnly) || f.write(tf.content) != tf.content.size()) {
            r.failure = FailureKind::Other;
            r.error = QStringLiteral("A file for %1's call could not be written.").arg(name);
            return r;
        }
    }
    const LaunchSpec launch = make_launch_spec(env.installation, plan.args);
    if (!launch.error.isEmpty()) {
        r.failure = FailureKind::NotInstalled;
        r.error = launch.error;
        return r;
    }
    QProcessEnvironment child_env;
    if (env.environment) {
        child_env = env.environment(plan);
    } else {
        child_env = QProcessEnvironment::systemEnvironment();
        for (const QString& k : plan.env_unset)
            child_env.remove(k);
        for (auto it = plan.env_set.cbegin(); it != plan.env_set.cend(); ++it)
            child_env.insert(it.key(), it.value());
    }

    // ── The process, driven by a local event loop on this (worker) thread ──
    std::unique_ptr<IStreamParser> parser = env.adapter->make_parser();
    if (!parser) {
        r.failure = FailureKind::Other;
        r.error = QStringLiteral("%1 has no output parser.").arg(name);
        return r;
    }
    const CancelPlan cancel_plan = env.adapter->cancel_plan();
    void* job = create_kill_on_close_job();
    QEventLoop loop;
    QProcess proc;
    proc.setWorkingDirectory(dir);
    proc.setProcessEnvironment(child_env);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    // The launch boundary (Flatpak: the CLI runs on the user's system).
    apply_launch_spec(&proc, launch, /*own_tree=*/true, host_env_names(desc, &plan));

    hcr::LineBuffer out_buf;
    hcr::LineBuffer err_buf;
    QStringList stderr_tail;
    HcrStreamChannel text_channel;
    HcrStreamChannel thinking_channel;
    QString segment_text; // answer text since the last tool call
    QHash<QString, QString> tool_names;
    qint64 pid = 0;
    bool job_assigned = false;
    bool done = false;
    bool failed_to_start = false;
    QString start_error;
    bool saw_line = false;
    bool timed_out_first = false;
    bool timed_out_total = false;
    bool cancel_started = false;
    bool killed = false;
    bool refused = false;
    QString refusal;
    int exit_code = -1;
    bool crashed = false;

    QTimer first_event_timer;
    first_event_timer.setSingleShot(true);
    QTimer total_timer;
    total_timer.setSingleShot(true);
    QTimer grace_timer;
    grace_timer.setSingleShot(true);
    QTimer poll_timer;
    poll_timer.setInterval(kChatPollMs);

    const auto finish_loop = [&]() {
        done = true;
        loop.quit();
    };
    const auto kill_tree = [&]() {
        killed = true;
        if (job && job_assigned)
            terminate_job(job);
        signal_tree(&proc, pid, /*force=*/true); // Flatpak: the CLI's host group too
        if (proc.state() != QProcess::NotRunning)
            proc.kill();
    };
    const auto mark_output = [&r]() { r.output_started = true; };

    const auto dispatch = [&](const hcr::AgentEvent& ev) {
        if (sink.event)
            sink.event(ev);
        switch (ev.kind) {
            case hcr::EventKind::Text: {
                const QString delta = text_channel.take(ev);
                if (delta.isEmpty())
                    break;
                segment_text += delta;
                mark_output();
                if (sink.text)
                    sink.text(delta);
                break;
            }
            case hcr::EventKind::Thinking: {
                const QString delta = thinking_channel.take(ev);
                if (delta.isEmpty())
                    break;
                mark_output();
                if (sink.thinking)
                    sink.thinking(delta);
                break;
            }
            case hcr::EventKind::ToolCall: {
                if (!segment_text.isEmpty()) {
                    // Narration before a tool call: the answer comes after the tools.
                    segment_text.clear();
                    text_channel.reset();
                    if (sink.text_reset)
                        sink.text_reset();
                }
                const QString tool = chat_tool_name(ev.title);
                if (!ev.ref_id.isEmpty())
                    tool_names.insert(ev.ref_id, tool);
                mark_output();
                if (sink.tool_call)
                    sink.tool_call(ev.ref_id, tool, ev.body);
                break;
            }
            case hcr::EventKind::ToolResult: {
                const QString tool = tool_names.value(ev.ref_id, chat_tool_name(ev.title));
                mark_output();
                if (sink.tool_result)
                    sink.tool_result(ev.ref_id, tool, !ev.is_error, ev.body);
                break;
            }
            default:
                break;
        }
    };

    const auto handle_line = [&](const QString& line) {
        if (line.trimmed().isEmpty() || refused)
            return;
        if (!saw_line) {
            saw_line = true;
            first_event_timer.stop();
        }
        if (env.line_guard && !r.output_started) {
            const QString why = env.line_guard(line);
            if (!why.isEmpty()) {
                refused = true;
                refusal = why;
                log(QStringLiteral("%1 call refused before any output: no API key in use").arg(desc.id));
                kill_tree();
                return;
            }
        }
        const QVector<hcr::AgentEvent> events = parser->parse_line(line);
        for (const auto& ev : events)
            dispatch(ev);
    };

    const auto drain_stdout = [&]() {
        const QList<QByteArray> lines = out_buf.feed(proc.readAllStandardOutput());
        for (const QByteArray& l : lines)
            handle_line(QString::fromUtf8(l));
    };
    const auto drain_stderr = [&]() {
        const QList<QByteArray> lines = err_buf.feed(proc.readAllStandardError());
        for (const QByteArray& l : lines) {
            const QString text = QString::fromUtf8(l).trimmed();
            if (text.isEmpty())
                continue;
            stderr_tail.append(text.left(2000));
            while (stderr_tail.size() > kChatStderrTailLines)
                stderr_tail.removeFirst();
        }
    };

    QObject::connect(&proc, &QProcess::started, &loop, [&]() {
        pid = proc.processId();
        const TreeStart tree = adopt_started_process(&proc, job);
        job_assigned = tree.assigned;
        if (!tree.resumed) {
            start_error = QStringLiteral("The process was created but could not be resumed.");
            kill_tree();
            return;
        }
        if (!plan.stdin_payload.isEmpty())
            proc.write(plan.stdin_payload);
        if (!plan.keep_stdin_open)
            proc.closeWriteChannel();
        first_event_timer.start(qMax(1000, req.first_event_timeout_ms));
    });
    QObject::connect(&proc, &QProcess::readyReadStandardOutput, &loop, drain_stdout);
    QObject::connect(&proc, &QProcess::readyReadStandardError, &loop, drain_stderr);
    QObject::connect(&proc, &QProcess::finished, &loop, [&](int code, QProcess::ExitStatus status) {
        exit_code = code;
        crashed = status == QProcess::CrashExit;
        finish_loop();
    });
    QObject::connect(&proc, &QProcess::errorOccurred, &loop, [&](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || done)
            return;
        failed_to_start = true;
        start_error = proc.errorString();
        finish_loop();
    });
    QObject::connect(&first_event_timer, &QTimer::timeout, &loop, [&]() {
        if (saw_line || done)
            return;
        timed_out_first = true;
        log(QStringLiteral("%1 call: no output within %2 s — stopping").arg(desc.id).arg(req.first_event_timeout_ms / 1000));
        kill_tree();
    });
    QObject::connect(&total_timer, &QTimer::timeout, &loop, [&]() {
        if (done)
            return;
        timed_out_total = true;
        log(QStringLiteral("%1 call: time limit of %2 s reached — stopping").arg(desc.id).arg(req.timeout_ms / 1000));
        kill_tree();
    });
    QObject::connect(&grace_timer, &QTimer::timeout, &loop, [&]() {
        if (!done)
            kill_tree();
    });
    QObject::connect(&poll_timer, &QTimer::timeout, &loop, [&]() {
        if (done || cancel_started || !is_cancelled())
            return;
        cancel_started = true;
        log(QStringLiteral("%1 call: cancelled — stopping the process tree").arg(desc.id));
        // The adapter's plan: close stdin, a graceful signal (POSIX), then the tree.
        if (cancel_plan.close_stdin && proc.state() != QProcess::NotRunning)
            proc.closeWriteChannel();
        signal_tree(&proc, pid, /*force=*/false);
        grace_timer.start(qMax(0, cancel_plan.grace_ms));
    });

    total_timer.start(qMax(1000, req.timeout_ms));
    poll_timer.start();
    proc.start();
    if (!done)
        loop.exec();
    poll_timer.stop();
    first_event_timer.stop();
    total_timer.stop();
    grace_timer.stop();

    // ── Drain everything, including an unterminated last line ──
    if (!failed_to_start) {
        if (proc.state() != QProcess::NotRunning) {
            // finished() was not seen (should not happen): make sure nothing is left behind.
            kill_tree();
            proc.waitForFinished(2000);
        }
        drain_stdout();
        drain_stderr();
        const QByteArray rest = out_buf.take_rest();
        if (!rest.trimmed().isEmpty())
            handle_line(QString::fromUtf8(rest));
        if (!refused) {
            for (const auto& ev : parser->finish())
                dispatch(ev);
        }
        const QString err_rest = QString::fromUtf8(err_buf.take_rest()).trimmed();
        if (!err_rest.isEmpty())
            stderr_tail.append(err_rest.left(2000));
    }
    close_job(job); // kill-on-close: reaps anything the call left behind
    job = nullptr;

    // ── Outcome ──
    const services::coding_agents::StreamFacts& f = parser->facts();
    r.model = f.model;
    r.session_id = f.session_id;
    r.input_tokens = f.input_tokens;
    r.output_tokens = f.output_tokens;
    r.cached_tokens = f.cached_tokens;
    r.cost_usd = f.cost_usd;
    r.retry_at_ms = f.rate_limit_resets_at_ms;
    r.exit_code = exit_code;
    r.duration_ms = clock.elapsed();

    if (refused) {
        r.failure = FailureKind::Auth;
        r.subscription_detected = true;
        r.error = refusal;
        return r;
    }
    if (cancel_started) {
        r.cancelled = true;
        r.error = QStringLiteral("Request cancelled");
        r.content = segment_text;
        return r;
    }
    if (failed_to_start) {
        r.failure = FailureKind::NotInstalled;
        r.error = QStringLiteral("%1 could not be started: %2").arg(name, start_error);
        return r;
    }
    if (timed_out_first || timed_out_total) {
        r.failure = FailureKind::Timeout;
        r.error = timed_out_first ? QStringLiteral("%1 did not start answering within %2 s.")
                                        .arg(name)
                                        .arg(req.first_event_timeout_ms / 1000)
                                  : QStringLiteral("%1 did not finish within %2 s.").arg(name).arg(req.timeout_ms / 1000);
        r.content = segment_text;
        return r;
    }
    FailureKind kind = env.adapter->classify_failure(crashed ? -1 : exit_code, f, stderr_tail);
    if (kind == FailureKind::None && (crashed || killed))
        kind = FailureKind::Other;
    if (kind != FailureKind::None && f.rate_limit_status == QLatin1String("rejected"))
        kind = FailureKind::RateLimit;
    if (kind == FailureKind::None) {
        r.success = true;
        r.content = !f.final_text.trimmed().isEmpty() ? f.final_text : segment_text;
        return r;
    }
    r.failure = kind;
    QString detail;
    if (!f.result_error.isEmpty())
        detail = f.result_error;
    else if (!f.last_error.isEmpty())
        detail = f.last_error;
    else if (!start_error.isEmpty())
        detail = start_error;
    else if (!stderr_tail.isEmpty())
        detail = stderr_tail.mid(qMax<qsizetype>(0, stderr_tail.size() - 3)).join(QLatin1Char('\n'));
    else if (crashed)
        detail = QStringLiteral("%1 crashed.").arg(name);
    else
        detail = QStringLiteral("%1 exited with code %2.").arg(name).arg(exit_code);
    r.error = hcr_failure_sentence(kind, name, req.model) + QStringLiteral(" ") + detail.left(600);
    r.content = segment_text;
    return r;
}

} // namespace fincept::harness
