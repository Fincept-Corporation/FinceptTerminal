#pragma once
// ComposerContextBar.h — the strip above Chat Mode's composer (WP2): context
// chips (symbol, portfolio, profile, one text attachment of up to
// kMaxAttachmentChars characters inlined into the prompt), the engine / mode
// picker and the "Run as task" toggle with its budget summary.
//
// ChatMessagePanel owns it (context_bar()); ChatModeScreen reads it when a
// message is sent: compose_prompt() is the prompt that goes out, context() /
// engine() / run_as_task() fill the RunRequest. The bar is always shown (the
// engine picker lives in it); chips appear as they are added.

#include "services/agent_tasks/TaskRunTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>

class QMenu;
class QPoint;

namespace fincept::chat_mode {

class ComposerContextBar : public QWidget {
    Q_OBJECT
  public:
    explicit ComposerContextBar(QWidget* parent = nullptr);
    ~ComposerContextBar() override;

    static constexpr int kMaxAttachmentChars = 32000;

    /// The chips as RunRequest::context: {"symbols": [..], "portfolio_id": …,
    /// "attachment": {"name", "chars"}} (keys present only when set).
    QJsonObject context() const;
    void set_context(const QJsonObject& context);
    void clear_context();

    /// `user_text` with the attachment and chips inlined, as sent to the agent.
    QString compose_prompt(const QString& user_text) const;

    /// agent_tasks::kEngineCloud (default) or kEngineLocal.
    QString engine() const;
    void set_engine(const QString& engine);

    /// Send the next message as a planned multi-step task instead of one reply.
    bool run_as_task() const;
    void set_run_as_task(bool on);

    /// The budget a "Run as task" message is started with (empty when not a
    /// task) and the credits it may cost. WP6: editable inline (the BUDGET
    /// button opens credits / steps / minutes fields), remembered between
    /// sessions; defaults kDefaultMaxCredits / kDefaultMaxSteps.
    agent_tasks::RunBudget budget() const;
    double estimated_credits() const;
    void set_budget(const agent_tasks::RunBudget& budget);

    /// WP6: the next send must be confirmed first (a task estimated above
    /// kConfirmAboveCredits).
    bool needs_confirmation() const;
    /// Show the inline "this may cost up to N credits — start?" step; `balance`
    /// < 0 when unknown. confirmed() follows its START.
    void ask_confirmation(double balance);

    // ── 2026-11-17 revamp: the composer ("@" mentions, drops) and the CONTEXT
    // page edit the same chips through these (each emits context_changed()). ──
    QStringList symbols() const;
    void add_symbol(const QString& symbol);    ///< normalised; ignored when invalid or present
    void remove_symbol(const QString& symbol);
    QString portfolio_name() const;            ///< empty when none
    void clear_portfolio();
    QString profile_name() const;              ///< empty when none
    void clear_profile();
    QString attachment_name() const;           ///< empty when no text is attached
    int attachment_chars() const;
    void clear_attachment();
    /// Inline `text` as the attachment (cut to kMaxAttachmentChars, a note says so).
    void attach_text(const QString& name, const QString& text);
    /// Read a text file on a worker thread and attach it (a note explains a refusal).
    void attach_file(const QString& path);
    /// The non-modal "Attach a text file" dialog.
    void open_attach_dialog();
    /// Open the shared symbol picker of "+ CONTEXT".
    void begin_symbol_entry();
    /// Pop the "+ CONTEXT" menu (symbol, portfolio, profile, file, clipboard) at `global_pos`.
    void popup_add_menu(const QPoint& global_pos);
    /// The engine menu (cloud one reply / multi-step / local / harness), also shown by
    /// the command bar's engine chip. Owned by the bar.
    QMenu* engine_menu() const;
    /// "Cloud · one reply", "Harness · Claude Code · sonnet" …
    QString engine_label() const;
    /// kExecutor* of the choice (what the next message runs on).
    QString executor_id() const;
    /// One plain line for the budget of the next message ("one reply · about 5 credits").
    QString budget_summary() const;

  signals:
    void context_changed();
    void engine_changed(const QString& engine);
    void run_as_task_toggled(bool on);
    /// The user confirmed the costly task in the inline step: send it now.
    void confirmed();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
