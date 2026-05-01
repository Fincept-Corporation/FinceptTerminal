#include "core/config/ProfileManager.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace fincept {

namespace {

QString sanitise_name(const QString& name) {
    QString clean = name.trimmed().toLower();
    for (QChar& c : clean) {
        if (!c.isLetterOrNumber() && c != '-' && c != '_')
            c = '_';
    }
    if (clean.isEmpty())
        clean = "default";
    return clean;
}

void ensure_profile_dirs(const QString& root) {
    QDir().mkpath(root + "/data");
    QDir().mkpath(root + "/logs");
    QDir().mkpath(root + "/cache");
    QDir().mkpath(root + "/files");
    QDir().mkpath(root + "/workspaces");
}

} // namespace

ProfileManager& ProfileManager::instance() {
    static ProfileManager s;
    return s;
}

void ProfileManager::set_active(const QString& name) {
    const QString clean = sanitise_name(name);
    active_profile_ = clean;
    ensure_profile_dirs(profile_root());

    // Mint a UUID if the profile is new to the manifest. We do this through
    // create_profile() so the manifest stays consistent — set_active() is the
    // path users hit on first launch (with name="default") and we want the
    // default profile to acquire its persistent UUID then, not on the second
    // launch.
    load_id_cache_locked();
    if (!id_cache_.contains(clean)) {
        const ProfileId minted = ProfileId::generate();
        id_cache_.insert(clean, minted);
        QStringList existing = list_profiles();
        if (!existing.contains(clean))
            existing.append(clean);
        save_manifest(existing);
    }
}

QString ProfileManager::active() const {
    return active_profile_;
}

ProfileId ProfileManager::active_profile_id() const {
    load_id_cache_locked();
    auto it = id_cache_.constFind(active_profile_);
    if (it != id_cache_.constEnd())
        return it.value();
    // No entry yet — synthesise + persist. This handles the rare case where
    // a profile directory was created out-of-band (e.g. user copied folder).
    const ProfileId minted = ProfileId::generate();
    id_cache_.insert(active_profile_, minted);
    QStringList existing = list_profiles();
    if (!existing.contains(active_profile_))
        existing.append(active_profile_);
    save_manifest(existing);
    return minted;
}

bool ProfileManager::is_custom_profile() const {
    return active_profile_ != "default";
}

QString ProfileManager::profile_root() const {
    if (active_profile_ == "default")
        return AppPaths::root();
    return AppPaths::root() + "/profiles/" + active_profile_;
}

QString ProfileManager::settings_group() const {
    if (active_profile_ == "default")
        return {};
    return "profiles/" + active_profile_;
}

QString ProfileManager::secure_key_prefix() const {
    if (active_profile_ == "default")
        return {};
    return "profiles/" + active_profile_ + "/";
}

QStringList ProfileManager::list_profiles() const {
    load_id_cache_locked();
    QStringList result{"default"};
    for (auto it = id_cache_.constBegin(); it != id_cache_.constEnd(); ++it) {
        if (!result.contains(it.key()))
            result.append(it.key());
    }
    return result;
}

QList<ProfileManager::Entry> ProfileManager::profile_entries() const {
    load_id_cache_locked();
    QList<Entry> out;
    // Always emit "default" first so UI ordering is stable.
    if (id_cache_.contains("default"))
        out.append({"default", id_cache_.value("default")});
    for (auto it = id_cache_.constBegin(); it != id_cache_.constEnd(); ++it) {
        if (it.key() == "default") continue;
        out.append({it.key(), it.value()});
    }
    return out;
}

ProfileId ProfileManager::profile_id_for(const QString& name) const {
    load_id_cache_locked();
    return id_cache_.value(sanitise_name(name));
}

QString ProfileManager::profile_name_for(const ProfileId& id) const {
    load_id_cache_locked();
    for (auto it = id_cache_.constBegin(); it != id_cache_.constEnd(); ++it) {
        if (it.value() == id)
            return it.key();
    }
    return {};
}

ProfileId ProfileManager::create_profile(const QString& name) {
    const QString clean = sanitise_name(name);
    if (clean.isEmpty())
        return ProfileId();

    load_id_cache_locked();
    if (id_cache_.contains(clean))
        return id_cache_.value(clean); // idempotent

    const ProfileId minted = ProfileId::generate();
    id_cache_.insert(clean, minted);

    QStringList profiles = list_profiles();
    if (!profiles.contains(clean))
        profiles.append(clean);
    save_manifest(profiles);

    // Create directory tree eagerly so it's ready for use.
    const QString root = AppPaths::root() + "/profiles/" + clean;
    ensure_profile_dirs(root);
    return minted;
}

void ProfileManager::delete_profile(const QString& name) {
    if (name == "default")
        return; // cannot delete the default profile

    load_id_cache_locked();
    id_cache_.remove(name);

    QStringList profiles = list_profiles();
    profiles.removeAll(name);
    save_manifest(profiles);
    // Intentionally NOT deleting the directory — user must do that to avoid data loss
}

QString ProfileManager::manifest_path() const {
    return AppPaths::root() + "/profiles.json";
}

void ProfileManager::load_id_cache_locked() const {
    if (id_cache_loaded_)
        return;
    id_cache_loaded_ = true;

    // Default profile always exists in cache; UUID is minted on first read
    // if the manifest doesn't already have it.
    if (!id_cache_.contains("default"))
        id_cache_.insert("default", ProfileId::generate());

    QFile f(manifest_path());
    if (!f.open(QIODevice::ReadOnly))
        return; // first run — keep the freshly-minted default UUID

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    const QJsonArray arr = doc.object().value("profiles").toArray();
    bool needs_rewrite = false;
    for (const QJsonValue& v : arr) {
        if (v.isString()) {
            // Legacy format: bare string. Mint a UUID for it, flag manifest
            // rewrite so the next save persists the upgraded form.
            const QString name = v.toString();
            if (name.isEmpty()) continue;
            if (!id_cache_.contains(name))
                id_cache_.insert(name, ProfileId::generate());
            needs_rewrite = true;
        } else if (v.isObject()) {
            const QJsonObject obj = v.toObject();
            const QString name = obj.value("name").toString();
            if (name.isEmpty()) continue;
            const QString id_str = obj.value("id").toString();
            ProfileId id = ProfileId::from_string(id_str);
            if (id.is_null()) {
                id = ProfileId::generate();
                needs_rewrite = true;
            }
            id_cache_.insert(name, id);
        }
    }

    if (needs_rewrite) {
        QStringList names;
        for (auto it = id_cache_.constBegin(); it != id_cache_.constEnd(); ++it)
            names.append(it.key());
        save_manifest(names);
    }
}

void ProfileManager::save_manifest(const QStringList& profiles) const {
    // Ensure every name has an id in the cache before writing.
    for (const QString& p : profiles) {
        if (!id_cache_.contains(p))
            id_cache_.insert(p, ProfileId::generate());
    }

    QJsonArray arr;
    // Always serialise "default" first for stable diffs.
    QStringList ordered;
    if (profiles.contains("default"))
        ordered.append("default");
    for (const QString& p : profiles) {
        if (p != "default" && !ordered.contains(p))
            ordered.append(p);
    }
    for (const QString& p : ordered) {
        QJsonObject entry;
        entry["name"] = p;
        entry["id"] = id_cache_.value(p).to_string();
        arr.append(entry);
    }

    QJsonObject obj;
    obj["profiles"] = arr;
    obj["schema_version"] = 2; // v1 = bare strings; v2 = {name, id} objects.

    QFile f(manifest_path());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    } else {
        LOG_ERROR("ProfileManager", "Failed to write profiles manifest: " + manifest_path());
    }
}

} // namespace fincept
