#pragma once
// CoreToolsCommon.h — small helpers shared by the core tool implementations.
// Header-only; every helper lives in `fincept::mcp::core::common` so nothing
// collides with file-local names in a unity batch.

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
/// immediately. Timer-driven — never blocks or spins the event loop.
inline void poll_until(int interval_ms, int timeout_ms, std::function<bool()> check, std::function<void(bool)> done) {
    if (check()) {
        done(true);
        return;
    }
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto* timer = new QTimer(QCoreApplication::instance());
    timer->setInterval(std::max(10, interval_ms));
    QObject::connect(timer, &QTimer::timeout, timer, [timer, clock, timeout_ms, check, done]() {
        const bool ok = check();
        if (!ok && clock->elapsed() < timeout_ms)
            return;
        timer->stop();
        timer->deleteLater();
        done(ok);
    });
    timer->start();
}

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
