#pragma once
// ReportScheduler — scheduled regeneration of library reports, IN-PROCESS: schedules run
// only while Fincept Terminal is open (nothing is installed in the operating system). The
// rules (daily / weekly / on start, the missed-run rule) are core/report/ReportSchedule.h.
//
// Every minute the due schedules are queued; runs never overlap (one at a time, in order).
// A run refreshes the report's data and exports it to the schedule's folder
// (ReportRegenerator); its result is kept (last 60 runs) and shown in the schedule editor
// with Open / Show in folder; a toast says when a run finished or failed.
// AI drafting is never part of a scheduled run.
//
// Stored per computer in <profile data>/report_schedules.json (written on a worker).
// GUI-thread singleton; start() is called a little after startup by ReportBuilderService.

#include "core/report/ReportSchedule.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

class QThreadPool;
class QTimer;

namespace fincept::services {

class ReportScheduler : public QObject {
    Q_OBJECT
  public:
    static ReportScheduler& instance();

    /// Load the schedules and begin checking (idempotent).
    void start();
    bool started() const { return started_; }

    QVector<report::schedule::Schedule> schedules() const { return schedules_; }
    bool schedule_for(qint64 report_id, report::schedule::Schedule* out) const;
    /// Add or replace the schedule of s.report_id.
    void set_schedule(const report::schedule::Schedule& s);
    void remove_schedule(qint64 report_id);

    /// Results, newest first (report_id 0 = every report).
    QVector<report::schedule::RunRecord> runs(qint64 report_id = 0) const;
    /// Run a report's schedule now (queued behind a running one).
    void run_now(qint64 report_id);
    bool is_queued_or_running(qint64 report_id) const;
    qint64 running_report() const { return running_; }
    /// Next automatic run of a report (invalid when none / on start).
    QDateTime next_run(qint64 report_id) const;

  signals:
    void changed();
    void run_started(qint64 report_id);
    void run_finished(const fincept::report::schedule::RunRecord& record);

  private:
    ReportScheduler();
    Q_DISABLE_COPY(ReportScheduler)
    void check();
    void enqueue(qint64 report_id, const QString& trigger);
    void pump();
    void save();
    void load_into(const QByteArray& json);
    static QString store_path();

    bool started_ = false;
    bool loaded_ = false;
    QDateTime session_start_;
    QVector<report::schedule::Schedule> schedules_;
    QVector<report::schedule::RunRecord> runs_; ///< newest first
    QVector<QPair<qint64, QString>> queue_;     ///< (report, trigger)
    QSet<qint64> ran_on_start_;
    qint64 running_ = 0;
    QTimer* timer_ = nullptr;
    QThreadPool* io_ = nullptr;
};

} // namespace fincept::services
