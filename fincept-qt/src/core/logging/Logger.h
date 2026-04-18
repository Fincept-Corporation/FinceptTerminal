#pragma once
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>

#include <atomic>
#include <utility>

namespace fincept {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5 };

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

    // Cheap level check used by the LOG_* macros to avoid evaluating args
    // when the level is filtered out (P2.11).
    bool is_enabled(LogLevel level, const QString& tag) const;

    void trace(const QString& tag, const QString& msg);
    void debug(const QString& tag, const QString& msg);
    void info(const QString& tag, const QString& msg);
    void warn(const QString& tag, const QString& msg);
    void error(const QString& tag, const QString& msg);
    void fatal(const QString& tag, const QString& msg);

    // Called automatically via qAddPostRoutine at startup; safe to call manually.
    void flush_and_close();

  private:
    Logger() = default;
    void write(LogLevel level, const QString& tag, const QString& msg);
    void rotate_if_needed_locked(); // mutex_ must already be held

    std::atomic<LogLevel> min_level_{LogLevel::Info};
    std::atomic<bool> json_mode_{false};
    std::atomic<bool> degraded_{false}; // set after a write failure so we only warn once

    QHash<QString, LogLevel> tag_levels_;
    QFile log_file_;
    qint64 bytes_written_{0};
    QMutex mutex_;
};

} // namespace fincept

// ── LOG_* macros — lazy evaluation via is_enabled() (P2.11) ────────────────
// Existing call sites LOG_INFO("Tag", some_qstring) keep working unchanged.
// If the level is filtered out, `msg` is never evaluated, so callers can pass
// `QString("...").arg(expensive())` without paying for it in production.
#define FINCEPT_LOG_IMPL(level_enum, level_fn, tag, msg)                                                     \
    do {                                                                                                    \
        if (fincept::Logger::instance().is_enabled(fincept::LogLevel::level_enum, tag))                     \
            fincept::Logger::instance().level_fn(tag, msg);                                                  \
    } while (0)

#define LOG_TRACE(tag, msg) FINCEPT_LOG_IMPL(Trace, trace, tag, msg)
#define LOG_DEBUG(tag, msg) FINCEPT_LOG_IMPL(Debug, debug, tag, msg)
#define LOG_INFO(tag, msg)  FINCEPT_LOG_IMPL(Info,  info,  tag, msg)
#define LOG_WARN(tag, msg)  FINCEPT_LOG_IMPL(Warn,  warn,  tag, msg)
#define LOG_ERROR(tag, msg) FINCEPT_LOG_IMPL(Error, error, tag, msg)

// LOG_FATAL writes the line, then aborts in debug builds (P3.20).
#ifndef NDEBUG
#    define LOG_FATAL(tag, msg)                                                                              \
        do {                                                                                                \
            fincept::Logger::instance().fatal(tag, msg);                                                    \
            fincept::Logger::instance().flush_and_close();                                                  \
            qFatal("FATAL [%s] %s", qUtf8Printable(QString(tag)), qUtf8Printable(QString(msg)));            \
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
inline QString log_fmt(const QString& fmt) { return fmt; }
template <typename A1>
inline QString log_fmt(const QString& fmt, A1&& a1) { return QString(fmt).arg(std::forward<A1>(a1)); }
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
    return QString(fmt).arg(std::forward<A1>(a1)).arg(std::forward<A2>(a2))
                       .arg(std::forward<A3>(a3)).arg(std::forward<A4>(a4));
}
template <typename A1, typename A2, typename A3, typename A4, typename A5>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5) {
    return QString(fmt).arg(std::forward<A1>(a1)).arg(std::forward<A2>(a2))
                       .arg(std::forward<A3>(a3)).arg(std::forward<A4>(a4))
                       .arg(std::forward<A5>(a5));
}
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline QString log_fmt(const QString& fmt, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6) {
    return QString(fmt).arg(std::forward<A1>(a1)).arg(std::forward<A2>(a2))
                       .arg(std::forward<A3>(a3)).arg(std::forward<A4>(a4))
                       .arg(std::forward<A5>(a5)).arg(std::forward<A6>(a6));
}
} // namespace fincept::detail

#define FINCEPT_LOG_F_IMPL(level_enum, level_fn, tag, fmt, ...)                                              \
    do {                                                                                                    \
        if (fincept::Logger::instance().is_enabled(fincept::LogLevel::level_enum, tag))                     \
            fincept::Logger::instance().level_fn(tag, fincept::detail::log_fmt(fmt, ##__VA_ARGS__));        \
    } while (0)

#define LOG_TRACE_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Trace, trace, tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Debug, debug, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO_F(tag, fmt, ...)  FINCEPT_LOG_F_IMPL(Info,  info,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN_F(tag, fmt, ...)  FINCEPT_LOG_F_IMPL(Warn,  warn,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR_F(tag, fmt, ...) FINCEPT_LOG_F_IMPL(Error, error, tag, fmt, ##__VA_ARGS__)
