#pragma once
// CodingAgentService.h — runs Claude Code and OpenAI Codex CLI agents LOCALLY as
// long-running background tasks, wired to the terminal's own tools through the
// external MCP server (mcp::McpHttpServer).
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
//   * On QCoreApplication::aboutToQuit the process trees of running agents are
//     killed and the runs are marked "interrupted".
//
// Process model
//   * claude -p --output-format stream-json --verbose … (prompt on stdin)
//   * codex exec --json --skip-git-repo-check … -        (prompt on stdin)
//   The MCP bearer token reaches the child only through its environment
//   (FINCEPT_MCP_TOKEN) — never argv, never disk. Stop = close stdin, SIGTERM
//   the process group (POSIX), kill the whole tree 3 s later (Windows: a
//   kill-on-close job object).

#include "services/coding_agents/CodingAgentTypes.h"

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

    // ── CLI detection ────────────────────────────────────────────────────────
    /// Last known detection result. Call refresh_cli_detection() to (re)probe.
    CliInfo cli_info(AgentKind kind) const;
    /// Resolve executable paths now (cheap) and probe `--version` async.
    /// Emits cli_status_changed() after resolution and after each probe.
    void refresh_cli_detection();
    QString cli_path_override(AgentKind kind) const;
    void set_cli_path_override(AgentKind kind, const QString& path);
    static QString install_guide_url(AgentKind kind);

    // ── Settings ─────────────────────────────────────────────────────────────
    int max_concurrent() const { return max_concurrent_; }
    void set_max_concurrent(int n);
    bool notify_on_finish() const { return notify_on_finish_; }
    void set_notify_on_finish(bool on);
    /// AppPaths::root()/agent-workspace — parent of the auto-created run folders.
    static QString workspace_root();

    // ── Runs ─────────────────────────────────────────────────────────────────
    /// Queue a run. Returns its id, or an empty string with `error` set.
    QString start_run(const RunRequest& request, QString* error = nullptr);
    /// Resume a finished run's session with a new prompt (a new run linked by parent_run_id).
    QString follow_up(const QString& run_id, const QString& prompt, QString* error = nullptr);
    bool can_follow_up(const QString& run_id, QString* why = nullptr) const;
    /// Queued → cancelled at once. Running → stdin closed, SIGTERM (POSIX), tree
    /// killed after 3 s; a second stop kills immediately.
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

    // ── External clients (Connect tab) ───────────────────────────────────────
    /// Run `claude mcp add …` / `codex mcp add …` for the terminal's MCP server,
    /// asynchronously. Without `replace_existing`, an existing `fincept` entry is
    /// reported (already_exists=true) instead of being overwritten.
    void register_with_client(AgentKind kind, bool replace_existing);

    // ── UI ───────────────────────────────────────────────────────────────────
    /// The Agent Workbench screen reports its visibility so a finished run only
    /// raises a desktop notification when the user is not looking at it.
    void set_view_visible(bool visible);

  signals:
    void run_added(const QString& run_id);
    void run_updated(const QString& run_id);
    void run_event(const QString& run_id, const fincept::services::coding_agents::AgentEvent& event);
    void run_finished(const QString& run_id);
    void runs_reset();
    void schedules_changed();
    void cli_status_changed();
    void client_registration_finished(const QString& agent, bool ok, bool already_exists, const QString& message);

  private:
    CodingAgentService();
    Q_DISABLE_COPY(CodingAgentService)

    // Persistence (writer thread)
    void submit_write(std::function<void()> job);
    void persist_run(const QString& run_id);
    void persist_schedule(const CodingAgentSchedule& schedule);
    void flush_events();
    void on_loaded(const QVector<CodingAgentRun>& runs, const QVector<CodingAgentSchedule>& schedules);
    void cache_loaded_events(const QString& run_id, const QVector<AgentEvent>& events);
    void touch_event_cache(const QString& run_id);

    // Run bookkeeping
    QString enqueue_run(CodingAgentRun run);
    void record_event(const QString& run_id, AgentEvent event);
    void record_status(const QString& run_id, const QString& title, const QString& body = {}, bool is_error = false);
    void schedule_pump();
    void pump_queue();
    void finish_run(const QString& run_id, RunStatus status, const QString& error);
    void write_prompt_file(const CodingAgentRun& run);
    void write_result_file(const CodingAgentRun& run);
    void notify_finished(const CodingAgentRun& run);
    void on_about_to_quit();

    // Processes (CodingAgentService_Process.cpp)
    bool launch(const QString& run_id, QString* error);
    void on_proc_started(const QString& run_id);
    void on_proc_stdout(const QString& run_id);
    void on_proc_stderr(const QString& run_id);
    void on_proc_finished(const QString& run_id, int exit_code, bool crashed);
    void on_proc_failed_to_start(const QString& run_id, const QString& message);
    void sync_facts(const QString& run_id);
    void kill_tree(LiveRun* live);
    void terminate_tree(LiveRun* live);
    void cleanup_live(const QString& run_id, bool immediate);
    void resolve_cli_paths();
    void probe_version(AgentKind kind);
    void run_cli_async(AgentKind kind, const QStringList& args, std::function<void(int, const QString&)> done);
    void add_to_client(AgentKind kind);

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
    QVector<QPair<QString, AgentEvent>> pending_events_;
    QHash<QString, CodingAgentSchedule> schedules_;
    QStringList schedule_order_;
    CliInfo claude_cli_;
    CliInfo codex_cli_;

    QThreadPool* writer_ = nullptr;
    QTimer* flush_timer_ = nullptr;
    QTimer* schedule_timer_ = nullptr;
    bool initialized_ = false;
    bool loaded_ = false;
    bool shutting_down_ = false;
    bool pump_scheduled_ = false;
    bool cli_resolved_ = false;
    int view_visible_ = 0;
    int max_concurrent_ = kDefaultMaxConcurrent;
    bool notify_on_finish_ = true;
    qint64 session_start_ms_ = 0;
};

} // namespace fincept::services::coding_agents
