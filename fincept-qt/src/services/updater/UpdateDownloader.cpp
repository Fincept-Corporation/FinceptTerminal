#include "services/updater/UpdateDownloader.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStorageInfo>
#include <QUrl>

namespace fincept::services::updater {

namespace {

constexpr int kDownloadInactivityTimeoutMs = 60000;
constexpr qint64 kDownloadProgressIntervalMs = 100; // at most ~10 progress signals a second
constexpr qint64 kDownloadDiskLowBytes = 32LL * 1024 * 1024;

QString download_part_path(const QString& destination) {
    return destination + QStringLiteral(".part");
}

} // namespace

UpdateDownloader::UpdateDownloader(QNetworkAccessManager* net, QObject* parent) : QObject(parent), net_(net) {}

UpdateDownloader::~UpdateDownloader() {
    if (reply_) {
        LOG_INFO("UpdateService", "Download object destroyed while a download was running — aborting it");
        cancelling_ = true;
        cleanup();
    }
}

void UpdateDownloader::start(const UpdateAsset& asset, const QString& destination) {
    if (reply_) {
        LOG_WARN("UpdateService", "A download is already running — ignoring a second start");
        return;
    }

    asset_ = asset;
    destination_ = destination;
    received_ = 0;
    write_failed_ = false;
    cancelling_ = false;
    hash_.reset();

    // Never spend hundreds of MB on a file that could not be checked afterwards.
    if (asset_.url.isEmpty() || asset_.sha256.size() != 64) {
        LOG_ERROR("UpdateService", QStringLiteral("Refusing to download '%1': no usable sha256 ('%2')")
                                       .arg(asset_.url, asset_.sha256));
        emit failed(tr("This update cannot be checked for integrity, so it was not downloaded."));
        return;
    }

    const QString part_path = download_part_path(destination_);
    if (part_.isOpen())
        part_.close();
    QFile::remove(part_path);
    part_.setFileName(part_path);
    if (!part_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR("UpdateService", QStringLiteral("Cannot create %1: %2").arg(part_path, part_.errorString()));
        emit failed(tr("The update could not be saved in %1.\n%2")
                        .arg(QDir::toNativeSeparators(QFileInfo(destination_).absolutePath()), part_.errorString()));
        return;
    }

    LOG_INFO("UpdateService", QStringLiteral("Downloading %1 → %2 (expected size %3, sha256 %4)")
                                  .arg(asset_.url, destination_,
                                       asset_.size > 0 ? QString::number(asset_.size) : QStringLiteral("unknown"),
                                       asset_.sha256));

    QNetworkRequest request{QUrl(asset_.url)};
    // GitHub release downloads redirect to a different host (release-assets /
    // objects.githubusercontent.com): allow that, but never https → http.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("FinceptTerminal/%1").arg(QCoreApplication::applicationVersion()));
    // Inactivity timeout: the request is aborted when no data moves for this
    // long — not a cap on the whole (large) download.
    request.setTransferTimeout(kDownloadInactivityTimeoutMs);

    reply_ = net_->get(request);
    connect(reply_, &QNetworkReply::readyRead, this, [this, clock = QElapsedTimer()]() mutable {
        on_ready_read();
        if (!reply_)
            return; // failed while writing
        if (!clock.isValid() || clock.elapsed() >= kDownloadProgressIntervalMs) {
            clock.start();
            const qint64 total =
                asset_.size > 0 ? asset_.size : reply_->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            emit progress(received_, total > 0 ? total : 0);
        }
    });
    connect(reply_, &QNetworkReply::finished, this, &UpdateDownloader::on_finished);
}

void UpdateDownloader::cancel() {
    if (!reply_)
        return;
    LOG_INFO("UpdateService", QStringLiteral("Download cancelled by the user after %1 bytes").arg(received_));
    cancelling_ = true;
    cleanup();
    emit cancelled();
}

void UpdateDownloader::on_ready_read() {
    if (!reply_ || write_failed_ || cancelling_)
        return;

    const QByteArray chunk = reply_->readAll();
    if (chunk.isEmpty())
        return;

    // An error page (404, 503, …) has a body too: never let it near the file.
    // on_finished() reports the status.
    const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200)
        return;

    received_ += chunk.size();
    if (asset_.size > 0 && received_ > asset_.size) {
        LOG_ERROR("UpdateService", QStringLiteral("Server sent more than the expected %1 bytes — stopping the download")
                                       .arg(asset_.size));
        fail(tr("The downloaded file is larger than expected, so it was discarded. Please try again."));
        return;
    }

    hash_.addData(chunk);
    if (part_.write(chunk) != chunk.size()) {
        write_failed_ = true;
        const QString detail = part_.errorString();
        LOG_ERROR("UpdateService", QStringLiteral("Writing %1 failed after %2 bytes: %3")
                                       .arg(part_.fileName(), QString::number(received_), detail));
        const QStorageInfo storage(QFileInfo(destination_).absolutePath());
        if (storage.isValid() && storage.bytesAvailable() < kDownloadDiskLowBytes)
            fail(tr("The disk is full, so the update could not be saved. Free up some disk space and try again."));
        else
            fail(tr("The update could not be saved to disk.\n%1").arg(detail));
    }
}

void UpdateDownloader::on_finished() {
    if (!reply_)
        return;
    if (cancelling_) {
        cleanup();
        emit cancelled();
        return;
    }

    // Anything still buffered (normally nothing: readyRead drained it).
    on_ready_read();
    if (!reply_)
        return; // on_ready_read() failed and reported it

    QNetworkReply* reply = reply_;
    const QNetworkReply::NetworkError error = reply->error();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (error != QNetworkReply::NoError) {
        LOG_ERROR("UpdateService", QStringLiteral("Download failed after %1 bytes: %2 (code %3, HTTP %4)")
                                       .arg(QString::number(received_), reply->errorString(),
                                            QString::number(int(error)), QString::number(status)));
        if (error == QNetworkReply::OperationCanceledError || error == QNetworkReply::TimeoutError) {
            // Not cancelled by us (cancelling_ is false): the inactivity timeout.
            fail(tr("The download stopped because no data arrived for a minute. "
                    "Check your internet connection and try again."));
        } else if (status >= 400 || error == QNetworkReply::ContentNotFoundError) {
            fail(tr("The update file could not be fetched from the server (error %1). Please try again later.")
                     .arg(status > 0 ? QString::number(status) : reply->errorString()));
        } else {
            fail(tr("The update could not be downloaded. Check your internet connection and try again.\n%1")
                     .arg(reply->errorString()));
        }
        return;
    }
    if (status != 200) {
        LOG_ERROR("UpdateService", QStringLiteral("Download answered HTTP %1 instead of 200").arg(status));
        fail(tr("The update file could not be fetched from the server (error %1). Please try again later.")
                 .arg(status));
        return;
    }

    // The body is complete — make sure it reached the disk.
    if (!part_.flush() || part_.error() != QFileDevice::NoError) {
        const QString detail = part_.errorString();
        LOG_ERROR("UpdateService", QStringLiteral("Flushing %1 failed: %2").arg(part_.fileName(), detail));
        const QStorageInfo storage(QFileInfo(destination_).absolutePath());
        if (storage.isValid() && storage.bytesAvailable() < kDownloadDiskLowBytes)
            fail(tr("The disk is full, so the update could not be saved. Free up some disk space and try again."));
        else
            fail(tr("The update could not be saved to disk.\n%1").arg(detail));
        return;
    }
    part_.close();

    emit progress(received_, asset_.size > 0 ? asset_.size : received_);

    if (asset_.size > 0 && received_ != asset_.size) {
        LOG_ERROR("UpdateService",
                  QStringLiteral("Download incomplete: %1 of %2 bytes").arg(received_).arg(asset_.size));
        fail(tr("The download was incomplete, so it was discarded. Please try again.") + QLatin1Char('\n') +
             tr("Received %1 of %2 bytes.").arg(QLocale().toString(received_), QLocale().toString(asset_.size)));
        return;
    }

    emit verifying();

    const QString actual = QString::fromLatin1(hash_.result().toHex());
    if (actual.compare(asset_.sha256, Qt::CaseInsensitive) != 0) {
        LOG_ERROR("UpdateService",
                  QStringLiteral("sha256 mismatch for %1: expected %2, got %3").arg(asset_.url, asset_.sha256, actual));
        fail(tr("The downloaded file failed its integrity check, so it was deleted. It may have been damaged "
                "on the way. Please try again."));
        return;
    }
    LOG_INFO("UpdateService", QStringLiteral("Download complete and verified: %1 bytes, sha256 %2")
                                  .arg(QString::number(received_), actual));

    const QString part_path = download_part_path(destination_);
    if (QFileInfo::exists(destination_) && !QFile::remove(destination_)) {
        LOG_ERROR("UpdateService", QStringLiteral("Cannot remove the old %1").arg(destination_));
        fail(tr("The update could not be saved, because an older download in the same place could not be "
                "replaced:\n%1")
                 .arg(QDir::toNativeSeparators(destination_)));
        return;
    }
    if (!QFile::rename(part_path, destination_)) {
        LOG_ERROR("UpdateService", QStringLiteral("Cannot rename %1 to %2").arg(part_path, destination_));
        fail(tr("The update could not be saved:\n%1").arg(QDir::toNativeSeparators(destination_)));
        return;
    }

    reply_ = nullptr;
    reply->disconnect(this);
    reply->deleteLater();
    emit finished(destination_);
}

void UpdateDownloader::fail(const QString& message) {
    cleanup();
    emit failed(message);
}

void UpdateDownloader::cleanup() {
    if (reply_) {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        // Disconnect first: abort() emits finished() synchronously.
        reply->disconnect(this);
        if (reply->isRunning())
            reply->abort();
        reply->deleteLater();
    }
    if (part_.isOpen())
        part_.close();
    if (!destination_.isEmpty())
        QFile::remove(download_part_path(destination_));
}

} // namespace fincept::services::updater
