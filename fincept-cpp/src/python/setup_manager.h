#pragma once
// Python setup manager — mirrors setup/ module from Tauri app
// Downloads Python, creates venvs, installs packages on first run

#include <string>
#include <filesystem>
#include <functional>
#include <atomic>
#include <mutex>

namespace fincept::python {

namespace fs = std::filesystem;

// ============================================================================
// Setup progress
// ============================================================================

struct SetupProgress {
    std::string step;       // "python", "uv", "venv", "packages", "complete"
    int progress = 0;       // 0-100
    std::string message;
    bool is_error = false;
};

struct SetupStatus {
    bool python_installed = false;
    bool packages_installed = false;
    bool needs_setup = true;
    std::string python_version;
};

// ============================================================================
// Setup manager
// ============================================================================

class SetupManager {
public:
    static SetupManager& instance();

    /// Check current installation status
    SetupStatus check_status();

    /// Run full setup (blocking — call from background thread)
    bool run_setup();

    /// Get current progress (thread-safe)
    SetupProgress get_progress() const;

    /// Is setup currently running?
    bool is_running() const { return running_.load(); }

    /// Was setup completed successfully?
    bool is_complete() const { return complete_.load(); }

    /// Skip setup (use app without Python)
    void skip_setup() { skipped_ = true; complete_ = true; }
    bool was_skipped() const { return skipped_.load(); }

    SetupManager(const SetupManager&) = delete;
    SetupManager& operator=(const SetupManager&) = delete;

private:
    SetupManager() = default;

    void emit(const std::string& step, int progress, const std::string& msg, bool error = false);

    // Setup steps
    bool install_python();
    bool install_uv();
    bool create_venv(const std::string& venv_name);
    bool install_packages(const std::string& venv_name, const std::string& requirements_file, const std::string& label);
    bool download_file(const std::string& url, const fs::path& dest);
    bool run_command(const std::string& cmd, std::string* output = nullptr);

    // Check helpers
    bool check_python_installed();
    bool check_packages_installed();
    std::string get_python_version();

    mutable std::mutex progress_mutex_;
    SetupProgress current_progress_;
    std::atomic<bool> running_{false};
    std::atomic<bool> complete_{false};
    std::atomic<bool> skipped_{false};
};

} // namespace fincept::python
