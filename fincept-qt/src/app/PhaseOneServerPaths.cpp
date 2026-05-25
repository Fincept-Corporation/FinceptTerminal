#include "app/PhaseOneServerPaths.h"

namespace fincept {

PhaseOneResolvedServerOptions PhaseOneServerPaths::resolve(const PhaseOneServerCliOptions& options) {
    PhaseOneResolvedServerOptions resolved;
    resolved.host = options.host.trimmed();
    resolved.port = options.port;
    resolved.requested_profile = options.profile.trimmed();
    resolved.ignored_profile = options.mode == PhaseOneProcessMode::Server && !resolved.requested_profile.isEmpty();
    resolved.server_root = AppPaths::root() + QStringLiteral("/server");
    resolved.db_path = options.db_path.trimmed();
    if (resolved.db_path.isEmpty())
        resolved.db_path = resolved.server_root + QStringLiteral("/phase1-server.db");
    resolved.app_path_overrides.data = resolved.server_root;
    resolved.app_path_overrides.logs = resolved.server_root + QStringLiteral("/logs");
    resolved.app_path_overrides.files = resolved.server_root + QStringLiteral("/files");
    resolved.app_path_overrides.cache = resolved.server_root + QStringLiteral("/cache");
    resolved.app_path_overrides.runtime = resolved.server_root + QStringLiteral("/runtime");
    resolved.app_path_overrides.workspaces = resolved.server_root + QStringLiteral("/workspaces");
    resolved.app_path_overrides.crashdumps = resolved.server_root + QStringLiteral("/crashdumps");
    return resolved;
}

} // namespace fincept
