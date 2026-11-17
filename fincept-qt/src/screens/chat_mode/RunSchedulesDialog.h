#pragma once
// RunSchedulesDialog.h — scheduled runs (WP6). NON-MODAL: the caller creates it
// with Qt::WA_DeleteOnClose and show()s it — never exec().
//
//   left : every schedule — on / off, name, recurrence, next run (in its time
//          zone), last result ("needs you", paused for review, skipped …) — with
//          NEW, RUN NOW, RESUME (after a failure) and DELETE (inline confirm);
//          the daily credit cap of scheduled runs and what today used of it
//   right: the editor — name, template + parameters + engine + budget (the same
//          form as NEW RUN), recurrence (once at / daily at HH:mm on chosen
//          weekdays / weekly / every N hours), the time zone shown explicitly,
//          the catch-up window, the next occurrences — and the schedule's runs.
// It says plainly that schedules run only while the terminal is open.

#include <QDialog>
#include <QJsonObject>
#include <QString>

#include <memory>

namespace fincept::chat_mode {

class RunSchedulesDialog : public QDialog {
    Q_OBJECT
  public:
    explicit RunSchedulesDialog(QWidget* parent = nullptr);
    ~RunSchedulesDialog() override;

    /// Open the editor on a new schedule pre-filled from NEW RUN's SCHEDULE…
    /// (NewRunDialog::schedule_requested()).
    void start_new_schedule(const QJsonObject& draft);

  signals:
    /// "Run now" started `run_id`, or the user opened one of a schedule's runs.
    void run_started(const QString& run_id);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
