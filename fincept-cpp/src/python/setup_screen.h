#pragma once
// First-run setup screen — ImGui UI for Python/packages installation
// Shows progress, allows skip, integrates with App state machine

#include <thread>
#include <atomic>

namespace fincept::python {

class SetupScreen {
public:
    /// Render the setup screen. Returns true when setup is done (or skipped).
    bool render();

    /// Check if setup is needed (call once on app startup)
    bool needs_setup();

private:
    bool checked_ = false;
    bool setup_needed_ = false;
    bool setup_started_ = false;
    bool setup_done_ = false;
    std::thread setup_thread_;
};

} // namespace fincept::python
