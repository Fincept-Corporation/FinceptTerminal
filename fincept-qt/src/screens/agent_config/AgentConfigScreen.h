// src/screens/agent_config/AgentConfigScreen.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

// Forward-declare all panels — constructed lazily on first navigation
namespace fincept::screens {
class AgentsViewPanel;
class CreateAgentPanel;
class TeamsViewPanel;
class WorkflowsViewPanel;
class PlannerViewPanel;
class ToolsViewPanel;
class AgentChatPanel;
class SystemViewPanel;
}

namespace fincept::screens {

/// Main Agent Studio screen — 8-view navigation shell.
/// Panels are constructed lazily on first navigation (P2 compliance).
class AgentConfigScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AgentConfigScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void build_nav_bar(QVBoxLayout* root);
    void build_status_bar(QVBoxLayout* root);
    void setup_service_connections();
    void set_view(services::AgentViewMode mode);
    QPushButton* make_nav_btn(const QString& text, services::AgentViewMode mode);

    void ensure_panel_built(services::AgentViewMode mode);
    QWidget* panel_widget(services::AgentViewMode mode) const;
    void wire_cross_panel_signals();

    QStackedWidget* view_stack_        = nullptr;
    QVector<QPushButton*> nav_buttons_;
    QLabel* status_label_              = nullptr;
    QLabel* agent_count_label_         = nullptr;

    // Lazily constructed panels (nullptr until first navigation)
    AgentsViewPanel*   agents_panel_    = nullptr;
    CreateAgentPanel*  create_panel_    = nullptr;
    TeamsViewPanel*    teams_panel_     = nullptr;
    WorkflowsViewPanel* workflows_panel_ = nullptr;
    PlannerViewPanel*  planner_panel_   = nullptr;
    ToolsViewPanel*    tools_panel_     = nullptr;
    AgentChatPanel*    chat_panel_      = nullptr;
    SystemViewPanel*   system_panel_    = nullptr;

    // Track which stack slots have been populated
    bool panel_built_[8] = {};

    services::AgentViewMode current_view_ = services::AgentViewMode::Agents;
    bool first_show_ = true;
};

} // namespace fincept::screens
