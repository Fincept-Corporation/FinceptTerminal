// UpdateInstaller for Linux (and other non-Apple UNIX) installations:
//   LinuxDeb      — install the package through pkexec with apt-get (dpkg as a
//                   fallback), streaming its output to status(), relaunch.
//   LinuxFlatpak  — the sandbox cannot install anything: open the bundle with
//                   the system's software installer (OpenURI portal).
// The AppImage and the .rpm are no longer published: InstallContext reports
// those kinds as unable to update themselves (with where to go instead), so
// no installer exists for them.

#include "core/logging/Logger.h"
#include "services/updater/UpdateInstaller.h"
#include "services/updater/UpdateInstallerPlatform.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace fincept::services::updater {

namespace {

constexpr int kMaxStatusChars = 120;
constexpr int kKeptOutputLines = 12;

QString shorten(const QString& line) {
    const QString simple = line.simplified();
    if (simple.size() <= kMaxStatusChars)
        return simple;
    return simple.left(kMaxStatusChars - 1) + QChar(0x2026);
}

// ── .deb through pkexec ──────────────────────────────────────────────────────

class PackageInstaller final : public UpdateInstaller {
  public:
    PackageInstaller(const InstallContext& context, QObject* parent) : UpdateInstaller(context, parent) {}

    ~PackageInstaller() override {
        // Never interrupt a package manager halfway (dpkg would be left
        // half-configured): let it run on without us.
        if (process_ && process_->state() != QProcess::NotRunning) {
            LOG_WARN("UpdateInstaller", "Installer object destroyed while the package manager runs — leaving it running");
            process_->disconnect(this);
            process_->setParent(nullptr);
        }
    }

    void install(const QString& package_path, const UpdateInfo& info) override {
        if (process_)
            return;
        version_ = info.version;
        const QString package = QFileInfo(package_path).absoluteFilePath();
        if (!QFileInfo(package).isFile()) {
            emit failed(tr("The downloaded update could not be found:\n%1").arg(package));
            return;
        }
        const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        if (pkexec.isEmpty()) {
            emit failed(tr("This system has no way to ask for the administrator password that installing the update "
                           "needs. Install %1 with your system's package manager instead.")
                            .arg(package));
            return;
        }

        // apt-get resolves a changed Depends list; plain dpkg only when apt is absent.
        QStringList args;
        const QString apt = QStandardPaths::findExecutable(QStringLiteral("apt-get"));
        const QString env = QStandardPaths::findExecutable(QStringLiteral("env"));
        if (!apt.isEmpty() && !env.isEmpty())
            args = {env, QStringLiteral("DEBIAN_FRONTEND=noninteractive"), apt, QStringLiteral("install"),
                    QStringLiteral("-y"), QStringLiteral("--allow-downgrades"), package};
        else
            args = {QStandardPaths::findExecutable(QStringLiteral("dpkg")), QStringLiteral("-i"), package};
        if (args.isEmpty() || args.first().isEmpty()) {
            emit failed(tr("No package manager was found to install %1.").arg(package));
            return;
        }

        process_ = new QProcess(this);
        process_->setProgram(pkexec);
        process_->setArguments(args);
        process_->setProcessChannelMode(QProcess::MergedChannels);
        process_->setStandardInputFile(QProcess::nullDevice());
        connect(process_, &QProcess::readyRead, this, &PackageInstaller::read_output);
        connect(process_, &QProcess::finished, this, &PackageInstaller::on_finished);
        connect(process_, &QProcess::errorOccurred, this, &PackageInstaller::on_error);

        LOG_INFO("UpdateInstaller", QStringLiteral("Installing the update: %1 %2").arg(pkexec, args.join(QLatin1Char(' '))));
        emit status(tr("Installing the update. Your system will ask for your password."));
        process_->start();
    }

  private:
    void read_output() {
        buffer_ += QString::fromLocal8Bit(process_->readAll());
        // Progress lines end in \r (apt's progress), others in \n.
        int cut = -1;
        for (int i = buffer_.size() - 1; i >= 0; --i) {
            if (buffer_.at(i) == QLatin1Char('\n') || buffer_.at(i) == QLatin1Char('\r')) {
                cut = i;
                break;
            }
        }
        if (cut < 0)
            return;
        const QStringList lines = buffer_.left(cut).split(QRegularExpression(QStringLiteral("[\r\n]")));
        buffer_ = buffer_.mid(cut + 1);
        for (const QString& raw : lines)
            note_line(raw);
    }

    void note_line(const QString& raw) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            return;
        last_lines_ << line;
        while (last_lines_.size() > kKeptOutputLines)
            last_lines_.removeFirst();
        LOG_DEBUG("UpdateInstaller", QStringLiteral("package manager: %1").arg(line));
        emit status(shorten(line));
    }

    void on_error(QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || done_)
            return;
        done_ = true;
        LOG_ERROR("UpdateInstaller", QStringLiteral("pkexec failed to start: %1").arg(process_->errorString()));
        emit failed(tr("The package manager could not be started (%1). Nothing has been changed.")
                        .arg(process_->errorString()));
    }

    void on_finished(int exit_code, QProcess::ExitStatus exit_status) {
        if (done_)
            return;
        done_ = true;
        if (!buffer_.trimmed().isEmpty())
            note_line(buffer_);
        buffer_.clear();

        if (exit_status != QProcess::NormalExit) {
            emit failed(tr("The package manager stopped unexpectedly. Your system may report details:\n%1")
                            .arg(last_lines_.join(QLatin1Char('\n'))));
            return;
        }
        // pkexec: 126 = the password dialog was dismissed, 127 = not
        // authorised / authentication failed. Nothing was installed.
        if (exit_code == 126) {
            emit failed(tr("The update was not installed because the password prompt was closed. Nothing has been "
                           "changed."));
            return;
        }
        if (exit_code == 127) {
            emit failed(tr("The update was not installed because the system did not allow it (wrong password or "
                           "no administrator rights). Nothing has been changed."));
            return;
        }
        if (exit_code != 0) {
            LOG_ERROR("UpdateInstaller", QStringLiteral("Package manager exited with %1:\n%2")
                                             .arg(exit_code)
                                             .arg(last_lines_.join(QLatin1Char('\n'))));
            emit failed(tr("The package manager could not install the update (exit code %1):\n%2")
                            .arg(exit_code)
                            .arg(last_lines_.join(QLatin1Char('\n'))));
            return;
        }

        LOG_INFO("UpdateInstaller", QStringLiteral("Package installed: version %1").arg(version_));
        // The edition's own package binary (InstallContext only reports a package
        // install under edition::kLinuxInstallPrefix) — never /usr/bin/FinceptTerminal,
        // which an older release's package may still own.
        const QString installed = context_.target_path + QStringLiteral("/bin/FinceptTerminal");
        const QString program = QFileInfo(installed).isExecutable() ? installed : QCoreApplication::applicationFilePath();
        if (!relaunch_after_exit(program, {})) {
            emit handed_off(tr("Fincept Terminal %1 is installed. Close Fincept Terminal and open it again to use "
                               "the new version.")
                                .arg(version_));
            return;
        }
        emit status(tr("Restarting Fincept Terminal…"));
        emit ready_to_restart();
    }

    QPointer<QProcess> process_;
    QString buffer_;
    QStringList last_lines_;
    QString version_;
    bool done_ = false;
};

// ── Flatpak ──────────────────────────────────────────────────────────────────

class FlatpakInstaller final : public UpdateInstaller {
  public:
    FlatpakInstaller(const InstallContext& context, QObject* parent) : UpdateInstaller(context, parent) {}

    void install(const QString& package_path, const UpdateInfo& info) override {
        const QString bundle = QFileInfo(package_path).absoluteFilePath();
        emit status(tr("Opening your system's software installer…"));
        // Through the OpenURI portal: GNOME Software / Discover / ... get
        // access to the bundle and install it outside the sandbox.
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(bundle))) {
            LOG_INFO("UpdateInstaller", QStringLiteral("Flatpak bundle %1 handed to the system").arg(bundle));
            emit handed_off(tr("Your system's software installer has been opened to finish the update to version "
                               "%1. When it is done, close Fincept Terminal and open it again.")
                                .arg(info.version));
            return;
        }
        LOG_ERROR("UpdateInstaller", QStringLiteral("No handler opened the Flatpak bundle %1").arg(bundle));
        emit failed(tr("Your system's software installer could not be opened. Install the update from a terminal "
                       "with:\nflatpak install --user %1")
                        .arg(bundle));
    }
};

} // namespace

UpdateInstaller* create_platform_installer(const InstallContext& context, QObject* parent) {
    switch (context.kind) {
        case InstallKind::LinuxDeb:
            return new PackageInstaller(context, parent);
        case InstallKind::LinuxFlatpak:
            return new FlatpakInstaller(context, parent);
        default:
            return nullptr;
    }
}

} // namespace fincept::services::updater
