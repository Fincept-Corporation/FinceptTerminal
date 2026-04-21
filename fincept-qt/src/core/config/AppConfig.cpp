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

bool AppConfig::dark_mode() const {
    return settings_.value("ui/dark_mode", true).toBool();
}

int AppConfig::refresh_interval_ms() const {
    return settings_.value("data/refresh_interval_ms", 30000).toInt();
}

bool AppConfig::crypto_markets_enabled() const {
    // Environment override for emergency production toggles.
    const QString env = QString::fromUtf8(qgetenv("FINCEPT_ENABLE_CRYPTO_MARKETS")).trimmed().toLower();
    if (!env.isEmpty()) {
        if (env == "1" || env == "true" || env == "yes" || env == "on")
            return true;
        if (env == "0" || env == "false" || env == "no" || env == "off")
            return false;
    }
    return settings_.value("features/crypto_markets_enabled", false).toBool();
}

} // namespace fincept
