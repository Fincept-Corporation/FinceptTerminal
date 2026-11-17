#pragma once
#include "core/logging/LogFields.h"
#include "core/logging/LogRedact.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <atomic>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>

namespace fincept {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5 };

/// What Logger::transition() knew about the state a key just left.
struct LogTransition {
    int previous_state = 0;
    int observations_in_previous = 0; // calls that reported the previous state (e.g. failed refreshes)
    qint64 ms_in_previous = 0;        // time the key spent in the previous state
};

/// A parsed level setting: "debug", "2", "info,DataHub=debug,HTTP:warn".
struct LogLevelSpec {
    std::optional<LogLevel> global;
    QHash<QString, LogLevel> tags;
    QStringList invalid; // items that were not understood
    bool isEmpty() const { return !global && tags.isEmpty(); }
};

/// Counters for a diagnostics view (whole session).
struct LogStats {
    quint64 dropped_lines = 0;      // queue full (disk stalled)
    quint64 lost_lines = 0;         // a write to fincept.log failed
    quint64 audit_lost_lines = 0;   // audit lines that did not reach audit.log (still in fincept.log unless dropped)
    quint64 repeats_suppressed = 0; // identical consecutive lines folded into "last message repeated"
    quint64 rate_limited = 0;       // LOG_*_RL lines counted instead of written
    int rate_limit_keys = 0;
    int transition_keys = 0;
    qsizetype queued_bytes = 0;
};

/// Process-wide logger.
///
/// Hot path (every LOG_* call, from any thread):
///   - `should_log()` is lock-free in the common case: one relaxed atomic load
///     rejects a level that no global/tag setting enables, without building a
///     QString for the tag. Per-tag overrides are consulted (under a read lock)
///     only when at least one exists. The message expression is not evaluated
///     when the level is off.
///   - Formatting (timestamp taken at the call, line built) happens on the
///     calling thread with no lock held; the finished line is appended to an
///     in-memory queue under a short mutex. No file I/O on the caller.
///
/// File I/O is done by one writer thread. TRACE..INFO reach the OS within ~250 ms
/// (sooner once 8 KB are queued); WARN, ERROR and AUDIT wake the writer at once.
/// FATAL, flush() and flush_and_close() drain synchronously on the caller.
///
/// Queue full (4 MB — only when the disk stalls): on the GUI thread nothing blocks —
/// the line is dropped and counted; from other threads WARN/ERROR wait up to 200 ms for
/// room, then are dropped and counted; TRACE..INFO are dropped at once. The next line
/// written is preceded by "Log lines dropped count=N reason=queue_full". FATAL and the
/// AUDIT mirror are never dropped.
///
/// Identical consecutive lines (same level, tag and text; any thread) are written once;
/// "last message repeated N times" follows when a different line arrives, after 5 s, or at
/// flush. AUDIT and FATAL lines are never folded.
///
/// Exit: flush_and_close() runs from a Qt post-routine (normal quit, and SIGTERM/SIGINT/
/// SIGHUP, which main.cpp turns into a normal quit) and from an atexit() hook registered
/// by set_file() (headless --smoke-test / --selftest-* returns). It is idempotent and safe
/// after static destruction has begun (the logger is never destroyed).
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

    /// Effective global level (an environment override wins over set_level()).
    LogLevel global_level() const { return min_level_.load(std::memory_order_relaxed); }
    /// Effective per-tag levels (settings overlaid by environment overrides).
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
    void fatal(const QString& tag, const QString& msg); // drains synchronously before returning
    /// Same as the level functions, with the level as a value (LOG_TRANSITION uses it).
    void log(LogLevel level, const QString& tag, const QString& msg);

    // ── Audit trail ─────────────────────────────────────────────────────────
    /// One AUDIT line in fincept.log and in audit.log. No level or tag setting can
    /// switch it off; never folded, never rate-limited. Use LOG_AUDIT.
    void audit(const QString& tag, const QString& msg);
    void audit(const QString& tag, const QString& sentence, std::initializer_list<LogField> fields);
    /// Where audit lines go. Default: audit.log next to the file given to set_file().
    void set_audit_file(const QString& path);
    QString audit_file() const;

    // ── Rate limits and state changes (use the LOG_*_RL / LOG_TRANSITION macros) ──
    /// True when a line for (tag, key) may be written now. The first call for a key passes;
    /// later calls within `interval_ms` of the last line written are counted and return false.
    /// When a line passes after such a burst, `*suppressed` is the count (the macros append
    /// " (suppressed=N)"). A burst that simply stops is reported by the writer thread within
    /// ~1 s after its interval ends: "Repeated lines suppressed key=… suppressed=N interval_ms=…".
    /// At most 1,024 keys (least recently used evicted, its pending count reported first).
    bool rate_limit_pass(const QString& tag, const QString& key, int interval_ms, int* suppressed,
                         LogLevel level = LogLevel::Info);

    /// True when the state stored for (tag, key) changed. State 0 means healthy: a key first
    /// seen healthy changes nothing, a key first seen in any other state is a change. Only
    /// non-zero states are stored (at most 1,024 keys, least recently used evicted — an evicted
    /// key that is still failing is reported again as a new failure). `info` (optional)
    /// describes the state that was left.
    bool transition(const QString& tag, const QString& key, int state, LogTransition* info = nullptr);
    /// Cheap pre-check: false when transition() cannot report a change (healthy and nothing stored).
    bool transition_possible(int state) const {
        return state != 0 || transition_keys_.load(std::memory_order_acquire) > 0;
    }

    // ── Levels: names, parsing, overrides ───────────────────────────────────
    /// "Trace", "Debug", "Info", "Warn", "Error", "Fatal" — the names Settings stores.
    static QString level_name(LogLevel level);
    /// Case-insensitive name (trace, debug, info, warn|warning, error|err, fatal|critical)
    /// or number 0-5.
    static std::optional<LogLevel> parse_level(QStringView text);
    /// Items separated by ',', ';' or whitespace: a level ("debug") sets the global level,
    /// "Tag=level" or "Tag:level" a tag. Unknown items are listed in `invalid`.
    static LogLevelSpec parse_level_spec(QStringView spec);
    /// Overrides that win over set_level()/set_tag_level() until cleared. The constructor
    /// applies FINCEPT_LOG_LEVEL (global and Tag=level items) and FINCEPT_LOG_TAGS (Tag:level
    /// items); main.cpp can apply a --log-level value the same way. Written to the log once
    /// (when the file opens, or at once if it is open).
    void apply_level_overrides(const LogLevelSpec& spec, const QString& source);
    void clear_level_overrides();
    /// "" when no override is active, else e.g. "level=Debug tags=DataHub:Trace (FINCEPT_LOG_LEVEL)".
    QString level_overrides() const;

    // ── Diagnostics ─────────────────────────────────────────────────────────
    /// The last `max_lines` WARN / ERROR / FATAL lines (oldest first, at most 200 are kept),
    /// each passed through log_scrub() — for a "Copy diagnostics (redacted)" action.
    QStringList recent_problems(int max_lines = 100) const;
    LogStats stats() const;

    /// Writes every queued line now, on the calling thread (blocks on disk I/O — exit, signal
    /// and crash paths only, never in normal GUI code). Emits a pending "last message
    /// repeated" line first.
    void flush();

    // Drains every queued line to disk, stops the writer thread and closes the
    // file. Called automatically via qAddPostRoutine and atexit(); safe to call
    // manually and more than once. Lines logged afterwards are dropped (as they
    // always were once the file was closed) until set_file() is called again.
    void flush_and_close();

    /// Tests only: hold the writer thread (lines stay queued) to exercise the full-queue policy.
    void set_writer_paused_for_testing(bool paused);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

  private:
    Logger();
    ~Logger();

    struct State; // queue, writer thread, files, tag overrides, tables — Logger.cpp

    void write(LogLevel level, const QString& tag, const QString& msg);
    void enqueue(int level, const QString& tag, const QString& msg, unsigned flags);
    void sweep_rate_limits(bool all);
    void announce_overrides();
    void recompute_thresholds_locked(); // State::tag_lock must be held for writing

    std::atomic<LogLevel> min_level_{LogLevel::Info};
    // min(global level, every tag override) — anything below it is off for all tags.
    std::atomic<int> lowest_enabled_{static_cast<int>(LogLevel::Info)};
    std::atomic<bool> has_tag_overrides_{false};
    std::atomic<bool> json_mode_{false};
    std::atomic<int> transition_keys_{0};

    std::unique_ptr<State> s_;
};

} // namespace fincept

// ── LOG_* macros — lazy evaluation via should_log() (P2.11) ────────────────
// Existing call sites LOG_INFO("Tag", some_qstring) keep working unchanged.
// If the level is filtered out, `msg` is never evaluated, so callers can pass
// `QString("...").arg(expensive())` or `logf("Sentence", {{"key", value}})`
// without paying for it in production.
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

// ── LOG_*_RL — per-key rate limit (policy B.8) ──────────────────────────────
// The first line for (tag, key) is written; later ones within interval_ms are counted and
// NOT formatted (msg is not evaluated); the next line written for the key ends with
// " (suppressed=N)". `key` is evaluated only when the level is on.
//   LOG_WARN_RL("DataHub", family + '|' + reason, 10000, logf("Topic failing", {{"topic", topic}, {"error", reason}}));
#define FINCEPT_LOG_RL_IMPL(level_enum, level_fn, tag, key, interval_ms, msg)                                          \
    do {                                                                                                               \
        if (fincept::Logger::instance().should_log(fincept::LogLevel::level_enum, tag)) {                              \
            int fincept_log_suppressed_ = 0;                                                                           \
            if (fincept::Logger::instance().rate_limit_pass(tag, key, interval_ms, &fincept_log_suppressed_,           \
                                                            fincept::LogLevel::level_enum))                            \
                fincept::Logger::instance().level_fn(                                                                  \
                    tag, fincept::detail::log_with_suppressed(msg, fincept_log_suppressed_));                          \
        }                                                                                                              \
    } while (0)

#define LOG_DEBUG_RL(tag, key, interval_ms, msg) FINCEPT_LOG_RL_IMPL(Debug, debug, tag, key, interval_ms, msg)
#define LOG_INFO_RL(tag, key, interval_ms, msg) FINCEPT_LOG_RL_IMPL(Info, info, tag, key, interval_ms, msg)
#define LOG_WARN_RL(tag, key, interval_ms, msg) FINCEPT_LOG_RL_IMPL(Warn, warn, tag, key, interval_ms, msg)
#define LOG_ERROR_RL(tag, key, interval_ms, msg) FINCEPT_LOG_RL_IMPL(Error, error, tag, key, interval_ms, msg)

// ── LOG_TRANSITION — log a keyed state only when it changes (policy B.2.4) ──
// `state`: 0 = healthy, anything else = a non-healthy state (bool or enum is fine).
// `level`: a fincept::LogLevel value (may depend on the state). msg is evaluated only when
// the line is written. Nothing is written for a key that is healthy the first time it is seen.
//   const bool failing = !ok;
//   LOG_TRANSITION("Network", "backend", failing, failing ? fincept::LogLevel::Warn : fincept::LogLevel::Info,
//                  failing ? logf("Backend unreachable", {{"error", err}}) : QStringLiteral("Backend reachable again"));
// For a recovery line with the outage length / failure count, call Logger::transition()
// with a LogTransition* (see the API notes).
#define LOG_TRANSITION(tag, key, state, level, msg)                                                                    \
    do {                                                                                                               \
        const fincept::LogLevel fincept_log_level_ = (level);                                                          \
        if (fincept::Logger::instance().should_log(fincept_log_level_, tag)) {                                         \
            const int fincept_log_state_ = static_cast<int>(state);                                                    \
            if (fincept::Logger::instance().transition_possible(fincept_log_state_) &&                                 \
                fincept::Logger::instance().transition(tag, key, fincept_log_state_))                                  \
                fincept::Logger::instance().log(fincept_log_level_, tag, msg);                                         \
        }                                                                                                              \
    } while (0)

// ── LOG_AUDIT — orders and money moves (policy B.7); cannot be switched off ──
//   LOG_AUDIT("Orders", "Order submitted", {{"venue", "zerodha"}, {"account", log_mask(client_id)}, {"qty", 10}});
//   LOG_AUDIT("Orders", logf("Order rejected", {{"client_id", cid}, {"reason", reason}}));
// Never a secret, a signed request's signature, an auth header or a raw request/response
// body; account numbers / client ids through log_mask(). Quantities and prices are fine.
#define LOG_AUDIT(tag, ...) fincept::Logger::instance().audit(tag, __VA_ARGS__)

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
