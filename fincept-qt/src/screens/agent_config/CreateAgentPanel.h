// src/screens/agent_config/CreateAgentPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// CREATE view — 3-column layout: saved agents | config form | live test panel.
class CreateAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit CreateAgentPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  public slots:
    void apply_tools_selection(const QStringList& tools);

  private:
    void build_ui();
    QWidget* build_saved_panel();
    QWidget* build_form_panel();
    QWidget* build_test_panel();
    void setup_connections();

    void load_saved_agents();
    void load_agent_into_form(const AgentConfig& config);
    void clear_form();
    void save_agent();
    void delete_agent();
    void test_agent();
    void export_json();
    void import_json();
    void load_profile_combo();
    void refresh_llm_pill();
    QJsonObject build_config_json() const;

    // ── Left: saved list ─────────────────────────────────────────────────────
    QListWidget* saved_list_  = nullptr;
    QLabel*      saved_count_ = nullptr;

    // ── Center: form ─────────────────────────────────────────────────────────
    QLineEdit*      name_edit_         = nullptr;
    QComboBox*      category_combo_    = nullptr;
    QPlainTextEdit* desc_edit_         = nullptr;
    QComboBox*      llm_profile_combo_ = nullptr;
    QLabel*         llm_resolved_lbl_  = nullptr;
    QPlainTextEdit* instructions_edit_ = nullptr;

    // Tools
    QLineEdit*   tool_search_edit_ = nullptr;
    QListWidget* tools_list_       = nullptr;

    // Feature toggles (checkable tag buttons)
    QCheckBox* reasoning_check_      = nullptr;
    QCheckBox* memory_check_         = nullptr;
    QCheckBox* knowledge_check_      = nullptr;
    QCheckBox* guardrails_check_     = nullptr;
    QCheckBox* agentic_memory_check_ = nullptr;
    QCheckBox* storage_check_        = nullptr;
    QCheckBox* tracing_check_        = nullptr;
    QCheckBox* compression_check_    = nullptr;
    QCheckBox* hooks_check_          = nullptr;
    QCheckBox* evaluation_check_     = nullptr;

    // Knowledge sub-fields
    QComboBox*      knowledge_type_combo_    = nullptr;
    QPlainTextEdit* knowledge_urls_edit_     = nullptr;
    QComboBox*      knowledge_vectordb_combo_= nullptr;
    QComboBox*      knowledge_embedder_combo_= nullptr;
    QWidget*        knowledge_sub_           = nullptr;

    // Guardrails sub-fields
    QCheckBox* guardrails_pii_check_        = nullptr;
    QCheckBox* guardrails_injection_check_  = nullptr;
    QCheckBox* guardrails_compliance_check_ = nullptr;
    QWidget*   guardrails_sub_             = nullptr;

    // Memory sub-fields
    QLineEdit* memory_db_path_edit_          = nullptr;
    QLineEdit* memory_table_edit_            = nullptr;
    QCheckBox* memory_user_memories_check_   = nullptr;
    QCheckBox* memory_session_summary_check_ = nullptr;
    QWidget*   memory_sub_                  = nullptr;

    // Storage sub-fields
    QComboBox* storage_type_combo_  = nullptr;
    QLineEdit* storage_db_path_edit_= nullptr;
    QLineEdit* storage_table_edit_  = nullptr;
    QWidget*   storage_sub_        = nullptr;

    // Agentic memory sub-fields
    QLineEdit* agentic_memory_user_id_edit_ = nullptr;
    QWidget*   agentic_memory_sub_         = nullptr;

    // MCP servers
    QListWidget* mcp_servers_list_ = nullptr;

    // Form actions
    QPushButton* save_btn_   = nullptr;
    QLabel*      status_lbl_ = nullptr;

    // ── Right: test panel ────────────────────────────────────────────────────
    QPlainTextEdit* test_query_edit_ = nullptr;
    QPushButton*    test_btn_        = nullptr;
    QLabel*         test_status_lbl_ = nullptr;
    QTextEdit*      test_result_     = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QVector<AgentConfig> saved_agents_;
    QStringList          available_tools_;
    QString              editing_id_;
    QString              pending_request_id_;
    bool                 data_loaded_ = false;
};

} // namespace fincept::screens
