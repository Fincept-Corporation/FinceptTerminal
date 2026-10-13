#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QWriteLocker>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>

namespace fincept {

namespace {

// P1.8 — rotate when file exceeds this size. Keep 3 historical files.
constexpr qint64 kLoggerRotateBytes = 5 * 1024 * 1024; // 5 MB
constexpr int kLoggerRotateKeep = 3;

// Async writer tuning. A TRACE..INFO line reaches the OS at most this long after
// it was logged; WARN and above are written synchronously by the caller.
constexpr int kLoggerFlushIntervalMs = 250;
// Queue size that wakes the writer before its interval elapses.
constexpr qsizetype kLoggerWakeBytes = 8 * 1024;
// Back-pressure: past this many queued bytes the logging thread drains the queue
// itself, so a log storm the disk can't keep up with is bounded in memory.
constexpr qsizetype kLoggerBackpressureBytes = 1024 * 1024;

// True while the current thread holds the file mutex (draining, rotating or
// reopening the file). A Qt warning raised by QFile/QDir in there comes straight
// back through main.cpp's message handler into Logger::warn(); this flag makes
// that nested call queue its line instead of re-locking the non-recursive file
// mutex on the same thread (deadlock).
thread_local bool t_logger_in_drain = false;

struct LoggerDrainGuard {
    bool prev;
    LoggerDrainGuard() : prev(t_logger_in_drain) { t_logger_in_drain = true; }
    ~LoggerDrainGuard() { t_logger_in_drain = prev; }
    LoggerDrainGuard(const LoggerDrainGuard&) = delete;
    LoggerDrainGuard& operator=(const LoggerDrainGuard&) = delete;
};

// Local UTC offset, cached per wall-clock second.
//
// QDateTime::currentDateTime().offsetFromUtc() costs ~7.5 µs and goes through
// Qt's process-wide time-zone lock, so every log line from every thread used to
// serialise on it (measured: 8 threads gained no throughput over 1). The offset
// only changes at a DST transition or a system time-zone change, so it is
// recomputed at most once per second; within a second after such a change a
// line may still carry the previous offset — local time and offset are computed
// together, so the printed instant is correct either way.
//
// Packed into one atomic so a reader never pairs one second's offset with
// another second: (utc_second << 20) | (offset_sec + 2^19).
std::atomic<qint64> g_logger_offset_cache{-1};

int logger_utc_offset_sec(qint64 utc_ms) {
    const qint64 utc_sec = utc_ms >= 0 ? utc_ms / 1000 : (utc_ms - 999) / 1000;
    const qint64 packed = g_logger_offset_cache.load(std::memory_order_acquire);
    if (packed >= 0 && (packed >> 20) == utc_sec)
        return static_cast<int>(packed & 0xFFFFF) - (1 << 19);
    const int offset = QDateTime::fromMSecsSinceEpoch(utc_ms).offsetFromUtc(); // local zone
    if (utc_sec >= 0)
        g_logger_offset_cache.store((utc_sec << 20) | (qint64(offset) + (1 << 19)), std::memory_order_release);
    return offset;
}

// Days since 1970-01-01 → proleptic Gregorian civil date (H. Hinnant's algorithm).
void logger_civil_from_days(qint64 z, int& y, int& m, int& d) {
    z += 719468;
    const qint64 era = (z >= 0 ? z : z - 146096) / 146097;
    const qint64 doe = z - era * 146097;                                      // [0, 146096]
    const qint64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const qint64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);               // [0, 365]
    const qint64 mp = (5 * doy + 2) / 153;                                    // [0, 11]
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);                       // [1, 31]
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);                          // [1, 12]
    y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

char* logger_put_digits(char* p, int value, int width) {
    for (int i = width - 1; i >= 0; --i) {
        p[i] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    return p + width;
}

// `utc_ms` rendered at `offset_sec` east of UTC — the same text as
// QDateTime::toString("yyyy-MM-dd HH:mm:ss.zzz") + "+HH:MM".
QString logger_format_timestamp_at(qint64 utc_ms, int offset_sec) {
    const qint64 local_ms = utc_ms + qint64(offset_sec) * 1000;
    qint64 days = local_ms / 86400000;
    qint64 ms_of_day = local_ms % 86400000;
    if (ms_of_day < 0) {
        ms_of_day += 86400000;
        --days;
    }
    int y = 0, mo = 0, d = 0;
    logger_civil_from_days(days, y, mo, d);
    const int abs_off = std::abs(offset_sec);

    char buf[32];
    char* p = buf;
    p = logger_put_digits(p, y, 4);
    *p++ = '-';
    p = logger_put_digits(p, mo, 2);
    *p++ = '-';
    p = logger_put_digits(p, d, 2);
    *p++ = ' ';
    p = logger_put_digits(p, static_cast<int>(ms_of_day / 3600000), 2);
    *p++ = ':';
    p = logger_put_digits(p, static_cast<int>((ms_of_day / 60000) % 60), 2);
    *p++ = ':';
    p = logger_put_digits(p, static_cast<int>((ms_of_day / 1000) % 60), 2);
    *p++ = '.';
    p = logger_put_digits(p, static_cast<int>(ms_of_day % 1000), 3);
    *p++ = offset_sec >= 0 ? '+' : '-';
    p = logger_put_digits(p, abs_off / 3600, 2);
    *p++ = ':';
    p = logger_put_digits(p, (abs_off % 3600) / 60, 2);
    return QString::fromLatin1(buf, p - buf);
}

// P1.5 + P1.6 — date + local time + offset suffix.
// Example: 2026-04-18 03:12:08.123+05:30
QString logger_format_timestamp() {
    const qint64 utc_ms = QDateTime::currentMSecsSinceEpoch(); // lock-free
    return logger_format_timestamp_at(utc_ms, logger_utc_offset_sec(utc_ms));
}

constexpr std::array<const char*, 6> kLoggerLevelNames = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static_assert(kLoggerLevelNames.size() == static_cast<std::size_t>(LogLevel::Fatal) + 1,
              "kLoggerLevelNames size must match LogLevel enum count");

} // namespace

// ── Shared state ────────────────────────────────────────────────────────────
//
// Lock order: file_mutex -> queue_mutex, never the reverse. tag_lock is never
// held together with either of them.
struct Logger::State {
    // Per-tag level overrides — read-mostly, so a read/write lock instead of the
    // file mutex (level checks used to queue behind disk writes).
    mutable QReadWriteLock tag_lock;
    QHash<QString, LogLevel> tag_levels;

    // Producer side: formatted lines waiting for the writer.
    QMutex queue_mutex;
    QWaitCondition queue_cv;
    QByteArray pending;
    bool drain_requested = false; // writer: skip the rest of the batching window
    bool stop_requested = false;  // writer: exit

    // File side.
    QMutex file_mutex;
    QFile log_file;
    qint64 bytes_written = 0;

    std::thread writer;
    // false => no writer thread (not started yet, stopping, or failed to start):
    // every line is drained synchronously by the thread that logs it.
    std::atomic<bool> writer_running{false};
    // A file is open; lines may be queued. Lines logged while false are dropped,
    // exactly as they were before the file was opened / after it was closed.
    std::atomic<bool> accepting{false};
    std::atomic<bool> degraded{false}; // set after a write failure so we only warn once

    void drain_locked();            // file_mutex must be held
    void rotate_if_needed_locked(); // file_mutex must be held
    void start_writer();
    void stop_writer();
    void writer_loop();
};

void Logger::State::drain_locked() {
    QByteArray batch;
    {
        QMutexLocker q(&queue_mutex);
        batch.swap(pending);
        drain_requested = false;
    }
    if (batch.isEmpty() || !log_file.isOpen())
        return;

    LoggerDrainGuard guard;
    const qint64 n = log_file.write(batch);
    if (n == batch.size()) {
        bytes_written += n;
        log_file.flush(); // P0.3 — hand the bytes to the OS (survives a process crash)
        rotate_if_needed_locked();
    } else if (!degraded.exchange(true)) {
        // P1.9 — warn at most once per session (via stderr directly, not
        // qWarning, to avoid re-entering the logger through the Qt handler).
        fprintf(stderr, "[Logger] write failed (%lld of %lld bytes): %s\n", static_cast<long long>(n),
                static_cast<long long>(batch.size()), qUtf8Printable(log_file.errorString()));
    }
}

void Logger::State::rotate_if_needed_locked() {
    // P1.8 — size-based rotation. Called with file_mutex held.
    if (bytes_written < kLoggerRotateBytes || !log_file.isOpen())
        return;

    const QString base = log_file.fileName();
    log_file.flush();
    log_file.close();

    // Shift .N → .N+1; drop the oldest.
    const QString oldest = QString("%1.%2").arg(base).arg(kLoggerRotateKeep);
    QFile::remove(oldest);
    for (int i = kLoggerRotateKeep - 1; i >= 1; --i) {
        const QString src = QString("%1.%2").arg(base).arg(i);
        const QString dst = QString("%1.%2").arg(base).arg(i + 1);
        if (QFile::exists(src))
            QFile::rename(src, dst);
    }
    // Current → .1
    if (QFile::exists(base))
        QFile::rename(base, base + ".1");

    log_file.setFileName(base);
    if (log_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        bytes_written = 0;
    } else {
        // stderr, not qWarning(): this runs with file_mutex held and main.cpp's
        // Qt message handler routes qWarning() straight back into the logger.
        fprintf(stderr, "[Logger] rotate: reopen failed for %s error: %s\n", qUtf8Printable(base),
                qUtf8Printable(log_file.errorString()));
    }
}

void Logger::State::start_writer() {
    QMutexLocker q(&queue_mutex);
    if (writer.joinable())
        return;
    stop_requested = false;
    try {
        writer = std::thread([this]() { writer_loop(); });
        writer_running.store(true, std::memory_order_release);
    } catch (const std::exception& e) {
        // Logging stays fully functional, just synchronous (the pre-async behaviour).
        writer_running.store(false, std::memory_order_release);
        fprintf(stderr, "[Logger] could not start the writer thread (%s) — logging synchronously\n", e.what());
    }
}

void Logger::State::stop_writer() {
    std::thread t;
    {
        QMutexLocker q(&queue_mutex);
        if (!writer.joinable())
            return;
        if (writer.get_id() == std::this_thread::get_id())
            return; // never join ourselves
        // From here on every new line drains synchronously; nothing waits on the writer.
        writer_running.store(false, std::memory_order_release);
        stop_requested = true;
        queue_cv.wakeAll();
        t = std::move(writer);
    }
    t.join();
}

void Logger::State::writer_loop() {
    QMutexLocker q(&queue_mutex);
    for (;;) {
        while (!stop_requested && pending.isEmpty())
            queue_cv.wait(&queue_mutex);
        if (stop_requested)
            return; // the remaining queue is drained by flush_and_close() after join()

        // Batching window: let lines accumulate so one write+flush covers many,
        // unless a WARN+ line or a full buffer asked for an immediate drain.
        if (!drain_requested) {
            const QDeadlineTimer deadline(kLoggerFlushIntervalMs);
            while (!stop_requested && !drain_requested && !deadline.hasExpired())
                queue_cv.wait(&queue_mutex, deadline);
            if (stop_requested)
                return;
        }

        q.unlock(); // lock order: file_mutex before queue_mutex
        {
            QMutexLocker f(&file_mutex);
            drain_locked();
        }
        q.relock();
    }
}

// ── Logger ──────────────────────────────────────────────────────────────────

Logger::Logger() : s_(std::make_unique<State>()) {}

Logger::~Logger() = default; // never runs — instance() is deliberately immortal

Logger& Logger::instance() {
    // Immortal: a LOG_* from a static destructor that runs after main() returns
    // must never touch a destroyed logger. Its file is drained and closed by the
    // post-routine / atexit hook below, not by a destructor.
    static Logger* const s = new Logger();

    // Flush-on-exit hook — runs before static destruction, while Qt is still alive.
    // Registered only once a QCoreApplication exists (qAddPostRoutine is a no-op
    // without one); the old guard flipped to "registered" on the first call even
    // when no application existed yet, so a LOG before QApplication meant the hook
    // was never installed.
    static std::atomic<bool> post_routine_registered{false};
    if (!post_routine_registered.load(std::memory_order_acquire) && QCoreApplication::instance()) {
        bool expected = false;
        if (post_routine_registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            qAddPostRoutine([]() { Logger::instance().flush_and_close(); });
    }
    return *s;
}

void Logger::recompute_thresholds_locked() {
    int lowest = static_cast<int>(min_level_.load(std::memory_order_relaxed));
    for (auto it = s_->tag_levels.cbegin(); it != s_->tag_levels.cend(); ++it)
        lowest = std::min(lowest, static_cast<int>(it.value()));
    lowest_enabled_.store(lowest, std::memory_order_relaxed);
    has_tag_overrides_.store(!s_->tag_levels.isEmpty(), std::memory_order_release);
}

void Logger::set_level(LogLevel level) {
    QWriteLocker lock(&s_->tag_lock);
    min_level_.store(level, std::memory_order_relaxed);
    recompute_thresholds_locked();
}

void Logger::set_tag_level(const QString& tag, LogLevel level) {
    QWriteLocker lock(&s_->tag_lock);
    s_->tag_levels[tag] = level;
    recompute_thresholds_locked();
}

void Logger::clear_tag_level(const QString& tag) {
    QWriteLocker lock(&s_->tag_lock);
    s_->tag_levels.remove(tag);
    recompute_thresholds_locked();
}

void Logger::clear_all_tag_levels() {
    QWriteLocker lock(&s_->tag_lock);
    s_->tag_levels.clear();
    recompute_thresholds_locked();
}

void Logger::set_json_mode(bool enabled) {
    json_mode_.store(enabled, std::memory_order_relaxed);
}

bool Logger::json_mode() const {
    return json_mode_.load(std::memory_order_relaxed);
}

QHash<QString, LogLevel> Logger::tag_levels() const {
    QReadLocker lock(&s_->tag_lock);
    return s_->tag_levels;
}

bool Logger::is_enabled(LogLevel level, const QString& tag) const {
    if (static_cast<int>(level) < lowest_enabled_.load(std::memory_order_relaxed))
        return false;
    LogLevel effective = min_level_.load(std::memory_order_relaxed);
    if (has_tag_overrides_.load(std::memory_order_acquire)) {
        QReadLocker lock(&s_->tag_lock);
        const auto it = s_->tag_levels.constFind(tag);
        if (it != s_->tag_levels.cend())
            effective = it.value();
    }
    return level >= effective;
}

void Logger::set_file(const QString& path) {
    // Drain-on-exit fallback for paths that never destroy the QApplication
    // (exit() from deep inside the app). flush_and_close() is idempotent, so
    // running after the post-routine already closed the file is harmless.
    static const bool atexit_hooked = []() {
        std::atexit([]() { Logger::instance().flush_and_close(); });
        return true;
    }();
    Q_UNUSED(atexit_hooked)

    bool opened = false;
    {
        QMutexLocker lock(&s_->file_mutex);
        LoggerDrainGuard guard;
        s_->drain_locked(); // P1.10 — queued lines belong to the previous file
        if (s_->log_file.isOpen()) {
            s_->log_file.flush();
            s_->log_file.close();
        }

        // Use fprintf to stderr for these diagnostics — qWarning would recurse
        // through our own Qt message handler installed in main.cpp.
        const auto try_open = [this](const QString& target) -> bool {
            const QString parent = QFileInfo(target).absolutePath();
            if (!parent.isEmpty() && !QDir().mkpath(parent)) {
                fprintf(stderr, "[Logger] mkpath failed for parent dir: %s\n", qUtf8Printable(parent));
            }

            // Rotate: keep one previous log (set_file is a fresh-start event).
            QFile prev(target + ".prev");
            if (prev.exists())
                prev.remove();
            QFile current(target);
            if (current.exists())
                current.rename(target + ".prev");

            s_->log_file.setFileName(target);
            // P1.7 — no QIODevice::Text so \n stays \n on all platforms; UTF-8 is
            // implicit because we write QByteArray directly (no QTextStream locale).
            if (s_->log_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                s_->bytes_written = s_->log_file.size();
                return true;
            }
            fprintf(stderr, "[Logger] failed to open log file: %s error: %s\n", qUtf8Printable(target),
                    qUtf8Printable(s_->log_file.errorString()));
            return false;
        };

        opened = try_open(path);
        if (!opened) {
            // Fallback: system temp dir so we always leave a breadcrumb.
            // Cross-platform: %TEMP% (Windows), /tmp or $TMPDIR (Linux/macOS).
            const QString fallback = QDir(QDir::tempPath()).filePath("fincept-boot.log");
            if (fallback != path) {
                fprintf(stderr, "[Logger] falling back to temp path: %s\n", qUtf8Printable(fallback));
                opened = try_open(fallback);
            }
        }
    }

    if (opened)
        s_->start_writer();
    s_->accepting.store(opened, std::memory_order_release);
}

void Logger::flush_and_close() {
    State& s = *s_;
    s.accepting.store(false, std::memory_order_release);
    if (t_logger_in_drain)
        return; // reached from inside a drain on this thread (e.g. a Qt fatal raised by QFile)

    s.stop_writer();

    QMutexLocker lock(&s.file_mutex);
    LoggerDrainGuard guard;
    s.drain_locked();
    if (s.log_file.isOpen()) {
        s.log_file.flush();
        s.log_file.close();
    }
}

void Logger::trace(const QString& tag, const QString& msg) {
    write(LogLevel::Trace, tag, msg);
}
void Logger::debug(const QString& tag, const QString& msg) {
    write(LogLevel::Debug, tag, msg);
}
void Logger::info(const QString& tag, const QString& msg) {
    write(LogLevel::Info, tag, msg);
}
void Logger::warn(const QString& tag, const QString& msg) {
    write(LogLevel::Warn, tag, msg);
}
void Logger::error(const QString& tag, const QString& msg) {
    write(LogLevel::Error, tag, msg);
}
void Logger::fatal(const QString& tag, const QString& msg) {
    write(LogLevel::Fatal, tag, msg);
}

void Logger::write(LogLevel level, const QString& tag, const QString& msg) {
    // Level check is also done by LOG_* macros, but re-check here for direct callers.
    if (!is_enabled(level, tag))
        return;
    State& s = *s_;
    if (!s.accepting.load(std::memory_order_acquire))
        return; // no file open — dropped, as before

    // Format on the calling thread with no lock held.
    const QString ts = logger_format_timestamp();
    const char* level_name = kLoggerLevelNames[static_cast<int>(level)];

    QByteArray bytes;
    if (json_mode_.load(std::memory_order_relaxed)) {
        // P3.17 — structured JSON output: one object per line.
        QJsonObject obj{
            {"ts", ts},
            {"level", QString::fromLatin1(level_name)},
            {"tag", tag},
            {"msg", msg},
        };
        bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        bytes.append('\n');
    } else {
        const QString line = QString("[%1] [%2] [%3] %4\n").arg(ts, level_name, tag, msg);
        bytes = line.toUtf8();
    }

    // We intentionally do NOT echo via qDebug(): main.cpp installs a Qt message
    // handler that routes qDebug back into this logger, so an echo here would
    // recurse. stderr / debugger visibility comes from the Qt handler fan-in.
    //
    // WARN+ lines are drained (written + flushed) right here, together with every
    // line queued before them, so they are on disk before the LOG_* call returns
    // and file order still matches queue order.
    bool drain_here = level >= LogLevel::Warn || !s.writer_running.load(std::memory_order_acquire);
    {
        QMutexLocker q(&s.queue_mutex);
        const bool was_empty = s.pending.isEmpty();
        s.pending.append(bytes);
        const qsizetype queued = s.pending.size();
        if (queued >= kLoggerBackpressureBytes)
            drain_here = true;
        if (queued >= kLoggerWakeBytes)
            s.drain_requested = true;
        if (was_empty || s.drain_requested)
            s.queue_cv.wakeOne(); // first line of a batch starts the writer's window
    }
    if (drain_here && !t_logger_in_drain) {
        QMutexLocker f(&s.file_mutex);
        s.drain_locked();
    }
}

} // namespace fincept
