// src/screens/agent_config/AgentConfigScreen.cpp
#include "screens/agent_config/AgentConfigScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/agent_config/AgentChatPanel.h"
#include "screens/agent_config/AgentsViewPanel.h"
#include "screens/agent_config/CreateAgentPanel.h"
#include "screens/agent_config/PlannerViewPanel.h"
#include "screens/agent_config/SystemViewPanel.h"
#include "screens/agent_config/TeamsViewPanel.h"
#include "screens/agent_config/ToolsViewPanel.h"
#include "screens/agent_config/WorkflowsViewPanel.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

// ── View mode metadata ───────────────────────────────────────────────────────

struct ViewMeta {
    services::AgentViewMode mode;
    const char* label;
};

static constexpr ViewMeta kViews[] = {
    {services::AgentViewMode::Agents, "AGENTS"},   {services::AgentViewMode::Create, "CREATE"},
    {services::AgentViewMode::Teams, "TEAMS"},     {services::AgentViewMode::Workflows, "WORKFLOWS"},
    {services::AgentViewMode::Planner, "PLANNER"}, {services::AgentViewMode::Tools, "TOOLS"},
    {services::AgentViewMode::Chat, "CHAT"},       {services::AgentViewMode::System, "SYSTEM"},
};

// ── Constructor ──────────────────────────────────────────────────────────────

AgentConfigScreen::AgentConfigScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentConfigScreen");
    build_ui();
    setup_service_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── UI construction ──────────────────────────────────────────────────────────

void AgentConfigScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_nav_bar(root);

    view_stack_ = new QStackedWidget;
    view_stack_->setObjectName("AgentViewStack");
    root->addWidget(view_stack_, 1);

    // No placeholders — panels are added to the stack on first navigation.
    // ensure_panel_built() appends them and sets currentWidget directly.

    build_status_bar(root);
}

void AgentConfigScreen::build_nav_bar(QVBoxLayout* root) {
    auto* nav_bar = new QWidget(this);
    nav_bar->setObjectName("AgentNavBar");
    nav_bar->setFixedHeight(40);
    nav_bar->setStyleSheet(QString("QWidget#AgentNavBar { background: %1; border-bottom: 1px solid %2; }")
                               .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(nav_bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto* title = new QLabel("AGENT STUDIO");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:2px;padding-right:16px;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_MED));
    sep->setFixedWidth(1);
    hl->addWidget(sep);

    for (const auto& vm : kViews) {
        auto* btn = make_nav_btn(vm.label, vm.mode);
        hl->addWidget(btn);
        nav_buttons_.append(btn);
    }

    hl->addStretch();

    agent_count_label_ = new QLabel;
    agent_count_label_->setStyleSheet(
        QString("color:%1;font-size:11px;padding:2px 8px;background:%2;border-radius:2px;")
            .arg(ui::colors::AMBER, ui::colors::BG_SURFACE));
    hl->addWidget(agent_count_label_);

    root->addWidget(nav_bar);
}

void AgentConfigScreen::build_status_bar(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);

    status_label_ = new QLabel("READY");
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    hl->addWidget(status_label_);
    hl->addStretch();

    auto* view_label = new QLabel;
    view_label->setObjectName("AgentStatusView");
    view_label->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::AMBER));
    hl->addWidget(view_label);

    root->addWidget(bar);

    connect(view_stack_, &QStackedWidget::currentChanged, this, [this, view_label](int) {
        // Find the label for the current mode
        const int idx = static_cast<int>(current_view_);
        if (idx >= 0 && idx < static_cast<int>(std::size(kViews)))
            view_label->setText(kViews[idx].label);
    });
}

QPushButton* AgentConfigScreen::make_nav_btn(const QString& text, services::AgentViewMode mode) {
    auto* btn = new QPushButton(text);
    btn->setCheckable(true);
    btn->setChecked(mode == services::AgentViewMode::Agents);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; font-size: 11px; font-weight: 600; "
                "letter-spacing: 1px; padding: 8px 12px; border: none; border-bottom: 2px solid transparent; }"
                "QPushButton:hover { color: %2; }"
                "QPushButton:checked { color: %2; border-bottom: 2px solid %2; }")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::AMBER));

    connect(btn, &QPushButton::clicked, this, [this, mode]() { set_view(mode); });
    return btn;
}

// ── Lazy panel construction ──────────────────────────────────────────────────

void AgentConfigScreen::ensure_panel_built(services::AgentViewMode mode) {
    const int idx = static_cast<int>(mode);
    if (panel_built_[idx])
        return;
    panel_built_[idx] = true;

    QWidget* panel = nullptr;
    switch (mode) {
        case services::AgentViewMode::Agents:
            agents_panel_ = new AgentsViewPanel;
            panel = agents_panel_;
            break;
        case services::AgentViewMode::Create:
            create_panel_ = new CreateAgentPanel;
            panel = create_panel_;
            break;
        case services::AgentViewMode::Teams:
            teams_panel_ = new TeamsViewPanel;
            panel = teams_panel_;
            break;
        case services::AgentViewMode::Workflows:
            workflows_panel_ = new WorkflowsViewPanel;
            panel = workflows_panel_;
            break;
        case services::AgentViewMode::Planner:
            planner_panel_ = new PlannerViewPanel;
            panel = planner_panel_;
            break;
        case services::AgentViewMode::Tools:
            tools_panel_ = new ToolsViewPanel;
            panel = tools_panel_;
            break;
        case services::AgentViewMode::Chat:
            chat_panel_ = new AgentChatPanel;
            panel = chat_panel_;
            break;
        case services::AgentViewMode::System:
            system_panel_ = new SystemViewPanel;
            panel = system_panel_;
            break;
    }

    // Add panel to stack — no placeholder removal needed, index tracked via pointer
    view_stack_->addWidget(panel);

    // Wire cross-panel signals now that relevant panels may be available
    wire_cross_panel_signals();

    LOG_INFO("AgentConfigScreen", QString("Built panel: %1").arg(kViews[idx].label));
}

QWidget* AgentConfigScreen::panel_widget(services::AgentViewMode mode) const {
    switch (mode) {
        case services::AgentViewMode::Agents:
            return agents_panel_;
        case services::AgentViewMode::Create:
            return create_panel_;
        case services::AgentViewMode::Teams:
            return teams_panel_;
        case services::AgentViewMode::Workflows:
            return workflows_panel_;
        case services::AgentViewMode::Planner:
            return planner_panel_;
        case services::AgentViewMode::Tools:
            return tools_panel_;
        case services::AgentViewMode::Chat:
            return chat_panel_;
        case services::AgentViewMode::System:
            return system_panel_;
    }
    return nullptr;
}

void AgentConfigScreen::wire_cross_panel_signals() {
    // AGENTS → TEAMS: "Add to Team" button
    if (agents_panel_ && teams_panel_)
        connect(agents_panel_, &AgentsViewPanel::add_agent_to_team, teams_panel_, &TeamsViewPanel::add_agent_from_panel,
                Qt::UniqueConnection);

    // TOOLS → AGENTS: live selection propagation
    if (tools_panel_ && agents_panel_)
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, agents_panel_,
                &AgentsViewPanel::apply_tools_selection, Qt::UniqueConnection);

    // TOOLS → AGENTS: reload agent tools from DB immediately after assign
    if (tools_panel_ && agents_panel_)
        connect(
            tools_panel_, &ToolsViewPanel::tool_assigned, agents_panel_,
            [this](const QString& /*id*/, const QStringList& tools) { agents_panel_->apply_tools_selection(tools); },
            Qt::UniqueConnection);

    // TOOLS → CREATE: selection propagation
    if (tools_panel_ && create_panel_)
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, create_panel_,
                &CreateAgentPanel::apply_tools_selection, Qt::UniqueConnection);

    // TOOLS → TEAMS: selection propagation
    if (tools_panel_ && teams_panel_)
        connect(tools_panel_, &ToolsViewPanel::tools_selection_changed, teams_panel_,
                &TeamsViewPanel::apply_tools_selection, Qt::UniqueConnection);

    // SERVICE → SYSTEM: refresh when agents/tools change
    if (system_panel_) {
        connect(
            &services::AgentService::instance(), &services::AgentService::agents_discovered, system_panel_,
            [this](QVector<services::AgentInfo>, QVector<services::AgentCategory>) {
                system_panel_->on_agents_changed();
            },
            Qt::UniqueConnection);

        connect(
            &services::AgentService::instance(), &services::AgentService::tools_loaded, system_panel_,
            [this](services::AgentToolsInfo info) { system_panel_->on_tools_changed(info); }, Qt::UniqueConnection);

        // LLM config saved/deleted → refresh LLM list in System tab
        connect(
            &services::AgentService::instance(), &services::AgentService::config_saved, system_panel_,
            [this]() { system_panel_->on_llm_config_changed(); }, Qt::UniqueConnection);

        connect(
            &services::AgentService::instance(), &services::AgentService::config_deleted, system_panel_,
            [this]() { system_panel_->on_llm_config_changed(); }, Qt::UniqueConnection);
    }
}

// ── View switching ───────────────────────────────────────────────────────────

void AgentConfigScreen::set_view(services::AgentViewMode mode) {
    const int idx = static_cast<int>(mode);

    // Build the panel on first visit (P2: lazy construction)
    ensure_panel_built(mode);

    // Use setCurrentWidget — safe regardless of insertion order in stack
    QWidget* panel = panel_widget(mode);
    if (panel)
        view_stack_->setCurrentWidget(panel);
    current_view_ = mode;

    for (int i = 0; i < nav_buttons_.size(); ++i)
        nav_buttons_[i]->setChecked(i == idx);

    fincept::ScreenStateManager::instance().notify_changed(this);
}

// ── Service-level connections (global signals, no per-panel wiring) ──────────

void AgentConfigScreen::setup_service_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                agent_count_label_->setText(QString("%1 agents").arg(agents.size()));
            });

    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& ctx, const QString& msg) {
        status_label_->setText(QString("ERROR [%1]: %2").arg(ctx, msg.left(60)));
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::NEGATIVE));
    });

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.success) {
            status_label_->setText(QString("DONE (%1ms)").arg(r.execution_time_ms));
            status_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::POSITIVE));
        } else {
            status_label_->setText(QString("FAILED: %1").arg(r.error.left(60)));
            status_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::NEGATIVE));
        }
    });

    // Task 4 & 9: when an agent is saved/deleted in CREATE, refresh the agent
    // list so AGENTS tab and TEAMS available list stay in sync.
    connect(&svc, &services::AgentService::config_saved, this, [this]() {
        services::AgentService::instance().clear_cache();
        services::AgentService::instance().discover_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, [this]() {
        services::AgentService::instance().clear_cache();
        services::AgentService::instance().discover_agents();
    });
}

// ── Visibility ───────────────────────────────────────────────────────────────

void AgentConfigScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (first_show_) {
        first_show_ = false;
        // Build the default (AGENTS) panel and kick off discovery once
        ensure_panel_built(services::AgentViewMode::Agents);
        services::AgentService::instance().discover_agents();
    }
}

void AgentConfigScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap AgentConfigScreen::save_state() const {
    return {{"view", static_cast<int>(current_view_)}};
}

void AgentConfigScreen::restore_state(const QVariantMap& state) {
    const int v = state.value("view", -1).toInt();
    if (v < 0)
        return;
    set_view(static_cast<services::AgentViewMode>(v));
}

} // namespace fincept::screens
