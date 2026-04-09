#include "core/config/ProfileManager.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept {

ProfileManager& ProfileManager::instance() {
    static ProfileManager s;
    return s;
}

void ProfileManager::set_active(const QString& name) {
    // Sanitise: only allow alphanumeric, dash, underscore
    QString clean = name.trimmed().toLower();
    for (QChar& c : clean) {
        if (!c.isLetterOrNumber() && c != '-' && c != '_')
            c = '_';
    }
    if (clean.isEmpty())
        clean = "default";

    active_profile_ = clean;

    // Ensure the profile directory tree exists
    QDir().mkpath(profile_root() + "/data");
    QDir().mkpath(profile_root() + "/logs");
    QDir().mkpath(profile_root() + "/cache");
    QDir().mkpath(profile_root() + "/files");
    QDir().mkpath(profile_root() + "/workspaces");

    // Register in manifest if not already there
    QStringList existing = list_profiles();
    if (!existing.contains(active_profile_)) {
        existing.append(active_profile_);
        save_manifest(existing);
    }
}

QString ProfileManager::active() const {
    return active_profile_;
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
    QStringList result{"default"};

    QFile f(manifest_path());
    if (!f.open(QIODevice::ReadOnly))
        return result;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject())
        return result;

    const QJsonArray arr = doc.object().value("profiles").toArray();
    for (const QJsonValue& v : arr) {
        const QString name = v.toString();
        if (!name.isEmpty() && !result.contains(name))
            result.append(name);
    }
    return result;
}

void ProfileManager::create_profile(const QString& name) {
    // Sanitise: same rules as set_active()
    QString clean = name.trimmed().toLower();
    for (QChar& c : clean) {
        if (!c.isLetterOrNumber() && c != '-' && c != '_')
            c = '_';
    }
    if (clean.isEmpty())
        return;

    QStringList profiles = list_profiles();
    if (!profiles.contains(clean)) {
        profiles.append(clean);
        save_manifest(profiles);
    }
    // Create directory tree eagerly so it's ready for use
    const QString root = AppPaths::root() + "/profiles/" + clean;
    QDir().mkpath(root + "/data");
    QDir().mkpath(root + "/logs");
    QDir().mkpath(root + "/cache");
    QDir().mkpath(root + "/files");
    QDir().mkpath(root + "/workspaces");
}

void ProfileManager::delete_profile(const QString& name) {
    if (name == "default")
        return; // cannot delete the default profile
    QStringList profiles = list_profiles();
    profiles.removeAll(name);
    save_manifest(profiles);
    // Intentionally NOT deleting the directory — user must do that to avoid data loss
}

QString ProfileManager::manifest_path() const {
    return AppPaths::root() + "/profiles.json";
}

void ProfileManager::save_manifest(const QStringList& profiles) const {
    QJsonArray arr;
    for (const QString& p : profiles)
        arr.append(p);

    QJsonObject obj;
    obj["profiles"] = arr;

    QFile f(manifest_path());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    } else {
        LOG_ERROR("ProfileManager", "Failed to write profiles manifest: " + manifest_path());
    }
}

} // namespace fincept
