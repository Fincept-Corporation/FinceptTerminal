// UpdateInstaller for Qt Installer Framework installations (Windows).
//
// The new FinceptTerminal-<v>-windows-x64-setup.exe is started in its update
// mode — `FinceptUpdate=1 TargetDir=<install folder>` (IFW stores key=value
// arguments as installer values) — and this process then quits. The update
// mode itself lives in packaging/installer/config/controlscript.qs and
// packages/com.fincept.terminal.core/meta/installscript.qs: the installer
// window waits until the terminal has exited, asks for elevation when the
// folder needs it, moves the old folder aside, installs into the same folder,
// removes the old one (or puts it back on failure) and starts the new version.
//
// Before anything is started, the installer's Authenticode signature is
// checked against the running executable's: when this exe is signed, the
// installer must carry a valid signature from the same publisher.

#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "services/updater/UpdateInstaller.h"
#include "services/updater/UpdateInstallerPlatform.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <string>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>
// clang-format off: wintrust.h needs windows.h first
#include <shellapi.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <wintrust.h>
// clang-format on

namespace fincept::services::updater {

namespace {

constexpr int kPollIntervalMs = 250;
constexpr int kWindowWaitMs = 20000;

/// Result of an Authenticode check of one file.
struct SignatureInfo {
    LONG status = TRUST_E_NOSIGNATURE; ///< WinVerifyTrust result; ERROR_SUCCESS = valid and trusted
    QString publisher;                 ///< signer certificate's display name when valid
};

/// WinVerifyTrust (WINTRUST_ACTION_GENERIC_VERIFY_V2), no UI, no network
/// (revocation is not fetched: the manifest signature and the sha256 already
/// pin this exact file; this check proves who built it). Slow on a large
/// file — call it off the GUI thread.
SignatureInfo check_authenticode(const QString& path) {
    SignatureInfo info;
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();

    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = wpath.c_str();

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    info.status = WinVerifyTrust(nullptr, &action, &data);
    if (info.status == ERROR_SUCCESS) {
        CRYPT_PROVIDER_DATA* prov = WTHelperProvDataFromStateData(data.hWVTStateData);
        CRYPT_PROVIDER_SGNR* signer = prov ? WTHelperGetProvSignerFromChain(prov, 0, FALSE, 0) : nullptr;
        CRYPT_PROVIDER_CERT* cert = signer ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
        if (cert && cert->pCert) {
            wchar_t name[512] = {};
            const DWORD n = CertGetNameStringW(cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name,
                                               static_cast<DWORD>(std::size(name)));
            if (n > 1)
                info.publisher = QString::fromWCharArray(name);
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    return info;
}

QString status_hex(LONG status) {
    return QStringLiteral("0x%1").arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
}

struct VisibleWindowSearch {
    DWORD pid = 0;
    bool found = false;
};

BOOL CALLBACK find_visible_window(HWND hwnd, LPARAM param) {
    auto* search = reinterpret_cast<VisibleWindowSearch*>(param);
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner == search->pid && IsWindowVisible(hwnd)) {
        search->found = true;
        return FALSE;
    }
    return TRUE;
}

/// True once process `pid` shows a visible top-level window.
bool has_visible_window(DWORD pid) {
    VisibleWindowSearch search;
    search.pid = pid;
    EnumWindows(find_visible_window, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

QString process_image(HANDLE process) {
    wchar_t path[MAX_PATH * 2] = {};
    DWORD size = static_cast<DWORD>(std::size(path));
    if (!QueryFullProcessImageNameW(process, 0, path, &size))
        return {};
    return QString::fromWCharArray(path, static_cast<int>(size));
}

/// A running child of `parent` that runs the executable `image`, or 0.
/// IFW's installerbase is a console program; started in some console
/// situations it re-launches itself and the first process exits.
DWORD find_relaunched_child(DWORD parent, const QString& image) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    DWORD found = 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot, &entry); more && !found; more = Process32NextW(snapshot, &entry)) {
        if (entry.th32ParentProcessID != parent || entry.th32ProcessID == parent)
            continue;
        HANDLE child = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (!child)
            continue;
        if (process_image(child).compare(image, Qt::CaseInsensitive) == 0)
            found = entry.th32ProcessID;
        CloseHandle(child);
    }
    CloseHandle(snapshot);
    return found;
}

class WindowsIfwInstaller final : public UpdateInstaller {
  public:
    WindowsIfwInstaller(const InstallContext& context, QObject* parent) : UpdateInstaller(context, parent) {
        poll_.setInterval(kPollIntervalMs);
        connect(&poll_, &QTimer::timeout, this, &WindowsIfwInstaller::poll_installer);
    }

    ~WindowsIfwInstaller() override { close_process_handle(); }

    void install(const QString& package_path, const UpdateInfo& info) override {
        if (busy_) {
            LOG_WARN("UpdateInstaller", "install() called while an update is already being started — ignored");
            return;
        }
        busy_ = true;
        package_ = QFileInfo(package_path).absoluteFilePath();
        version_ = info.version;
        target_ = QDir::toNativeSeparators(QDir::cleanPath(context_.target_path));

        if (!QFileInfo(package_).isFile()) {
            finish_failed(tr("The downloaded installer could not be found:\n%1").arg(QDir::toNativeSeparators(package_)));
            return;
        }
        if (target_.isEmpty() || !QFileInfo(QDir(target_).filePath(QStringLiteral("FinceptMaintenanceTool.exe"))).isFile()) {
            finish_failed(tr("The installation folder of Fincept Terminal could not be found, so the update cannot "
                             "be applied. Nothing has been changed."));
            return;
        }

        emit status(tr("Checking the installer's signature…"));
        const QString own_exe = QCoreApplication::applicationFilePath();
        const QString package = package_;
        auto* watcher = new QFutureWatcher<std::pair<SignatureInfo, SignatureInfo>>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
            const auto [own, pkg] = watcher->result();
            watcher->deleteLater();
            on_signatures_checked(own, pkg);
        });
        watcher->setFuture(QtConcurrent::run([own_exe, package]() {
            return std::make_pair(check_authenticode(own_exe), check_authenticode(package));
        }));
    }

  private:
    void on_signatures_checked(const SignatureInfo& own, const SignatureInfo& pkg) {
        if (own.status != ERROR_SUCCESS) {
            // Development and CI builds without a certificate: nothing to
            // compare against. The manifest signature and sha256 still apply.
            LOG_INFO("UpdateInstaller",
                     QStringLiteral("Running executable has no valid Authenticode signature (%1) — skipping the "
                                    "installer signature check")
                         .arg(status_hex(own.status)));
        } else if (pkg.status != ERROR_SUCCESS) {
            LOG_ERROR("UpdateInstaller", QStringLiteral("Installer %1 failed Authenticode verification (%2) — not "
                                                        "starting it")
                                             .arg(package_, status_hex(pkg.status)));
            finish_failed(tr("The downloaded installer does not carry a valid Fincept signature, so it was not "
                             "started. Nothing has been changed."));
            return;
        } else if (pkg.publisher.compare(own.publisher, Qt::CaseInsensitive) != 0) {
            LOG_ERROR("UpdateInstaller", QStringLiteral("Installer is signed by '%1', the running executable by '%2' "
                                                        "— not starting it")
                                             .arg(pkg.publisher, own.publisher));
            finish_failed(tr("The downloaded installer is signed by \"%1\", not by the publisher of this Fincept "
                             "Terminal, so it was not started. Nothing has been changed.")
                              .arg(pkg.publisher));
            return;
        } else {
            LOG_INFO("UpdateInstaller", QStringLiteral("Installer signature valid, publisher '%1'").arg(pkg.publisher));
        }
        start_installer();
    }

    void start_installer() {
        emit status(tr("Starting the installer…"));
        QStringList args{QStringLiteral("FinceptUpdate=1"), QStringLiteral("TargetDir=") + target_};
        // The installer starts the new version itself: it must come back on
        // the profile this one runs (data and the single-instance key are per
        // profile; without it the new version would open on "default").
        const QString profile = ProfileManager::instance().active();
        if (!profile.isEmpty())
            args << QStringLiteral("FinceptProfile=") + profile;
        const QString work_dir = QDir::toNativeSeparators(QFileInfo(package_).absolutePath());

        QProcess proc;
        proc.setProgram(QDir::toNativeSeparators(package_));
        proc.setArguments(args);
        // Never the install folder: a process whose current directory is
        // inside it would stop the installer from moving that folder.
        proc.setWorkingDirectory(work_dir);
        // main.cpp sets QT_TLS_BACKEND for this process only; the installer
        // (and the new terminal it starts, which sets it again) must not
        // inherit it.
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QStringLiteral("QT_TLS_BACKEND"));
        proc.setProcessEnvironment(env);
        // installerbase is a console-subsystem program: give it its own
        // hidden console instead of ours (if we ever have one). Sharing a
        // console makes it re-launch itself, and no console window may flash.
        proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags &= ~static_cast<DWORD>(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
            cpa->flags |= CREATE_NO_WINDOW;
        });

        qint64 pid = 0;
        if (proc.startDetached(&pid) && pid > 0) {
            process_ = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        } else {
            // CreateProcess refuses an executable whose manifest asks for
            // elevation (ERROR_ELEVATION_REQUIRED); the shell asks for it.
            LOG_WARN("UpdateInstaller", QStringLiteral("CreateProcess failed for the installer (%1) — trying the shell")
                                            .arg(proc.errorString()));
            const std::wstring file = QDir::toNativeSeparators(package_).toStdWString();
            QString shell_params = QStringLiteral("FinceptUpdate=1 \"TargetDir=") + target_ + QStringLiteral("\"");
            if (!profile.isEmpty())
                shell_params += QStringLiteral(" \"FinceptProfile=") + profile + QStringLiteral("\"");
            const std::wstring params = shell_params.toStdWString();
            const std::wstring dir = work_dir.toStdWString();
            SHELLEXECUTEINFOW sei{};
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
            sei.lpVerb = L"open";
            sei.lpFile = file.c_str();
            sei.lpParameters = params.c_str();
            sei.lpDirectory = dir.c_str();
            sei.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&sei) || !sei.hProcess) {
                const DWORD error = GetLastError();
                LOG_ERROR("UpdateInstaller", QStringLiteral("Could not start the installer %1 (Windows error %2)")
                                                 .arg(package_)
                                                 .arg(error));
                finish_failed(tr("The installer could not be started (Windows error %1). Nothing has been changed.")
                                  .arg(error));
                return;
            }
            process_ = sei.hProcess;
            pid = static_cast<qint64>(GetProcessId(sei.hProcess));
        }
        pid_ = static_cast<DWORD>(pid);
        // The installer comes up while we still own the foreground; let it
        // take the foreground so its window is not hidden behind ours.
        AllowSetForegroundWindow(pid_);
        LOG_INFO("UpdateInstaller", QStringLiteral("Installer started: pid %1, %2 %3")
                                        .arg(pid)
                                        .arg(package_, args.join(QLatin1Char(' '))));
        since_.start();
        poll_.start();
    }

    /// Ready once the installer shows its window; failed when it exits first.
    void poll_installer() {
        if (process_ && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(process_, &code);
            const DWORD successor = find_relaunched_child(pid_, QDir::toNativeSeparators(package_));
            HANDLE next = successor ? OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, successor)
                                    : nullptr;
            if (next) {
                LOG_INFO("UpdateInstaller", QStringLiteral("Installer pid %1 exited (%2) after re-launching itself as "
                                                           "pid %3 — following it")
                                                .arg(pid_)
                                                .arg(code)
                                                .arg(successor));
                close_process_handle();
                process_ = next;
                pid_ = successor;
                AllowSetForegroundWindow(pid_);
                return;
            }
            poll_.stop();
            LOG_ERROR("UpdateInstaller", QStringLiteral("Installer exited before showing its window (exit code %1)")
                                             .arg(code));
            finish_failed(tr("The installer closed right after it started (exit code %1). Nothing has been changed.")
                              .arg(code));
            return;
        }
        const bool shown = has_visible_window(pid_);
        if (!shown && since_.elapsed() < kWindowWaitMs)
            return;
        poll_.stop();
        if (shown)
            LOG_INFO("UpdateInstaller", QStringLiteral("Installer window is up after %1 ms").arg(since_.elapsed()));
        else
            LOG_WARN("UpdateInstaller", QStringLiteral("Installer still running without a window after %1 ms — "
                                                       "handing over anyway")
                                            .arg(since_.elapsed()));
        emit status(tr("The installer is updating Fincept Terminal to version %1. Fincept Terminal closes now and "
                       "opens again when the update is done.")
                        .arg(version_));
        finish_ready();
    }

    void close_process_handle() {
        if (process_) {
            CloseHandle(process_);
            process_ = nullptr;
        }
    }

    void finish_ready() {
        close_process_handle();
        emit ready_to_restart();
    }

    void finish_failed(const QString& message) {
        close_process_handle();
        busy_ = false;
        emit failed(message);
    }

    QString package_;
    QString version_;
    QString target_;
    bool busy_ = false;
    HANDLE process_ = nullptr;
    DWORD pid_ = 0;
    QTimer poll_;
    QElapsedTimer since_;
};

} // namespace

UpdateInstaller* create_platform_installer(const InstallContext& context, QObject* parent) {
    if (context.kind == InstallKind::WindowsIfw)
        return new WindowsIfwInstaller(context, parent);
    return nullptr;
}

} // namespace fincept::services::updater
