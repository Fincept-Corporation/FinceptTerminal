#pragma once
#include "core/identity/Uuid.h"

#include <QHash>
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
///   %LOCALAPPDATA%/com.fincept.terminal/profiles/<name>/
///
/// A manifest at root/profiles.json tracks the list of profiles. Each entry
/// carries a stable `ProfileId` UUID alongside its human-readable name —
/// names are what the UI shows, UUIDs are what code uses for identity. The
/// distinction matters for cloud sync (a name can be reused on a different
/// machine; a UUID cannot collide) and for the multi-window refactor where
/// per-profile state is keyed by UUID downstream.
///
/// The default profile is named "default" and is auto-created on first run.
///
/// Call set_active() BEFORE AppPaths::ensure_all() in main().
class ProfileManager {
  public:
    static ProfileManager& instance();

    /// Set the active profile. Must be called before AppPaths is used.
    /// Creates the profile directory + manifest entry (with a freshly-minted
    /// UUID) if the profile name is not yet known.
    void set_active(const QString& name);

    /// Returns the active profile name (default: "default").
    QString active() const;

    /// Stable UUID of the active profile. Lazy-mints + persists if the
    /// active profile is unknown to the manifest (e.g. first run, or a
    /// profile created on disk by hand).
    ProfileId active_profile_id() const;

    /// Returns true if a non-default profile is active.
    bool is_custom_profile() const;

    /// Returns all known profile names from the manifest.
    QStringList list_profiles() const;

    /// Returns the manifest entries: pairs of (name, ProfileId) in
    /// definition order. The "default" profile is always present even if
    /// missing from the manifest file.
    struct Entry {
        QString name;
        ProfileId id;
    };
    QList<Entry> profile_entries() const;

    /// Look up a profile's UUID by its name. Null on miss.
    ProfileId profile_id_for(const QString& name) const;

    /// Look up a profile's name by its UUID. Empty on miss.
    QString profile_name_for(const ProfileId& id) const;

    /// Creates a new profile entry in the manifest (does not switch to it).
    /// Mints a fresh UUID. Idempotent: if the name already exists, returns
    /// the existing UUID without touching the manifest.
    ProfileId create_profile(const QString& name);

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

    /// Cache of (name → ProfileId) loaded from the manifest. Populated lazily
    /// on first read; mutated by create_profile / delete_profile / set_active
    /// (when minting a new UUID for a previously-unknown name).
    mutable QHash<QString, ProfileId> id_cache_;
    mutable bool id_cache_loaded_ = false;

    QString manifest_path() const;

    /// Read the manifest into id_cache_. Idempotent. Tolerates legacy
    /// manifests that store profiles as plain strings rather than {name,id}
    /// objects: legacy entries get UUIDs minted + the manifest rewritten on
    /// next save.
    void load_id_cache_locked() const;

    /// Persist the current name list + UUID map back to disk. Always writes
    /// the new {name, id} object format.
    void save_manifest(const QStringList& profiles) const;
};

} // namespace fincept
