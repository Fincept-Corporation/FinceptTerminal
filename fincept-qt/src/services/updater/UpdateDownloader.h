#pragma once

#include "services/updater/UpdateTypes.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkReply;

namespace fincept::services::updater {

/// Downloads one release file to disk and checks its sha256.
///
/// The body is streamed to "<destination>.part" as it arrives (release files
/// are several hundred MB — never held in memory) and hashed on the way; the
/// file is renamed to `destination` only after the checksum matches. One
/// download at a time per instance.
class UpdateDownloader : public QObject {
    Q_OBJECT

  public:
    explicit UpdateDownloader(QNetworkAccessManager* net, QObject* parent = nullptr);
    ~UpdateDownloader() override;

    /// Start downloading `asset` to `destination` (parent folder must exist).
    /// Ends with exactly one of finished() / failed() / cancelled().
    void start(const UpdateAsset& asset, const QString& destination);

    /// Abort the running download and delete the partial file. No-op when idle.
    void cancel();

    bool is_active() const { return !reply_.isNull(); }

  signals:
    /// `total` is the manifest size, else Content-Length, else 0 (unknown).
    void progress(qint64 received, qint64 total);
    /// The body is complete and is being checked (the hash is already known;
    /// this marks the hand-over for the UI).
    void verifying();
    void finished(const QString& path);
    /// `message` is a translated sentence for the user.
    void failed(const QString& message);
    void cancelled();

  private:
    void on_ready_read();
    void on_finished();
    void fail(const QString& message);
    void cleanup();

    QNetworkAccessManager* net_;
    QPointer<QNetworkReply> reply_;
    QFile part_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    UpdateAsset asset_;
    QString destination_;
    qint64 received_ = 0;
    bool write_failed_ = false;
    bool cancelling_ = false;
};

} // namespace fincept::services::updater
