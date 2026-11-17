// src/screens/agent_config/TeamsViewPanel.cpp
#include "screens/agent_config/TeamsViewPanel.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentProfileRows.h"
#include "screens/agent_config/AgentResultFormat.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {
QString in_ss() {
    return QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:" + fincept::ui::fonts::px(-3) + "px;")
        .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_MED());
}

// Default stylesheet of the result document (rendered markdown / HTML).
QString result_doc_css() {
    namespace colors = fincept::ui::colors;
    return QString("body { color: %1; background: %2; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                   "h1, h2, h3, h4 { color: %3; margin: 8px 0 4px 0; }"
                   "h1 { font-size: " + fincept::ui::fonts::px(2) + "px; border-bottom: 1px solid %4; padding-bottom: 4px; }"
                   "h2 { font-size: " + fincept::ui::fonts::px(0) + "px; }"
                   "h3 { font-size: " + fincept::ui::fonts::px(-1) + "px; color: %5; }"
                   "code { background: %6; color: %7; padding: 1px 4px; border-radius: 3px; }"
                   "pre  { background: %6; color: %7; padding: 8px; border-radius: 4px; }"
                   "blockquote { border-left: 3px solid %3; margin: 4px 0; padding-left: 10px; color: %8; }"
                   "a { color: %5; } ul, ol { margin: 4px 0; padding-left: 20px; }"
                   "li { margin: 2px 0; } strong { color: %1; }"
                   "hr { border: none; border-top: 1px solid %4; }")
        .arg(colors::TEXT_PRIMARY(), colors::BG_BASE(), colors::AMBER(), colors::BORDER_DIM(), colors::CYAN(),
             colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY());
}
} // namespace

namespace fincept::screens {

namespace {
// Status-line styling in one place (was seven identical inline setStyleSheet pairs).
void set_exec_status(QLabel* lbl, const QString& text, const QString& color) {
    lbl->setText(text);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(color));
}
} // namespace

QString TeamsViewPanel::mode_description(const QString& mode) {
    if (mode == QLatin1String("coordinate"))
        return tr("Agents coordinate through a leader who delegates and synthesizes results.");
    if (mode == QLatin1String("route"))
        return tr("Queries are routed to the most appropriate agent based on intent.");
    if (mode == QLatin1String("collaborate"))
        return tr("All agents work on the query simultaneously, results are merged.");
    return {};
}

TeamsViewPanel::TeamsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("TeamsViewPanel");
    build_ui();
    setup_connections();
    // The result document keeps its own stylesheet, built from the tokens.
    // Rendered answers carry the theme's colours in their HTML: re-render from the source.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        result_display_->document()->setDefaultStyleSheet(result_doc_css());
        agent_result::rerender(result_display_);
    });

    // Seed from cache immediately — handles case where agents_discovered was
    // already emitted before this panel was lazily constructed (same as AgentsViewPanel).
    auto cached = services::AgentService::instance().cached_agents();
    if (!cached.isEmpty()) {
        all_agents_ = cached;
        populate_available_agents(all_agents_);
    }
    update_team_state(); // empty team: the hint + LOAD TEAM, RUN TEAM disabled
}

void TeamsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));
    sp->addWidget(build_team_panel());
    sp->addWidget(build_agents_panel());
    sp->addWidget(build_execution_panel());
    sp->setSizes({220, 260, 400});
    sp->setStretchFactor(0, 0);
    sp->setStretchFactor(1, 0);
    sp->setStretchFactor(2, 1);
    root->addWidget(sp);
}

QWidget* TeamsViewPanel::build_team_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(200);
    p->setMaximumWidth(280);
    // Scoped to the panel: a selector-less sheet drew its border on every child.
    p->setObjectName(QStringLiteral("teamsTeamPanel"));
    p->setAttribute(Qt::WA_StyledBackground, true);
    p->setStyleSheet(QString("QWidget#teamsTeamPanel{background:%1;border-right:1px solid %2;}")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    team_title_ = new QLabel(tr("TEAM"));
    team_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hdr->addWidget(team_title_);
    team_count_ = new QLabel("0");
    team_count_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;padding:1px 6px;border-radius:2px;")
                                   .arg(ui::colors::CYAN(), ui::colors::BG_RAISED()));
    hdr->addWidget(team_count_);
    hdr->addStretch();
    vl->addLayout(hdr);

    // Mode
    mode_caption_ = new QLabel(tr("Mode:"));
    mode_caption_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(mode_caption_);
    mode_combo_ = new QComboBox;
    mode_combo_->addItems({"coordinate", "route", "collaborate"});
    mode_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(in_ss()));
    vl->addWidget(mode_combo_);

    mode_desc_label_ = new QLabel(mode_description("coordinate"));
    mode_desc_label_->setWordWrap(true);
    mode_desc_label_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-style:italic;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(mode_desc_label_);

    // Leader selector
    leader_caption_ = new QLabel(tr("Leader:"));
    leader_caption_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(leader_caption_);
    leader_combo_ = new QComboBox;
    leader_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(in_ss()));
    vl->addWidget(leader_combo_);

    // Show member responses
    show_responses_check_ = new QCheckBox(tr("Show member responses"));
    show_responses_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;}").arg(ui::colors::TEXT_PRIMARY()));
    vl->addWidget(show_responses_check_);

    // Team list, or what to do while it is empty.
    team_stack_ = new QStackedWidget(p);
    team_list_ = new QListWidget(team_stack_);
    team_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                                      "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                      "QListWidget::item:selected{background:%4;color:%6;}"
                                      "QListWidget::item:hover{background:%5;}")
                                  .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                                       ui::colors::AMBER_DIM(), ui::colors::BG_HOVER(), ui::colors::TEXT_ON_ACCENT()));
    team_stack_->addWidget(team_list_);
    team_empty_ = new ui::EmptyState(team_stack_);
    team_empty_->apply(ui::EmptyState::no_data_spec(
        tr("No team members yet"), tr("Add agents from the list on the right, or load a saved team."),
        tr("LOAD TEAM")));
    team_empty_->set_compact(true);
    team_empty_->set_alignment(Qt::AlignTop);
    connect(team_empty_, &ui::EmptyState::primary_clicked, this, &TeamsViewPanel::load_team);
    team_stack_->addWidget(team_empty_);
    team_stack_->setCurrentWidget(team_empty_);
    vl->addWidget(team_stack_, 1);

    // SAVE / LOAD persist the team as an AgentConfig row with category "team" - the
    // rows the TOOLS tab's "Team" assign target already lists (it was always empty,
    // nothing ever created one).
    auto* io_host = new QWidget(this);
    io_host->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:5px;"
                                   "font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;}QPushButton:hover{background:%3;}")
                               .arg(ui::colors::CYAN(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    auto* io_row = new QHBoxLayout(io_host);
    io_row->setContentsMargins(0, 0, 0, 0);
    io_row->setSpacing(6);
    team_save_btn_ = new QPushButton(tr("SAVE TEAM"));
    team_save_btn_->setCursor(Qt::PointingHandCursor);
    team_save_btn_->setToolTip(tr("Save this team (members, mode, leader) so it can be loaded later"));
    team_load_btn_ = new QPushButton(tr("LOAD TEAM"));
    team_load_btn_->setCursor(Qt::PointingHandCursor);
    team_load_btn_->setToolTip(tr("Replace the current team with a saved one"));
    io_row->addWidget(team_save_btn_);
    io_row->addWidget(team_load_btn_);
    connect(team_save_btn_, &QPushButton::clicked, this, &TeamsViewPanel::save_team);
    connect(team_load_btn_, &QPushButton::clicked, this, &TeamsViewPanel::load_team);
    vl->addWidget(io_host);

    team_remove_btn_ = new QPushButton(tr("REMOVE"));
    team_remove_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the team
    team_remove_btn_->setCursor(Qt::PointingHandCursor);
    team_remove_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:5px;"
                "font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::DANGER(), ui::colors::BG_HOVER()));
    connect(team_remove_btn_, &QPushButton::clicked, this, [this]() {
        int row = team_list_->currentRow();
        if (row >= 0)
            remove_from_team(row);
    });
    vl->addWidget(team_remove_btn_);
    return p;
}

QWidget* TeamsViewPanel::build_agents_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(220);
    p->setMaximumWidth(320);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    available_title_ = new QLabel(tr("AVAILABLE AGENTS"));
    available_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    hdr->addWidget(available_title_);
    available_count_ = new QLabel("0");
    available_count_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;padding:1px 6px;border-radius:2px;")
                                        .arg(ui::colors::CYAN(), ui::colors::BG_RAISED()));
    hdr->addWidget(available_count_);
    hdr->addStretch();
    vl->addLayout(hdr);

    available_list_ = new QListWidget;
    available_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                                           "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                           "QListWidget::item:selected{background:%4;color:%6;}"
                                           "QListWidget::item:hover{background:%5;}")
                                       .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                                            ui::colors::AMBER_DIM(), ui::colors::BG_HOVER(), ui::colors::TEXT_ON_ACCENT()));
    vl->addWidget(available_list_, 1);

    add_to_team_btn_ = new QPushButton(tr("ADD TO TEAM"));
    add_to_team_btn_->setCursor(Qt::PointingHandCursor);
    add_to_team_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:6px;"
                "font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM()));
    connect(add_to_team_btn_, &QPushButton::clicked, this, [this]() {
        int row = available_list_->currentRow();
        if (row >= 0 && row < all_agents_.size())
            add_to_team(all_agents_[row]);
    });
    vl->addWidget(add_to_team_btn_);
    return p;
}

QWidget* TeamsViewPanel::build_execution_panel() {
    auto* p = new QWidget(this);
    p->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Coordinator LLM profile picker
    coordinator_hdr_ = new QLabel(tr("COORDINATOR LLM PROFILE"));
    coordinator_hdr_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(coordinator_hdr_);

    team_profile_combo_ = new QComboBox;
    team_profile_combo_->setToolTip(
        tr("LLM profile for the team coordinator. Members use their own assigned profiles."));
    team_profile_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%4;selection-color:%5;}")
            .arg(in_ss(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(),
                 ui::colors::TEXT_ON_ACCENT()));
    vl->addWidget(team_profile_combo_);

    team_resolved_lbl_ = new QLabel;
    team_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(team_resolved_lbl_);

    // Query
    query_hdr_ = new QLabel(tr("TEAM QUERY"));
    query_hdr_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                                  .arg(ui::colors::AMBER()));
    vl->addWidget(query_hdr_);
    query_input_ = new QPlainTextEdit;
    query_input_->setPlaceholderText(tr("Enter a query for the team..."));
    query_input_->setMaximumHeight(80);
    query_input_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    vl->addWidget(query_input_);

    run_btn_ = new QPushButton(tr("RUN TEAM"));
    run_btn_->setProperty("fincept_agent_risk", "spend"); // agents: runs the team (model quota)
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                    "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(),
                                     ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    vl->addWidget(run_btn_);

    exec_status_ = new QLabel;
    exec_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(exec_status_);

    log_hdr_ = new QLabel(tr("EXECUTION LOG"));
    log_hdr_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                                .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(log_hdr_);
    log_display_ = new QTextEdit;
    log_display_->setReadOnly(true);
    log_display_->setMaximumHeight(120);
    log_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:" + fincept::ui::fonts::px(-3) + "px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    vl->addWidget(log_display_);

    result_hdr_ = new QLabel(tr("RESULT"));
    result_hdr_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(result_hdr_);
    // Result, or — before the first run — what a run does. A team run calls the
    // LLM once per member (and the coordinator): it never starts by itself.
    result_stack_ = new QStackedWidget(p);
    result_empty_ = new ui::EmptyState(result_stack_);
    result_empty_->apply(ui::EmptyState::preview_spec(
        tr("Run the team"),
        tr("Sends the query above to the team in the selected mode; the members' work and the final answer appear "
           "here, the steps in the execution log."),
        tr("RUN TEAM"), tr("LLM tokens for every member and the coordinator — billed to your providers"),
        ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Text));
    result_empty_->set_compact(true);
    connect(result_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (query_input_->toPlainText().trimmed().isEmpty()) {
            query_input_->setFocus();
            set_exec_status(exec_status_, tr("Type a query for the team first"), ui::colors::WARNING());
            return;
        }
        run_team();
    });
    result_stack_->addWidget(result_empty_);
    result_display_ = new QTextEdit(result_stack_);
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_BRIGHT()));
    result_display_->document()->setDefaultStyleSheet(result_doc_css());
    result_stack_->addWidget(result_display_);
    result_stack_->setCurrentWidget(result_empty_);
    vl->addWidget(result_stack_, 1);
    return p;
}

void TeamsViewPanel::update_team_state() {
    const bool has_members = !team_members_.isEmpty();
    if (team_stack_)
        team_stack_->setCurrentWidget(has_members ? static_cast<QWidget*>(team_list_) : team_empty_);
    if (result_empty_)
        result_empty_->set_primary_enabled(has_members);
    if (run_btn_ && !executing_)
        run_btn_->setEnabled(has_members);
}

void TeamsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                all_agents_ = agents;
                populate_available_agents(agents);
            });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        // An idle panel (empty pending id) must not take results of other screens.
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN TEAM"));
        if (r.success) {
            agent_result::show_markdown(result_display_, r.response);
            set_exec_status(exec_status_, tr("Completed in %1").arg(agent_result::duration(r.execution_time_ms)),
                            ui::colors::SUCCESS());
            log_display_->append(QString("[DONE] Team completed (%1)").arg(agent_result::duration(r.execution_time_ms)));
        } else {
            agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            set_exec_status(exec_status_, tr("FAILED"), ui::colors::DANGER());
            log_display_->append("[ERROR] " + r.error);
        }
    });

    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_)
                    return;
                exec_status_->setText(status);
                log_display_->append("[THINK] " + status);
            });

    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_)
                    return;
                QTextCursor cursor = result_display_->textCursor();
                cursor.movePosition(QTextCursor::End);
                result_display_->setTextCursor(cursor);
                result_display_->insertPlainText(token);
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN TEAM"));
        if (r.success) {
            agent_result::show_markdown(result_display_, r.response);
            set_exec_status(exec_status_, tr("Completed in %1").arg(agent_result::duration(r.execution_time_ms)),
                            ui::colors::SUCCESS());
            log_display_->append(QString("[DONE] Team completed (%1)").arg(agent_result::duration(r.execution_time_ms)));
        } else {
            // STOP pressed: keep whatever streamed in, just mark the run as stopped.
            const bool stopped = r.error == QLatin1String(services::AgentService::kCancelledError);
            if (!stopped)
                agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            set_exec_status(exec_status_, stopped ? tr("STOPPED") : tr("FAILED"), ui::colors::DANGER());
            log_display_->append((stopped ? QStringLiteral("[STOP] ") : QStringLiteral("[ERROR] ")) + r.error);
        }
    });
    // Profile combo for coordinator
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &TeamsViewPanel::load_team_profile_combo);
    connect(team_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TeamsViewPanel::refresh_team_llm_label);
    load_team_profile_combo();

    // No AgentService::error_occurred handler: run_team always ends in
    // agent_stream_done (crash, spawn failure and missing Python included).
    // error_occurred reports unrelated discovery / task / schedule / plan /
    // config calls, which used to flip a running team to ERROR and then drop
    // its real result.

    connect(run_btn_, &QPushButton::clicked, this, &TeamsViewPanel::run_team);
    connect(available_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        int row = available_list_->currentRow();
        if (row >= 0 && row < all_agents_.size())
            add_to_team(all_agents_[row]);
    });
    connect(mode_combo_, &QComboBox::currentTextChanged, this,
            [this](const QString& mode) { mode_desc_label_->setText(mode_description(mode)); });
}

void TeamsViewPanel::populate_available_agents(const QVector<services::AgentInfo>& agents) {
    available_list_->clear();
    for (const auto& a : agents) {
        auto* item = new QListWidgetItem(QString("%1 [%2]").arg(a.name, a.category));
        item->setToolTip(a.description);
        available_list_->addItem(item);
    }
    available_count_->setText(QString::number(agents.size()));
}

void TeamsViewPanel::add_agent_from_panel(const services::AgentInfo& agent) {
    add_to_team(agent);
}

void TeamsViewPanel::add_to_team(const services::AgentInfo& agent) {
    for (const auto& m : team_members_)
        if (m.id == agent.id)
            return;
    team_members_.append(agent);
    team_list_->addItem(QString("%1 [%2]").arg(agent.name, agent.category));
    team_count_->setText(QString::number(team_members_.size()));
    update_leader_combo();
    log_display_->append(QString("[+] Added %1").arg(agent.name));
}

void TeamsViewPanel::remove_from_team(int row) {
    if (row < 0 || row >= team_members_.size())
        return;
    QString name = team_members_[row].name;
    team_members_.removeAt(row);
    delete team_list_->takeItem(row);
    team_count_->setText(QString::number(team_members_.size()));
    update_leader_combo();
    log_display_->append(QString("[-] Removed %1").arg(name));
}

void TeamsViewPanel::update_leader_combo() {
    leader_combo_->clear();
    for (int i = 0; i < team_members_.size(); ++i)
        leader_combo_->addItem(QString("%1. %2").arg(i + 1).arg(team_members_[i].name));
    update_team_state(); // every membership change (add / remove / load) passes here
}

void TeamsViewPanel::load_team_profile_combo() {
    const QString prev_id = team_profile_combo_->currentData().toString();
    team_profile_combo_->blockSignals(true);
    team_profile_combo_->clear();
    team_profile_combo_->addItem(tr("Default (Global)"), QString{});

    // Usable profiles, then harness profiles as disabled rows (no id: never restored below).
    agent_profiles::add_profile_rows(team_profile_combo_, tr(" [default]"), QStringLiteral("team"));

    int restore = 0;
    if (!prev_id.isEmpty()) {
        for (int i = 1; i < team_profile_combo_->count(); ++i) {
            if (team_profile_combo_->itemData(i).toString() == prev_id) {
                restore = i;
                break;
            }
        }
    }
    team_profile_combo_->blockSignals(false);
    team_profile_combo_->setCurrentIndex(restore);
    refresh_team_llm_label();
}

void TeamsViewPanel::refresh_team_llm_label() {
    const QString profile_id = team_profile_combo_->currentData().toString();
    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        const auto pr2 = LlmProfileRepository::instance().list_profiles();
        const auto profiles2 = pr2.is_ok() ? pr2.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles2) {
            if (p.id == profile_id) {
                resolved.profile_id = p.id;
                resolved.profile_name = p.name;
                resolved.provider = p.provider;
                resolved.model_id = p.model_id;
                resolved.api_key = p.api_key;
                resolved.base_url = p.base_url;
                resolved.temperature = p.temperature;
                resolved.max_tokens = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
    }
    if (resolved.provider.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile("team_coordinator", {});

    if (!resolved.provider.isEmpty()) {
        QString text = resolved.provider.toUpper() + " / " + resolved.model_id;
        if (text.length() > 60)
            text = text.left(58) + "..";
        if (profile_id.isEmpty())
            text += tr(" (inherited)");
        team_resolved_lbl_->setText(text);
        team_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        team_resolved_lbl_->setText(tr("No provider — go to Settings > LLM Config"));
        team_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::DANGER()));
    }
}

void TeamsViewPanel::run_team() {
    // While the team is running the button doubles as STOP.
    if (executing_) {
        services::AgentService::instance().cancel_run(pending_request_id_);
        return;
    }
    QString q = query_input_->toPlainText().trimmed();
    if (q.isEmpty() || team_members_.isEmpty())
        return;
    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText(tr("RUNNING..."));
    agent_result::show_plain(result_display_, QString());
    result_stack_->setCurrentWidget(result_display_);
    set_exec_status(exec_status_, tr("Executing..."), ui::colors::AMBER());
    log_display_->append(QString("[START] Running team (%1 members, mode: %2)")
                             .arg(team_members_.size())
                             .arg(mode_combo_->currentText()));

    // Resolve coordinator profile
    const QString coord_profile_id = team_profile_combo_->currentData().toString();
    ResolvedLlmProfile coord;
    if (!coord_profile_id.isEmpty()) {
        const auto pr3 = LlmProfileRepository::instance().list_profiles();
        const auto profiles3 = pr3.is_ok() ? pr3.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles3) {
            if (p.id == coord_profile_id) {
                coord.profile_id = p.id;
                coord.provider = p.provider;
                coord.model_id = p.model_id;
                coord.api_key = p.api_key;
                coord.base_url = p.base_url;
                coord.temperature = p.temperature;
                coord.max_tokens = p.max_tokens;
                break;
            }
        }
    }
    if (coord.provider.isEmpty())
        coord = ai_chat::LlmService::instance().resolve_profile("team_coordinator", {});

    QJsonObject tc;
    tc["name"] = "Custom Team";
    tc["mode"] = mode_combo_->currentText();
    tc["show_members_responses"] = show_responses_check_->isChecked();
    tc["leader_index"] = leader_combo_->currentIndex();
    tc["model"] = ai_chat::LlmService::profile_to_json(coord);

    QJsonArray members;
    for (const auto& m : team_members_) {
        QJsonObject member;
        member["name"] = m.name;
        member["role"] = m.description;

        // Always reload from DB — in-memory AgentInfo.config is stale if the user
        // just saved tools/LLM assignments without triggering a full agent rediscovery.
        QJsonObject fresh_config = m.config; // fallback to in-memory
        auto db_result = AgentConfigRepository::instance().get(m.id);
        if (db_result.is_ok()) {
            QJsonDocument doc = QJsonDocument::fromJson(db_result.value().config_json.toUtf8());
            if (!doc.isNull())
                fresh_config = doc.object();
        }

        // Resolve per-member LLM profile via assignment chain, then override with
        // any explicit api_key stored in the agent's saved config (takes priority).
        const ResolvedLlmProfile member_profile = ai_chat::LlmService::instance().resolve_profile("agent", m.id);
        QJsonObject mc = ai_chat::LlmService::profile_to_json(member_profile);

        if (fresh_config.contains("model")) {
            QJsonObject saved = fresh_config["model"].toObject();
            if (!saved["api_key"].toString().isEmpty())
                mc = saved;
        }
        member["model"] = mc;

        // Use agent's own tools if set; otherwise fall back to TOOLS-tab selection
        if (fresh_config.contains("tools") && !fresh_config["tools"].toArray().isEmpty()) {
            member["tools"] = fresh_config["tools"];
        } else if (!selected_tools_.isEmpty()) {
            QJsonArray ta;
            for (const auto& t : selected_tools_)
                ta.append(t);
            member["tools"] = ta;
        }
        if (fresh_config.contains("instructions"))
            member["instructions"] = fresh_config["instructions"];
        members.append(member);
        log_display_->append(QString("  [MEMBER] %1").arg(m.name));
    }
    tc["members"] = members;
    pending_request_id_ = services::AgentService::instance().run_team(q, tc);
    // run_team streams, so it can be stopped — re-enable the button as STOP.
    run_btn_->setEnabled(true);
    run_btn_->setText(tr("STOP"));
}

void TeamsViewPanel::save_team() {
    if (team_members_.isEmpty()) {
        log_display_->append(tr("[!] Add at least one agent before saving the team"));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save Team"), tr("Team name:"), QLineEdit::Normal,
                                               saved_team_name_.isEmpty() ? tr("My Team") : saved_team_name_, &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;

    // Same name as the team that was loaded/saved last = overwrite it in place;
    // anything else is a new team.
    const bool overwrite = !saved_team_id_.isEmpty() && name == saved_team_name_;
    const QString id = overwrite ? saved_team_id_ : QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject cfg;
    if (overwrite) {
        // Keep what other tabs wrote into the row (e.g. tools assigned from TOOLS).
        const auto existing = AgentConfigRepository::instance().get(id);
        if (existing.is_ok())
            cfg = QJsonDocument::fromJson(existing.value().config_json.toUtf8()).object();
    }
    QJsonArray member_ids;
    for (const auto& m : team_members_)
        member_ids.append(m.id);
    cfg["members"] = member_ids;
    cfg["mode"] = mode_combo_->currentText();
    cfg["leader_index"] = leader_combo_->currentIndex();
    cfg["show_members_responses"] = show_responses_check_->isChecked();
    cfg["coordinator_profile_id"] = team_profile_combo_->currentData().toString();

    AgentConfig row;
    row.id = id;
    row.name = name;
    row.description = QStringLiteral("%1 team, %2 member(s)").arg(mode_combo_->currentText()).arg(team_members_.size());
    row.category = QStringLiteral("team");
    row.config_json = QString::fromUtf8(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(row);
    saved_team_id_ = id;
    saved_team_name_ = name;
    log_display_->append(QString("[SAVE] Team \"%1\" (%2 members)").arg(name).arg(team_members_.size()));
}

void TeamsViewPanel::load_team() {
    const auto teams = AgentConfigRepository::instance().list_by_category(QStringLiteral("team"));
    if (teams.is_err() || teams.value().isEmpty()) {
        log_display_->append(tr("[!] No saved teams yet - build a team and press SAVE TEAM"));
        return;
    }
    QStringList names;
    for (const auto& t : teams.value())
        names << t.name;
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, tr("Load Team"), tr("Saved teams:"), names, 0, false, &ok);
    if (!ok)
        return;
    const int idx = static_cast<int>(names.indexOf(pick));
    if (idx < 0)
        return;
    apply_team_row(teams.value()[idx]);
}

bool TeamsViewPanel::open_team(const QString& team_id) {
    if (team_id.isEmpty())
        return false;
    if (team_id == saved_team_id_)
        return true; // already loaded (unsaved edits stay)
    const auto row = AgentConfigRepository::instance().get(team_id);
    if (row.is_err() || row.value().category != QLatin1String("team"))
        return false;
    apply_team_row(row.value());
    return true;
}

void TeamsViewPanel::apply_team_row(const AgentConfig& row) {
    const QJsonObject cfg = QJsonDocument::fromJson(row.config_json.toUtf8()).object();

    // Replace the current team.
    team_members_.clear();
    team_list_->clear();
    int missing = 0;
    for (const auto& v : cfg.value("members").toArray()) {
        const QString agent_id = v.toString();
        bool found = false;
        for (const auto& a : all_agents_) {
            if (a.id == agent_id) {
                add_to_team(a);
                found = true;
                break;
            }
        }
        if (!found)
            ++missing;
    }
    update_leader_combo(); // also covers the all-members-missing case (clears the combo)
    team_count_->setText(QString::number(team_members_.size()));

    const int mode_idx = mode_combo_->findText(cfg.value("mode").toString());
    if (mode_idx >= 0)
        mode_combo_->setCurrentIndex(mode_idx);
    show_responses_check_->setChecked(cfg.value("show_members_responses").toBool());
    if (!team_members_.isEmpty())
        leader_combo_->setCurrentIndex(qBound(0, cfg.value("leader_index").toInt(), int(team_members_.size()) - 1));
    const int prof_idx = team_profile_combo_->findData(cfg.value("coordinator_profile_id").toString());
    team_profile_combo_->setCurrentIndex(prof_idx >= 0 ? prof_idx : 0);

    // Team-level tools (assigned from the TOOLS tab) back members that have none.
    QStringList tools;
    for (const auto& t : cfg.value("tools").toArray())
        tools << t.toString();
    selected_tools_ = tools;

    saved_team_id_ = row.id;
    saved_team_name_ = row.name;
    log_display_->append(QString("[LOAD] Team \"%1\" (%2 members)").arg(row.name).arg(team_members_.size()));
    if (missing > 0)
        log_display_->append(tr("[!] %1 saved member(s) no longer exist and were skipped").arg(missing));
}

void TeamsViewPanel::apply_tools_selection(const QStringList& tools) {
    selected_tools_ = tools;
    log_display_->append(QString("[TOOLS] %1 tool(s) selected from TOOLS tab").arg(tools.size()));
}

void TeamsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Trigger a fresh discover so the list stays current.
    // AgentService caches results with TTL — no Python spawn if cache is warm.
    services::AgentService::instance().discover_agents();
}

// ── Re-translation ───────────────────────────────────────────────────────────

void TeamsViewPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TeamsViewPanel::retranslateUi() {
    // Left: team panel.
    if (team_title_)
        team_title_->setText(tr("TEAM"));
    if (mode_caption_)
        mode_caption_->setText(tr("Mode:"));
    if (leader_caption_)
        leader_caption_->setText(tr("Leader:"));
    if (show_responses_check_)
        show_responses_check_->setText(tr("Show member responses"));
    if (team_remove_btn_)
        team_remove_btn_->setText(tr("REMOVE"));
    if (team_save_btn_) {
        team_save_btn_->setText(tr("SAVE TEAM"));
        team_save_btn_->setToolTip(tr("Save this team (members, mode, leader) so it can be loaded later"));
    }
    if (team_load_btn_) {
        team_load_btn_->setText(tr("LOAD TEAM"));
        team_load_btn_->setToolTip(tr("Replace the current team with a saved one"));
    }
    // Mode description tracks the selected mode code (combo items stay as codes).
    if (mode_desc_label_ && mode_combo_)
        mode_desc_label_->setText(mode_description(mode_combo_->currentText()));

    // Center: available agents.
    if (available_title_)
        available_title_->setText(tr("AVAILABLE AGENTS"));
    if (add_to_team_btn_)
        add_to_team_btn_->setText(tr("ADD TO TEAM"));

    // Right: execution panel section headers + input.
    if (coordinator_hdr_)
        coordinator_hdr_->setText(tr("COORDINATOR LLM PROFILE"));
    if (team_profile_combo_)
        team_profile_combo_->setToolTip(
            tr("LLM profile for the team coordinator. Members use their own assigned profiles."));
    if (query_hdr_)
        query_hdr_->setText(tr("TEAM QUERY"));
    if (query_input_)
        query_input_->setPlaceholderText(tr("Enter a query for the team..."));
    if (log_hdr_)
        log_hdr_->setText(tr("EXECUTION LOG"));
    if (result_hdr_)
        result_hdr_->setText(tr("RESULT"));
    if (run_btn_ && !executing_)
        run_btn_->setText(tr("RUN TEAM"));

    // exec_status_ / log_display_ hold live state. Refresh the resolved coordinator
    // label (re-derives provider/model from current config).
    refresh_team_llm_label();
}

} // namespace fincept::screens
