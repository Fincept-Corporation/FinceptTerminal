#pragma once
#include <QString>
#include <QStringList>

namespace fincept {

/// Manages named user profiles, each with isolated data/logs/cache directories
/// and separate credentials.
///
/// Usage:
///   ProfileManager::instance().set_active("work");
///   // AppPaths::data() now returns .../profiles/work/data/
///
/// Profiles are stored under:
///   %LOCALAPPDATA%\com.fincept.terminal\profiles\<name>\
///
/// A manifest at root/profiles.json tracks the list of profiles.
/// The default profile is named "default".
///
/// Call set_active() BEFORE AppPaths::ensure_all() in main().
class ProfileManager {
  public:
    static ProfileManager& instance();

    /// Set the active profile. Must be called before AppPaths is used.
    /// Creates the profile directory if it doesn't exist.
    void set_active(const QString& name);

    /// Returns the active profile name (default: "default").
    QString active() const;

    /// Returns true if a non-default profile is active.
    bool is_custom_profile() const;

    /// Returns all known profile names from the manifest.
    QStringList list_profiles() const;

    /// Creates a new profile entry in the manifest (does not switch to it).
    void create_profile(const QString& name);

    /// Deletes a profile from the manifest. Does NOT delete its data directory
    /// (user must do that manually to avoid accidental data loss).
    void delete_profile(const QString& name);

    /// Returns the root directory for the active profile.
    /// When active == "default", returns AppPaths::root() directly (no migration needed).
    /// Otherwise returns AppPaths::root()/profiles/<name>/.
    QString profile_root() const;

    /// QSettings group prefix for the active profile — used by AppConfig
    /// to namespace per-profile settings under a shared Registry key.
    QString settings_group() const;

    /// SecureStorage key prefix — credentials are keyed as "profiles/<name>/<key>"
    /// so different profiles never share credential entries.
    QString secure_key_prefix() const;

  private:
    ProfileManager() = default;

    QString active_profile_{"default"};

    QString manifest_path() const;
    void save_manifest(const QStringList& profiles) const;
};

} // namespace fincept
