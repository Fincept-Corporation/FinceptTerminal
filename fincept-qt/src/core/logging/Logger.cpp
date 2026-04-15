#include "core/logging/Logger.h"

#include <array>
#include <cstddef>

namespace fincept {

Logger& Logger::instance() {
    static Logger s;
    return s;
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
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

QHash<QString, LogLevel> Logger::tag_levels() const {
    return tag_levels_;
}

void Logger::set_file(const QString& path) {
    QMutexLocker lock(&mutex_);
    if (log_file_.isOpen())
        log_file_.close();

    // Rotate: keep one previous log
    QFile prev(path + ".prev");
    if (prev.exists())
        prev.remove();
    QFile current(path);
    if (current.exists())
        current.rename(path + ".prev");

    log_file_.setFileName(path);
    log_file_.open(QIODevice::WriteOnly | QIODevice::Text);
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

void Logger::write(LogLevel level, const QString& tag, const QString& msg) {
    LogLevel effective = min_level_;
    {
        QMutexLocker lock(&mutex_);
        auto it = tag_levels_.find(tag);
        if (it != tag_levels_.end())
            effective = it.value();
    }
    if (level < effective)
        return;

    static constexpr std::array<const char*, 4> names = {"DEBUG", "INFO", "WARN", "ERROR"};
    static_assert(names.size() == static_cast<std::size_t>(LogLevel::Error) + 1,
                  "LogLevel names array size must match LogLevel enum count");
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString line = QString("[%1] [%2] [%3] %4").arg(timestamp, names[static_cast<int>(level)], tag, msg);

    QMutexLocker lock(&mutex_);
    qDebug().noquote() << line;
    if (log_file_.isOpen()) {
        QTextStream stream(&log_file_);
        stream << line << "\n";
        stream.flush();
    }
}

} // namespace fincept
