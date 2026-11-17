// src/screens/agent_config/AgentConfigScreen.h
#pragma once
// AgentConfigScreen — the one AGENTS screen (screen id "agent_config", tab label
// "Agent Config"; the old "agent_workbench" id is an alias that opens it on RUNS).
// It merges Agent Studio (agents, builder, teams, workflows, planner, tools,
// chat, system, agentic tasks) and the Agent Workbench (coding-agent runs,
// schedules, harnesses, the MCP server for external agents):
//
//   ┌ ScreenCommandBar: ▌AGENTS [› find …  /] [BUILD|RUN|CONNECT] NEW AGENT · NEW RUN ·
//   │                   TEMPLATES ▾ · IMPORT / EXPORT ▾ · INSPECT · SETTINGS · AGENT CONNECT ›
//   ├ section tabs of the view (stable ids, AgentScreenSections.h)
//   ├ LEFT rail: agents by category, teams, coding agents (state chips), active runs
//   ├ CENTRE: the section (lazily built)
//   ├ RIGHT inspector: the selected run — harness transcript / agentic task detail /
//   │                  Chat Mode summary — or what to do when none is selected
//   └ ScreenStatusBar: selection · model · run state · cost · MCP · coding agents
//
// Implements IStatefulScreen (one state for both old tabs; restore_state reads the
// two old shapes too) and ISectionLinked (open_section(section, focus): focus may
// be an agent id, a team id, a harness id or a run key / id).

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QMenu;
class QPushButton;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTimer;

namespace fincept::ui {
class EmptyState;
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens {

class AgentChatPanel;
class AgentConnectSection;
class AgentRail;
class AgentRunComposer;
class AgentRunPanel;
class AgentRunsPanel;
class AgentSchedulesPanel;
class AgenticTasksPanel;
class AgentsViewPanel;
class CreateAgentPanel;
class HarnessListWidget;
class PlannerViewPanel;
class SystemViewPanel;
class TeamsViewPanel;
class ToolsViewPanel;
class WorkflowsViewPanel;

class AgentConfigScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit AgentConfigScreen(QWidget* parent = nullptr);
    ~AgentConfigScreen() override;

    // IStatefulScreen — section, view, inspector, the panels' drafts and the composer.
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return QStringLiteral("agent_config"); }

    // ISectionLinked
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    // Layout (AgentConfigScreen.cpp)
    void build_ui();
    void build_command_bar();
    void build_status_bar();
    void install_shortcuts();
    void retranslate();
    void rebuild_tabs();
    QString section_title(const QString& section) const;

    // Navigation
    void set_group(const QString& group);
    void set_section(const QString& section);
    QWidget* ensure_section(const QString& section);
    QWidget* section_widget(const QString& section) const;
    void wire_cross_panel_signals();
    void apply_pending_drafts();
    void apply_agentic_visibility();

    // Inspector (AgentConfigScreen_Inspector.cpp)
    void ensure_inspector();
    void set_inspector_visible(bool on);
    void open_run_key(const QString& key, bool move_focus);
    void show_chat_run(const QString& run_id);
    void fit_splitter();

    // Actions + status (AgentConfigScreen_Actions.cpp)
    void setup_service_connections();
    void on_bar_action(const QString& id);
    void on_rail_item(const QString& kind, const QString& id);
    void on_agentic_action(const QString& action);
    void open_run_settings();
    void open_agent_connect();
    void update_status();
    QMenu* build_templates_menu();
    QMenu* build_io_menu();
    void carry_workbench_state_once();
    static QString screen_qss();

    // ── Shell ────────────────────────────────────────────────────────────────
    ui::ScreenCommandBar* command_bar_ = nullptr;
    ui::ScreenStatusBar* status_bar_ = nullptr;
    QTabBar* tab_bar_ = nullptr;
    QSplitter* splitter_ = nullptr;
    AgentRail* rail_ = nullptr;
    QWidget* centre_ = nullptr;
    QStackedWidget* centre_stack_ = nullptr;
    QWidget* inspector_host_ = nullptr;
    QStackedWidget* inspector_ = nullptr;
    QMenu* templates_menu_ = nullptr;
    QMenu* io_menu_ = nullptr;
    QTimer* status_timer_ = nullptr; // coalesces status-bar refreshes (run updates arrive in bursts)

    // ── Sections (built on first use) ───────────────────────────────────────
    AgentsViewPanel* agents_panel_ = nullptr;
    CreateAgentPanel* create_panel_ = nullptr;
    TeamsViewPanel* teams_panel_ = nullptr;
    WorkflowsViewPanel* workflows_panel_ = nullptr;
    PlannerViewPanel* planner_panel_ = nullptr;
    ToolsViewPanel* tools_panel_ = nullptr;
    AgentChatPanel* chat_panel_ = nullptr;
    SystemViewPanel* system_panel_ = nullptr;
    AgentRunsPanel* runs_panel_ = nullptr;
    AgentRunComposer* composer_ = nullptr;
    AgentSchedulesPanel* schedules_panel_ = nullptr;
    QScrollArea* harness_scroll_ = nullptr;
    HarnessListWidget* harness_list_ = nullptr;
    AgentConnectSection* connect_panel_ = nullptr;
    QHash<QString, QWidget*> sections_;

    // ── Inspector pages ──────────────────────────────────────────────────────
    ui::EmptyState* inspector_empty_ = nullptr;
    AgentRunPanel* run_view_ = nullptr;
    AgenticTasksPanel* task_view_ = nullptr;
    QWidget* chat_card_ = nullptr;
    QLabel* chat_title_ = nullptr;
    QLabel* chat_meta_ = nullptr;
    QLabel* chat_prompt_ = nullptr;
    QPushButton* chat_open_btn_ = nullptr;
    QString chat_run_id_;

    // ── State ────────────────────────────────────────────────────────────────
    QString group_;
    QString section_;
    QHash<QString, QString> group_section_; // last section of each view group
    QHash<QString, bool> inspector_pref_;    // inspector shown per view group
    QString selected_run_key_;
    QString pending_agent_;       // open_section() focus waiting for the agent list
    QVariantMap pending_state_;   // drafts of panels not built yet
    QVariantMap pending_run_;     // composer + inspector state until they exist
    int last_agent_count_ = 0;
    bool agentic_mode_enabled_ = true;
    bool first_show_ = true;
    bool tools_assign_wired_ = false;
    bool reported_visible_ = false; // AgentService + CodingAgentService visible-screen counts
};

} // namespace fincept::screens
