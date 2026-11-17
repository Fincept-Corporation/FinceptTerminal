// tst_chat_run_views — the pure logic behind Chat Mode's run views (WP2,
// src/screens/chat_mode/RunViewLogic.h): status tones and pills, the RUNS list's
// filter / search / order, the actions a run allows, step progress and budget
// lines, the ACTIVITY timeline's pairing and collapsing, and the composer's
// prompt with its context and attachment.

#include "screens/chat_mode/RunViewLogic.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QSet>
#include <QtTest>

#include <algorithm>

using namespace fincept::agent_tasks;
using namespace fincept::chat_mode::run_view;

namespace {

TaskRun make_run(const QString& id, RunStatus status, qint64 created_ms, const QString& title = {},
                 const QString& prompt = {}) {
    TaskRun r;
    r.id = id;
    r.status = status;
    r.created_at_ms = created_ms;
    r.title = title;
    r.prompt = prompt;
    r.engine = QString::fromLatin1(kEngineCloud);
    r.session_uuid = QStringLiteral("session-1");
    return r;
}

RunEvent make_event(int seq, RunEventKind kind, const QString& title = {}, const QString& ref = {}, qint64 ts = 0) {
    RunEvent e;
    e.seq = seq;
    e.kind = kind;
    e.title = title;
    e.ref_id = ref;
    e.ts_ms = ts;
    return e;
}

RunStep make_step(int idx, StepStatus status, const QString& summary = {}) {
    RunStep s;
    s.idx = idx;
    s.status = status;
    s.summary = summary;
    s.title = QStringLiteral("Step %1").arg(idx + 1);
    return s;
}

const QVector<RunStatus> kAllStatuses = {RunStatus::Queued,    RunStatus::Planning,      RunStatus::Running,
                                         RunStatus::WaitingInput, RunStatus::WaitingApproval, RunStatus::Paused,
                                         RunStatus::Completed, RunStatus::Failed,        RunStatus::Cancelled,
                                         RunStatus::BudgetStopped, RunStatus::Interrupted};

} // namespace

class TestChatRunViews : public QObject {
    Q_OBJECT

  private slots:
    // ── Tones and pills ──────────────────────────────────────────────────────
    void tones_follow_state();
    void pills_are_distinct();

    // ── RUNS list ────────────────────────────────────────────────────────────
    void filter_partitions_runs();
    void search_matches_terms_in_title_and_prompt();
    void sort_puts_attention_first_then_newest();
    void sort_by_status_rank();
    void run_links_resolve_id_or_prefix();

    // ── Actions, steps, budget, formats ──────────────────────────────────────
    void actions_follow_state();
    void step_progress_and_text();
    void budget_line_texts();
    void duration_and_credit_formats();

    // ── ACTIVITY ─────────────────────────────────────────────────────────────
    void activity_pairs_tool_calls();
    void activity_orphans_and_duplicates();
    void activity_delegation_question_approval();
    void activity_step_durations_and_verbose_rows();
    void activity_row_tones();

    // ── Composer prompt ──────────────────────────────────────────────────────
    void compose_without_context_is_unchanged();
    void compose_inlines_context_and_attachment();
    void compose_truncates_a_long_attachment();
    void compose_keeps_within_the_prompt_limit();

    // ── WP5 / WP6 ────────────────────────────────────────────────────────────
    void paused_run_clock_stands_still();
    void pending_step_shows_no_duration();
    void unreported_credits_are_labelled();
    void budget_confirmation_threshold();
    // ── Harness engine ───────────────────────────────────────────────────────
    void harness_option_follows_executor_and_route();
    void harness_runs_spend_no_credits_and_cannot_pause();
};

// Notes' OPEN CHAT MODE sends the run id it stored, or the 8-character prefix of
// the note's footer: Chat Mode opens that run, or says it no longer exists.
void TestChatRunViews::run_links_resolve_id_or_prefix() {
    const QStringList ids{QStringLiteral("1a2b3c4d-0000-4000-8000-000000000001"),
                          QStringLiteral("9f8e7d6c-0000-4000-8000-000000000002"),
                          QStringLiteral("9f8e7d6c-1111-4000-8000-000000000003"),
                          QStringLiteral("abcdef01-0000-4000-8000-000000000004")};
    QCOMPARE(resolve_run_ref(ids, ids.at(0)), ids.at(0));                       // the full id
    QCOMPARE(resolve_run_ref(ids, QStringLiteral(" 1a2b3c4d ")), ids.at(0));     // the footer prefix
    QCOMPARE(resolve_run_ref(ids, QStringLiteral("1A2B3C4D")), ids.at(0));       // any case
    QCOMPARE(resolve_run_ref(ids, QStringLiteral("ABCDEF01-0000")), ids.at(3));
    QVERIFY(resolve_run_ref(ids, QStringLiteral("9f8e7d6c")).isEmpty());         // two runs: never a guess
    QCOMPARE(resolve_run_ref(ids, QStringLiteral("9f8e7d6c-1111")), ids.at(2));  // a longer prefix decides
    QVERIFY(resolve_run_ref(ids, QStringLiteral("1a2b3c")).isEmpty());           // shorter than 8: no prefix match
    QVERIFY(resolve_run_ref(ids, QStringLiteral("deadbeef")).isEmpty());         // deleted / pruned run
    QVERIFY(resolve_run_ref(ids, QString()).isEmpty());
    QVERIFY(resolve_run_ref({}, ids.at(0)).isEmpty());
}

void TestChatRunViews::tones_follow_state() {
    QCOMPARE(run_status_tone(RunStatus::Completed), Tone::Success);
    QCOMPARE(run_status_tone(RunStatus::Failed), Tone::Danger);
    QCOMPARE(run_status_tone(RunStatus::Running), Tone::Info);
    QCOMPARE(run_status_tone(RunStatus::Planning), Tone::Info);
    QCOMPARE(run_status_tone(RunStatus::WaitingInput), Tone::Warning);
    QCOMPARE(run_status_tone(RunStatus::WaitingApproval), Tone::Warning);
    QCOMPARE(run_status_tone(RunStatus::BudgetStopped), Tone::Warning);
    QCOMPARE(run_status_tone(RunStatus::Interrupted), Tone::Warning);
    QCOMPARE(run_status_tone(RunStatus::Queued), Tone::Neutral);
    QCOMPARE(run_status_tone(RunStatus::Paused), Tone::Neutral);
    QCOMPARE(run_status_tone(RunStatus::Cancelled), Tone::Neutral);

    QCOMPARE(step_status_tone(StepStatus::Pending), Tone::Neutral);
    QCOMPARE(step_status_tone(StepStatus::Running), Tone::Info);
    QCOMPARE(step_status_tone(StepStatus::Done), Tone::Success);
    QCOMPARE(step_status_tone(StepStatus::Failed), Tone::Danger);
    QCOMPARE(step_status_tone(StepStatus::Skipped), Tone::Neutral);
}

void TestChatRunViews::pills_are_distinct() {
    QSet<QString> seen;
    for (RunStatus s : kAllStatuses) {
        const QString pill = run_status_pill(s);
        QVERIFY(!pill.isEmpty());
        QCOMPARE(pill, pill.toUpper());
        QVERIFY2(!seen.contains(pill), qPrintable(pill));
        seen.insert(pill);
    }
    QCOMPARE(step_status_pill(StepStatus::Done), QStringLiteral("DONE"));
}

void TestChatRunViews::filter_partitions_runs() {
    for (RunStatus s : kAllStatuses) {
        QVERIFY(run_matches_filter(s, RunFilter::All));
        // Every run is either active (not ended) or finished — never both.
        QVERIFY(run_matches_filter(s, RunFilter::Active) != run_matches_filter(s, RunFilter::Finished));
        // "Needs attention" counts what the toolbar badge counts.
        QCOMPARE(run_matches_filter(s, RunFilter::Attention), needs_attention(s));
    }
    QVERIFY(run_matches_filter(RunStatus::Paused, RunFilter::Active));
    QVERIFY(run_matches_filter(RunStatus::WaitingApproval, RunFilter::Active));
    QVERIFY(run_matches_filter(RunStatus::Interrupted, RunFilter::Finished));
    QVERIFY(!run_matches_filter(RunStatus::Running, RunFilter::Attention));
}

void TestChatRunViews::search_matches_terms_in_title_and_prompt() {
    TaskRun r = make_run(QStringLiteral("a"), RunStatus::Running, 1, QStringLiteral("Earnings review: AAPL"),
                         QStringLiteral("Analyse the Q3 earnings of Apple and compare margins"));
    QVERIFY(run_matches_search(r, QString()));
    QVERIFY(run_matches_search(r, QStringLiteral("   ")));
    QVERIFY(run_matches_search(r, QStringLiteral("aapl")));
    QVERIFY(run_matches_search(r, QStringLiteral("  apple   MARGINS ")));
    QVERIFY(!run_matches_search(r, QStringLiteral("apple tesla"))); // every term must match
    // Only the beginning of a very long prompt is searched.
    r.prompt = QString(5000, QLatin1Char(' ')) + QStringLiteral("needle");
    QVERIFY(!run_matches_search(r, QStringLiteral("needle")));
}

void TestChatRunViews::sort_puts_attention_first_then_newest() {
    const TaskRun waiting = make_run(QStringLiteral("w"), RunStatus::WaitingInput, 100);
    const TaskRun done = make_run(QStringLiteral("d"), RunStatus::Completed, 300);
    const TaskRun running = make_run(QStringLiteral("r"), RunStatus::Running, 200);
    QVERIFY(run_sorts_before(waiting, done, RunSort::Recent)); // older, but it needs the user
    QVERIFY(run_sorts_before(done, running, RunSort::Recent)); // then newest first
    QVERIFY(!run_sorts_before(running, done, RunSort::Recent));

    // Strict weak order: irreflexive, ties broken by id.
    const TaskRun a = make_run(QStringLiteral("a"), RunStatus::Completed, 5);
    const TaskRun b = make_run(QStringLiteral("b"), RunStatus::Completed, 5);
    QVERIFY(!run_sorts_before(a, a, RunSort::Recent));
    QVERIFY(run_sorts_before(a, b, RunSort::Recent) != run_sorts_before(b, a, RunSort::Recent));

    QVector<TaskRun> list{done, running, waiting, a, b};
    std::sort(list.begin(), list.end(),
              [](const TaskRun& x, const TaskRun& y) { return run_sorts_before(x, y, RunSort::Recent); });
    QCOMPARE(list.first().id, QStringLiteral("w"));
    QCOMPARE(list.at(1).id, QStringLiteral("d"));
    QCOMPARE(list.at(2).id, QStringLiteral("r"));
}

void TestChatRunViews::sort_by_status_rank() {
    const TaskRun waiting = make_run(QStringLiteral("w"), RunStatus::WaitingApproval, 1);
    const TaskRun running = make_run(QStringLiteral("r"), RunStatus::Running, 2);
    const TaskRun queued = make_run(QStringLiteral("q"), RunStatus::Queued, 3);
    const TaskRun failed = make_run(QStringLiteral("f"), RunStatus::Failed, 4);
    const TaskRun done = make_run(QStringLiteral("d"), RunStatus::Completed, 5);
    QVERIFY(run_sorts_before(waiting, running, RunSort::Status));
    QVERIFY(run_sorts_before(running, queued, RunSort::Status));
    QVERIFY(run_sorts_before(queued, failed, RunSort::Status));
    QVERIFY(run_sorts_before(failed, done, RunSort::Status));
    QVERIFY(!run_sorts_before(done, running, RunSort::Status));
    QVERIFY(run_sorts_before(done, running, RunSort::Recent)); // newer
    QVERIFY(run_status_rank(RunStatus::WaitingInput) < run_status_rank(RunStatus::Running));
    QVERIFY(run_status_rank(RunStatus::Paused) < run_status_rank(RunStatus::Interrupted));
}

void TestChatRunViews::actions_follow_state() {
    TaskRun r = make_run(QStringLiteral("x"), RunStatus::Running, 1);
    RunActions a = run_actions(r, /*executor_can_pause=*/false);
    QVERIFY(!a.pause.enabled);
    QVERIFY(!a.pause.reason.isEmpty()); // a single reply says why
    QVERIFY(a.cancel.enabled);
    QVERIFY(!a.retry.enabled);
    QVERIFY(!a.retry.reason.isEmpty());
    QVERIFY(!a.remove.enabled);
    QVERIFY(!a.resume.enabled);
    QVERIFY(a.open.enabled);

    r.plan_first = true;
    QVERIFY(run_actions(r, true).pause.enabled);

    r.status = RunStatus::Queued;
    QVERIFY(run_actions(r, false).pause.enabled); // held back before it starts

    r.status = RunStatus::Paused;
    a = run_actions(r, false);
    QVERIFY(a.resume.enabled && a.retry.enabled && a.cancel.enabled && !a.remove.enabled && !a.pause.enabled);

    r.status = RunStatus::WaitingInput;
    a = run_actions(r, true);
    QVERIFY(!a.pause.enabled && a.cancel.enabled && a.retry.enabled && !a.remove.enabled);

    r.status = RunStatus::Failed;
    a = run_actions(r, false);
    QVERIFY(a.remove.enabled && a.retry.enabled && !a.cancel.enabled && !a.resume.enabled);
    QVERIFY(!a.cancel.reason.isEmpty());

    r.status = RunStatus::Interrupted;
    QVERIFY(run_actions(r, false).resume.enabled);

    r.session_uuid.clear();
    a = run_actions(r, false);
    QVERIFY(!a.open.enabled && !a.open.reason.isEmpty());
}

void TestChatRunViews::step_progress_and_text() {
    QVector<RunStep> steps{make_step(0, StepStatus::Done), make_step(1, StepStatus::Running),
                           make_step(2, StepStatus::Pending)};
    StepProgress p = step_progress(steps);
    QCOMPARE(p.total, 3);
    QCOMPARE(p.done, 1);
    QCOMPARE(p.current, 1);
    QCOMPARE(steps_started(steps), 2);

    TaskRun r = make_run(QStringLiteral("x"), RunStatus::Running, 1);
    r.plan_first = true;
    QVERIFY(!is_single_reply(r, steps));
    QCOMPARE(step_progress_text(r, steps), QStringLiteral("step 2/3"));

    steps[1].status = StepStatus::Done;
    steps[2].status = StepStatus::Failed;
    p = step_progress(steps);
    QCOMPARE(p.current, -1);
    QCOMPARE(p.failed, 1);
    QCOMPARE(step_progress_text(r, steps), QStringLiteral("step 3/3"));

    // A cloud reply is one turn: no "step n/m".
    r.plan_first = false;
    QVERIFY(is_single_reply(r, {make_step(0, StepStatus::Running)}));
    QVERIFY(step_progress_text(r, {make_step(0, StepStatus::Running)}).isEmpty());
    // A local run with one step is still a plan.
    r.engine = QString::fromLatin1(kEngineLocal);
    QVERIFY(!is_single_reply(r, {make_step(0, StepStatus::Running)}));
    QCOMPARE(step_progress_text(r, {make_step(0, StepStatus::Running)}), QStringLiteral("step 1/1"));
    // No plan yet.
    r.plan_first = true;
    r.engine = QString::fromLatin1(kEngineCloud);
    QVERIFY(step_progress_text(r, {}).isEmpty());
}

void TestChatRunViews::budget_line_texts() {
    TaskRun r = make_run(QStringLiteral("x"), RunStatus::Running, 1);
    r.plan_first = true;
    r.budget.max_credits = 50;
    r.budget.max_steps = 8;
    r.budget.max_minutes = 30;
    r.credits_spent = 12.5;
    r.started_at_ms = 1000;
    const QVector<RunStep> steps{make_step(0, StepStatus::Done), make_step(1, StepStatus::Running),
                                 make_step(2, StepStatus::Pending)};
    BudgetLine b = budget_line(r, steps, 1000 + 250000);
    QCOMPARE(b.credits, QStringLiteral("12.5 / 50 credits"));
    QCOMPARE(b.steps, QStringLiteral("2 / 8 steps"));
    QCOMPARE(b.time, QStringLiteral("4m 10s / 30 min"));
    QVERIFY(b.limited);
    QVERIFY(!b.stopped);
    QCOMPARE(b.credits_fraction, 0.25);

    r.budget = {};
    r.status = RunStatus::BudgetStopped;
    b = budget_line(r, steps, 1000 + 250000);
    QCOMPARE(b.credits, QStringLiteral("12.5 credits"));
    QCOMPARE(b.steps, QStringLiteral("2 step(s)"));
    QVERIFY(b.time.isEmpty());
    QVERIFY(!b.limited);
    QVERIFY(b.stopped);
    QCOMPARE(b.credits_fraction, -1.0);

    // A single reply has no step budget line.
    r.plan_first = false;
    QVERIFY(budget_line(r, {make_step(0, StepStatus::Done)}, 2000).steps.isEmpty());
}

void TestChatRunViews::duration_and_credit_formats() {
    QCOMPARE(format_duration(0), QStringLiteral("0s"));
    QCOMPARE(format_duration(-5), QStringLiteral("0s"));
    QCOMPARE(format_duration(42000), QStringLiteral("42s"));
    QCOMPARE(format_duration(185000), QStringLiteral("3m 05s"));
    QCOMPARE(format_duration(7620000), QStringLiteral("2h 07m"));
    QCOMPARE(format_duration(qint64(3) * 86400000 + qint64(4) * 3600000), QStringLiteral("3d 04h"));
    QCOMPARE(ActivityLog::format_duration_precise(850), QStringLiteral("850 ms"));
    QCOMPARE(ActivityLog::format_duration_precise(1234), QStringLiteral("1.2 s"));
    QCOMPARE(ActivityLog::format_duration_precise(14000), QStringLiteral("14 s"));
    QCOMPARE(format_credits(12.5), QStringLiteral("12.5"));
    QCOMPARE(format_credits(50.0), QStringLiteral("50"));
    QCOMPARE(format_credits(1234.5), QStringLiteral("1,234.5"));
    QCOMPARE(format_credits(0.35), QStringLiteral("0.35"));

    TaskRun r;
    QCOMPARE(run_elapsed_ms(r, 5000), qint64(0)); // not started
    r.started_at_ms = 1000;
    QCOMPARE(run_elapsed_ms(r, 5000), qint64(4000));
    r.finished_at_ms = 3000;
    QCOMPARE(run_elapsed_ms(r, 9000), qint64(2000)); // stops at the end
}

void TestChatRunViews::activity_pairs_tool_calls() {
    ActivityLog log;
    RunEvent call = make_event(1, RunEventKind::ToolCall, QStringLiteral("get_quote"), QStringLiteral("tool-1"), 1000);
    call.body = QStringLiteral("AAPL");
    RunEvent result = make_event(2, RunEventKind::ToolResult, QStringLiteral("get_quote"), QStringLiteral("tool-1"), 2200);
    result.body = QStringLiteral("189.5");
    result.data[QStringLiteral("durationMs")] = 1150;
    result.data[QStringLiteral("success")] = true;

    const ActivityLog::Change c1 = log.append(call);
    QCOMPARE(c1.inserted, 0);
    QCOMPARE(c1.updated, -1);
    QVERIFY(!log.at(0).has_end);
    const ActivityLog::Change c2 = log.append(result);
    QCOMPARE(c2.inserted, -1); // folded into the call's row
    QCOMPARE(c2.updated, 0);
    QCOMPARE(log.size(), 1);
    QVERIFY(log.at(0).has_end);
    QCOMPARE(log.at(0).duration_ms, qint64(1150));
    QVERIFY(!activity_row_failed(log.at(0)));
    const QString text = ActivityLog::row_text(log.at(0));
    QVERIFY(text.contains(QStringLiteral("AAPL")));
    QVERIFY(text.contains(QStringLiteral("189.5")));

    // A failed terminal tool without durationMs: the duration comes from the timestamps.
    RunEvent call2 = make_event(3, RunEventKind::ToolCall, QStringLiteral("t"), QStringLiteral("terminal-9"), 5000);
    RunEvent res2 = make_event(4, RunEventKind::ToolResult, QStringLiteral("t"), QStringLiteral("terminal-9"), 5800);
    res2.is_error = true;
    res2.data[QStringLiteral("success")] = false;
    log.append(call2);
    QCOMPARE(log.append(res2).updated, 1);
    QCOMPARE(log.at(1).duration_ms, qint64(800));
    QVERIFY(activity_row_failed(log.at(1)));

    // Two calls of the same tool pair by ref id, not by name or order.
    RunEvent a = make_event(5, RunEventKind::ToolCall, QStringLiteral("q"), QStringLiteral("tool-2"), 6000);
    RunEvent b = make_event(6, RunEventKind::ToolCall, QStringLiteral("q"), QStringLiteral("tool-3"), 6100);
    RunEvent rb = make_event(7, RunEventKind::ToolResult, QStringLiteral("q"), QStringLiteral("tool-3"), 6500);
    log.append(a);
    log.append(b);
    QCOMPARE(log.append(rb).updated, 3);
    QVERIFY(!log.at(2).has_end);
    QVERIFY(log.at(3).has_end);
}

void TestChatRunViews::activity_orphans_and_duplicates() {
    ActivityLog log;
    const RunEvent orphan = make_event(1, RunEventKind::ToolResult, QStringLiteral("x"), QStringLiteral("tool-9"), 100);
    QCOMPARE(log.append(orphan).inserted, 0); // an end without a start gets its own row
    QVERIFY(log.append(orphan).ignored);      // the same seq again
    QVERIFY(log.append(make_event(1, RunEventKind::Note, QStringLiteral("dup"))).ignored);
    QCOMPARE(log.size(), 1);
    QCOMPARE(log.last_seq(), 1);

    // History, then live events that overlap it.
    log.reset({make_event(1, RunEventKind::Note, QStringLiteral("a")), make_event(2, RunEventKind::Note, QStringLiteral("b"))});
    QCOMPARE(log.size(), 2);
    QVERIFY(log.append(make_event(2, RunEventKind::Note, QStringLiteral("b"))).ignored);
    QCOMPARE(log.append(make_event(3, RunEventKind::Note, QStringLiteral("c"))).inserted, 2);

    // Thousands of events stay cheap and ordered.
    QVector<RunEvent> many;
    for (int i = 1; i <= 5000; ++i)
        many.append(make_event(i, i % 2 ? RunEventKind::ToolCall : RunEventKind::ToolResult, QStringLiteral("t"),
                               QStringLiteral("tool-%1").arg((i + 1) / 2), i));
    log.reset(many);
    QCOMPARE(log.size(), 2500);
    QVERIFY(std::all_of(log.rows().cbegin(), log.rows().cend(), [](const ActivityRow& r) { return r.has_end; }));
}

void TestChatRunViews::activity_delegation_question_approval() {
    ActivityLog log;
    RunEvent ds = make_event(1, RunEventKind::Delegation, QStringLiteral("Delegating to 2 specialists"),
                             QStringLiteral("delegation-1"), 1000);
    ds.data[QStringLiteral("phase")] = QStringLiteral("start");
    RunEvent de = make_event(2, RunEventKind::Delegation, QStringLiteral("Specialists finished in 3 s"),
                             QStringLiteral("delegation-1"), 4000);
    de.data[QStringLiteral("phase")] = QStringLiteral("end");
    de.data[QStringLiteral("durationMs")] = 3000;
    QCOMPARE(log.append(ds).inserted, 0);
    QCOMPARE(log.append(de).updated, 0);
    QCOMPARE(log.at(0).duration_ms, qint64(3000));

    QCOMPARE(log.append(make_event(3, RunEventKind::Question, QStringLiteral("The agent asks"))).inserted, 1);
    QVERIFY(!log.at(1).settled);
    QCOMPARE(activity_row_tone(log.at(1)), Tone::Warning);
    const ActivityLog::Change answered = log.append(make_event(4, RunEventKind::Answer, QStringLiteral("Your answer")));
    QCOMPARE(answered.inserted, 2);
    QCOMPARE(answered.updated, 1);
    QVERIFY(log.at(1).settled);
    QCOMPARE(activity_row_tone(log.at(1)), Tone::Neutral);

    QCOMPARE(log.append(make_event(5, RunEventKind::Approval, QStringLiteral("Trade approval"), QStringLiteral("p-1")))
                 .inserted,
             3);
    RunEvent decided =
        make_event(6, RunEventKind::ApprovalDecided, QStringLiteral("Trade proposal executed"), QStringLiteral("p-1"));
    decided.data[QStringLiteral("status")] = QStringLiteral("executed");
    const ActivityLog::Change cd = log.append(decided);
    QCOMPARE(cd.inserted, 4);
    QCOMPARE(cd.updated, 3);
    QVERIFY(log.at(3).settled);
    QCOMPARE(activity_row_tone(log.at(4)), Tone::Success);
}

void TestChatRunViews::activity_step_durations_and_verbose_rows() {
    ActivityLog log;
    RunEvent s0 = make_event(1, RunEventKind::StepStart, QStringLiteral("Fetch"), {}, 1000);
    s0.step = 0;
    RunEvent note = make_event(2, RunEventKind::Note, QStringLiteral("Model call #1"), {}, 1500);
    note.data[QStringLiteral("verbose")] = true;
    RunEvent e0 = make_event(3, RunEventKind::StepEnd, QStringLiteral("Fetch"), {}, 61000);
    e0.step = 0;
    e0.data[QStringLiteral("status")] = QStringLiteral("done");
    log.append(s0);
    log.append(note);
    const ActivityLog::Change ce = log.append(e0);
    QCOMPARE(ce.inserted, 2); // a step end is its own row (the timeline keeps it)
    QCOMPARE(log.at(2).duration_ms, qint64(60000));
    QVERIFY(log.at(1).verbose);
    QVERIFY(!log.at(0).verbose);

    // Server work windows pair among themselves, not with run steps.
    RunEvent ws = make_event(4, RunEventKind::StepStart, QStringLiteral("Work window 1"), {}, 2000);
    ws.step = 0;
    ws.data[QStringLiteral("sub")] = QStringLiteral("window");
    ws.data[QStringLiteral("window")] = 1;
    RunEvent we = make_event(5, RunEventKind::StepEnd, QStringLiteral("Work window 1 done"), {}, 2500);
    we.step = 0;
    we.data[QStringLiteral("sub")] = QStringLiteral("window");
    we.data[QStringLiteral("window")] = 1;
    log.append(ws);
    log.append(we);
    QCOMPARE(log.at(4).duration_ms, qint64(500));
}

void TestChatRunViews::activity_row_tones() {
    ActivityRow open_call;
    open_call.ev = make_event(1, RunEventKind::ToolCall, QStringLiteral("t"), QStringLiteral("r"));
    QCOMPARE(activity_row_tone(open_call), Tone::Info);

    ActivityRow failed_step;
    failed_step.ev = make_event(2, RunEventKind::StepEnd, QStringLiteral("s"));
    failed_step.ev.is_error = true;
    QCOMPARE(activity_row_tone(failed_step), Tone::Danger);

    ActivityRow skipped;
    skipped.ev = make_event(3, RunEventKind::StepEnd, QStringLiteral("s"));
    skipped.ev.data[QStringLiteral("status")] = QStringLiteral("skipped");
    QCOMPARE(activity_row_tone(skipped), Tone::Neutral);

    ActivityRow error;
    error.ev = make_event(4, RunEventKind::Error, QStringLiteral("e"));
    QCOMPARE(activity_row_tone(error), Tone::Danger);

    ActivityRow note;
    note.ev = make_event(5, RunEventKind::Note, QStringLiteral("n"));
    QCOMPARE(activity_row_tone(note), Tone::Neutral);
}

void TestChatRunViews::compose_without_context_is_unchanged() {
    const ComposerContextData none;
    QCOMPARE(compose_run_prompt(QStringLiteral("Hello"), none).text, QStringLiteral("Hello"));
    QCOMPARE(compose_run_prompt(QStringLiteral("  spaced  "), none).text, QStringLiteral("  spaced  "));
}

void TestChatRunViews::compose_inlines_context_and_attachment() {
    ComposerContextData c;
    c.symbols = {QStringLiteral("AAPL"), QStringLiteral("MSFT")};
    c.portfolio_id = QStringLiteral("p1");
    c.portfolio_name = QStringLiteral("Growth");
    c.profile_name = QStringLiteral("Balanced");
    const ComposedPrompt p = compose_run_prompt(QStringLiteral("Compare them"), c);
    QVERIFY(p.text.startsWith(QStringLiteral("Compare them\n\n")));
    QVERIFY(p.text.contains(QStringLiteral("Symbols: AAPL, MSFT")));
    QVERIFY(p.text.contains(QStringLiteral("Portfolio: Growth")));
    QVERIFY(p.text.contains(QStringLiteral("Investment profile: Balanced")));
    QVERIFY(!p.attachment_truncated);
    QCOMPARE(p.attachment_chars_sent, 0);

    c.attachment_name = QStringLiteral("notes.txt");
    c.attachment_text = QStringLiteral("line1\nline2");
    const ComposedPrompt p2 = compose_run_prompt(QStringLiteral("Summarise"), c);
    QVERIFY(p2.text.contains(QStringLiteral("Attached text \"notes.txt\":\n<<<\nline1\nline2\n>>>")));
    QCOMPARE(p2.attachment_chars_sent, 11);
    QVERIFY(!p2.attachment_truncated);

    // An attachment alone (no chips) still follows the message.
    ComposerContextData only;
    only.attachment_text = QStringLiteral("data");
    QVERIFY(compose_run_prompt(QStringLiteral("Look"), only).text.startsWith(QStringLiteral("Look\n\nAttached text")));
}

void TestChatRunViews::compose_truncates_a_long_attachment() {
    ComposerContextData c;
    c.attachment_name = QStringLiteral("big.txt");
    c.attachment_text = QString(40000, QLatin1Char('x'));
    const ComposedPrompt p = compose_run_prompt(QStringLiteral("Read this"), c, 32000, 48000);
    QVERIFY(p.attachment_truncated);
    QVERIFY(p.attachment_chars_sent <= 32000);
    QVERIFY(p.attachment_chars_sent > 31000);
    QVERIFY(p.text.size() <= 48000);
    QVERIFY(p.text.contains(QStringLiteral("more characters of the attachment were not included")));

    // The cut never splits a surrogate pair.
    QString s(31903, QLatin1Char('a'));
    s += QChar(0xD83D);
    s += QChar(0xDE00);
    s += QString(10000, QLatin1Char('b'));
    c.attachment_text = s;
    const ComposedPrompt q = compose_run_prompt(QStringLiteral("x"), c, 32000, 48000);
    QVERIFY(q.attachment_truncated);
    QCOMPARE(q.attachment_chars_sent, 31903);
    const qsizetype open_at = q.text.indexOf(QStringLiteral("<<<\n")) + 4;
    QVERIFY(!q.text.at(open_at + q.attachment_chars_sent - 1).isHighSurrogate());
}

void TestChatRunViews::compose_keeps_within_the_prompt_limit() {
    ComposerContextData c;
    c.attachment_name = QStringLiteral("a");
    c.attachment_text = QString(32000, QLatin1Char('y'));
    const QString user(20000, QLatin1Char('u')); // the composer's own maximum
    const ComposedPrompt p = compose_run_prompt(user, c, 32000, kRunPromptMaxChars);
    QVERIFY(p.text.size() <= kRunPromptMaxChars);
    QVERIFY(p.attachment_truncated);
    QVERIFY(p.attachment_chars_sent > 27000);
}

void TestChatRunViews::paused_run_clock_stands_still() {
    // Recorded with the working clock: 2 minutes of work, then paused.
    TaskRun r = make_run(QStringLiteral("p"), RunStatus::Paused, 1);
    r.started_at_ms = 1000;
    r.active_ms = 120'000;
    QCOMPARE(run_elapsed_ms(r, 10'000'000), qint64(120'000));
    QCOMPARE(run_elapsed_ms(r, 99'000'000), qint64(120'000));
    // Working again: the current stretch counts.
    r.status = RunStatus::Running;
    r.active_since_ms = 50'000'000;
    QCOMPARE(run_elapsed_ms(r, 50'030'000), qint64(150'000));
    // A legacy record (no clock) that is paused stops at its last state change.
    TaskRun legacy = make_run(QStringLiteral("l"), RunStatus::Paused, 1);
    legacy.started_at_ms = 1000;
    legacy.updated_at_ms = 61'000;
    QCOMPARE(run_elapsed_ms(legacy, 99'000'000), qint64(60'000));
    QVERIFY(budget_line(r, {}, 50'030'000).time.isEmpty()); // no time limit set
    r.budget.max_minutes = 10;
    QVERIFY(budget_line(r, {}, 50'030'000).time.startsWith(QStringLiteral("2m 30s")));
}

void TestChatRunViews::pending_step_shows_no_duration() {
    RunStep s = make_step(0, StepStatus::Pending);
    s.started_at_ms = 1000; // a restart put the running step back to pending
    QCOMPARE(step_elapsed_ms(s, 99'000), qint64(-1));
    s.status = StepStatus::Running;
    QCOMPARE(step_elapsed_ms(s, 5000), qint64(4000));
    s.status = StepStatus::Done;
    s.finished_at_ms = 3000;
    QCOMPARE(step_elapsed_ms(s, 99'000), qint64(2000));
}

void TestChatRunViews::unreported_credits_are_labelled() {
    TaskRun r = make_run(QStringLiteral("u"), RunStatus::Completed, 1);
    r.engine = QStringLiteral("cloud");
    QCOMPARE(credits_text(r, QStringLiteral("5 credits")), QStringLiteral("5 credits"));
    r.credits_unreported = 1;
    QCOMPARE(credits_text(r, QStringLiteral("0 credits")), QStringLiteral("credits not reported"));
    r.credits_spent = 5;
    QVERIFY(credits_text(r, QStringLiteral("5 credits")).contains(QStringLiteral("not reported")));
    QVERIFY(budget_line(r, {}, 10).credits.contains(QStringLiteral("not reported")));
    r.engine = QStringLiteral("local");
    QCOMPARE(credits_text(r, QStringLiteral("0 credits")), QStringLiteral("0 credits"));
}

void TestChatRunViews::budget_confirmation_threshold() {
    const QString plan = QString::fromLatin1(kExecutorCloudPlan);
    // The composer's former fixed budget (50 credits / 8 steps) asks first.
    BudgetConfirmation c = budget_confirmation(plan, {kDefaultMaxCredits, kDefaultMaxSteps, 0}, 12, 120);
    QVERIFY(c.ask);
    QVERIFY(c.question.contains(QStringLiteral("50")));
    QVERIFY(c.question.contains(QStringLiteral("120")));
    // A small budget starts at once.
    c = budget_confirmation(plan, {20, 3, 15}, 12);
    QVERIFY(!c.ask);
    QVERIFY(c.summary.contains(QStringLiteral("15 min")));
    // One reply / local never ask.
    QVERIFY(!budget_confirmation(QString::fromLatin1(kExecutorCloudTurn), {}, 12).ask);
    c = budget_confirmation(QString::fromLatin1(kExecutorLocal), {0, 6, 0}, 12);
    QVERIFY(!c.ask);
    QVERIFY(c.summary.contains(QStringLiteral("no credits")));
    // No credit limit: asks, and says so.
    c = budget_confirmation(plan, {0, 0, 0}, 12);
    QVERIFY(c.ask);
    QVERIFY(c.question.contains(QStringLiteral("no credit limit")));
}

namespace {

fincept::ai_chat::ModelChoice harness_choice(const QString& id, const QString& label, const QString& vendor) {
    fincept::ai_chat::ModelChoice c;
    c.kind = fincept::ai_chat::ModelChoice::Kind::Harness;
    c.harness_id = id;
    c.provider = QStringLiteral("harness:") + id;
    c.label = label;
    c.vendor = vendor;
    return c;
}

fincept::ai_chat::ModelChoice fincept_choice() {
    fincept::ai_chat::ModelChoice c;
    c.kind = fincept::ai_chat::ModelChoice::Kind::Fincept;
    c.label = QStringLiteral("Fincept LLM");
    return c;
}

} // namespace

void TestChatRunViews::harness_option_follows_executor_and_route() {
    using fincept::ai_chat::RouteResult;
    using fincept::ai_chat::SkippedChoice;
    // No harness executor in this build: not listed at all.
    RouteResult route;
    route.chain << harness_choice(QStringLiteral("claude"), QStringLiteral("Claude Code · sonnet"), QStringLiteral("Anthropic"))
                << fincept_choice();
    HarnessEngineOption o = harness_engine_option(false, route);
    QVERIFY(!o.listed);
    QVERIFY(!o.enabled);

    // The route holds a harness: enabled, labelled with it (the first harness of the chain).
    route.chain.prepend(fincept_choice()); // an API / Fincept entry first does not hide the harness
    o = harness_engine_option(true, route);
    QVERIFY(o.listed);
    QVERIFY(o.enabled);
    QCOMPARE(o.choice.harness_id, QStringLiteral("claude"));
    QCOMPARE(o.label, QStringLiteral("Claude Code · sonnet"));
    QVERIFY(o.reason.isEmpty());

    // No harness in the chain, the router passed one over: disabled with its reason.
    RouteResult skipped;
    skipped.chain << fincept_choice();
    skipped.skipped << SkippedChoice{QStringLiteral("Profile “Fast”"), QStringLiteral("it has no API key"), QString()}
                    << SkippedChoice{QStringLiteral("Claude Code"),
                                     QStringLiteral("Workbench only — sign in with an API key"), QStringLiteral("claude")}
                    << SkippedChoice{QStringLiteral("Codex"), QStringLiteral("Signed out — run codex login"),
                                     QStringLiteral("codex")};
    o = harness_engine_option(true, skipped);
    QVERIFY(o.listed);
    QVERIFY(!o.enabled);
    QCOMPARE(o.state, QStringLiteral("Workbench only")); // the first harness's short reason
    QVERIFY(o.reason.contains(QStringLiteral("Claude Code: Workbench only — sign in with an API key")));
    QVERIFY(o.reason.contains(QStringLiteral("Codex: Signed out")));
    QVERIFY(!o.reason.contains(QStringLiteral("Fast"))); // API profiles are not about the harness

    // Passed over by the data policy (no " — " in the reason): a generic short state.
    RouteResult policy;
    policy.chain << fincept_choice();
    policy.skipped << SkippedChoice{QStringLiteral("Claude Code"),
                                    QStringLiteral("not used automatically for Chat Mode · harness tasks: it sends your "
                                                   "financial data to Anthropic."),
                                    QStringLiteral("claude")};
    o = harness_engine_option(true, policy);
    QVERIFY(!o.enabled);
    QVERIFY(!o.state.isEmpty());
    QVERIFY(o.state.size() < 40);
    QVERIFY(o.reason.contains(QStringLiteral("financial data")));

    // Nothing connected: still listed, disabled, says how to set one up.
    RouteResult none;
    none.chain << fincept_choice();
    o = harness_engine_option(true, none);
    QVERIFY(o.listed);
    QVERIFY(!o.enabled);
    QVERIFY(!o.state.isEmpty());
    QVERIFY(o.reason.contains(QStringLiteral("API key")));
}

void TestChatRunViews::harness_runs_spend_no_credits_and_cannot_pause() {
    const QString harness = QString::fromLatin1(kHarnessEngineId);
    QCOMPARE(executor_for(harness, true), harness); // TaskRunTypes maps the engine to its executor
    // No Fincept credits: never asks, says so, keeps the time limit in the summary.
    const BudgetConfirmation c = budget_confirmation(harness, {0, 6, 30}, 12, 3);
    QVERIFY(!c.ask);
    QVERIFY(c.question.isEmpty());
    QVERIFY(c.summary.contains(QStringLiteral("no Fincept credits")));
    QVERIFY(c.summary.contains(QStringLiteral("30 min")));
    QVERIFY(!estimate_run_cost(harness, {100, 6, 0}, 12).uses_credits);

    TaskRun r = make_run(QStringLiteral("h"), RunStatus::Running, 1);
    r.engine = harness;
    r.plan_first = true;
    QVERIFY(!run_uses_credits(r));
    r.credits_unreported = 2; // never shown as unreported credits
    QCOMPARE(credits_text(r, QStringLiteral("0 credits")), QStringLiteral("0 credits"));
    // HarnessRunExecutor::can_pause() is false: pause disabled with a harness-specific reason.
    const RunActions a = run_actions(r, /*executor_can_pause=*/false);
    QVERIFY(!a.pause.enabled);
    QVERIFY(a.pause.reason.contains(QStringLiteral("harness")));
    QVERIFY(a.cancel.enabled);

    TaskRun cloud = make_run(QStringLiteral("c"), RunStatus::Running, 1);
    cloud.engine = QString::fromLatin1(kEngineCloud);
    QVERIFY(run_uses_credits(cloud));
    TaskRun local = cloud;
    local.engine = QString::fromLatin1(kEngineLocal);
    QVERIFY(!run_uses_credits(local));
}

QTEST_GUILESS_MAIN(TestChatRunViews)
#include "tst_chat_run_views.moc"
