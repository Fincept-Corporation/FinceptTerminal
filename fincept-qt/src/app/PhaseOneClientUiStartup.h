#pragma once

#include "python/PythonSetupManager.h"

class QApplication;

namespace fincept {

class PhaseOneClientLifecycle;
class PhaseOneClientStartup;

class PhaseOneClientUiStartup {
  public:
    static void initialize_pre_app();

    int run(QApplication& app, PhaseOneClientLifecycle& lifecycle,
            const PhaseOneClientStartup& startup,
            const python::SetupStatus& setup_status) const;
};

} // namespace fincept
