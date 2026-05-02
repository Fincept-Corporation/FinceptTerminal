// test_dispatcher.cpp — Phase 5 ToolDispatcher tests.
//
// Validates the multi-round orchestration loop using a mock adapter and
// pre-registered MCP tools. Confirms:
//   • No tool calls → text returned in one round
//   • One tool call → execute → follow-up returns final text
//   • Two tool calls in one round → fan-out via execute_openai_function_async
//   • Cancellation between rounds → loop terminates with error
//   • Max rounds exhausted → max_rounds_hit set

#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/McpTypes.h"
#include "mcp/dispatch/ProviderAdapter.h"
#include "mcp/dispatch/ToolDispatcher.h"

#include <QCoreApplication>
#include <QFuture>
#include <QObject>
#include <QPromise>
#include <QTest>

#include <atomic>
#include <memory>

using fincept::ai_chat::ConversationMessage;
using fincept::mcp::McpProvider;
using fincept::mcp::ToolDef;
using fincept::mcp::ToolResult;
using fincept::mcp::dispatch::ProviderAdapter;
using fincept::mcp::dispatch::ToolDispatcher;
using fincept::mcp::dispatch::ToolInvocation;
using fincept::mcp::dispatch::ToolResultEnvelope;

namespace {

/// Test adapter with scriptable behaviour. Each round, returns a queued
/// "response" — either a final-text response or a synthetic tool-call
/// response that the dispatcher must execute and follow-up.
class ScriptedAdapter : public ProviderAdapter {
  public:
    QString id() const override { return "scripted"; }
    int default_max_rounds() const override { return max_rounds_; }

    QJsonObject build_initial_request(const QString&,
                                       const std::vector<ConversationMessage>&,
                                       const QJsonArray&) const override {
        return QJsonObject{{"round", 0}};
    }

    std::vector<ToolInvocation> parse_tool_invocations(const QJsonObject& response) const override {
        std::vector<ToolInvocation> out;
        QJsonArray calls = response["tool_calls"].toArray();
        for (const auto& c : calls) {
            QJsonObject co = c.toObject();
            ToolInvocation inv;
            inv.call_id = co["id"].toString();
            inv.function_name = co["name"].toString();
            inv.arguments = co["arguments"].toObject();
            inv.origin = "scripted";
            out.push_back(std::move(inv));
        }
        return out;
    }

    QJsonObject build_followup_request(const QJsonObject&,
                                        const std::vector<ToolResultEnvelope>& results,
                                        const QString&,
                                        const std::vector<ConversationMessage>&) const override {
        QJsonArray rs;
        for (const auto& r : results)
            rs.append(QJsonObject{{"call_id", r.call_id}, {"result", r.result.to_json()}});
        QJsonObject body;
        body["followup_results"] = rs;
        return body;
    }

    QString extract_text(const QJsonObject& response) const override {
        return response["text"].toString();
    }

    int max_rounds_ = 5;
};

} // anonymous namespace

class TestDispatcher : public QObject {
    Q_OBJECT

  private:
    void clear_provider() { McpProvider::instance().clear(); }

  private slots:

    void cleanup() { clear_provider(); }

    // ── No tool calls → text returned ────────────────────────────────────

    void noToolCalls_returnsTextImmediately() {
        clear_provider();
        ScriptedAdapter adapter;
        ToolDispatcher dispatcher;

        auto send = [](QJsonObject) {
            QPromise<QJsonObject> p;
            p.start();
            p.addResult(QJsonObject{{"text", "hello"}});
            p.finish();
            return p.future();
        };

        auto out = dispatcher.run("hi", {}, adapter, send, {}, {});
        QCOMPARE(out.text, QString("hello"));
        QCOMPARE(out.rounds_used, 1);
        QVERIFY(!out.max_rounds_hit);
        QVERIFY(out.error.isEmpty());
    }

    // ── One tool call → execute → follow-up returns text ────────────────

    void singleToolCall_executesAndFollowsUp() {
        clear_provider();
        ToolDef t;
        t.name = "echo";
        t.handler = [](const QJsonObject& args) {
            return ToolResult::ok_data(QJsonObject{{"echo", args["msg"].toString()}});
        };
        McpProvider::instance().register_tool(std::move(t));

        ScriptedAdapter adapter;
        ToolDispatcher dispatcher;

        // Round 0: request a tool call. Round 1: return text.
        auto round = std::make_shared<int>(0);
        auto send = [round](QJsonObject) {
            QPromise<QJsonObject> p;
            p.start();
            if (*round == 0) {
                ++*round;
                p.addResult(QJsonObject{
                    {"tool_calls",
                     QJsonArray{QJsonObject{{"id", "c1"},
                                            {"name", "fincept-terminal__echo"},
                                            {"arguments", QJsonObject{{"msg", "hi"}}}}}}});
            } else {
                p.addResult(QJsonObject{{"text", "done"}});
            }
            p.finish();
            return p.future();
        };

        auto out = dispatcher.run("hi", {}, adapter, send, {}, {});
        QCOMPARE(out.text, QString("done"));
        QCOMPARE(out.rounds_used, 2);
    }

    // ── Two tool calls in one round → both execute via fan-out ─────────

    void twoToolCalls_inOneRound_bothExecute() {
        clear_provider();
        std::atomic<int> call_count{0};
        ToolDef t;
        t.name = "counter";
        t.handler = [&call_count](const QJsonObject&) {
            ++call_count;
            return ToolResult::ok("ok");
        };
        McpProvider::instance().register_tool(std::move(t));

        ScriptedAdapter adapter;
        ToolDispatcher dispatcher;

        auto round = std::make_shared<int>(0);
        auto send = [round](QJsonObject) {
            QPromise<QJsonObject> p;
            p.start();
            if (*round == 0) {
                ++*round;
                p.addResult(QJsonObject{
                    {"tool_calls",
                     QJsonArray{
                         QJsonObject{{"id", "a"}, {"name", "fincept-terminal__counter"}, {"arguments", QJsonObject{}}},
                         QJsonObject{{"id", "b"}, {"name", "fincept-terminal__counter"}, {"arguments", QJsonObject{}}}}}});
            } else {
                p.addResult(QJsonObject{{"text", "done"}});
            }
            p.finish();
            return p.future();
        };

        auto out = dispatcher.run("hi", {}, adapter, send, {}, {});
        QCOMPARE(out.text, QString("done"));
        QCOMPARE(call_count.load(), 2);
    }

    // ── Cancellation between rounds ──────────────────────────────────────

    void cancellation_terminatesLoop() {
        clear_provider();
        ToolDef t;
        t.name = "noop";
        t.handler = [](const QJsonObject&) { return ToolResult::ok("ok"); };
        McpProvider::instance().register_tool(std::move(t));

        ScriptedAdapter adapter;
        ToolDispatcher dispatcher;
        auto cancel_flag = std::make_shared<std::atomic<bool>>(false);

        // Always returns a tool call — would loop forever absent cancellation.
        auto send = [cancel_flag](QJsonObject) {
            cancel_flag->store(true); // cancel after first response
            QPromise<QJsonObject> p;
            p.start();
            p.addResult(QJsonObject{
                {"tool_calls",
                 QJsonArray{QJsonObject{{"id", "x"},
                                        {"name", "fincept-terminal__noop"},
                                        {"arguments", QJsonObject{}}}}}});
            p.finish();
            return p.future();
        };

        auto out = dispatcher.run("hi", {}, adapter, send, {}, cancel_flag);
        QVERIFY(!out.error.isEmpty());
        QVERIFY(out.error.contains("cancelled"));
    }

    // ── Max rounds exhausted ─────────────────────────────────────────────

    void maxRounds_exhausted() {
        clear_provider();
        ToolDef t;
        t.name = "noop";
        t.handler = [](const QJsonObject&) { return ToolResult::ok("ok"); };
        McpProvider::instance().register_tool(std::move(t));

        ScriptedAdapter adapter;
        adapter.max_rounds_ = 3;
        ToolDispatcher dispatcher;

        // Always returns a tool call — exhausts rounds.
        auto send = [](QJsonObject) {
            QPromise<QJsonObject> p;
            p.start();
            p.addResult(QJsonObject{
                {"text", ""},
                {"tool_calls",
                 QJsonArray{QJsonObject{{"id", "x"},
                                        {"name", "fincept-terminal__noop"},
                                        {"arguments", QJsonObject{}}}}}});
            p.finish();
            return p.future();
        };

        auto out = dispatcher.run("hi", {}, adapter, send, {}, {});
        QVERIFY(out.max_rounds_hit);
        QCOMPARE(out.rounds_used, 3);
    }
};

QTEST_MAIN(TestDispatcher)
#include "test_dispatcher.moc"
