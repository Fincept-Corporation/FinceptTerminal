// src/screens/agent_config/PlannerViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// PLANNER view — generate and execute multi-step agent plans.
/// Left: templates + history. Center: plan steps editor. Right: step results.
class PlannerViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PlannerViewPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    QWidget* build_templates_panel();
    QWidget* build_plan_editor();
    QWidget* build_results_panel();
    void setup_connections();

    void generate_plan();
    void execute_plan();
    void populate_plan(const services::ExecutionPlan& plan);
    void update_step_status(int row, const QString& status);
    void add_step();
    void remove_step();
    void move_step_up();
    void move_step_down();
    void save_plan_to_history();
    void copy_result();

    // Left
    QComboBox* llm_profile_combo_ = nullptr;
    QComboBox* portfolio_combo_ = nullptr;
    QListWidget* template_list_ = nullptr;
    QPlainTextEdit* custom_query_ = nullptr;
    QPushButton* generate_btn_ = nullptr;
    QListWidget* history_list_ = nullptr;
    QLineEdit* history_search_ = nullptr;
    QLabel* history_header_ = nullptr;

    // Center
    QTableWidget* steps_table_ = nullptr;
    QPushButton* execute_btn_ = nullptr;
    QPushButton* add_step_btn_ = nullptr;
    QPushButton* remove_step_btn_ = nullptr;
    QPushButton* move_up_btn_ = nullptr;
    QPushButton* move_down_btn_ = nullptr;
    QLabel* plan_status_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
    QLabel* progress_label_ = nullptr;

    // Right
    QTextEdit* result_display_ = nullptr;
    QLabel* result_header_ = nullptr;
    QPushButton* copy_btn_ = nullptr;

    // Loading overlay
    ui::LoadingOverlay* loading_overlay_ = nullptr;

    // State
    services::ExecutionPlan current_plan_;
    QVector<services::ExecutionPlan> plan_history_;
    bool generating_ = false;
    bool executing_ = false;
    bool data_loaded_ = false;
    QString pending_request_id_;
};

} // namespace fincept::screens
