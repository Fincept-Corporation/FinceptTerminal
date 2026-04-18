#include "services/updater/UpdateService.h"

#include "core/logging/Logger.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QAbstractButton>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

namespace fincept::services {

UpdateService& UpdateService::instance() {
    static UpdateService inst;
    return inst;
}

UpdateService::UpdateService(QObject* parent) : QObject(parent) {
    net_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    // GitHub release asset downloads redirect across origins — allow the jump.
    net_.setStrictTransportSecurityEnabled(true);
}

QWidget* UpdateService::dialog_parent() {
    if (dialog_parent_)
        return dialog_parent_.data();
    return QApplication::activeWindow();
}

// ── Platform / arch detection ───────────────────────────────────────────────

QString UpdateService::current_platform_key() {
    const QString arch = QSysInfo::currentCpuArchitecture();
    // QSysInfo returns "x86_64", "i386", "arm64", "arm" etc.
    const bool is_arm64 = arch.contains(QLatin1String("arm64")) ||
                          arch.contains(QLatin1String("aarch64"));
    const bool is_x64 = arch.contains(QLatin1String("x86_64")) ||
                        arch.contains(QLatin1String("x64"));

#if defined(Q_OS_WIN)
    if (is_arm64)
        return QStringLiteral("windows-arm64");
    if (is_x64)
        return QStringLiteral("windows-x64");
    return {};
#elif defined(Q_OS_MACOS)
    if (is_arm64)
        return QStringLiteral("macos-arm64");
    if (is_x64)
        return QStringLiteral("macos-x64");
    return {};
#elif defined(Q_OS_LINUX)
    if (is_arm64)
        return QStringLiteral("linux-arm64");
    if (is_x64)
        return QStringLiteral("linux-x64");
    return {};
#else
    return {};
#endif
}

// ── Version parsing ─────────────────────────────────────────────────────────

bool UpdateService::parse_version(const QString& s, int& major, int& minor, int& patch) {
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)$"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return false;
    bool ok_major = false, ok_minor = false, ok_patch = false;
    major = m.captured(1).toInt(&ok_major);
    minor = m.captured(2).toInt(&ok_minor);
    patch = m.captured(3).toInt(&ok_patch);
    return ok_major && ok_minor && ok_patch;
}

bool UpdateService::is_newer(const QString& local, const QString& remote) {
    int lmaj = 0, lmin = 0, lpat = 0;
    int rmaj = 0, rmin = 0, rpat = 0;
    if (!parse_version(local, lmaj, lmin, lpat))
        return false; // dev build or malformed — never treat as "update available"
    if (!parse_version(remote, rmaj, rmin, rpat))
        return false; // malformed manifest — reject
    if (rmaj != lmaj)
        return rmaj > lmaj;
    if (rmin != lmin)
        return rmin > lmin;
    return rpat > lpat;
}

// ── Public entry point ──────────────────────────────────────────────────────

void UpdateService::check_for_updates(bool silent) {
    if (in_progress_) {
        LOG_INFO("UpdateService", "Check already in progress — ignoring duplicate call");
        return;
    }
    if (silent && silent_check_done_) {
        LOG_DEBUG("UpdateService", "Silent check already completed this session — skipping");
        return;
    }
    if (silent)
        silent_check_done_ = true;

    silent_ = silent;
    update_available_ = false;
    latest_version_.clear();
    changelog_.clear();
    pending_download_url_.clear();
    pending_expected_sha256_.clear();

    // Skip the check for dev builds — their version string doesn't match the
    // manifest format and we'd otherwise never find a match.
    const QString local_version = QApplication::applicationVersion();
    int maj = 0, min = 0, pat = 0;
    if (!parse_version(local_version, maj, min, pat)) {
        LOG_INFO("UpdateService",
                 QString("Skipping update check — running version '%1' is not a release build").arg(local_version));
        if (!silent_)
            show_error(QStringLiteral("This is a development build (%1). Auto-update is disabled.").arg(local_version));
        emit check_finished(false);
        return;
    }

    const QString platform_key = current_platform_key();
    if (platform_key.isEmpty()) {
        LOG_WARN("UpdateService", QString("Unsupported platform/arch: %1 / %2")
                                      .arg(QSysInfo::kernelType(), QSysInfo::currentCpuArchitecture()));
        if (!silent_)
            show_error(QStringLiteral("Auto-update is not supported on this platform."));
        emit check_finished(false);
        return;
    }

    in_progress_ = true;
    LOG_INFO("UpdateService",
             QString("Checking for updates — platform=%1, local=%2, manifest=%3")
                 .arg(platform_key, local_version, manifest_url_));

    QNetworkRequest req{QUrl(manifest_url_)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("FinceptTerminal/%1 (%2)").arg(local_version, platform_key));
    QNetworkReply* reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, &UpdateService::on_manifest_reply_finished);
}

// ── Manifest response ───────────────────────────────────────────────────────

void UpdateService::on_manifest_reply_finished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN("UpdateService",
                 QString("Manifest fetch failed: %1 (%2)").arg(reply->errorString()).arg(reply->error()));
        if (!silent_)
            show_error(QStringLiteral("Could not reach the update server.\n\n%1").arg(reply->errorString()));
        finish_check(false);
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError parse_err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN("UpdateService", QString("Manifest JSON parse failed: %1").arg(parse_err.errorString()));
        if (!silent_)
            show_error(QStringLiteral("The update manifest is malformed."));
        finish_check(false);
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject updates = root.value(QStringLiteral("updates")).toObject();
    const QString key = current_platform_key();
    if (!updates.contains(key)) {
        LOG_INFO("UpdateService",
                 QString("Manifest has no entry for '%1' — no update available").arg(key));
        finish_check(false);
        return;
    }

    const QJsonObject entry = updates.value(key).toObject();
    const QString remote_version = entry.value(QStringLiteral("latest-version")).toString();
    const QString download_url = entry.value(QStringLiteral("download-url")).toString();
    const QString sha256 = entry.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    const QString open_url = entry.value(QStringLiteral("open-url")).toString();
    const QString changelog = entry.value(QStringLiteral("changelog")).toString();

    latest_version_ = remote_version;
    changelog_ = changelog;

    if (remote_version.isEmpty() || download_url.isEmpty()) {
        LOG_WARN("UpdateService",
                 QString("Manifest entry for '%1' is missing latest-version or download-url").arg(key));
        finish_check(false);
        return;
    }

    const QString local_version = QApplication::applicationVersion();
    if (!is_newer(local_version, remote_version)) {
        LOG_INFO("UpdateService",
                 QString("Already up to date — local=%1, remote=%2").arg(local_version, remote_version));
        if (!silent_) {
            QMessageBox::information(
                dialog_parent(), QStringLiteral("Fincept Terminal"),
                QStringLiteral("You're running the latest version (%1).").arg(local_version));
        }
        finish_check(false);
        return;
    }

    update_available_ = true;
    LOG_INFO("UpdateService",
             QString("Update available: %1 → %2").arg(local_version, remote_version));

    // Prompt the user. Include changelog if present. Always offer a "view release
    // notes" escape hatch via open-url so users can read more before installing.
    QString prompt = QStringLiteral("A new version of Fincept Terminal is available.\n\n"
                                    "Current version: %1\nLatest version:  %2\n\n")
                         .arg(local_version, remote_version);
    if (!changelog.isEmpty()) {
        QString snippet = changelog;
        if (snippet.size() > 500)
            snippet = snippet.left(500) + QStringLiteral("\n…");
        prompt += QStringLiteral("What's new:\n%1\n\n").arg(snippet);
    }
    prompt += QStringLiteral("Download the installer now?");

    QMessageBox box(dialog_parent());
    box.setWindowTitle(QStringLiteral("Update Available"));
    box.setIcon(QMessageBox::Information);
    box.setText(prompt);
    QPushButton* install_btn = box.addButton(QStringLiteral("Download && Install"), QMessageBox::AcceptRole);
    QPushButton* release_btn = open_url.isEmpty()
                                   ? nullptr
                                   : box.addButton(QStringLiteral("View Release Notes"), QMessageBox::HelpRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(install_btn);
    box.exec();

    QAbstractButton* clicked = box.clickedButton();
    if (clicked == install_btn) {
        pending_expected_sha256_ = sha256;
        start_download(download_url, sha256);
    } else if (release_btn && clicked == release_btn) {
        QDesktopServices::openUrl(QUrl(open_url));
        finish_check(true);
    } else {
        LOG_INFO("UpdateService", "User declined the update");
        finish_check(true);
    }
}

// ── Download + verify ───────────────────────────────────────────────────────

void UpdateService::start_download(const QString& url, const QString& expected_sha256) {
    pending_download_url_ = url;

    // Build a stable local path in the system temp dir, preserving the
    // installer's basename so the user recognises it.
    const QString file_name = QFileInfo(QUrl(url).path()).fileName();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    pending_local_path_ = QDir(dir).filePath(file_name.isEmpty()
                                                 ? QStringLiteral("FinceptTerminal-update.tmp")
                                                 : file_name);

    // Drop any leftover from a previous run — otherwise a partial file could
    // survive and poison sha256 verification.
    QFile::remove(pending_local_path_);

    LOG_INFO("UpdateService",
             QString("Downloading installer: %1 → %2 (expected sha256=%3)")
                 .arg(url, pending_local_path_, expected_sha256.isEmpty() ? QStringLiteral("<none>") : expected_sha256));

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("FinceptTerminal/%1").arg(QApplication::applicationVersion()));
    QNetworkReply* reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, &UpdateService::on_download_reply_finished);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateService::on_download_progress);
}

void UpdateService::on_download_progress(qint64 received, qint64 total) {
    // Log every ~5 MiB to avoid spamming. No GUI progress for v1 — keep the
    // surface area small. A proper progress dialog can be layered on later.
    static qint64 last_logged = 0;
    if (received - last_logged >= 5 * 1024 * 1024 || received == total) {
        last_logged = received;
        LOG_DEBUG("UpdateService",
                  QString("Download progress: %1 / %2 bytes").arg(received).arg(total));
    }
}

void UpdateService::on_download_reply_finished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("UpdateService", QString("Installer download failed: %1").arg(reply->errorString()));
        show_error(QStringLiteral("The installer could not be downloaded.\n\n%1").arg(reply->errorString()));
        QFile::remove(pending_local_path_);
        finish_check(true);
        return;
    }

    // Write body to disk.
    QFile f(pending_local_path_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR("UpdateService", QString("Cannot open %1 for writing: %2").arg(pending_local_path_, f.errorString()));
        show_error(QStringLiteral("Cannot save the installer to disk:\n%1").arg(f.errorString()));
        finish_check(true);
        return;
    }
    const QByteArray body = reply->readAll();
    f.write(body);
    f.close();

    // Verify sha256 if the manifest provided one. Missing sha256 logs a
    // warning but does not abort — the server may be a pre-sha256 deployment.
    if (!pending_expected_sha256_.isEmpty()) {
        QFile vf(pending_local_path_);
        if (!vf.open(QIODevice::ReadOnly)) {
            LOG_ERROR("UpdateService", QString("Cannot reopen %1 for hashing").arg(pending_local_path_));
            show_error(QStringLiteral("Could not verify the downloaded installer."));
            QFile::remove(pending_local_path_);
            finish_check(true);
            return;
        }
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        if (!hasher.addData(&vf)) {
            vf.close();
            LOG_ERROR("UpdateService", "Sha256 hashing failed");
            show_error(QStringLiteral("Could not verify the downloaded installer."));
            QFile::remove(pending_local_path_);
            finish_check(true);
            return;
        }
        vf.close();
        const QString actual = QString::fromLatin1(hasher.result().toHex()).toLower();
        if (actual != pending_expected_sha256_) {
            LOG_ERROR("UpdateService",
                      QString("Sha256 mismatch — expected=%1, actual=%2").arg(pending_expected_sha256_, actual));
            show_error(QStringLiteral("The downloaded installer failed integrity verification. "
                                      "It may be corrupt or tampered with."));
            QFile::remove(pending_local_path_);
            finish_check(true);
            return;
        }
        LOG_INFO("UpdateService", QString("Sha256 verified: %1").arg(actual));
    } else {
        LOG_WARN("UpdateService", "No sha256 in manifest — skipping integrity verification");
    }

    emit download_finished(pending_local_path_);

    // Hand off: on platforms where we can launch the installer (and the user
    // will be prompted by the OS anyway), do so. Otherwise reveal the file.
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    launch_installer(pending_local_path_);
#else
    reveal_in_file_manager(pending_local_path_);
#endif

    finish_check(true);
}

// ── Launch / reveal ─────────────────────────────────────────────────────────

void UpdateService::launch_installer(const QString& path) {
    LOG_INFO("UpdateService", QString("Launching installer: %1").arg(path));

#ifdef Q_OS_LINUX
    // Linux installers (.run) need the executable bit set before they can be
    // launched. QDesktopServices::openUrl on a non-executable .run just hands
    // it to the file manager, which is not what the user expects after
    // clicking "Install".
    QFile f(path);
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser |
                     QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif

    // startDetached is the right call here — we want the installer to outlive
    // this process so the user can restart and finish installation.
    const bool started = QProcess::startDetached(path, {});
    if (!started) {
        LOG_WARN("UpdateService", "startDetached failed — falling back to reveal-in-file-manager");
        reveal_in_file_manager(path);
        QMessageBox::information(
            dialog_parent(), QStringLiteral("Update Downloaded"),
            QStringLiteral("The installer has been downloaded to:\n%1\n\n"
                           "Please run it manually to complete the update.")
                .arg(QDir::toNativeSeparators(path)));
    }
}

void UpdateService::reveal_in_file_manager(const QString& path) {
    const QUrl url = QUrl::fromLocalFile(QFileInfo(path).absolutePath());
    QDesktopServices::openUrl(url);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void UpdateService::finish_check(bool update_found) {
    in_progress_ = false;
    emit check_finished(update_found);
}

void UpdateService::show_error(const QString& text) {
    QMessageBox::warning(dialog_parent(), QStringLiteral("Fincept Terminal"), text);
}

} // namespace fincept::services
