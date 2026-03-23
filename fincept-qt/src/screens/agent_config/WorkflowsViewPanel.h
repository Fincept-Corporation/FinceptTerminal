// src/screens/agent_config/WorkflowsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// WORKFLOWS view — predefined financial workflows with execution and results.
class WorkflowsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit WorkflowsViewPanel(QWidget* parent = nullptr);

  private:
    void build_ui();
    QWidget* build_workflow_cards();
    QWidget* build_results_panel();
    void setup_connections();
    void run_workflow(const QString& type, const QJsonObject& params = {});

    QPushButton* make_workflow_card(const QString& title, const QString& desc, const char* color,
                                   const QString& action_text, QVBoxLayout* parent_layout);

    // Workflow inputs
    QLineEdit* symbol_input_ = nullptr;

    // Results
    QTextEdit* result_display_ = nullptr;
    QLabel* result_status_ = nullptr;
    QLabel* workflow_title_ = nullptr;

    bool executing_ = false;
};

} // namespace fincept::screens
