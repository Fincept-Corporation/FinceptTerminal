// tst_agent_task_runs — Chat Mode's run model (states, budgets, ids, JSON) and the
// cloud agent's SSE vocabulary mapped to run events (src/services/agent_tasks).

#include "services/agent_tasks/CloudSseMapping.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::agent_tasks;

namespace {

QJsonObject obj(const char* json) {
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

/// Feed `raw` (a whole stream) line by line; return the messages in order.
QVector<SseParser::Message> parse_all(const QByteArray& raw) {
    SseParser p;
    QVector<SseParser::Message> out;
    for (const QByteArray& line : raw.split('\n')) {
        SseParser::Message m;
        if (p.feed_line(line, &m))
            out.append(m);
    }
    SseParser::Message m;
    if (p.flush(&m))
        out.append(m);
    return out;
}

} // namespace

class TestAgentTaskRuns : public QObject {
    Q_OBJECT

  private slots:
    // ── SSE framing ──────────────────────────────────────────────────────────
    void sse_basic_blocks();
    void sse_multiline_crlf_comments();
    void sse_event_without_blank_line_and_eof();

    // ── Event vocabulary ─────────────────────────────────────────────────────
    void maps_full_vocabulary();
    void pairs_tool_calls_by_ref();
    void pairs_delegations();
    void text_and_question();
    void finish_billing_and_turn_end();
    void error_with_retry_and_code();
    void unknown_event_is_verbose_note();

    // ── Tokens (plan B9) ─────────────────────────────────────────────────────
    void tokens_lite_sum_replaced_by_finish_total();
    void tokens_deep_take_max_within_window();
    void tokens_deep_across_windows();

    // ── Run model ────────────────────────────────────────────────────────────
    void status_ids_round_trip();
    void state_machine();
    void restart_semantics();
    void budget_checks();
    void executor_mapping_and_origins();
    void request_json_round_trip();
    void plan_json_round_trip();
};

void TestAgentTaskRuns::sse_basic_blocks() {
    const auto msgs = parse_all("event: text-delta\ndata: {\"text\":\"Hi\"}\n\nevent: finish\ndata: {}\n\n");
    QCOMPARE(msgs.size(), 2);
    QCOMPARE(msgs[0].event, QStringLiteral("text-delta"));
    QCOMPARE(msgs[0].data, QByteArray("{\"text\":\"Hi\"}"));
    QCOMPARE(msgs[1].event, QStringLiteral("finish"));
}

void TestAgentTaskRuns::sse_multiline_crlf_comments() {
    const auto msgs = parse_all(": keep-alive\r\nevent: note\r\ndata: line1\r\ndata: line2\r\n\r\ndata: x\n\n");
    QCOMPARE(msgs.size(), 2);
    QCOMPARE(msgs[0].event, QStringLiteral("note"));
    QCOMPARE(msgs[0].data, QByteArray("line1\nline2"));
    QCOMPARE(msgs[1].event, QStringLiteral("message")); // no event: field
    QCOMPARE(msgs[1].data, QByteArray("x"));
}

void TestAgentTaskRuns::sse_event_without_blank_line_and_eof() {
    // A server that omits the blank line still gets each event delivered, and the
    // last block is flushed at the end of the stream.
    const auto msgs = parse_all("event: a\ndata: 1\nevent: b\ndata: 2");
    QCOMPARE(msgs.size(), 2);
    QCOMPARE(msgs[0].event, QStringLiteral("a"));
    QCOMPARE(msgs[0].data, QByteArray("1"));
    QCOMPARE(msgs[1].event, QStringLiteral("b"));
    QCOMPARE(msgs[1].data, QByteArray("2"));
    // A block without data is dropped.
    QCOMPARE(parse_all("event: lonely\n\n").size(), 0);
}

void TestAgentTaskRuns::maps_full_vocabulary() {
    struct Case {
        const char* name;
        const char* data;
        RunEventKind kind;
        bool verbose;
    };
    const Case cases[] = {
        {"session-meta", R"({"session_id":"s1","new_title":"T"})", RunEventKind::Status, true},
        {"thinking", R"({"content":"hmm"})", RunEventKind::Thinking, false},
        {"planning", R"({"summary":"1. look 2. write"})", RunEventKind::Plan, false},
        {"step-start", R"({"stepNumber":1})", RunEventKind::Note, true},
        {"step-finish", R"({"stepNumber":1,"tokensUsed":10})", RunEventKind::Usage, true},
        {"window-start", R"({"window":1,"maxWindows":5})", RunEventKind::StepStart, true},
        {"window-done", R"({"window":1,"tokensUsed":10})", RunEventKind::StepEnd, true},
        {"llm-call-start", R"({"callNumber":1})", RunEventKind::Note, true},
        {"llm-call-end", R"({"callNumber":1,"tokens":5})", RunEventKind::Note, true},
        {"tool-start", R"({"tool":"get_quote","preview":"symbol=AAPL"})", RunEventKind::ToolCall, false},
        {"tool-end", R"({"tool":"get_quote","durationMs":12,"success":true,"outputPreview":"{}"})",
         RunEventKind::ToolResult, false},
        {"tool-discover", R"({"query":"options"})", RunEventKind::Note, true},
        {"delegation-start", R"({"specialists":["equity"],"taskCount":1,"queries":{"equity":"q"}})",
         RunEventKind::Delegation, false},
        {"delegation-end", R"({"durationMs":3000,"outputPreview":"done"})", RunEventKind::Delegation, false},
        {"interrupt", R"({"question":"Which?","mode":"deep","resumable":true})", RunEventKind::Question, false},
        {"approval-required", R"({"proposal_id":"p1","action":"buy","symbol":"AAPL","quantity":2})",
         RunEventKind::Approval, false},
        {"proposal-decided", R"({"proposal_id":"p1","status":"executed"})", RunEventKind::ApprovalDecided, false},
        {"trade-executed", R"({"source":"auto"})", RunEventKind::Note, false},
        {"history-compressed", R"({"beforeTokens":100,"afterTokens":10})", RunEventKind::Note, false},
        {"rate-limit-retry", R"({"attempt":1,"backoffSecs":5})", RunEventKind::Note, false},
        {"follow-ups", R"({"questions":["a","b"]})", RunEventKind::Note, false},
        {"critique", R"({"confidence":0.8,"summary":"ok"})", RunEventKind::Note, false},
        {"finish", R"({"totalTokens":5})", RunEventKind::Usage, false},
        {"error", R"({"message":"boom"})", RunEventKind::Error, false},
    };
    for (const Case& c : cases) {
        CloudTurnTracker t;
        const CloudEventOutcome out = apply_cloud_event(t, QString::fromLatin1(c.name), obj(c.data));
        QVERIFY2(out.events.size() == 1, c.name);
        QVERIFY2(out.events.first().kind == c.kind, c.name);
        QVERIFY2(is_verbose_event(out.events.first()) == c.verbose, c.name);
    }
    // text-delta and heartbeat produce no rows.
    CloudTurnTracker t;
    QVERIFY(apply_cloud_event(t, QStringLiteral("text-delta"), obj(R"({"text":"x"})")).events.isEmpty());
    const CloudEventOutcome hb = apply_cloud_event(t, QStringLiteral("heartbeat"), obj(R"({"status":"processing"})"));
    QVERIFY(hb.events.isEmpty());
    QVERIFY(hb.heartbeat);
    // A proposal decision without id or status is not an event.
    QVERIFY(apply_cloud_event(t, QStringLiteral("proposal-decided"), obj(R"({"status":"x"})")).events.isEmpty());
}

void TestAgentTaskRuns::pairs_tool_calls_by_ref() {
    CloudTurnTracker t;
    const auto a = apply_cloud_event(t, QStringLiteral("tool-start"), obj(R"({"tool":"q","preview":"A"})")).events.first();
    const auto b = apply_cloud_event(t, QStringLiteral("tool-start"), obj(R"({"tool":"q","preview":"B"})")).events.first();
    const auto c = apply_cloud_event(t, QStringLiteral("tool-start"), obj(R"({"tool":"n"})")).events.first();
    QVERIFY(a.ref_id != b.ref_id);
    // Ends pair FIFO per tool name.
    const auto end_n = apply_cloud_event(t, QStringLiteral("tool-end"), obj(R"({"tool":"n","durationMs":1})")).events.first();
    const auto end_q1 = apply_cloud_event(t, QStringLiteral("tool-end"), obj(R"({"tool":"q","durationMs":2})")).events.first();
    const auto end_q2 = apply_cloud_event(t, QStringLiteral("tool-end"),
                                          obj(R"({"tool":"q","durationMs":3,"success":false,"outputPreview":"x"})"))
                            .events.first();
    QCOMPARE(end_n.ref_id, c.ref_id);
    QCOMPARE(end_q1.ref_id, a.ref_id);
    QCOMPARE(end_q2.ref_id, b.ref_id);
    QVERIFY(!end_q1.is_error);
    QVERIFY(end_q2.is_error);
    QCOMPARE(end_q2.body, QStringLiteral("x"));
    QCOMPARE(end_q1.data.value("durationMs").toInt(), 2);
    // An end without a start gets a fresh ref instead of stealing one.
    const auto orphan = apply_cloud_event(t, QStringLiteral("tool-end"), obj(R"({"tool":"q"})")).events.first();
    QVERIFY(orphan.ref_id != a.ref_id && orphan.ref_id != b.ref_id);
    QVERIFY(t.open_tools.isEmpty());
}

void TestAgentTaskRuns::pairs_delegations() {
    CloudTurnTracker t;
    const auto s = apply_cloud_event(t, QStringLiteral("delegation-start"),
                                     obj(R"({"specialists":["equity","macro"],"taskCount":2,
                                            "queries":{"equity":"AAPL valuation","macro":"rates"}})"))
                       .events.first();
    QCOMPARE(s.data.value("phase").toString(), QStringLiteral("start"));
    QVERIFY(s.body.contains(QStringLiteral("equity: AAPL valuation")));
    QVERIFY(s.title.contains(QStringLiteral("2")));
    const auto e = apply_cloud_event(t, QStringLiteral("delegation-end"), obj(R"({"durationMs":2500,"outputPreview":"ok"})"))
                       .events.first();
    QCOMPARE(e.ref_id, s.ref_id);
    QCOMPARE(e.data.value("phase").toString(), QStringLiteral("end"));
}

void TestAgentTaskRuns::text_and_question() {
    CloudTurnTracker t;
    QCOMPARE(apply_cloud_event(t, QStringLiteral("text-delta"), obj(R"({"text":"Hello "})")).text_delta,
             QStringLiteral("Hello "));
    apply_cloud_event(t, QStringLiteral("text-delta"), obj(R"({"text":"world"})"));
    QCOMPARE(t.text, QStringLiteral("Hello world"));
    const auto q = apply_cloud_event(t, QStringLiteral("interrupt"),
                                     obj(R"({"question":"  Which account? ","mode":"lite","resumable":true})"));
    QVERIFY(t.interrupted);
    QVERIFY(t.resumable);
    QCOMPARE(t.question, QStringLiteral("Which account?"));
    QCOMPARE(q.events.first().body, QStringLiteral("Which account?"));
    QVERIFY(q.events.first().data.value("resumable").toBool());
    // Older servers: {question} only -> not resumable.
    CloudTurnTracker old;
    apply_cloud_event(old, QStringLiteral("interrupt"), obj(R"({"question":"Q"})"));
    QVERIFY(old.interrupted && !old.resumable);
    // A new turn keeps only the step.
    t.step = 3;
    t.reset_for_new_turn();
    QCOMPARE(t.step, 3);
    QVERIFY(t.text.isEmpty() && !t.interrupted);
}

void TestAgentTaskRuns::finish_billing_and_turn_end() {
    CloudTurnTracker t;
    const auto out = apply_cloud_event(
        t, QStringLiteral("finish"), obj(R"({"totalTokens":321,"credits_charged":5.0,"credit_balance":4995.5,"elapsedMs":10})"));
    QVERIFY(out.terminal);
    QVERIFY(t.finished);
    QVERIFY(t.credits_known);
    QCOMPARE(t.credits_charged, 5.0);
    QVERIFY(t.balance_known);
    QCOMPARE(t.credit_balance, 4995.5);
    const RunEvent& e = out.events.first();
    QVERIFY(e.data.value("turn_end").toBool());
    QCOMPARE(e.data.value("tokens_total").toInt(), 321);
    // Older servers: no billing in finish; interrupted finish marks the pause.
    CloudTurnTracker old;
    apply_cloud_event(old, QStringLiteral("finish"), obj(R"({"interrupted":true})"));
    QVERIFY(!old.credits_known && !old.balance_known);
    QVERIFY(old.interrupted);
}

void TestAgentTaskRuns::error_with_retry_and_code() {
    CloudTurnTracker t;
    const auto out = apply_cloud_event(t, QStringLiteral("error"),
                                       obj(R"({"message":"Agent busy","code":"AGENT_BUSY","retry_after":30})"));
    QVERIFY(out.terminal);
    QVERIFY(t.errored);
    QCOMPARE(t.error_code, QStringLiteral("agent_busy"));
    QVERIFY(t.error_message.startsWith(QStringLiteral("Agent busy.")));
    QVERIFY(t.error_message.contains(QStringLiteral("30")));
    QCOMPARE(out.events.first().data.value("code").toString(), QStringLiteral("agent_busy"));
    QVERIFY(out.events.first().is_error);
    // "error" key on older paths; empty -> a generic sentence.
    CloudTurnTracker o;
    apply_cloud_event(o, QStringLiteral("error"), obj(R"({"error":"old path"})"));
    QCOMPARE(o.error_message, QStringLiteral("old path"));
    CloudTurnTracker e;
    apply_cloud_event(e, QStringLiteral("error"), obj("{}"));
    QVERIFY(!e.error_message.isEmpty());
}

void TestAgentTaskRuns::unknown_event_is_verbose_note() {
    CloudTurnTracker t;
    const auto out = apply_cloud_event(t, QStringLiteral("brand-new"), obj(R"({"x":1})"));
    QCOMPARE(out.events.size(), 1);
    QCOMPARE(out.events.first().kind, RunEventKind::Note);
    QVERIFY(is_verbose_event(out.events.first()));
    QCOMPARE(out.events.first().data.value("event").toString(), QStringLiteral("brand-new"));
}

void TestAgentTaskRuns::tokens_lite_sum_replaced_by_finish_total() {
    TokenMeter m;
    m.on_step_finish(obj(R"({"stepNumber":0,"tokensUsed":100})"));
    m.on_step_finish(obj(R"({"stepNumber":1,"tokensUsed":50})"));
    QCOMPARE(m.total(), qint64(150));
    m.on_finish(obj(R"({"totalTokens":150})"));
    QCOMPARE(m.total(), qint64(150)); // replaced, never doubled
    m.on_finish(obj(R"({})"));
    QCOMPARE(m.total(), qint64(150));
}

void TestAgentTaskRuns::tokens_deep_take_max_within_window() {
    // windowed_runner: step-finish carries the window's CUMULATIVE count.
    TokenMeter m;
    m.on_step_finish(obj(R"({"window":0,"tokensUsed":100})"));
    m.on_step_finish(obj(R"({"window":0,"tokensUsed":250})"));
    m.on_step_finish(obj(R"({"window":0,"tokensUsed":400})"));
    QCOMPARE(m.total(), qint64(400)); // not 750
    m.on_window_done(obj(R"({"window":1,"tokensUsed":400})"));
    QCOMPARE(m.total(), qint64(400));
    m.on_finish(obj(R"({"windows":1})")); // deep finish has no totalTokens
    QCOMPARE(m.total(), qint64(400));
}

void TestAgentTaskRuns::tokens_deep_across_windows() {
    TokenMeter m;
    m.on_step_finish(obj(R"({"window":0,"tokensUsed":300})"));
    m.on_window_done(obj(R"({"window":1,"tokensUsed":350})"));
    m.on_step_finish(obj(R"({"window":1,"tokensUsed":120})"));
    m.on_window_done(obj(R"({"window":2,"tokensUsed":100})")); // lower than seen: max stays
    QCOMPARE(m.total(), qint64(350 + 120));
    m.reset();
    QCOMPARE(m.total(), qint64(0));
    // Through the mapping too.
    CloudTurnTracker t;
    apply_cloud_event(t, QStringLiteral("step-finish"), obj(R"({"window":0,"tokensUsed":10})"));
    const auto out = apply_cloud_event(t, QStringLiteral("step-finish"), obj(R"({"window":0,"tokensUsed":30})"));
    QVERIFY(out.tokens_changed);
    QCOMPARE(t.tokens.total(), qint64(30));
}

void TestAgentTaskRuns::status_ids_round_trip() {
    for (RunStatus s : {RunStatus::Queued, RunStatus::Planning, RunStatus::Running, RunStatus::WaitingInput,
                        RunStatus::WaitingApproval, RunStatus::Paused, RunStatus::Completed, RunStatus::Failed,
                        RunStatus::Cancelled, RunStatus::BudgetStopped, RunStatus::Interrupted}) {
        QCOMPARE(run_status_from_id(run_status_id(s)).value(), s);
        QVERIFY(!run_status_label(s).isEmpty());
    }
    QVERIFY(!run_status_from_id(QStringLiteral("nope")).has_value());
    for (StepStatus s : {StepStatus::Pending, StepStatus::Running, StepStatus::Done, StepStatus::Failed, StepStatus::Skipped})
        QCOMPARE(step_status_from_id(step_status_id(s)).value(), s);
    for (int k = int(RunEventKind::Plan); k <= int(RunEventKind::Status); ++k) {
        const auto kind = RunEventKind(k);
        QCOMPARE(event_kind_from_id(event_kind_id(kind)).value(), kind);
    }
}

void TestAgentTaskRuns::state_machine() {
    using S = RunStatus;
    // The happy path and the waits.
    QVERIFY(can_transition(S::Queued, S::Running));
    QVERIFY(can_transition(S::Queued, S::Planning));
    QVERIFY(can_transition(S::Planning, S::Running));
    QVERIFY(can_transition(S::Running, S::WaitingInput));
    QVERIFY(can_transition(S::WaitingInput, S::Running));
    QVERIFY(can_transition(S::Running, S::Paused));
    QVERIFY(can_transition(S::Paused, S::Queued));
    QVERIFY(can_transition(S::Running, S::Completed));
    QVERIFY(can_transition(S::Running, S::BudgetStopped));
    QVERIFY(can_transition(S::WaitingInput, S::Completed)); // a new message instead of an answer
    QVERIFY(can_transition(S::Queued, S::Cancelled));
    QVERIFY(can_transition(S::Running, S::Interrupted));
    QVERIFY(can_transition(S::Interrupted, S::Queued)); // resume
    // Final states stay final; nothing goes back to the queue from work.
    for (S end : {S::Completed, S::Failed, S::Cancelled, S::BudgetStopped})
        for (S to : {S::Queued, S::Running, S::Planning, S::WaitingInput, S::Paused})
            QVERIFY(!can_transition(end, to));
    QVERIFY(!can_transition(S::Running, S::Queued));
    QVERIFY(!can_transition(S::Queued, S::WaitingInput));
    QVERIFY(can_transition(S::Running, S::Running));

    QVERIFY(is_terminal(S::Interrupted) && is_terminal(S::BudgetStopped));
    QVERIFY(!is_terminal(S::WaitingInput) && !is_terminal(S::Paused));
    QVERIFY(is_active(S::Queued) && is_active(S::Running) && !is_active(S::WaitingInput));
    QVERIFY(holds_slot(S::Running) && holds_slot(S::Planning) && !holds_slot(S::Queued) && !holds_slot(S::WaitingInput));
    QVERIFY(needs_attention(S::WaitingInput) && needs_attention(S::WaitingApproval) && !needs_attention(S::Paused));
    QVERIFY(can_resume(S::Paused) && can_resume(S::Interrupted) && !can_resume(S::Failed));
}

void TestAgentTaskRuns::restart_semantics() {
    using S = RunStatus;
    QCOMPARE(status_after_restart(S::Queued), S::Interrupted);
    QCOMPARE(status_after_restart(S::Planning), S::Interrupted);
    QCOMPARE(status_after_restart(S::Running), S::Interrupted);
    QCOMPARE(status_after_restart(S::WaitingApproval), S::Interrupted);
    QCOMPARE(status_after_restart(S::WaitingInput), S::WaitingInput); // kept
    QCOMPARE(status_after_restart(S::Paused), S::Paused);             // kept
    QCOMPARE(status_after_restart(S::Completed), S::Completed);
}

void TestAgentTaskRuns::budget_checks() {
    TaskRun r;
    QVERIFY(!check_budget(r, 100, 1).exceeded); // 0 = unlimited
    r.budget.max_credits = 50;
    r.credits_spent = 50;
    QVERIFY(!check_budget(r, 0, 0).exceeded); // at the limit is fine
    r.credits_spent = 55;
    BudgetVerdict v = check_budget(r, 0, 0);
    QVERIFY(v.exceeded);
    QVERIFY(v.reason.contains(QStringLiteral("55")));

    TaskRun s;
    s.budget.max_steps = 8;
    QVERIFY(!check_budget(s, 8, 0).exceeded);
    QVERIFY(check_budget(s, 9, 0).exceeded);

    TaskRun m;
    m.budget.max_minutes = 2;
    m.started_at_ms = 1000;
    QVERIFY(!check_budget(m, 0, 1000 + 120 * 1000).exceeded);
    QVERIFY(check_budget(m, 0, 1000 + 121 * 1000).exceeded);
    m.started_at_ms = 0; // not started: no clock yet
    QVERIFY(!check_budget(m, 0, 10'000'000).exceeded);

    QCOMPARE(estimate_cloud_plan_credits(3), (3 + 2) * kCloudTurnCredits);
    QCOMPARE(estimate_cloud_plan_credits(0), (1 + 2) * kCloudTurnCredits);
    QVERIFY(estimate_cloud_plan_credits(kDefaultMaxSteps) <= kDefaultMaxCredits);
}

void TestAgentTaskRuns::executor_mapping_and_origins() {
    QCOMPARE(executor_for(QStringLiteral("cloud"), false), QStringLiteral("cloud_turn"));
    QCOMPARE(executor_for(QStringLiteral("cloud"), true), QStringLiteral("cloud_plan"));
    QCOMPARE(executor_for(QStringLiteral("local"), true), QStringLiteral("local"));
    QVERIFY(executor_for(QStringLiteral("mystery"), false).isEmpty());
    QVERIFY(is_unattended_origin(QStringLiteral("schedule")));
    QVERIFY(!is_unattended_origin(QStringLiteral("chat")));
    TaskRun r;
    r.engine = QStringLiteral("cloud");
    r.origin = QStringLiteral("schedule");
    QVERIFY(r.is_unattended());
    QCOMPARE(r.executor_id(), QStringLiteral("cloud_turn"));
}

void TestAgentTaskRuns::request_json_round_trip() {
    RunRequest r;
    r.title = QStringLiteral("T");
    r.prompt = QStringLiteral("P");
    r.engine = QStringLiteral("cloud");
    r.mode = QStringLiteral("deep");
    r.template_id = QStringLiteral("earnings_review");
    r.session_uuid = QStringLiteral("s");
    r.origin = QStringLiteral("schedule");
    r.profile_id = 7;
    r.context = obj(R"({"symbol":"AAPL"})");
    r.budget = {12.5, 4, 30};
    r.plan_first = true;
    const RunRequest b = run_request_from_json(run_request_to_json(r));
    QCOMPARE(b.title, r.title);
    QCOMPARE(b.prompt, r.prompt);
    QCOMPARE(b.mode, r.mode);
    QCOMPARE(b.template_id, r.template_id);
    QCOMPARE(b.origin, r.origin);
    QCOMPARE(b.profile_id, 7);
    QCOMPARE(b.context.value("symbol").toString(), QStringLiteral("AAPL"));
    QCOMPARE(b.budget.max_credits, 12.5);
    QCOMPARE(b.budget.max_steps, 4);
    QCOMPARE(b.budget.max_minutes, 30);
    QVERIFY(b.plan_first);
}

void TestAgentTaskRuns::plan_json_round_trip() {
    QVector<RunStep> steps(2);
    steps[0].title = QStringLiteral("Gather");
    steps[0].instruction = QStringLiteral("Get data");
    steps[0].deliverable = QStringLiteral("table");
    steps[1].title = QStringLiteral("Write");
    const QVector<RunStep> back = steps_from_plan_json(QStringLiteral("run"), steps_to_plan_json(steps));
    QCOMPARE(back.size(), 2);
    QCOMPARE(back[0].run_id, QStringLiteral("run"));
    QCOMPARE(back[1].idx, 1);
    QCOMPARE(back[0].deliverable, QStringLiteral("table"));
    QCOMPARE(back[1].title, QStringLiteral("Write"));
    RunEvent e;
    e.seq = 4;
    e.ts_ms = 1700000000123;
    e.kind = RunEventKind::ToolResult;
    e.title = QStringLiteral("t");
    e.is_error = true;
    e.ref_id = QStringLiteral("tool-1");
    e.data = obj(R"({"durationMs":5})");
    const RunEvent eb = run_event_from_json(run_event_to_json(e));
    QCOMPARE(eb.seq, 4);
    QCOMPARE(eb.ts_ms, e.ts_ms);
    QCOMPARE(eb.kind, RunEventKind::ToolResult);
    QVERIFY(eb.is_error);
    QCOMPARE(eb.ref_id, e.ref_id);
    QCOMPARE(eb.data.value("durationMs").toInt(), 5);
}

QTEST_GUILESS_MAIN(TestAgentTaskRuns)
#include "tst_agent_task_runs.moc"
