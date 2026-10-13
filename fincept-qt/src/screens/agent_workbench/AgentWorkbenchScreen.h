#pragma once
// AgentWorkbenchScreen.h — "AGENT WORKBENCH": run Claude Code / OpenAI Codex CLI
// agents locally as long-running background tasks wired to the terminal's own
// tools over MCP, schedule recurring briefs, audit every run, and connect
// standalone agents (Claude Code, Codex, Cursor, Claude Desktop) to the terminal.
//
//   RUN        composer + live transcript of the selected run
//   RUNS       sortable / filterable history
//   SCHEDULES  recurring runs
//   CONNECT    the external MCP server + client setup + activity log
//
// The screen only renders: runs, schedules and processes live in
// services::coding_agents::CodingAgentService and keep going when the screen is
// hidden or closed. Styling is ONE stylesheet on this root (object-name rules).

#include "screens/common/IStatefulScreen.h"

#include <QList>
#include <QMetaObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace fincept::screens {

class AgentConnectPanel;
class AgentRunPanel;
class AgentRunsPanel;
class AgentSchedulesPanel;

class AgentWorkbenchScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AgentWorkbenchScreen(QWidget* parent = nullptr);
    ~AgentWorkbenchScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("agent_workbench"); }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void apply_theme();
    void retranslate();
    void set_tab(int index);
    void update_summary();
    void open_settings();
    void open_run(const QString& run_id);

    QLabel* title_ = nullptr;
    QList<QPushButton*> tab_btns_;
    QLabel* summary_ = nullptr;
    QPushButton* settings_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    AgentRunPanel* run_panel_ = nullptr;
    AgentRunsPanel* runs_panel_ = nullptr;
    AgentSchedulesPanel* schedules_panel_ = nullptr;
    AgentConnectPanel* connect_panel_ = nullptr;

    int tab_ = 0;
    bool visibility_reported_ = false;
    bool cli_probed_ = false;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
