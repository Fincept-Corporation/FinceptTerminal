// src/screens/agent_config/AgentsViewPanel.h
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
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// AGENTS view — 3-panel layout for listing, editing, and testing agents.
/// LLM provider/model is sourced from LlmService (configured in Settings > LLM Config).
class AgentsViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentsViewPanel(QWidget* parent = nullptr);
    QVariantMap save_draft() const;
    void restore_draft(const QVariantMap& draft);
    /// The agents screen's rail is the agent list: it hides this panel's own list.
    void set_list_visible(bool visible);
    /// Open an agent in the editor (switches the category to All when needed);
    /// false while discovery has not listed it.
    bool select_agent(const QString& agent_id);
    QString selected_agent_id() const;

  public slots:
    void apply_tools_selection(const QStringList& tools);

  signals:
    void add_agent_to_team(services::AgentInfo agent);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    QWidget* build_agent_list_panel();
    QWidget* build_config_panel();
    QWidget* build_query_panel();
    void setup_connections();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    void populate_agent_list(const QVector<services::AgentInfo>& agents);
    void on_agent_selected(int row);
    void on_category_changed(int index);
    void load_agent_into_editor(const services::AgentInfo& agent);
    QJsonObject build_config_from_editor() const;
    void save_current_config();
    void delete_current_config();
    void run_query();
    void toggle_json_editor();
    void refresh_llm_pill();
    void load_profile_combo();
    /// A run's answer: a structured model's laid-out view (with a RAW toggle to
    /// the JSON), or the markdown answer.
    void show_result(const services::AgentExecutionResult& r);
    /// Rebuild the structured page from structured_model_ / structured_data_.
    void rebuild_structured_view();
    /// Back to the markdown page with no structured result.
    void clear_structured();
    /// The agent list's page: the list, the loading skeleton (cold discovery),
    /// or a state (discovery failed + RETRY / no agents / none in the category).
    void show_list_state();
    /// Discover again from Python (cache dropped); shows the skeleton when the list is empty.
    void rediscover();
    /// The test pane's preview (class C: a run sends the query to the LLM):
    /// which agent and model would answer, the cost note, RUN.
    void refresh_test_preview();

    // ── Left panel ───────────────────────────────────────────────────────────
    QWidget* list_panel_ = nullptr;
    bool list_hidden_by_host_ = false; // set_list_visible(false): shown only for its states
    QLabel* list_title_ = nullptr;
    QStackedWidget* list_stack_ = nullptr; // 0 = agent_list_, 1 = list_skeleton_, 2 = list_state_
    ui::LoadingSkeleton* list_skeleton_ = nullptr;
    ui::EmptyState* list_state_ = nullptr;
    bool discovering_ = false;     // a discovery is running and the list had nothing to show
    QString discover_error_;       // last discovery failure (shown when the list is empty)
    QListWidget* agent_list_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QLabel* list_count_label_ = nullptr;
    QPushButton* list_refresh_btn_ = nullptr;

    // ── Center panel ─────────────────────────────────────────────────────────
    QPushButton* json_toggle_btn_ = nullptr;
    QPushButton* add_team_btn_ = nullptr;
    QLabel* agent_name_label_ = nullptr;
    QLabel* agent_desc_label_ = nullptr;
    // Section headers in the config form.
    QLabel* llm_profile_hdr_ = nullptr;
    QLabel* instructions_hdr_ = nullptr;
    QLabel* tools_hdr_ = nullptr;
    QLabel* features_hdr_ = nullptr;
    // LLM profile picker — selects which named profile this agent uses
    QComboBox* llm_profile_combo_ = nullptr; // "Default (Global)" + named profiles
    QLabel* llm_resolved_lbl_ = nullptr;     // shows resolved provider/model beneath
    // Agent-specific fields
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
    // JSON editor
    QPushButton* json_apply_btn_ = nullptr;
    QPlainTextEdit* json_editor_ = nullptr;
    QWidget* form_widget_ = nullptr;
    QWidget* json_widget_ = nullptr;
    bool json_mode_ = false;

    // ── Right panel ──────────────────────────────────────────────────────────
    QLabel* test_query_hdr_ = nullptr;
    QLabel* output_caption_ = nullptr;
    QLabel* result_hdr_ = nullptr;
    QPlainTextEdit* query_input_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QTextEdit* result_display_ = nullptr;
    // Page 0 = result_display_ (markdown / raw JSON), page 1 = structured view,
    // page 2 = test_empty_ (no result yet: what a run would do, RUN).
    QStackedWidget* result_stack_ = nullptr;
    ui::EmptyState* test_empty_ = nullptr;
    bool test_llm_ok_ = false; // an LLM resolves for the selected agent (refresh_llm_pill)
    QScrollArea* structured_scroll_ = nullptr;
    QPushButton* raw_toggle_btn_ = nullptr; // CARDS <-> RAW, shown for structured results
    QString structured_model_;
    QJsonObject structured_data_;
    bool structured_valid_ = true;
    QLabel* result_status_ = nullptr;
    QComboBox* output_model_combo_ = nullptr;
    QCheckBox* auto_route_check_ = nullptr;
    QLabel* routing_info_label_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QVector<services::AgentInfo> all_agents_;
    QVector<services::AgentInfo> filtered_agents_;
    int selected_agent_idx_ = -1;
    bool executing_ = false;
    QString pending_request_id_; // guards against cross-panel signal contamination
    QString routing_request_id_; // Auto-Route: the route_query() whose routing_result labels the run
};

} // namespace fincept::screens
