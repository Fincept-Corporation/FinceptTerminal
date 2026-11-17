// src/screens/agent_config/AgentConfigScreen.cpp
//
// The merged AGENTS screen (see the header): shell, sections, navigation and
// IStatefulScreen. The inspector lives in AgentConfigScreen_Inspector.cpp, the
// command-bar actions, status bar and carry-over in AgentConfigScreen_Actions.cpp.
#include "screens/agent_config/AgentConfigScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/agent_config/AgentChatPanel.h"
#include "screens/agent_config/AgentRail.h"
#include "screens/agent_config/AgentRunComposer.h"
#include "screens/agent_config/AgentRunPanel.h"
#include "screens/agent_config/AgentRunsPanel.h"
#include "screens/agent_config/AgentScreenSections.h"
#include "screens/agent_config/AgentSchedulesPanel.h"
#include "screens/agent_config/AgenticTasksPanel.h"
#include "screens/agent_config/AgentsViewPanel.h"
#include "screens/agent_config/CreateAgentPanel.h"
#include "screens/agent_config/HarnessListWidget.h"
#include "screens/agent_config/PlannerViewPanel.h"
#include "screens/agent_config/SystemViewPanel.h"
#include "screens/agent_config/TeamsViewPanel.h"
#include "screens/agent_config/ToolsViewPanel.h"
#include "screens/agent_config/WorkflowsViewPanel.h"
#include "screens/settings/AgentConnectSection.h"
#include "services/agents/AgentService.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QKeySequence>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

#include <functional>

namespace fincept::screens {

namespace sec = agent_sections;

// ── Construction ─────────────────────────────────────────────────────────────

AgentConfigScreen::AgentConfigScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("AgentConfigScreen"));
    // One stylesheet, once, before any child exists: the run / harness panels
    // moved from the Agent Workbench style themselves through object names
    // (awb*) under hosts named "awbScreen". ThemeManager rewrites its colours and
    // font sizes on an appearance change.
    setStyleSheet(screen_qss());
    services::coding_agents::CodingAgentService::instance().initialize_deferred(); // cheap + idempotent
    agentic_mode_enabled_ = services::AgentService::agentic_mode_enabled();
    inspector_pref_ = {{QString::fromLatin1(sec::kGroupBuild), false},
                       {QString::fromLatin1(sec::kGroupRun), true},
                       {QString::fromLatin1(sec::kGroupConnect), false}};
    build_ui();
    install_shortcuts();
    setup_service_connections();
    retranslate();
    set_group(QString::fromLatin1(sec::kGroupBuild));
    LOG_DEBUG("AgentConfig", "Screen constructed");
}

AgentConfigScreen::~AgentConfigScreen() {
    // A screen destroyed while shown never gets its hideEvent — balance the counts.
    if (reported_visible_) {
        services::AgentService::instance().note_agents_screen_visible(false);
        services::coding_agents::CodingAgentService::instance().set_view_visible(false);
    }
}

// ── Layout ───────────────────────────────────────────────────────────────────

void AgentConfigScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_bar();
    root->addWidget(command_bar_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(true);
    splitter_->setHandleWidth(1);

    rail_ = new AgentRail(splitter_);
    connect(rail_, &AgentRail::item_activated, this, &AgentConfigScreen::on_rail_item);
    splitter_->addWidget(rail_);

    centre_ = new QWidget(splitter_);
    centre_->setObjectName(QStringLiteral("awbScreen")); // hosts the moved Workbench panels' object-name styles
    auto* cv = new QVBoxLayout(centre_);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    tab_bar_ = new QTabBar(centre_);
    tab_bar_->setObjectName(QStringLiteral("agentSectionTabs"));
    tab_bar_->setExpanding(false);
    tab_bar_->setDrawBase(false);
    tab_bar_->setUsesScrollButtons(true);
    tab_bar_->setElideMode(Qt::ElideNone);
    tab_bar_->setAccessibleName(tr("Agent sections"));
    connect(tab_bar_, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0)
            return;
        const QString s = tab_bar_->tabData(index).toString();
        if (!s.isEmpty() && s != section_)
            set_section(s);
    });
    cv->addWidget(tab_bar_);
    centre_stack_ = new QStackedWidget(centre_);
    centre_stack_->setObjectName(QStringLiteral("AgentViewStack"));
    cv->addWidget(centre_stack_, 1);
    splitter_->addWidget(centre_);

    inspector_host_ = new QWidget(splitter_);
    inspector_host_->setObjectName(QStringLiteral("awbScreen"));
    auto* iv = new QVBoxLayout(inspector_host_);
    iv->setContentsMargins(0, 0, 0, 0);
    iv->setSpacing(0);
    inspector_ = new QStackedWidget(inspector_host_);
    inspector_->setObjectName(QStringLiteral("agentInspector"));
    iv->addWidget(inspector_);
    splitter_->addWidget(inspector_host_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    root->addWidget(splitter_, 1);

    build_status_bar();
    root->addWidget(status_bar_);

    fit_splitter();
    // Widths come from the user's font: re-fit once an appearance change settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &AgentConfigScreen::fit_splitter);
}

void AgentConfigScreen::build_command_bar() {
    command_bar_ = new ui::ScreenCommandBar(this);
    command_bar_->set_title(tr("AGENTS"));
    command_bar_->set_command_field(tr("Find an agent, team, coding agent or run"), QStringLiteral("/"));
    command_bar_->set_command_width_chars(34, 14);
    command_bar_->add_segmented(QStringLiteral("view"),
                                {{QString::fromLatin1(sec::kGroupBuild), tr("BUILD"), QStringLiteral("1"),
                                  tr("Agents, builder, teams, workflows, planner and tools (Alt+1)")},
                                 {QString::fromLatin1(sec::kGroupRun), tr("RUN"), QStringLiteral("2"),
                                  tr("Every run of every engine, NEW RUN, chat and schedules (Alt+2)")},
                                 {QString::fromLatin1(sec::kGroupConnect), tr("CONNECT"), QStringLiteral("3"),
                                  tr("Coding agents, the MCP server for external agents and the system (Alt+3)")}},
                                90);
    command_bar_->add_action(QStringLiteral("new_agent"), tr("NEW AGENT"), {}, tr("Build a new agent"),
                             ui::ScreenCommandBar::Role::Primary, 85);
    command_bar_->add_action(QStringLiteral("new_run"), tr("NEW RUN"), {},
                             tr("Open the coding-agent composer — nothing runs until you press RUN"),
                             ui::ScreenCommandBar::Role::Normal, 80);
    templates_menu_ = build_templates_menu();
    command_bar_->add_menu_action(QStringLiteral("templates"), tr("TEMPLATES"), templates_menu_, {},
                                  tr("Starter prompts for a run, or an empty agent"), 60);
    io_menu_ = build_io_menu();
    command_bar_->add_menu_action(QStringLiteral("io"), tr("IMPORT / EXPORT"), io_menu_, {},
                                  tr("Agent configs as JSON, run transcripts as Markdown"), 50);
    command_bar_->add_separator(45);
    command_bar_->add_toggle(QStringLiteral("inspect"), tr("INSPECT"), true, {},
                             tr("Show or hide the run inspector on the right"), 55);
    command_bar_->add_action(QStringLiteral("settings"), tr("SETTINGS"), {},
                             tr("Concurrency, notifications and the workspace folder of coding-agent runs"),
                             ui::ScreenCommandBar::Role::Normal, 40);
    command_bar_->add_action(QStringLiteral("agent_connect"), tr("AGENT CONNECT ›"), {},
                             tr("Settings › AI Agents: install, sign in and CHECK CONNECTIONS of coding agents"),
                             ui::ScreenCommandBar::Role::Normal, 35);
    command_bar_->set_overflow_text(tr("More"));

    connect(command_bar_, &ui::ScreenCommandBar::segment_selected, this,
            [this](const QString&, const QString& key) { set_group(key); });
    connect(command_bar_, &ui::ScreenCommandBar::action_triggered, this, &AgentConfigScreen::on_bar_action);
    connect(command_bar_, &ui::ScreenCommandBar::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("inspect")) {
            inspector_pref_[group_] = on;
            set_inspector_visible(on);
            fincept::ScreenStateManager::instance().notify_changed(this);
        }
    });
    connect(command_bar_, &ui::ScreenCommandBar::command_edited, this, [this](const QString& text) {
        rail_->set_filter_text(text);
        if (runs_panel_)
            runs_panel_->set_text_filter(text);
    });
    connect(command_bar_, &ui::ScreenCommandBar::command_navigate, this, [this](int) { rail_->setFocus(); });
}

void AgentConfigScreen::build_status_bar() {
    using SB = ui::ScreenStatusBar;
    status_bar_ = new SB(this);
    status_bar_->add_item(QStringLiteral("selection"), SB::Segment::Left, {}, SB::Tone::Normal, 100);
    status_bar_->add_item(QStringLiteral("model"), SB::Segment::Left, {}, SB::Tone::Info, 80);
    status_bar_->add_item(QStringLiteral("agents"), SB::Segment::Left, {}, SB::Tone::Dim, 40);
    status_bar_->add_item(QStringLiteral("runs"), SB::Segment::Right, {}, SB::Tone::Normal, 90);
    status_bar_->add_item(QStringLiteral("cost"), SB::Segment::Right, {}, SB::Tone::Dim, 70);
    status_bar_->add_item(QStringLiteral("mcp"), SB::Segment::Right, {}, SB::Tone::Dim, 50);
    status_bar_->add_item(QStringLiteral("harnesses"), SB::Segment::Right, {}, SB::Tone::Dim, 45);
}

void AgentConfigScreen::install_shortcuts() {
    auto add = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, std::move(fn));
    };
    add(QKeySequence(Qt::Key_Slash), [this]() { command_bar_->focus_command_field(); });
    add(QKeySequence(Qt::ALT | Qt::Key_1), [this]() { set_group(QString::fromLatin1(sec::kGroupBuild)); });
    add(QKeySequence(Qt::ALT | Qt::Key_2), [this]() { set_group(QString::fromLatin1(sec::kGroupRun)); });
    add(QKeySequence(Qt::ALT | Qt::Key_3), [this]() { set_group(QString::fromLatin1(sec::kGroupConnect)); });
}

QString AgentConfigScreen::section_title(const QString& s) const {
    if (s == QLatin1String(sec::kAgents))
        return tr("AGENTS");
    if (s == QLatin1String(sec::kCreate))
        return tr("CREATE");
    if (s == QLatin1String(sec::kTeams))
        return tr("TEAMS");
    if (s == QLatin1String(sec::kWorkflows))
        return tr("WORKFLOWS");
    if (s == QLatin1String(sec::kPlanner))
        return tr("PLANNER");
    if (s == QLatin1String(sec::kTools))
        return tr("TOOLS");
    if (s == QLatin1String(sec::kRuns))
        return tr("RUNS");
    if (s == QLatin1String(sec::kRun))
        return tr("NEW RUN");
    if (s == QLatin1String(sec::kChat))
        return tr("CHAT");
    if (s == QLatin1String(sec::kSchedules))
        return tr("SCHEDULES");
    if (s == QLatin1String(sec::kHarnesses))
        return tr("HARNESSES");
    if (s == QLatin1String(sec::kConnect))
        return tr("CONNECT");
    if (s == QLatin1String(sec::kSystem))
        return tr("SYSTEM");
    return s.toUpper();
}

void AgentConfigScreen::rebuild_tabs() {
    const QSignalBlocker block(tab_bar_);
    while (tab_bar_->count() > 0)
        tab_bar_->removeTab(0);
    for (const QString& s : sec::sections_of(group_)) {
        const int i = tab_bar_->addTab(section_title(s));
        tab_bar_->setTabData(i, s);
    }
    for (int i = 0; i < tab_bar_->count(); ++i) {
        if (tab_bar_->tabData(i).toString() == section_)
            tab_bar_->setCurrentIndex(i);
    }
}

void AgentConfigScreen::retranslate() {
    command_bar_->set_title(tr("AGENTS"));
    rebuild_tabs();
    update_status();
}

void AgentConfigScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

// ── Navigation ───────────────────────────────────────────────────────────────

void AgentConfigScreen::set_group(const QString& group_in) {
    const QString group = sec::sections_of(group_in).isEmpty() ? QString::fromLatin1(sec::kGroupBuild) : group_in;
    group_ = group;
    command_bar_->set_segment(QStringLiteral("view"), group_);
    QString target = group_section_.value(group_);
    if (target.isEmpty() || sec::group_of(target) != group_)
        target = sec::sections_of(group_).constFirst();
    section_.clear(); // force set_section to apply
    set_section(target);
    const bool inspect = inspector_pref_.value(group_, false);
    command_bar_->set_action_checked(QStringLiteral("inspect"), inspect);
    set_inspector_visible(inspect);
}

void AgentConfigScreen::set_section(const QString& section) {
    if (!sec::is_section(section))
        return;
    const QString group = sec::group_of(section);
    if (group != group_) {
        group_section_[group] = section;
        set_group(group); // re-enters with the section
        return;
    }
    if (section == section_ && section_widget(section))
        return;
    section_ = section;
    group_section_[group_] = section;
    if (QWidget* w = ensure_section(section)) {
        // A stacked widget is never narrower than its widest page: only the page in
        // view takes part in the width (a hidden page at a 24 px font once made the
        // whole screen scroll sideways).
        for (int i = 0; i < centre_stack_->count(); ++i) {
            if (QWidget* page = centre_stack_->widget(i)) {
                QSizePolicy sp = page->sizePolicy();
                sp.setHorizontalPolicy(page == w ? QSizePolicy::Preferred : QSizePolicy::Ignored);
                if (page->sizePolicy() != sp)
                    page->setSizePolicy(sp);
            }
        }
        centre_stack_->setCurrentWidget(w);
    }
    rebuild_tabs();
    update_status();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

QWidget* AgentConfigScreen::section_widget(const QString& section) const {
    return sections_.value(section, nullptr);
}

QWidget* AgentConfigScreen::ensure_section(const QString& section) {
    if (QWidget* w = section_widget(section))
        return w;
    QWidget* w = nullptr;
    if (section == QLatin1String(sec::kAgents)) {
        agents_panel_ = new AgentsViewPanel(centre_stack_);
        agents_panel_->set_list_visible(false); // the rail is the agent list
        w = agents_panel_;
    } else if (section == QLatin1String(sec::kCreate)) {
        create_panel_ = new CreateAgentPanel(centre_stack_);
        w = create_panel_;
    } else if (section == QLatin1String(sec::kTeams)) {
        teams_panel_ = new TeamsViewPanel(centre_stack_);
        w = teams_panel_;
    } else if (section == QLatin1String(sec::kWorkflows)) {
        workflows_panel_ = new WorkflowsViewPanel(centre_stack_);
        w = workflows_panel_;
    } else if (section == QLatin1String(sec::kPlanner)) {
        planner_panel_ = new PlannerViewPanel(centre_stack_);
        w = planner_panel_;
    } else if (section == QLatin1String(sec::kTools)) {
        tools_panel_ = new ToolsViewPanel(centre_stack_);
        w = tools_panel_;
    } else if (section == QLatin1String(sec::kChat)) {
        chat_panel_ = new AgentChatPanel(centre_stack_);
        w = chat_panel_;
    } else if (section == QLatin1String(sec::kSystem)) {
        system_panel_ = new SystemViewPanel(centre_stack_);
        w = system_panel_;
    } else if (section == QLatin1String(sec::kRuns)) {
        runs_panel_ = new AgentRunsPanel(centre_stack_);
        runs_panel_->set_agentic_enabled(agentic_mode_enabled_);
        runs_panel_->set_text_filter(command_bar_->command_text());
        connect(runs_panel_, &AgentRunsPanel::run_selected, this,
                [this](const QString& key) { open_run_key(key, false); });
        connect(runs_panel_, &AgentRunsPanel::open_run_requested, this,
                [this](const QString& key) { open_run_key(key, true); });
        connect(runs_panel_, &AgentRunsPanel::new_run_requested, this,
                [this]() { set_section(QString::fromLatin1(sec::kRun)); });
        connect(runs_panel_, &AgentRunsPanel::agentic_action_requested, this, &AgentConfigScreen::on_agentic_action);
        connect(runs_panel_, &AgentRunsPanel::counts_changed, this, &AgentConfigScreen::update_status);
        if (!selected_run_key_.isEmpty())
            runs_panel_->select_run(selected_run_key_);
        w = runs_panel_;
    } else if (section == QLatin1String(sec::kRun)) {
        composer_ = new AgentRunComposer(centre_stack_);
        connect(composer_, &AgentRunComposer::run_started, this, [this](const QString& id) {
            const QString key = agent_runs::run_key(agent_runs::Engine::Harness, id);
            set_section(QString::fromLatin1(sec::kRuns));
            if (runs_panel_)
                runs_panel_->select_run(key);
            open_run_key(key, false);
            inspector_pref_[group_] = true;
            command_bar_->set_action_checked(QStringLiteral("inspect"), true);
            set_inspector_visible(true);
        });
        connect(composer_, &AgentRunComposer::state_changed, this,
                [this]() { fincept::ScreenStateManager::instance().notify_changed(this); });
        if (pending_run_.contains(QStringLiteral("composer")))
            composer_->restore_state(pending_run_.take(QStringLiteral("composer")).toMap());
        w = composer_;
    } else if (section == QLatin1String(sec::kSchedules)) {
        schedules_panel_ = new AgentSchedulesPanel(centre_stack_);
        connect(schedules_panel_, &AgentSchedulesPanel::open_run_requested, this, [this](const QString& id) {
            const QString key = agent_runs::run_key(agent_runs::Engine::Harness, id);
            open_run_key(key, true);
        });
        w = schedules_panel_;
    } else if (section == QLatin1String(sec::kHarnesses)) {
        // The same widget as Settings › AI Agents › Agent harnesses.
        harness_scroll_ = new QScrollArea(centre_stack_);
        harness_scroll_->setWidgetResizable(true);
        harness_scroll_->setFrameShape(QFrame::NoFrame);
        harness_list_ = new HarnessListWidget(harness_scroll_);
        harness_scroll_->setWidget(harness_list_);
        w = harness_scroll_;
    } else if (section == QLatin1String(sec::kConnect)) {
        // The MCP-server half of Settings › AI Agents (the harness half is HARNESSES).
        connect_panel_ = new AgentConnectSection(centre_stack_, AgentConnectSection::Sections::McpServerOnly);
        w = connect_panel_;
    }
    if (!w)
        return nullptr;
    sections_.insert(section, w);
    centre_stack_->addWidget(w);
    wire_cross_panel_signals();
    apply_pending_drafts();
    LOG_DEBUG("AgentConfig", QString("Section built section=%1").arg(section));
    return w;
}

void AgentConfigScreen::wire_cross_panel_signals() {
    // AGENTS → TEAMS: "Add to Team"
    if (agents_panel_ && teams_panel_)
        connect(agents_panel_, &AgentsViewPanel::add_agent_to_team, teams_panel_, &TeamsViewPanel::add_agent_from_panel,
                Qt::UniqueConnection);
    // TOOLS → AGENTS / CREATE / TEAMS: live selection propagation
    if (tools_panel_ && agents_panel_) {
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, agents_panel_,
                &AgentsViewPanel::apply_tools_selection, Qt::UniqueConnection);
        // TOOLS → AGENTS: reload the agent's tools right after an assign (a lambda:
        // wired once, UniqueConnection does not apply to functors).
        if (!tools_assign_wired_) {
            tools_assign_wired_ = true;
            AgentsViewPanel* agents = agents_panel_;
            connect(tools_panel_, &ToolsViewPanel::tool_assigned, agents,
                    [agents](const QString&, const QStringList& tools) { agents->apply_tools_selection(tools); });
        }
    }
    if (tools_panel_ && create_panel_)
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, create_panel_,
                &CreateAgentPanel::apply_tools_selection, Qt::UniqueConnection);
    if (tools_panel_ && teams_panel_)
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, teams_panel_,
                &TeamsViewPanel::apply_tools_selection, Qt::UniqueConnection);
    // SERVICE → SYSTEM: refresh when agents / tools / LLM configs change
    if (system_panel_) {
        auto& svc = services::AgentService::instance();
        connect(&svc, &services::AgentService::agents_discovered, system_panel_, &SystemViewPanel::on_agents_changed,
                Qt::UniqueConnection);
        connect(&svc, &services::AgentService::tools_loaded, system_panel_, &SystemViewPanel::on_tools_changed,
                Qt::UniqueConnection);
        connect(&svc, &services::AgentService::config_saved, system_panel_, &SystemViewPanel::on_llm_config_changed,
                Qt::UniqueConnection);
        connect(&svc, &services::AgentService::config_deleted, system_panel_,
                &SystemViewPanel::on_llm_config_changed, Qt::UniqueConnection);
    }
}

void AgentConfigScreen::apply_pending_drafts() {
    if (create_panel_ && pending_state_.contains(QStringLiteral("create_draft")))
        create_panel_->restore_draft(pending_state_.take(QStringLiteral("create_draft")).toMap());
    if (agents_panel_ && pending_state_.contains(QStringLiteral("agents_draft")))
        agents_panel_->restore_draft(pending_state_.take(QStringLiteral("agents_draft")).toMap());
    if (workflows_panel_ && pending_state_.contains(QStringLiteral("workflows_draft")))
        workflows_panel_->restore_draft(pending_state_.take(QStringLiteral("workflows_draft")).toMap());
}

void AgentConfigScreen::apply_agentic_visibility() {
    if (runs_panel_)
        runs_panel_->set_agentic_enabled(agentic_mode_enabled_);
    // An agentic task in the inspector while Agentic Mode went off: back to the empty page.
    if (!agentic_mode_enabled_ && task_view_ && inspector_ && inspector_->currentWidget() == task_view_) {
        selected_run_key_.clear();
        inspector_->setCurrentWidget(inspector_empty_);
    }
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool AgentConfigScreen::open_section(const QString& section, const QString& focus) {
    const sec::Target t = sec::resolve(section, focus);
    if (t.section.isEmpty())
        return false;
    set_section(t.section);
    if (t.section == QLatin1String(sec::kAgents) && !t.focus.isEmpty() && agents_panel_) {
        // Not listed yet (discovery still running): selected when the list arrives.
        pending_agent_ = agents_panel_->select_agent(t.focus) ? QString() : t.focus;
        rail_->set_current(QStringLiteral("agent"), t.focus);
    } else if (t.section == QLatin1String(sec::kTeams) && !t.focus.isEmpty() && teams_panel_) {
        teams_panel_->open_team(t.focus);
        rail_->set_current(QStringLiteral("team"), t.focus);
    } else if (t.section == QLatin1String(sec::kHarnesses) && !t.focus.isEmpty() && harness_list_) {
        harness_list_->select_harness(t.focus);
        rail_->set_current(QStringLiteral("harness"), t.focus);
    } else if (t.section == QLatin1String(sec::kRuns) && runs_panel_) {
        if (!t.engine.isEmpty())
            runs_panel_->set_engine_filter(t.engine);
        if (!t.focus.isEmpty()) {
            agent_runs::Engine e;
            QString id;
            if (agent_runs::split_run_key(t.focus, &e, &id)) {
                const QString key = agent_runs::run_key(e, id);
                runs_panel_->select_run(key);
                open_run_key(key, false);
            }
        }
    } else if (t.section == QLatin1String(sec::kRun) && composer_) {
        if (t.templates)
            composer_->open_templates();
        else
            composer_->focus_prompt();
    }
    LOG_DEBUG("AgentConfig", QString("Section opened by link section=%1").arg(t.section));
    return true;
}

// ── Visibility ───────────────────────────────────────────────────────────────

void AgentConfigScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Runs that finish while this screen is hidden raise a desktop notification
    // (both services count their visible screens).
    if (!reported_visible_) {
        reported_visible_ = true;
        services::AgentService::instance().note_agents_screen_visible(true);
        services::coding_agents::CodingAgentService::instance().set_view_visible(true);
    }
    if (first_show_) {
        first_show_ = false;
        carry_workbench_state_once();
        // Discovery is a local read (Python, cached) — never a model call.
        rail_->set_discovery_state(services::AgentService::instance().cached_agents().isEmpty(), {});
        services::AgentService::instance().discover_agents();
    }
    // Rate-gated pull of cloud agent configs on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("agent_config"));
    status_bar_->start_clock();
    update_status();
}

void AgentConfigScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (reported_visible_) {
        reported_visible_ = false;
        services::AgentService::instance().note_agents_screen_visible(false);
        services::coding_agents::CodingAgentService::instance().set_view_visible(false);
    }
    status_bar_->stop_clock();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap AgentConfigScreen::save_state() const {
    QVariantMap state{{QStringLiteral("section"), section_},
                      {QStringLiteral("group"), group_},
                      {QStringLiteral("inspect_build"), inspector_pref_.value(QString::fromLatin1(sec::kGroupBuild))},
                      {QStringLiteral("inspect_run"), inspector_pref_.value(QString::fromLatin1(sec::kGroupRun))},
                      {QStringLiteral("inspect_connect"),
                       inspector_pref_.value(QString::fromLatin1(sec::kGroupConnect))},
                      {QStringLiteral("selected_run"), selected_run_key_}};
    // A panel never opened this session still owns the draft restored for it.
    if (create_panel_)
        state[QStringLiteral("create_draft")] = create_panel_->save_draft();
    else if (pending_state_.contains(QStringLiteral("create_draft")))
        state[QStringLiteral("create_draft")] = pending_state_.value(QStringLiteral("create_draft"));
    if (agents_panel_)
        state[QStringLiteral("agents_draft")] = agents_panel_->save_draft();
    else if (pending_state_.contains(QStringLiteral("agents_draft")))
        state[QStringLiteral("agents_draft")] = pending_state_.value(QStringLiteral("agents_draft"));
    if (workflows_panel_)
        state[QStringLiteral("workflows_draft")] = workflows_panel_->save_draft();
    else if (pending_state_.contains(QStringLiteral("workflows_draft")))
        state[QStringLiteral("workflows_draft")] = pending_state_.value(QStringLiteral("workflows_draft"));
    // The composer's options and the inspector's run (the Workbench's "run" map).
    QVariantMap run = composer_ ? composer_->save_state() : pending_run_.value(QStringLiteral("composer")).toMap();
    if (run_view_ && !run_view_->current_run_id().isEmpty())
        run.insert(QStringLiteral("run_id"), run_view_->current_run_id());
    else if (pending_run_.contains(QStringLiteral("run_id")))
        run.insert(QStringLiteral("run_id"), pending_run_.value(QStringLiteral("run_id")));
    if (!run.isEmpty())
        state[QStringLiteral("run")] = run;
    return state;
}

void AgentConfigScreen::restore_state(const QVariantMap& state) {
    // Three shapes: this screen's, AGENT CONFIG's (view int + drafts) and AGENT
    // WORKBENCH's (tab_id / tab + run) — a saved panel of either old tab restores here.
    const sec::StateShape shape = sec::state_shape(state);
    if (shape == sec::StateShape::Empty)
        return;
    pending_state_ = state;
    const QVariantMap run = sec::run_part_of_state(state);
    if (!run.isEmpty()) {
        QVariantMap composer = run;
        composer.remove(QStringLiteral("run_id"));
        pending_run_.insert(QStringLiteral("composer"), composer);
        if (run.contains(QStringLiteral("run_id")))
            pending_run_.insert(QStringLiteral("run_id"), run.value(QStringLiteral("run_id")));
        if (composer_)
            composer_->restore_state(pending_run_.take(QStringLiteral("composer")).toMap());
    }
    if (shape == sec::StateShape::Merged) {
        inspector_pref_[QString::fromLatin1(sec::kGroupBuild)] = state.value(QStringLiteral("inspect_build")).toBool();
        inspector_pref_[QString::fromLatin1(sec::kGroupRun)] = state.value(QStringLiteral("inspect_run"), true).toBool();
        inspector_pref_[QString::fromLatin1(sec::kGroupConnect)] =
            state.value(QStringLiteral("inspect_connect")).toBool();
        selected_run_key_ = state.value(QStringLiteral("selected_run")).toString();
    } else if (shape == sec::StateShape::WorkbenchV1 && pending_run_.contains(QStringLiteral("run_id"))) {
        selected_run_key_ = agent_runs::run_key(agent_runs::Engine::Harness,
                                                pending_run_.value(QStringLiteral("run_id")).toString());
    }
    QString section = sec::section_for_state(state);
    // The AGENTIC view became the run list: only while Agentic Mode is on does it
    // open filtered to agentic tasks; otherwise the run list as it is.
    if (section.isEmpty())
        section = QString::fromLatin1(sec::kAgents);
    set_section(section);
    if (shape == sec::StateShape::AgentConfigV1 && state.value(QStringLiteral("view")).toInt() == 8 && runs_panel_ &&
        agentic_mode_enabled_)
        runs_panel_->set_engine_filter(QStringLiteral("agentic"));
    if (!selected_run_key_.isEmpty()) {
        if (runs_panel_)
            runs_panel_->select_run(selected_run_key_);
        open_run_key(selected_run_key_, false);
    }
    apply_pending_drafts();
}

} // namespace fincept::screens
