#include "core/config/AppPaths.h"

#include "core/config/ProfileManager.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstdio>

namespace fincept {

namespace {

AppPaths::Overrides& path_overrides() {
    static AppPaths::Overrides overrides;
    return overrides;
}

QString override_or_default(const QString& override_value, const QString& default_value) {
    return override_value.isEmpty() ? default_value : override_value;
}

} // namespace

QString AppPaths::root() {
#ifdef _WIN32
    // Use GenericDataLocation which returns %LOCALAPPDATA% directly on Windows,
    // avoiding the fragile double-cdUp() from AppLocalDataLocation.
    // GenericDataLocation = %LOCALAPPDATA% on Windows (Qt docs: QStandardPaths).
    const QString local_app_data =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return local_app_data + "/com.fincept.terminal";
#elif defined(__APPLE__)
    return QDir::homePath() + "/Library/Application Support/com.fincept.terminal";
#else
    return QDir::homePath() + "/.local/share/com.fincept.terminal";
#endif
}

QString AppPaths::data() {
    return override_or_default(path_overrides().data, ProfileManager::instance().profile_root() + "/data");
}

QString AppPaths::logs() {
    return override_or_default(path_overrides().logs, ProfileManager::instance().profile_root() + "/logs");
}

QString AppPaths::files() {
    return override_or_default(path_overrides().files, ProfileManager::instance().profile_root() + "/files");
}

QString AppPaths::cache() {
    return override_or_default(path_overrides().cache, ProfileManager::instance().profile_root() + "/cache");
}

QString AppPaths::models() {
    return override_or_default(path_overrides().models, root() + "/models");
}

QString AppPaths::runtime() {
    return override_or_default(path_overrides().runtime, root() + "/runtime");
}

QString AppPaths::workspaces() {
    return override_or_default(path_overrides().workspaces, ProfileManager::instance().profile_root() + "/workspaces");
}

QString AppPaths::crashdumps() {
    return override_or_default(path_overrides().crashdumps, ProfileManager::instance().profile_root() + "/crashdumps");
}

void AppPaths::ensure_all() {
    // Use fprintf to stderr (not qWarning) — main.cpp installs a Qt message
    // handler that routes qWarning into Logger, but Logger's file isn't open
    // yet when ensure_all() runs during startup. Raw stderr is the safe channel.
    const auto try_mkpath = [](const QString& p) {
        if (p.isEmpty()) {
            fprintf(stderr, "[AppPaths] empty path — skipping mkpath\n");
            return;
        }
        if (QDir().mkpath(p))
            return;
        const QFileInfo info(p);
        fprintf(stderr,
                "[AppPaths] mkpath failed: %s exists=%d isDir=%d writable=%d\n",
                qUtf8Printable(p), info.exists(), info.isDir(), info.isWritable());
    };
    try_mkpath(root());
    try_mkpath(data());
    try_mkpath(logs());
    try_mkpath(files());
    try_mkpath(cache());
    try_mkpath(models());
    try_mkpath(runtime());
    try_mkpath(workspaces());
    try_mkpath(crashdumps());
}

void AppPaths::set_overrides(const Overrides& overrides) {
    path_overrides() = overrides;
}

void AppPaths::clear_overrides() {
    path_overrides() = {};
}

} // namespace fincept
