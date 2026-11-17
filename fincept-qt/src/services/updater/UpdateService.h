#pragma once

#include "services/updater/UpdateTypes.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

class QNetworkReply;

namespace fincept::services {

namespace updater {
class UpdateDownloader;
class UpdateInstaller;
} // namespace updater

/// In-app updater: finds a newer GitHub release, and on the user's say-so
/// downloads it, verifies it and replaces the running installation in place,
/// then restarts into the new version.
///
/// No widgets here. The service is a state machine (updater::UpdateStage) that
/// reports through signals; ui::UpdateDialog (src/ui/widgets/UpdateDialog.h)
/// is the window that shows it.
///
///   check_for_updates()            Idle/UpToDate/Failed → Checking → UpToDate | Available | Failed
///   start_update()                 Available → Downloading → Verifying → Installing → Restarting | Failed
///   cancel()                       Downloading → Available
///
/// Checks run on every start of the terminal (the app shell calls
/// check_for_updates() once its first window is up) and again every few hours
/// while it stays open.
///
/// Trust: the manifest (updates.json, see UpdateManifest.h) is fetched over
/// TLS from the latest GitHub release and must carry a valid Ed25519 signature
/// under the pinned key; the release file must match the manifest's sha256;
/// on Windows and macOS the file's code signature is checked as well.
class UpdateService : public QObject {
    Q_OBJECT

  public:
    static UpdateService& instance();

    /// Start a check. `user_initiated` only affects reporting: a background
    /// check that finds nothing (or cannot reach the server) ends quietly in
    /// UpToDate / Idle, a user-initiated one ends in UpToDate / Failed so the
    /// window can say so. Ignored while a check or an update is running —
    /// except that a user-initiated call promotes a running background check.
    void check_for_updates(bool user_initiated = false);

    /// Download and install the release found by the last check. Only valid in
    /// stage Available (or Failed after a failed update attempt — retry).
    void start_update();

    /// Abort a running download. No effect in other stages (verification and
    /// installation are not interruptible).
    void cancel();

    updater::UpdateStage stage() const { return stage_; }
    /// The newer release (valid from stage Available onwards).
    const updater::UpdateInfo& info() const { return info_; }
    /// One line saying what is happening right now (translated).
    QString status_text() const { return status_text_; }
    /// Why the last check or update failed (translated). Empty unless Failed.
    QString error_text() const { return error_text_; }
    /// True when the failure happened during an update (retry = start_update)
    /// rather than during a check (retry = check_for_updates).
    bool failed_during_update() const { return failed_during_update_; }

    /// A newer release is known (any stage from Available onwards, including
    /// Failed after an update attempt).
    bool update_available() const { return !info_.version.isEmpty(); }
    QString latest_version() const { return info_.version; }

    /// Whether this installation can be updated in place. When false the
    /// window offers the release page instead, with blocked_reason().
    bool can_self_update() const;
    QString blocked_reason() const;

    /// False in a development build (version is not "X.Y.Z") — checks are
    /// skipped there.
    static bool update_checks_available();

    static QString releases_page_url();

    /// If the previous run of the terminal ended in an update, returns what
    /// happened and forgets it: `to_version` is the version that update was
    /// meant to install; `succeeded` is whether the running version is that
    /// one. Returns false when there is nothing to report.
    static bool take_last_update_result(QString& to_version, bool& succeeded);

  signals:
    void stage_changed(fincept::services::updater::UpdateStage stage);
    /// Download progress; `total` is 0 when unknown.
    void progress_changed(qint64 received, qint64 total);
    void status_text_changed(const QString& text);
    /// After every check. `update_found` is true when a newer release exists.
    void check_finished(bool update_found);
    /// The update continues outside the terminal (see UpdateInstaller::handed_off).
    void handed_off(const QString& message);
    /// The update is staged and the application must close now. The app shell
    /// connects this to its normal quit path (saving the session first).
    void restart_requested();

  private:
    explicit UpdateService(QObject* parent = nullptr);

    void fetch_manifest(int source_index);
    void on_manifest_reply(QNetworkReply* reply, int source_index);
    void on_signature_reply(QNetworkReply* reply, int source_index, const QByteArray& manifest);
    void manifest_source_failed(int source_index, const QString& log_reason, const QString& user_reason);
    void process_verified_manifest(const QByteArray& manifest);

    void begin_install(const QString& package_path);
    void set_stage(updater::UpdateStage stage, const QString& status_text = {});
    void set_status(const QString& text);
    void fail_check(const QString& user_message);
    void fail_update(const QString& user_message);
    void write_pending_marker() const;

    QStringList manifest_urls() const;
    static QString pending_marker_path();
    static QString download_dir();

    QNetworkAccessManager net_;
    QTimer recheck_timer_;
    QPointer<updater::UpdateDownloader> downloader_;
    QPointer<updater::UpdateInstaller> installer_;

    updater::UpdateStage stage_ = updater::UpdateStage::Idle;
    updater::UpdateInfo info_;
    QString status_text_;
    QString error_text_;
    bool user_initiated_ = false;
    bool failed_during_update_ = false;
    bool every_source_404_ = true; ///< this check: every update source answered "nothing published" so far
};

} // namespace fincept::services
