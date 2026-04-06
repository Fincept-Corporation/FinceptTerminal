#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QWidget>

namespace fincept::chat_mode {

/// Right panel — tabs for Memory, Schedules, Tasks.
class ChatAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatAgentPanel(QWidget* parent = nullptr);

    void refresh_memory();
    void refresh_schedules();
    void refresh_tasks();

  private slots:
    void on_tab_changed(int index);

    // Memory
    void on_add_memory();
    void on_delete_memory();
    void on_clear_all_memory();

    // Schedules
    void on_add_schedule();
    void on_delete_schedule();
    void on_toggle_schedule();

    // Tasks
    void on_refresh_tasks();
    void on_cancel_task();

  private:
    QTabBar*        tab_bar_    = nullptr;
    QStackedWidget* stack_      = nullptr;

    // Memory tab
    QListWidget*  memory_list_  = nullptr;
    QPushButton*  mem_add_btn_  = nullptr;
    QPushButton*  mem_del_btn_  = nullptr;
    QPushButton*  mem_clear_btn_= nullptr;

    // Schedules tab
    QListWidget*  sched_list_   = nullptr;
    QPushButton*  sched_add_btn_  = nullptr;
    QPushButton*  sched_del_btn_  = nullptr;
    QPushButton*  sched_toggle_btn_ = nullptr;

    // Tasks tab
    QListWidget*  task_list_    = nullptr;
    QPushButton*  task_refresh_btn_ = nullptr;
    QPushButton*  task_cancel_btn_  = nullptr;
    QLabel*       task_status_lbl_  = nullptr;

    QVector<AgentMemory>   memories_;
    QVector<AgentSchedule> schedules_;
    QVector<AgentTask>     tasks_;

    void build_ui();
    QWidget* build_memory_tab();
    QWidget* build_schedules_tab();
    QWidget* build_tasks_tab();

    static QPushButton* make_action_btn(const QString& text, const QString& tooltip);
};

} // namespace fincept::chat_mode
