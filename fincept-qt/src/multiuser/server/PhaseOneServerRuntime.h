#pragma once

#include "app/PhaseOneServerPaths.h"

#include <memory>

namespace fincept::multiuser {

class PhaseOneHttpServer;
class PhaseOnePortfolioServer;

struct PhaseOneServerErrorContext {
    QString code;
    QString message;
};

struct PhaseOneServerStartResult {
    bool ok = false;
    PhaseOneServerErrorContext error_context;
};

class PhaseOneServerRuntime {
  public:
    PhaseOneServerRuntime();
    ~PhaseOneServerRuntime();

    PhaseOneServerStartResult start(const fincept::PhaseOneResolvedServerOptions& options);

  private:
    void register_routes();

    std::unique_ptr<PhaseOneHttpServer> http_server_;
    std::unique_ptr<PhaseOnePortfolioServer> portfolio_server_;
};

} // namespace fincept::multiuser

namespace fincept {
using PhaseOneServerStartResult = fincept::multiuser::PhaseOneServerStartResult;
using PhaseOneServerRuntime = fincept::multiuser::PhaseOneServerRuntime;
}
