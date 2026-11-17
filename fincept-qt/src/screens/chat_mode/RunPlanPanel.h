#pragma once
// RunPlanPanel.h — the PLAN tab of Chat Mode's inspector (WP2): the run's steps
// with live status, and pause / resume / cancel / retry-from-step. Actions call
// TaskRunService directly (pause_run, resume_run, cancel_run, retry_from_step)
// and never block (no exec() dialogs). Consumes TaskRunService signals only.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class RunPlanPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RunPlanPanel(QWidget* parent = nullptr);
    ~RunPlanPanel() override;

    /// Show `run_id` (empty: nothing selected).
    void set_run(const QString& run_id);
    QString run_id() const;

  signals:
    /// retry_from_step() created `new_run_id`: the inspector should follow it.
    void run_created(const QString& new_run_id);
    /// WP2 (additive): the run waits for the user (a question / an approval) and
    /// the user asked to go to its conversation, where the composer / approval card is.
    void conversation_requested(const QString& run_id);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
