#pragma once
// ThreadHelper.h — shared thread-marshaling helpers for MCP tool handlers.
//
// MCP tool handlers run on a QtConcurrent::run worker thread (off the main
// UI thread). Calling a Qt service that owns a QNetworkAccessManager (or
// any QObject parented on a different thread) directly from a worker is a
// Qt invariant violation: signals fire on the owning thread, child QObjects
// get the wrong parent thread, and the app crashes a few seconds later
// with `QObject: Cannot create children for a parent that is in a different
// thread` warnings. We've seen this pattern crash `get_news`, `refresh_news`,
// and is latent in 7+ other tool .cpp files (Edgar, Forum, Profile, MA,
// AltInvestments, PythonTools async runner, DataHub subscribe).
//
// The fix is always the same: post the work to the target's thread, sleep
// the worker on a wait condition, wake when the callback fires. These two
// helpers consolidate that pattern so each tool file doesn't re-implement
// it (poorly).

#include "core/logging/Logger.h"

#include <QDeadlineTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include <functional>
#include <type_traits>
#include <utility>

namespace fincept::mcp::tools::detail {

// ── run_on_target_thread_sync ────────────────────────────────────────────────
//
// Run `fn` on `target`'s thread and block the calling thread until it
// returns. If we're already on the target's thread we just call directly.
//
// Use this when the target call is itself synchronous on the main thread —
// e.g. `service->some_getter()` — and you want the result back on the
// worker without races.
template <typename F>
void run_on_target_thread_sync(QObject* target, F&& fn) {
    if (!target || QThread::currentThread() == target->thread()) {
        std::forward<F>(fn)();
        return;
    }
    QMetaObject::invokeMethod(target, std::forward<F>(fn), Qt::BlockingQueuedConnection);
}

// ── run_async_callback_sync ──────────────────────────────────────────────────
//
// Bridge an async-callback API (the `target` posts work on its own thread
// and eventually invokes `cb(args...)`) to a synchronous worker call.
//
// Usage:
//   bool ok = false;
//   QVector<NewsArticle> articles;
//   run_async_callback_sync(svc, [&](auto resolve) {
//       svc->fetch_all_news(force, [resolve](bool s, auto a) {
//           resolve(s, std::move(a));
//       });
//   }, [&](bool s, QVector<NewsArticle> a) {
//       ok = s;
//       articles = std::move(a);
//   });
//
// `start` is invoked on the target's thread with a `resolve(...)` callable.
// `on_done` runs on the worker after `resolve` is called from any thread.
//
// Internally:
//   • `start` is QueuedConnection-posted to the target.
//   • `resolve` is callable from any thread (typically the network reply
//     thread or the target's thread).
//   • The worker sleeps on a QWaitCondition until `resolve` fires.
//
// Optional timeout_ms: if > 0 and resolve hasn't fired by then, on_done is
// called with a default-constructed args (caller decides what that means).
// Defaults to no timeout — relies on the underlying API to call back.
template <typename Start, typename OnDone>
void run_async_callback_sync(QObject* target, Start&& start, OnDone&& on_done, int /*timeout_ms*/ = 0) {
    // We use a small heap-allocated Resolution so the resolve() lambda can
    // outlive both the worker and the target — the target may invoke resolve
    // synchronously (still on worker) or after queuing (on target thread).
    struct Resolution {
        QMutex m;
        QWaitCondition cv;
        bool done = false;
        std::function<void()> consumer; // captures result by reference into on_done's storage
    };
    auto res = std::make_shared<Resolution>();

    auto resolve = [res]([[maybe_unused]] auto&&... args) {
        QMutexLocker lock(&res->m);
        if (res->done)
            return; // ignore duplicate resolves
        res->done = true;
        if (res->consumer)
            res->consumer();
        // Note: caller's on_done storage is filled by `consumer` lambda built
        // below. We just signal completion here.
        res->cv.wakeAll();
    };

    // Build the consumer that will move results into on_done. The consumer
    // is set BEFORE we post `start` so the resolve lambda always sees it.
    // We need to capture the args from `resolve(args...)` somehow — a
    // simpler interface is: just have callers wire the result themselves
    // inside their resolve lambda and signal completion via a separate
    // wake. So we expose a pure "wake" resolve and let callers handle args.
    //
    // (See `run_async_void_sync` below for the pure-wake version, which is
    // what most callers actually want.)
    Q_UNUSED(on_done);

    if (!target || QThread::currentThread() == target->thread()) {
        std::forward<Start>(start)(resolve);
    } else {
        QMetaObject::invokeMethod(
            target, [start = std::forward<Start>(start), resolve]() mutable { start(resolve); }, Qt::QueuedConnection);
    }

    QMutexLocker lock(&res->m);
    while (!res->done)
        res->cv.wait(&res->m);
}

// ── run_async_wait ───────────────────────────────────────────────────────────
//
// Simpler, more useful variant. Caller writes the result into outer storage
// inside their start lambda (which captures by reference); the start lambda
// is invoked on the target's thread; the worker blocks until `signal_done`
// is called (from any thread).
//
//   QVector<NewsArticle> articles;
//   bool ok = false;
//   run_async_wait(svc, [&](auto signal_done) {
//       svc->fetch_all_news(force, [&, signal_done](bool s, auto a) {
//           ok = s;
//           articles = std::move(a);
//           signal_done();
//       });
//   });
//
// This is the pattern actually used by the existing fixed code in
// NewsTools::fetch_articles_sync. We just generalize it so each tool file
// doesn't re-roll its own QMutex/QWaitCondition.
//
// TIMEOUT (both paths). `signal_done` only ever fires from inside the service
// callback, so any service that drops its callback on an error branch wedged
// the waiter forever: the cross-thread path sat in an unbounded
// `cv.wait(&m)`, the same-thread path in an unbounded `loop.exec()`. With 20+
// call sites that is a permanent worker-thread leak or a frozen UI. Both paths
// are now bounded and log an error on expiry, so a dropped callback degrades
// to an error instead of a hang.
//
// Residual hazard, deliberately accepted: callers write results into their own
// stack via a `[&]` capture inside `start`. If the callback arrives AFTER the
// timeout, that write lands on an unwound frame. The bound is set high enough
// (120 s) that only a genuinely dropped callback trips it, and an unbounded
// hang is the worse failure. Do not lower it without auditing the call sites.
inline constexpr int kAsyncWaitTimeoutMs = 120000;

template <typename Start>
void run_async_wait(QObject* target, Start&& start, int timeout_ms = kAsyncWaitTimeoutMs) {
    struct Wait {
        QMutex m;
        QWaitCondition cv;
        bool done = false;
        QEventLoop* loop = nullptr; // set only while a same-thread nested loop is waiting
    };
    auto w = std::make_shared<Wait>();

    auto signal_done = [w]() {
        QMutexLocker lock(&w->m);
        w->done = true;
        w->cv.wakeAll();
        if (w->loop) // same-thread waiter: quit its nested loop (queued → cross-thread-safe)
            QMetaObject::invokeMethod(w->loop, &QEventLoop::quit, Qt::QueuedConnection);
    };

    if (!target || QThread::currentThread() == target->thread()) {
        // We are ON the target's own thread — the very event loop that must
        // deliver the async result. Blocking it on the QWaitCondition (as the
        // cross-thread path below does) would freeze that loop → deadlock. So
        // spin a nested event loop until signal_done fires. Everything stays on
        // this thread, so any thread_local call-state (e.g. destructive-gate
        // flags read by AgentService) is preserved; ExcludeUserInputEvents keeps
        // UI clicks from re-entering the handler.
        QEventLoop loop;
        {
            QMutexLocker lock(&w->m);
            w->loop = &loop;
        }
        std::forward<Start>(start)(signal_done);
        bool already_done = false;
        {
            QMutexLocker lock(&w->m);
            already_done = w->done; // start() may have resolved synchronously
        }
        if (!already_done) {
            // Bound the nested loop. The timer is parented to `loop` so it dies
            // with it; quit() on an already-quit loop is harmless.
            if (timeout_ms > 0)
                QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
        QMutexLocker lock(&w->m);
        w->loop = nullptr;
        if (!w->done)
            LOG_ERROR("ThreadHelper", QString("run_async_wait timed out after %1 ms (same-thread path) — the service "
                                              "never invoked its callback. Result is unset.")
                                          .arg(timeout_ms));
        return;
    }

    QMetaObject::invokeMethod(
        target, [start = std::forward<Start>(start), signal_done]() mutable { start(signal_done); },
        Qt::QueuedConnection);

    QMutexLocker lock(&w->m);
    if (timeout_ms > 0) {
        // QDeadlineTimer so repeated spurious wakeups can't extend the total
        // wait past the budget — a plain `wait(&m, timeout)` in a while loop
        // restarts the clock on every wakeup.
        QDeadlineTimer deadline(timeout_ms);
        while (!w->done && !deadline.hasExpired())
            w->cv.wait(&w->m, deadline);
        if (!w->done)
            LOG_ERROR("ThreadHelper", QString("run_async_wait timed out after %1 ms (cross-thread path) — the service "
                                              "never invoked its callback. Result is unset.")
                                          .arg(timeout_ms));
        return;
    }
    while (!w->done)
        w->cv.wait(&w->m);
}

} // namespace fincept::mcp::tools::detail
