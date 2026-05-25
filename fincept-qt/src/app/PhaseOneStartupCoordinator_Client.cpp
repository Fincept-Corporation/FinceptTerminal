#include "app/PhaseOneClientLifecycle.h"
#include "app/PhaseOneClientStartup.h"
#include "app/PhaseOneClientUiStartup.h"
#include "app/PhaseOneStartupCoordinator.h"

#include "core/crash/CrashHandler.h"

#include <QApplication>
#include <QCoreApplication>

namespace fincept {

void PhaseOneStartupCoordinator::initialize_client_pre_app(const PhaseOneServerCliOptions& options) const {
    PhaseOneClientStartup::initialize_pre_app(options);
    crash::install();
    fprintf(stderr, "[PhaseOneStartup] crash handler installed for client mode\n");
    PhaseOneClientUiStartup::initialize_pre_app();
}

int PhaseOneStartupCoordinator::run_client(QApplication& app) const {
    app.setApplicationName(QStringLiteral("FinceptTerminal"));
    app.setOrganizationName(QStringLiteral("Fincept"));
#ifndef FINCEPT_VERSION_STRING
#    define FINCEPT_VERSION_STRING "0.0.0-dev"
#endif
    app.setApplicationVersion(QStringLiteral(FINCEPT_VERSION_STRING));
    app.setQuitOnLastWindowClosed(false);

    initialize_post_app();

    PhaseOneClientLifecycle lifecycle;
    if (!lifecycle.acquire_primary(QCoreApplication::arguments()))
        return 0;

    PhaseOneClientStartup startup;
    const PhaseOneClientStartup::Result startup_result = startup.initialize(app, *this);
    lifecycle.initialize_session_guard();

    PhaseOneClientUiStartup ui_startup;
    return ui_startup.run(app, lifecycle, startup, startup_result.setup_status);
}

} // namespace fincept
