#pragma once
// Hot Reload — watches source files, triggers rebuild, relaunches app

#include <string>

namespace fincept::core {

class HotReload {
public:
    static HotReload& instance() {
        static HotReload hr;
        return hr;
    }

    // Start watching source files for changes
    void initialize();

    // Stop the watcher thread
    void shutdown();

    // Check if a successful rebuild happened and app should relaunch
    bool should_relaunch() const;

    // Relaunch the executable
    void relaunch();

    // Status for UI display
    std::string status() const;
    bool is_rebuilding() const;

    HotReload(const HotReload&) = delete;
    HotReload& operator=(const HotReload&) = delete;

private:
    HotReload() = default;
};

} // namespace fincept::core
