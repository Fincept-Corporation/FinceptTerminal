// src/screens/agent_config/AgentConfigScreen_Actions.cpp
//
// The agents screen's command-bar actions, rail clicks, status bar, service
// wiring, the coding-agent run settings dialog (from the Agent Workbench), the
// one-time carry-over of the Workbench's saved screen state, and the screen's
// one stylesheet.
#include "screens/agent_config/AgentConfigScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/server/McpHttpServer.h"
#include "screens/agent_config/AgentPromptTemplates.h"
#include "screens/agent_config/AgentRail.h"
#include "screens/agent_config/AgentRunCommon.h"
#include "screens/agent_config/AgentRunComposer.h"
#include "screens/agent_config/AgentRunPanel.h"
#include "screens/agent_config/AgentRunsPanel.h"
#include "screens/agent_config/AgentScreenSections.h"
#include "screens/agent_config/AgenticTasksPanel.h"
#include "screens/agent_config/AgentsViewPanel.h"
#include "screens/agent_config/CreateAgentPanel.h"
#include "screens/agent_config/HarnessListWidget.h"
#include "screens/agent_config/TeamsViewPanel.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agents/AgentService.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace fincept::screens {

namespace sec = agent_sections;
namespace ca = services::coding_agents;

// ── Service wiring ───────────────────────────────────────────────────────────

void AgentConfigScreen::setup_service_connections() {
    status_timer_ = new QTimer(this);
    status_timer_->setSingleShot(true);
    status_timer_->setInterval(400);
    connect(status_timer_, &QTimer::timeout, this, [this]() {
        if (isVisible())
            update_status();
    });
    auto later = [this]() {
        if (isVisible() && !status_timer_->isActive())
            status_timer_->start();
    };

    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                last_agent_count_ = static_cast<int>(agents.size());
                update_status();
                if (!pending_agent_.isEmpty()) {
                    // After the AGENTS panel has taken the same list (its slot runs after this one).
                    QTimer::singleShot(0, this, [this]() {
                        if (agents_panel_ && !pending_agent_.isEmpty() && agents_panel_->select_agent(pending_agent_))
                            pending_agent_.clear();
                    });
                }
            });
    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& ctx, const QString& msg) {
        if (ctx.contains(QLatin1String("discover"), Qt::CaseInsensitive))
            rail_->set_discovery_state(false, msg);
        if (isVisible())
            status_bar_->show_notice(tr("ERROR [%1]: %2").arg(ctx, msg.left(80)), {}, {}, 9000,
                                     ui::ScreenStatusBar::Tone::Danger);
    });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (!isVisible())
            return;
        if (r.success)
            status_bar_->show_notice(tr("DONE (%1ms)").arg(r.execution_time_ms), {}, {}, 5000,
                                     ui::ScreenStatusBar::Tone::Ok);
        else
            status_bar_->show_notice(tr("FAILED: %1").arg(r.error.left(80)), {}, {}, 9000,
                                     ui::ScreenStatusBar::Tone::Danger);
    });
    // An agent saved / deleted anywhere: the agent list, the rail and TEAMS follow.
    connect(&svc, &services::AgentService::config_saved, this, []() {
        services::AgentService::instance().clear_cache();
        services::AgentService::instance().discover_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, []() {
        services::AgentService::instance().clear_cache();
        services::AgentService::instance().discover_agents();
    });
    // Settings › Developer › Agentic Mode, live.
    connect(&fincept::EventBus::instance(), &fincept::EventBus::eventPublished, this,
            [this](const QString& event, const QVariantMap& data) {
                if (event == QStringLiteral("settings.agentic_mode_changed")) {
                    agentic_mode_enabled_ = data.value(QStringLiteral("enabled")).toBool();
                    apply_agentic_visibility();
                }
            });

    auto& runs = ca::CodingAgentService::instance();
    connect(&runs, &ca::CodingAgentService::run_added, this, later);
    connect(&runs, &ca::CodingAgentService::run_finished, this, later);
    connect(&runs, &ca::CodingAgentService::runs_reset, this, later);
    connect(&runs, &ca::CodingAgentService::run_updated, this, later);
    auto& tasks = agent_tasks::TaskRunService::instance();
    connect(&tasks, &agent_tasks::TaskRunService::attention_changed, this, later);
    connect(&mcp::McpHttpServer::instance(), &mcp::McpHttpServer::status_changed, this, later);
    connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::status_changed, this, later);
}

// ── Command bar ──────────────────────────────────────────────────────────────

void AgentConfigScreen::on_bar_action(const QString& id) {
    if (id == QLatin1String("new_agent")) {
        set_section(QString::fromLatin1(sec::kCreate));
        if (create_panel_)
            create_panel_->start_new_agent();
    } else if (id == QLatin1String("new_run")) {
        set_section(QString::fromLatin1(sec::kRun));
        if (composer_)
            composer_->focus_prompt();
    } else if (id == QLatin1String("settings")) {
        open_run_settings();
    } else if (id == QLatin1String("agent_connect")) {
        open_agent_connect();
    }
}

QMenu* AgentConfigScreen::build_templates_menu() {
    auto* menu = new QMenu(this);
    menu->setStyleSheet(ui::ScreenCommandBar::menu_css());
    // Filled on every open: the entries follow the language of the moment.
    connect(menu, &QMenu::aboutToShow, this, [this, menu]() {
        menu->clear();
        menu->addSection(tr("Start a coding-agent run from"));
        const auto templates = agent_prompt_templates();
        for (int i = 0; i < templates.size(); ++i) {
            // Inserts the prompt into NEW RUN; RUN stays the only way it starts.
            menu->addAction(templates[i].title, this, [this, i]() {
                set_section(QString::fromLatin1(sec::kRun));
                if (composer_)
                    composer_->insert_template(i);
            });
        }
        menu->addSeparator();
        menu->addAction(tr("Empty agent (CREATE)"), this, [this]() { on_bar_action(QStringLiteral("new_agent")); });
    });
    return menu;
}

QMenu* AgentConfigScreen::build_io_menu() {
    auto* menu = new QMenu(this);
    menu->setStyleSheet(ui::ScreenCommandBar::menu_css());
    connect(menu, &QMenu::aboutToShow, this, [this, menu]() {
        menu->clear();
        menu->addAction(tr("Import agent (JSON)…"), this, [this]() {
            set_section(QString::fromLatin1(sec::kCreate));
            if (create_panel_)
                create_panel_->import_agent_json();
        });
        QAction* exp = menu->addAction(tr("Export agent (JSON)…"), this, [this]() {
            set_section(QString::fromLatin1(sec::kCreate));
            if (create_panel_)
                create_panel_->export_agent_json();
        });
        exp->setProperty("fincept_agent_risk", "file"); // agents: writes a file the user picks
        menu->addSeparator();
        QAction* tx = menu->addAction(tr("Export run transcript (.md)…"), this, [this]() {
            if (run_view_)
                run_view_->export_transcript();
        });
        tx->setProperty("fincept_agent_risk", "file"); // agents: writes a file the user picks
        const bool harness_run = run_view_ && !run_view_->current_run_id().isEmpty() &&
                                 selected_run_key_.startsWith(QLatin1String("harness:"));
        tx->setEnabled(harness_run);
        if (!harness_run)
            tx->setToolTip(tr("Select a coding-agent run first"));
    });
    return menu;
}

void AgentConfigScreen::on_agentic_action(const QString& action) {
    ensure_inspector();
    if (!task_view_ || !agentic_mode_enabled_)
        return;
    if (action == QLatin1String("schedule"))
        task_view_->schedule_new_task();
    else if (action == QLatin1String("schedules"))
        task_view_->on_schedules_clicked();
    else if (action == QLatin1String("libraries"))
        task_view_->on_libraries_clicked();
}

// ── Rail ─────────────────────────────────────────────────────────────────────

void AgentConfigScreen::on_rail_item(const QString& kind, const QString& id) {
    if (kind == QLatin1String("agent")) {
        open_section(QStringLiteral("agent:") + id, {});
    } else if (kind == QLatin1String("team")) {
        open_section(QStringLiteral("team:") + id, {});
    } else if (kind == QLatin1String("harness")) {
        open_section(QStringLiteral("harness:") + id, {});
    } else if (kind == QLatin1String("run")) {
        open_section(QString::fromLatin1(sec::kRuns), id);
        open_run_key(id, false); // the run list may not hold it yet (agentic list still loading)
    } else if (kind == QLatin1String("action")) {
        if (id == QLatin1String("new_agent"))
            on_bar_action(QStringLiteral("new_agent"));
        else if (id == QLatin1String("templates"))
            open_section(QStringLiteral("templates"), {});
        else if (id == QLatin1String("teams"))
            set_section(QString::fromLatin1(sec::kTeams));
        else if (id == QLatin1String("runs"))
            set_section(QString::fromLatin1(sec::kRuns));
        else if (id == QLatin1String("agent_connect"))
            open_agent_connect();
        else if (id == QLatin1String("retry_discovery")) {
            rail_->set_discovery_state(true, {});
            services::AgentService::instance().clear_cache();
            services::AgentService::instance().discover_agents();
        }
    }
    update_status();
}

void AgentConfigScreen::open_agent_connect() {
    // Settings › AI Agents (install, sign in, CHECK CONNECTIONS) — linked, not duplicated.
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                 {{QStringLiteral("screen_id"), QStringLiteral("settings")},
                                  {QStringLiteral("section"), QStringLiteral("ai_agents")}});
}

// ── Status bar ───────────────────────────────────────────────────────────────

void AgentConfigScreen::update_status() {
    if (!status_bar_)
        return;
    using SB = ui::ScreenStatusBar;
    // Selection + model: the inspector's run, else the agent in the editor, else the section.
    QString selection = section_title(section_);
    QString model;
    QString cost;
    agent_runs::Engine engine{}; // read only when split_run_key() filled it (C4701 under /GL)
    QString id;
    const bool run_shown = inspector_host_ && !inspector_host_->isHidden() && inspector_ &&
                           inspector_->currentWidget() != inspector_empty_ &&
                           agent_runs::split_run_key(selected_run_key_, &engine, &id);
    if (run_shown && engine == agent_runs::Engine::Harness) {
        if (const ca::CodingAgentRun* r = ca::CodingAgentService::instance().find_run(id)) {
            selection = tr("RUN %1").arg(r->title.left(60));
            model = QStringLiteral("%1 · %2").arg(awb_harness_name(r->harness),
                                                  r->model.isEmpty() ? tr("CLI default") : r->model);
            if (r->cost_usd.has_value())
                cost = QStringLiteral("$%1").arg(*r->cost_usd, 0, 'f', 4);
            if (r->input_tokens > 0 || r->output_tokens > 0)
                cost += (cost.isEmpty() ? QString() : QStringLiteral(" · ")) +
                        tr("%1 / %2 tok").arg(awb_tokens(r->input_tokens), awb_tokens(r->output_tokens));
        }
    } else if (run_shown && engine == agent_runs::Engine::Chat) {
        if (const agent_tasks::TaskRun* r = agent_tasks::TaskRunService::instance().find_run(id)) {
            selection = tr("CHAT RUN %1").arg((r->title.isEmpty() ? r->prompt : r->title).left(60));
            model = r->engine.toUpper();
            if (r->credits_spent > 0)
                cost = tr("%1 credits").arg(r->credits_spent, 0, 'f', 2);
        }
    } else if (run_shown && engine == agent_runs::Engine::Agentic) {
        selection = tr("AGENTIC TASK %1").arg(id.left(8));
        model = tr("local agentic runner");
    } else if (section_ == QLatin1String(sec::kAgents) && agents_panel_) {
        const QString agent_id = agents_panel_->selected_agent_id();
        for (const services::AgentInfo& a : services::AgentService::instance().cached_agents()) {
            if (a.id != agent_id)
                continue;
            selection = tr("AGENT %1").arg(a.name);
            // The config's model: a plain id, or an object naming it.
            const QJsonValue mv = a.config.value(QStringLiteral("model"));
            QString m = mv.toString();
            if (mv.isObject()) {
                const QJsonObject mo = mv.toObject();
                m = mo.value(QStringLiteral("model_id")).toString();
                if (m.isEmpty())
                    m = mo.value(QStringLiteral("model")).toString();
            }
            model = a.provider.isEmpty() ? m : (m.isEmpty() ? a.provider : a.provider + QStringLiteral(" · ") + m);
            break;
        }
    }
    status_bar_->set_text(QStringLiteral("selection"), selection);
    status_bar_->set_text(QStringLiteral("model"), model);
    status_bar_->set_item_visible(QStringLiteral("model"), !model.isEmpty());
    status_bar_->set_text(QStringLiteral("cost"), cost);
    status_bar_->set_item_visible(QStringLiteral("cost"), !cost.isEmpty());
    status_bar_->set_text(QStringLiteral("agents"), tr("%1 agents").arg(last_agent_count_));

    // Run state across engines.
    auto& runs = ca::CodingAgentService::instance();
    auto& tasks = agent_tasks::TaskRunService::instance();
    const int running = runs.running_count() + tasks.active_count();
    const int queued = runs.queued_count();
    const int attention = tasks.attention_count();
    QString state = tr("%1 RUNNING · %2 QUEUED").arg(running).arg(queued);
    SB::Tone tone = running > 0 ? SB::Tone::Accent : SB::Tone::Dim;
    if (attention > 0) {
        state += QStringLiteral(" · ") + tr("%1 NEED YOU").arg(attention);
        tone = SB::Tone::Warn;
    }
    status_bar_->set_item(QStringLiteral("runs"), state, tone);

    auto& srv = mcp::McpHttpServer::instance();
    status_bar_->set_item(QStringLiteral("mcp"), srv.is_running() ? tr("MCP ● ON :%1").arg(srv.port()) : tr("MCP ○ OFF"),
                          srv.is_running() ? SB::Tone::Ok : SB::Tone::Dim);
    auto& registry = harness::HarnessRegistry::instance();
    int ready = 0;
    for (const QString& hid : registry.installed_ids())
        if (awb_harness_state(hid).tone == AwbHarnessTone::Ready)
            ++ready;
    status_bar_->set_item(QStringLiteral("harnesses"),
                          tr("%1/%2 CODING AGENTS READY").arg(ready).arg(registry.ids().size()),
                          ready > 0 ? SB::Tone::Ok : SB::Tone::Dim);
}

// ── Coding-agent run settings (the Agent Workbench's SETTINGS) ────────────────

void AgentConfigScreen::open_run_settings() {
    // Only what is about coding-agent runs themselves. Everything about harnesses
    // (paths, defaults, sign-in, API keys) is on HARNESSES / Settings › AI Agents.
    auto& svc = ca::CodingAgentService::instance();
    QDialog dlg(centre_); // under the "awbScreen" host: its object-name styles apply
    dlg.setObjectName(QStringLiteral("awbDialog"));
    dlg.setWindowTitle(tr("Coding-agent run settings"));
    dlg.setMinimumWidth(qMax(480, QFontMetrics(ui::fonts::font()).averageCharWidth() * 60));
    auto* v = new QVBoxLayout(&dlg);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int r = 0;

    auto* conc_lbl = new QLabel(tr("MAX CONCURRENT RUNS"), &dlg);
    conc_lbl->setObjectName(QStringLiteral("awbLabel"));
    auto* conc_spin = new QSpinBox(&dlg);
    conc_spin->setRange(1, ca::CodingAgentService::kMaxConcurrentLimit);
    conc_spin->setValue(svc.max_concurrent());
    auto* conc_hint = new QLabel(tr("Extra runs wait in a queue."), &dlg);
    conc_hint->setObjectName(QStringLiteral("awbHint"));
    grid->addWidget(conc_lbl, r, 0);
    grid->addWidget(conc_spin, r, 1, Qt::AlignLeft);
    grid->addWidget(conc_hint, r, 2);
    ++r;

    auto* notify = new QCheckBox(tr("Desktop notification when a run finishes while this screen is not in view"), &dlg);
    notify->setChecked(svc.notify_on_finish());
    grid->addWidget(notify, r, 0, 1, 3);
    ++r;

    auto* ws_lbl = new QLabel(tr("WORKSPACE"), &dlg);
    ws_lbl->setObjectName(QStringLiteral("awbLabel"));
    auto* ws_path = new QLabel(QDir::toNativeSeparators(ca::CodingAgentService::workspace_root()), &dlg);
    ws_path->setObjectName(QStringLiteral("awbHint"));
    ws_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ws_path->setWordWrap(true);
    auto* ws_open = new QPushButton(tr("OPEN"), &dlg);
    ws_open->setObjectName(QStringLiteral("awbBtn"));
    connect(ws_open, &QPushButton::clicked, &dlg, [this]() {
        // Created (if needed) on the service's file thread, then opened.
        const QString root = ca::CodingAgentService::workspace_root();
        ca::CodingAgentService::instance().ensure_folder(root, this, [root](bool ok, const QString&) {
            if (ok)
                QDesktopServices::openUrl(QUrl::fromLocalFile(root));
        });
    });
    grid->addWidget(ws_lbl, r, 0);
    grid->addWidget(ws_path, r, 1);
    grid->addWidget(ws_open, r, 2);
    grid->setColumnStretch(1, 1);
    v->addLayout(grid);

    auto* note = new QLabel(tr("Harness paths, default models and efforts, sign-in and API keys: the HARNESSES tab "
                               "(also in Settings › AI Agents)."),
                            &dlg);
    note->setObjectName(QStringLiteral("awbHint"));
    note->setWordWrap(true);
    v->addWidget(note);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton(tr("CANCEL"), &dlg);
    cancel->setObjectName(QStringLiteral("awbBtn"));
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    auto* save = new QPushButton(tr("SAVE"), &dlg);
    save->setObjectName(QStringLiteral("awbPrimaryBtn"));
    connect(save, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;
    svc.set_max_concurrent(conc_spin->value());
    svc.set_notify_on_finish(notify->isChecked());
    update_status();
}

// ── Carry-over of the old Workbench screen state ─────────────────────────────

void AgentConfigScreen::carry_workbench_state_once() {
    // The Agent Workbench saved its composer (harness, model, effort, mode, tools,
    // limits) and the run in view under its own state key. Carried into this
    // screen's state when this one has no composer state yet — so it happens once:
    // the merged state written afterwards holds a "run" part and stops it. The
    // old row is left in place (read-only for us; nothing reads it any more).
    const QVariantMap wb = fincept::ScreenStateManager::instance().load_direct(QStringLiteral("agent_workbench"), 1);
    const QVariantMap merged = save_state();
    if (!sec::should_carry_workbench_state(false, wb, merged))
        return;
    QVariantMap run = sec::run_part_of_state(wb);
    const QString run_id = run.take(QStringLiteral("run_id")).toString();
    if (!run.isEmpty()) {
        if (composer_)
            composer_->restore_state(run);
        else
            pending_run_.insert(QStringLiteral("composer"), run);
    }
    if (!run_id.isEmpty() && selected_run_key_.isEmpty()) {
        if (run_view_)
            run_view_->restore_state({{QStringLiteral("run_id"), run_id}});
        else
            pending_run_.insert(QStringLiteral("run_id"), run_id);
    }
    // Verify the copy before relying on it: the merged state must now carry it.
    const QVariantMap check = sec::run_part_of_state(save_state());
    const bool ok = run.isEmpty() || !check.isEmpty();
    LOG_INFO("AgentConfig", QString("Workbench screen state carried over composer_keys=%1 run=%2 verified=%3")
                                .arg(run.size())
                                .arg(run_id.isEmpty() ? 0 : 1)
                                .arg(ok ? 1 : 0));
    if (ok)
        fincept::ScreenStateManager::instance().notify_changed(this);
}

// ── Stylesheet ───────────────────────────────────────────────────────────────

QString AgentConfigScreen::screen_qss() {
    using namespace ui::colors;
    // Object-name rules of the run / harness panels (moved from the Agent
    // Workbench) under hosts named "awbScreen", plus the section tabs and the
    // splitter. Tokens (@name) take the live theme's values; ThemeManager rewrites
    // them in place on a theme or font change, so it is set once.
    QString qss = QStringLiteral(R"QSS(
#awbScreen { background: @bg_base; }
#awbScreen QWidget#awbComposer, #awbScreen QWidget#awbTranscriptPane, #awbScreen QWidget#awbPage { background: @bg_base; }
QTabBar#agentSectionTabs { background: @bg_raised; border-bottom: 1px solid @border_dim; }
QTabBar#agentSectionTabs::tab { background: transparent; color: @text_secondary; padding: 6px 14px; border: none; border-bottom: 2px solid transparent; font-weight: 700; letter-spacing: 1px; }
QTabBar#agentSectionTabs::tab:hover { color: @text_primary; }
QTabBar#agentSectionTabs::tab:selected { color: @amber; border-bottom: 2px solid @amber; }
#awbSection { color: @amber; font-weight: 700; letter-spacing: 2px; background: transparent; }
#awbLabel { color: @text_secondary; font-weight: 700; letter-spacing: 1px; background: transparent; }
#awbHint { color: @text_tertiary; background: transparent; }
#awbRunTitle { color: @text_primary; font-weight: 700; background: transparent; }
#awbPlaceholder { color: @text_tertiary; background: @bg_base; border: 1px solid @border_dim; padding: 24px; }
#awbErrorBanner { color: @danger; background: @danger_bg; border: 1px solid @danger; padding: 6px 8px; }
#awbScreen QPushButton#awbTabBtn { background: transparent; color: @text_secondary; border: 1px solid @border_dim; padding: 5px 14px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbTabBtn:hover { color: @text_primary; border-color: @border_bright; }
#awbScreen QPushButton#awbTabBtn:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbTabBtn[active="true"] { background: @amber; color: @text_on_accent; border-color: @amber; }
#awbScreen QLineEdit, #awbScreen QPlainTextEdit, #awbScreen QTextBrowser, #awbScreen QComboBox, #awbScreen QSpinBox, #awbScreen QDoubleSpinBox, #awbScreen QTimeEdit { background: @bg_surface; color: @text_primary; border: 1px solid @border_med; padding: 3px 6px; selection-background-color: @amber_dim; selection-color: @text_on_accent; }
#awbScreen QLineEdit:focus, #awbScreen QPlainTextEdit:focus, #awbScreen QComboBox:focus, #awbScreen QSpinBox:focus, #awbScreen QDoubleSpinBox:focus, #awbScreen QTimeEdit:focus { border-color: @amber; }
#awbScreen QLineEdit:disabled, #awbScreen QComboBox:disabled, #awbScreen QDoubleSpinBox:disabled, #awbScreen QSpinBox:disabled, #awbScreen QTimeEdit:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QComboBox QAbstractItemView { background: @bg_surface; color: @text_primary; selection-background-color: @accent_bg; selection-color: @amber; border: 1px solid @border_med; }
#awbScreen QLineEdit#awbSnippet, #awbScreen QPlainTextEdit#awbSnippet { color: @cyan; background: @bg_base; }
#awbScreen QPushButton#awbBtn { background: @bg_raised; color: @text_primary; border: 1px solid @border_med; padding: 4px 12px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbBtn:hover { color: @amber; border-color: @amber; }
#awbScreen QPushButton#awbBtn:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbPrimaryBtn { background: @amber; color: @text_on_accent; border: 1px solid @amber; padding: 6px 18px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbPrimaryBtn:hover { background: @amber_dim; border-color: @amber_dim; }
#awbScreen QPushButton#awbPrimaryBtn:disabled { background: @bg_raised; color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbDangerBtn { background: transparent; color: @danger; border: 1px solid @danger; padding: 4px 12px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbDangerBtn:hover { background: @danger_bg; }
#awbScreen QPushButton#awbDangerBtn:disabled { color: @text_dim; border-color: @border_dim; }
QLabel#awbPill { color: @text_secondary; border: 1px solid @border_med; padding: 2px 10px; font-weight: 700; letter-spacing: 1px; background: transparent; }
QLabel#awbPill[status="queued"] { color: @info; border-color: @info; }
QLabel#awbPill[status="running"] { color: @amber; border-color: @amber; }
QLabel#awbPill[status="succeeded"] { color: @success; border-color: @success; }
QLabel#awbPill[status="failed"], QLabel#awbPill[status="interrupted"] { color: @danger; border-color: @danger; }
QLabel#awbPill[status="cancelled"] { color: @text_tertiary; border-color: @border_med; }
#awbScreen QCheckBox, #awbScreen QRadioButton { color: @text_primary; background: transparent; spacing: 6px; }
#awbScreen QCheckBox:disabled, #awbScreen QRadioButton:disabled { color: @text_dim; }
#awbScreen QTableView { background: @bg_base; alternate-background-color: @row_alt; color: @text_primary; border: 1px solid @border_dim; gridline-color: @border_dim; selection-background-color: @bg_hover; selection-color: @text_primary; }
#awbScreen QHeaderView::section { background: @bg_raised; color: @text_secondary; border: none; border-right: 1px solid @border_dim; border-bottom: 1px solid @border_dim; padding: 4px 6px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QListView#awbTranscript { background: @bg_base; border: 1px solid @border_dim; outline: none; }
#awbScreen QListWidget { background: @bg_surface; color: @text_primary; border: 1px solid @border_dim; outline: none; }
#awbScreen QListWidget::item { padding: 6px 8px; border-bottom: 1px solid @border_dim; }
#awbScreen QListWidget::item:selected { background: @accent_bg; color: @amber; }
#awbScreen QScrollArea { background: transparent; border: none; }
QDialog#awbDialog { background: @bg_surface; }
)QSS");
    std::vector<std::pair<QString, QString>> tokens = {
        {QStringLiteral("@bg_base"), BG_BASE()},
        {QStringLiteral("@bg_surface"), BG_SURFACE()},
        {QStringLiteral("@bg_raised"), BG_RAISED()},
        {QStringLiteral("@bg_hover"), BG_HOVER()},
        {QStringLiteral("@border_dim"), BORDER_DIM()},
        {QStringLiteral("@border_med"), BORDER_MED()},
        {QStringLiteral("@border_bright"), BORDER_BRIGHT()},
        {QStringLiteral("@text_primary"), TEXT_PRIMARY()},
        {QStringLiteral("@text_secondary"), TEXT_SECONDARY()},
        {QStringLiteral("@text_tertiary"), TEXT_TERTIARY()},
        {QStringLiteral("@text_dim"), TEXT_DIM()},
        {QStringLiteral("@text_on_accent"), TEXT_ON_ACCENT()},
        {QStringLiteral("@amber_dim"), AMBER_DIM()},
        {QStringLiteral("@amber"), AMBER()},
        {QStringLiteral("@accent_bg"), ACCENT_BG()},
        {QStringLiteral("@success"), SUCCESS()},
        {QStringLiteral("@danger_bg"), tint(DANGER())},
        {QStringLiteral("@danger"), DANGER()},
        {QStringLiteral("@info"), INFO()},
        {QStringLiteral("@cyan"), CYAN()},
        {QStringLiteral("@row_alt"), ROW_ALT()},
    };
    // Longest token first so "@amber" never eats the prefix of "@amber_dim".
    std::sort(tokens.begin(), tokens.end(),
              [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
    for (const auto& [key, value] : tokens)
        qss.replace(key, value);
    // The run title and the section titles one step above the base size.
    qss += QStringLiteral("#awbRunTitle { font-size:%1px; }").arg(ui::fonts::px(1));
    return qss;
}

} // namespace fincept::screens
