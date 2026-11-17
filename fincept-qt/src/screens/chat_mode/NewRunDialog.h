#pragma once
// NewRunDialog.h — start a long-running task (WP6). NON-MODAL: the caller
// creates it with Qt::WA_DeleteOnClose and show()s it — never exec().
//
//   left : the templates (built-in finance tasks by category, the user's own,
//          and "Custom task"), each with its description
//   right: the template's parameters (symbol through the shared SymbolSearchBox,
//          portfolio, watchlist, period, text, choices), the task text the agent
//          gets (a live preview; editable), engine (only those
//          TaskRunService::has_executor() reports and the build implements),
//          mode, budget (max credits / steps / minutes) with its estimate
//   bottom: plain validation messages, an inline "this may cost up to N credits —
//          start?" step above kConfirmAboveCredits, SCHEDULE… / START NOW.
// Save / update / delete user templates inline. The last inputs (template,
// engine, budget, each template's parameters) are remembered.

#include <QDialog>
#include <QJsonObject>
#include <QString>

#include <memory>

namespace fincept::chat_mode {

class NewRunDialog : public QDialog {
    Q_OBJECT
  public:
    explicit NewRunDialog(QWidget* parent = nullptr);
    ~NewRunDialog() override;

    /// Preselect a template (RunTemplate::id; empty = Custom task).
    void set_template(const QString& template_id);
    /// Pre-fill the task text (switches to Custom task).
    void set_prompt(const QString& prompt);
    /// Pre-fill parameters from context chips ({"symbols": [..], "portfolio_id",
    /// "portfolio_name", "profile_id", …}); the rest goes with the run's context.
    void set_context(const QJsonObject& context);
    /// Run in this conversation (empty: the run creates its own).
    void set_session(const QString& session_uuid);

  signals:
    /// The run was started (the dialog closes itself).
    void run_started(const QString& run_id);
    /// SCHEDULE…: the form as a schedule draft (RunSchedulesDialog takes it):
    /// {"template_id", "prompt", "engine", "mode", "plan_first", "budget", "context", "name"}.
    void schedule_requested(const QJsonObject& draft);

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
