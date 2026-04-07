#include "core/config/AppPaths.h"
#include "core/config/ProfileManager.h"

#include <QDir>
#include <QStandardPaths>

namespace fincept {

QString AppPaths::root() {
#ifdef _WIN32
    // AppLocalDataLocation = %LOCALAPPDATA%\Fincept\FinceptTerminal
    // Walk up two levels to reach %LOCALAPPDATA%, then append our app folder.
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir d(base);
    d.cdUp(); // %LOCALAPPDATA%\Fincept
    d.cdUp(); // %LOCALAPPDATA%
    return d.absolutePath() + "/com.fincept.terminal";
#elif defined(__APPLE__)
    return QDir::homePath() + "/Library/Application Support/com.fincept.terminal";
#else
    return QDir::homePath() + "/.local/share/com.fincept.terminal";
#endif
}

QString AppPaths::data() {
    return ProfileManager::instance().profile_root() + "/data";
}

QString AppPaths::logs() {
    return ProfileManager::instance().profile_root() + "/logs";
}

QString AppPaths::files() {
    return ProfileManager::instance().profile_root() + "/files";
}

QString AppPaths::cache() {
    return ProfileManager::instance().profile_root() + "/cache";
}

QString AppPaths::models() {
    return root() + "/models";
}

QString AppPaths::runtime() {
    return root() + "/runtime";
}

QString AppPaths::workspaces() {
    return ProfileManager::instance().profile_root() + "/workspaces";
}

void AppPaths::ensure_all() {
    QDir().mkpath(root());
    QDir().mkpath(data());
    QDir().mkpath(logs());
    QDir().mkpath(files());
    QDir().mkpath(cache());
    QDir().mkpath(models());
    QDir().mkpath(runtime());
    QDir().mkpath(workspaces());
}

} // namespace fincept
