// test_async.cpp — Phase 4 async-handler dispatch tests.
//
// Validates the dual-shape contract on McpProvider: sync handlers return
// immediately, async handlers resolve their QPromise on a callback, and
// the provider's timeout watchdog fires when the handler stalls.

#include "mcp/AsyncDispatch.h"
#include "mcp/McpProvider.h"
#include "mcp/McpTypes.h"

#include <QCoreApplication>
#include <QFuture>
#include <QObject>
#include <QPromise>
#include <QTest>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <memory>

using fincept::mcp::AsyncToolHandler;
using fincept::mcp::McpProvider;
using fincept::mcp::ToolContext;
using fincept::mcp::ToolDef;
using fincept::mcp::ToolHandler;
using fincept::mcp::ToolResult;

class TestAsyncDispatch : public QObject {
    Q_OBJECT

  private:
    void clear_provider() {
        McpProvider::instance().clear();
    }

  private slots:

    void cleanup() {
        clear_provider();
    }

    // ── Sync legacy handler still works ─────────────────────────────────

    void syncHandler_returnsResult() {
        clear_provider();
        ToolDef t;
        t.name = "sync_ok";
        t.handler = [](const QJsonObject&) -> ToolResult {
            return ToolResult::ok_data(QJsonObject{{"value", 42}});
        };
        McpProvider::instance().register_tool(std::move(t));

        auto fut = McpProvider::instance().call_tool_async("sync_ok", {});
        fut.waitForFinished();
        QVERIFY(fut.resultCount() > 0);
        const auto r = fut.result();
        QVERIFY(r.success);
        QCOMPARE(r.data.toObject()["value"].toInt(), 42);
    }

    void syncHandler_rejectsViaValidation() {
        clear_provider();
        ToolDef t;
        t.name = "sync_validates";
        t.input_schema.required = {"required_field"};
        t.input_schema.properties = QJsonObject{
            {"required_field", QJsonObject{{"type", "string"}}}};
        t.handler = [](const QJsonObject&) { return ToolResult::ok("never reached"); };
        McpProvider::instance().register_tool(std::move(t));

        auto fut = McpProvider::instance().call_tool_async("sync_validates", {});
        fut.waitForFinished();
        const auto r = fut.result();
        QVERIFY(!r.success);
        QVERIFY(r.error.contains("required_field"));
    }

    // ── Async handler resolves via callback ─────────────────────────────

    void asyncHandler_resolvesEventually() {
        clear_provider();
        ToolDef t;
        t.name = "async_ok";
        t.async_handler = [](const QJsonObject&, ToolContext,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QTimer::singleShot(20, [promise]() {
                promise->addResult(ToolResult::ok("done"));
                promise->finish();
            });
        };
        McpProvider::instance().register_tool(std::move(t));

        auto fut = McpProvider::instance().call_tool_async("async_ok", {});
        QVERIFY(QTest::qWaitFor([&] { return fut.isFinished(); }, 1000));
        QVERIFY(fut.resultCount() > 0);
        QVERIFY(fut.result().success);
    }

    // ── Timeout fires when handler stalls ────────────────────────────────

    void asyncHandler_timesOut() {
        clear_provider();
        ToolDef t;
        t.name = "async_stalls";
        t.default_timeout_ms = 100;  // tight timeout
        t.async_handler = [](const QJsonObject&, ToolContext,
                              std::shared_ptr<QPromise<ToolResult>>) {
            // Never resolves — provider's timeout should kick in.
        };
        McpProvider::instance().register_tool(std::move(t));

        auto fut = McpProvider::instance().call_tool_async("async_stalls", {});
        QVERIFY(QTest::qWaitFor([&] { return fut.isFinished(); }, 2000));
        const auto r = fut.result();
        QVERIFY(!r.success);
        QVERIFY(r.error.contains("timed out"));
    }

    // ── Cancellation observed via ctx.is_cancelled ──────────────────────

    void asyncHandler_observesCancellation() {
        clear_provider();
        ToolDef t;
        t.name = "async_cancellable";
        t.default_timeout_ms = 200; // forces is_cancelled to flip when timeout fires
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            // Poll for cancellation, then resolve gracefully when set.
            auto* poll = new QTimer;
            poll->setInterval(20);
            QObject::connect(poll, &QTimer::timeout, poll, [poll, ctx, promise]() {
                if (ctx.cancelled()) {
                    if (!promise->future().isFinished()) {
                        promise->addResult(ToolResult::fail("cancelled by handler"));
                        promise->finish();
                    }
                    poll->deleteLater();
                }
            });
            poll->start();
        };
        McpProvider::instance().register_tool(std::move(t));

        auto fut = McpProvider::instance().call_tool_async("async_cancellable", {});
        QVERIFY(QTest::qWaitFor([&] { return fut.isFinished(); }, 2000));
        const auto r = fut.result();
        QVERIFY(!r.success);
        // Either the watchdog OR the handler's own check produced the failure;
        // both are acceptable as long as something resolves the promise.
        QVERIFY(r.error.contains("timed out") || r.error.contains("cancelled"));
    }

    // ── Tool-not-found short-circuits ────────────────────────────────────

    void unknownTool_failsImmediately() {
        clear_provider();
        auto fut = McpProvider::instance().call_tool_async("does_not_exist", {});
        QVERIFY(fut.isFinished());
        QVERIFY(!fut.result().success);
        QVERIFY(fut.result().error.contains("not found"));
    }

    // ── Both sync and async work via blocking call_tool ─────────────────

    void blockingCallTool_sync() {
        clear_provider();
        ToolDef t;
        t.name = "block_sync";
        t.handler = [](const QJsonObject&) { return ToolResult::ok("sync"); };
        McpProvider::instance().register_tool(std::move(t));

        auto r = McpProvider::instance().call_tool("block_sync", {});
        QCOMPARE(r.message, QString("sync"));
    }

    void blockingCallTool_async() {
        clear_provider();
        ToolDef t;
        t.name = "block_async";
        t.async_handler = [](const QJsonObject&, ToolContext,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QTimer::singleShot(20, [promise]() {
                promise->addResult(ToolResult::ok("async-via-blocking"));
                promise->finish();
            });
        };
        McpProvider::instance().register_tool(std::move(t));

        auto r = McpProvider::instance().call_tool("block_async", {});
        QCOMPARE(r.message, QString("async-via-blocking"));
    }
};

QTEST_MAIN(TestAsyncDispatch)
#include "test_async.moc"
