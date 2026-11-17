#pragma once
// HarnessRunExecutor.h — Chat Mode's "harness" engine: a long-running task run
// by an agent harness (Claude Code, Codex) through its own agent loop.
// engine_id() "harness" (RunRequest::engine "harness").
//
// Which harness: ModelRouter's route for kFeatureChatModeHarness — the first
// harness entry of the chain (API providers and Fincept LLM are not used by
// this engine). Agentic tasks send the user's financial data to the vendor, so
// a harness is only picked automatically once the user allowed it for this
// feature (Settings › LLM Configuration › Terminal AI); an explicit choice works
// as always. Eligibility is the terminal-AI rule: API-key authentication only,
// never a personal subscription (owner decision).
//
// Each run (and each answer) is one HarnessChatBackend call on a private
// worker pool: a fresh empty working folder, read-only, no built-in tools, the
// terminal's MCP tools through a RUN-SCOPED token that is paper-only (no real
// order is ever placed, changed or cancelled), the session kept so a paused /
// interrupted Claude run continues it (Codex terminal-AI sessions cannot be
// resumed: the task starts again). No Fincept credits are spent: usage reports
// 0 credits and the tokens the harness reported.
//
// Events: text deltas → text_delta(); thinking, tool calls / results (Codex
// commands as a call + result pair), errors → run events; a todo list the
// harness reports (Codex todo_list; Claude TodoWrite when available) → the
// run's steps (plan_ready once, step_changed afterwards); the harness session
// id is bound as remote_task_id ("<harness>:<session>"). Pause is not
// supported (can_pause() false); cancel kills the process tree.
//
// The mapping is pure and header-only below (namespace harness_run_mapping),
// so tests include this header without linking the executor.

#include "services/agent_tasks/IRunExecutor.h"
#include "services/agent_tasks/TaskRunTypes.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/HarnessChatTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>

namespace fincept::agent_tasks {

/// RunRequest::engine and IRunExecutor::engine_id() of this engine.
inline constexpr const char* kEngineHarness = "harness";
inline constexpr const char* kExecutorHarness = "harness";

namespace harness_run_mapping {

namespace hrm_cad = fincept::services::coding_agents;

/// One entry of a todo list the harness reported.
struct TodoItem {
    QString text;
    StepStatus status = StepStatus::Pending;
};

/// A Codex todo_list (Status events titled "Plan", "Plan updated", "Plan complete").
inline bool is_plan_status(const hrm_cad::AgentEvent& ev) {
    return ev.kind == hrm_cad::EventKind::Status &&
           (ev.title == QLatin1String("Plan") || ev.title == QLatin1String("Plan updated") ||
            ev.title == QLatin1String("Plan complete"));
}

/// Codex todo body: one line per item, "[x] done" / "[ ] open". Items are Done
/// or Pending; the first open one is Running while the plan is not complete.
inline QVector<TodoItem> parse_codex_todo(const QString& body, bool plan_complete = false) {
    QVector<TodoItem> out;
    for (const QString& raw : body.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        TodoItem t;
        if (line.startsWith(QLatin1String("[x]"), Qt::CaseInsensitive)) {
            t.status = StepStatus::Done;
            t.text = line.mid(3).trimmed();
        } else if (line.startsWith(QLatin1String("[ ]"))) {
            t.text = line.mid(3).trimmed();
        } else {
            t.text = line;
        }
        if (!t.text.isEmpty())
            out.append(t);
    }
    if (!plan_complete) {
        for (TodoItem& t : out) {
            if (t.status == StepStatus::Pending) {
                t.status = StepStatus::Running;
                break;
            }
        }
    }
    return out;
}

/// The bare tool name of a transcript tool title ("↳ TodoWrite", "fincept.ui_open" → "TodoWrite", "ui_open").
inline QString tool_of(const hrm_cad::AgentEvent& ev) {
    QString t = ev.title;
    if (t.startsWith(QStringLiteral("↳ ")))
        t = t.mid(2);
    return harness::chat_tool_name(t.trimmed());
}

/// A Claude TodoWrite call (its input carries the whole list).
inline bool is_todo_write(const hrm_cad::AgentEvent& ev) {
    return ev.kind == hrm_cad::EventKind::ToolCall && tool_of(ev) == QLatin1String("TodoWrite");
}

/// TodoWrite input {"todos":[{"content","status":"pending|in_progress|completed","activeForm"}]}.
inline QVector<TodoItem> parse_todo_write(const QString& args_json) {
    QVector<TodoItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(args_json.toUtf8());
    const QJsonArray todos = doc.object().value(QStringLiteral("todos")).toArray();
    for (const QJsonValue& v : todos) {
        const QJsonObject o = v.toObject();
        TodoItem t;
        t.text = o.value(QStringLiteral("content")).toString().trimmed();
        const QString st = o.value(QStringLiteral("status")).toString();
        if (st == QLatin1String("completed"))
            t.status = StepStatus::Done;
        else if (st == QLatin1String("in_progress"))
            t.status = StepStatus::Running;
        if (!t.text.isEmpty())
            out.append(t);
    }
    return out;
}

/// The run steps of a todo list (titles; statuses come through step_changed).
inline QVector<RunStep> steps_from_todos(const QVector<TodoItem>& todos) {
    QVector<RunStep> steps;
    steps.reserve(todos.size());
    for (int i = 0; i < todos.size(); ++i) {
        RunStep s;
        s.idx = i;
        s.title = todos[i].text.left(200);
        s.instruction = todos[i].text;
        steps.append(s);
    }
    return steps;
}

inline QStringList todo_titles(const QVector<TodoItem>& todos) {
    QStringList out;
    for (const TodoItem& t : todos)
        out << t.text.left(200);
    return out;
}

/// The session id in a "Session started" (Claude) / "Thread started" (Codex)
/// Status body ("session: <id>" / "thread: <id>"); empty for anything else.
inline QString session_id_from_status(const hrm_cad::AgentEvent& ev) {
    if (ev.kind != hrm_cad::EventKind::Status)
        return {};
    const bool claude = ev.title.startsWith(QLatin1String("Session started"));
    const bool codex = ev.title == QLatin1String("Thread started");
    if (!claude && !codex)
        return {};
    const QString key = claude ? QStringLiteral("session: ") : QStringLiteral("thread: ");
    for (const QString& raw : ev.body.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(key))
            return line.mid(key.size()).trimmed().left(128);
    }
    return {};
}

/// remote_task_id of a run: "<harness>:<session>".
inline QString remote_id(const QString& harness_id, const QString& session_id) {
    return session_id.isEmpty() ? QString() : harness_id + QLatin1Char(':') + session_id;
}

/// The session of `harness_id` stored in remote_task_id ("" when it belongs to another harness).
inline QString session_of(const QString& remote_task_id, const QString& harness_id) {
    const QString prefix = harness_id + QLatin1Char(':');
    return remote_task_id.startsWith(prefix) ? remote_task_id.mid(prefix.size()) : QString();
}

/// Streamed text: a partial event carries the message so far (cumulative per
/// ref_id); the complete event the whole message. Returns what was not sent yet.
class DeltaTracker {
  public:
    QString take(const hrm_cad::AgentEvent& ev) {
        const QString& body = ev.body;
        QString delta;
        const auto it = ev.ref_id.isEmpty() ? sent_.end() : sent_.find(ev.ref_id);
        if (it != sent_.end()) {
            const qsizetype already = it.value();
            if (body.size() <= already)
                return {};
            it.value() = body.size();
            delta = body.mid(already);
        } else {
            if (body.trimmed().isEmpty())
                return {};
            if (!ev.ref_id.isEmpty())
                sent_.insert(ev.ref_id, body.size());
            delta = (!any_ || ends_with_newline_) ? body : QStringLiteral("\n\n") + body;
        }
        any_ = true;
        ends_with_newline_ = delta.endsWith(QLatin1Char('\n'));
        return delta;
    }

  private:
    QHash<QString, qsizetype> sent_;
    bool any_ = false;
    bool ends_with_newline_ = false;
};

inline RunEvent make_event(RunEventKind kind, const QString& title, const QString& body = {}, QJsonObject data = {},
                           bool verbose = false) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    if (verbose)
        data.insert(QStringLiteral("verbose"), true);
    e.data = data;
    return e;
}

/// What one transcript event of the harness becomes.
struct Mapped {
    QVector<RunEvent> events;
    QString text_delta;            // answer text to stream (text_delta signal)
    std::optional<QVector<TodoItem>> todos; // the harness reported its todo list
    QString session_id;            // the harness session started
};

inline constexpr int kHrmPreviewChars = 4000;

/// Pure mapping of one harness transcript event onto Chat Mode run events.
inline Mapped map_event(const hrm_cad::AgentEvent& ev, DeltaTracker& text) {
    using K = hrm_cad::EventKind;
    Mapped m;
    switch (ev.kind) {
        case K::Text:
            m.text_delta = text.take(ev);
            break;
        case K::Thinking:
            if (!ev.partial && !ev.body.trimmed().isEmpty())
                m.events << make_event(RunEventKind::Thinking, QStringLiteral("Thinking"), ev.body.left(kHrmPreviewChars));
            break;
        case K::ToolCall: {
            const QString tool = tool_of(ev);
            if (is_todo_write(ev))
                m.todos = parse_todo_write(ev.body);
            RunEvent e = make_event(RunEventKind::ToolCall, tool, ev.body.left(kHrmPreviewChars),
                                    QJsonObject{{QStringLiteral("tool"), tool}}, is_todo_write(ev));
            e.ref_id = ev.ref_id;
            m.events << e;
            break;
        }
        case K::ToolResult: {
            const QString tool = tool_of(ev);
            RunEvent e = make_event(RunEventKind::ToolResult, tool, ev.body.left(kHrmPreviewChars),
                                    QJsonObject{{QStringLiteral("tool"), tool}, {QStringLiteral("success"), !ev.is_error}},
                                    tool == QLatin1String("TodoWrite"));
            e.ref_id = ev.ref_id;
            e.is_error = ev.is_error;
            m.events << e;
            break;
        }
        case K::Command: {
            // Codex: "(running)" when it starts, the output when it ends — a call + result pair.
            const bool running = ev.body == QLatin1String("(running)");
            RunEvent e = make_event(running ? RunEventKind::ToolCall : RunEventKind::ToolResult, ev.title.left(200),
                                    running ? QString() : ev.body.left(kHrmPreviewChars),
                                    running ? QJsonObject{{QStringLiteral("tool"), QStringLiteral("shell")}}
                                            : QJsonObject{{QStringLiteral("tool"), QStringLiteral("shell")},
                                                          {QStringLiteral("success"), !ev.is_error}});
            e.ref_id = ev.ref_id;
            e.is_error = !running && ev.is_error;
            m.events << e;
            break;
        }
        case K::Status: {
            if (is_plan_status(ev)) {
                m.todos = parse_codex_todo(ev.body, ev.title == QLatin1String("Plan complete"));
                m.events << make_event(RunEventKind::Plan, ev.title, ev.body.left(kHrmPreviewChars), {}, true);
                break;
            }
            m.session_id = session_id_from_status(ev);
            m.events << make_event(RunEventKind::Note, ev.title.left(200), ev.body.left(kHrmPreviewChars), {}, true);
            if (ev.is_error)
                m.events.last().is_error = true;
            break;
        }
        case K::Usage:
            m.events << make_event(RunEventKind::Usage, QStringLiteral("Usage"), ev.body.left(kHrmPreviewChars), {}, true);
            break;
        case K::Error: {
            RunEvent e = make_event(RunEventKind::Error, ev.title.left(200), ev.body.left(kHrmPreviewChars),
                                    QJsonObject{{QStringLiteral("code"), QStringLiteral("harness_error")}});
            e.is_error = true;
            m.events << e;
            break;
        }
        case K::FileChange:
            m.events << make_event(RunEventKind::Note, ev.title.left(200), ev.body.left(kHrmPreviewChars), {}, true);
            break;
        case K::Final:
            break; // TaskRunService records the run's final text itself
    }
    return m;
}

/// The run's usage at the end of a harness call: no Fincept credits, the
/// tokens the harness reported (and its cost estimate when it gives one).
inline RunEvent usage_event(qint64 input_tokens, qint64 output_tokens, const QString& vendor,
                            std::optional<double> cost_usd) {
    const qint64 total = qMax<qint64>(0, input_tokens) + qMax<qint64>(0, output_tokens);
    const QString billing = QStringLiteral("0 credits · billed to your %1 API key").arg(vendor);
    QJsonObject d{{QStringLiteral("turn_end"), true},
                  {QStringLiteral("tokens_total"), static_cast<double>(total)},
                  {QStringLiteral("credits_charged"), 0.0},
                  {QStringLiteral("billing"), billing}};
    QString title = QStringLiteral("%1 · %2 tokens").arg(billing).arg(total);
    if (cost_usd.has_value() && *cost_usd >= 0) {
        d.insert(QStringLiteral("cost_usd"), *cost_usd);
        title += QStringLiteral(" · about $%1").arg(*cost_usd, 0, 'f', 4);
    }
    return make_event(RunEventKind::Usage, title, {}, d);
}

} // namespace harness_run_mapping

class HarnessRunExecutor : public IRunExecutor {
    Q_OBJECT
  public:
    explicit HarnessRunExecutor(QObject* parent = nullptr);
    ~HarnessRunExecutor() override;

    QString engine_id() const override;
    void start(const TaskRun& run, const QVector<RunStep>& steps) override;
    void answer(const QString& run_id, const QString& text) override;
    void request_pause(const QString& run_id) override;
    void cancel(const QString& run_id) override;
    bool can_pause(const QString& run_id) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::agent_tasks
