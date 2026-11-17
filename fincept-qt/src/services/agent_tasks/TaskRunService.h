#pragma once
// TaskRunService.h — Chat Mode's run engine: every long-running agentic task
// (a chat reply, a planned cloud task, a local agentic task) is a run owned here.
//
// Ownership & threading
//   * GUI-thread QObject singleton, created from main.cpp's deferred init
//     (initialize_deferred(), idempotent — every entry point calls it too).
//   * Runs belong to the service, not to a screen or window: they keep going when
//     the user switches conversations, leaves Chat Mode or locks the window. Only
//     an app quit interrupts them (they come back as Interrupted).
//   * At most kMaxConcurrentRuns runs hold a slot (Planning / Running); up to
//     kMaxQueuedRuns more wait in a FIFO queue.
//   * One IRunExecutor per engine (executor_for()). The service records what they
//     signal: assigns event seq / timestamps, applies budgets, moves the run's
//     state and notifies the user when the view is hidden or the app inactive.
//   * Every SQLite write goes to a private single-thread pool (ordered: a run row
//     before its steps and events, never on the GUI thread). Events are batched
//     (every 500 ms or 50 events). Text deltas are not persisted; the step's
//     final text is.
//
// The interface block of the Chat Mode plan (section D) is reproduced verbatim;
// members marked "WP1" were added for the detach-safe chat slice.

#include "services/agent_tasks/IRunExecutor.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QThreadPool;
class QTimer;

namespace fincept::agent_tasks {

class TaskRunService : public QObject {
    Q_OBJECT
  public:
    static TaskRunService& instance();
    ~TaskRunService() override;

    static constexpr int kRetainRuns = 500;     ///< older runs are pruned at startup
    static constexpr int kMaxEventsPerRun = 5000; ///< beyond this only essential events are kept

    void initialize_deferred();
    void register_executor(IRunExecutor*);

    QString start_run(const RunRequest&, QString* error = nullptr);
    bool pause_run(const QString& id);
    bool cancel_run(const QString& id);
    bool answer(const QString& id, const QString& text);
    QString resume_run(const QString& id, QString* error = nullptr);
    QString retry_from_step(const QString& id, int step, QString* error = nullptr);
    QStringList run_ids() const;
    const TaskRun* find_run(const QString& id) const;
    QVector<RunStep> steps(const QString& id) const;
    QVector<RunArtefact> artefacts(const QString& id) const;
    void fetch_events(const QString& id, QObject* ctx, std::function<void(const QVector<RunEvent>&)> cb);
    QString add_artefact(const QString& run_id, const RunArtefact& a);
    int delete_runs(const QStringList& ids);
    void set_view_visible(bool visible);

    // ── WP1 additions ────────────────────────────────────────────────────────
    /// Persisted runs are loaded (initialize_deferred() finished its first read).
    bool is_loaded() const { return loaded_; }
    /// The text streamed so far in the run's current turn (not persisted).
    QString live_text(const QString& id) const;
    /// Events of a run kept in memory (active runs and recently viewed ones),
    /// empty when not cached — fetch_events() loads the rest.
    QVector<RunEvent> cached_events(const QString& id) const;
    /// The run bound to `session_uuid` that is not finished (Queued / Planning /
    /// Running / Waiting* / Paused), newest first; empty when none.
    QString open_run_for_session(const QString& session_uuid) const;
    /// The newest run bound to `session_uuid`, finished or not; empty when none.
    QString latest_run_for_session(const QString& session_uuid) const;
    /// Ids of runs in Queued / Planning / Running, optionally of one engine.
    QStringList active_run_ids(const QString& engine = {}) const;
    int active_count() const;
    int attention_count() const;
    /// 1-based position of a Queued run in the queue; 0 when not queued.
    int queue_position(const QString& id) const;
    /// Whether an executor with engine_id() `executor_id` is registered.
    bool has_executor(const QString& executor_id) const { return executors_.contains(executor_id); }
    /// IRunExecutor::can_pause() of the run's executor (false: unknown run, or no executor).
    bool can_pause(const QString& id) const;
    /// Append an event to a run from outside its executor (the terminal tool
    /// bridge reports the tool calls it ran for the cloud agent).
    void record_event(const QString& run_id, const RunEvent& ev);

    // ── WP5 additions ────────────────────────────────────────────────────────
    /// ArtefactService stored `a` (file and row): record its Artefact event and
    /// emit artefact_added. ArtefactService owns the artefact index; artefacts()
    /// and add_artefact() go through it.
    void note_artefact(const RunArtefact& a);

  signals:
    void run_added(const QString&);
    void run_updated(const QString&);
    void run_finished(const QString&);
    void run_event(const QString&, const fincept::agent_tasks::RunEvent&);
    void run_text_delta(const QString&, int step, const QString& chunk);
    void steps_changed(const QString&);
    void artefact_added(const QString& run_id, const QString& artefact_id);
    void attention_changed(int active, int needs_attention);
    /// WP1: persisted runs were loaded (after initialize_deferred()).
    void runs_reset();

  private:
    TaskRunService();
    Q_DISABLE_COPY(TaskRunService)

    // Executor signals
    void on_executor_event(const QString& run_id, const RunEvent& ev);
    void on_executor_text(const QString& run_id, int step, const QString& chunk);
    void on_executor_plan(const QString& run_id, const QVector<RunStep>& steps);
    void on_executor_step(const QString& run_id, int idx, StepStatus status, const QString& summary);
    void on_executor_usage(const QString& run_id, double credits, int tokens);
    void on_executor_waiting(const QString& run_id, RunStatus why, const QJsonObject& payload);
    void on_executor_finished(const QString& run_id, RunStatus terminal, const QString& final_text,
                              const QString& error);

    // Bookkeeping
    IRunExecutor* executor_of(const TaskRun& run) const;
    QString enqueue(TaskRun run, const QVector<RunStep>& steps);
    bool set_status(const QString& run_id, RunStatus status, const QString& activity = {});
    void schedule_pump();
    void pump_queue();
    void launch(const QString& run_id);
    void finish(const QString& run_id, RunStatus status, const QString& final_text, const QString& error);
    void enforce_budget(const QString& run_id);
    void on_budget_tick();
    void notify(const TaskRun& run);
    void report_attention();
    void on_about_to_quit();
    void on_loaded(const QVector<TaskRun>& runs, const QVector<RunStep>& steps, const QVector<RunArtefact>& artefacts,
                   const QHash<QString, int>& max_seq);
    void touch_event_cache(const QString& run_id);
    void cache_loaded_events(const QString& run_id, const QVector<RunEvent>& events);
    int steps_started(const QString& run_id) const;

    // Persistence (writer thread)
    void submit_write(std::function<void()> job);
    void persist_run(const QString& run_id);
    void persist_step(const RunStep& step);
    void persist_steps(const QString& run_id);
    void flush_events();

    QHash<QString, TaskRun> runs_;
    QStringList order_; // newest first
    QStringList queue_; // FIFO of queued run ids
    QHash<QString, QVector<RunStep>> steps_;
    QHash<QString, QVector<RunEvent>> events_; // active runs + LRU of viewed ones
    QStringList events_lru_;
    QHash<QString, int> next_seq_;
    QHash<QString, QString> live_text_;
    QHash<QString, QString> budget_stop_; // run id -> why it was stopped
    QHash<QString, int> dropped_events_;
    QVector<QPair<QString, RunEvent>> pending_events_;
    QHash<QString, IRunExecutor*> executors_; // by engine_id()

    QThreadPool* writer_ = nullptr;
    QTimer* flush_timer_ = nullptr;
    QTimer* budget_timer_ = nullptr;
    bool initialized_ = false;
    bool loaded_ = false;
    bool shutting_down_ = false;
    bool pump_scheduled_ = false;
    int view_visible_ = 0;
    int last_active_reported_ = -1;
    int last_attention_reported_ = -1;
    qint64 session_start_ms_ = 0;
};

} // namespace fincept::agent_tasks
