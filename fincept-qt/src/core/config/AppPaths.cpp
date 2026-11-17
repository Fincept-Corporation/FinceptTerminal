#include "core/config/AppPaths.h"

#include "core/config/EditionIdentity.h"
#include "core/config/ProfileManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstdio>

namespace fincept {

QString AppPaths::platform_data_base() {
#ifdef _WIN32
    // Use GenericDataLocation which returns %LOCALAPPDATA% directly on Windows,
    // avoiding the fragile double-cdUp() from AppLocalDataLocation.
    // GenericDataLocation = %LOCALAPPDATA% on Windows (Qt docs: QStandardPaths).
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#elif defined(__APPLE__)
    return QDir::homePath() + "/Library/Application Support";
#else
    return QDir::homePath() + "/.local/share";
#endif
}

namespace {

struct RootOverride {
    QString accepted; ///< the folder to use, or empty
    bool refused = false;
};

/// The same folder, or one inside it? Canonical paths when they exist (links,
/// junctions, 8.3 names, case on Windows), else cleaned absolute paths.
bool app_paths_is_same_or_inside(const QString& path, const QString& folder) {
    const auto norm = [](const QString& p) {
        const QFileInfo fi(p);
        const QString canonical = fi.canonicalFilePath();
        return QDir::cleanPath(canonical.isEmpty() ? fi.absoluteFilePath() : canonical);
    };
    const QString a = norm(path);
    const QString b = norm(folder);
#ifdef _WIN32
    constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    return a.compare(b, cs) == 0 || a.startsWith(b + QLatin1Char('/'), cs);
}

RootOverride app_paths_evaluate_override(const QString& raw) {
    RootOverride r;
    if (raw.isEmpty())
        return r;
    const QString dir = QDir::cleanPath(raw);
    QString why;
    if (app_paths_is_same_or_inside(dir, AppPaths::legacy_shared_root())) {
        why = QStringLiteral("it is the com.fincept.terminal folder, which the private build of Fincept Terminal uses");
    } else {
        for (const char* rel : edition::kPrivateBuildFingerprints) {
            const QFileInfo fi(QDir(dir).filePath(QString::fromLatin1(rel)));
            if (fi.exists() || fi.isSymLink()) {
                why = QStringLiteral("it holds %1, a file only the private build of Fincept Terminal writes")
                          .arg(QString::fromLatin1(rel));
                break;
            }
        }
    }
    if (why.isEmpty()) {
        r.accepted = dir;
        return r;
    }
    r.refused = true;
    // Nor may a child process (scripts started without PythonRunner's
    // environment) inherit it and write into that folder.
    qunsetenv("FINCEPT_DATA_DIR");
    // stderr: root() is asked long before the log file exists.
    std::fprintf(stderr, "[AppPaths] FINCEPT_DATA_DIR=%s ignored: %s; using the default data folder\n",
                 qUtf8Printable(QDir::toNativeSeparators(dir)), qUtf8Printable(why));
    std::fflush(stderr);
    return r;
}

/// Evaluated once: the environment of a running terminal does not change, and
/// root() is asked on every path lookup.
const RootOverride& app_paths_root_override() {
    static const RootOverride r = app_paths_evaluate_override(qEnvironmentVariable("FINCEPT_DATA_DIR").trimmed());
    return r;
}

} // namespace

QString AppPaths::root_override() {
    return app_paths_root_override().accepted;
}

bool AppPaths::root_override_refused() {
    return app_paths_root_override().refused;
}

QString AppPaths::root() {
    const QString env_dir = root_override();
    if (!env_dir.isEmpty())
        return env_dir;
    // Edition-specific folder (EditionIdentity.h): never the com.fincept.terminal
    // folder the private build uses.
    return platform_data_base() + QLatin1Char('/') + QLatin1String(edition::kDataDirName);
}

QString AppPaths::legacy_shared_root() {
    return platform_data_base() + QLatin1Char('/') + QLatin1String(edition::kLegacyDataDirName);
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

QString AppPaths::workspaces() {
    return ProfileManager::instance().profile_root() + "/workspaces";
}

QString AppPaths::crashdumps() {
    return ProfileManager::instance().profile_root() + "/crashdumps";
}

QString AppPaths::temp() {
    const QString dir = cache() + QStringLiteral("/tmp");
    if (QFileInfo(dir).isDir() || QDir().mkpath(dir))
        return dir;
    const QFileInfo info(dir);
    std::fprintf(stderr, "[AppPaths] cannot create %s writable_parent=%d\n",
                 qUtf8Printable(QDir::toNativeSeparators(dir)), QFileInfo(info.absolutePath()).isWritable());
    std::fflush(stderr);
    return {};
}

QString AppPaths::temp_file(const QString& name) {
    const QString dir = temp();
    if (!dir.isEmpty())
        return dir + QLatin1Char('/') + name;
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1Char('/') +
           QLatin1String(edition::kTempPrefix) + QLatin1Char('-') + name;
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
        fprintf(stderr, "[AppPaths] mkpath failed: %s exists=%d isDir=%d writable=%d\n", qUtf8Printable(p),
                info.exists(), info.isDir(), info.isWritable());
    };
    try_mkpath(root());
#ifndef _WIN32
    // Owner-only (0700) default root: it holds the databases (encrypted
    // credentials, chat history), logs, and the temp files handed to scripts,
    // and mkpath() leaves it 0755 under the usual umask — readable by every
    // account on the machine. A FINCEPT_DATA_DIR folder keeps the permissions
    // its owner gave it. (Windows: %LOCALAPPDATA% is already private to the user.)
    if (root_override().isEmpty()) {
        const QFileDevice::Permissions group_or_other =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
            QFileDevice::WriteOther | QFileDevice::ExeOther;
        const QFileInfo root_info(root());
        if (root_info.isDir() && root_info.permissions().testAnyFlags(group_or_other) &&
            !QFile::setPermissions(root(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
            fprintf(stderr, "[AppPaths] could not make %s private to this user\n", qUtf8Printable(root()));
        }
    }
#endif
    try_mkpath(data());
    try_mkpath(logs());
    try_mkpath(files());
    try_mkpath(cache());
    try_mkpath(workspaces());
    try_mkpath(crashdumps());
}

QString AppPaths::bundled_resource(const QString& relative_path) {
    // Same layout list ConnectorCatalog / PythonSetupManager / NotebookLibrary
    // search. A plain "<exe>/resources/..." lookup is wrong on the shipped macOS
    // bundle (the release job moves Contents/MacOS/resources/* into
    // Contents/Resources/ for codesign) and on .deb/.rpm installs (binary in
    // <prefix>/bin, data in <prefix>/share/fincept-terminal/resources).
    const QString exe = QCoreApplication::applicationDirPath();
    const QString primary = exe + QStringLiteral("/resources/") + relative_path;
    const QStringList candidates = {
        primary,
        exe + QStringLiteral("/../Resources/") + relative_path,
        exe + QStringLiteral("/../Resources/resources/") + relative_path,
        exe + QStringLiteral("/../share/fincept-terminal/resources/") + relative_path,
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }
    return QDir::cleanPath(primary);
}

} // namespace fincept
