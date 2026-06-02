#include "storage/sync/CloudSyncSettings.h"

#include "storage/repositories/SettingsRepository.h"

namespace fincept {
namespace {
bool g_cloudsync_loaded = false;
bool g_cloudsync_master = false;
QSet<QString> g_cloudsync_excluded; // entities the user opted out of (Advanced)

const QString kCloudExcludePrefix = QStringLiteral("cloud.exclude.");

QString cloud_exclude_key(const QString& entity) {
    return kCloudExcludePrefix + entity;
}
} // namespace

void CloudSyncSettings::ensure_loaded() {
    if (g_cloudsync_loaded)
        return;
    g_cloudsync_loaded = true;

    auto cat = SettingsRepository::instance().get_by_category(kCategory);
    if (cat.is_err())
        return;
    for (const Setting& s : cat.value()) {
        if (s.key == QLatin1String(kMasterKey))
            g_cloudsync_master = (s.value == QLatin1String("1"));
        else if (s.key.startsWith(kCloudExcludePrefix) && s.value == QLatin1String("1"))
            g_cloudsync_excluded.insert(s.key.mid(kCloudExcludePrefix.size()));
    }
}

bool CloudSyncSettings::master_enabled() {
    ensure_loaded();
    return g_cloudsync_master;
}

void CloudSyncSettings::set_master_enabled(bool on) {
    ensure_loaded();
    g_cloudsync_master = on;
    SettingsRepository::instance().set(kMasterKey, on ? "1" : "0", kCategory);
}

bool CloudSyncSettings::is_domain_excluded(const QString& entity) {
    ensure_loaded();
    return g_cloudsync_excluded.contains(entity);
}

void CloudSyncSettings::set_domain_excluded(const QString& entity, bool excluded) {
    ensure_loaded();
    if (excluded)
        g_cloudsync_excluded.insert(entity);
    else
        g_cloudsync_excluded.remove(entity);
    SettingsRepository::instance().set(cloud_exclude_key(entity), excluded ? "1" : "0", kCategory);
}

bool CloudSyncSettings::should_sync(const QString& entity) {
    return master_enabled() && !is_domain_excluded(entity);
}

bool CloudSyncSettings::is_syncable_setting_key(const QString& key) {
    static const char* const kSyncedPrefixes[] = {"appearance.", "general.", "notifications.", "keybindings.",
                                                  "voice."};
    for (const char* p : kSyncedPrefixes)
        if (key.startsWith(QLatin1String(p)))
            return true;
    return false;
}

} // namespace fincept
