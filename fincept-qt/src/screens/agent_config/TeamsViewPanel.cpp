// src/screens/agent_config/TeamsViewPanel.cpp
#include "screens/agent_config/TeamsViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
static const QString kIn =
    QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:11px;")
        .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_MED);
} // namespace

namespace fincept::screens {

static const QMap<QString, QString> kModeDescriptions = {
    {"coordinate", "Agents coordinate through a leader who delegates and synthesizes results."},
    {"route", "Queries are routed to the most appropriate agent based on intent."},
    {"collaborate", "All agents work on the query simultaneously, results are merged."},
};

TeamsViewPanel::TeamsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("TeamsViewPanel");
    build_ui();
    setup_connections();
}

void TeamsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM));
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
    auto* p = new QWidget;
    p->setMinimumWidth(200);
    p->setMaximumWidth(280);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    auto* t = new QLabel("TEAM");
    t->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hdr->addWidget(t);
    team_count_ = new QLabel("0");
    team_count_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                   .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    hdr->addWidget(team_count_);
    hdr->addStretch();
    vl->addLayout(hdr);

    // Mode
    auto* ml = new QLabel("Mode:");
    ml->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(ml);
    mode_combo_ = new QComboBox;
    mode_combo_->addItems({"coordinate", "route", "collaborate"});
    mode_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    vl->addWidget(mode_combo_);

    mode_desc_label_ = new QLabel(kModeDescriptions["coordinate"]);
    mode_desc_label_->setWordWrap(true);
    mode_desc_label_->setStyleSheet(
        QString("color:%1;font-size:10px;font-style:italic;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(mode_desc_label_);

    // Leader selector
    auto* ll = new QLabel("Leader:");
    ll->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(ll);
    leader_combo_ = new QComboBox;
    leader_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    vl->addWidget(leader_combo_);

    // Show member responses
    show_responses_check_ = new QCheckBox("Show member responses");
    show_responses_check_->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;}").arg(ui::colors::TEXT_PRIMARY));
    vl->addWidget(show_responses_check_);

    // Team list
    team_list_ = new QListWidget;
    team_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                                      "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                      "QListWidget::item:selected{background:%4;}"
                                      "QListWidget::item:hover{background:%5;}")
                                  .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                       ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(team_list_, 1);

    auto* rb = new QPushButton("REMOVE");
    rb->setCursor(Qt::PointingHandCursor);
    rb->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:5px;"
                              "font-size:10px;font-weight:600;}QPushButton:hover{background:%2;}")
                          .arg(ui::colors::NEGATIVE, ui::colors::BG_HOVER));
    connect(rb, &QPushButton::clicked, this, [this]() {
        int row = team_list_->currentRow();
        if (row >= 0)
            remove_from_team(row);
    });
    vl->addWidget(rb);
    return p;
}

QWidget* TeamsViewPanel::build_agents_panel() {
    auto* p = new QWidget;
    p->setMinimumWidth(220);
    p->setMaximumWidth(320);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    auto* t = new QLabel("AVAILABLE AGENTS");
    t->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    hdr->addWidget(t);
    available_count_ = new QLabel("0");
    available_count_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                        .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    hdr->addWidget(available_count_);
    hdr->addStretch();
    vl->addLayout(hdr);

    available_list_ = new QListWidget;
    available_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                                           "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                           "QListWidget::item:selected{background:%4;}"
                                           "QListWidget::item:hover{background:%5;}")
                                       .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                            ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(available_list_, 1);

    auto* ab = new QPushButton("ADD TO TEAM");
    ab->setCursor(Qt::PointingHandCursor);
    ab->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:6px;"
                              "font-size:10px;font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}")
                          .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    connect(ab, &QPushButton::clicked, this, [this]() {
        int row = available_list_->currentRow();
        if (row >= 0 && row < all_agents_.size())
            add_to_team(all_agents_[row]);
    });
    vl->addWidget(ab);
    return p;
}

QWidget* TeamsViewPanel::build_execution_panel() {
    auto* p = new QWidget;
    p->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // LLM override section
    auto* llm_hdr = new QLabel("TEAM LLM OVERRIDE");
    llm_hdr->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(llm_hdr);
    auto* lr = new QHBoxLayout;
    auto* pl = new QLabel("Provider:");
    pl->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    lr->addWidget(pl);
    team_provider_combo_ = new QComboBox;
    team_provider_combo_->addItems({"(default)", "openai", "anthropic", "google", "groq", "deepseek", "ollama"});
    team_provider_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    lr->addWidget(team_provider_combo_);
    auto* mdl = new QLabel("Model:");
    mdl->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    lr->addWidget(mdl);
    team_model_edit_ = new QLineEdit;
    team_model_edit_->setPlaceholderText("(use agent default)");
    team_model_edit_->setStyleSheet(kIn);
    lr->addWidget(team_model_edit_, 1);
    vl->addLayout(lr);

    // Query
    auto* qh = new QLabel("TEAM QUERY");
    qh->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;").arg(ui::colors::AMBER));
    vl->addWidget(qh);
    query_input_ = new QPlainTextEdit;
    query_input_->setPlaceholderText("Enter a query for the team...");
    query_input_->setMaximumHeight(80);
    query_input_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    vl->addWidget(query_input_);

    run_btn_ = new QPushButton("RUN TEAM");
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                                    "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE, ui::colors::BG_RAISED,
                                     ui::colors::TEXT_TERTIARY));
    vl->addWidget(run_btn_);

    exec_status_ = new QLabel;
    exec_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(exec_status_);

    auto* lh = new QLabel("EXECUTION LOG");
    lh->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                          .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(lh);
    log_display_ = new QTextEdit;
    log_display_->setReadOnly(true);
    log_display_->setMaximumHeight(120);
    log_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:11px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
    vl->addWidget(log_display_);

    auto* rh = new QLabel("RESULT");
    rh->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                          .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(rh);
    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BORDER_BRIGHT));
    vl->addWidget(result_display_, 1);
    return p;
}

void TeamsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                all_agents_ = agents;
                populate_available_agents(agents);
            });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (!executing_)
            return;
        executing_ = false;
        run_btn_->setEnabled(true);
        run_btn_->setText("RUN TEAM");
        if (r.success) {
            result_display_->setPlainText(r.response);
            exec_status_->setText(QString("Completed in %1ms").arg(r.execution_time_ms));
            exec_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
            log_display_->append(QString("[DONE] Team completed (%1ms)").arg(r.execution_time_ms));
        } else {
            result_display_->setPlainText("Error: " + r.error);
            exec_status_->setText("FAILED");
            exec_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
            log_display_->append("[ERROR] " + r.error);
        }
    });
    connect(run_btn_, &QPushButton::clicked, this, &TeamsViewPanel::run_team);
    connect(available_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        int row = available_list_->currentRow();
        if (row >= 0 && row < all_agents_.size())
            add_to_team(all_agents_[row]);
    });
    connect(mode_combo_, &QComboBox::currentTextChanged, this,
            [this](const QString& mode) { mode_desc_label_->setText(kModeDescriptions.value(mode, "")); });
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
}

void TeamsViewPanel::run_team() {
    QString q = query_input_->toPlainText().trimmed();
    if (q.isEmpty() || team_members_.isEmpty() || executing_)
        return;
    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText("RUNNING...");
    result_display_->clear();
    exec_status_->setText("Executing...");
    exec_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER));
    log_display_->append(QString("[START] Running team (%1 members, mode: %2)")
                             .arg(team_members_.size())
                             .arg(mode_combo_->currentText()));

    QJsonObject tc;
    tc["name"] = "Custom Team";
    tc["mode"] = mode_combo_->currentText();
    tc["show_members_responses"] = show_responses_check_->isChecked();
    tc["leader_index"] = leader_combo_->currentIndex();

    // Team LLM override
    QString tp = team_provider_combo_->currentText();
    if (tp != "(default)" && !tp.isEmpty()) {
        QJsonObject model;
        model["provider"] = tp;
        if (!team_model_edit_->text().trimmed().isEmpty())
            model["model_id"] = team_model_edit_->text().trimmed();
        tc["model"] = model;
    }

    QJsonArray members;
    for (const auto& m : team_members_) {
        QJsonObject member;
        member["name"] = m.name;
        member["role"] = m.description;
        QJsonObject mc = m.config.contains("model") ? m.config["model"].toObject()
                                                    : QJsonObject{{"provider", "openai"}, {"model_id", "gpt-4o"}};
        member["model"] = mc;
        if (m.config.contains("tools"))
            member["tools"] = m.config["tools"];
        if (m.config.contains("instructions"))
            member["instructions"] = m.config["instructions"];
        members.append(member);
        log_display_->append(QString("  [MEMBER] %1").arg(m.name));
    }
    tc["members"] = members;
    services::AgentService::instance().run_team(q, tc);
}

void TeamsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        if (all_agents_.isEmpty())
            services::AgentService::instance().discover_agents();
    }
}

} // namespace fincept::screens
