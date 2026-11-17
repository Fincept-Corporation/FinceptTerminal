#pragma once

#include "services/updater/UpdateTypes.h"

namespace fincept::services::updater {

/// Detect how the running copy was installed (see InstallKind). Cheap after
/// the first call: the result is computed once and cached for the process.
///
/// Detection, in order:
///   Windows  — FinceptMaintenanceTool.exe beside the executable → WindowsIfw.
///   macOS    — executable inside "<X>.app/Contents/MacOS" → MacAppBundle
///              (can_self_update is false while Gatekeeper runs the app from
///              an AppTranslocation path, or straight from the mounted .dmg).
///   Linux    — /.flatpak-info exists → LinuxFlatpak;
///              executable inside the AppImage mount ($APPDIR, with $APPIMAGE
///              an existing file) → LinuxAppImage;
///              executable under the edition's /opt prefix owned by dpkg →
///              LinuxDeb, owned by rpm → LinuxRpm.
///              The AppImage and the .rpm are no longer published: those two
///              kinds get can_self_update = false and a blocked_reason that
///              names the Flatpak / .deb and the release page.
///   Anything else → Unknown.
const InstallContext& install_context();

/// "<os>-<arch>" of the running build, or empty when unsupported.
QString current_platform_key();

} // namespace fincept::services::updater
