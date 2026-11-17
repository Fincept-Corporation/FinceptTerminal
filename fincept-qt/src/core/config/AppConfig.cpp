#include "core/config/AppConfig.h"

#include "core/config/AppSettings.h"
#include "core/logging/Logger.h"

namespace fincept {

AppConfig& AppConfig::instance() {
    static AppConfig s;
    return s;
}

// This data folder's own store (core/config/AppSettings.h); it names the store
// itself, so it is right even if AppConfig is first touched before main() names
// the application.
AppConfig::AppConfig() : settings_(app_settings::open()) {}

QVariant AppConfig::get(const QString& key, const QVariant& default_val) const {
    return settings_.value(key, default_val);
}

// "Setting changed key=<name>" — written only when the stored value really changes. The
// value is logged only for booleans and numbers (flags, ports, limits); text values can be
// URLs, tokens or paths and are never logged.
static bool app_config_value_loggable(const QVariant& v) {
    switch (v.typeId()) {
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
            return true;
        default:
            return false;
    }
}

void AppConfig::set(const QString& key, const QVariant& value) {
    const bool log_change = Logger::instance().should_log(LogLevel::Info, "Settings") && settings_.value(key) != value;
    settings_.setValue(key, value);
    if (log_change) {
        if (app_config_value_loggable(value))
            LOG_INFO("Settings", logf("Setting changed", {{"key", key}, {"value", value.toString()}}));
        else
            LOG_INFO("Settings", logf("Setting changed", {{"key", key}}));
    }
}

void AppConfig::remove(const QString& key) {
    const bool log_change = Logger::instance().should_log(LogLevel::Info, "Settings") && settings_.contains(key);
    settings_.remove(key);
    if (log_change)
        LOG_INFO("Settings", logf("Setting removed", {{"key", key}}));
}

QString AppConfig::api_base_url() const {
    return settings_.value("api/base_url", "https://api.fincept.in").toString();
}

QString AppConfig::cloud_base_url() const {
    return settings_.value("api/cloud_base_url", "https://api.fincept.in/v1").toString();
}

bool AppConfig::dark_mode() const {
    return settings_.value("ui/dark_mode", true).toBool();
}

int AppConfig::refresh_interval_ms() const {
    return settings_.value("data/refresh_interval_ms", 30000).toInt();
}

} // namespace fincept
