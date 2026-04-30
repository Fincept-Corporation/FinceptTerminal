#pragma once
// AsyncDispatch.h — Helpers for bridging async-callback APIs into AsyncToolHandler.
//
// Phase 4 of the MCP refactor (see plans/mcp-refactor-phase-4-async-execution.md).
//
// Most Fincept services expose their work as `void fn(args, callback)` —
// PythonRunner::run, MarketDataService::fetch_quotes, NewsService::fetch_*,
// HttpClient calls, and so on. They all share the same shape:
//
//   service->run(args, [callback](Result r) {
//       // ...
//   });
//
// Pre-Phase-4, every MCP tool that wrapped one of these had to spin its
// own QMutex + QWaitCondition + QMetaObject::invokeMethod ritual to bridge
// the worker-thread → service-thread → worker-thread hop synchronously
// (see ThreadHelper.h::run_async_wait — replicated 8+ times). The result
// blocked the worker thread for the entire call.
//
// This helper consolidates the pattern into a single QPromise-based
// utility so async handlers fulfil their promise directly when the
// service callback fires — no thread blocking, no event loops.
//
// Usage in an AsyncToolHandler:
//
//   t.async_handler = [](const QJsonObject& args, ToolContext ctx,
//                        std::shared_ptr<QPromise<ToolResult>> promise) {
//       auto* runner = &python::PythonRunner::instance();
//       AsyncDispatch::callback_to_promise(
//           runner, ctx, promise,
//           [args, ctx](auto resolve) {
//               python::PythonRunner::instance().run(
//                   args["script"].toString(), {},
//                   [resolve, ctx](python::PythonResult r) {
//                       if (ctx.cancelled()) {
//                           resolve(ToolResult::fail("cancelled"));
//                           return;
//                       }
//                       resolve(r.success ? ToolResult::ok_data(r.output) : ToolResult::fail(r.error));
//                   });
//           });
//   };
//
// The provider has already armed the timeout watchdog; if the callback
// never fires the promise is resolved with a timeout error. The handler
// only needs to bridge service-callback → resolve(ToolResult).

#include "mcp/McpTypes.h"

#include <QFuture>
#include <QMetaObject>
#include <QPromise>
#include <QThread>

#include <functional>
#include <memory>

namespace fincept::mcp {

class AsyncDispatch {
  public:
    /// Bridge an async-callback API to a QPromise<ToolResult>.
    ///
    /// `target`     — the QObject whose thread the body runs on (e.g. the
    ///                service singleton). If null, body runs synchronously
    ///                on the calling thread.
    /// `ctx`        — passed through unchanged; handlers can read
    ///                ctx.is_cancelled() while waiting on the service.
    /// `promise`    — fulfilled by the body when its callback fires.
    /// `body`       — invoked on `target`'s thread. Receives a `resolve`
    ///                callable; pass it to the service callback so the
    ///                promise resolves when the service finishes.
    ///
    /// Re-entrancy: `resolve` is no-op after the first call. Useful when
    /// the timeout watchdog races with the service callback.
    template <typename BodyFn>
    static void callback_to_promise(QObject* target, ToolContext /*ctx*/,
                                     std::shared_ptr<QPromise<ToolResult>> promise,
                                     BodyFn&& body) {
        // Wrap resolve so it's idempotent — the timeout watchdog and the
        // service callback can race; whichever fires first wins.
        auto resolve = [promise](ToolResult r) {
            if (promise->future().isFinished())
                return;
            promise->addResult(std::move(r));
            promise->finish();
        };

        if (!target || QThread::currentThread() == target->thread()) {
            std::forward<BodyFn>(body)(resolve);
            return;
        }

        QMetaObject::invokeMethod(
            target,
            [body = std::forward<BodyFn>(body), resolve]() mutable { body(resolve); },
            Qt::QueuedConnection);
    }
};

} // namespace fincept::mcp
