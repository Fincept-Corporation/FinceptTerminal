#pragma once
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>

#include <atomic>
#include <memory>
#include <utility>

namespace fincept {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5 };

/// Process-wide logger.
///
/// Hot path (every LOG_* call, from any thread):
///   - `should_log()` is lock-free in the common case: one relaxed atomic load
///     rejects a level that no global/tag setting enables, without building a
///     QString for the tag. Per-tag overrides are consulted (under a read lock)
///     only when at least one exists.
///   - Formatting (timestamp, JSON/text line) happens on the calling thread with
///     no lock held; the finished line is appended to an in-memory queue under a
///     short mutex.
///
/// File I/O is done by a dedicated writer thread that flushes queued lines at
/// most every ~250 ms, or sooner once a few KB are queued. WARN / ERROR / FATAL
/// lines are written AND flushed to the OS synchronously by the calling thread
/// (together with every line queued before them, so file order is preserved):
/// they survive a crash that follows them immediately. A crash can lose at most
/// the last ~250 ms of TRACE..INFO lines.
///
/// flush_and_close() drains every queued line synchronously before closing — it
/// is called by the Qt post-routine at QApplication teardown, by the QtFatalMsg
/// handler in main.cpp, by LOG_FATAL (debug builds) and from an atexit() hook,
/// so a normal shutdown is lossless.
class Logger {
  public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_file(const QString& path);
    void set_tag_level(const QString& tag, LogLevel level);
    void clear_tag_level(const QString& tag);
    void clear_all_tag_levels();

    // JSON output mode — one JSON object per line when enabled (P3.17).
    void set_json_mode(bool enabled);
    bool json_mode() const;

    LogLevel global_level() const { return min_level_.load(std::memory_order_relaxed); }
    QHash<QString, LogLevel> tag_levels() const;

    // Level check honouring per-tag overrides (P2.11). Lock-free unless at least
    // one tag override is configured.
    bool is_enabled(LogLevel level, const QString& tag) const;

    /// Fast pre-check used by the LOG_* macros. Takes the tag in whatever form
    /// the call site has it (usually a `const char*` literal) and only converts
    /// it to a QString when a tag override actually has to be looked up — the
    /// common "level is off" and "no overrides" cases never allocate.
    template <typename Tag>
    bool should_log(LogLevel level, const Tag& tag) const {
        const int lv = static_cast<int>(level);
        if (lv < lowest_enabled_.load(std::memory_order_relaxed))
            return false; // below every global/tag threshold — off for every tag
        if (!has_tag_overrides_.load(std::memory_order_acquire))
            return lv >= static_cast<int>(min_level_.load(std::memory_order_relaxed));
        return is_enabled(level, tag);
    }

    void trace(const QString& tag, const QString& msg);
    void debug(const QString& tag, const QString& msg);
    void info(const QString& tag, const QString& msg);
    void warn(const QString& tag, const QString& msg);
    void error(const QString& tag, const QString& msg);
    void fatal(const QString& tag, const QString& msg);

    // Drains every queued line to disk, stops the writer thread and closes the
    // file. Called automatically via qAddPostRoutine and atexit(); safe to call
    // manually and more than once. Lines logged afterwards are dropped (as they
    // always were once the file was closed) until set_file() is called again.
    void flush_and_close();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

  private:
    Logger();
    ~Logger();

    struct State; // queue, writer thread, file, tag overrides — Logger.cpp

    void write(LogLevel level, const QString& tag, const QString& msg);
    void recompute_thresholds_locked(); // State::tag_lock must be held for writing

    std::atomic<LogLevel> min_level_{LogLevel::Info};
    // min(global level, every tag override) — anything below it is off for all tags.
    std::atomic<int> lowest_enabled_{static_cast<int>(LogLevel::Info)};
    std::atomic<bool> has_tag_overrides_{false};
    std::atomic<bool> json_mode_{false};

    std::unique_ptr<State> s_;
};

} // namespace fincept

// ── LOG_* macros — lazy evaluation via should_log() (P2.11) ────────────────
// Existing call sites LOG_INFO("Tag", some_qstring) keep working unchanged.
// If the level is filtered out, `msg` is never evaluated, so callers can pass
// `QString("...").arg(expensive())` without paying for it in production.
#define FINCEPT_LOG_IMPL(level_enum, level_fn, tag, msg)                                                               \
    do {                                                                                                               \
        if (fincept::Logger::instance().should_log(fincept::LogLevel::level_enum, tag))                                \
            fincept::Logger::instance().level_fn(tag, msg);                                                            \
    } while (0)

#define LOG_TRACE(tag, msg) FINCEPT_LOG_IMPL(Trace, trace, tag, msg)
#define LOG_DEBUG(tag, msg) FINCEPT_LOG_IMPL(Debug, debug, tag, msg)
#define LOG_INFO(tag, msg) FINCEPT_LOG_IMPL(Info, info, tag, msg)
#define LOG_WARN(tag, msg) FINCEPT_LOG_IMPL(Warn, warn, tag, msg)
#define LOG_ERROR(tag, msg) FINCEPT_LOG_IMPL(Error, error, tag, msg)

// LOG_FATAL writes the line, then aborts in debug builds (P3.20).
#ifndef NDEBUG
#    define LOG_FATAL(tag, msg)                                                                                        \
        do {                                                                                                           \
            fincept::Logger::instance().fatal(tag, msg);                                                               \
            fincept::Logger::instance().flush_and_close();                                                             \
            qFatal("FATAL [%s] %s", qUtf8Printable(QString(tag)), qUtf8Printable(QString(msg)));                       \
        } while (0)
#else
#    define LOG_FATAL(tag, msg) FINCEPT_LOG_IMPL(Fatal, fatal, tag, msg)
#endif

// ── LOG_*_F variadic helpers (P2.12) ────────────────────────────────────────
// Format arguments are only evaluated when the level is enabled, so expensive
// QString building is skipped when the call is filtered out.
//
// Usage:
//   LOG_INFO_F("Net", "GET %1 -> %2", url, status);
//
// The format helper expands QString(fmt).arg(a).arg(b)... for up to 6 args.
namespace fincept::detail {
inline QString log_fmt(const QString& fmt) {
    return fmt;
}
template <typename A1>
inline QString log_fmt(const QString& fmt, A1&& a1) {
    return QString(fmt).arg(std::forward<A1>(a1));
}
template <typename A1, typename A2>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2) {
    return QString(fmt).arg(std::forward<A1>(a1)).arg(std::forward<A2>(a2));
}
template <typename A1, typename A2, typename A3>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3) {
    return QString(fmt).arg(std::forward<A1>(a1)).arg(std::forward<A2>(a2)).arg(std::forward<A3>(a3));
}
template <typename A1, typename A2, typename A3, typename A4>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3, A4&& a4) {
    return QString(fmt)
        .arg(std::forward<A1>(a1))
        .arg(std::forward<A2>(a2))
        .arg(std::forward<A3>(a3))
        .arg(std::forward<A4>(a4));
}
template <typename A1, typename A2, typename A3, typename A4, typename A5>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5) {
    return QString(fmt)
        .arg(std::forward<A1>(a1))
        .arg(std::forward<A2>(a2))
        .arg(std::forward<A3>(a3))
        .arg(std::forward<A4>(a4))
        .arg(std::forward<A5>(a5));
}
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6) {
    return QString(fmt)
        .arg(std::forward<A1>(a1))
        .arg(std::forward<A2>(a2))
        .arg(std::forward<A3>(a3))
        .arg(std::forward<A4>(a4))
        .arg(std::forward<A5>(a5))
        .arg(std::forward<A6>(a6));
}
} // namespace fincept::detail

#define FINCEPT_LOG_F_IMPL(level_enum, level_fn, tag, fmt, ...)                                                        \
    do {                                                                                                               \
        if (fincept::Logger::instance().should_log(fincept::LogLevel::level_enum, tag))                                \
            fincept::Logger::instance().level_fn(tag, fincept::detail::log_fmt(fmt, ##__VA_ARGS__));                   \
    } while (0)

#define LOG_TRACE_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Trace, trace, tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Debug, debug, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Info, info, tag, fmt, ##__VA_ARGS__)
#define LOG_WARN_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Warn, warn, tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Error, error, tag, fmt, ##__VA_ARGS__)
