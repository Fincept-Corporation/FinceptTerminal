#pragma once

#include "app/PhaseOneServerCli.h"
#include "core/config/AppPaths.h"

namespace fincept {

struct PhaseOneResolvedServerOptions {
    QString host;
    quint16 port = 45450;
    QString db_path;
    QString server_root;
    QString requested_profile;
    bool ignored_profile = false;
    AppPaths::Overrides app_path_overrides;
};

class PhaseOneServerPaths {
  public:
    static PhaseOneResolvedServerOptions resolve(const PhaseOneServerCliOptions& options);
};

} // namespace fincept
