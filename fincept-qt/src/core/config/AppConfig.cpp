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

QStringList AppConfig::auth_base_urls() const {
    QStringList urls = settings_.value("auth/base_urls").toStringList();
    if (urls.isEmpty()) {
        urls = {
            "https://api.fincept.in",
            "https://finceptbackend.share.zrok.io",
        };
    }
    urls.removeDuplicates();
    return urls;
}

bool AppConfig::dark_mode() const {
    return settings_.value("ui/dark_mode", true).toBool();
}

int AppConfig::refresh_interval_ms() const {
    return settings_.value("data/refresh_interval_ms", 30000).toInt();
}

} // namespace fincept
