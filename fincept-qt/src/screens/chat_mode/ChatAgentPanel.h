#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QString>
#include <QWidget>

#include <memory>

class QEvent;
class QShowEvent;

namespace fincept::chat_mode {

class ComposerContextBar;

/// Which pages a ChatAgentPanel carries (2026-11-17 revamp: one class, two places).
enum class AgentPanelPages {
    Inspector, ///< right rail: CONTEXT | PLAN | ACTIVITY | FILES (the run's artefacts)
    Agent,     ///< left rail: MEMORY | MCP (the cloud agent's memory, remote MCP servers)
};

/// Chat Mode's run inspector (WP2) and agent panel.
///
/// Inspector pages CONTEXT (what the run can see and use: context chips with
/// detach / attach, tools and MCP servers, the run's files, budget and limits, run
/// stats) | PLAN (RunPlanPanel) | ACTIVITY (RunActivityView) | FILES
/// (RunArtefactsPanel). Agent pages MEMORY | MCP. The run pages show the window's
/// current run (set_current_run: the selected conversation's latest run, or the run
/// picked in RUNS). Pages are built when first shown; the last page picked is
/// remembered per panel kind. Nothing here blocks: no exec() dialogs — forms and
/// confirmations are inline.
class ChatAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatAgentPanel(QWidget* parent = nullptr, AgentPanelPages pages = AgentPanelPages::Inspector);
    ~ChatAgentPanel() override;

    AgentPanelPages pages() const;

    void refresh_memory();
    void refresh_mcp_servers();

    /// The run this window shows (ChatModeScreen; empty: none). The CONTEXT, PLAN,
    /// ACTIVITY and FILES pages follow it.
    void set_current_run(const QString& run_id);
    QString current_run() const;

    /// The composer's context bar: the CONTEXT page lists its chips (detach) and
    /// adds to them (attach). Not owned.
    void set_context_bar(ComposerContextBar* bar);

    /// Page ids: "context", "plan", "activity", "files" (Inspector); "memory",
    /// "mcp" (Agent). Builds the page on first use. False for an id this panel
    /// does not carry.
    bool show_page(const QString& id);
    QString current_page() const;
    /// The panel's own tab strip (the left rail hides it: its switch picks the page).
    void set_tabs_visible(bool visible);
    /// Keyboard focus into the shown page (its list / first control).
    void focus_page();

  signals:
    /// The inspector wants the window to show `run_id` (e.g. a retry it started).
    void run_selection_requested(const QString& run_id);
    /// WP2: the user wants the conversation of `run_id` (to answer the question
    /// it waits on / review its approval card).
    void run_open_requested(const QString& run_id);
    /// WP5: an empty FILES page's action — start a task (NEW RUN).
    void new_run_requested();
    /// WP5: open another screen after a hand-off ("report_builder", "notes").
    void open_screen_requested(const QString& screen_id);
    /// The shown page changed (user or show_page()).
    void page_changed(const QString& id);
    /// The cloud agent's MCP servers were read (CONTEXT or MCP page): connected /
    /// total, (-1, -1) when they could not be read — for the status bar.
    void mcp_state_changed(int connected, int total);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
