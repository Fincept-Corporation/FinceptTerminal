// src/screens/agent_config/AgenticTasksPanel.h
//
// AGENTIC view — durable, long-running tasks with per-step progress.
// Visible only when Settings > Developer > "Enable Agentic Mode" is on.
//
// Subscribes to AgentService::task_event for live updates; on show, requests
// list_tasks() to populate the list. Provides Pause/Resume/Cancel/Delete
// controls and an editable plan view (read-only in Phase 1; editable in Phase 2).

#pragma once
#include <QJsonArray>
#include <QJsonObject>
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

namespace fincept::screens {

class AgenticTasksPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgenticTasksPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

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
    void on_libraries_clicked();

  private:
    void build_ui();
    void wire_service();
    void apply_task_event(const QString& task_id, const QJsonObject& event);
    void render_task_detail(const QJsonObject& task);
    void render_plan(const QJsonObject& plan);
    void render_budget(const QJsonObject& budget);
    void render_question(const QString& question);
    void clear_question();
    static QString status_color_for(const QString& status);

    QComboBox* filter_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QListWidget* task_list_ = nullptr;
    QLabel* detail_header_ = nullptr;
    QLabel* detail_meta_ = nullptr;
    QPlainTextEdit* plan_view_ = nullptr;
    QPlainTextEdit* step_log_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* resume_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* schedule_btn_ = nullptr;
    QPushButton* libraries_btn_ = nullptr;

    // Budget meter (per-task, updated from task_event.budget snapshot).
    QLabel* budget_label_ = nullptr;
    QProgressBar* budget_tokens_bar_ = nullptr;
    QProgressBar* budget_cost_bar_ = nullptr;
    QProgressBar* budget_wall_bar_ = nullptr;
    QProgressBar* budget_steps_bar_ = nullptr;

    // HITL banner: only visible when task.status == paused_for_input.
    QFrame* question_banner_ = nullptr;
    QLabel* question_label_ = nullptr;
    QLineEdit* reply_edit_ = nullptr;
    QPushButton* reply_btn_ = nullptr;

    QString selected_task_id_;
    bool first_show_ = true;
};

} // namespace fincept::screens
