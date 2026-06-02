#pragma once
#include <QSet>
#include <QString>

namespace fincept {

/// Device-local flags controlling cloud sync. These are NEVER synced to the cloud
/// (the master flag itself must stay local — chicken/egg). Backed by
/// SettingsRepository (category "cloud") and cached in memory so the per-mutation
/// `should_sync()` check stays cheap.
///
/// Keys:
///   cloud.sync_enabled        -> "1"/"0"   (master switch)
///   cloud.exclude.<entity>    -> "1"/"0"   (advanced per-domain opt-out)
class CloudSyncSettings {
  public:
    static bool master_enabled();
    static void set_master_enabled(bool on);

    static bool is_domain_excluded(const QString& entity);
    static void set_domain_excluded(const QString& entity, bool excluded);

    /// master_enabled() && !is_domain_excluded(entity)
    static bool should_sync(const QString& entity);

    /// True if a setting key belongs to a cross-device-safe category and may sync.
    /// Excludes device-local & sensitive keys (cloud.* sync flags, credentials,
    /// security, python, storage, llm, mcp, data sources). Used by the settings
    /// repo hook + adapter. Allowlist by key prefix.
    static bool is_syncable_setting_key(const QString& key);

    static constexpr const char* kCategory = "cloud";
    static constexpr const char* kMasterKey = "cloud.sync_enabled";

  private:
    static void ensure_loaded();
};

} // namespace fincept
