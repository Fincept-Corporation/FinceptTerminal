// src/screens/agent_config/AgentsViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/AgentConfigRepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// AGENTS view — 3-panel layout for listing, editing, and testing agents.
class AgentsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentsViewPanel(QWidget* parent = nullptr);

  signals:
    void add_agent_to_team(services::AgentInfo agent);

  private:
    void build_ui();
    QWidget* build_agent_list_panel();
    QWidget* build_config_panel();
    QWidget* build_query_panel();
    void setup_connections();

    void populate_agent_list(const QVector<services::AgentInfo>& agents);
    void on_agent_selected(int row);
    void on_category_changed(int index);
    void load_agent_into_editor(const services::AgentInfo& agent);
    QJsonObject build_config_from_editor() const;
    void save_current_config();
    void delete_current_config();
    void run_query();
    void toggle_json_editor();

    // ── Left panel ───────────────────────────────────────────────────────────
    QListWidget* agent_list_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QLabel* list_count_label_ = nullptr;

    // ── Center panel ─────────────────────────────────────────────────────────
    QLabel* agent_name_label_ = nullptr;
    QLabel* agent_desc_label_ = nullptr;
    QComboBox* provider_combo_ = nullptr;
    QLineEdit* model_id_edit_ = nullptr;
    QDoubleSpinBox* temperature_spin_ = nullptr;
    QSpinBox* max_tokens_spin_ = nullptr;
    QPlainTextEdit* instructions_edit_ = nullptr;
    QListWidget* tools_list_ = nullptr;
    QCheckBox* reasoning_check_ = nullptr;
    QCheckBox* memory_check_ = nullptr;
    QCheckBox* knowledge_check_ = nullptr;
    QCheckBox* guardrails_check_ = nullptr;
    QCheckBox* tracing_check_ = nullptr;
    QCheckBox* agentic_memory_check_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* add_team_btn_ = nullptr;
    // JSON editor
    QPushButton* json_toggle_btn_ = nullptr;
    QPlainTextEdit* json_editor_ = nullptr;
    QWidget* form_widget_ = nullptr;
    QWidget* json_widget_ = nullptr;
    bool json_mode_ = false;

    // ── Right panel ──────────────────────────────────────────────────────────
    QPlainTextEdit* query_input_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QTextEdit* result_display_ = nullptr;
    QLabel* result_status_ = nullptr;
    QComboBox* output_model_combo_ = nullptr;
    QCheckBox* auto_route_check_ = nullptr;
    QLabel* routing_info_label_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QVector<services::AgentInfo> all_agents_;
    QVector<services::AgentInfo> filtered_agents_;
    int selected_agent_idx_ = -1;
    bool executing_ = false;
};

} // namespace fincept::screens
