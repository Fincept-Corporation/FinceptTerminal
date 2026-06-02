// src/screens/agent_config/CreateAgentPanel.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/AgentConfigRepository.h"
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

/// CREATE view — 3-column layout: saved agents | config form | live test panel.
class CreateAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit CreateAgentPanel(QWidget* parent = nullptr);

    // Called by AgentConfigScreen for IStatefulScreen state persistence.
    QVariantMap save_draft() const;
    void restore_draft(const QVariantMap& draft);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  public slots:
    void apply_tools_selection(const QStringList& tools);

  private:
    void build_ui();
    QWidget* build_saved_panel();
    QWidget* build_form_panel();
    QWidget* build_test_panel();
    void setup_connections();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange. Covers widgets built
    /// in both CreateAgentPanel.cpp and the CreateAgentPanel_Layout.cpp split TU.
    void retranslateUi();

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
    QLabel* saved_title_ = nullptr;
    QListWidget* saved_list_ = nullptr;
    QLabel* saved_count_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QPushButton* del_btn_ = nullptr;
    QPushButton* exp_btn_ = nullptr;
    QPushButton* imp_btn_ = nullptr;

    // ── Center: form ─────────────────────────────────────────────────────────
    // Section headers + field labels (built in the _Layout split TU).
    QLabel* identity_hdr_ = nullptr;
    QLabel* name_field_lbl_ = nullptr;
    QLabel* category_field_lbl_ = nullptr;
    QLabel* model_hdr_ = nullptr;
    QLabel* instructions_hdr_ = nullptr;
    QLabel* tools_hdr_ = nullptr;
    QLabel* terminal_tools_hdr_ = nullptr;
    QLabel* terminal_cat_lbl_ = nullptr;
    QLabel* terminal_max_lbl_ = nullptr;
    QLabel* features_hdr_ = nullptr;
    QLabel* knowledge_type_lbl_ = nullptr;
    QLabel* knowledge_vectordb_lbl_ = nullptr;
    QLabel* knowledge_embedder_lbl_ = nullptr;
    QLabel* memory_dbpath_lbl_ = nullptr;
    QLabel* memory_table_lbl_ = nullptr;
    QLabel* storage_type_lbl_ = nullptr;
    QLabel* storage_dbpath_lbl_ = nullptr;
    QLabel* storage_table_lbl_ = nullptr;
    QLabel* agentic_userid_lbl_ = nullptr;
    QLabel* mcp_servers_hdr_ = nullptr;
    QPushButton* clear_btn_ = nullptr;

    QLineEdit* name_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QPlainTextEdit* desc_edit_ = nullptr;
    QComboBox* llm_profile_combo_ = nullptr;
    QLabel* llm_resolved_lbl_ = nullptr;
    QPlainTextEdit* instructions_edit_ = nullptr;

    // Tools
    QLineEdit* tool_search_edit_ = nullptr;
    QListWidget* tools_list_ = nullptr;

    // Feature toggles (checkable tag buttons)
    QCheckBox* reasoning_check_ = nullptr;
    QCheckBox* memory_check_ = nullptr;
    QCheckBox* knowledge_check_ = nullptr;
    QCheckBox* guardrails_check_ = nullptr;
    QCheckBox* agentic_memory_check_ = nullptr;
    QCheckBox* storage_check_ = nullptr;
    QCheckBox* tracing_check_ = nullptr;
    QCheckBox* compression_check_ = nullptr;
    QCheckBox* hooks_check_ = nullptr;
    QCheckBox* evaluation_check_ = nullptr;

    // Knowledge sub-fields
    QComboBox* knowledge_type_combo_ = nullptr;
    QPlainTextEdit* knowledge_urls_edit_ = nullptr;
    QComboBox* knowledge_vectordb_combo_ = nullptr;
    QComboBox* knowledge_embedder_combo_ = nullptr;
    QWidget* knowledge_sub_ = nullptr;

    // Guardrails sub-fields
    QCheckBox* guardrails_pii_check_ = nullptr;
    QCheckBox* guardrails_injection_check_ = nullptr;
    QCheckBox* guardrails_compliance_check_ = nullptr;
    QWidget* guardrails_sub_ = nullptr;

    // Memory sub-fields
    QLineEdit* memory_db_path_edit_ = nullptr;
    QLineEdit* memory_table_edit_ = nullptr;
    QCheckBox* memory_user_memories_check_ = nullptr;
    QCheckBox* memory_session_summary_check_ = nullptr;
    QWidget* memory_sub_ = nullptr;

    // Storage sub-fields
    QComboBox* storage_type_combo_ = nullptr;
    QLineEdit* storage_db_path_edit_ = nullptr;
    QLineEdit* storage_table_edit_ = nullptr;
    QWidget* storage_sub_ = nullptr;

    // Agentic memory sub-fields
    QLineEdit* agentic_memory_user_id_edit_ = nullptr;
    QWidget* agentic_memory_sub_ = nullptr;

    // MCP servers
    QListWidget* mcp_servers_list_ = nullptr;

    // Terminal MCP bridge — internal tools exposed to this agent
    QCheckBox* terminal_tools_check_ = nullptr;
    QCheckBox* terminal_destructive_check_ = nullptr;
    QCheckBox* terminal_external_check_ = nullptr;
    QCheckBox* terminal_dry_run_check_ = nullptr;
    QListWidget* terminal_categories_list_ = nullptr;
    QLineEdit* terminal_exclude_cats_edit_ = nullptr;
    QLineEdit* terminal_name_include_edit_ = nullptr;
    QLineEdit* terminal_name_exclude_edit_ = nullptr;
    QSpinBox* terminal_max_tools_spin_ = nullptr;
    QWidget* terminal_sub_ = nullptr;

    // Form actions
    QPushButton* save_btn_ = nullptr;
    QLabel* status_lbl_ = nullptr;

    // ── Right: test panel ────────────────────────────────────────────────────
    QLabel* test_panel_hdr_ = nullptr;
    QLabel* test_query_hdr_ = nullptr;
    QLabel* test_output_hdr_ = nullptr;
    QPlainTextEdit* test_query_edit_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QLabel* test_status_lbl_ = nullptr;
    QTextEdit* test_result_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QVector<AgentConfig> saved_agents_;
    QStringList available_tools_;
    QString editing_id_;
    QString pending_request_id_;
    bool data_loaded_ = false;
};

} // namespace fincept::screens
