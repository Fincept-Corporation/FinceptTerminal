#pragma once
// CoreToolsCommon.h — small helpers shared by the core tool implementations.
// Header-only; every helper lives in `fincept::mcp::core::common` so nothing
// collides with file-local names in a unity batch.

#include "core/logging/Logger.h"
#include "mcp/McpTypes.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPromise>
#include <QString>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>

namespace fincept::mcp::core::common {

/// Resolver bound to the provider's single-winner guard (ToolContext::
/// resolve_guard). Returns true when THIS call finished the promise — callers
/// that must know whether they won (e.g. to leave a job result uncollected
/// when a receipt already went out) test the return value.
using Resolver = std::function<bool(ToolResult)>;

inline Resolver make_resolver(const ToolContext& ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
    auto guard = ctx.resolve_guard ? ctx.resolve_guard : std::make_shared<std::atomic<bool>>(false);
    return [promise, guard](ToolResult r) -> bool {
        bool expected = false;
        if (!guard->compare_exchange_strong(expected, true))
            return false;
        promise->addResult(std::move(r));
        promise->finish();
        return true;
    };
}

/// Run `fn`, swallowing (and logging) anything it throws: for callbacks the
/// event loop runs, where an escaping exception would end the process.
/// Returns false when it threw.
inline bool run_guarded(const char* where, const std::function<void()>& fn) {
    try {
        fn();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CoreTools", QStringLiteral("%1 threw: %2").arg(QString::fromLatin1(where), QString::fromUtf8(e.what())));
    } catch (...) {
        LOG_ERROR("CoreTools", QStringLiteral("%1 threw an unknown exception").arg(QString::fromLatin1(where)));
    }
    return false;
}

/// Run `fn` on the GUI thread: inline when already there, queued otherwise.
inline void on_gui_thread(std::function<void()> fn) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return;
    if (QThread::currentThread() == app->thread()) {
        fn();
        return;
    }
    QMetaObject::invokeMethod(app, std::move(fn), Qt::QueuedConnection);
}

/// GUI thread. Calls `check` every `interval_ms` until it returns true or
/// `timeout_ms` passes; then calls `done(satisfied)` once. `check` runs once
/// immediately. Timer-driven — never blocks or spins the event loop. A `check`
/// that throws ends the poll as unsatisfied; `done` always runs exactly once.
inline void poll_until(int interval_ms, int timeout_ms, std::function<bool()> check, std::function<void(bool)> done) {
    auto safe_check = [check]() {
        bool ok = false;
        bool threw = !run_guarded("poll check", [&]() { ok = check(); });
        return std::make_pair(ok, threw);
    };
    auto safe_done = [done](bool ok) { run_guarded("poll done", [&]() { done(ok); }); };
    const auto first = safe_check();
    if (first.first || first.second) {
        safe_done(first.first);
        return;
    }
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto* timer = new QTimer(QCoreApplication::instance());
    timer->setInterval(std::max(10, interval_ms));
    QObject::connect(timer, &QTimer::timeout, timer, [timer, clock, timeout_ms, safe_check, safe_done]() {
        const auto r = safe_check();
        if (!r.first && !r.second && clock->elapsed() < timeout_ms)
            return;
        timer->stop();
        timer->deleteLater();
        safe_done(r.first);
    });
    timer->start();
}

/// While an agent's action runs, file / colour / font dialogs it opens are Qt's
/// widget dialogs instead of the platform's native ones: a native dialog is not
/// a widget, so an agent could neither see nor answer it and the user's window
/// would sit under a modal loop nobody can end. Reference-counted across
/// overlapping actions; the user's own setting is restored when the last one
/// ends. GUI thread only. The attribute is read when a dialog is shown, so
/// holding it from just before an action until it settles covers the dialog
/// the action opens (nothing else in that short window is affected beyond
/// using Qt's dialog).
class NonNativeDialogs {
  public:
    NonNativeDialogs() {
        if (count()++ == 0) {
            saved() = QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        }
    }
    ~NonNativeDialogs() { release(); }
    NonNativeDialogs(const NonNativeDialogs&) = delete;
    NonNativeDialogs& operator=(const NonNativeDialogs&) = delete;
    /// End this hold now (idempotent).
    void release() {
        if (released_)
            return;
        released_ = true;
        if (--count() == 0)
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, saved());
    }
    static int holders() { return count(); }

  private:
    static int& count() {
        static int n = 0;
        return n;
    }
    static bool& saved() {
        static bool v = false;
        return v;
    }
    bool released_ = false;
};

/// Whitespace-collapsed text clipped to `max_chars` with an ellipsis.
inline QString clip(const QString& text, int max_chars) {
    const QString flat = text.simplified();
    if (flat.size() <= max_chars)
        return flat;
    return flat.left(std::max(0, max_chars - 1)) + QChar(0x2026);
}

/// Quote a name for compact element lines: "Refresh" (inner quotes escaped).
inline QString quoted(const QString& text) {
    QString t = text;
    t.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + t + QLatin1Char('"');
}

} // namespace fincept::mcp::core::common
