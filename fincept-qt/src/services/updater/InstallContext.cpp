#include "services/updater/InstallContext.h"

#include "core/config/EditionIdentity.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>

namespace fincept::services::updater {

namespace {

// User-facing text is translated in the "InstallContext" context. The calls
// spell QCoreApplication::translate out in full so lupdate can extract them.

QString install_kind_name(InstallKind kind) {
    switch (kind) {
        case InstallKind::Unknown:
            return QStringLiteral("unknown");
        case InstallKind::WindowsIfw:
            return QStringLiteral("windows-ifw");
        case InstallKind::MacAppBundle:
            return QStringLiteral("mac-app-bundle");
        case InstallKind::LinuxAppImage:
            return QStringLiteral("linux-appimage");
        case InstallKind::LinuxDeb:
            return QStringLiteral("linux-deb");
        case InstallKind::LinuxRpm:
            return QStringLiteral("linux-rpm");
        case InstallKind::LinuxFlatpak:
            return QStringLiteral("linux-flatpak");
    }
    return QStringLiteral("unknown");
}

/// Fill `ctx` as "cannot update itself" with the generic reason.
void install_mark_unknown(InstallContext& ctx) {
    ctx.kind = InstallKind::Unknown;
    ctx.asset_key.clear();
    ctx.target_path.clear();
    ctx.can_self_update = false;
    ctx.blocked_reason = QCoreApplication::translate("InstallContext",
                                                     "This copy of Fincept Terminal was not installed from a release "
                                                     "package, so it cannot update itself.");
}

void install_set_kind(InstallContext& ctx, InstallKind kind, const QString& asset_suffix, const QString& target) {
    ctx.kind = kind;
    ctx.asset_key = ctx.platform_key + QLatin1Char('-') + asset_suffix;
    ctx.target_path = target;
    ctx.can_self_update = true;
    ctx.blocked_reason.clear();
}

#if defined(Q_OS_LINUX)
/// Where the Linux packages are published (the same page as
/// UpdateService::releases_page_url()).
constexpr const char* kInstallReleasesPageUrl = "https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest";

/// A kind that is no longer published (the AppImage and the .rpm, up to 4.5.3):
/// recognised so the user is told plainly where to go, never offered an update.
void install_set_discontinued(InstallContext& ctx, InstallKind kind, const QString& target, const QString& reason) {
    ctx.kind = kind;
    ctx.asset_key.clear();
    ctx.target_path = target;
    ctx.can_self_update = false;
    ctx.blocked_reason = reason;
}

/// Run `program args` and report whether it exited with status 0. Short
/// timeouts: this runs once, on the GUI thread, the first time the updater
/// asks how the terminal was installed.
bool install_query_succeeds(const QString& program, const QStringList& args) {
    const QString exe = QStandardPaths::findExecutable(program);
    if (exe.isEmpty())
        return false;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(exe, args);
    if (!proc.waitForStarted(2000))
        return false;
    if (!proc.waitForFinished(2000)) {
        LOG_WARN("UpdateService", QStringLiteral("'%1' did not answer within 2 s while detecting the install kind")
                                      .arg(program));
        proc.kill();
        proc.waitForFinished(500);
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}
#endif

InstallContext install_detect() {
    InstallContext ctx;
    ctx.platform_key = current_platform_key();
    install_mark_unknown(ctx);

    if (ctx.platform_key.isEmpty()) {
        ctx.blocked_reason = QCoreApplication::translate("InstallContext",
                                                         "Updates are not published for this kind of computer, "
                                                         "so Fincept Terminal cannot update itself.");
        LOG_INFO("UpdateService", QStringLiteral("Install kind: unknown — unsupported platform (%1 / %2)")
                                      .arg(QSysInfo::kernelType(), QSysInfo::buildCpuArchitecture()));
        return ctx;
    }

#if defined(Q_OS_WIN)
    const QString app_dir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    if (QFileInfo::exists(QDir(app_dir).filePath(QStringLiteral("FinceptMaintenanceTool.exe"))))
        install_set_kind(ctx, InstallKind::WindowsIfw, QStringLiteral("ifw"), app_dir);
#elif defined(Q_OS_MACOS)
    const QString suffix = QStringLiteral(".app/Contents/MacOS");
    const QString clean_dir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    if (clean_dir.endsWith(suffix)) {
        const QString bundle = clean_dir.left(clean_dir.size() - QStringLiteral("/Contents/MacOS").size());
        install_set_kind(ctx, InstallKind::MacAppBundle, QStringLiteral("dmg"), bundle);
        // Gatekeeper runs a quarantined app from a random read-only copy
        // (App Translocation); an app opened straight from the mounted .dmg
        // lives under /Volumes. Neither copy can be replaced.
        if (bundle.contains(QLatin1String("/AppTranslocation/")) || bundle.startsWith(QLatin1String("/Volumes/"))) {
            ctx.can_self_update = false;
            ctx.blocked_reason = QCoreApplication::translate(
                "InstallContext", "Fincept Terminal is running from a temporary or read-only location, so it "
                                  "cannot update itself. Move Fincept Terminal to your Applications folder, then "
                                  "open it again.");
        }
    }
#elif defined(Q_OS_LINUX)
    const QString real_exe = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    // Running from an AppImage means this executable lies inside the image's
    // mount ($APPDIR). $APPIMAGE alone proves nothing: every program started
    // from an AppImage (a launcher, a terminal emulator) inherits it, and it
    // then names another application's file.
    const QString appdir = QFileInfo(qEnvironmentVariable("APPDIR")).canonicalFilePath();
    const QFileInfo appimage_info(qEnvironmentVariable("APPIMAGE"));
    const bool in_appimage = !appdir.isEmpty() && appdir != QLatin1String("/") && appimage_info.isFile() &&
                             real_exe.startsWith(appdir + QLatin1Char('/'));
    if (QFileInfo::exists(QStringLiteral("/.flatpak-info"))) {
        // The installer hands the downloaded bundle to the system; nothing in
        // the sandbox is replaced by the terminal itself.
        install_set_kind(ctx, InstallKind::LinuxFlatpak, QStringLiteral("flatpak"), QString());
    } else if (in_appimage) {
        install_set_discontinued(
            ctx, InstallKind::LinuxAppImage, appimage_info.absoluteFilePath(),
            QCoreApplication::translate("InstallContext",
                                        "Fincept Terminal is no longer published as an AppImage, so this copy cannot "
                                        "update itself. Install the Flatpak (recommended) or, on Ubuntu and Debian, "
                                        "the .deb package from %1, then delete this AppImage.")
                .arg(QString::fromLatin1(kInstallReleasesPageUrl)));
    } else if (real_exe.startsWith(QLatin1String(edition::kLinuxInstallPrefix) + QLatin1Char('/'))) {
        // Only the edition's own package prefix: /opt/fincept-terminal is the
        // private build's package (and releases up to 4.5.2), which this
        // edition's update must never replace. A legacy install there reads as
        // "not installed from a release package" and points the user to the
        // download page instead.
        const QString prefix = QString::fromLatin1(edition::kLinuxInstallPrefix);
        if (install_query_succeeds(QStringLiteral("dpkg-query"), {QStringLiteral("-S"), real_exe}))
            install_set_kind(ctx, InstallKind::LinuxDeb, QStringLiteral("deb"), prefix);
        else if (install_query_succeeds(QStringLiteral("rpm"), {QStringLiteral("-qf"), real_exe}))
            install_set_discontinued(
                ctx, InstallKind::LinuxRpm, prefix,
                QCoreApplication::translate("InstallContext",
                                            "Fincept Terminal is no longer published as an .rpm package, so this copy "
                                            "cannot update itself. Install the Flatpak from %1, then remove the %2 "
                                            "package with your package manager.")
                    .arg(QString::fromLatin1(kInstallReleasesPageUrl), QString::fromLatin1(edition::kLinuxPackageName)));
        if (ctx.kind == InstallKind::LinuxDeb && QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty()) {
            ctx.can_self_update = false;
            ctx.blocked_reason = QCoreApplication::translate(
                "InstallContext", "This system has no way to ask for the administrator password that installing "
                                  "the update needs. Install the new version with your system's package manager "
                                  "instead.");
        }
    }
#endif

    LOG_INFO("UpdateService",
             QStringLiteral("Install kind: %1, platform=%2, asset=%3, target='%4', self-update=%5%6")
                 .arg(install_kind_name(ctx.kind), ctx.platform_key,
                      ctx.asset_key.isEmpty() ? QStringLiteral("<none>") : ctx.asset_key, ctx.target_path,
                      ctx.can_self_update ? QStringLiteral("yes") : QStringLiteral("no"),
                      ctx.can_self_update ? QString() : QStringLiteral(" (%1)").arg(ctx.blocked_reason)));
    return ctx;
}

} // namespace

QString current_platform_key() {
    // The build's own architecture (compile time), not the CPU it happens to
    // run on: an x64 build under emulation must fetch the x64 package.
#if defined(Q_PROCESSOR_X86_64)
    const QString arch = QStringLiteral("x64");
#elif defined(Q_PROCESSOR_ARM_64)
    const QString arch = QStringLiteral("arm64");
#else
    const QString arch;
#endif
#if defined(Q_OS_WIN)
    const QString os = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    const QString os = QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    const QString os = QStringLiteral("linux");
#else
    const QString os;
#endif
    if (arch.isEmpty() || os.isEmpty())
        return {};
    return os + QLatin1Char('-') + arch;
}

const InstallContext& install_context() {
    // Computed on first use (it may start dpkg-query / rpm), never at static
    // init; thread-safe by the rules for function-local statics.
    static const InstallContext context = install_detect();
    return context;
}

} // namespace fincept::services::updater
