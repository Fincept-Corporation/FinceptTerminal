#pragma once

// Private to src/services/updater: the per-platform half of UpdateInstaller.
// Exactly one of UpdateInstaller_win.cpp / UpdateInstaller_mac.cpp /
// UpdateInstaller_linux.cpp is compiled for a given target (CMakeLists.txt,
// _FINCEPT_UPDATER_PLATFORM_SOURCES) and defines create_platform_installer().

#include "services/updater/UpdateTypes.h"

class QObject;

namespace fincept::services::updater {

class UpdateInstaller;

/// The installer for `context.kind` on this platform, or nullptr when this
/// platform cannot apply that kind of installation in place.
UpdateInstaller* create_platform_installer(const InstallContext& context, QObject* parent);

} // namespace fincept::services::updater
