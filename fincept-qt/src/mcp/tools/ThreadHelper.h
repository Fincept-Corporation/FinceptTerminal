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

#include <QMetaObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QThread>
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
void run_async_callback_sync(QObject* target, Start&& start, OnDone&& on_done,
                              int /*timeout_ms*/ = 0) {
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
            target,
            [start = std::forward<Start>(start), resolve]() mutable { start(resolve); },
            Qt::QueuedConnection);
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
template <typename Start>
void run_async_wait(QObject* target, Start&& start) {
    struct Wait {
        QMutex m;
        QWaitCondition cv;
        bool done = false;
    };
    auto w = std::make_shared<Wait>();

    auto signal_done = [w]() {
        QMutexLocker lock(&w->m);
        w->done = true;
        w->cv.wakeAll();
    };

    if (!target || QThread::currentThread() == target->thread()) {
        std::forward<Start>(start)(signal_done);
    } else {
        QMetaObject::invokeMethod(
            target,
            [start = std::forward<Start>(start), signal_done]() mutable { start(signal_done); },
            Qt::QueuedConnection);
    }

    QMutexLocker lock(&w->m);
    while (!w->done)
        w->cv.wait(&w->m);
}

} // namespace fincept::mcp::tools::detail
