// src/screens/agent_config/AgentConfigScreen_Inspector.cpp
//
// The right-hand inspector of the agents screen: the selected run of any engine.
//   harness run  -> AgentRunPanel (transcript, tool cards, files, cost, follow-up)
//   agentic task -> AgenticTasksPanel in detail mode (plan, steps, budget, reply,
//                   pause / resume / cancel / delete)
//   Chat Mode    -> a summary with OPEN IN CHAT MODE (its transcript lives there)
//   none         -> what to do (never a blank pane)
#include "screens/agent_config/AgentConfigScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/agent_config/AgentRail.h"
#include "screens/agent_config/AgentRunPanel.h"
#include "screens/agent_config/AgentRunsPanel.h"
#include "screens/agent_config/AgentScreenSections.h"
#include "screens/agent_config/AgenticTasksPanel.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace sec = agent_sections;

void AgentConfigScreen::ensure_inspector() {
    if (inspector_empty_)
        return;
    inspector_empty_ = new ui::EmptyState(inspector_);
    inspector_empty_->set_compact(true);
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No run selected"),
        tr("Pick a run in RUNS or under ACTIVE RUNS on the left — its transcript, tool calls, cost and files show "
           "here. NEW RUN opens the coding-agent composer; nothing runs until you press RUN."),
        tr("OPEN RUNS"));
    spec.secondary = tr("NEW RUN");
    inspector_empty_->apply(spec);
    connect(inspector_empty_, &ui::EmptyState::primary_clicked, this,
            [this]() { set_section(QString::fromLatin1(sec::kRuns)); });
    connect(inspector_empty_, &ui::EmptyState::secondary_clicked, this,
            [this]() { set_section(QString::fromLatin1(sec::kRun)); });
    inspector_->addWidget(inspector_empty_);

    run_view_ = new AgentRunPanel(inspector_);
    connect(run_view_, &AgentRunPanel::run_started, this, [this](const QString& id) {
        const QString key = agent_runs::run_key(agent_runs::Engine::Harness, id);
        selected_run_key_ = key;
        if (runs_panel_)
            runs_panel_->select_run(key);
    });
    connect(run_view_, &AgentRunPanel::new_run_requested, this, [this]() {
        set_section(QString::fromLatin1(sec::kRun));
        open_section(QStringLiteral("templates"), {});
    });
    connect(run_view_, &AgentRunPanel::show_harnesses_requested, this,
            [this]() { set_section(QString::fromLatin1(sec::kHarnesses)); });
    connect(run_view_, &AgentRunPanel::notice, this, [this](const QString& text) {
        status_bar_->show_notice(text, {}, {}, 7000, ui::ScreenStatusBar::Tone::Ok);
    });
    connect(run_view_, &AgentRunPanel::run_shown, this, [this](const QString& id) {
        if (id.isEmpty() || inspector_->currentWidget() != run_view_)
            return;
        selected_run_key_ = agent_runs::run_key(agent_runs::Engine::Harness, id);
        rail_->set_current(QStringLiteral("run"), selected_run_key_);
        update_status();
    });
    inspector_->addWidget(run_view_);

    task_view_ = new AgenticTasksPanel(inspector_);
    task_view_->set_detail_only(true);
    connect(task_view_, &AgenticTasksPanel::task_deleted, this, [this](const QString&) {
        selected_run_key_.clear();
        inspector_->setCurrentWidget(inspector_empty_);
        if (runs_panel_)
            runs_panel_->request_agentic_refresh();
        update_status();
    });
    inspector_->addWidget(task_view_);

    chat_card_ = new QWidget(inspector_);
    chat_card_->setObjectName(QStringLiteral("awbPage"));
    auto* cv = new QVBoxLayout(chat_card_);
    cv->setContentsMargins(12, 12, 12, 12);
    cv->setSpacing(8);
    chat_title_ = new QLabel(chat_card_);
    chat_title_->setObjectName(QStringLiteral("awbRunTitle"));
    chat_title_->setWordWrap(true);
    cv->addWidget(chat_title_);
    chat_meta_ = new QLabel(chat_card_);
    chat_meta_->setObjectName(QStringLiteral("awbHint"));
    chat_meta_->setWordWrap(true);
    cv->addWidget(chat_meta_);
    chat_prompt_ = new QLabel(chat_card_);
    chat_prompt_->setObjectName(QStringLiteral("awbHint"));
    chat_prompt_->setWordWrap(true);
    chat_prompt_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cv->addWidget(chat_prompt_);
    chat_open_btn_ = new QPushButton(tr("OPEN IN CHAT MODE"), chat_card_);
    chat_open_btn_->setObjectName(QStringLiteral("awbPrimaryBtn"));
    chat_open_btn_->setCursor(Qt::PointingHandCursor);
    chat_open_btn_->setToolTip(tr("The run's transcript, plan and artefacts are in Chat Mode"));
    connect(chat_open_btn_, &QPushButton::clicked, this, [this]() {
        if (chat_run_id_.isEmpty())
            return;
        // Opening shows the run; it never sends anything.
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     {{QStringLiteral("screen_id"), QStringLiteral("chat_mode")},
                                      {QStringLiteral("run_id"), chat_run_id_}});
    });
    cv->addWidget(chat_open_btn_, 0, Qt::AlignLeft);
    cv->addStretch(1);
    inspector_->addWidget(chat_card_);

    inspector_->setCurrentWidget(inspector_empty_);
    if (pending_run_.contains(QStringLiteral("run_id"))) {
        run_view_->restore_state({{QStringLiteral("run_id"), pending_run_.take(QStringLiteral("run_id"))}});
    }
}

void AgentConfigScreen::set_inspector_visible(bool on) {
    if (on)
        ensure_inspector();
    if (inspector_host_->isHidden() == !on)
        return;
    inspector_host_->setVisible(on);
    if (on)
        fit_splitter();
}

void AgentConfigScreen::fit_splitter() {
    // Widths from the user's font: the rail a name and a chip, the inspector a
    // transcript line with its gutters.
    const QFontMetrics fm(ui::fonts::font());
    const int rail_w = qBound(fm.averageCharWidth() * 22, rail_->width(), fm.averageCharWidth() * 36);
    const int inspector_w = std::max(380, fm.averageCharWidth() * 46);
    QList<int> sizes = splitter_->sizes();
    if (sizes.size() != 3)
        return;
    const int total = sizes[0] + sizes[1] + sizes[2];
    if (total <= 0) {
        splitter_->setSizes({rail_w, 800, inspector_host_->isVisibleTo(this) ? inspector_w : 0});
        return;
    }
    const bool show_inspector = inspector_host_->isVisibleTo(this);
    sizes[0] = qMax(sizes[0], fm.averageCharWidth() * 20);
    if (show_inspector && sizes[2] < inspector_w)
        sizes[2] = inspector_w;
    sizes[1] = qMax(fm.averageCharWidth() * 30, total - sizes[0] - (show_inspector ? sizes[2] : 0));
    splitter_->setSizes(sizes);
}

void AgentConfigScreen::open_run_key(const QString& key, bool move_focus) {
    agent_runs::Engine engine;
    QString id;
    if (!agent_runs::split_run_key(key, &engine, &id))
        return;
    ensure_inspector();
    selected_run_key_ = agent_runs::run_key(engine, id);
    switch (engine) {
        case agent_runs::Engine::Harness:
            if (run_view_->current_run_id() != id)
                run_view_->show_run(id);
            inspector_->setCurrentWidget(run_view_);
            break;
        case agent_runs::Engine::Agentic: {
            if (!agentic_mode_enabled_)
                return;
            const QJsonObject row = runs_panel_ ? runs_panel_->runs_model()->agentic_json(id) : QJsonObject();
            if (task_view_->current_task_id() != id)
                task_view_->open_task(id, row);
            inspector_->setCurrentWidget(task_view_);
            break;
        }
        case agent_runs::Engine::Chat:
            show_chat_run(id);
            inspector_->setCurrentWidget(chat_card_);
            break;
    }
    if (!inspector_host_->isVisibleTo(this)) {
        inspector_pref_[group_] = true;
        command_bar_->set_action_checked(QStringLiteral("inspect"), true);
        set_inspector_visible(true);
    }
    if (move_focus)
        inspector_->currentWidget()->setFocus();
    rail_->set_current(QStringLiteral("run"), selected_run_key_);
    update_status();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AgentConfigScreen::show_chat_run(const QString& run_id) {
    chat_run_id_ = run_id;
    const agent_tasks::TaskRun* r = agent_tasks::TaskRunService::instance().find_run(run_id);
    if (!r) {
        chat_title_->setText(tr("This Chat Mode run is no longer stored."));
        chat_meta_->clear();
        chat_prompt_->clear();
        chat_open_btn_->setEnabled(false);
        return;
    }
    chat_open_btn_->setEnabled(true);
    chat_title_->setText(r->title.isEmpty() ? r->prompt.left(160) : r->title);
    QStringList meta;
    meta << (r->engine == QLatin1String(agent_tasks::kEngineLocal) ? tr("Chat Mode · local engine")
                                                                     : tr("Chat Mode · cloud"));
    meta << agent_tasks::run_status_id(r->status).toUpper();
    if (!r->mode.isEmpty())
        meta << r->mode;
    if (r->credits_spent > 0)
        meta << tr("%1 credits").arg(r->credits_spent, 0, 'f', 2);
    if (r->tokens_used > 0)
        meta << tr("%1 tokens").arg(r->tokens_used);
    if (r->created_at_ms > 0)
        meta << QDateTime::fromMSecsSinceEpoch(r->created_at_ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    chat_meta_->setText(meta.join(QStringLiteral("  ·  ")));
    chat_prompt_->setText(r->prompt.left(1500));
}

} // namespace fincept::screens
