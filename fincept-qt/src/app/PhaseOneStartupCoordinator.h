#pragma once

#include "app/PhaseOneServerCli.h"

#include <optional>

class QApplication;

namespace fincept {

class PhaseOneStartupCoordinator {
  public:
    void initialize_pre_app() const;
    void initialize_client_pre_app(const PhaseOneServerCliOptions& options) const;
    void initialize_post_app() const;
    void configure_client_logging() const;
    void install_qt_message_handler() const;
    std::optional<int> maybe_run(int& argc, char* argv[], const PhaseOneServerCliOptions& options) const;
    int run_client(QApplication& app) const;
};

} // namespace fincept
