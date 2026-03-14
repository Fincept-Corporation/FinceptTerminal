#pragma once
// Lightweight logger — structured output with levels
// No external dependencies (spdlog can replace this later)
// Thread-safe via mutex

#include <string>
#include <mutex>
#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace fincept::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };

class Logger {
public:
    static Logger& instance() {
        static Logger s;
        return s;
    }

    void set_level(LogLevel level) { min_level_ = level; }
    LogLevel level() const { return min_level_; }

    // Enable file logging (call once at startup)
    void set_log_file(const char* path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_) fclose(log_file_);
        log_file_ = fopen(path, "a");
    }

    void log(LogLevel level, const char* tag, const char* fmt, ...) {
        if (level < min_level_) return;

        va_list args;
        va_start(args, fmt);

        char msg[2048];
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        char timestamp[32];
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", t);

        std::lock_guard<std::mutex> lock(mutex_);
        fprintf(stderr, "[%s] [%s] [%s] %s\n",
                timestamp, level_str(level), tag, msg);

        // Also write to log file if enabled
        if (log_file_) {
            fprintf(log_file_, "[%s] [%s] [%s] %s\n",
                    timestamp, level_str(level), tag, msg);
            fflush(log_file_);
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger() {
        if (log_file_) fclose(log_file_);
    }
    LogLevel min_level_ = LogLevel::Info;
    FILE* log_file_ = nullptr;
    std::mutex mutex_;

    static const char* level_str(LogLevel l) {
        switch (l) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
        }
        return "?????";
    }
};

// Convenience macros
#define LOG_TRACE(tag, ...) ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Trace, tag, __VA_ARGS__)
#define LOG_DEBUG(tag, ...) ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Info,  tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Warn,  tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Error, tag, __VA_ARGS__)
#define LOG_FATAL(tag, ...) ::fincept::core::Logger::instance().log(::fincept::core::LogLevel::Fatal, tag, __VA_ARGS__)

} // namespace fincept::core
