#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkReply;
class QWidget;

namespace fincept::services {

/// Cross-platform auto-update service.
///
/// Downloads a JSON manifest, compares the running app version with the
/// manifest's per-platform/per-arch `latest-version`, and — if newer — fetches
/// the installer, verifies its sha256, and launches it.
///
/// Manifest schema (see updates.json):
/// {
///   "updates": {
///     "windows-x64":  { "latest-version": "4.1.0", "download-url": "...", "sha256": "...", "open-url": "...",
///     "changelog": "..." }, "windows-arm64":{ ... }, "linux-x64":    { ... }, "macos-arm64":  { ... }, "macos-x64": {
///     ... }, "macos-universal": { ... }
///   }
/// }
///
/// Platforms where auto-install isn't possible (unsigned / no installer) fall
/// back to revealing the downloaded file to the user.
class UpdateService : public QObject {
    Q_OBJECT

  public:
    static UpdateService& instance();

    /// Trigger an update check.
    /// - silent=true  → no dialog when already up to date or manifest unreachable.
    /// - silent=false → show result either way (used by "Check for Updates" menu).
    void check_for_updates(bool silent = true);

    /// Whether an update was found after the last check.
    bool update_available() const { return update_available_; }

    /// Latest version string from the manifest (empty until first successful check).
    QString latest_version() const { return latest_version_; }

    /// Changelog text from the manifest.
    QString changelog() const { return changelog_; }

    /// Override the manifest URL (default: GitHub raw updates.json).
    /// The detached signature is always fetched from `<manifest_url> + ".sig"`.
    void set_manifest_url(const QString& url) { manifest_url_ = url; }

    /// Parent widget for dialogs (defaults to QApplication::activeWindow()).
    void set_dialog_parent(QWidget* parent) { dialog_parent_ = parent; }

  signals:
    /// Emitted after every check, regardless of result.
    void check_finished(bool update_found);

    /// Emitted after a successful verified download, before launch.
    void download_finished(const QString& filepath);

  private slots:
    void on_manifest_reply_finished();
    void on_signature_reply_finished();
    void on_download_reply_finished();
    void on_download_progress(qint64 received, qint64 total);

  private:
    explicit UpdateService(QObject* parent = nullptr);

    /// Parse + act on a manifest whose detached signature has already been
    /// verified. Never call this with unverified bytes.
    void process_verified_manifest(const QByteArray& body);

    /// True iff `signature` (base64 or hex, 64 raw bytes) is a valid Ed25519
    /// signature over `manifest` under UPDATE_SIGNING_PUBLIC_KEY_HEX.
    static bool verify_manifest_signature(const QByteArray& manifest, const QByteArray& signature);

    /// Raw 32-byte signing public key, or an empty QByteArray when no key is
    /// pinned (or the pinned value is malformed) — in which case auto-update is
    /// refused outright.
    static QByteArray signing_public_key();

    /// Return the lookup key for the current runtime (e.g. "windows-x64").
    /// Returns empty string if the platform/arch combination isn't supported.
    static QString current_platform_key();

    /// Strict semver parse: "1.2.3" → {1,2,3}. Returns false for anything else
    /// (dev builds, tags, suffixes). Used to both validate the running version
    /// and the manifest version.
    static bool parse_version(const QString& s, int& major, int& minor, int& patch);

    /// Returns true iff `remote` is strictly newer than `local`.
    static bool is_newer(const QString& local, const QString& remote);

    void start_download(const QString& url, const QString& expected_sha256);
    void launch_installer(const QString& path);
    void reveal_in_file_manager(const QString& path);
    void finish_check(bool update_found);
    void show_error(const QString& text);
    QWidget* dialog_parent();

    static constexpr const char* DEFAULT_MANIFEST_URL =
        "https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/updates.json";

    // ── Update-manifest signing key ─────────────────────────────────────────
    // Raw Ed25519 public key, 32 bytes as 64 lowercase hex characters.
    //
    // WHY: `download-url` and `sha256` both come out of the same updates.json
    // on raw.githubusercontent.com. The sha256 check proves the installer
    // matches what the manifest claimed — it says nothing about whether the
    // manifest is ours. Anyone who can push to `main`, or who takes over that
    // account, otherwise gets code execution on every installation at next
    // launch, because the verified installer is auto-launched via
    // startDetached and silent checks fire at startup.
    //
    // A detached signature over the manifest bytes fixes that: the private key
    // never touches CI or the repo, so a repo compromise alone cannot produce a
    // manifest this binary will accept.
    //
    // EMPTY = auto-update is DISABLED (fail closed). See the signing runbook in
    // UpdateService.cpp for how to generate the keypair and produce
    // updates.json.sig.
    static constexpr const char* UPDATE_SIGNING_PUBLIC_KEY_HEX = "";

    QNetworkAccessManager net_;
    QByteArray pending_manifest_body_;
    QString manifest_url_ = DEFAULT_MANIFEST_URL;
    QString latest_version_;
    QString changelog_;
    QString pending_download_url_;
    QString pending_expected_sha256_;
    QString pending_local_path_;
    QPointer<QWidget> dialog_parent_;
    bool update_available_ = false;
    bool silent_ = true;
    bool in_progress_ = false;
    /// Silent auto-checks skip themselves after the first one per process run
    /// so multiple login/unlock code paths don't fire repeated checks. A
    /// user-initiated check (silent=false) always runs regardless.
    bool silent_check_done_ = false;
};

} // namespace fincept::services
