#include "app/PhaseOneServerCli.h"
#include "app/PhaseOneStartupCoordinator.h"

#include <QApplication>

#include <cstdio>

int main(int argc, char* argv[]) {
    const auto cli = fincept::PhaseOneServerCli::parse(argc, argv);
    if (!cli.ok) {
        fprintf(stderr, "[PhaseOneServerCli] %s\n", qUtf8Printable(cli.message));
        return 1;
    }

    fincept::PhaseOneStartupCoordinator startup_coordinator;
    startup_coordinator.initialize_pre_app();
    if (const auto server_exit = startup_coordinator.maybe_run(argc, argv, cli.options); server_exit.has_value())
        return *server_exit;

    startup_coordinator.initialize_client_pre_app(cli.options);
    QApplication app(argc, argv);
    return startup_coordinator.run_client(app);
}
