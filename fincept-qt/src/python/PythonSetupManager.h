// src/python/PythonSetupManager.h
//
// First-run Python environment bootstrapping — UV-first approach.
//
// Optimized flow (vs Tauri version):
//   1. Download UV standalone binary (~13MB) — single download, no pip/get-pip.py
//   2. uv python install 3.12                — UV handles Python download internally
//   3. uv venv venv-numpy1 + venv-numpy2     — PARALLEL venv creation
//   4. uv pip install requirements            — PARALLEL package install (UV is 10-100x faster than pip)
//
// Eliminates: get-pip.py download, pip bootstrap, "pip install uv" step.
// Estimated setup time: 3-5 minutes (down from 10-20 minutes).
#pragma once

#include <QObject>
#include <QString>

namespace fincept::python {

/// Status returned by check_status()
struct SetupStatus {
    bool uv_installed = false;
    bool python_installed = false;
    bool venv_numpy1_ready = false;
    bool venv_numpy2_ready = false;
    bool needs_setup = true;
    bool needs_package_sync = false; // true if requirements files changed since last install
    QString python_version;
    QString install_dir;
};

/// Progress emitted during setup
struct SetupProgress {
    QString step; // "uv", "python", "venv", "packages-numpy1", "packages-numpy2", "complete"
    int progress; // 0-100
    QString message;
    bool is_error = false;
};

class PythonSetupManager : public QObject {
    Q_OBJECT
  public:
    static PythonSetupManager& instance();

    /// Check current installation status (fast, synchronous)
    SetupStatus check_status() const;

    /// Run the full setup (async, emits progress signals)
    void run_setup();

    /// Get the install directory (com.fincept.terminal)
    QString install_dir() const;

    /// Get UV executable path
    QString uv_path() const;

    /// Get Python executable path for a given venv
    QString python_path(const QString& venv_name = "venv-numpy2") const;

    /// Get the UV-managed Python executable (not venv)
    QString base_python_path() const;

  signals:
    void progress_changed(const SetupProgress& progress);
    void setup_complete(bool success, const QString& error);

  private:
    explicit PythonSetupManager(QObject* parent = nullptr);
    Q_DISABLE_COPY(PythonSetupManager)

    void emit_progress(const QString& step, int pct, const QString& msg, bool err = false);

    // Installation steps
    bool download_uv();
    bool install_python_via_uv();
    bool create_venv(const QString& venv_name);
    bool install_packages(const QString& venv_name, const QString& requirements_file);
    QString find_requirements_file(const QString& filename) const;

    // Helpers
    bool run_command(const QString& program, const QStringList& args, const QStringList& env_vars = {}) const;
    // Like run_command but captures stderr — used for per-package failure diagnosis.
    bool run_command_capture(const QString& program, const QStringList& args,
                             const QStringList& env_vars, QString& stderr_out) const;
    QString download_file(const QString& url, const QString& dest_path) const;

    // Two-pass resilient package install helpers
    QStringList read_packages_from_file(const QString& req_path) const;
    QStringList install_packages_individually(const QString& venv_name,
                                              const QStringList& packages,
                                              const QStringList& env_vars,
                                              const QString& step_key);

    // Requirements hash tracking — detect when requirements change across app versions
    QString compute_requirements_hash(const QString& filename) const;
    QString load_stored_hash(const QString& venv_name) const;
    void save_hash(const QString& venv_name, const QString& hash) const;
    bool check_requirements_changed() const;

    static constexpr const char* kPythonVersion = "3.12";
    static constexpr const char* kUvVersion = "0.7.12";
};

} // namespace fincept::python
