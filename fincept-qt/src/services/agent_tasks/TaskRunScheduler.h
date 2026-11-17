#pragma once
// TaskRunScheduler.h — recurring runs (WP6): once at a time, daily at HH:mm on
// chosen weekdays, weekly, or every N hours (at least hourly) — in an explicit
// time zone (schedule_logic has the rules and the DST behaviour).
//
// Rules
//   * In-process only: a schedule fires while the terminal runs (the UI says so).
//   * An occurrence missed by less than the schedule's catch-up window (default
//     12 h, kCatchUpWindowMs) runs once when the terminal is back; an older one
//     is recorded as missed. Several missed occurrences run at most once.
//   * Never overlaps itself: an occurrence is skipped while the schedule's
//     previous run is still going (queued, working, waiting or paused).
//   * Scheduled cloud runs share a daily credit cap (daily_credit_cap()): an
//     occurrence is skipped when the day's scheduled runs spent or reserved it,
//     and a run's budget is lowered to what is left.
//   * Scheduled runs are unattended (origin "schedule"): money actions are
//     refused (paper only). One that needs input parks as "needs you" and
//     notifies (TaskRunService). One that fails or stops at its budget sets
//     paused_for_review until the user looks.
//   * The next occurrence is stored BEFORE the run starts: a crash can lose an
//     occurrence, never run one twice. A cloud schedule waits while nobody is
//     signed in (and is recorded as missed once its window passes).
//   * Runs a schedule starts carry its id (TaskRun::schedule_id); runs_of().
//
// GUI-thread singleton; persists through TaskRunStore (agent_task_schedules)
// off the GUI thread. initialize_deferred() is called by TaskRunService once its
// runs are loaded. Never fires in --smoke-test / --selftest runs.

#include "services/agent_tasks/TaskRunTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

namespace fincept::agent_tasks {

class TaskRunScheduler : public QObject {
    Q_OBJECT
  public:
    static TaskRunScheduler& instance();
    ~TaskRunScheduler() override;

    static constexpr qint64 kCatchUpWindowMs = 12LL * 60 * 60 * 1000;

    /// Load schedules, run missed occurrences, arm the timer. Idempotent.
    void initialize_deferred();
    bool is_loaded() const;
    /// False in --smoke-test / --selftest runs: schedules are shown but never fire.
    bool is_active() const;

    QVector<RunSchedule> schedules() const;
    const RunSchedule* find_schedule(const QString& id) const;
    /// Create (empty id) or update a schedule. Returns its id, or empty with `error`
    /// (plain sentences from schedule_logic::validate_schedule()).
    QString save_schedule(const RunSchedule& schedule, QString* error = nullptr);
    void delete_schedule(const QString& id);
    void set_enabled(const QString& id, bool enabled);
    /// Clear paused_for_review (the user looked at the failed run).
    void resume_after_review(const QString& id);

    using StartCallback = std::function<void(bool ok, const QString& run_id, const QString& error)>;
    /// Start the schedule's run now (overlap and the daily cap still apply).
    void run_schedule_now(const QString& id, QObject* ctx, StartCallback cb);

    /// Runs started by `schedule_id`, newest first.
    QStringList runs_of(const QString& schedule_id) const;

    /// Credits all scheduled runs of one day may spend (<= 0: no cap).
    double daily_credit_cap() const;
    void set_daily_credit_cap(double credits);
    /// Credits today's scheduled runs spent, or still may spend (their budget).
    double committed_today() const;

    /// The first occurrence of `schedule` strictly after `after_ms` (epoch ms), or
    /// 0 when it has none. Pure (schedule_logic::next_occurrence_ms).
    static qint64 next_run_ms(const RunSchedule& schedule, qint64 after_ms);

  signals:
    void schedules_changed();
    /// A schedule started `run_id` (on time, caught up, or "run now").
    void schedule_fired(const QString& schedule_id, const QString& run_id);

  private:
    TaskRunScheduler();
    Q_DISABLE_COPY(TaskRunScheduler)

    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::agent_tasks
