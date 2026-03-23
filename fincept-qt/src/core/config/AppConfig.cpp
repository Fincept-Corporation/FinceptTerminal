#include "core/config/AppConfig.h"

namespace fincept {

AppConfig& AppConfig::instance() {
    static AppConfig s;
    return s;
}

AppConfig::AppConfig() : settings_("Fincept", "FinceptTerminal") {}

QVariant AppConfig::get(const QString& key, const QVariant& default_val) const {
    return settings_.value(key, default_val);
}

void AppConfig::set(const QString& key, const QVariant& value) {
    settings_.setValue(key, value);
}

void AppConfig::remove(const QString& key) {
    settings_.remove(key);
}

QString AppConfig::api_base_url() const {
    return settings_.value("api/base_url", "https://api.fincept.in").toString();
}

QString AppConfig::auth_token() const {
    return settings_.value("auth/token").toString();
}

void AppConfig::set_auth_token(const QString& token) {
    settings_.setValue("auth/token", token);
}

bool AppConfig::dark_mode() const {
    return settings_.value("ui/dark_mode", true).toBool();
}

int AppConfig::refresh_interval_ms() const {
    return settings_.value("data/refresh_interval_ms", 30000).toInt();
}

} // namespace fincept
