#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <array>
#include <cstddef>

namespace fincept {

namespace {

// P1.8 — rotate when file exceeds this size. Keep 3 historical files.
constexpr qint64 kRotateBytes = 5 * 1024 * 1024; // 5 MB
constexpr int kRotateKeep = 3;

// P1.5 + P1.6 — date + local time + offset suffix.
// Example: 2026-04-18 03:12:08.123+05:30
QString format_timestamp() {
    const QDateTime now = QDateTime::currentDateTime();
    const int offset_sec = now.offsetFromUtc();
    const QChar sign = offset_sec >= 0 ? QChar('+') : QChar('-');
    const int abs_sec = std::abs(offset_sec);
    const int offs_h = abs_sec / 3600;
    const int offs_m = (abs_sec % 3600) / 60;
    return now.toString("yyyy-MM-dd HH:mm:ss.zzz") +
           QString("%1%2:%3")
               .arg(sign)
               .arg(offs_h, 2, 10, QChar('0'))
               .arg(offs_m, 2, 10, QChar('0'));
}

constexpr std::array<const char*, 6> kLevelNames = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static_assert(kLevelNames.size() == static_cast<std::size_t>(LogLevel::Fatal) + 1,
              "kLevelNames size must match LogLevel enum count");

} // namespace

Logger& Logger::instance() {
    static Logger s;
    // Flush-on-exit hook — runs before static destruction, while Qt is still alive.
    // Guarded so multiple instance() calls don't register duplicate routines.
    static std::atomic<bool> registered{false};
    bool expected = false;
    if (registered.compare_exchange_strong(expected, true)) {
        if (QCoreApplication::instance())
            qAddPostRoutine([]() { Logger::instance().flush_and_close(); });
    }
    return s;
}

void Logger::set_level(LogLevel level) {
    min_level_.store(level, std::memory_order_relaxed);
}

void Logger::set_tag_level(const QString& tag, LogLevel level) {
    QMutexLocker lock(&mutex_);
    tag_levels_[tag] = level;
}

void Logger::clear_tag_level(const QString& tag) {
    QMutexLocker lock(&mutex_);
    tag_levels_.remove(tag);
}

void Logger::clear_all_tag_levels() {
    QMutexLocker lock(&mutex_);
    tag_levels_.clear();
}

void Logger::set_json_mode(bool enabled) {
    json_mode_.store(enabled, std::memory_order_relaxed);
}

bool Logger::json_mode() const {
    return json_mode_.load(std::memory_order_relaxed);
}

QHash<QString, LogLevel> Logger::tag_levels() const {
    QMutexLocker lock(const_cast<QMutex*>(&mutex_));
    return tag_levels_;
}

bool Logger::is_enabled(LogLevel level, const QString& tag) const {
    LogLevel effective = min_level_.load(std::memory_order_relaxed);
    {
        QMutexLocker lock(const_cast<QMutex*>(&mutex_));
        auto it = tag_levels_.find(tag);
        if (it != tag_levels_.end())
            effective = it.value();
    }
    return level >= effective;
}

void Logger::set_file(const QString& path) {
    QMutexLocker lock(&mutex_);
    if (log_file_.isOpen()) {
        log_file_.flush(); // P1.10 — don't lose buffered bytes if called twice
        log_file_.close();
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

        log_file_.setFileName(target);
        // P1.7 — no QIODevice::Text so \n stays \n on all platforms; UTF-8 is
        // implicit because we write QByteArray directly (no QTextStream locale).
        if (log_file_.open(QIODevice::WriteOnly | QIODevice::Append)) {
            bytes_written_ = log_file_.size();
            return true;
        }
        fprintf(stderr, "[Logger] failed to open log file: %s error: %s\n",
                qUtf8Printable(target), qUtf8Printable(log_file_.errorString()));
        return false;
    };

    if (try_open(path))
        return;

    // Fallback: system temp dir so we always leave a breadcrumb.
    // Cross-platform: %TEMP% (Windows), /tmp or $TMPDIR (Linux/macOS).
    const QString fallback = QDir(QDir::tempPath()).filePath("fincept-boot.log");
    if (fallback == path)
        return;
    fprintf(stderr, "[Logger] falling back to temp path: %s\n", qUtf8Printable(fallback));
    try_open(fallback);
}

void Logger::rotate_if_needed_locked() {
    // P1.8 — size-based rotation. Called with mutex_ held.
    if (bytes_written_ < kRotateBytes || !log_file_.isOpen())
        return;

    const QString base = log_file_.fileName();
    log_file_.flush();
    log_file_.close();

    // Shift .N → .N+1; drop the oldest.
    const QString oldest = QString("%1.%2").arg(base).arg(kRotateKeep);
    QFile::remove(oldest);
    for (int i = kRotateKeep - 1; i >= 1; --i) {
        const QString src = QString("%1.%2").arg(base).arg(i);
        const QString dst = QString("%1.%2").arg(base).arg(i + 1);
        if (QFile::exists(src))
            QFile::rename(src, dst);
    }
    // Current → .1
    if (QFile::exists(base))
        QFile::rename(base, base + ".1");

    log_file_.setFileName(base);
    if (log_file_.open(QIODevice::WriteOnly | QIODevice::Append)) {
        bytes_written_ = 0;
    } else {
        qWarning() << "[Logger] rotate: reopen failed for" << base
                   << "error:" << log_file_.errorString();
    }
}

void Logger::flush_and_close() {
    QMutexLocker lock(&mutex_);
    if (log_file_.isOpen()) {
        log_file_.flush();
        log_file_.close();
    }
}

void Logger::trace(const QString& tag, const QString& msg) { write(LogLevel::Trace, tag, msg); }
void Logger::debug(const QString& tag, const QString& msg) { write(LogLevel::Debug, tag, msg); }
void Logger::info(const QString& tag, const QString& msg)  { write(LogLevel::Info,  tag, msg); }
void Logger::warn(const QString& tag, const QString& msg)  { write(LogLevel::Warn,  tag, msg); }
void Logger::error(const QString& tag, const QString& msg) { write(LogLevel::Error, tag, msg); }
void Logger::fatal(const QString& tag, const QString& msg) { write(LogLevel::Fatal, tag, msg); }

void Logger::write(LogLevel level, const QString& tag, const QString& msg) {
    // Level check is also done by LOG_* macros, but re-check here for direct callers.
    if (!is_enabled(level, tag))
        return;

    const QString ts = format_timestamp();
    const char* level_name = kLevelNames[static_cast<int>(level)];

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

    // P2.13 — single mutex acquisition for file write and rotation.
    // We intentionally do NOT echo via qDebug(): main.cpp installs a Qt
    // message handler that routes qDebug back into this logger, so an echo
    // here would recurse. stderr / debugger visibility is achieved by the
    // Qt handler fan-in, not by double-writing from this path.
    QMutexLocker lock(&mutex_);
    if (!log_file_.isOpen())
        return;

    const qint64 n = log_file_.write(bytes);
    if (n == bytes.size()) {
        bytes_written_ += n;
        log_file_.flush(); // P0.3 — survive crash/kill
        rotate_if_needed_locked();
    } else if (!degraded_.load(std::memory_order_relaxed)) {
        // P1.9 — warn at most once per session (via stderr directly, not
        // qWarning, to avoid re-entering the logger through the Qt handler).
        degraded_.store(true, std::memory_order_relaxed);
        fprintf(stderr, "[Logger] write failed (%lld bytes): %s\n",
                static_cast<long long>(n), qUtf8Printable(log_file_.errorString()));
    }
}

} // namespace fincept
