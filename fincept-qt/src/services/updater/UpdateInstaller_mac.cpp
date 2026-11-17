// UpdateInstaller for macOS .app bundles (MacAppBundle).
//
//   1. hdiutil attach the downloaded .dmg (read-only, hidden, own mount point)
//      and find the one *.app at its root.
//   2. codesign --verify --deep --strict the new bundle; when the running
//      bundle has a Team ID, the new one must have the same.
//   3. ditto it to "<installed bundle>.update-<pid>" beside the installed one
//      (same volume), strip com.apple.quarantine, detach the image (always).
//   4. Hand-over: a detached /bin/sh (own session) waits until this process has
//      exited, renames the old bundle to "<bundle>.old-<pid>", the staged one
//      into place (moving the old one back if that fails), deletes the old one
//      and runs `open -n <bundle>` — whatever is there, so a version always
//      starts.
//
// When the folder holding the bundle is not writable by the user, steps 3-4's
// file work runs through `osascript ... with administrator privileges` (one
// password prompt): the same root shell stages the copy and starts the waiting
// swap, and a user-level waiter opens the app once the swap is done (never as
// root). The scripts are passed inline to `sh -c` with their paths as
// arguments — nothing executable sits in a user-writable file between the
// password prompt and its use.

#include "core/config/EditionIdentity.h"
#include "core/logging/Logger.h"
#include "services/updater/UpdateInstaller.h"
#include "services/updater/UpdateInstallerPlatform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>
#include <memory>

namespace fincept::services::updater {

namespace {

constexpr int kShortStepMs = 120 * 1000;
constexpr int kCopyStepMs = 15 * 60 * 1000;

/// POSIX shell single-quoted word.
QString sh_quote(const QString& s) {
    QString out = s;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + out + QLatin1Char('\'');
}

/// AppleScript string literal.
QString applescript_quote(const QString& s) {
    QString out = s;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

/// Positional arguments: $1 mode (swap | open | both), $2 pid to wait for,
/// $3 bundle, $4 staged bundle, $5 name for the old bundle, $6 marker file
/// ("" = none). "swap" replaces the bundle and touches the marker; "open"
/// waits for the marker, then opens the bundle; "both" does both.
const char* const kHandOverScript = R"SH(mode="$1"; pid="$2"; target="$3"; staged="$4"; old="$5"; marker="$6"
n=0
while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 300 ]; do sleep 0.2; n=$((n + 1)); done
if [ "$mode" != open ]; then
  if [ -d "$staged" ]; then
    if [ -e "$old" ]; then rm -rf "$old"; fi
    if mv "$target" "$old"; then
      if mv "$staged" "$target"; then rm -rf "$old"; else mv "$old" "$target"; rm -rf "$staged"; fi
    else
      rm -rf "$staged"
    fi
  fi
  if [ -n "$marker" ]; then : > "$marker"; fi
fi
if [ "$mode" != swap ]; then
  if [ "$mode" = open ] && [ -n "$marker" ]; then
    n=0
    while [ ! -e "$marker" ] && [ "$n" -lt 600 ]; do sleep 0.2; n=$((n + 1)); done
    rm -f "$marker"
  fi
  exec /usr/bin/open -n "$target"
fi
)SH";

class MacBundleInstaller final : public UpdateInstaller {
  public:
    MacBundleInstaller(const InstallContext& context, QObject* parent) : UpdateInstaller(context, parent) {}

    void install(const QString& package_path, const UpdateInfo& info) override {
        if (busy_)
            return;
        busy_ = true;
        version_ = info.version;
        dmg_ = QFileInfo(package_path).absoluteFilePath();
        target_ = QDir::cleanPath(context_.target_path);
        const QString pid = QString::number(QCoreApplication::applicationPid());
        staged_ = target_ + QStringLiteral(".update-") + pid;
        old_ = target_ + QStringLiteral(".old-") + pid;

        if (!QFileInfo(dmg_).isFile()) {
            fail(tr("The downloaded update could not be found:\n%1").arg(dmg_));
            return;
        }
        if (target_.isEmpty() || !QFileInfo(target_).isDir()) {
            fail(tr("The installed Fincept Terminal could not be found, so it cannot be replaced. Nothing has been "
                    "changed."));
            return;
        }
        QTemporaryDir mount_dir(QDir::tempPath() + QStringLiteral("/fincept-update-XXXXXX"));
        if (!mount_dir.isValid()) {
            fail(tr("No temporary folder could be created for the update. Nothing has been changed."));
            return;
        }
        mount_dir.setAutoRemove(false);
        mount_ = mount_dir.path();

        emit status(tr("Opening the downloaded update…"));
        run(QStringLiteral("/usr/bin/hdiutil"),
            {QStringLiteral("attach"), QStringLiteral("-nobrowse"), QStringLiteral("-readonly"),
             QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), mount_, dmg_},
            kShortStepMs, [this](int code, const QString& out) {
                if (code != 0) {
                    LOG_ERROR("UpdateInstaller", QStringLiteral("hdiutil attach failed (%1): %2").arg(code).arg(out));
                    QDir().rmdir(mount_);
                    mount_.clear();
                    fail(tr("The downloaded update could not be opened. Nothing has been changed."));
                    return;
                }
                mounted_ = true;
                find_bundle();
            });
    }

  private:
    using Done = std::function<void(int code, const QString& output)>;

    /// Runs `program args` asynchronously; `done` gets the exit code (-1 on
    /// crash or timeout, -2 when it did not start) and the merged output.
    void run(const QString& program, const QStringList& args, int timeout_ms, Done done) {
        auto* proc = new QProcess(this);
        proc->setProgram(program);
        proc->setArguments(args);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        proc->setStandardInputFile(QProcess::nullDevice());
        auto* timer = new QTimer(proc);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, proc, [proc, program]() {
            LOG_ERROR("UpdateInstaller", QStringLiteral("%1 timed out — stopping it").arg(program));
            proc->kill();
        });
        auto once = std::make_shared<Done>(std::move(done));
        connect(proc, &QProcess::finished, this, [proc, once](int code, QProcess::ExitStatus status) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            if (*once) {
                Done d = std::move(*once);
                *once = nullptr;
                d(status == QProcess::NormalExit ? code : -1, out);
            }
        });
        connect(proc, &QProcess::errorOccurred, this, [proc, once](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || !*once)
                return;
            const QString why = proc->errorString();
            proc->deleteLater();
            Done d = std::move(*once);
            *once = nullptr;
            d(-2, why);
        });
        timer->start(timeout_ms);
        proc->start();
    }

    void find_bundle() {
        const QStringList apps =
            QDir(mount_).entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        if (apps.size() != 1) {
            LOG_ERROR("UpdateInstaller", QStringLiteral("Expected one .app in the image, found: %1").arg(apps.join(QStringLiteral(", "))));
            fail(tr("The downloaded update does not contain the Fincept Terminal app. Nothing has been changed."));
            return;
        }
        new_app_ = QDir(mount_).filePath(apps.first());
        // The image must hold THIS edition (CFBundleIdentifier): both editions are
        // signed by the same team, so the Team ID check below cannot tell the
        // private build's app from this one, and installing it here would replace
        // this edition with the other.
        run(QStringLiteral("/usr/libexec/PlistBuddy"),
            {QStringLiteral("-c"), QStringLiteral("Print :CFBundleIdentifier"),
             new_app_ + QStringLiteral("/Contents/Info.plist")},
            kShortStepMs, [this](int code, const QString& out) {
                const QString id = out.trimmed();
                if (code != 0 || id != QLatin1String(edition::kBundleId)) {
                    LOG_ERROR("UpdateInstaller", QStringLiteral("Update bundle id '%1' (exit %2) is not this edition's %3")
                                                     .arg(id)
                                                     .arg(code)
                                                     .arg(QLatin1String(edition::kBundleId)));
                    fail(tr("The downloaded update is not this edition of Fincept Terminal, so it was not installed. "
                            "Nothing has been changed."));
                    return;
                }
                verify_signature();
            });
    }

    void verify_signature() {
        emit status(tr("Checking the update's signature…"));
        run(QStringLiteral("/usr/bin/codesign"),
            {QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), new_app_}, kCopyStepMs,
            [this](int code, const QString& out) {
                if (code != 0) {
                    LOG_ERROR("UpdateInstaller", QStringLiteral("codesign --verify failed (%1): %2").arg(code).arg(out));
                    fail(tr("The downloaded update does not carry a valid signature, so it was not installed. "
                            "Nothing has been changed."));
                    return;
                }
                read_team_id(target_, [this](const QString& running_team) {
                    read_team_id(new_app_, [this, running_team](const QString& new_team) {
                        check_team(running_team, new_team);
                    });
                });
            });
    }

    /// `codesign -dv` prints "TeamIdentifier=..." (to stderr; merged here).
    /// Empty when the bundle is unsigned, ad-hoc signed or has no team.
    void read_team_id(const QString& bundle, std::function<void(const QString&)> next) {
        run(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("-dv"), QStringLiteral("--verbose=2"), bundle},
            kShortStepMs, [next = std::move(next)](int code, const QString& out) {
                QString team;
                if (code == 0) {
                    const QStringList lines = out.split(QLatin1Char('\n'));
                    for (const QString& line : lines) {
                        if (line.startsWith(QLatin1String("TeamIdentifier=")))
                            team = line.mid(int(qstrlen("TeamIdentifier="))).trimmed();
                    }
                }
                if (team.compare(QLatin1String("not set"), Qt::CaseInsensitive) == 0)
                    team.clear();
                next(team);
            });
    }

    void check_team(const QString& running_team, const QString& new_team) {
        if (running_team.isEmpty()) {
            LOG_INFO("UpdateInstaller", QStringLiteral("Running bundle has no Team ID (unsigned or ad-hoc) — skipping "
                                                       "the Team ID comparison; new bundle team '%1'")
                                            .arg(new_team));
        } else if (new_team != running_team) {
            LOG_ERROR("UpdateInstaller",
                      QStringLiteral("Team ID mismatch: running %1, update %2").arg(running_team, new_team));
            fail(tr("The downloaded update was not signed by the publisher of this Fincept Terminal, so it was not "
                    "installed. Nothing has been changed."));
            return;
        } else {
            LOG_INFO("UpdateInstaller", QStringLiteral("Update signed by the same team (%1)").arg(new_team));
        }
        stage();
    }

    void stage() {
        const QString parent_dir = QFileInfo(target_).absolutePath();
        privileged_ = !QFileInfo(parent_dir).isWritable();
        emit status(tr("Copying the new version…"));
        if (!privileged_) {
            run(QStringLiteral("/bin/rm"), {QStringLiteral("-rf"), staged_}, kCopyStepMs, [this](int, const QString&) {
                run(QStringLiteral("/usr/bin/ditto"), {new_app_, staged_}, kCopyStepMs,
                    [this](int code, const QString& out) {
                        if (code != 0) {
                            LOG_ERROR("UpdateInstaller", QStringLiteral("ditto failed (%1): %2").arg(code).arg(out));
                            fail(tr("The new version could not be copied (%1). Nothing has been changed.").arg(out));
                            return;
                        }
                        staged_created_ = true;
                        run(QStringLiteral("/usr/bin/xattr"),
                            {QStringLiteral("-dr"), QStringLiteral("com.apple.quarantine"), staged_}, kShortStepMs,
                            [this](int, const QString&) { detach_then([this]() { hand_over(); }); });
                    });
            });
            return;
        }

        // Not writable by this user (e.g. /Applications for a standard
        // account): one administrator prompt stages the copy and starts the
        // root half of the hand-over, which waits for this process to exit.
        marker_ = QDir(QDir::tempPath()).filePath(QStringLiteral("fincept-update-%1.done")
                                                      .arg(QCoreApplication::applicationPid()));
        QFile::remove(marker_);
        const QString pid = QString::number(QCoreApplication::applicationPid());
        const QString swap = QStringLiteral("/usr/bin/nohup /bin/sh -c %1 fincept-update swap %2 %3 %4 %5 %6 "
                                            ">/dev/null 2>&1 &")
                                 .arg(sh_quote(QString::fromUtf8(kHandOverScript)), pid, sh_quote(target_),
                                      sh_quote(staged_), sh_quote(old_), sh_quote(marker_));
        const QString command = QStringLiteral("/bin/rm -rf %1 && /usr/bin/ditto %2 %1 || { /bin/rm -rf %1; exit 1; }; "
                                               "/usr/bin/xattr -dr com.apple.quarantine %1 2>/dev/null; %3")
                                    .arg(sh_quote(staged_), sh_quote(new_app_), swap);
        emit status(tr("Copying the new version. macOS will ask for an administrator password…"));
        run(QStringLiteral("/usr/bin/osascript"),
            {QStringLiteral("-e"),
             QStringLiteral("do shell script %1 with administrator privileges").arg(applescript_quote(command))},
            kCopyStepMs, [this](int code, const QString& out) {
                if (code != 0) {
                    LOG_ERROR("UpdateInstaller", QStringLiteral("Privileged staging failed (%1): %2").arg(code).arg(out));
                    const bool cancelled = out.contains(QLatin1String("-128"));
                    fail(cancelled ? tr("The update was not installed because the password prompt was closed. "
                                        "Nothing has been changed.")
                                   : tr("The new version could not be copied (%1). Nothing has been changed.").arg(out));
                    return;
                }
                // The root swap now waits for us; from here on the bundle is
                // replaced when we exit, whatever else happens.
                swap_armed_ = true;
                detach_then([this]() { hand_over(); });
            });
    }

    /// Detaches the image (forcing it if needed), removes the mount point,
    /// then continues with `next`. Called on every path once mounted.
    void detach_then(std::function<void()> next) {
        if (!mounted_) {
            if (!mount_.isEmpty())
                QDir().rmdir(mount_);
            next();
            return;
        }
        run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), mount_}, kShortStepMs,
            [this, next](int code, const QString& out) {
                if (code == 0) {
                    mounted_ = false;
                    QDir().rmdir(mount_);
                    next();
                    return;
                }
                LOG_WARN("UpdateInstaller", QStringLiteral("hdiutil detach failed (%1): %2 — forcing").arg(code).arg(out));
                run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), QStringLiteral("-force"), mount_},
                    kShortStepMs, [this, next](int, const QString&) {
                        mounted_ = false;
                        QDir().rmdir(mount_);
                        next();
                    });
            });
    }

    void hand_over() {
        const QString pid = QString::number(QCoreApplication::applicationPid());
        QProcess proc;
        proc.setProgram(QStringLiteral("/bin/sh"));
        proc.setArguments({QStringLiteral("-c"), QString::fromUtf8(kHandOverScript), QStringLiteral("fincept-update"),
                           privileged_ ? QStringLiteral("open") : QStringLiteral("both"), pid, target_, staged_, old_,
                           privileged_ ? marker_ : QString()});
        proc.setWorkingDirectory(QDir::homePath());
        proc.setStandardInputFile(QProcess::nullDevice());
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession |
                                      QProcess::UnixProcessFlag::CloseFileDescriptors |
                                      QProcess::UnixProcessFlag::ResetSignalHandlers);
        qint64 helper = 0;
        if (!proc.startDetached(&helper)) {
            LOG_ERROR("UpdateInstaller", QStringLiteral("Could not start the hand-over script: %1").arg(proc.errorString()));
            if (swap_armed_) {
                // The root half replaces the bundle when we exit anyway.
                busy_ = false;
                emit handed_off(tr("Fincept Terminal %1 will be installed when you close Fincept Terminal. Open it "
                                   "again afterwards.")
                                    .arg(version_));
                return;
            }
            fail(tr("The update could not be handed over (%1). Nothing has been changed.").arg(proc.errorString()));
            return;
        }
        LOG_INFO("UpdateInstaller", QStringLiteral("Hand-over pid %1 (%2): swaps %3 -> %4 once pid %5 exits")
                                        .arg(helper)
                                        .arg(privileged_ ? QStringLiteral("root swap + user open") : QStringLiteral("user"))
                                        .arg(staged_, target_, pid));
        emit status(tr("Restarting Fincept Terminal…"));
        emit ready_to_restart();
    }

    /// Fails the update: removes what was staged (never once the root swap
    /// is armed) and detaches the image first.
    void fail(const QString& message) {
        auto finish = [this, message]() {
            busy_ = false;
            emit failed(message);
        };
        auto cleanup = [this, finish]() {
            if (staged_created_ && !privileged_ && !swap_armed_) {
                staged_created_ = false;
                run(QStringLiteral("/bin/rm"), {QStringLiteral("-rf"), staged_}, kCopyStepMs,
                    [finish](int, const QString&) { finish(); });
                return;
            }
            finish();
        };
        detach_then(cleanup);
    }

    bool busy_ = false;
    bool mounted_ = false;
    bool privileged_ = false;
    bool staged_created_ = false;
    bool swap_armed_ = false;
    QString version_;
    QString dmg_;
    QString target_;
    QString staged_;
    QString old_;
    QString mount_;
    QString new_app_;
    QString marker_;
};

} // namespace

UpdateInstaller* create_platform_installer(const InstallContext& context, QObject* parent) {
    if (context.kind == InstallKind::MacAppBundle)
        return new MacBundleInstaller(context, parent);
    return nullptr;
}

} // namespace fincept::services::updater
