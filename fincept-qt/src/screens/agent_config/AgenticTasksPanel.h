// src/screens/agent_config/AgenticTasksPanel.h
//
// AGENTIC view — durable, long-running tasks with per-step progress.
// Visible while Settings > Developer > "Enable Agentic Mode" is on (the default).
//
// Subscribes to AgentService::task_event for live updates; on show, requests
// list_tasks() to populate the list. Provides Pause/Resume/Cancel/Delete
// controls and an editable plan view (read-only in Phase 1; editable in Phase 2).

#pragma once
#include <QEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTimer;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class AgenticTasksPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgenticTasksPanel(QWidget* parent = nullptr);

    /// Inspector use on the agents screen: the task list, its filter bar and the
    /// SCHEDULES… / LIBRARIES… buttons are hidden (the screen's one run list and
    /// its RUNS toolbar have them); the panel shows the task open_task() names.
    void set_detail_only(bool on);
    /// Show one task (its agent_tasks row when known), then re-read it.
    void open_task(const QString& task_id, const QJsonObject& row = {});
    QString current_task_id() const { return selected_task_id_; }

  public slots:
    /// SCHEDULE A TASK… (an empty schedule dialog).
    void schedule_new_task();
    void on_schedules_clicked();
    void on_libraries_clicked();

  signals:
    /// A task was deleted from the detail pane (the run list drops it).
    void task_deleted(const QString& task_id);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void refresh_list();
    void on_filter_changed(int idx);
    void on_task_selected(QListWidgetItem* item);
    void on_pause_clicked();
    void on_resume_clicked();
    void on_cancel_clicked();
    void on_delete_clicked();
    void on_reply_clicked();
    void on_schedule_clicked();

  private:
    void build_ui();
    void wire_service();
    void apply_task_event(const QString& task_id, const QJsonObject& event);
    void render_task_detail(const QJsonObject& task);
    /// PAUSE / RESUME / CANCEL and the header colour for a task in `status`.
    /// Called from render_task_detail and from live events, so the buttons
    /// follow the task without the row being clicked again.
    void apply_status_controls(const QString& status);
    /// The SCHEDULE… dialog, pre-filled; re-opened with the user's text when the
    /// schedule is rejected (e.g. an expression the DSL does not accept).
    void open_schedule_dialog(const QString& name, const QString& schedule_expr, const QString& query);
    void render_plan(const QJsonObject& plan);
    /// Step log: one rendered entry per event. A step result is a header line
    /// (number, name, coloured status glyph) over its markdown answer, in full.
    /// `state`: "running" / "done" / "error" / "warn" / "info".
    void log_line(const QString& text, const QString& state = QStringLiteral("info"));
    void log_step(int number, const QString& name, const QString& state, const QString& markdown);
    void clear_step_log();
    void render_step_log();
    /// The task's final answer (rendered markdown) above the step log; empty hides it.
    void show_final(const QString& markdown);
    /// Status pill next to the meta line.
    void set_status_pill(const QString& status);
    void render_budget(const QJsonObject& budget);
    void render_question(const QString& question);
    void clear_question();
    /// Rate-limited refresh_list(): task events arrive in bursts and every list
    /// refresh spawns a Python process. Defers while the tab is hidden.
    void request_refresh();
    static QString status_color_for(const QString& status);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* filter_title_ = nullptr;
    QComboBox* filter_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QStackedWidget* list_stack_ = nullptr; // task_list_ | list_empty_ (no tasks / none with this status)
    ui::EmptyState* list_empty_ = nullptr;
    QListWidget* task_list_ = nullptr;
    QLabel* detail_header_ = nullptr;
    QLabel* detail_meta_ = nullptr;
    QLabel* status_pill_ = nullptr;
    QLabel* plan_title_ = nullptr;
    QListWidget* plan_view_ = nullptr; // one row per plan step, status glyph coloured
    QLabel* final_title_ = nullptr;
    QTextBrowser* final_view_ = nullptr; // the task's final answer (markdown)
    QLabel* step_log_title_ = nullptr;
    QTextBrowser* step_log_ = nullptr;
    struct StepLogEntry {
        bool step = false; // step result (header + markdown) vs a one-line event
        QString state;
        QString title;
        QString markdown;
    };
    QVector<StepLogEntry> step_log_entries_;
    QJsonObject shown_plan_; // re-rendered on theme change (glyph colours)
    QPushButton* pause_btn_ = nullptr;
    QPushButton* resume_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* schedule_btn_ = nullptr;
    QPushButton* schedules_btn_ = nullptr;
    QPushButton* libraries_btn_ = nullptr;

    // Budget meter (per-task, updated from task_event.budget snapshot).
    QLabel* budget_label_ = nullptr;
    QLabel* budget_tokens_label_ = nullptr;
    QLabel* budget_cost_label_ = nullptr;
    QLabel* budget_wall_label_ = nullptr;
    QLabel* budget_steps_label_ = nullptr;
    QProgressBar* budget_tokens_bar_ = nullptr;
    QProgressBar* budget_cost_bar_ = nullptr;
    QProgressBar* budget_wall_bar_ = nullptr;
    QProgressBar* budget_steps_bar_ = nullptr;

    // HITL banner: only visible when task.status == paused_for_input.
    QFrame* question_banner_ = nullptr;
    QLabel* question_title_ = nullptr;
    QLabel* question_label_ = nullptr;
    QLineEdit* reply_edit_ = nullptr;
    QPushButton* reply_btn_ = nullptr;

    QString selected_task_id_;
    QJsonObject selected_task_config_; // stored agent config of the selected task (no secrets)
    QString shown_status_;             // status the detail pane's controls currently reflect
    // Schedule submitted from this panel and not yet confirmed / rejected.
    QString pending_schedule_name_;
    QString pending_schedule_expr_;
    QString pending_schedule_query_;
    bool first_show_ = true;
    bool detail_only_ = false;           // set_detail_only(): no list, no auto-selection
    QString selected_query_;             // the shown task's query (SCHEDULE… pre-fill)
    bool stale_ = false;                 // events arrived while hidden — refresh on next show
    QTimer* refresh_throttle_ = nullptr; // single-shot, armed by request_refresh()
};

} // namespace fincept::screens
