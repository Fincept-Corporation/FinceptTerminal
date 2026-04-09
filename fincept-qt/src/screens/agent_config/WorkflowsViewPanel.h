// src/screens/agent_config/WorkflowsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// WORKFLOWS view — 3-column: catalog | params | execution log + result.
class WorkflowsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit WorkflowsViewPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    QWidget* build_catalog_panel();
    QWidget* build_params_panel();
    QWidget* build_output_panel();
    void setup_connections();

    void on_workflow_selected(int row);
    void run_current_workflow();

    // ── Left: catalog ────────────────────────────────────────────────────────
    QListWidget* catalog_list_ = nullptr;
    QLabel* wf_desc_label_ = nullptr;

    // ── Center: params ───────────────────────────────────────────────────────
    QLabel* params_title_ = nullptr;
    QComboBox* llm_profile_combo_ = nullptr;
    QLabel* llm_resolved_lbl_ = nullptr;

    // Symbol input (stock analysis)
    QWidget* symbol_row_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;

    // Custom query input (multi-query / custom)
    QWidget* query_row_ = nullptr;
    QTextEdit* query_input_ = nullptr;

    QPushButton* run_btn_ = nullptr;
    QLabel* params_status_ = nullptr;

    // ── Right: output ────────────────────────────────────────────────────────
    QLabel* output_title_ = nullptr;
    QLabel* output_status_ = nullptr;
    QTextEdit* log_display_ = nullptr;
    QTextEdit* result_display_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    bool executing_ = false;
    bool data_loaded_ = false;
    QString pending_request_id_;
    QString current_workflow_type_;
};

} // namespace fincept::screens
