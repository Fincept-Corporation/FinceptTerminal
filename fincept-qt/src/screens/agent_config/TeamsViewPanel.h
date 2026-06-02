// src/screens/agent_config/TeamsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
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
    void changeEvent(QEvent* event) override;

  public slots:
    void add_agent_from_panel(const services::AgentInfo& agent);
    void apply_tools_selection(const QStringList& tools);

  private:
    void build_ui();
    QWidget* build_team_panel();
    QWidget* build_agents_panel();
    QWidget* build_execution_panel();
    void setup_connections();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    /// Translated description for a team mode code ("coordinate"/"route"/"collaborate").
    static QString mode_description(const QString& mode);

    void populate_available_agents(const QVector<services::AgentInfo>& agents);
    void add_to_team(const services::AgentInfo& agent);
    void remove_from_team(int row);
    void run_team();
    void update_leader_combo();
    void load_team_profile_combo();
    void refresh_team_llm_label();

    // Left: team
    QLabel* team_title_ = nullptr;
    QLabel* mode_caption_ = nullptr;
    QListWidget* team_list_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QLabel* leader_caption_ = nullptr;
    QComboBox* leader_combo_ = nullptr;
    QCheckBox* show_responses_check_ = nullptr;
    QPushButton* team_remove_btn_ = nullptr;
    QLabel* team_count_ = nullptr;
    QLabel* mode_desc_label_ = nullptr;

    // Center: available agents
    QLabel* available_title_ = nullptr;
    QListWidget* available_list_ = nullptr;
    QPushButton* add_to_team_btn_ = nullptr;
    QLabel* available_count_ = nullptr;

    // Right: execution + LLM profile
    QLabel* coordinator_hdr_ = nullptr;
    QLabel* query_hdr_ = nullptr;
    QLabel* log_hdr_ = nullptr;
    QLabel* result_hdr_ = nullptr;
    QComboBox* team_profile_combo_ = nullptr; // coordinator profile picker
    QLabel* team_resolved_lbl_ = nullptr;     // shows resolved provider/model
    QPlainTextEdit* query_input_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QTextEdit* result_display_ = nullptr;
    QTextEdit* log_display_ = nullptr;
    QLabel* exec_status_ = nullptr;

    // State
    QVector<services::AgentInfo> all_agents_;
    QVector<services::AgentInfo> team_members_;
    QStringList selected_tools_; // synced from TOOLS tab
    bool executing_ = false;
    QString pending_request_id_;
};

} // namespace fincept::screens
