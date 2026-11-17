#pragma once
// ShutdownReason — why this process is ending, for the shutdown log lines.
//
// Whoever starts a quit records the reason first (main.cpp: termination signal,
// update restart, last window closed, OS session end, a fatal Qt message); the
// lines that close a session read it:
//   "Shutting down reason=<r> [detail] uptime_s=<n>"   (main.cpp, start of aboutToQuit)
//   "Shutdown complete reason=<r> uptime_s=<n>"         (main.cpp, last post-routine)
// The FIRST reason recorded wins: a SIGTERM closes the windows, which then fires
// lastWindowClosed — the session still ended because of the signal. note_fatal()
// always wins. Header-only, thread-safe, no logging of its own.

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

namespace fincept::shutdown {

enum class Reason { Unknown, LastWindowClosed, Quit, Signal, Update, SessionEnd, StartupFailed, Fatal };

inline const char* reason_name(Reason r) {
    switch (r) {
        case Reason::LastWindowClosed:
            return "last_window_closed";
        case Reason::Quit:
            return "quit";
        case Reason::Signal:
            return "signal";
        case Reason::Update:
            return "update";
        case Reason::SessionEnd:
            return "session_end";
        case Reason::StartupFailed:
            return "startup_failed";
        case Reason::Fatal:
            return "fatal";
        case Reason::Unknown:
            break;
    }
    return "unknown";
}

namespace detail {
struct State {
    QMutex mutex;
    Reason reason = Reason::Unknown;
    QString detail; // e.g. "signal=SIGTERM"
};
inline State& state() {
    static State s;
    return s;
}
/// Wall-clock time of static initialisation (≈ process start).
inline const qint64 kProcessStartMs = QDateTime::currentMSecsSinceEpoch();
} // namespace detail

/// Record why the process ends. The first reason recorded is kept.
inline void note(Reason r, const QString& detail_kv = {}) {
    auto& s = detail::state();
    QMutexLocker lock(&s.mutex);
    if (s.reason != Reason::Unknown)
        return;
    s.reason = r;
    s.detail = detail_kv;
}

/// A fatal error overrides any earlier reason.
inline void note_fatal(const QString& detail_kv = {}) {
    auto& s = detail::state();
    QMutexLocker lock(&s.mutex);
    s.reason = Reason::Fatal;
    s.detail = detail_kv;
}

inline Reason reason() {
    auto& s = detail::state();
    QMutexLocker lock(&s.mutex);
    return s.reason;
}

inline qint64 uptime_s() {
    return (QDateTime::currentMSecsSinceEpoch() - detail::kProcessStartMs) / 1000;
}

/// "reason=signal signal=SIGTERM" — `fallback` names an unrecorded reason
/// (aboutToQuit without a recorded cause is an explicit quit).
inline QString describe(Reason fallback = Reason::Unknown) {
    auto& s = detail::state();
    QMutexLocker lock(&s.mutex);
    const Reason r = s.reason == Reason::Unknown ? fallback : s.reason;
    QString out = QStringLiteral("reason=") + QLatin1String(reason_name(r));
    if (!s.detail.isEmpty())
        out += QLatin1Char(' ') + s.detail;
    return out;
}

} // namespace fincept::shutdown
