#include "core/profile/ProfilePaths.h"

#include "core/config/ProfileManager.h"
#include "core/identity/Uuid.h"
#include "core/logging/Logger.h"

#include <QDir>
#include <QFileInfo>

namespace fincept {

QString ProfilePaths::profile_root() {
    return ProfileManager::instance().profile_root();
}

QString ProfilePaths::workspace_db() {
    return profile_root() + "/workspace.db";
}

QString ProfilePaths::layouts_dir() {
    return profile_root() + "/layouts";
}

QString ProfilePaths::layouts_index_db() {
    return layouts_dir() + "/_index.db";
}

QString ProfilePaths::panel_cache_db() {
    return profile_root() + "/cache.db";
}

QString ProfilePaths::crashes_dir() {
    return profile_root() + "/crashes";
}

void ProfilePaths::ensure_all() {
    const QString root = profile_root();
    if (root.isEmpty()) {
        LOG_ERROR("ProfilePaths",
                  "ensure_all() called before ProfileManager::set_active() — skipping");
        return;
    }

    // Mirror AppPaths::ensure_all() failure-reporting pattern: try mkpath,
    // log diagnostics on failure rather than throwing.
    const auto try_mkpath = [](const QString& p) {
        if (QDir().mkpath(p))
            return;
        const QFileInfo info(p);
        LOG_WARN("ProfilePaths",
                 QString("mkpath failed: %1 exists=%2 isDir=%3 writable=%4")
                     .arg(p).arg(info.exists()).arg(info.isDir()).arg(info.isWritable()));
    };

    try_mkpath(layouts_dir());
    try_mkpath(crashes_dir());
    // workspace.db / cache.db / layouts_index.db are files — created by their
    // respective stores on first open. Don't pre-create.
}

ProfileId ProfilePaths::active_profile_id() {
    return ProfileManager::instance().active_profile_id();
}

} // namespace fincept
