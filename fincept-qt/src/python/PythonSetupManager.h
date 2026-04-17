// src/python/PythonSetupManager.h
//
// First-run Python environment bootstrapping — UV-first approach.
//
// Flow:
//   1. Download UV standalone binary (~13MB) — single download, no pip/get-pip.py
//   2. uv python install 3.12                — UV handles Python download internally
//   3. uv venv venv-numpy1 + venv-numpy2     — PARALLEL venv creation
//   4. uv pip install requirements            — PARALLEL package install (UV is 10-100x faster than pip)
//
// Estimated setup time: 3-5 minutes.
#pragma once

#include <QMap>
#include <QObject>
#include <QString>

namespace fincept::python {

/// Status returned by check_status()
struct SetupStatus {
    bool uv_installed = false;
    bool python_installed = false;
    bool venv_numpy1_created = false;   // venv directory and python exe exist
    bool venv_numpy2_created = false;
    bool venv_numpy1_ready = false;     // venv created AND current requirements hash matches marker
    bool venv_numpy2_ready = false;
    bool needs_setup = true;
    bool needs_package_sync = false;    // infra OK but ≥1 venv has a stale requirements hash
    QString python_version;
    QString install_dir;
};

/// Progress emitted during setup
struct SetupProgress {
    QString step;       // "uv", "python", "venv", "packages-numpy1", "packages-numpy2", "complete"
    int     progress = 0; // 0-100 — default-initialized to prevent garbage reads
    QString message;
    bool    is_error = false;
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
    Q_DISABLE_COPY_MOVE(PythonSetupManager)

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
    bool run_command_capture(const QString& program, const QStringList& args, const QStringList& env_vars,
                             QString& stderr_out) const;
    QString download_file(const QString& url, const QString& dest_path) const;

    // Two-pass resilient package install helpers
    QStringList read_packages_from_file(const QString& req_path) const;
    QStringList install_packages_individually(const QString& venv_name, const QStringList& packages,
                                              const QStringList& env_vars, const QString& step_key);

    // Hash-based marker helpers — marker stores the SHA-256 of the requirements
    // file used for the last successful install; stale or absent → reinstall.
    QString compute_requirements_hash(const QString& filename) const;
    QString read_marker_hash(const QString& venv_name) const;
    void    write_marker_hash(const QString& venv_name, const QString& req_hash) const;

    // Slow-path package verification via `uv pip list` — checks every package
    // in requirements_file is actually present in the given venv.
    bool verify_packages_installed(const QString& venv_name,
                                   const QString& requirements_file) const;

    static constexpr const char* kPythonVersion = "3.12";
    static constexpr const char* kUvVersion = "0.7.12";

    // Session-lifetime caches — requirements files never change at runtime.
    mutable QString cached_python_path_;                  // cleared after fresh Python install
    mutable QMap<QString, QString> cached_req_paths_;     // filename → resolved absolute path
    mutable QMap<QString, QString> cached_req_hashes_;    // filename → SHA-256 hex
};

} // namespace fincept::python
