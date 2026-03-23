#include "core/logging/Logger.h"

namespace fincept {

Logger& Logger::instance() {
    static Logger s;
    return s;
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
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
    if (level < min_level_)
        return;

    static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
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
