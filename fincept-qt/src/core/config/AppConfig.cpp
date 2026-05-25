#include "core/config/AppConfig.h"

#include "core/config/ProfileManager.h"

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

QString AppConfig::get_phase_one_server_url() const {
    return settings_.value(phase_one_server_url_key()).toString().trimmed();
}

void AppConfig::set_phase_one_server_url(const QString& value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        remove_phase_one_server_url();
        return;
    }

    settings_.setValue(phase_one_server_url_key(), trimmed);
    settings_.sync();
}

void AppConfig::remove_phase_one_server_url() {
    settings_.remove(phase_one_server_url_key());
    settings_.sync();
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

QString AppConfig::phase_one_server_url_key() const {
    const QString group = ProfileManager::instance().settings_group();
    if (group.isEmpty())
        return QStringLiteral("phase_one_server_url");
    return group + QStringLiteral("/phase_one_server_url");
}

} // namespace fincept
