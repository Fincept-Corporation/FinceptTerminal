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
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>
#include <QThread>
#include <QtConcurrent>

#include <algorithm>
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
// Install directory — com.fincept.terminal
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
    // Guard: this function may block up to 10s — must not be called from the UI thread.
    // Acceptable call sites: main() before exec(), background threads, QtConcurrent::run().
    Q_ASSERT(QCoreApplication::instance() == nullptr ||
             QThread::currentThread() != QCoreApplication::instance()->thread());

    // Return cached result if already resolved this session.
    if (!cached_python_path_.isEmpty())
        return cached_python_path_;

    // Resolve via `uv python find` — spawns a process, so we only do this once.
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("UV_PYTHON_INSTALL_DIR", install_dir() + "/python");
    proc.setProcessEnvironment(env);
    proc.start(uv_path(), {"python", "find", kPythonVersion});
    if (proc.waitForFinished(10000) && proc.exitCode() == 0) {
        cached_python_path_ = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        return cached_python_path_;
    }

    // Fallback: scan known install directory for the cpython-3.12 subdirectory.
#ifdef _WIN32
    QDir py_dir(install_dir() + "/python");
    auto entries = py_dir.entryList({"cpython-3.12*"}, QDir::Dirs);
    if (!entries.isEmpty()) {
        cached_python_path_ = py_dir.filePath(entries.first() + "/python.exe");
        return cached_python_path_;
    }
#else
    QDir py_dir(install_dir() + "/python");
    auto entries = py_dir.entryList({"cpython-3.12*"}, QDir::Dirs);
    if (!entries.isEmpty()) {
        cached_python_path_ = py_dir.filePath(entries.first() + "/bin/python3");
        return cached_python_path_;
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

// Shared UV env — applied to every `uv` invocation so download/install behaviour
// is consistent across platforms. See header for per-var rationale.
//
// Concurrency is clamped on both ends so the same defaults are safe on a 2-core
// / 4 GB netbook and a 32-core workstation:
//   - installs:  floor 2  (so single-core CPUs still parallelise I/O), ceiling 8
//                (more workers don't help — install is disk-bound — but they
//                 do consume RAM unpacking torch/catboost wheels in parallel).
//   - downloads: ceiling 8 too. UV's own default is 50; capping prevents a
//                slow / metered connection on a low-end machine from opening
//                dozens of TLS sockets at once.
QStringList PythonSetupManager::uv_env_extra() const {
    const QString root = install_dir();
    const int cores = QThread::idealThreadCount();
    const int installs = std::clamp(cores, 2, 8);
    const int downloads = std::clamp(cores * 2, 4, 8);
    return {
        "UV_PYTHON_INSTALL_DIR=" + root + "/python",
        "UV_CACHE_DIR=" + root + "/uv-cache",
        "UV_LINK_MODE=hardlink",
        "UV_COMPILE_BYTECODE=1",
        "UV_CONCURRENT_DOWNLOADS=" + QString::number(downloads),
        "UV_CONCURRENT_INSTALLS=" + QString::number(installs),
        "UV_HTTP_TIMEOUT=120",
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Marker / hash helpers
//
// The .packages_installed marker now stores the SHA-256 hex of the requirements
// file that was used for the last *successful* install.  A venv is "ready" only
// when this hash matches the current requirements file on disk.
//
// Old installs wrote "ok\n" — that never matches a 64-char hex string, so the
// first launch after updating to this code transparently migrates via the slow
// path (uv pip list check), then writes the real hash.
// ─────────────────────────────────────────────────────────────────────────────

static QString sentinel_path_for(const QString& install_dir) {
    return install_dir + "/.setup_complete";
}

static QString packages_marker_for(const QString& install_dir, const QString& venv_name) {
    return install_dir + "/" + venv_name + "/.packages_installed";
}

QString PythonSetupManager::read_marker_hash(const QString& venv_name) const {
    QFile f(packages_marker_for(install_dir(), venv_name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

void PythonSetupManager::write_marker_hash(const QString& venv_name, const QString& req_hash) const {
    if (req_hash.isEmpty())
        return;
    QFile f(packages_marker_for(install_dir(), venv_name));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write((req_hash + "\n").toUtf8());
}

// ─────────────────────────────────────────────────────────────────────────────
// verify_packages_installed — slow-path cross-check via `uv pip list`
//
// Reads every package name from requirements_file, then asks UV what is
// actually installed in the venv.  Returns true only if every required package
// is present.  This runs on first launch, after marker deletion, or after a
// partial install — never on the fast path.
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::verify_packages_installed(const QString& venv_name,
                                                    const QString& requirements_file) const {
    QString req_path = find_requirements_file(requirements_file);
    if (req_path.isEmpty()) {
        LOG_WARN("PythonSetup",
                 QString("[%1] verify: requirements file not found: %2").arg(venv_name, requirements_file));
        return false;
    }

    QStringList expected = read_packages_from_file(req_path);
    if (expected.isEmpty())
        return false;

    // Run `uv pip list --python <venv_python>` — output is "name   version" per line.
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("UV_PYTHON_INSTALL_DIR", install_dir() + "/python");
    proc.setProcessEnvironment(env);
#ifdef _WIN32
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    proc.start(uv_path(), {"pip", "list", "--python", python_path(venv_name)});
    // 90s — generous headroom for large venvs (300+ packages) on slow disks or
    // when Windows Defender is scanning the venv directory.  A false 30s timeout
    // previously marked a correctly-installed venv as broken, forcing a full reinstall.
    if (!proc.waitForFinished(90000) || proc.exitCode() != 0) {
        LOG_WARN("PythonSetup",
                 QString("[%1] verify: uv pip list failed or timed out").arg(venv_name));
        return false;
    }

    // Build a normalised set of installed package names.
    // PEP 503: canonical name = lowercase, replace [-_.] with a single '-'.
    // We normalise both sides to lowercase + replace '-'/'.' with '_' for
    // a simple consistent comparison.
    auto normalise = [](const QString& name) -> QString {
        QString n = name.toLower();
        n.replace('-', '_');
        n.replace('.', '_');
        return n;
    };

    QSet<QString> installed;
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
        const QString name = line.split(' ', Qt::SkipEmptyParts).value(0);
        if (!name.isEmpty())
            installed.insert(normalise(name));
    }

    // Check each required package.  Strip version specifiers and extras before
    // comparing: "numpy>=1.26,<2.0" → "numpy", "scikit-learn>=1.4" → "scikit_learn".
    static const QRegularExpression kVersionRe(R"([><=!~\s\[].*)");
    QStringList missing;
    for (const QString& pkg : expected) {
        QString base = pkg;
        base.remove(kVersionRe);
        base = normalise(base.trimmed());
        if (base.isEmpty())
            continue;
        if (!installed.contains(base))
            missing << base;
    }

    if (!missing.isEmpty()) {
        LOG_WARN("PythonSetup",
                 QString("[%1] verify: %2 package(s) missing: %3")
                     .arg(venv_name).arg(missing.size()).arg(missing.join(", ").left(400)));
        return false;
    }

    LOG_INFO("PythonSetup",
             QString("[%1] verify: all %2 required packages present").arg(venv_name).arg(expected.size()));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// check_status (fast, synchronous)
// ─────────────────────────────────────────────────────────────────────────────

SetupStatus PythonSetupManager::check_status() const {
    SetupStatus status;
    status.install_dir = install_dir();

    LOG_INFO("PythonSetup", "=== check_status BEGIN === install_dir: " + status.install_dir);

    // ── File-existence checks (always fast) ───────────────────────────────────
    status.uv_installed        = QFileInfo::exists(uv_path());
    status.venv_numpy1_created = QFileInfo::exists(python_path("venv-numpy1"));
    status.venv_numpy2_created = QFileInfo::exists(python_path("venv-numpy2"));

    LOG_INFO("PythonSetup",
             QString("Existence: uv=%1 (%2)  venv1_python=%3 (%4)  venv2_python=%5 (%6)")
                 .arg(status.uv_installed ? "YES" : "NO", uv_path())
                 .arg(status.venv_numpy1_created ? "YES" : "NO", python_path("venv-numpy1"))
                 .arg(status.venv_numpy2_created ? "YES" : "NO", python_path("venv-numpy2")));

    // ── Compute current requirements hashes ───────────────────────────────────
    // If a file is not found the hash is empty — treated as "not current",
    // which forces reinstall (correct) rather than silently skipping.
    const QString req_hash1 = compute_requirements_hash("requirements-numpy1.txt");
    const QString req_hash2 = compute_requirements_hash("requirements-numpy2.txt");

    LOG_INFO("PythonSetup",
             QString("Requirements hashes — numpy1: %1  numpy2: %2")
                 .arg(req_hash1.isEmpty() ? "(file not found)" : req_hash1.left(16) + "...")
                 .arg(req_hash2.isEmpty() ? "(file not found)" : req_hash2.left(16) + "..."));

    if (req_hash1.isEmpty())
        LOG_WARN("PythonSetup",
                 "requirements-numpy1.txt NOT FOUND — find_requirements_file() returned empty. "
                 "Check that resources/ folder is deployed beside the exe.");
    if (req_hash2.isEmpty())
        LOG_WARN("PythonSetup",
                 "requirements-numpy2.txt NOT FOUND — find_requirements_file() returned empty. "
                 "Check that resources/ folder is deployed beside the exe.");

    // A venv is "ready" only when its marker content equals the current hash.
    // Empty stored hash  → old "ok\n" marker or absent file → not current.
    // Empty req hash     → requirements file missing → not current (will fail at install).
    const QString stored1 = read_marker_hash("venv-numpy1");
    const QString stored2 = read_marker_hash("venv-numpy2");
    const bool pkg1_current = !req_hash1.isEmpty() && (stored1 == req_hash1);
    const bool pkg2_current = !req_hash2.isEmpty() && (stored2 == req_hash2);

    LOG_INFO("PythonSetup",
             QString("Markers — venv1 stored: %1  venv2 stored: %2  venv1_current: %3  venv2_current: %4")
                 .arg(stored1.isEmpty() ? "(absent)" : stored1.left(16) + "...")
                 .arg(stored2.isEmpty() ? "(absent)" : stored2.left(16) + "...")
                 .arg(pkg1_current ? "YES" : "NO")
                 .arg(pkg2_current ? "YES" : "NO"));

    status.venv_numpy1_ready = status.venv_numpy1_created && pkg1_current;
    status.venv_numpy2_ready = status.venv_numpy2_created && pkg2_current;

    const bool sentinel_exists = QFileInfo::exists(sentinel_path_for(install_dir()));
    LOG_INFO("PythonSetup",
             QString("Sentinel: %1 (%2)")
                 .arg(sentinel_exists ? "EXISTS" : "ABSENT", sentinel_path_for(install_dir())));

    // ── Fast path: sentinel + UV + both markers current ───────────────────────
    if (sentinel_exists && status.uv_installed && status.venv_numpy1_ready && status.venv_numpy2_ready) {
        status.python_installed = true;
        status.needs_setup      = false;
        LOG_INFO("PythonSetup", "Fast-path OK — sentinel + both hash-markers current → needs_setup=false");
        return status;
    }

    // ── Package-sync sub-case: infra present but ≥1 venv hash is stale ────────
    // This sub-case is ONLY for a hash mismatch (requirements file updated after
    // a successful install).  A missing marker (stored1/stored2 empty) means the
    // packages were never installed at all — that is needs_setup=true, not sync.
    //
    // Guard: skip this sub-case if either venv has an empty marker AND its venv
    // directory exists — that indicates an incomplete/empty venv that needs a
    // full package install, not just a background sync.
    const bool venv1_marker_absent = stored1.isEmpty();
    const bool venv2_marker_absent = stored2.isEmpty();
    const bool venv1_needs_full_install = status.venv_numpy1_created && venv1_marker_absent;
    const bool venv2_needs_full_install = status.venv_numpy2_created && venv2_marker_absent;

    if (sentinel_exists && status.uv_installed && status.venv_numpy1_created && status.venv_numpy2_created
        && !venv1_needs_full_install && !venv2_needs_full_install) {
        // Verify Python is present (cheap — just check the exe path).
        QString py = base_python_path();
        LOG_INFO("PythonSetup",
                 QString("Sync-check: base python = %1  exists = %2")
                     .arg(py.isEmpty() ? "(not found)" : py)
                     .arg(QFileInfo::exists(py) ? "YES" : "NO"));
        if (!py.isEmpty() && QFileInfo::exists(py)) {
            status.python_installed = true;
            if (!pkg1_current || !pkg2_current) {
                status.needs_setup        = false;
                status.needs_package_sync = true;
                LOG_INFO("PythonSetup",
                         QString("Package-sync path: infra OK, hash stale — needs_sync=true "
                                 "(venv1_current=%1 venv2_current=%2)")
                             .arg(pkg1_current ? "YES" : "NO")
                             .arg(pkg2_current ? "YES" : "NO"));
                return status;
            }
        }
    } else if (venv1_needs_full_install || venv2_needs_full_install) {
        LOG_WARN("PythonSetup",
                 QString("Skipping sync sub-case — venv(s) have empty markers (packages never installed): "
                         "venv1_needs_full=%1  venv2_needs_full=%2")
                     .arg(venv1_needs_full_install ? "YES" : "NO")
                     .arg(venv2_needs_full_install ? "YES" : "NO"));
    }

    // ── Slow path: first run or broken install ────────────────────────────────
    LOG_INFO("PythonSetup", "Slow path: running full status check (sentinel/markers absent or stale)");

    // Check Python is working (blocking — only runs on first launch or after
    // partial install when no window is visible yet).
    if (status.uv_installed) {
        QString py = base_python_path();
        LOG_INFO("PythonSetup",
                 QString("Python check: path=%1  exists=%2")
                     .arg(py.isEmpty() ? "(not found)" : py)
                     .arg(py.isEmpty() ? "N/A" : (QFileInfo::exists(py) ? "YES" : "NO")));
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
                status.python_version   = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
                LOG_INFO("PythonSetup", "Python OK: " + status.python_version);
            } else {
                LOG_WARN("PythonSetup",
                         QString("Python --version failed: exit=%1 stderr=%2")
                             .arg(proc.exitCode())
                             .arg(QString::fromUtf8(proc.readAllStandardError()).left(300)));
            }
        } else {
            LOG_WARN("PythonSetup", "Python not installed yet (UV present but python path missing)");
        }
    } else {
        LOG_WARN("PythonSetup", "UV not installed — skipping Python check");
    }

    // ── Slow-path package verification ────────────────────────────────────────
    // The marker is absent OR stale (hash mismatch).  Run `uv pip list` to
    // check whether every required package is actually present.  This covers:
    //   - First launch after updating the app (old "ok\n" marker)
    //   - Manual marker deletion
    //   - Partial install that failed to write the marker
    // If verification passes we write the current hash marker so future launches
    // use the fast path.  If it fails we leave needs_setup=true → full install.
    if (status.venv_numpy1_created && !pkg1_current) {
        LOG_INFO("PythonSetup", "venv-numpy1: marker stale/absent — running uv pip list verification");
        if (!req_hash1.isEmpty() && verify_packages_installed("venv-numpy1", "requirements-numpy1.txt")) {
            write_marker_hash("venv-numpy1", req_hash1);
            status.venv_numpy1_ready = true;
            LOG_INFO("PythonSetup", "venv-numpy1: verification passed — marker written");
        } else {
            LOG_WARN("PythonSetup",
                     QString("venv-numpy1: packages absent/incomplete/changed — needs install "
                             "(req_hash_empty=%1)")
                         .arg(req_hash1.isEmpty() ? "YES" : "NO"));
        }
    }

    if (status.venv_numpy2_created && !pkg2_current) {
        LOG_INFO("PythonSetup", "venv-numpy2: marker stale/absent — running uv pip list verification");
        if (!req_hash2.isEmpty() && verify_packages_installed("venv-numpy2", "requirements-numpy2.txt")) {
            write_marker_hash("venv-numpy2", req_hash2);
            status.venv_numpy2_ready = true;
            LOG_INFO("PythonSetup", "venv-numpy2: verification passed — marker written");
        } else {
            LOG_WARN("PythonSetup",
                     QString("venv-numpy2: packages absent/incomplete/changed — needs install "
                             "(req_hash_empty=%1)")
                         .arg(req_hash2.isEmpty() ? "YES" : "NO"));
        }
    }

    status.needs_setup =
        !status.uv_installed || !status.python_installed ||
        !status.venv_numpy1_ready || !status.venv_numpy2_ready;

    LOG_INFO("PythonSetup",
             QString("=== check_status RESULT === uv=%1 py=%2 "
                     "venv1_created=%3 venv1_ready=%4 "
                     "venv2_created=%5 venv2_ready=%6 "
                     "needs_setup=%7 needs_sync=%8")
                 .arg(status.uv_installed ? "YES" : "NO")
                 .arg(status.python_installed ? "YES" : "NO")
                 .arg(status.venv_numpy1_created ? "YES" : "NO")
                 .arg(status.venv_numpy1_ready ? "YES" : "NO")
                 .arg(status.venv_numpy2_created ? "YES" : "NO")
                 .arg(status.venv_numpy2_ready ? "YES" : "NO")
                 .arg(status.needs_setup ? "YES" : "NO")
                 .arg(status.needs_package_sync ? "YES" : "NO"));
    return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// Run full setup (background thread)
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::run_setup() {
    LOG_INFO("PythonSetup", "=== run_setup START ===");
    QPointer<PythonSetupManager> self = this;

    (void)QtConcurrent::run([self]() {
        if (!self)
            return;

        auto fail = [&](const QString& msg) {
            LOG_ERROR("PythonSetup", "run_setup FAILED: " + msg);
            QMetaObject::invokeMethod(
                self,
                [self, msg]() {
                    if (self)
                        emit self->setup_complete(false, msg);
                },
                Qt::QueuedConnection);
        };

        LOG_INFO("PythonSetup", "run_setup: calling check_status...");
        auto status = self->check_status();
        LOG_INFO("PythonSetup",
                 QString("run_setup: status — uv=%1 py=%2 venv1_ready=%3 venv2_ready=%4")
                     .arg(status.uv_installed ? "YES" : "NO")
                     .arg(status.python_installed ? "YES" : "NO")
                     .arg(status.venv_numpy1_ready ? "YES" : "NO")
                     .arg(status.venv_numpy2_ready ? "YES" : "NO"));

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
        // Use venv_NNN_created (python exe exists) to decide if venv creation
        // is needed — distinct from venv_NNN_ready (packages installed).
        bool need_venv1 = !status.venv_numpy1_created;
        bool need_venv2 = !status.venv_numpy2_created;

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
        // Use venv_NNN_ready (packages marker exists) — not venv_NNN_created —
        // so we always install packages when only the venv dir exists but the
        // package-install marker is absent (e.g. network failed last time).
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

        // ── Write sentinel so future launches skip the slow import checks ────
        // (Package markers already written with the requirements hash inside
        //  install_packages() — no separate hash-file step needed.)
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

    // Extract — prefer tar.exe (ships with Windows 10 1803+, much faster than
    // PowerShell Expand-Archive). Fall back to PowerShell on older systems.
#ifdef _WIN32
    {
        QString unused;
        bool extracted = false;
        if (run_command_capture("tar.exe", {"--version"}, {}, unused)) {
            // tar on Windows accepts -xf for zip since Windows 10 build 17063
            extracted = run_command("tar.exe", {"-xf", archive_path, "-C", dir});
        }
        if (!extracted) {
            // Fallback: PowerShell Expand-Archive
            extracted = run_command("powershell",
                                    {"-NoProfile", "-NonInteractive", "-Command",
                                     QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                                         .arg(archive_path, dir)});
        }
        if (!extracted) {
            LOG_ERROR("PythonSetup", "Failed to extract UV archive");
            return false;
        }
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

    // Shared UV env (cache dir, hardlinks, bytecode compile, concurrency, timeout).
    QStringList env = uv_env_extra();

    if (!run_command(uv_path(), {"python", "install", kPythonVersion}, env)) {
        LOG_ERROR("PythonSetup", "uv python install failed");
        return false;
    }

    emit_progress("python", 80, "Verifying Python...");

    // Clear the cached path — Python was just installed so the previous
    // (empty or stale) cached value must not be returned by base_python_path().
    cached_python_path_.clear();

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

    QStringList env = uv_env_extra();

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
    // Derive step key first — needed for all emit_progress calls including early returns
    const QString step_key = venv_name.contains("numpy1") ? "packages-numpy1" : "packages-numpy2";

    LOG_INFO("PythonSetup",
             QString("install_packages: venv=%1  req_file=%2  venv_python=%3")
                 .arg(venv_name, requirements_file, python_path(venv_name)));

    QString req_path = find_requirements_file(requirements_file);
    if (req_path.isEmpty()) {
        QString exe_dir = QCoreApplication::applicationDirPath();
        LOG_ERROR("PythonSetup",
                  QString("Requirements file not found: %1\n"
                          "  exe_dir: %2\n"
                          "  Expected: %2/resources/%1\n"
                          "  The resources/ folder must be deployed beside the exe.\n"
                          "  CMakeLists.txt POST_BUILD should copy resources/*.txt — rebuild to fix.")
                      .arg(requirements_file, exe_dir));
        emit_progress(step_key, 0,
                      QString("Setup file missing: %1 — reinstall the application").arg(requirements_file), true);
        return false;
    }

    LOG_INFO("PythonSetup", QString("install_packages: resolved req_path=%1").arg(req_path));
    LOG_INFO("PythonSetup", QString("Installing packages into %1 from %2").arg(venv_name, req_path));

    const QString venv_python = python_path(venv_name);

    // Shared UV env + peewee build flags (peewee is a transitive dep that fails
    // to build its C extension under the embedded Python on some platforms).
    QStringList env = uv_env_extra();
    env << "PEEWEE_NO_SQLITE_EXTENSIONS=1"
        << "PEEWEE_NO_C_EXTENSION=1";

    // ── Pass 1: try installing everything at once (fast path) ────────────────
    // This succeeds on most machines and is 10-100x faster than one-by-one.
    LOG_INFO("PythonSetup", QString("[%1] Pass 1: bulk install from %2").arg(venv_name, requirements_file));
    emit_progress(step_key, 5, "Installing packages (bulk)...");

    QString bulk_stderr;
    bool bulk_ok =
        run_command_capture(uv_path(), {"pip", "install", "--python", venv_python, "-r", req_path}, env, bulk_stderr);

    if (bulk_ok) {
        LOG_INFO("PythonSetup", QString("[%1] Bulk install succeeded").arg(venv_name));
        // Write the requirements hash as the marker — future check_status() calls
        // compare this hash against the current requirements file to detect changes.
        write_marker_hash(venv_name, compute_requirements_hash(requirements_file));
        emit_progress(step_key, 100, "All packages installed");
        return true;
    }

    // ── Pass 2: bulk failed — install packages one-by-one, skip failures ────
    LOG_WARN("PythonSetup",
             QString("[%1] Bulk install failed — falling back to per-package install.\n"
                     "  UV command: %2 pip install --python %3 -r %4\n"
                     "  stderr (first 600 chars): %5")
                 .arg(venv_name, uv_path(), venv_python, req_path, bulk_stderr.left(600)));
    emit_progress(step_key, 10, "Bulk install failed — retrying package by package...");

    QStringList packages = read_packages_from_file(req_path);
    if (packages.isEmpty()) {
        LOG_ERROR("PythonSetup", "No packages parsed from: " + req_path);
        return false;
    }

    QStringList failed = install_packages_individually(venv_name, packages, env, step_key);

    if (failed.isEmpty()) {
        LOG_INFO("PythonSetup", QString("[%1] All packages installed individually").arg(venv_name));
        // Write the requirements hash as the marker — all packages installed.
        write_marker_hash(venv_name, compute_requirements_hash(requirements_file));
        emit_progress(step_key, 100, "All packages installed");
    } else {
        LOG_WARN("PythonSetup", QString("[%1] %2 package(s) failed: %3")
                                    .arg(venv_name)
                                    .arg(failed.size())
                                    .arg(failed.join(", ").left(300)));
        // Do NOT write the marker when any packages failed — the stale/absent
        // marker ensures check_status() returns needs_setup=true on the next
        // launch so the failed packages are retried automatically.
        emit_progress(step_key, 100,
                      QString("Done — %1 package(s) failed (will retry on next launch)").arg(failed.size()));
    }

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

QStringList PythonSetupManager::install_packages_individually(const QString& venv_name, const QStringList& packages,
                                                              const QStringList& env_vars, const QString& step_key) {
    QString venv_python = python_path(venv_name);
    QStringList failed;
    int total = packages.size();

    for (int i = 0; i < total; ++i) {
        const QString& pkg = packages[i];
        int pct = 10 + static_cast<int>(90.0 * i / total);
        emit_progress(step_key, pct, QString("Installing %1/%2: %3").arg(i + 1).arg(total).arg(pkg));

        QString pkg_stderr;
        bool ok =
            run_command_capture(uv_path(), {"pip", "install", "--python", venv_python, pkg}, env_vars, pkg_stderr);

        if (ok) {
            LOG_INFO("PythonSetup", QString("[%1] Installed: %2").arg(venv_name, pkg));
        } else {
            LOG_WARN("PythonSetup",
                     QString("[%1] Skipped (failed): %2 — %3").arg(venv_name, pkg, pkg_stderr.left(200)));
            failed << pkg;
        }
    }

    return failed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Find requirements file
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::find_requirements_file(const QString& filename) const {
    // Return cached path — requirements files never move at runtime.
    auto it = cached_req_paths_.find(filename);
    if (it != cached_req_paths_.end()) {
        LOG_DEBUG("PythonSetup", "find_requirements_file cache hit: " + filename + " → " + it.value());
        return it.value();
    }

    QString exe_dir = QCoreApplication::applicationDirPath();
    LOG_INFO("PythonSetup", "find_requirements_file: searching for " + filename + "  exe_dir=" + exe_dir);

    // Search relative to executable
    QStringList candidates = {
        exe_dir + "/resources/" + filename,
        exe_dir + "/../resources/" + filename,
        exe_dir + "/../../resources/" + filename,
    };

    // Walk up from exe to find project root (dev/build layout)
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        QString c = dir.filePath("resources/" + filename);
        if (QFileInfo::exists(c)) {
            QString resolved = QDir::cleanPath(c);
            LOG_INFO("PythonSetup", "find_requirements_file found (walk-up): " + resolved);
            cached_req_paths_.insert(filename, resolved);
            return resolved;
        }
        if (dir.isRoot())
            break;
    }

    for (const auto& c : candidates) {
        LOG_DEBUG("PythonSetup", "find_requirements_file candidate: " + c +
                  (QFileInfo::exists(c) ? " [EXISTS]" : " [missing]"));
        if (QFileInfo::exists(c)) {
            QString resolved = QDir::cleanPath(c);
            LOG_INFO("PythonSetup", "find_requirements_file found (candidate): " + resolved);
            cached_req_paths_.insert(filename, resolved);
            return resolved;
        }
    }

    LOG_ERROR("PythonSetup",
              "find_requirements_file: " + filename + " NOT FOUND anywhere.\n"
              "  exe_dir=" + exe_dir + "\n"
              "  Tried: " + candidates.join(", ") + "\n"
              "  Fix: ensure the POST_BUILD CMake step copies resources/ beside the exe.");
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: configure and run a QProcess synchronously (background threads only)
//
// Both run_command() and run_command_capture() delegate here.
// Returns true on exit-code 0; populates stderr_out (may be nullptr to discard).
// ─────────────────────────────────────────────────────────────────────────────

static bool run_process(const QString& program, const QStringList& args,
                        const QStringList& env_vars, QString* stderr_out) {
    QProcess proc;

    if (!env_vars.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const auto& var : env_vars) {
            const int eq = var.indexOf('=');
            if (eq > 0)
                env.insert(var.left(eq), var.mid(eq + 1));
        }
        proc.setProcessEnvironment(env);
    }

#ifdef _WIN32
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW — prevent console flash
    });
#endif

    proc.start(program, args);
    constexpr int kTimeoutMs = 30 * 60 * 1000; // 30 minutes
    if (!proc.waitForFinished(kTimeoutMs)) {
        proc.kill();
        if (stderr_out)
            *stderr_out = QStringLiteral("timed out");
        return false;
    }

    if (stderr_out)
        *stderr_out = QString::fromUtf8(proc.readAllStandardError());

    return proc.exitCode() == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: run command (synchronous, used on background threads)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::run_command(const QString& program, const QStringList& args,
                                     const QStringList& env_vars) const {
    LOG_INFO("PythonSetup", QString("Running: %1 %2").arg(program, args.join(' ')));

    QString stderr_out;
    const bool ok = run_process(program, args, env_vars, &stderr_out);
    if (!ok) {
        LOG_ERROR("PythonSetup",
                  QString("Command failed: %1\nStderr: %2").arg(program, stderr_out.left(500)));
    }
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: run command, capture stderr (used for per-package failure diagnosis)
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::run_command_capture(const QString& program, const QStringList& args,
                                             const QStringList& env_vars, QString& stderr_out) const {
    return run_process(program, args, env_vars, &stderr_out);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: download file
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::download_file(const QString& url, const QString& dest_path) const {
#ifdef _WIN32
    // Windows 10 1803+ ships curl.exe in System32. Try it first — clear errors,
    // --retry support, and starts much faster than spawning PowerShell.
    bool downloaded = false;

    {
        // Probe for curl.exe availability (3-second timeout)
        QString unused;
        if (run_process("curl.exe", {"--version"}, {}, &unused)) {
            QString curl_err;
            downloaded = run_process("curl.exe",
                                     {"-L", "--fail", "--retry", "3",
                                      "--connect-timeout", "30", "-o", dest_path, url},
                                     {}, &curl_err);
            if (!downloaded)
                LOG_WARN("PythonSetup", "curl.exe failed, falling back to PowerShell: " + curl_err.left(300));
        }
    }

    if (!downloaded) {
        // Fallback: PowerShell Invoke-WebRequest
        QString ps_err;
        const QString ps_cmd =
            QString("[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                    "Invoke-WebRequest -Uri '%1' -OutFile '%2' -UseBasicParsing")
                .arg(url, dest_path);
        if (!run_process("powershell", {"-NoProfile", "-NonInteractive", "-Command", ps_cmd}, {}, &ps_err)) {
            if (ps_err.isEmpty())
                ps_err = "unknown error (no stderr)";
            return "Download failed. Check your internet connection.\nDetail: " + ps_err.left(300);
        }
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
    proc.start("curl", {"-L", "--fail", "--retry", "3", "--connect-timeout", "30", "-o", dest_path, url});
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
    // Return cached hash — requirements files never change at runtime.
    auto it = cached_req_hashes_.find(filename);
    if (it != cached_req_hashes_.end())
        return it.value();

    QString path = find_requirements_file(filename);
    if (path.isEmpty())
        return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray content = file.readAll();
    QString hash = QString(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    cached_req_hashes_.insert(filename, hash);
    return hash;
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
