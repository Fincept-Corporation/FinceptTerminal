// src/screens/agent_config/AgentConfigScreen.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Main Agent Studio screen — 8-view navigation shell.
/// Uses QStackedWidget to switch between AGENTS, CREATE, TEAMS,
/// WORKFLOWS, PLANNER, TOOLS, CHAT, SYSTEM sub-panels.
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
    void setup_connections();
    void set_view(services::AgentViewMode mode);
    QPushButton* make_nav_btn(const QString& text, services::AgentViewMode mode);

    QStackedWidget* view_stack_ = nullptr;
    QVector<QPushButton*> nav_buttons_;
    QLabel* status_label_ = nullptr;
    QLabel* agent_count_label_ = nullptr;

    services::AgentViewMode current_view_ = services::AgentViewMode::Agents;
    bool first_show_ = true;
};

} // namespace fincept::screens
