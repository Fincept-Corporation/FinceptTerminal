// tst_harness_run_executor.cpp — the pure mapping of Chat Mode's harness engine
// (services/agent_tasks/HarnessRunExecutor.h, namespace harness_run_mapping):
// harness transcript events → run events, todo lists → steps, streamed text
// deltas, session binding and the 0-credit usage record. The executor itself
// (worker pool, HarnessChatBackend) is not linked.

#include "services/agent_tasks/HarnessRunExecutor.h"

#include <QtTest>

using namespace fincept::agent_tasks;
namespace hrm = fincept::agent_tasks::harness_run_mapping;
namespace cad = fincept::services::coding_agents;

namespace {

cad::AgentEvent ev(cad::EventKind kind, const QString& title, const QString& body = {}, const QString& ref = {},
                   bool partial = false, bool is_error = false) {
    cad::AgentEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.ref_id = ref;
    e.partial = partial;
    e.is_error = is_error;
    return e;
}

} // namespace

class TestHarnessRunExecutor : public QObject {
    Q_OBJECT
  private slots:
    void codex_todo_list_open_items_first_one_running();
    void codex_todo_list_complete_plan_has_no_running();
    void claude_todo_write_statuses();
    void todos_become_steps();
    void plan_status_maps_to_plan_event_and_todos();
    void todo_write_call_is_a_verbose_tool_call_with_todos();
    void tool_call_and_result_pair_by_ref_id();
    void codex_command_is_a_call_and_result_pair();
    void text_deltas_from_cumulative_partials();
    void separate_messages_are_separated();
    void thinking_only_complete();
    void session_id_from_claude_and_codex_status();
    void remote_id_round_trip();
    void usage_has_zero_credits_and_turn_end();
    void error_event_carries_code();
};

void TestHarnessRunExecutor::codex_todo_list_open_items_first_one_running() {
    const auto todos = hrm::parse_codex_todo(QStringLiteral("[x] Open the watchlist\n[ ] Read quotes\n[ ] Summarise\n"));
    QCOMPARE(todos.size(), 3);
    QCOMPARE(todos[0].text, QStringLiteral("Open the watchlist"));
    QCOMPARE(todos[0].status, StepStatus::Done);
    QCOMPARE(todos[1].status, StepStatus::Running);
    QCOMPARE(todos[2].status, StepStatus::Pending);
}

void TestHarnessRunExecutor::codex_todo_list_complete_plan_has_no_running() {
    const auto todos = hrm::parse_codex_todo(QStringLiteral("[x] a\n[ ] b"), /*plan_complete=*/true);
    QCOMPARE(todos.size(), 2);
    QCOMPARE(todos[1].status, StepStatus::Pending);
}

void TestHarnessRunExecutor::claude_todo_write_statuses() {
    const QString input = QStringLiteral(
        R"({"todos":[{"content":"Open AAPL","status":"completed","activeForm":"Opening"},)"
        R"({"content":"Read ratios","status":"in_progress"},{"content":"Report","status":"pending"},)"
        R"({"content":"","status":"pending"}]})");
    const auto todos = hrm::parse_todo_write(input);
    QCOMPARE(todos.size(), 3); // the empty one is dropped
    QCOMPARE(todos[0].status, StepStatus::Done);
    QCOMPARE(todos[1].status, StepStatus::Running);
    QCOMPARE(todos[2].status, StepStatus::Pending);
    QVERIFY(hrm::parse_todo_write(QStringLiteral("not json")).isEmpty());
}

void TestHarnessRunExecutor::todos_become_steps() {
    const auto steps = hrm::steps_from_todos(hrm::parse_codex_todo(QStringLiteral("[ ] one\n[ ] two")));
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps[0].idx, 0);
    QCOMPARE(steps[1].idx, 1);
    QCOMPARE(steps[1].title, QStringLiteral("two"));
    QCOMPARE(steps[1].status, StepStatus::Pending); // statuses travel through step_changed
}

void TestHarnessRunExecutor::plan_status_maps_to_plan_event_and_todos() {
    hrm::DeltaTracker text;
    const auto m = hrm::map_event(ev(cad::EventKind::Status, QStringLiteral("Plan updated"), QStringLiteral("[x] a\n[ ] b")),
                                  text);
    QVERIFY(m.todos.has_value());
    QCOMPARE(m.todos->size(), 2);
    QCOMPARE(m.events.size(), 1);
    QCOMPARE(m.events[0].kind, RunEventKind::Plan);
    QVERIFY(m.events[0].data.value(QStringLiteral("verbose")).toBool());
    // An ordinary status is a verbose note, never a plan.
    const auto n = hrm::map_event(ev(cad::EventKind::Status, QStringLiteral("Turn started")), text);
    QVERIFY(!n.todos.has_value());
    QCOMPARE(n.events[0].kind, RunEventKind::Note);
}

void TestHarnessRunExecutor::todo_write_call_is_a_verbose_tool_call_with_todos() {
    hrm::DeltaTracker text;
    const auto m = hrm::map_event(
        ev(cad::EventKind::ToolCall, QStringLiteral("TodoWrite"),
           QStringLiteral(R"({"todos":[{"content":"x","status":"in_progress"}]})"), QStringLiteral("toolu_1")),
        text);
    QVERIFY(m.todos.has_value());
    QCOMPARE(m.todos->constFirst().status, StepStatus::Running);
    QCOMPARE(m.events[0].kind, RunEventKind::ToolCall);
    QVERIFY(m.events[0].data.value(QStringLiteral("verbose")).toBool());
}

void TestHarnessRunExecutor::tool_call_and_result_pair_by_ref_id() {
    hrm::DeltaTracker text;
    const auto call = hrm::map_event(
        ev(cad::EventKind::ToolCall, QStringLiteral("fincept.ui_open"), QStringLiteral(R"({"screen":"watchlist"})"),
           QStringLiteral("toolu_7")),
        text);
    const auto result = hrm::map_event(ev(cad::EventKind::ToolResult, QStringLiteral("fincept.ui_open"),
                                          QStringLiteral("Opened Watchlist"), QStringLiteral("toolu_7"), false, true),
                                       text);
    QCOMPARE(call.events[0].kind, RunEventKind::ToolCall);
    QCOMPARE(call.events[0].title, QStringLiteral("ui_open"));
    QCOMPARE(call.events[0].data.value(QStringLiteral("tool")).toString(), QStringLiteral("ui_open"));
    QCOMPARE(result.events[0].kind, RunEventKind::ToolResult);
    QCOMPARE(result.events[0].ref_id, call.events[0].ref_id);
    QVERIFY(result.events[0].is_error);
    QCOMPARE(result.events[0].data.value(QStringLiteral("success")).toBool(), false);
    QVERIFY(!call.events[0].data.value(QStringLiteral("verbose")).toBool());
}

void TestHarnessRunExecutor::codex_command_is_a_call_and_result_pair() {
    hrm::DeltaTracker text;
    const auto started = hrm::map_event(
        ev(cad::EventKind::Command, QStringLiteral("$ ls"), QStringLiteral("(running)"), QStringLiteral("item_3")), text);
    const auto done = hrm::map_event(
        ev(cad::EventKind::Command, QStringLiteral("$ ls  → exit 0"), QStringLiteral("a.txt"), QStringLiteral("item_3")),
        text);
    QCOMPARE(started.events[0].kind, RunEventKind::ToolCall);
    QCOMPARE(done.events[0].kind, RunEventKind::ToolResult);
    QCOMPARE(done.events[0].ref_id, QStringLiteral("item_3"));
    QCOMPARE(done.events[0].body, QStringLiteral("a.txt"));
}

void TestHarnessRunExecutor::text_deltas_from_cumulative_partials() {
    hrm::DeltaTracker text;
    QCOMPARE(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("Hel"),
                               QStringLiteral("m1:0"), true),
                            text)
                 .text_delta,
             QStringLiteral("Hel"));
    QCOMPARE(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("Hello"),
                               QStringLiteral("m1:0"), true),
                            text)
                 .text_delta,
             QStringLiteral("lo"));
    // The complete event repeats the whole message: only what is new goes out.
    QCOMPARE(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("Hello world"),
                               QStringLiteral("m1:0")),
                            text)
                 .text_delta,
             QStringLiteral(" world"));
    QVERIFY(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("Hello world"),
                              QStringLiteral("m1:0")),
                           text)
                .text_delta.isEmpty());
}

void TestHarnessRunExecutor::separate_messages_are_separated() {
    hrm::DeltaTracker text;
    QCOMPARE(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("First"),
                               QStringLiteral("item_1")),
                            text)
                 .text_delta,
             QStringLiteral("First"));
    QCOMPARE(hrm::map_event(ev(cad::EventKind::Text, QStringLiteral("Assistant"), QStringLiteral("Second"),
                               QStringLiteral("item_2")),
                            text)
                 .text_delta,
             QStringLiteral("\n\nSecond"));
}

void TestHarnessRunExecutor::thinking_only_complete() {
    hrm::DeltaTracker text;
    QVERIFY(hrm::map_event(ev(cad::EventKind::Thinking, QStringLiteral("Thinking"), QStringLiteral("hmm"),
                              QStringLiteral("m1:0"), true),
                           text)
                .events.isEmpty());
    const auto m = hrm::map_event(ev(cad::EventKind::Thinking, QStringLiteral("Thinking"), QStringLiteral("hmm ok")), text);
    QCOMPARE(m.events.size(), 1);
    QCOMPARE(m.events[0].kind, RunEventKind::Thinking);
    QVERIFY(m.text_delta.isEmpty());
}

void TestHarnessRunExecutor::session_id_from_claude_and_codex_status() {
    QCOMPARE(hrm::session_id_from_status(ev(cad::EventKind::Status, QStringLiteral("Session started · claude-sonnet"),
                                            QStringLiteral("model: x\nsession: 5c1d-01\nworking folder: /w"))),
             QStringLiteral("5c1d-01"));
    QCOMPARE(hrm::session_id_from_status(
                 ev(cad::EventKind::Status, QStringLiteral("Thread started"), QStringLiteral("thread: 0199a213"))),
             QStringLiteral("0199a213"));
    QVERIFY(hrm::session_id_from_status(ev(cad::EventKind::Status, QStringLiteral("Turn started"))).isEmpty());
    hrm::DeltaTracker text;
    const auto m = hrm::map_event(
        ev(cad::EventKind::Status, QStringLiteral("Thread started"), QStringLiteral("thread: t-9")), text);
    QCOMPARE(m.session_id, QStringLiteral("t-9"));
}

void TestHarnessRunExecutor::remote_id_round_trip() {
    const QString r = hrm::remote_id(QStringLiteral("claude"), QStringLiteral("abc"));
    QCOMPARE(r, QStringLiteral("claude:abc"));
    QCOMPARE(hrm::session_of(r, QStringLiteral("claude")), QStringLiteral("abc"));
    QVERIFY(hrm::session_of(r, QStringLiteral("codex")).isEmpty()); // another harness's session is not resumed
    QVERIFY(hrm::remote_id(QStringLiteral("claude"), {}).isEmpty());
}

void TestHarnessRunExecutor::usage_has_zero_credits_and_turn_end() {
    const RunEvent u = hrm::usage_event(1200, 300, QStringLiteral("Anthropic"), 0.0123);
    QCOMPARE(u.kind, RunEventKind::Usage);
    QVERIFY(u.data.value(QStringLiteral("turn_end")).toBool());
    QCOMPARE(u.data.value(QStringLiteral("credits_charged")).toDouble(), 0.0);
    QCOMPARE(u.data.value(QStringLiteral("tokens_total")).toDouble(), 1500.0);
    QVERIFY(u.data.value(QStringLiteral("billing")).toString().contains(QStringLiteral("0 credits")));
    QVERIFY(u.data.value(QStringLiteral("billing")).toString().contains(QStringLiteral("Anthropic API key")));
    QVERIFY(u.data.contains(QStringLiteral("cost_usd")));
    const RunEvent no_cost = hrm::usage_event(10, 5, QStringLiteral("OpenAI"), std::nullopt);
    QVERIFY(!no_cost.data.contains(QStringLiteral("cost_usd")));
}

void TestHarnessRunExecutor::error_event_carries_code() {
    hrm::DeltaTracker text;
    const auto m = hrm::map_event(ev(cad::EventKind::Error, QStringLiteral("Rate limit reached"), QStringLiteral("x"), {},
                                     false, true),
                                  text);
    QCOMPARE(m.events[0].kind, RunEventKind::Error);
    QVERIFY(m.events[0].is_error);
    QCOMPARE(m.events[0].data.value(QStringLiteral("code")).toString(), QStringLiteral("harness_error"));
}

QTEST_GUILESS_MAIN(TestHarnessRunExecutor)
#include "tst_harness_run_executor.moc"
