#include "services/updater/UpdateService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "services/updater/InstallContext.h"
#include "services/updater/UpdateDownloader.h"
#include "services/updater/UpdateInstaller.h"
#include "services/updater/UpdateManifest.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStorageInfo>
#include <QUrl>

// Update-manifest signing (runbook for the release side):
//
//   The manifest is trusted only with a valid detached Ed25519 signature made
//   with the key whose public half is UPDATE_SIGNING_PUBLIC_KEY_HEX
//   (UpdateManifest.h). Sign the exact bytes that are published:
//     openssl pkeyutl -sign -rawin -inkey <key> -in updates.json -out sig.bin
//     base64 -w0 sig.bin > updates.json.sig
//   Any change to updates.json after signing (reformatting, a trailing newline,
//   CRLF conversion) invalidates the signature. Key rotation: ship a build with
//   the new public key first, then start signing with the new private key.

namespace fincept::services {

namespace {

constexpr const char* kSvcLatestReleaseManifestUrl =
    "https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest/download/updates.json";
constexpr const char* kSvcMainBranchManifestUrl =
    "https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/updates.json";
constexpr const char* kSvcReleasesPageUrl = "https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest";
constexpr const char* kSvcTooLargeProperty = "fincept_update_reply_too_large";

constexpr int kSvcManifestTimeoutMs = 20000;
constexpr qint64 kSvcMaxManifestBytes = 512 * 1024;
constexpr qint64 kSvcMaxSignatureBytes = 4096;
constexpr int kSvcRecheckIntervalMs = 6 * 60 * 60 * 1000;
constexpr int kSvcRestartDelayMs = 1200;
constexpr qint64 kSvcDiskHeadroomBytes = 200LL * 1024 * 1024;

QString svc_stage_name(updater::UpdateStage stage) {
    switch (stage) {
        case updater::UpdateStage::Idle:
            return QStringLiteral("Idle");
        case updater::UpdateStage::Checking:
            return QStringLiteral("Checking");
        case updater::UpdateStage::UpToDate:
            return QStringLiteral("UpToDate");
        case updater::UpdateStage::Available:
            return QStringLiteral("Available");
        case updater::UpdateStage::Downloading:
            return QStringLiteral("Downloading");
        case updater::UpdateStage::Verifying:
            return QStringLiteral("Verifying");
        case updater::UpdateStage::Installing:
            return QStringLiteral("Installing");
        case updater::UpdateStage::Restarting:
            return QStringLiteral("Restarting");
        case updater::UpdateStage::Failed:
            return QStringLiteral("Failed");
    }
    return QStringLiteral("?");
}

/// Messages can carry a second line of detail; logs want one line.
QString svc_one_line(const QString& text) {
    QString out = text;
    out.replace(QLatin1Char('\n'), QLatin1String(" | "));
    return out;
}

QString svc_manifest_override() {
    return qEnvironmentVariable("FINCEPT_UPDATE_MANIFEST_URL").trimmed();
}

/// Plain http asset URLs to this machine are accepted only while testing
/// against a local server named by FINCEPT_UPDATE_MANIFEST_URL.
bool svc_allow_loopback_http() {
    const QString override_url = svc_manifest_override();
    if (override_url.isEmpty())
        return false;
    const QString host = QUrl(override_url).host();
    return host == QLatin1String("127.0.0.1") || host == QLatin1String("::1") ||
           host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0;
}

QString svc_user_agent() {
    return QStringLiteral("FinceptTerminal/%1").arg(QCoreApplication::applicationVersion());
}

QNetworkRequest svc_small_request(const QString& url) {
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setHeader(QNetworkRequest::UserAgentHeader, svc_user_agent());
    request.setTransferTimeout(kSvcManifestTimeoutMs);
    return request;
}

/// Abort `reply` once it has delivered more than `limit` bytes: the manifest
/// and its signature are small, and nothing is read before the signature is
/// checked, so an oversized answer is never buffered whole.
void svc_cap_reply_size(QNetworkReply* reply, qint64 limit) {
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [reply, limit](qint64 received, qint64) {
        if (received > limit && reply->isRunning()) {
            reply->setProperty(kSvcTooLargeProperty, true);
            reply->abort();
        }
    });
}

/// Classify a finished small request. False with both reasons filled when it
/// did not produce a usable 200 answer.
bool svc_reply_ok(QNetworkReply* reply, QString& log_reason, QString& user_reason) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->property(kSvcTooLargeProperty).toBool()) {
        log_reason = QStringLiteral("answer was larger than allowed");
        user_reason = UpdateService::tr("The update server sent an unexpected answer. Please try again later.");
        return false;
    }
    if (reply->error() != QNetworkReply::NoError && status == 0) {
        log_reason = QStringLiteral("network error %1: %2").arg(QString::number(int(reply->error())), reply->errorString());
        user_reason = UpdateService::tr("Could not reach the update server. Check your internet connection and try again.") +
                      QLatin1Char('\n') + reply->errorString();
        return false;
    }
    if (status == 404) {
        // Nothing published at this address (e.g. no release carries update
        // information yet). Not an error the user can act on — say so calmly.
        log_reason = QStringLiteral("HTTP 404 — nothing published at this address");
        user_reason = UpdateService::tr("No update information is available right now. New versions are also "
                                        "listed on the download page.");
        return false;
    }
    if (status != 200) {
        log_reason = QStringLiteral("HTTP %1 (%2)").arg(QString::number(status), reply->errorString());
        user_reason =
            UpdateService::tr("The update server is not answering correctly right now. Please try again later.") +
            QLatin1Char('\n') + UpdateService::tr("Server response: %1").arg(status);
        return false;
    }
    return true;
}

/// Delete everything inside the private updates folder (never the folder's
/// parents; symlinks are removed, not followed).
void svc_clear_dir(const QString& dir_path) {
    const QDir dir(dir_path);
    if (!dir.exists())
        return;
    const QFileInfoList entries =
        dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString path = entry.absoluteFilePath();
        const bool removed = (entry.isDir() && !entry.isSymLink()) ? QDir(path).removeRecursively()
                                                                   : QFile::remove(path);
        if (removed)
            LOG_INFO("UpdateService", QStringLiteral("Removed leftover update file %1").arg(path));
        else
            LOG_WARN("UpdateService", QStringLiteral("Could not remove leftover update file %1").arg(path));
    }
}

/// The release file's own name (the installers care about the extension),
/// reduced to characters that are safe on every file system.
QString svc_download_file_name(const QString& url) {
    QString name = QFileInfo(QUrl(url).path()).fileName();
    for (QChar& c : name) {
        const bool safe = (c.unicode() < 0x80 && c.isLetterOrNumber()) || c == QLatin1Char('.') ||
                          c == QLatin1Char('-') || c == QLatin1Char('_') || c == QLatin1Char('+');
        if (!safe)
            c = QLatin1Char('_');
    }
    if (name.isEmpty())
        return QStringLiteral("FinceptTerminal-update.bin");
    if (name.startsWith(QLatin1Char('.')))
        name.prepend(QStringLiteral("FinceptTerminal-update"));
    return name;
}

QString svc_available_text(const QString& version) {
    return UpdateService::tr("Fincept Terminal %1 is available (you have %2).")
        .arg(version, QCoreApplication::applicationVersion());
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

UpdateService& UpdateService::instance() {
    static UpdateService inst;
    return inst;
}

UpdateService::UpdateService(QObject* parent) : QObject(parent) {
    qRegisterMetaType<updater::UpdateStage>();

    // GitHub serves release assets through a redirect to another host: follow
    // it, but never from https down to http.
    net_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    net_.setStrictTransportSecurityEnabled(true);

    recheck_timer_.setInterval(kSvcRecheckIntervalMs);
    recheck_timer_.setTimerType(Qt::VeryCoarseTimer);
    connect(&recheck_timer_, &QTimer::timeout, this, [this] {
        using S = updater::UpdateStage;
        const bool quiet_stage =
            stage_ == S::Idle || stage_ == S::UpToDate || (stage_ == S::Failed && !failed_during_update_);
        if (!quiet_stage) {
            LOG_DEBUG("UpdateService",
                      QStringLiteral("Periodic update check skipped (stage %1)").arg(svc_stage_name(stage_)));
            return;
        }
        LOG_INFO("UpdateService", "Periodic update check");
        check_for_updates(false);
    });
}

// ── Checking ────────────────────────────────────────────────────────────────

void UpdateService::check_for_updates(bool user_initiated) {
    using S = updater::UpdateStage;
    switch (stage_) {
        case S::Checking:
            if (user_initiated && !user_initiated_) {
                user_initiated_ = true;
                LOG_INFO("UpdateService", "Update check requested while a background check is running — "
                                          "that check will now report its result");
            } else {
                LOG_DEBUG("UpdateService", "Update check already running — ignoring the duplicate request");
            }
            return;
        case S::Downloading:
        case S::Verifying:
        case S::Installing:
        case S::Restarting:
            LOG_INFO("UpdateService",
                     QStringLiteral("Update check ignored — an update is in progress (%1)").arg(svc_stage_name(stage_)));
            return;
        case S::Available:
            // The newer release is already known and waiting for the user.
            LOG_INFO("UpdateService",
                     QStringLiteral("Update check skipped — version %1 is already known to be available")
                         .arg(info_.version));
            emit check_finished(true);
            return;
        case S::Idle:
        case S::UpToDate:
        case S::Failed:
            break;
    }

    user_initiated_ = user_initiated;
    failed_during_update_ = false;
    error_text_.clear();
    info_ = {};

    const QString local = QCoreApplication::applicationVersion();
    if (!update_checks_available()) {
        LOG_INFO("UpdateService",
                 QStringLiteral("Update check skipped — '%1' is a development build, not a release").arg(local));
        fail_check(tr("This is a development build of Fincept Terminal (%1), so it cannot check for updates. "
                      "Released versions are published on the releases page.")
                       .arg(local));
        return;
    }

    // Re-check every few hours while the terminal stays open.
    if (!recheck_timer_.isActive())
        recheck_timer_.start();

    const updater::InstallContext& ctx = updater::install_context();
    const QStringList urls = manifest_urls();
    LOG_INFO("UpdateService",
             QStringLiteral("Checking for updates (%1) — running %2, package %3, %4 source(s)%5")
                 .arg(user_initiated ? QStringLiteral("requested by the user") : QStringLiteral("background"), local,
                      ctx.asset_key.isEmpty() ? QStringLiteral("<none>") : ctx.asset_key,
                      QString::number(urls.size()),
                      svc_manifest_override().isEmpty() ? QString()
                                                        : QStringLiteral(" (FINCEPT_UPDATE_MANIFEST_URL override)")));
    set_stage(S::Checking, tr("Checking for updates…"));
    fetch_manifest(0);
}

QStringList UpdateService::manifest_urls() const {
    const QString override_url = svc_manifest_override();
    if (!override_url.isEmpty())
        return {override_url};
    return {QString::fromLatin1(kSvcLatestReleaseManifestUrl), QString::fromLatin1(kSvcMainBranchManifestUrl)};
}

void UpdateService::fetch_manifest(int source_index) {
    const QStringList urls = manifest_urls();
    if (source_index < 0 || source_index >= urls.size()) {
        LOG_ERROR("UpdateService", QStringLiteral("No update source #%1").arg(source_index));
        fail_check(tr("Could not reach the update server. Check your internet connection and try again."));
        return;
    }
    const QString url = urls.at(source_index);
    LOG_DEBUG("UpdateService", QStringLiteral("Fetching update information %1/%2: %3")
                                  .arg(QString::number(source_index + 1), QString::number(urls.size()), url));
    QNetworkReply* reply = net_.get(svc_small_request(url));
    svc_cap_reply_size(reply, kSvcMaxManifestBytes);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, source_index] { on_manifest_reply(reply, source_index); });
}

void UpdateService::on_manifest_reply(QNetworkReply* reply, int source_index) {
    reply->deleteLater();
    if (stage_ != updater::UpdateStage::Checking) {
        LOG_DEBUG("UpdateService", "Ignoring an update-information answer that arrived after the check ended");
        return;
    }

    QString log_reason;
    QString user_reason;
    if (!svc_reply_ok(reply, log_reason, user_reason)) {
        manifest_source_failed(source_index, QStringLiteral("manifest: ") + log_reason, user_reason);
        return;
    }

    // Held as raw bytes: NOTHING in it is read until the signature verifies.
    const QByteArray manifest = reply->readAll();
    if (manifest.isEmpty()) {
        manifest_source_failed(source_index, QStringLiteral("manifest: empty answer"),
                               tr("The update server sent an empty answer. Please try again later."));
        return;
    }

    // The signature sits beside the manifest at the URL that was asked for
    // (not the redirect target, which is a short-lived storage link).
    const QString sig_url = manifest_urls().value(source_index) + QStringLiteral(".sig");
    LOG_DEBUG("UpdateService", QStringLiteral("Received %1 bytes of update information; fetching its signature %2")
                                   .arg(QString::number(manifest.size()), sig_url));
    QNetworkReply* sig_reply = net_.get(svc_small_request(sig_url));
    svc_cap_reply_size(sig_reply, kSvcMaxSignatureBytes);
    connect(sig_reply, &QNetworkReply::finished, this,
            [this, sig_reply, source_index, manifest] { on_signature_reply(sig_reply, source_index, manifest); });
}

void UpdateService::on_signature_reply(QNetworkReply* reply, int source_index, const QByteArray& manifest) {
    reply->deleteLater();
    if (stage_ != updater::UpdateStage::Checking) {
        LOG_DEBUG("UpdateService", "Ignoring a signature answer that arrived after the check ended");
        return;
    }

    QString log_reason;
    QString user_reason;
    if (!svc_reply_ok(reply, log_reason, user_reason)) {
        // A missing signature is a hard failure for this source — never a
        // reason to use the manifest unsigned. To the user a 404 here reads as
        // "no update information available" (svc_reply_ok words it that way).
        manifest_source_failed(source_index, QStringLiteral("signature: ") + log_reason, user_reason);
        return;
    }

    const QByteArray signature = reply->readAll();
    if (!updater::verify_manifest_signature(manifest, signature)) {
        manifest_source_failed(
            source_index,
            QStringLiteral("signature does NOT verify against the pinned key (manifest %1 bytes, signature %2 bytes)")
                .arg(QString::number(manifest.size()), QString::number(signature.size())),
            tr("The update information did not pass the authenticity check, so it was ignored. "
               "Nothing was downloaded."));
        return;
    }
    LOG_INFO("UpdateService", QStringLiteral("Update information from source %1 is authentic (signature verified)")
                                  .arg(source_index + 1));

    // Well-formedness decides whether this source is usable at all; the
    // decision itself is made in process_verified_manifest().
    const updater::ManifestResult parsed =
        updater::parse_manifest(manifest, updater::install_context().asset_key, svc_allow_loopback_http());
    if (!parsed.info) {
        manifest_source_failed(source_index, QStringLiteral("manifest rejected: ") + parsed.error,
                               tr("The update information could not be read. Please try again later."));
        return;
    }
    process_verified_manifest(manifest);
}

void UpdateService::manifest_source_failed(int source_index, const QString& log_reason, const QString& user_reason) {
    const QStringList urls = manifest_urls();
    // HTTP 404 = nothing published at that address: expected, not a fault (svc_reply_ok words it so).
    const bool not_published = log_reason.contains(QLatin1String("HTTP 404"));
    if (source_index == 0)
        every_source_404_ = true;
    if (!not_published)
        every_source_404_ = false;
    const QString line = logf("Update source unavailable",
                              {{"source", QStringLiteral("%1/%2").arg(source_index + 1).arg(urls.size())},
                               {"url", log_url(urls.value(source_index))},
                               {"error", log_reason}});
    if (not_published)
        LOG_DEBUG("UpdateService", line);
    else
        LOG_INFO("UpdateService", line);
    if (stage_ != updater::UpdateStage::Checking)
        return;
    if (source_index + 1 < urls.size()) {
        fetch_manifest(source_index + 1);
        return;
    }
    if (every_source_404_)
        LOG_INFO("UpdateService", logf("No update information published", {{"sources", urls.size()}, {"status", 404}}));
    else
        LOG_WARN("UpdateService", logf("Update check failed", {{"sources", urls.size()}, {"last_error", log_reason}}));
    fail_check(user_reason);
}

void UpdateService::process_verified_manifest(const QByteArray& manifest) {
    using S = updater::UpdateStage;
    const updater::InstallContext& ctx = updater::install_context();
    const updater::ManifestResult parsed = updater::parse_manifest(manifest, ctx.asset_key, svc_allow_loopback_http());
    if (!parsed.info) {
        LOG_ERROR("UpdateService", QStringLiteral("Verified manifest rejected: %1").arg(parsed.error));
        fail_check(tr("The update information could not be read. Please try again later."));
        return;
    }

    const QString local = QCoreApplication::applicationVersion();
    const updater::UpdateInfo& remote = *parsed.info;
    if (!updater::is_newer_version(local, remote.version)) {
        info_ = {};
        LOG_INFO("UpdateService",
                 QStringLiteral("Up to date — running %1, latest release %2").arg(local, remote.version));
        set_stage(S::UpToDate, tr("Fincept Terminal is up to date (version %1).").arg(local));
        emit check_finished(false);
        return;
    }

    info_ = remote;
    if (info_.has_asset()) {
        LOG_INFO("UpdateService", QStringLiteral("Update available: %1 → %2; package %3 (%4 bytes, sha256 %5)")
                                      .arg(local, info_.version, info_.asset.url,
                                           QString::number(info_.asset.size), info_.asset.sha256));
    } else {
        QString entry = QStringLiteral("no entry");
        if (!ctx.asset_key.isEmpty()) {
            const QJsonValue raw = QJsonDocument::fromJson(manifest)
                                       .object()
                                       .value(QLatin1String("assets"))
                                       .toObject()
                                       .value(ctx.asset_key);
            if (raw.isObject())
                entry = QStringLiteral("entry rejected: ") +
                        QString::fromUtf8(QJsonDocument(raw.toObject()).toJson(QJsonDocument::Compact));
        }
        LOG_WARN("UpdateService", QStringLiteral("Update available: %1 → %2, but the release has no usable package "
                                                 "for '%3' (%4)")
                                      .arg(local, info_.version,
                                           ctx.asset_key.isEmpty() ? QStringLiteral("<none>") : ctx.asset_key, entry));
    }
    if (!can_self_update())
        LOG_INFO("UpdateService", QStringLiteral("This installation cannot update itself: %1").arg(blocked_reason()));

    set_stage(S::Available, svc_available_text(info_.version));
    emit check_finished(true);
}

bool UpdateService::can_self_update() const {
    return updater::install_context().can_self_update && info_.has_asset();
}

QString UpdateService::blocked_reason() const {
    const updater::InstallContext& ctx = updater::install_context();
    if (!ctx.can_self_update)
        return ctx.blocked_reason;
    if (!info_.has_asset())
        return tr("This release has no package for this kind of installation, so it cannot be installed "
                  "automatically. Download it from the releases page instead.");
    return {};
}

bool UpdateService::update_checks_available() {
    int major = 0, minor = 0, patch = 0;
    return updater::parse_version(QCoreApplication::applicationVersion(), major, minor, patch);
}

QString UpdateService::releases_page_url() {
    return QString::fromLatin1(kSvcReleasesPageUrl);
}

// ── Updating ────────────────────────────────────────────────────────────────

void UpdateService::start_update() {
    using S = updater::UpdateStage;
    const bool retry = stage_ == S::Failed && failed_during_update_;
    if (stage_ != S::Available && !retry) {
        LOG_WARN("UpdateService", logf("Update not started: another update step is under way",
                                       {{"stage", svc_stage_name(stage_)}}));
        return;
    }
    if (!update_available()) {
        LOG_WARN("UpdateService", logf("Update not started: no newer release is known", {{"stage", svc_stage_name(stage_)}}));
        return;
    }
    LOG_INFO("UpdateService", QStringLiteral("%1 the update to %2")
                                  .arg(retry ? QStringLiteral("Retrying") : QStringLiteral("Starting"), info_.version));

    if (!can_self_update()) {
        fail_update(blocked_reason());
        return;
    }
    error_text_.clear();
    failed_during_update_ = false;

    // The installer object first: there is no point downloading hundreds of MB
    // for an installation kind this build has no code to apply.
    if (installer_) {
        installer_->disconnect(this);
        installer_->deleteLater();
        installer_ = nullptr;
    }
    installer_ = updater::UpdateInstaller::create(updater::install_context(), this);
    if (!installer_) {
        LOG_ERROR("UpdateService", "No installer for this installation kind in this build");
        fail_update(tr("This installation of Fincept Terminal cannot be updated automatically. Download the new "
                       "version from the releases page instead."));
        return;
    }

    const QString dir = download_dir();
    if (!QDir().mkpath(dir)) {
        LOG_ERROR("UpdateService", QStringLiteral("Cannot create the download folder %1").arg(dir));
        fail_update(tr("The update could not be saved, because the folder %1 could not be created.")
                        .arg(QDir::toNativeSeparators(dir)));
        return;
    }
    svc_clear_dir(dir);

    if (info_.asset.size > 0) {
        const QStorageInfo storage(dir);
        const qint64 needed = info_.asset.size * 2 + kSvcDiskHeadroomBytes;
        if (storage.isValid() && storage.isReady() && storage.bytesAvailable() < needed) {
            LOG_WARN("UpdateService", QStringLiteral("Not enough free space in %1: %2 bytes free, %3 needed")
                                          .arg(dir, QString::number(storage.bytesAvailable()),
                                               QString::number(needed)));
            fail_update(tr("There is not enough free disk space for the update: it needs %1, but only %2 is free. "
                           "Free up some disk space and try again.")
                            .arg(QLocale().formattedDataSize(needed),
                                 QLocale().formattedDataSize(storage.bytesAvailable())));
            return;
        }
    }

    const QString path = QDir(dir).filePath(svc_download_file_name(info_.asset.url));

    if (downloader_) {
        downloader_->disconnect(this);
        downloader_->deleteLater();
        downloader_ = nullptr;
    }
    auto* downloader = new updater::UpdateDownloader(&net_, this);
    downloader_ = downloader;
    connect(downloader, &updater::UpdateDownloader::progress, this,
            [this, last_logged_tenth = -1](qint64 received, qint64 total) mutable {
                if (total > 0) {
                    const int tenth = int(received * 10 / total);
                    if (tenth != last_logged_tenth) {
                        last_logged_tenth = tenth;
                        LOG_DEBUG("UpdateService", QStringLiteral("Downloaded %1 of %2 bytes")
                                                      .arg(QString::number(received), QString::number(total)));
                    }
                }
                emit progress_changed(received, total);
            });
    connect(downloader, &updater::UpdateDownloader::verifying, this,
            [this] { set_stage(updater::UpdateStage::Verifying, tr("Verifying the download…")); });
    connect(downloader, &updater::UpdateDownloader::finished, this,
            [this](const QString& package_path) { begin_install(package_path); });
    connect(downloader, &updater::UpdateDownloader::failed, this,
            [this](const QString& message) { fail_update(message); });
    connect(downloader, &updater::UpdateDownloader::cancelled, this, [this] {
        LOG_INFO("UpdateService", "Download cancelled — the update is still available");
        set_stage(updater::UpdateStage::Available, svc_available_text(info_.version));
    });

    set_stage(S::Downloading, tr("Downloading Fincept Terminal %1…").arg(info_.version));
    emit progress_changed(0, info_.asset.size);
    downloader->start(info_.asset, path);
}

void UpdateService::begin_install(const QString& package_path) {
    using S = updater::UpdateStage;
    set_stage(S::Installing, tr("Installing Fincept Terminal %1…").arg(info_.version));

    if (!installer_)
        installer_ = updater::UpdateInstaller::create(updater::install_context(), this);
    if (!installer_) {
        LOG_ERROR("UpdateService", "No installer for this installation kind in this build");
        fail_update(tr("This installation of Fincept Terminal cannot be updated automatically. Download the new "
                       "version from the releases page instead."));
        return;
    }

    updater::UpdateInstaller* installer = installer_;
    installer->disconnect(this);
    connect(installer, &updater::UpdateInstaller::status, this, [this](const QString& text) {
        if (stage_ == updater::UpdateStage::Installing)
            set_status(text);
    });
    connect(installer, &updater::UpdateInstaller::ready_to_restart, this, [this] {
        if (stage_ != updater::UpdateStage::Installing) {
            LOG_WARN("UpdateService", QStringLiteral("Installer reported 'ready to restart' in stage %1 — ignored")
                                          .arg(svc_stage_name(stage_)));
            return;
        }
        LOG_INFO("UpdateService", QStringLiteral("Update to %1 is in place — restarting").arg(info_.version));
        write_pending_marker();
        set_stage(updater::UpdateStage::Restarting, tr("Restarting Fincept Terminal…"));
        QTimer::singleShot(kSvcRestartDelayMs, this, [this] {
            LOG_INFO("UpdateService", "Asking the application to close for the update");
            emit restart_requested();
        });
    });
    connect(installer, &updater::UpdateInstaller::handed_off, this, [this, installer](const QString& message) {
        if (stage_ != updater::UpdateStage::Installing)
            return;
        LOG_INFO("UpdateService", QStringLiteral("Update handed off to the system: %1").arg(svc_one_line(message)));
        installer->deleteLater();
        set_stage(updater::UpdateStage::Available, message);
        emit handed_off(message);
    });
    connect(installer, &updater::UpdateInstaller::failed, this, [this, installer](const QString& message) {
        if (stage_ != updater::UpdateStage::Installing)
            return;
        installer->deleteLater();
        fail_update(message);
    });

    LOG_INFO("UpdateService", QStringLiteral("Installing %1 as version %2").arg(package_path, info_.version));
    installer->install(package_path, info_);
}

void UpdateService::cancel() {
    if (stage_ != updater::UpdateStage::Downloading) {
        LOG_DEBUG("UpdateService",
                  QStringLiteral("cancel() ignored in stage %1").arg(svc_stage_name(stage_)));
        return;
    }
    LOG_INFO("UpdateService", "Cancelling the download at the user's request");
    if (downloader_ && downloader_->is_active())
        downloader_->cancel(); // → cancelled() → Available
    else
        set_stage(updater::UpdateStage::Available, svc_available_text(info_.version));
}

// ── State ───────────────────────────────────────────────────────────────────

void UpdateService::set_stage(updater::UpdateStage stage, const QString& status_text) {
    const updater::UpdateStage previous = stage_;
    const bool stage_moved = previous != stage;
    const bool text_changed = status_text_ != status_text;
    stage_ = stage;
    status_text_ = status_text;
    if (stage != updater::UpdateStage::Failed)
        error_text_.clear();

    LOG_DEBUG("UpdateService", QStringLiteral("Stage %1 → %2%3")
                                  .arg(svc_stage_name(previous), svc_stage_name(stage),
                                       status_text.isEmpty() ? QString()
                                                             : QStringLiteral(": ") + svc_one_line(status_text)));
    if (stage_moved)
        emit stage_changed(stage);
    if (text_changed)
        emit status_text_changed(status_text_);
}

void UpdateService::set_status(const QString& text) {
    if (text == status_text_)
        return;
    status_text_ = text;
    LOG_DEBUG("UpdateService", QStringLiteral("Status: %1").arg(svc_one_line(text)));
    emit status_text_changed(status_text_);
}

void UpdateService::fail_check(const QString& user_message) {
    info_ = {};
    failed_during_update_ = false;
    if (user_initiated_) {
        LOG_WARN("UpdateService", QStringLiteral("Update check failed: %1").arg(svc_one_line(user_message)));
        error_text_ = user_message;
        set_stage(updater::UpdateStage::Failed, tr("Could not check for updates."));
    } else {
        // The source / summary lines above already say why (no third line for the same outcome).
        set_stage(updater::UpdateStage::Idle);
    }
    emit check_finished(false);
}

void UpdateService::fail_update(const QString& user_message) {
    LOG_ERROR("UpdateService", QStringLiteral("Update to %1 failed: %2").arg(info_.version, svc_one_line(user_message)));
    failed_during_update_ = true;
    error_text_ = user_message;
    // Nothing in the folder is usable after a failure (a retry downloads
    // again), so give the disk space back now.
    svc_clear_dir(download_dir());
    set_stage(updater::UpdateStage::Failed, tr("The update did not complete."));
}

// ── Pending-update marker ───────────────────────────────────────────────────

QString UpdateService::pending_marker_path() {
    // The install-wide data root, not a profile folder: whichever profile the
    // new version opens with must find it.
    return QDir(AppPaths::root()).filePath(QStringLiteral("update-pending.json"));
}

QString UpdateService::download_dir() {
    return QDir(AppPaths::cache()).filePath(QStringLiteral("updates"));
}

void UpdateService::write_pending_marker() const {
    const QString path = pending_marker_path();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject marker;
    marker.insert(QStringLiteral("from"), QCoreApplication::applicationVersion());
    marker.insert(QStringLiteral("to"), info_.version);
    marker.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_WARN("UpdateService", QStringLiteral("Cannot write the update marker %1: %2").arg(path, file.errorString()));
        return;
    }
    file.write(QJsonDocument(marker).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        LOG_WARN("UpdateService", QStringLiteral("Cannot write the update marker %1: %2").arg(path, file.errorString()));
        return;
    }
    LOG_INFO("UpdateService", QStringLiteral("Wrote the update marker %1 (%2 → %3)")
                                  .arg(path, QCoreApplication::applicationVersion(), info_.version));
}

bool UpdateService::take_last_update_result(QString& to_version, bool& succeeded) {
    const QString path = pending_marker_path();
    if (!QFileInfo::exists(path))
        return false;

    QByteArray data;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        data = file.read(64 * 1024);
        file.close();
    }
    if (!QFile::remove(path))
        LOG_WARN("UpdateService", QStringLiteral("Could not delete the update marker %1").arg(path));

    const QJsonObject marker = QJsonDocument::fromJson(data).object();
    const QString from = marker.value(QLatin1String("from")).toString();
    const QString to = marker.value(QLatin1String("to")).toString();
    int major = 0, minor = 0, patch = 0;
    if (!updater::parse_version(to, major, minor, patch)) {
        LOG_WARN("UpdateService", QStringLiteral("Ignoring a malformed update marker (%1 bytes)").arg(data.size()));
        return false;
    }

    const QString running = QCoreApplication::applicationVersion();
    const bool running_is_release = updater::parse_version(running, major, minor, patch);
    to_version = to;
    succeeded = running_is_release && !updater::is_newer_version(running, to);
    if (succeeded) {
        LOG_INFO("UpdateService",
                 QStringLiteral("The update from %1 to %2 succeeded (running %3)").arg(from, to, running));
        svc_clear_dir(download_dir());
    } else {
        LOG_WARN("UpdateService",
                 QStringLiteral("The update from %1 to %2 did not complete — still running %3").arg(from, to, running));
    }
    return true;
}

} // namespace fincept::services
