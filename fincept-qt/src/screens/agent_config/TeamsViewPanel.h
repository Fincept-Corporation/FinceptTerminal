// src/screens/agent_config/TeamsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// TEAMS view — assemble multi-agent teams and execute queries.
class TeamsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TeamsViewPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  public slots:
    void add_agent_from_panel(const services::AgentInfo& agent);

  private:
    void build_ui();
    QWidget* build_team_panel();
    QWidget* build_agents_panel();
    QWidget* build_execution_panel();
    void setup_connections();

    void populate_available_agents(const QVector<services::AgentInfo>& agents);
    void add_to_team(const services::AgentInfo& agent);
    void remove_from_team(int row);
    void run_team();
    void update_leader_combo();
    void load_team_profile_combo();
    void refresh_team_llm_label();

    // Left: team
    QListWidget* team_list_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QComboBox* leader_combo_ = nullptr;
    QCheckBox* show_responses_check_ = nullptr;
    QLabel* team_count_ = nullptr;
    QLabel* mode_desc_label_ = nullptr;

    // Center: available agents
    QListWidget* available_list_ = nullptr;
    QLabel* available_count_ = nullptr;

    // Right: execution + LLM profile
    QComboBox* team_profile_combo_  = nullptr;  // coordinator profile picker
    QLabel*    team_resolved_lbl_   = nullptr;  // shows resolved provider/model
    QPlainTextEdit* query_input_    = nullptr;
    QPushButton* run_btn_ = nullptr;
    QTextEdit* result_display_ = nullptr;
    QTextEdit* log_display_ = nullptr;
    QLabel* exec_status_ = nullptr;

    // State
    QVector<services::AgentInfo> all_agents_;
    QVector<services::AgentInfo> team_members_;
    bool executing_ = false;
    bool data_loaded_ = false;
};

} // namespace fincept::screens
