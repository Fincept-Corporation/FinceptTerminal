// src/python/PythonSetupManager.cpp
//
// UV-first Python setup: download one binary, UV does the rest.
// Parallel venv creation + package installation.
#include "python/PythonSetupManager.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QMetaObject>
#include <QProcess>
#include <QSysInfo>
#include <QtConcurrent>

#include <atomic>

namespace fincept::python {

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

PythonSetupManager& PythonSetupManager::instance() {
    static PythonSetupManager s;
    return s;
}

PythonSetupManager::PythonSetupManager(QObject* parent) : QObject(parent) {}

// ─────────────────────────────────────────────────────────────────────────────
// Install directory — matches Tauri's com.fincept.terminal location
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::install_dir() const {
    return fincept::AppPaths::root();
}

// ─────────────────────────────────────────────────────────────────────────────
// Paths
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::uv_path() const {
#ifdef _WIN32
    return install_dir() + "/uv/uv.exe";
#else
    return install_dir() + "/uv/uv";
#endif
}

QString PythonSetupManager::base_python_path() const {
    // UV-managed Python lives under UV's python dir
    // We'll resolve it dynamically via `uv python find`
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("UV_PYTHON_INSTALL_DIR", install_dir() + "/python");
    proc.setProcessEnvironment(env);
    proc.start(uv_path(), {"python", "find", kPythonVersion});
    if (proc.waitForFinished(10000) && proc.exitCode() == 0) {
        return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }

    // Fallback: check known locations
#ifdef _WIN32
    // UV installs Python to UV_PYTHON_INSTALL_DIR
    QDir py_dir(install_dir() + "/python");
    auto entries = py_dir.entryList({"cpython-3.12*"}, QDir::Dirs);
    if (!entries.isEmpty()) {
        return py_dir.filePath(entries.first() + "/python.exe");
    }
#else
    QDir py_dir(install_dir() + "/python");
    auto entries = py_dir.entryList({"cpython-3.12*"}, QDir::Dirs);
    if (!entries.isEmpty()) {
        return py_dir.filePath(entries.first() + "/bin/python3");
    }
#endif
    return {};
}

QString PythonSetupManager::python_path(const QString& venv_name) const {
#ifdef _WIN32
    return install_dir() + "/" + venv_name + "/Scripts/python.exe";
#else
    return install_dir() + "/" + venv_name + "/bin/python3";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Check status (fast, synchronous)
// ─────────────────────────────────────────────────────────────────────────────

// Sentinel file written after a successful full setup. Its presence means we
// can skip the slow per-venv import checks on every subsequent launch.
static QString sentinel_path_for(const QString& install_dir) {
    return install_dir + "/.setup_complete";
}

SetupStatus PythonSetupManager::check_status() const {
    SetupStatus status;
    status.install_dir = install_dir();

    // ── Fast path: sentinel file written by run_setup() on success ───────────
    // If it exists, trust that setup completed. Still do lightweight file-exists
    // checks to catch a partially-deleted installation.
    bool sentinel_exists = QFileInfo::exists(sentinel_path_for(install_dir()));
    status.uv_installed = QFileInfo::exists(uv_path());
    status.venv_numpy1_ready = QFileInfo::exists(python_path("venv-numpy1"));
    status.venv_numpy2_ready = QFileInfo::exists(python_path("venv-numpy2"));

    if (sentinel_exists && status.uv_installed && status.venv_numpy1_ready && status.venv_numpy2_ready) {
        // Everything looks intact — skip spawning any Python processes.
        status.python_installed = true;
        status.needs_setup = false;
        status.needs_package_sync = check_requirements_changed();
        LOG_INFO("PythonSetup",
                 QString("Fast-path OK (sentinel present). needs_sync=%1").arg(status.needs_package_sync));
        return status;
    }

    // ── Slow path: first run or broken install — do full verification ─────────
    LOG_INFO("PythonSetup", "Sentinel absent or install incomplete — running full check");

    // Check Python (via UV) — run uv python find on a background-safe call
    // (this function is only reached on first run, so blocking is acceptable)
    if (status.uv_installed) {
        QString py = base_python_path();
        if (!py.isEmpty() && QFileInfo::exists(py)) {
            QProcess proc;
#ifdef _WIN32
            proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
                cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
            });
#endif
            proc.start(py, {"--version"});
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                status.python_installed = true;
                status.python_version = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            }
        }
    }

    // Check venvs — verify python runs AND key packages are importable
    auto check_venv = [this](const QString& name, const QStringList& marker_packages) -> bool {
        QString vpy = python_path(name);
        if (!QFileInfo::exists(vpy))
            return false;

        QProcess ver_proc;
#ifdef _WIN32
        ver_proc.setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments* cpa) { cpa->flags |= 0x08000000; });
#endif
        ver_proc.start(vpy, {"--version"});
        if (!ver_proc.waitForFinished(5000) || ver_proc.exitCode() != 0)
            return false;

        QStringList imports;
        for (const auto& pkg : marker_packages)
            imports << ("import " + pkg);
        QString check_code = imports.join("; ") + "; print('OK')";

        QProcess pkg_proc;
#ifdef _WIN32
        pkg_proc.setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments* cpa) { cpa->flags |= 0x08000000; });
#endif
        pkg_proc.start(vpy, {"-c", check_code});
        if (!pkg_proc.waitForFinished(15000) || pkg_proc.exitCode() != 0) {
            QString err = QString::fromUtf8(pkg_proc.readAllStandardError());
            LOG_WARN("PythonSetup", QString("Venv %1 missing packages: %2").arg(name, err.left(200)));
            return false;
        }
        return QString::fromUtf8(pkg_proc.readAllStandardOutput()).trimmed().contains("OK");
    };

    status.venv_numpy1_ready = check_venv("venv-numpy1", {"numpy", "pandas", "vectorbt"});
    status.venv_numpy2_ready = check_venv("venv-numpy2", {"numpy", "pandas", "requests", "sklearn"});

    status.needs_setup =
        !status.uv_installed || !status.python_installed || !status.venv_numpy1_ready || !status.venv_numpy2_ready;

    if (!status.needs_setup) {
        status.needs_package_sync = check_requirements_changed();
    }

    LOG_INFO("PythonSetup", QString("Full check: uv=%1 py=%2 venv1=%3 venv2=%4 needs_setup=%5 needs_sync=%6")
                                .arg(status.uv_installed)
                                .arg(status.python_installed)
                                .arg(status.venv_numpy1_ready)
                                .arg(status.venv_numpy2_ready)
                                .arg(status.needs_setup)
                                .arg(status.needs_package_sync));
    return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// Run full setup (background thread)
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::run_setup() {
    QPointer<PythonSetupManager> self = this;

    QtConcurrent::run([self]() {
        if (!self)
            return;

        auto fail = [&](const QString& msg) {
            QMetaObject::invokeMethod(
                self,
                [self, msg]() {
                    if (self)
                        emit self->setup_complete(false, msg);
                },
                Qt::QueuedConnection);
        };

        auto status = self->check_status();

        // ── Step 1: Download UV standalone binary (~13MB) ────────────────────
        if (!status.uv_installed) {
            self->emit_progress("uv", 0, "Downloading UV package manager...");
            if (!self->download_uv()) {
                self->emit_progress("uv", 0, "Failed to download UV", true);
                fail("UV download failed");
                return;
            }
            self->emit_progress("uv", 100, "UV ready");
        } else {
            self->emit_progress("uv", 100, "UV already installed");
        }

        // ── Step 2: Install Python via UV ────────────────────────────────────
        if (!status.python_installed) {
            self->emit_progress("python", 0, "Installing Python 3.12 via UV...");
            if (!self->install_python_via_uv()) {
                self->emit_progress("python", 0, "Failed to install Python", true);
                fail("Python installation failed");
                return;
            }
            self->emit_progress("python", 100, "Python 3.12 installed");
        } else {
            self->emit_progress("python", 100, "Python already installed: " + status.python_version);
        }

        // ── Step 3: Create both venvs in PARALLEL ───────────────────────────
        bool need_venv1 = !status.venv_numpy1_ready;
        bool need_venv2 = !status.venv_numpy2_ready;

        if (need_venv1 || need_venv2) {
            self->emit_progress("venv", 0, "Creating virtual environments...");

            std::atomic<bool> v1_ok{!need_venv1}; // already OK if not needed
            std::atomic<bool> v2_ok{!need_venv2};

            QFuture<void> f1, f2;
            if (need_venv1) {
                f1 = QtConcurrent::run([self, &v1_ok]() { v1_ok = self && self->create_venv("venv-numpy1"); });
            }
            if (need_venv2) {
                f2 = QtConcurrent::run([self, &v2_ok]() { v2_ok = self && self->create_venv("venv-numpy2"); });
            }

            if (need_venv1)
                f1.waitForFinished();
            if (need_venv2)
                f2.waitForFinished();

            if (!v1_ok || !v2_ok) {
                self->emit_progress("venv", 0, "Failed to create virtual environments", true);
                fail("Venv creation failed");
                return;
            }
            self->emit_progress("venv", 100, "Virtual environments created");
        } else {
            self->emit_progress("venv", 100, "Virtual environments already ready");
        }

        // ── Step 4: Install packages in PARALLEL ────────────────────────────
        bool need_pkg1 = !status.venv_numpy1_ready;
        bool need_pkg2 = !status.venv_numpy2_ready;

        std::atomic<bool> p1_ok{!need_pkg1};
        std::atomic<bool> p2_ok{!need_pkg2};

        if (need_pkg1) {
            self->emit_progress("packages-numpy1", 0, "Installing NumPy 1.x packages...");
        } else {
            self->emit_progress("packages-numpy1", 100, "NumPy 1.x packages ready");
        }
        if (need_pkg2) {
            self->emit_progress("packages-numpy2", 0, "Installing NumPy 2.x packages...");
        } else {
            self->emit_progress("packages-numpy2", 100, "NumPy 2.x packages ready");
        }

        // Run both package installations in parallel
        QFuture<void> pf1, pf2;
        if (need_pkg1) {
            pf1 = QtConcurrent::run([self, &p1_ok]() {
                p1_ok = self && self->install_packages("venv-numpy1", "requirements-numpy1.txt");
                if (self) {
                    self->emit_progress("packages-numpy1", p1_ok ? 100 : 0,
                                        p1_ok ? "NumPy 1.x packages installed" : "NumPy 1.x package install failed",
                                        !p1_ok);
                }
            });
        }
        if (need_pkg2) {
            pf2 = QtConcurrent::run([self, &p2_ok]() {
                p2_ok = self && self->install_packages("venv-numpy2", "requirements-numpy2.txt");
                if (self) {
                    self->emit_progress("packages-numpy2", p2_ok ? 100 : 0,
                                        p2_ok ? "NumPy 2.x packages installed" : "NumPy 2.x package install failed",
                                        !p2_ok);
                }
            });
        }

        if (need_pkg1)
            pf1.waitForFinished();
        if (need_pkg2)
            pf2.waitForFinished();

        if (!p1_ok || !p2_ok) {
            fail("Package installation failed");
            return;
        }

        // ── Save requirements hashes so we can detect changes on next app update ──
        QString h1 = self->compute_requirements_hash("requirements-numpy1.txt");
        QString h2 = self->compute_requirements_hash("requirements-numpy2.txt");
        if (!h1.isEmpty())
            self->save_hash("venv-numpy1", h1);
        if (!h2.isEmpty())
            self->save_hash("venv-numpy2", h2);

        // ── Write sentinel so future launches skip the slow import checks ────
        {
            QFile sentinel(sentinel_path_for(self->install_dir()));
            if (sentinel.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                sentinel.write("ok\n");
            }
        }

        // ── Done ────────────────────────────────────────────────────────────
        self->emit_progress("complete", 100, "Setup complete! All environments ready.");
        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (self)
                    emit self->setup_complete(true, {});
            },
            Qt::QueuedConnection);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 1: Download UV standalone binary
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::download_uv() {
    QString dir = install_dir() + "/uv";
    QDir().mkpath(dir);

    // Determine platform-specific archive
    QString target;
    QString ext;
#ifdef _WIN32
    target = "x86_64-pc-windows-msvc";
    ext = "zip";
#elif defined(__APPLE__)
    QString arch = QSysInfo::currentCpuArchitecture();
    target = (arch == "arm64") ? "aarch64-apple-darwin" : "x86_64-apple-darwin";
    ext = "tar.gz";
#else
    QString arch = QSysInfo::currentCpuArchitecture();
    target = (arch == "aarch64" || arch == "arm64") ? "aarch64-unknown-linux-musl" : "x86_64-unknown-linux-musl";
    ext = "tar.gz";
#endif

    QString archive_name = QString("uv-%1.%2").arg(target, ext);
    QString url = QString("https://github.com/astral-sh/uv/releases/download/%1/%2").arg(kUvVersion, archive_name);
    QString archive_path = dir + "/" + archive_name;

    LOG_INFO("PythonSetup", "Downloading UV from: " + url);
    emit_progress("uv", 20, "Downloading UV binary...");

    QString err = download_file(url, archive_path);
    if (!err.isEmpty()) {
        LOG_ERROR("PythonSetup", "UV download failed: " + err);
        return false;
    }

    emit_progress("uv", 60, "Extracting UV...");

    // Extract
#ifdef _WIN32
    if (!run_command("powershell",
                     {"-NoProfile", "-Command",
                      QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(archive_path, dir)})) {
        LOG_ERROR("PythonSetup", "Failed to extract UV");
        return false;
    }
    // The zip contains uv.exe inside a subfolder — move it up if needed
    QString nested = dir + "/uv-" + target + "/uv.exe";
    if (QFileInfo::exists(nested) && !QFileInfo::exists(uv_path())) {
        QFile::rename(nested, uv_path());
        // Also move uvx.exe if present
        QString nested_uvx = dir + "/uv-" + target + "/uvx.exe";
        if (QFileInfo::exists(nested_uvx)) {
            QFile::rename(nested_uvx, dir + "/uvx.exe");
        }
    }
#else
    if (!run_command("tar", {"xzf", archive_path, "-C", dir, "--strip-components=1"})) {
        LOG_ERROR("PythonSetup", "Failed to extract UV");
        return false;
    }
    // Make executable
    run_command("chmod", {"+x", uv_path()});
#endif

    QFile::remove(archive_path);

    if (!QFileInfo::exists(uv_path())) {
        LOG_ERROR("PythonSetup", "UV binary not found after extraction: " + uv_path());
        return false;
    }

    // Verify
    emit_progress("uv", 80, "Verifying UV...");
    if (!run_command(uv_path(), {"--version"})) {
        LOG_ERROR("PythonSetup", "UV verification failed");
        return false;
    }

    LOG_INFO("PythonSetup", "UV installed at: " + uv_path());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2: Install Python via UV (replaces manual download + get-pip.py)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::install_python_via_uv() {
    emit_progress("python", 20, "UV is downloading Python 3.12...");

    // Tell UV where to install Python
    QStringList env = {
        "UV_PYTHON_INSTALL_DIR=" + install_dir() + "/python",
    };

    if (!run_command(uv_path(), {"python", "install", kPythonVersion}, env)) {
        LOG_ERROR("PythonSetup", "uv python install failed");
        return false;
    }

    emit_progress("python", 80, "Verifying Python...");

    // Verify
    QString py = base_python_path();
    if (py.isEmpty() || !QFileInfo::exists(py)) {
        LOG_ERROR("PythonSetup", "Python not found after UV install");
        return false;
    }

    LOG_INFO("PythonSetup", "Python installed at: " + py);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 3: Create venv (can run in parallel)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::create_venv(const QString& venv_name) {
    QString venv_path = install_dir() + "/" + venv_name;

    // If the directory exists but is not a valid venv (no pyvenv.cfg), remove it.
    // This happens when a previous setup run created the dir but failed before
    // UV could populate it — UV refuses to overwrite a non-venv directory.
    if (QFileInfo::exists(venv_path) && !QFileInfo::exists(venv_path + "/pyvenv.cfg")) {
        LOG_WARN("PythonSetup", QString("Removing stale non-venv dir: %1").arg(venv_path));
        QDir(venv_path).removeRecursively();
    }

    QStringList env = {
        "UV_PYTHON_INSTALL_DIR=" + install_dir() + "/python",
    };

    if (run_command(uv_path(), {"venv", venv_path, "--python", kPythonVersion}, env)) {
        LOG_INFO("PythonSetup", "Created venv: " + venv_name);
        return true;
    }

    LOG_ERROR("PythonSetup", "Failed to create venv: " + venv_name);
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 4: Install packages (can run in parallel, UV handles internal parallelism)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::install_packages(const QString& venv_name, const QString& requirements_file) {
    QString req_path = find_requirements_file(requirements_file);
    if (req_path.isEmpty()) {
        LOG_ERROR("PythonSetup", "Requirements file not found: " + requirements_file);
        return false;
    }

    LOG_INFO("PythonSetup", QString("Installing packages into %1 from %2").arg(venv_name, req_path));

    QString venv_python = python_path(venv_name);
    QString step_key = venv_name.contains("numpy1") ? "packages-numpy1" : "packages-numpy2";

    QStringList env = {
        "UV_PYTHON_INSTALL_DIR=" + install_dir() + "/python",
        "PEEWEE_NO_SQLITE_EXTENSIONS=1",
        "PEEWEE_NO_C_EXTENSION=1",
    };

    // ── Pass 1: try installing everything at once (fast path) ────────────────
    // This succeeds on most machines and is 10-100x faster than one-by-one.
    LOG_INFO("PythonSetup", QString("[%1] Pass 1: bulk install from %2").arg(venv_name, requirements_file));
    emit_progress(step_key, 5, "Installing packages (bulk)...");

    QString bulk_stderr;
    bool bulk_ok = run_command_capture(
        uv_path(),
        {"pip", "install", "--python", venv_python, "-r", req_path},
        env,
        bulk_stderr);

    if (bulk_ok) {
        LOG_INFO("PythonSetup", QString("[%1] Bulk install succeeded").arg(venv_name));
        emit_progress(step_key, 100, "All packages installed");
        return true;
    }

    // ── Pass 2: bulk failed — install packages one-by-one, skip failures ────
    LOG_WARN("PythonSetup",
             QString("[%1] Bulk install failed (stderr: %2) — falling back to per-package install")
                 .arg(venv_name, bulk_stderr.left(300)));
    emit_progress(step_key, 10, "Bulk install failed — retrying package by package...");

    QStringList packages = read_packages_from_file(req_path);
    if (packages.isEmpty()) {
        LOG_ERROR("PythonSetup", "No packages parsed from: " + req_path);
        return false;
    }

    QStringList failed = install_packages_individually(venv_name, packages, env, step_key);

    if (failed.isEmpty()) {
        LOG_INFO("PythonSetup", QString("[%1] All packages installed individually").arg(venv_name));
        emit_progress(step_key, 100, "All packages installed");
    } else {
        LOG_WARN("PythonSetup",
                 QString("[%1] %2 package(s) skipped: %3")
                     .arg(venv_name)
                     .arg(failed.size())
                     .arg(failed.join(", ").left(300)));
        emit_progress(step_key, 100,
                      QString("Done — %1 package(s) skipped (see log)").arg(failed.size()));
    }

    // Always return true from pass 2 — a partial environment is more useful
    // than no environment. The skipped packages are logged for diagnosis.
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Read packages from a requirements file, stripping comments and blank lines.
// Returns one entry per non-empty, non-comment line.
// ─────────────────────────────────────────────────────────────────────────────

QStringList PythonSetupManager::read_packages_from_file(const QString& req_path) const {
    QFile file(req_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QStringList packages;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        // Skip blank lines, comments, and pip options (-r, --extra-index-url, etc.)
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('-'))
            continue;
        // Strip inline comments  e.g. "numpy>=1.26  # required by X"
        int comment_pos = line.indexOf('#');
        if (comment_pos > 0)
            line = line.left(comment_pos).trimmed();
        if (!line.isEmpty())
            packages << line;
    }
    return packages;
}

// ─────────────────────────────────────────────────────────────────────────────
// Install packages one by one, skipping any that fail.
// Returns the list of packages that failed (for logging).
// ─────────────────────────────────────────────────────────────────────────────

QStringList PythonSetupManager::install_packages_individually(const QString& venv_name,
                                                              const QStringList& packages,
                                                              const QStringList& env_vars,
                                                              const QString& step_key) {
    QString venv_python = python_path(venv_name);
    QStringList failed;
    int total = packages.size();

    for (int i = 0; i < total; ++i) {
        const QString& pkg = packages[i];
        int pct = 10 + static_cast<int>(90.0 * i / total);
        emit_progress(step_key, pct,
                      QString("Installing %1/%2: %3").arg(i + 1).arg(total).arg(pkg));

        QString pkg_stderr;
        bool ok = run_command_capture(
            uv_path(),
            {"pip", "install", "--python", venv_python, pkg},
            env_vars,
            pkg_stderr);

        if (ok) {
            LOG_INFO("PythonSetup", QString("[%1] Installed: %2").arg(venv_name, pkg));
        } else {
            LOG_WARN("PythonSetup",
                     QString("[%1] Skipped (failed): %2 — %3")
                         .arg(venv_name, pkg, pkg_stderr.left(200)));
            failed << pkg;
        }
    }

    return failed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Find requirements file
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::find_requirements_file(const QString& filename) const {
    QString exe_dir = QCoreApplication::applicationDirPath();

    // Search relative to executable
    QStringList candidates = {
        exe_dir + "/resources/" + filename,
        exe_dir + "/../resources/" + filename,
        exe_dir + "/../../resources/" + filename,
    };

    // Walk up from exe to find project root
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        QString c = dir.filePath("resources/" + filename);
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
        if (dir.isRoot())
            break;
    }

    for (const auto& c : candidates) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }

    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: run command (synchronous, used on background threads)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::run_command(const QString& program, const QStringList& args,
                                     const QStringList& env_vars) const {
    QProcess proc;

    if (!env_vars.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const auto& var : env_vars) {
            int eq = var.indexOf('=');
            if (eq > 0) {
                env.insert(var.left(eq), var.mid(eq + 1));
            }
        }
        proc.setProcessEnvironment(env);
    }

#ifdef _WIN32
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    LOG_INFO("PythonSetup", QString("Running: %1 %2").arg(program, args.join(" ")));

    proc.start(program, args);
    if (!proc.waitForFinished(30 * 60 * 1000)) { // 30 min timeout
        LOG_ERROR("PythonSetup", "Command timed out: " + program);
        proc.kill();
        return false;
    }

    if (proc.exitCode() != 0) {
        QString stderr_out = QString::fromUtf8(proc.readAllStandardError());
        LOG_ERROR("PythonSetup", QString("Command failed (exit %1): %2\nStderr: %3")
                                     .arg(proc.exitCode())
                                     .arg(program)
                                     .arg(stderr_out.left(500)));
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: run command, capture stderr (used for per-package failure diagnosis)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::run_command_capture(const QString& program, const QStringList& args,
                                             const QStringList& env_vars, QString& stderr_out) const {
    QProcess proc;

    if (!env_vars.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const auto& var : env_vars) {
            int eq = var.indexOf('=');
            if (eq > 0)
                env.insert(var.left(eq), var.mid(eq + 1));
        }
        proc.setProcessEnvironment(env);
    }

#ifdef _WIN32
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    proc.start(program, args);
    if (!proc.waitForFinished(30 * 60 * 1000)) {
        proc.kill();
        stderr_out = "timed out";
        return false;
    }

    stderr_out = QString::fromUtf8(proc.readAllStandardError());
    return proc.exitCode() == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: download file
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::download_file(const QString& url, const QString& dest_path) const {
#ifdef _WIN32
    QProcess proc;
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) { cpa->flags |= 0x08000000; });
    proc.start("powershell",
               {"-NoProfile", "-Command",
                QString("[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                        "Invoke-WebRequest -Uri '%1' -OutFile '%2' -UseBasicParsing")
                    .arg(url, dest_path)});
    if (!proc.waitForFinished(15 * 60 * 1000)) {
        proc.kill();
        return "Download timed out";
    }
    if (proc.exitCode() != 0) {
        return "Download failed: " + QString::fromUtf8(proc.readAllStandardError()).left(200);
    }
#else
    // Verify curl is available before launching — gives a clear error
    // instead of a cryptic "failed to start" message.
    {
        QProcess which;
        which.start("which", {"curl"});
        which.waitForFinished(3000);
        if (which.exitCode() != 0) {
            return "curl not found. Install it first:\n"
                   "  Ubuntu/Debian: sudo apt install curl\n"
                   "  Fedora/RHEL:   sudo dnf install curl\n"
                   "  macOS:         brew install curl  (or use system curl)";
        }
    }
    QProcess proc;
    // --retry 3: automatic retry on transient network errors
    // --connect-timeout 30: fail fast if server unreachable
    proc.start("curl", {"-L", "--fail", "--retry", "3",
                        "--connect-timeout", "30",
                        "-o", dest_path, url});
    if (!proc.waitForFinished(15 * 60 * 1000)) {
        proc.kill();
        return "Download timed out";
    }
    if (proc.exitCode() != 0) {
        return "curl failed: " + QString::fromUtf8(proc.readAllStandardError()).left(200);
    }
#endif

    if (!QFileInfo::exists(dest_path)) {
        return "Downloaded file not found: " + dest_path;
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Requirements hash tracking
// Detects when requirements files change across app versions so packages
// can be re-synced without a full reinstall.
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::compute_requirements_hash(const QString& filename) const {
    QString path = find_requirements_file(filename);
    if (path.isEmpty())
        return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray content = file.readAll();
    return QString(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
}

QString PythonSetupManager::load_stored_hash(const QString& venv_name) const {
    QString hash_file = install_dir() + "/" + venv_name + "/.requirements_hash";
    QFile file(hash_file);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

void PythonSetupManager::save_hash(const QString& venv_name, const QString& hash) const {
    QString hash_file = install_dir() + "/" + venv_name + "/.requirements_hash";
    QFile file(hash_file);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(hash.toUtf8());
    }
}

bool PythonSetupManager::check_requirements_changed() const {
    // Compare current requirements file hashes with stored ones
    QString hash1 = compute_requirements_hash("requirements-numpy1.txt");
    QString hash2 = compute_requirements_hash("requirements-numpy2.txt");

    if (hash1.isEmpty() || hash2.isEmpty())
        return false; // can't compute, skip

    QString stored1 = load_stored_hash("venv-numpy1");
    QString stored2 = load_stored_hash("venv-numpy2");

    bool changed = (stored1 != hash1) || (stored2 != hash2);
    if (changed) {
        LOG_INFO("PythonSetup", "Requirements files have changed since last install");
    }
    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit progress (thread-safe)
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::emit_progress(const QString& step, int pct, const QString& msg, bool err) {
    SetupProgress p{step, pct, msg, err};
    LOG_INFO("PythonSetup", QString("[%1 %2%] %3").arg(step).arg(pct).arg(msg));
    QMetaObject::invokeMethod(this, [this, p]() { emit progress_changed(p); }, Qt::QueuedConnection);
}

} // namespace fincept::python
