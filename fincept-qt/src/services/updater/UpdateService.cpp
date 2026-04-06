#include "services/updater/UpdateService.h"

#include "core/logging/Logger.h"

#include <QSimpleUpdater.h>
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>

#ifdef Q_OS_LINUX
#include <QProcess>
#include <QFileInfo>
#endif

namespace fincept::services {

UpdateService& UpdateService::instance() {
    static UpdateService inst;
    return inst;
}

UpdateService::UpdateService(QObject* parent)
    : QObject(parent)
    , url_(DEFAULT_MANIFEST_URL)
{
    updater_ = QSimpleUpdater::getInstance();

    updater_->setModuleVersion(url_, QApplication::applicationVersion());
    updater_->setModuleName(url_, "Fincept Terminal");
    updater_->setNotifyOnUpdate(url_, true);
    updater_->setNotifyOnFinish(url_, false);

#ifdef Q_OS_LINUX
    // On Linux (AppImage): disable the built-in downloader — we use
    // AppImageUpdate for in-place delta updates instead.
    updater_->setDownloaderEnabled(url_, false);
    updater_->setUseCustomInstallProcedures(url_, true);
#else
    updater_->setDownloaderEnabled(url_, true);
#endif

    connect(updater_, &QSimpleUpdater::checkingFinished,
            this, [this](const QString& checked_url) {
        if (checked_url != url_) return;

        // If the manifest is unreachable or returns a non-200 response,
        // QSimpleUpdater sets latestVersion to empty — treat as "no update"
        // rather than surfacing an error dialog to the user.
        const QString latest = updater_->getLatestVersion(url_);
        if (latest.isEmpty()) {
            LOG_WARN("UpdateService",
                     "Update manifest returned empty version — server may be unreachable "
                     "or updates.json not yet published. Skipping silently.");
            emit check_finished(false);
            return;
        }

        bool found = updater_->getUpdateAvailable(url_);
        LOG_INFO("UpdateService", QString("Check finished — update available: %1, latest: %2")
                     .arg(found ? "yes" : "no")
                     .arg(latest));

#ifdef Q_OS_LINUX
        if (found) {
            // Prompt the user then run AppImageUpdate in-place
            auto btn = QMessageBox::question(
                nullptr,
                "Update Available",
                QString("Fincept Terminal %1 is available.\n\n"
                        "The update will be applied in the background.\n"
                        "Restart when prompted to complete installation.\n\n"
                        "Update now?")
                    .arg(latest),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (btn == QMessageBox::Yes)
                run_appimage_update();
        }
#endif
        emit check_finished(found);
    });

    connect(updater_, &QSimpleUpdater::downloadFinished,
            this, [this](const QString& checked_url, const QString& filepath) {
        if (checked_url != url_) return;
        LOG_INFO("UpdateService", QString("Download finished: %1").arg(filepath));
        emit download_finished(filepath);
    });
}

void UpdateService::check_for_updates(bool silent) {
    updater_->setNotifyOnFinish(url_, !silent);
    LOG_INFO("UpdateService", QString("Checking for updates at: %1").arg(url_));
    updater_->checkForUpdates(url_);
}

bool UpdateService::update_available() const {
    return updater_->getUpdateAvailable(url_);
}

QString UpdateService::latest_version() const {
    return updater_->getLatestVersion(url_);
}

QString UpdateService::changelog() const {
    return updater_->getChangelog(url_);
}

void UpdateService::set_mandatory(bool mandatory) {
    mandatory_ = mandatory;
    updater_->setMandatoryUpdate(url_, mandatory);
}

void UpdateService::set_manifest_url(const QString& url) {
    url_ = url;
    updater_->setModuleVersion(url_, QApplication::applicationVersion());
    updater_->setModuleName(url_, "Fincept Terminal");
    updater_->setNotifyOnUpdate(url_, true);
    updater_->setNotifyOnFinish(url_, false);
    updater_->setMandatoryUpdate(url_, mandatory_);
#ifdef Q_OS_LINUX
    updater_->setDownloaderEnabled(url_, false);
    updater_->setUseCustomInstallProcedures(url_, true);
#else
    updater_->setDownloaderEnabled(url_, true);
#endif
}

void UpdateService::set_download_dir(const QString& dir) {
    updater_->setDownloadDir(url_, dir);
}

void UpdateService::set_notify_on_finish(bool notify) {
    updater_->setNotifyOnFinish(url_, notify);
}

// ── Linux: AppImage in-place update ─────────────────────────────────────────

#ifdef Q_OS_LINUX
void UpdateService::run_appimage_update() {
    // APPIMAGE env var is set by the AppImage runtime — it's the path to
    // the running .AppImage file. AppImageUpdate patches it in-place.
    QString appimage_path = qEnvironmentVariable("APPIMAGE");
    if (appimage_path.isEmpty()) {
        LOG_WARN("UpdateService", "APPIMAGE env var not set — not running as AppImage, skipping in-place update");
        emit appimage_update_finished(false);
        return;
    }

    // Look for AppImageUpdate tool bundled inside the AppImage or on PATH
    QString tool;
    QStringList candidates = {
        QApplication::applicationDirPath() + "/AppImageUpdate",
        "/usr/local/bin/AppImageUpdate",
        "/usr/bin/AppImageUpdate",
    };
    for (const auto& c : candidates) {
        if (QFileInfo::exists(c)) { tool = c; break; }
    }

    if (tool.isEmpty()) {
        // Fall back: open the GitHub release page so user can download manually
        LOG_WARN("UpdateService", "AppImageUpdate not found — falling back to browser download");
        updater_->setDownloaderEnabled(url_, true);
        updater_->setUseCustomInstallProcedures(url_, false);
        emit check_finished(true); // re-trigger QSimpleUpdater UI
        return;
    }

    if (appimage_update_process_) {
        appimage_update_process_->deleteLater();
        appimage_update_process_ = nullptr;
    }

    appimage_update_process_ = new QProcess(this);

    connect(appimage_update_process_, &QProcess::finished,
            this, [this](int exit_code, QProcess::ExitStatus) {
        bool ok = (exit_code == 0);
        LOG_INFO("UpdateService", QString("AppImageUpdate finished — exit code: %1").arg(exit_code));
        if (ok) {
            QMessageBox::information(
                nullptr,
                "Update Complete",
                "Fincept Terminal has been updated.\nPlease restart the application to use the new version."
            );
        } else {
            QString err = appimage_update_process_->readAllStandardError();
            LOG_ERROR("UpdateService", QString("AppImageUpdate failed: %1").arg(err));
            QMessageBox::warning(
                nullptr,
                "Update Failed",
                "The automatic update could not be applied.\n"
                "Please download the latest version from the Fincept website."
            );
        }
        emit appimage_update_finished(ok);
        appimage_update_process_->deleteLater();
        appimage_update_process_ = nullptr;
    });

    LOG_INFO("UpdateService", QString("Running AppImageUpdate: %1 %2").arg(tool, appimage_path));
    // -O flag: overwrite the source AppImage in-place
    appimage_update_process_->start(tool, {"-O", appimage_path});
}
#endif

} // namespace fincept::services
