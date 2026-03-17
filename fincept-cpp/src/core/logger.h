#pragma once
// Logger — spdlog backend with rotating file sink
// Drop-in replacement: same LOG_* macros, no call sites changed
// Features: async logging, rotating file (5MB x 3), colored console

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace fincept::core {

class Logger {
public:
    static Logger& instance() {
        static Logger s;
        return s;
    }

    // Call once at startup to enable file logging with rotation
    // max_size: bytes per file, max_files: number of rotated files to keep
    void set_log_file(const std::string& path,
                      std::size_t max_size  = 5 * 1024 * 1024,
                      std::size_t max_files = 3) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            path, max_size, max_files);
        file_sink->set_pattern("[%H:%M:%S] [%l] [%n] %v");
        logger_->sinks().push_back(file_sink);
    }

    void set_level(spdlog::level::level_enum level) {
        logger_->set_level(level);
    }

    spdlog::logger* get() { return logger_.get(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() {
        // Async thread pool: 8192-slot queue, 1 background thread
        spdlog::init_thread_pool(8192, 1);

        auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        console_sink->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");

        logger_ = std::make_shared<spdlog::async_logger>(
            "fincept",
            spdlog::sinks_init_list{console_sink},
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);

        logger_->set_level(spdlog::level::info);
        spdlog::register_logger(logger_);
    }

    ~Logger() {
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::async_logger> logger_;
};

} // namespace fincept::core

// Convenience macros — printf-style, same interface as before
// Level check BEFORE stack allocation avoids 2KB buffer when message would be dropped
#define LOG_IMPL(level, spdlevel, tag, fmt, ...) do { \
    auto* _lg = ::fincept::core::Logger::instance().get(); \
    if (_lg->should_log(spdlevel)) { \
        char _log_buf[2048]; \
        std::snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
        _lg->level("[{}] {}", tag, _log_buf); \
    } \
} while(0)

#define LOG_TRACE(tag, fmt, ...) LOG_IMPL(trace,    spdlog::level::trace,    tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...) LOG_IMPL(debug,    spdlog::level::debug,    tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)  LOG_IMPL(info,     spdlog::level::info,     tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  LOG_IMPL(warn,     spdlog::level::warn,     tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) LOG_IMPL(error,    spdlog::level::err,      tag, fmt, ##__VA_ARGS__)
#define LOG_FATAL(tag, fmt, ...) LOG_IMPL(critical, spdlog::level::critical, tag, fmt, ##__VA_ARGS__)
