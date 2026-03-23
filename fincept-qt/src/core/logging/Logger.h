#pragma once
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>

namespace fincept {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
  public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_file(const QString& path);

    void debug(const QString& tag, const QString& msg);
    void info(const QString& tag, const QString& msg);
    void warn(const QString& tag, const QString& msg);
    void error(const QString& tag, const QString& msg);

  private:
    Logger() = default;
    void write(LogLevel level, const QString& tag, const QString& msg);

    LogLevel min_level_ = LogLevel::Info;
    QFile log_file_;
    QMutex mutex_;
};

} // namespace fincept

// Convenience macros
#define LOG_DEBUG(tag, msg) fincept::Logger::instance().debug(tag, msg)
#define LOG_INFO(tag, msg) fincept::Logger::instance().info(tag, msg)
#define LOG_WARN(tag, msg) fincept::Logger::instance().warn(tag, msg)
#define LOG_ERROR(tag, msg) fincept::Logger::instance().error(tag, msg)
