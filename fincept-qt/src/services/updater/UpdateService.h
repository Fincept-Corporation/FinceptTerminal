#pragma once
#include <QObject>
#include <QString>

class QSimpleUpdater;
class QProcess;

namespace fincept::services {

/// Cross-platform auto-update service.
///
/// - Windows/macOS: QSimpleUpdater downloads the package and opens it
///   (zip/tar → user runs installer or drags .app).
/// - Linux (AppImage): AppImageUpdate performs an in-place delta update
///   transparently; user just sees a "restart to apply" prompt.
class UpdateService : public QObject {
    Q_OBJECT

  public:
    static UpdateService& instance();

    /// Trigger an update check. Safe to call from any screen.
    /// silent=true → no "up to date" popup when already current.
    void check_for_updates(bool silent = true);

    /// Whether an update was found after the last check.
    bool update_available() const;

    /// Latest version string from the manifest (empty if not checked yet).
    QString latest_version() const;

    /// Changelog text from the manifest.
    QString changelog() const;

    /// If true the user cannot dismiss the update dialog.
    void set_mandatory(bool mandatory);

    // ── Configuration ────────────────────────────────────────────────────────

    /// Override the manifest URL (default: GitHub releases JSON).
    void set_manifest_url(const QString& url);

    /// Override the download directory (default: system temp).
    void set_download_dir(const QString& dir);

    /// Show "no updates available" dialog even when up to date.
    void set_notify_on_finish(bool notify);

  signals:
    /// Emitted after every check, regardless of result.
    void check_finished(bool update_found);

    /// Emitted when a downloaded file is ready (Windows/macOS).
    void download_finished(const QString& filepath);

    /// Emitted when AppImage in-place update completes (Linux only).
    void appimage_update_finished(bool success);

  private:
    explicit UpdateService(QObject* parent = nullptr);

#ifdef Q_OS_LINUX
    /// Runs AppImageUpdate CLI on the current AppImage file in-place.
    void run_appimage_update();
    QProcess* appimage_update_process_ = nullptr;
#endif

    static constexpr const char* DEFAULT_MANIFEST_URL =
        "https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/updates.json";

    QSimpleUpdater* updater_ = nullptr;
    QString url_;
    bool mandatory_ = false;
};

} // namespace fincept::services
