#pragma once

#include "services/updater/UpdateTypes.h"

#include <QObject>
#include <QString>

namespace fincept::services::updater {

/// Applies a downloaded, checksum-verified release file to the running
/// installation, in place. One instance per update attempt.
///
/// Contract for every platform:
///   - install() returns immediately; work is asynchronous (QProcess / signals),
///     never a blocked GUI thread.
///   - status() is emitted for every step with a short translated sentence, so
///     the progress window always says what is happening.
///   - The user's data root is never touched.
///   - Ends with exactly one of:
///       ready_to_restart() — the update is applied, or fully staged with a
///         visible hand-over (an installer window, or a relauncher that starts
///         the new version the moment this process exits). The caller must now
///         quit the application; the new version starts without the user doing
///         anything.
///       handed_off(message) — the update continues outside the terminal and
///         the terminal keeps running (Flatpak: the system's software installer
///         has been opened on the downloaded bundle). `message` tells the user
///         what to expect.
///       failed(message) — nothing was changed, or the change was rolled back;
///         the installed version still works. `message` is for the user.
///
/// Platform behaviour (UpdateInstaller_win.cpp / _mac.cpp / _linux.cpp):
///   WindowsIfw    — checks the installer's Authenticode signature when the
///                   running exe is signed, then starts the new setup.exe in
///                   its update mode (FinceptUpdate=1 TargetDir=<dir>): the
///                   installer window shows progress, replaces the installation
///                   in the same folder and starts the new version.
///   MacAppBundle  — mounts the .dmg, checks the new bundle's code signature
///                   (same Team ID as the running app when that is signed),
///                   copies it beside the installed bundle, then a relauncher
///                   swaps the two once this process has exited (rolling back
///                   on failure) and opens the new version.
///   LinuxDeb      — installs the package through pkexec with apt-get (the
///                   system asks for the password), streaming its output to
///                   status(), then relaunches.
///   LinuxFlatpak  — opens the downloaded bundle with the system handler.
///   LinuxAppImage / LinuxRpm (discontinued) — no installer; InstallContext
///                   blocks the update with where to go instead.
class UpdateInstaller : public QObject {
    Q_OBJECT

  public:
    /// Installer for `context.kind`, or nullptr when that kind cannot be
    /// updated in place (Unknown, or a kind this build has no code for).
    static UpdateInstaller* create(const InstallContext& context, QObject* parent = nullptr);

    /// Apply `package_path` (already sha256-verified) as an update to `info.version`.
    virtual void install(const QString& package_path, const UpdateInfo& info) = 0;

  signals:
    void status(const QString& text);
    void ready_to_restart();
    void handed_off(const QString& message);
    void failed(const QString& message);

  protected:
    explicit UpdateInstaller(const InstallContext& context, QObject* parent);

    /// Start `program` so that it outlives this process, runs only after this
    /// process has exited, and then launches the terminal again:
    /// POSIX: a detached `/bin/sh` that waits for our pid, then exec's
    /// `program args`. Returns false when the relauncher could not be started.
    bool relaunch_after_exit(const QString& program, const QStringList& args);

    InstallContext context_;
};

} // namespace fincept::services::updater
