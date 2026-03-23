// src/screens/agent_config/CreateAgentPanel.h
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
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// CREATE view — build and save custom agent configurations.
/// Full config editor with all 50+ fields matching the Tauri version.
class CreateAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit CreateAgentPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    QWidget* build_saved_list_panel();
    QWidget* build_form_panel();
    void setup_connections();

    void load_saved_agents();
    void load_agent_into_form(const AgentConfig& config);
    void clear_form();
    void save_agent();
    void delete_agent();
    void test_agent();
    void export_json();
    void import_json();
    QJsonObject build_config_json() const;

    // Left panel
    QListWidget* saved_list_ = nullptr;
    QLabel* saved_count_ = nullptr;

    // ── Basic fields ─────────────────────────────────────────────────────────
    QLineEdit* name_edit_ = nullptr;
    QPlainTextEdit* desc_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;

    // ── Model fields ─────────────────────────────────────────────────────────
    QComboBox* provider_combo_ = nullptr;
    QLineEdit* model_id_edit_ = nullptr;
    QDoubleSpinBox* temperature_spin_ = nullptr;
    QSpinBox* max_tokens_spin_ = nullptr;

    // ── Instructions ─────────────────────────────────────────────────────────
    QPlainTextEdit* instructions_edit_ = nullptr;

    // ── Tools (multi-select list) ────────────────────────────────────────────
    QListWidget* tools_list_ = nullptr;
    QLineEdit* tool_search_edit_ = nullptr;

    // ── Feature toggles ──────────────────────────────────────────────────────
    QCheckBox* reasoning_check_ = nullptr;
    QCheckBox* memory_check_ = nullptr;
    QCheckBox* knowledge_check_ = nullptr;
    QCheckBox* guardrails_check_ = nullptr;
    QCheckBox* agentic_memory_check_ = nullptr;
    QCheckBox* tracing_check_ = nullptr;
    QCheckBox* compression_check_ = nullptr;
    QCheckBox* hooks_check_ = nullptr;
    QCheckBox* evaluation_check_ = nullptr;
    QCheckBox* storage_check_ = nullptr;

    // ── Knowledge config ─────────────────────────────────────────────────────
    QComboBox* knowledge_type_combo_ = nullptr;
    QPlainTextEdit* knowledge_urls_edit_ = nullptr;
    QComboBox* knowledge_vectordb_combo_ = nullptr;
    QComboBox* knowledge_embedder_combo_ = nullptr;

    // ── Guardrails config ────────────────────────────────────────────────────
    QCheckBox* guardrails_pii_check_ = nullptr;
    QCheckBox* guardrails_injection_check_ = nullptr;
    QCheckBox* guardrails_compliance_check_ = nullptr;

    // ── Memory config ────────────────────────────────────────────────────────
    QLineEdit* memory_db_path_edit_ = nullptr;
    QLineEdit* memory_table_edit_ = nullptr;
    QCheckBox* memory_user_memories_check_ = nullptr;
    QCheckBox* memory_session_summary_check_ = nullptr;

    // ── Storage config ───────────────────────────────────────────────────────
    QComboBox* storage_type_combo_ = nullptr;
    QLineEdit* storage_db_path_edit_ = nullptr;
    QLineEdit* storage_table_edit_ = nullptr;

    // ── Agentic memory config ────────────────────────────────────────────────
    QLineEdit* agentic_memory_user_id_edit_ = nullptr;

    // ── MCP servers ──────────────────────────────────────────────────────────
    QListWidget* mcp_servers_list_ = nullptr;

    // ── Actions / test ───────────────────────────────────────────────────────
    QPushButton* save_btn_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTextEdit* test_result_ = nullptr;

    // State
    QVector<AgentConfig> saved_agents_;
    QStringList available_tools_;
    QString editing_id_;
    bool data_loaded_ = false;
};

} // namespace fincept::screens
