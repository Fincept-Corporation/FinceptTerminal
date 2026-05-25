#pragma once

#include "app/PhaseOneServerCli.h"
#include "python/PythonSetupManager.h"

class QApplication;

namespace fincept {

class PhaseOneStartupCoordinator;

class PhaseOneClientStartup {
  public:
    struct Result {
        python::SetupStatus setup_status;
    };

    static void initialize_pre_app(const PhaseOneServerCliOptions& options);

    Result initialize(QApplication& app, const PhaseOneStartupCoordinator& coordinator) const;
    void schedule_agent_discovery(QApplication& app) const;
};

} // namespace fincept
