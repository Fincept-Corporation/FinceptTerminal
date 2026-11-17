#pragma once

#include <QMetaType>
#include <QString>

// Shared value types of the in-app updater (src/services/updater).
//
// The updater replaces the running installation in place: same folder, same
// shortcuts, and the user's data (which lives in the data root, never in the
// install folder) is not touched. How that is done depends on how this copy of
// the terminal was installed — see InstallKind.

namespace fincept::services::updater {

/// How the running copy was installed. Decides which release asset is
/// downloaded and how it is applied.
enum class InstallKind {
    Unknown,      ///< dev build, unpacked tree, unsupported arch — cannot self-update
    WindowsIfw,   ///< Qt IFW setup.exe install (has FinceptMaintenanceTool.exe beside the exe)
    MacAppBundle, ///< .app bundle copied out of the release .dmg
    LinuxAppImage, ///< an AppImage (released up to 4.5.3, discontinued) — cannot self-update
    LinuxDeb,     ///< the edition's /opt prefix owned by the dpkg package
    LinuxRpm,     ///< the edition's /opt prefix owned by an rpm (up to 4.5.3, discontinued) — cannot self-update
    LinuxFlatpak, ///< running inside the Flatpak sandbox
};

/// Facts about the running installation, detected once at startup.
struct InstallContext {
    InstallKind kind = InstallKind::Unknown;
    /// "<os>-<arch>": windows-x64, windows-arm64, linux-x64, linux-arm64,
    /// macos-arm64, macos-x64. Empty when the platform is unsupported.
    QString platform_key;
    /// Manifest asset key "<platform_key>-<kind>": windows-x64-ifw,
    /// macos-arm64-dmg, linux-x64-deb, linux-x64-flatpak. Empty when kind is
    /// Unknown or a discontinued kind (LinuxAppImage, LinuxRpm).
    QString asset_key;
    /// What gets replaced: the install folder (WindowsIfw), the .app bundle
    /// path (MacAppBundle), the package prefix (LinuxDeb). Informational for
    /// the discontinued kinds (the AppImage file, the rpm's prefix). Empty for
    /// Flatpak / Unknown.
    QString target_path;
    /// True when this installation can be updated in place by the terminal.
    bool can_self_update = false;
    /// When can_self_update is false: a sentence for the user saying why
    /// (already translated). Empty otherwise.
    QString blocked_reason;
};

/// One downloadable release file.
struct UpdateAsset {
    QString url;     ///< https download URL
    QString sha256;  ///< 64 lower-case hex characters
    qint64 size = 0; ///< bytes; 0 when the manifest did not say
};

/// The newer release found by a check.
struct UpdateInfo {
    QString version;     ///< "4.5.4"
    QString release_url; ///< release page (https); may be empty
    QString notes;       ///< short plain-text "what's new"; may be empty
    UpdateAsset asset;   ///< the file for this installation; url empty when the release has none
    bool has_asset() const { return !asset.url.isEmpty(); }
};

/// Where the updater is. Every stage has a line of text for the user
/// (UpdateService::status_text()), so there is never a silent stretch between
/// "Update now" and the new version starting.
enum class UpdateStage {
    Idle,        ///< nothing going on; no newer release known
    Checking,    ///< fetching + verifying the manifest
    UpToDate,    ///< last check found nothing newer
    Available,   ///< newer release known; waiting for the user
    Downloading, ///< fetching the release file (progress_changed fires)
    Verifying,   ///< checksum / code-signature check of the downloaded file
    Installing,  ///< applying the update
    Restarting,  ///< update staged or applied; the terminal is about to close and reopen
    Failed,      ///< check or update failed; error_text() says why
};

} // namespace fincept::services::updater

Q_DECLARE_METATYPE(fincept::services::updater::UpdateStage)
