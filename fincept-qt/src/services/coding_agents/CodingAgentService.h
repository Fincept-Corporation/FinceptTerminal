#pragma once
// CodingAgentService.h — runs agent-harness CLIs (Claude Code, Codex, Grok, …;
// see src/services/harness) LOCALLY as long-running background tasks, wired to
// the terminal's own tools through the external MCP server (mcp::McpHttpServer).
//
// Ownership & threading
//   * GUI-thread QObject singleton. Constructing it is cheap: no processes, no
//     disk I/O. initialize_deferred() (called once from main's deferred init, and
//     idempotently by every consumer) loads persisted runs/schedules on the
//     writer thread and arms the schedule timer only when a schedule exists.
//   * Runs belong to the service, not to a screen: they keep going when the
//     user navigates away. At most max_concurrent() (1–4, default 2) CLIs run at
//     once; extra runs wait in a FIFO queue.
//   * Every SQLite write goes to a private single-thread pool, so writes stay
//     ordered (run row before its events) and never touch the GUI thread.
//     Transcript events are batched (every 500 ms or 50 events).
//   * Every file-system touch — creating a run's working folder, the per-run
//     files an adapter asks for, PROMPT.md / RESULT.md, deleting temp files,
//     the folder checks and writes the screens ask for — runs on a second
//     single-thread pool (FIFO), so a working folder on a slow or dead network
//     share never stalls the UI or the database writes.
//   * On QCoreApplication::aboutToQuit the process trees of running agents are
//     killed and the runs are marked "interrupted".
//
// Harnesses
//   * A run names its harness by registry id. Where it is installed, which
//     version, its sign-in state, models, efforts and the user's defaults all
//     come from harness::HarnessRegistry — the single source of truth; the
//     adapter builds the command line and the stream parser; HarnessLaunch
//     starts it. Nothing here is specific to one harness.
//   * A failed run is classified by its adapter (signed out, rate limited,
//     model, …) and reported to the registry, which re-checks the sign-in or
//     sets a cooldown; the run's error starts with that plain sentence.
//   * The run's time limit (or the 4 h hard cap) is enforced here for every
//     harness by a watchdog.
//
// Process model
//   The MCP bearer token reaches a child only through its environment
//   (FINCEPT_MCP_TOKEN) — never argv, never disk. Stop = close stdin, SIGTERM
//   the process group (POSIX), kill the whole tree after the adapter's grace
//   period (Windows: a kill-on-close job object the child joins before it runs).

#include "services/coding_agents/CodingAgentTypes.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QThreadPool;
class QTimer;

namespace fincept::services::coding_agents {

struct LiveRun; // CodingAgentLiveRun.h (private)

class CodingAgentService : public QObject {
    Q_OBJECT
  public:
    static CodingAgentService& instance();
    ~CodingAgentService() override;

    static constexpr int kDefaultMaxConcurrent = 2;
    static constexpr int kMaxConcurrentLimit = 4;
    static constexpr int kRetainRuns = 500;   // older runs are pruned at startup
    static constexpr int kMaxQueuedRuns = 20; // a runaway caller cannot queue unbounded paid work

    // ── Lifecycle ────────────────────────────────────────────────────────────
    /// Load persisted runs + schedules (async, writer thread), mark runs left
    /// over from a crash as interrupted, prune to kRetainRuns, run missed
    /// schedules (< 12 h late) once, arm the schedule timer. Idempotent.
    void initialize_deferred();
    bool is_loaded() const { return loaded_; }

    // ── Settings ─────────────────────────────────────────────────────────────
    int max_concurrent() const { return max_concurrent_; }
    void set_max_concurrent(int n);
    bool notify_on_finish() const { return notify_on_finish_; }
    void set_notify_on_finish(bool on);
    /// AppPaths::root()/agent-workspace — parent of the auto-created run folders.
    static QString workspace_root();

    // ── Runs ─────────────────────────────────────────────────────────────────
    /// Queue a run. Returns its id, or an empty string with `error` set.
    /// Options the harness does not support (a budget, a turn limit, an effort,
    /// terminal tools without per-run MCP) are dropped, not refused.
    QString start_run(const RunRequest& request, QString* error = nullptr);
    /// Resume a finished run's session with a new prompt (a new run linked by parent_run_id).
    QString follow_up(const QString& run_id, const QString& prompt, QString* error = nullptr);
    bool can_follow_up(const QString& run_id, QString* why = nullptr) const;
    /// Queued → cancelled at once. Running → stdin closed, SIGTERM (POSIX), tree
    /// killed after the adapter's grace period; a second stop kills immediately.
    bool stop_run(const QString& run_id);
    /// Deletes finished runs (active ones are skipped). Working folders stay on disk.
    int delete_runs(const QStringList& ids);
    int clear_finished();

    QStringList run_ids() const { return order_; } // newest first
    const CodingAgentRun* find_run(const QString& run_id) const;
    int running_count() const;
    int queued_count() const { return static_cast<int>(queue_.size()); }
    int queue_position(const QString& run_id) const; // 1-based; 0 = not queued
    bool has_active_tool_runs() const;               // a running/queued run uses the MCP server
    /// The exact command line a run was started with in this session, with the
    /// prompt / system prompt shown as their lengths and secrets masked. Empty
    /// for runs from earlier sessions (their transcript's start event has it).
    QString command_line(const QString& run_id) const { return command_lines_.value(run_id); }

    // Transcript
    bool has_cached_events(const QString& run_id) const;
    QVector<AgentEvent> cached_events(const QString& run_id) const;
    /// Cached → callback runs synchronously. Otherwise loaded on the writer thread
    /// and delivered on the GUI thread — dropped if `context` died meanwhile.
    void fetch_events(const QString& run_id, QObject* context,
                      std::function<void(const QVector<AgentEvent>&)> callback);
    QStringList stderr_tail(const QString& run_id) const;

    // ── Schedules ────────────────────────────────────────────────────────────
    QVector<CodingAgentSchedule> schedules() const;
    const CodingAgentSchedule* find_schedule(const QString& id) const;
    QString save_schedule(const CodingAgentSchedule& schedule, QString* error = nullptr);
    void delete_schedule(const QString& id);
    QString run_schedule_now(const QString& id, QString* error = nullptr);

    // ── External clients (CONNECT) ───────────────────────────────────────────
    /// Add the terminal's MCP server to the user's own copy of a harness with
    /// the adapter's one-click command (harness::McpRegistration::add — never
    /// the token on its command line), asynchronously. Without
    /// `replace_existing`, an existing entry is reported (already_exists=true)
    /// instead of being overwritten.
    void register_with_client(const QString& harness_id, bool replace_existing);

    // ── Files, off the GUI thread ────────────────────────────────────────────
    /// `path` is an existing folder? Answered on the GUI thread — dropped if
    /// `context` died meanwhile.
    void check_folder(const QString& path, QObject* context, std::function<void(bool exists)> done);
    /// Create `path` (and parents).
    void ensure_folder(const QString& path, QObject* context, std::function<void(bool ok, const QString& error)> done);
    /// Write `bytes` to `path` atomically (QSaveFile).
    void write_file(const QString& path, const QByteArray& bytes, QObject* context,
                    std::function<void(bool ok, const QString& error)> done);

    // ── UI ───────────────────────────────────────────────────────────────────
    /// The Agent Workbench screen reports its visibility so a finished run only
    /// raises a desktop notification when the user is not looking at it.
    void set_view_visible(bool visible);

  signals:
    void run_added(const QString& run_id);
    void run_updated(const QString& run_id);
    /// A recorded transcript event — or, with event.partial, the live text of a
    /// message still streaming (coalesced to ≤ 10 per second; never stored).
    void run_event(const QString& run_id, const fincept::services::coding_agents::AgentEvent& event);
    void run_finished(const QString& run_id);
    void runs_reset();
    void schedules_changed();
    void client_registration_finished(const QString& harness_id, bool ok, bool already_exists, const QString& message);

  private:
    CodingAgentService();
    Q_DISABLE_COPY(CodingAgentService)

    // Persistence (writer thread)
    void submit_write(std::function<void()> job);
    void submit_file_job(std::function<void()> job);
    void persist_run(const QString& run_id);
    void persist_schedule(const CodingAgentSchedule& schedule);
    void flush_events();
    void flush_partials();
    void on_loaded(const QVector<CodingAgentRun>& runs, const QVector<CodingAgentSchedule>& schedules);
    void cache_loaded_events(const QString& run_id, const QVector<AgentEvent>& events);
    void touch_event_cache(const QString& run_id);

    // Run bookkeeping
    QString enqueue_run(CodingAgentRun run);
    void record_event(const QString& run_id, AgentEvent event);
    void record_status(const QString& run_id, const QString& title, const QString& body = {}, bool is_error = false);
    void schedule_pump();
    void pump_queue();
    void finish_run(const QString& run_id, RunStatus status, const QString& error, const QString& failure = {});
    void write_prompt_file(const CodingAgentRun& run);
    void write_result_file(const CodingAgentRun& run);
    void notify_finished(const CodingAgentRun& run);
    void on_about_to_quit();

    // Processes (CodingAgentService_Process.cpp)
    bool launch(const QString& run_id, QString* error, QString* failure);
    void on_launch_prepared(const QString& run_id, bool ok, const QString& error, const QStringList& temp_paths);
    void on_proc_started(const QString& run_id);
    void on_proc_stdout(const QString& run_id);
    void on_proc_stderr(const QString& run_id);
    void on_proc_finished(const QString& run_id, int exit_code, bool crashed);
    void on_proc_failed_to_start(const QString& run_id, const QString& message);
    void sync_facts(const QString& run_id);
    void kill_tree(LiveRun* live);
    void terminate_tree(LiveRun* live);
    void cleanup_live(const QString& run_id, bool immediate);
    void run_cli_async(const QString& harness_id, const QStringList& args, bool with_token,
                       std::function<void(int, const QString&)> done);
    void add_to_client(const QString& harness_id);

    // Schedules
    void rearm_schedule_timer();
    void on_schedule_tick();
    void fire_schedule(const QString& id, qint64 now_ms);

    // ── State ────────────────────────────────────────────────────────────────
    QHash<QString, CodingAgentRun> runs_;
    QStringList order_; // newest first
    QStringList queue_; // FIFO of queued run ids
    QHash<QString, LiveRun*> live_;
    QHash<QString, QVector<AgentEvent>> events_; // active runs + LRU of viewed ones
    QStringList events_lru_;
    QHash<QString, int> next_seq_;
    QHash<QString, QStringList> stderr_tails_;
    QHash<QString, QString> command_lines_; // runs started this session
    QVector<QPair<QString, AgentEvent>> pending_events_;
    QHash<QString, AgentEvent> pending_partials_; // run id -> newest live text (coalesced)
    QHash<QString, CodingAgentSchedule> schedules_;
    QStringList schedule_order_;

    QThreadPool* writer_ = nullptr; // SQLite, FIFO
    QThreadPool* files_ = nullptr;  // folders and files, FIFO
    QTimer* flush_timer_ = nullptr;
    QTimer* partial_timer_ = nullptr;
    QTimer* schedule_timer_ = nullptr;
    bool initialized_ = false;
    bool loaded_ = false;
    bool shutting_down_ = false;
    bool pump_scheduled_ = false;
    int view_visible_ = 0;
    int max_concurrent_ = kDefaultMaxConcurrent;
    bool notify_on_finish_ = true;
    qint64 session_start_ms_ = 0;
};

} // namespace fincept::services::coding_agents
