// src/python/PythonSetupManager.h
//
// First-run Python environment bootstrapping (uv-first) and background sync.
//
// The pipeline is driven from the GUI thread by async QProcess / QNetworkReply
// signals — nothing here ever blocks the GUI thread. A run has a SCOPE:
//
// ESSENTIALS (the first-run default — about a minute, no lock compile):
//   RUNTIME   uv (verified, pinned 0.7.12, SHA-256 checked) → `uv python install`
//   ANALYTICS venv-numpy2: venv → install requirements-numpy2-core.txt directly,
//             constrained by requirements-numpy2.txt's own pins → core_ready()
//             ← the terminal launches here → precompile bytecode (background)
//   The trading environment and the rest of the analytics set are DEFERRED:
//   optional, installed on request (Settings → Python Env, or on demand when a
//   feature hits a missing module — see request_full_install()).
//
// FULL (opt-in; remembered, so later launches keep it in sync):
//   RUNTIME   as above
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
#include <QSet>
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
    bool core_current = false;       // ... and holds THIS build's core requirements (or the full set)
    bool full_requested = false;     // the full library set was opted into (or an older full install exists)
    bool full_ready = false;         // both environments hold their complete requirement sets
    bool needs_setup = true;         // no usable venv-numpy2 → show the first-run screen
    bool needs_package_sync = false; // terminal usable now; finish / verify installs in background
};

/// What a run installs (see the file header).
enum class SetupScope : int { Essentials = 0, Full = 1 };

/// The four rows the setup UI shows.
enum class SetupStage : int { Runtime = 0, Trading = 1, AnalyticsCore = 2, AnalyticsFull = 3 };
inline constexpr int kSetupStageCount = 4;

/// Overall-progress weight per stage (Runtime, Trading, AnalyticsCore, AnalyticsFull)
/// for each scope. A stage outside the scope weighs nothing.
inline constexpr std::array<int, kSetupStageCount> kSetupWeightsEssentials{40, 0, 60, 0};
inline constexpr std::array<int, kSetupStageCount> kSetupWeightsFull{10, 25, 15, 50};

/// Deferred: outside this run's scope — optional, installed only on request.
enum class SetupStageState : int { Waiting, Running, Done, Failed, Skipped, Deferred };

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
    SetupScope scope = SetupScope::Essentials; // what the current / last run installs
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
    /// needs_package_sync and the pipeline verifies them for real. Also refreshes
    /// the cached is_core_ready() / is_full_installed() flags.
    SetupStatus check_status() const;

    /// Start (or resume) the pipeline for the scope the user has opted into:
    /// Full once the full library set was requested, Essentials otherwise.
    /// Returns immediately; a call while a run is in progress is a no-op. GUI thread.
    void run_setup();
    /// Same, for an explicit scope. A Full request that arrives while an
    /// Essentials run is in progress is queued and starts when that run ends.
    void run_setup(SetupScope scope);

    /// Opt into the full library set: remembered on disk (later launches keep it
    /// in sync), then installed in the background now — the terminal stays
    /// usable and the status-bar chip shows progress. Returns immediately.
    void request_full_install();
    /// The full library set was opted into (or an older full install exists). Filesystem only.
    bool is_full_requested() const;
    /// Both environments hold their complete requirement sets (cached; see check_status()).
    bool is_full_installed() const { return full_installed_.load(); }
    /// Scope of the run in progress (meaningful while is_running()).
    SetupScope running_scope() const { return static_cast<SetupScope>(run_scope_.load()); }

    /// "Install on demand": a Python feature failed because `module` is not
    /// installed and the full set is not either. Emits libraries_missing() once
    /// per module per session. Thread-safe.
    void note_missing_module(const QString& module, const QString& script);
    /// For a feature's error text: when it is a "No module named 'x'" failure and
    /// the optional full set is not installed, returns an actionable message
    /// (install from Settings / the status bar, or "still installing" while a run
    /// that brings it is going) and notes the module; otherwise returns
    /// `error_text` unchanged. Thread-safe.
    QString explain_missing_module(const QString& error_text, const QString& feature);
    /// Last module reported through note_missing_module() (empty if none).
    QString last_missing_module() const;

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
    /// Top-level module named by a "No module named 'x.y'" error ("x"); empty if none.
    static QString missing_module_name(const QString& error_text);

  signals:
    void progress_changed(const fincept::python::SetupProgress& progress);
    void setup_complete(bool success, const QString& error);
    /// venv-numpy2 now holds the core packages — the terminal can launch.
    void core_ready();
    void running_changed(bool running);
    /// A feature needs a library from the full set, which is not installed (see note_missing_module()).
    void libraries_missing(const QString& module, const QString& script);

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
    /// The lane's venv exists: Essentials → install the core directly, Full → seed + compile.
    void lane_venv_ready(int lane);
    void lane_seed_preferences(int lane);
    void lane_compile(int lane);
    void lane_install_core(int lane);
    /// Essentials scope: core requirements straight into the venv (no lock compile).
    void lane_install_essentials(int lane);
    /// Essentials scope: precompile site-packages off the launch-critical path.
    void lane_compile_bytecode(int lane);
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
    QString site_packages_path(const QString& venv_name) const;
    bool venv_valid(const QString& venv_name) const;
    QString python_request() const;

    // Pinned to an exact patch so `uv python install <ver>` resolves to the
    // same build on every machine. Bumping either one invalidates every venv
    // marker, so the next launch re-syncs.
    static constexpr const char* kPythonVersion = "3.11.9";
    static constexpr const char* kUvVersion = "0.7.12";

    // guards the caches, snapshot_, stage_fraction_, stage_weights_, system_proxy_
    // and the missing-module bookkeeping
    mutable QMutex mutex_;
    mutable QHash<QString, QString> req_path_cache_;
    mutable QHash<QString, QString> req_hash_cache_;
    SetupSnapshot snapshot_;
    std::array<double, kSetupStageCount> stage_fraction_{}; // real milestones only — drives overall %
    std::array<double, kSetupStageCount> stage_weights_{};  // the running scope's kSetupWeights*
    QString system_proxy_;
    QString last_missing_module_;
    QSet<QString> reported_modules_;

    std::atomic<bool> running_{false};
    mutable std::atomic<bool> core_ready_{false};     // also refreshed by check_status()
    mutable std::atomic<bool> full_installed_{false}; // refreshed by check_status() and a Full success
    std::atomic<bool> full_requested_session_{false}; // request_full_install() even if its flag write failed
    std::atomic<int> run_scope_{0};
    std::atomic<int> overall_percent_{0};
    std::atomic<bool> native_tls_{false};
    std::atomic<bool> background_mode_{false};

    std::unique_ptr<Run> run_;
    quint64 generation_ = 0;
    bool pending_full_ = false; // a Full request is waiting for the current Essentials run to end
    QTimer* progress_timer_ = nullptr;
    std::array<bool, kSetupStageCount> dirty_stages_{};
    QNetworkAccessManager* nam_ = nullptr;
    QPointer<QNetworkReply> download_reply_;
    QList<QPointer<QProcess>> live_procs_;
};

} // namespace fincept::python
