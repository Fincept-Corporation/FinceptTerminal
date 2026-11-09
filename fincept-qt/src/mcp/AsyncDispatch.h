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
#include <QRegularExpression>
#include <QString>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>

namespace fincept::mcp {

/// Ceiling on a progress message. The snapshot it lands in is polled
/// repeatedly, so an unbounded line is re-billed on every poll.
inline constexpr int kProgressMessageMaxChars = 160;

class AsyncDispatch {
  public:
    /// Bridge a line-oriented stdout/stderr stream into `ctx.on_progress`, for
    /// services that expose one (PythonRunner::run's `on_line`).
    ///
    /// Returns a callback with PythonRunner's StreamCallback shape, or an empty
    /// one when `ctx` has no progress hook — so a caller can pass the result
    /// straight through without branching.
    ///
    /// What this deliberately does NOT do is invent a percentage. Job status once
    /// reported `progress: 0.0` for every job in the catalogue because no
    /// handler ever called `on_progress`, and the obvious repair — synthesising
    /// elapsed/timeout as a completion ratio — would have replaced "no
    /// information" with "confident misinformation": a 3 s script at a 300 s
    /// budget is not 1% done. So the numeric field moves ONLY when the script
    /// says so, by printing a line carrying `"progress": <0..1>` (or a bare
    /// `progress: 42%`). Everything else becomes the human-readable `message`,
    /// which is what actually distinguishes a working job from a wedged one.
    static std::function<void(QString, bool)> line_progress_bridge(const ToolContext& ctx) {
        if (!ctx.on_progress)
            return {};
        auto on_progress = ctx.on_progress;
        return [on_progress](QString line, bool /*is_stderr*/) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
                return;
            // Cap the message: this lands in a snapshot the model polls
            // repeatedly, so a stray 4 KB traceback line would be re-billed on
            // every poll for the life of the job.
            on_progress(parse_progress_fraction(trimmed), trimmed.left(kProgressMessageMaxChars));
        };
    }

    /// Extract a machine-readable completion fraction from a status line, or 0
    /// when the line carries none. Accepts `"progress": 0.42`, `progress=0.42`
    /// and `progress: 42%`; anything else leaves the number untouched (0 is the
    /// registry's "no numeric progress reported" sentinel).
    static double parse_progress_fraction(const QString& line) {
        static const QRegularExpression rx(
            QStringLiteral(R"(\bprogress"?\s*[:=]\s*"?(\d+(?:\.\d+)?)\s*(%?))"),
            QRegularExpression::CaseInsensitiveOption);
        const auto m = rx.match(line);
        if (!m.hasMatch())
            return 0.0;
        bool ok = false;
        double v = m.captured(1).toDouble(&ok);
        if (!ok)
            return 0.0;
        if (m.captured(2) == QLatin1String("%") || v > 1.0)
            v /= 100.0;
        return std::clamp(v, 0.0, 1.0);
    }

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
    static void callback_to_promise(QObject* target, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise,
                                    BodyFn&& body) {
        // Wrap resolve so it's idempotent — the timeout watchdog, the
        // cancellation watch and the service callback can all race; whichever
        // fires first wins.
        //
        // The guard must be a compare-exchange, not a `future().isFinished()`
        // test: the watchdog fires on the GUI thread while the service callback
        // fires on its own thread, so both can observe "not finished" and then
        // both addResult() — the loser writing after finish(), which Qt asserts
        // on. Only the thread that flips the flag proceeds.
        //
        // Critically, the flag must be the SAME one the provider's watchdog
        // races. Minting a private one here made this look guarded while the
        // only contenders it actually excluded were each other's: the provider
        // let its winner through, this let its winner through, and both wrote.
        // `ctx.resolve_guard` carries the provider's flag; the fallback is for
        // a hand-built context in a test or a direct caller.
        auto resolved = ctx.resolve_guard ? ctx.resolve_guard : std::make_shared<std::atomic<bool>>(false);
        auto resolve = [promise, resolved](ToolResult r) {
            bool expected = false;
            if (!resolved->compare_exchange_strong(expected, true))
                return;
            promise->addResult(std::move(r));
            promise->finish();
        };

        if (!target || QThread::currentThread() == target->thread()) {
            std::forward<BodyFn>(body)(resolve);
            return;
        }

        QMetaObject::invokeMethod(
            target, [body = std::forward<BodyFn>(body), resolve]() mutable { body(resolve); }, Qt::QueuedConnection);
    }
};

} // namespace fincept::mcp
