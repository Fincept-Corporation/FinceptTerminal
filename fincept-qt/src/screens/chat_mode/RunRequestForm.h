#pragma once
// RunRequestForm.h — the part of NEW RUN and of a schedule's editor that says
// what a run does (WP6): the template's description and expected artefacts, its
// typed parameters (symbol / symbols through the shared SymbolSearchBox,
// portfolio, watchlist, period, text, choice), the task text the agent gets (a
// live preview, editable), the engine (only those this build runs; the agent
// harness — Claude Code / Codex on the user's API key — when one may run Chat
// Mode tasks, else listed disabled with the reason), the cloud mode, and the
// budget (max credits / steps / minutes) with its cost estimate.
//
// Portfolios and watchlists load off the GUI thread. Nothing here starts a run:
// the owner reads request() / problems() / confirmation() and starts it.

#include "services/agent_tasks/RunTemplates.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode::run_view {
struct BudgetConfirmation;
}

namespace fincept::chat_mode {

class RunRequestForm : public QWidget {
    Q_OBJECT
  public:
    /// `for_schedule`: periods are relative to the day the schedule runs (the
    /// preview says so) and the task text is re-filled from the template then.
    explicit RunRequestForm(QWidget* parent = nullptr, bool for_schedule = false);
    ~RunRequestForm() override;

    /// Show `template_id` (built-in or user; empty = a custom task). Keeps the
    /// values remembered for that template.
    void set_template(const QString& template_id);
    QString template_id() const;
    /// The template shown (nullopt for a custom task).
    std::optional<agent_tasks::RunTemplate> current_template() const;

    /// Everything entered, to remember or to restore:
    /// {"template_id", "params", "prompt", "prompt_edited", "engine", "mode", "budget"}.
    QJsonObject values() const;
    void set_values(const QJsonObject& values);
    /// Context chips to fill parameters from (symbols, portfolio_id / _name);
    /// profile_id and other keys go with the run's context.
    void set_extra_context(const QJsonObject& context);
    /// A custom task's text (switches to the custom task).
    void set_custom_prompt(const QString& prompt);

    /// What is missing or wrong, as plain sentences (empty: it can start).
    QStringList problems() const;
    /// The request as entered (relative dates not expanded, holdings not added —
    /// RunTemplateLibrary::prepare_request() does that at start).
    agent_tasks::RunRequest request() const;
    /// The template to fill the task text from at start; empty when the user
    /// edited the text or wrote a custom task.
    QString refill_template_id() const;
    /// The task text as typed / edited (empty when it follows the template).
    QString edited_prompt() const;
    /// The executor the engine choice runs on (kExecutor*).
    QString executor_id() const;
    /// Cost estimate and whether it must be confirmed; `balance` < 0 when unknown.
    run_view::BudgetConfirmation confirmation(double balance) const;
    /// The account balance, when known (shown beside the estimate).
    void set_balance(double credits);

    /// The "engine" value of values() / set_values() for a run's engine and
    /// plan_first (a schedule opened in the editor).
    static int engine_value(const QString& engine, bool plan_first);

  signals:
    /// Anything the user changed (the owner re-validates and remembers).
    void changed();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
