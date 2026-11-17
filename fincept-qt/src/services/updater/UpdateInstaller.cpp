#include "services/updater/UpdateInstaller.h"

#include "core/logging/Logger.h"
#include "services/updater/UpdateInstallerPlatform.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

namespace fincept::services::updater {

UpdateInstaller::UpdateInstaller(const InstallContext& context, QObject* parent)
    : QObject(parent), context_(context) {}

UpdateInstaller* UpdateInstaller::create(const InstallContext& context, QObject* parent) {
    if (context.kind == InstallKind::Unknown)
        return nullptr;
    UpdateInstaller* installer = create_platform_installer(context, parent);
    if (!installer)
        LOG_WARN("UpdateInstaller", QStringLiteral("No in-place installer for asset '%1' on this platform")
                                        .arg(context.asset_key));
    return installer;
}

#if defined(Q_OS_WIN)

bool UpdateInstaller::relaunch_after_exit(const QString& program, const QStringList& args) {
    // Unused on Windows: the IFW installer starts the new version itself
    // (packaging/installer/config/controlscript.qs, update mode).
    Q_UNUSED(program);
    Q_UNUSED(args);
    return false;
}

#else

namespace {

/// Our environment, minus what the AppImage runtime set up for THIS image:
/// APPDIR / APPIMAGE / ARGV0 / OWD and every path-list entry inside the old
/// image's mount (LD_LIBRARY_PATH, QT_PLUGIN_PATH, XDG_DATA_DIRS, ...). That
/// mount disappears when we exit; the next AppImage sets up its own.
QProcessEnvironment relaunch_environment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appdir = QDir::cleanPath(env.value(QStringLiteral("APPDIR")));
    if (env.value(QStringLiteral("APPDIR")).isEmpty())
        return env;
    for (const char* name : {"APPDIR", "APPIMAGE", "ARGV0", "OWD"})
        env.remove(QString::fromLatin1(name));
    const QStringList keys = env.keys();
    for (const QString& key : keys) {
        const QString value = env.value(key);
        if (!value.contains(appdir))
            continue;
        QStringList kept;
        const QStringList parts = value.split(QLatin1Char(':'));
        for (const QString& part : parts) {
            const QString clean = QDir::cleanPath(part);
            if (clean != appdir && !clean.startsWith(appdir + QLatin1Char('/')))
                kept << part;
        }
        if (kept.isEmpty())
            env.remove(key);
        else
            env.insert(key, kept.join(QLatin1Char(':')));
    }
    return env;
}

} // namespace

bool UpdateInstaller::relaunch_after_exit(const QString& program, const QStringList& args) {
    if (program.isEmpty())
        return false;
    // Waits until this process is gone — the single-instance lock would
    // otherwise bounce the new instance back to us while we are still
    // closing — then becomes `program args`. Gives up waiting after ~60 s and
    // starts it anyway. `sleep 0.2` is not POSIX; fall back to whole seconds.
    // $0 / "$@" carry the program and its arguments, so nothing is re-quoted.
    const QString script = QStringLiteral("n=0\n"
                                          "while kill -0 %1 2>/dev/null && [ \"$n\" -lt 300 ]; do\n"
                                          "  if sleep 0.2 2>/dev/null; then n=$((n + 1)); else sleep 1; n=$((n + 5)); fi\n"
                                          "done\n"
                                          "exec \"$0\" \"$@\"\n")
                               .arg(QCoreApplication::applicationPid());
    QStringList sh_args{QStringLiteral("-c"), script, program};
    sh_args += args;

    QProcess proc;
    proc.setProgram(QStringLiteral("/bin/sh"));
    proc.setArguments(sh_args);
    proc.setProcessEnvironment(relaunch_environment());
    proc.setWorkingDirectory(QDir::homePath());
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    // Own session (outlives us, no controlling terminal); none of our file
    // descriptors — the instance-lock socket, the AppImage mount — leak into it.
    proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession |
                                  QProcess::UnixProcessFlag::CloseFileDescriptors |
                                  QProcess::UnixProcessFlag::ResetSignalHandlers);
    qint64 pid = 0;
    if (!proc.startDetached(&pid)) {
        LOG_ERROR("UpdateInstaller",
                  QStringLiteral("Could not start the relauncher for %1: %2").arg(program, proc.errorString()));
        return false;
    }
    LOG_INFO("UpdateInstaller", QStringLiteral("Relauncher pid %1 starts %2 once pid %3 has exited")
                                    .arg(pid)
                                    .arg(program)
                                    .arg(QCoreApplication::applicationPid()));
    return true;
}

#endif

} // namespace fincept::services::updater
