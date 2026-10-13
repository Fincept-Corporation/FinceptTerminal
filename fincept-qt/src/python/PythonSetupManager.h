// src/python/PythonSetupManager.h
//
// First-run Python environment bootstrapping (uv-first) and background sync.
//
// The pipeline is driven from the GUI thread by async QProcess / QNetworkReply
// signals — nothing here ever blocks the GUI thread:
//
//   RUNTIME   uv (verified, pinned 0.7.12, SHA-256 checked) → `uv python install`
//   then two INDEPENDENT lanes, no barrier between them:
//   TRADING   venv-numpy1: venv → compile requirements-numpy1.txt to a lock → install lock
//   ANALYTICS venv-numpy2: venv → compile requirements-numpy2.txt to a lock
//             → install the CORE subset (requirements-numpy2-core.txt, versions from
//               the lock) → core_ready()  ← the terminal can launch here
//             → install the rest of the lock in the background
//
// A lane failure never discards the other lane; markers are written per lane,
// only after an install from exactly that requirements content succeeded.
#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <array>
#include <atomic>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;

namespace fincept::python {

/// Result of the cheap, filesystem-only check_status().
struct SetupStatus {
    bool uv_installed = false;        // uv binary present (not executed)
    bool python_installed = false;    // managed CPython <kPythonVersion> present
    bool venv_numpy1_created = false; // venv python + pyvenv.cfg present
    bool venv_numpy2_created = false;
    bool venv_numpy1_ready = false; // created AND marker matches requirements + python + uv
    bool venv_numpy2_ready = false;
    bool core_ready = false;         // venv-numpy2 can serve the first-run experience
    bool needs_setup = true;         // no usable venv-numpy2 → show the first-run screen
    bool needs_package_sync = false; // terminal usable now; finish / verify installs in background
};

/// The four rows the setup UI shows.
enum class SetupStage : int { Runtime = 0, Trading = 1, AnalyticsCore = 2, AnalyticsFull = 3 };
inline constexpr int kSetupStageCount = 4;

enum class SetupStageState : int { Waiting, Running, Done, Failed, Skipped };

struct SetupStageInfo {
    SetupStageState state = SetupStageState::Waiting;
    int percent = -1;    // 0..100 when a REAL fraction is known; -1 = indeterminate (busy)
    QString detail;      // last meaningful live line, e.g. "Downloading numpy (12.0 MiB)"
    QString error;       // set when state == Failed
    qint64 started_ms = 0;
    qint64 finished_ms = 0;
};

/// Point-in-time copy of the pipeline state. Safe to take from any thread.
struct SetupSnapshot {
    bool running = false;
    bool core_ready = false;
    bool finished = false;  // a run has ended (see succeeded / error)
    bool succeeded = false;
    bool cancelled = false; // the run ended through cancel()
    bool background = false; // the UI moved on (early launch / sync) — failures surface on the status chip
    int overall_percent = 0;
    qint64 started_ms = 0;
    QString error;
    std::array<SetupStageInfo, kSetupStageCount> stages{};

    const SetupStageInfo& stage(SetupStage s) const { return stages[static_cast<std::size_t>(s)]; }
};

/// Progress event. `step` keeps the legacy keys ("uv", "python", "venv",
/// "packages-numpy1", "packages-numpy2-core", "packages-numpy2", "complete");
/// the remaining fields carry the per-stage state the UI renders.
struct SetupProgress {
    QString step;
    int progress = 0; // 0-100 for the stage (0 while indeterminate)
    QString message;
    bool is_error = false;
    SetupStage stage = SetupStage::Runtime;
    SetupStageState state = SetupStageState::Waiting;
    bool busy = false; // no real fraction available — render an indeterminate bar
    QString detail;
    qint64 bytes_done = 0; // uv download only
    qint64 bytes_total = 0;
    int overall = 0;
};

class PythonSetupManager : public QObject {
    Q_OBJECT
  public:
    static PythonSetupManager& instance();

    /// Cheap filesystem-only status (no process is spawned). Thread-safe and fast
    /// enough for the GUI thread. Inconclusive states resolve to needs_setup /
    /// needs_package_sync and the pipeline verifies them for real.
    SetupStatus check_status() const;

    /// Start (or resume) the pipeline. Returns immediately; a call while a run is
    /// in progress is a no-op. GUI thread.
    void run_setup();

    /// Abort the running pipeline: kills every uv / curl / tar child (and their
    /// children) and emits setup_complete(false, ...). `wait_for_exit` blocks up
    /// to ~2 s for the children to die — only for application teardown.
    void cancel(bool wait_for_exit = false);

    bool is_running() const { return running_.load(); }
    /// True once venv-numpy2 holds the core packages (this run or on disk).
    bool is_core_ready() const { return core_ready_.load(); }
    /// Weighted overall progress of the current / last run (0-100).
    int overall_percent() const { return overall_percent_.load(); }
    SetupSnapshot snapshot() const;

    /// The UI has moved on (early launch / background sync): post one in-app +
    /// desktop notification when the current run finishes, and let the status
    /// chip own failure reporting.
    void set_background_mode();

    /// Get the install directory (com.fincept.terminal)
    QString install_dir() const;
    /// uv executable path (may not exist yet)
    QString uv_path() const;
    /// Python executable inside a venv (may not exist yet)
    QString python_path(const QString& venv_name = "venv-numpy2") const;
    /// Managed CPython executable (filesystem scan, never spawns a process). Empty if absent.
    QString base_python_path() const;

    /// The environment every uv invocation must use: app-owned Python + cache dirs,
    /// managed Python only, no user uv.toml, inherited UV_* / VIRTUAL_ENV / CONDA_PREFIX /
    /// PYTHONHOME / PYTHONPATH stripped (network settings such as UV_NATIVE_TLS and
    /// index mirrors are kept), CPU torch on Linux. Thread-safe.
    QProcessEnvironment uv_environment() const;

    /// Locate a bundled requirements file (resources/ beside the exe, macOS
    /// Contents/Resources, FHS share/fincept-terminal/resources, dev tree). Cached; thread-safe.
    QString find_requirements_file(const QString& filename) const;
    /// Requirement specifiers from a requirements file (comments, options and blank lines dropped).
    static QStringList read_requirement_lines(const QString& path);
    /// PEP 503 normalised distribution name ("Scikit_Learn" → "scikit-learn").
    static QString canonical_package_name(const QString& name);
    /// Name part of a requirement specifier ("gluonts[torch]==0.16.2" → "gluonts").
    static QString requirement_name(const QString& spec);

  signals:
    void progress_changed(const fincept::python::SetupProgress& progress);
    void setup_complete(bool success, const QString& error);
    /// venv-numpy2 now holds the core packages — the terminal can launch.
    void core_ready();
    void running_changed(bool running);

  private:
    explicit PythonSetupManager(QObject* parent = nullptr);
    ~PythonSetupManager() override;
    Q_DISABLE_COPY_MOVE(PythonSetupManager)

    struct Run;         // per-run pipeline state (PythonSetupManager.cpp)
    struct StepOptions; // child-process options
    struct StepResult;  // child-process outcome
    using LineFn = std::function<void(const QString& line, bool is_stderr)>;
    using DoneFn = std::function<void(const StepResult& result)>;

    // ── Runtime stage ────────────────────────────────────────────────────────
    void runtime_begin();
    void runtime_download_uv();
    void runtime_download_uv_with_curl(const QString& why);
    void runtime_verify_archive(const QString& part_path);
    void runtime_extract_uv(const QString& archive_path, bool use_powershell);
    void runtime_check_uv_version(bool after_install);
    void runtime_install_python();
    void runtime_done();
    void runtime_failed(const QString& error);

    // ── Package lanes (0 = trading / venv-numpy1, 1 = analytics / venv-numpy2) ─
    void lane_start(int lane);
    void lane_create_venv(int lane);
    void lane_seed_preferences(int lane);
    void lane_compile(int lane);
    void lane_install_core(int lane);
    void lane_install_full(int lane);
    void lane_succeeded(int lane);
    void lane_failed(int lane, const QString& error);
    void open_trading_gate();
    void maybe_finish_run();
    void finish_run(bool success, const QString& error);

    // ── Child processes ──────────────────────────────────────────────────────
    QProcess* start_step(const QString& program, const QStringList& args, const StepOptions& opts, LineFn on_line,
                         DoneFn on_done);
    QString describe_failure(const QString& what, const StepResult& r) const;
    bool retry_with_native_tls(const StepResult& r);
    /// True while the run identified by `generation` is current and not cancelled/finished.
    bool alive(quint64 generation) const;
    /// Runs `fn` once the system-proxy probe has finished (or timed out).
    void when_network_ready(std::function<void()> fn);
    void detect_system_proxy();
    void network_ready();

    // ── Progress ─────────────────────────────────────────────────────────────
    void update_stage(SetupStage s, const std::function<void(SetupStageInfo&)>& fn, bool flush_now = false);
    void set_stage_fraction(SetupStage s, double fraction);
    void flush_progress();

    // ── Status helpers ───────────────────────────────────────────────────────
    QString requirements_hash(const QString& filename) const;
    QString expected_marker(const QString& requirements_filename) const;
    QString lock_path(const QString& venv_name) const;
    bool venv_valid(const QString& venv_name) const;
    QString python_request() const;

    // Pinned to an exact patch so `uv python install <ver>` resolves to the
    // same build on every machine. Bumping either one invalidates every venv
    // marker, so the next launch re-syncs.
    static constexpr const char* kPythonVersion = "3.11.9";
    static constexpr const char* kUvVersion = "0.7.12";

    mutable QMutex mutex_; // guards the caches, snapshot_, stage_fraction_ and system_proxy_
    mutable QHash<QString, QString> req_path_cache_;
    mutable QHash<QString, QString> req_hash_cache_;
    SetupSnapshot snapshot_;
    std::array<double, kSetupStageCount> stage_fraction_{}; // real milestones only — drives overall %
    QString system_proxy_;

    std::atomic<bool> running_{false};
    std::atomic<bool> core_ready_{false};
    std::atomic<int> overall_percent_{0};
    std::atomic<bool> native_tls_{false};
    std::atomic<bool> background_mode_{false};

    std::unique_ptr<Run> run_;
    quint64 generation_ = 0;
    QTimer* progress_timer_ = nullptr;
    std::array<bool, kSetupStageCount> dirty_stages_{};
    QNetworkAccessManager* nam_ = nullptr;
    QPointer<QNetworkReply> download_reply_;
    QList<QPointer<QProcess>> live_procs_;
};

} // namespace fincept::python
