// src/screens/agent_config/AgentsViewPanel.cpp
#include "screens/agent_config/AgentsViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

static QLabel* section_hdr(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding:6px 0 2px 0;")
                           .arg(ui::colors::AMBER));
    return lbl;
}

static const QString kInput = QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:12px;")
                                  .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED);

AgentsViewPanel::AgentsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentsViewPanel");
    build_ui();
    setup_connections();
}

void AgentsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM));
    splitter->addWidget(build_agent_list_panel());
    splitter->addWidget(build_config_panel());
    splitter->addWidget(build_query_panel());
    splitter->setSizes({220, 500, 350});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter);
}

// ── Left: agent list ─────────────────────────────────────────────────────────

QWidget* AgentsViewPanel::build_agent_list_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(200);
    panel->setMaximumWidth(300);
    panel->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    auto* title = new QLabel("AGENTS");
    title->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hdr->addWidget(title);
    list_count_label_ = new QLabel("0");
    list_count_label_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                         .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    hdr->addWidget(list_count_label_);
    hdr->addStretch();
    vl->addLayout(hdr);

    category_combo_ = new QComboBox;
    category_combo_->addItem("All Categories", "");
    category_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%4;}")
            .arg(kInput, ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::AMBER_DIM));
    vl->addWidget(category_combo_);

    agent_list_ = new QListWidget;
    agent_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                                       "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                       "QListWidget::item:selected{background:%4;}"
                                       "QListWidget::item:hover{background:%5;}")
                                   .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                        ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(agent_list_, 1);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:6px;"
                                       "font-size:10px;font-weight:600;letter-spacing:1px;}"
                                       "QPushButton:hover{background:%3;}")
                                   .arg(ui::colors::BG_RAISED, ui::colors::AMBER, ui::colors::AMBER_DIM));
    connect(refresh_btn, &QPushButton::clicked, this, []() {
        services::AgentService::instance().clear_cache();
        services::AgentService::instance().discover_agents();
    });
    vl->addWidget(refresh_btn);
    return panel;
}

// ── Center: config editor with JSON toggle ───────────────────────────────────

QWidget* AgentsViewPanel::build_config_panel() {
    auto* wrapper = new QWidget;
    auto* wl = new QVBoxLayout(wrapper);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->setSpacing(0);

    // Toggle bar
    auto* toggle_bar = new QWidget;
    toggle_bar->setFixedHeight(32);
    toggle_bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* tbl = new QHBoxLayout(toggle_bar);
    tbl->setContentsMargins(8, 0, 8, 0);

    json_toggle_btn_ = new QPushButton("JSON EDITOR");
    json_toggle_btn_->setCheckable(true);
    json_toggle_btn_->setCursor(Qt::PointingHandCursor);
    json_toggle_btn_->setStyleSheet(
        QString(
            "QPushButton{background:transparent;color:%1;border:none;font-size:10px;font-weight:600;padding:4px 8px;}"
            "QPushButton:checked{color:%2;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    tbl->addWidget(json_toggle_btn_);
    tbl->addStretch();

    add_team_btn_ = new QPushButton("+ ADD TO TEAM");
    add_team_btn_->setCursor(Qt::PointingHandCursor);
    add_team_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;font-size:9px;"
                "font-weight:600;padding:3px 8px;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::CYAN, ui::colors::BG_HOVER));
    tbl->addWidget(add_team_btn_);
    wl->addWidget(toggle_bar);

    // Stacked: form vs JSON
    auto* stack = new QStackedWidget;

    // Form view
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                  "QScrollBar:vertical{background:%1;width:6px;}"
                                  "QScrollBar::handle:vertical{background:%2;min-height:30px;}")
                              .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    form_widget_ = new QWidget;
    auto* vl = new QVBoxLayout(form_widget_);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    agent_name_label_ = new QLabel("Select an agent");
    agent_name_label_->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;").arg(ui::colors::TEXT_PRIMARY));
    vl->addWidget(agent_name_label_);

    agent_desc_label_ = new QLabel;
    agent_desc_label_->setWordWrap(true);
    agent_desc_label_->setStyleSheet(
        QString("color:%1;font-size:12px;padding-bottom:8px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(agent_desc_label_);

    // MODEL
    vl->addWidget(section_hdr("MODEL"));
    auto* mg = new QHBoxLayout;
    provider_combo_ = new QComboBox;
    for (const auto& p : {"openai", "anthropic", "google", "groq", "deepseek", "ollama", "openrouter", "fincept"})
        provider_combo_->addItem(p);
    provider_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%4;}")
            .arg(kInput, ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::AMBER_DIM));
    mg->addWidget(provider_combo_, 1);
    model_id_edit_ = new QLineEdit;
    model_id_edit_->setPlaceholderText("model id");
    model_id_edit_->setStyleSheet(kInput);
    mg->addWidget(model_id_edit_, 2);
    vl->addLayout(mg);

    auto* pg = new QHBoxLayout;
    auto* tl = new QLabel("Temp:");
    tl->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    pg->addWidget(tl);
    temperature_spin_ = new QDoubleSpinBox;
    temperature_spin_->setRange(0.0, 2.0);
    temperature_spin_->setSingleStep(0.1);
    temperature_spin_->setValue(0.7);
    temperature_spin_->setStyleSheet(kInput);
    pg->addWidget(temperature_spin_);
    auto* tkl = new QLabel("Max Tokens:");
    tkl->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    pg->addWidget(tkl);
    max_tokens_spin_ = new QSpinBox;
    max_tokens_spin_->setRange(100, 128000);
    max_tokens_spin_->setSingleStep(500);
    max_tokens_spin_->setValue(4096);
    max_tokens_spin_->setStyleSheet(kInput);
    pg->addWidget(max_tokens_spin_);
    pg->addStretch();
    vl->addLayout(pg);

    // INSTRUCTIONS
    vl->addWidget(section_hdr("INSTRUCTIONS"));
    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText("System prompt / instructions...");
    instructions_edit_->setMaximumHeight(120);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kInput));
    vl->addWidget(instructions_edit_);

    // TOOLS
    vl->addWidget(section_hdr("TOOLS"));
    tools_list_ = new QListWidget;
    tools_list_->setMaximumHeight(100);
    tools_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                                       "QListWidget::item{padding:2px 6px;}")
                                   .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY));
    vl->addWidget(tools_list_);

    // FEATURES (expanded)
    vl->addWidget(section_hdr("FEATURES"));
    auto chk_style = QString("QCheckBox{color:%1;font-size:11px;spacing:6px;}").arg(ui::colors::TEXT_PRIMARY);
    auto* feat_grid = new QGridLayout;
    reasoning_check_ = new QCheckBox("Reasoning");
    reasoning_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(reasoning_check_, 0, 0);
    memory_check_ = new QCheckBox("Memory");
    memory_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(memory_check_, 0, 1);
    knowledge_check_ = new QCheckBox("Knowledge");
    knowledge_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(knowledge_check_, 1, 0);
    guardrails_check_ = new QCheckBox("Guardrails");
    guardrails_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(guardrails_check_, 1, 1);
    tracing_check_ = new QCheckBox("Tracing");
    tracing_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(tracing_check_, 2, 0);
    agentic_memory_check_ = new QCheckBox("Agentic Memory");
    agentic_memory_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(agentic_memory_check_, 2, 1);
    vl->addLayout(feat_grid);

    // Actions
    vl->addSpacing(12);
    auto* acts = new QHBoxLayout;
    save_btn_ = new QPushButton("SAVE CONFIG");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px 16px;"
                "font-size:11px;font-weight:600;letter-spacing:1px;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    acts->addWidget(save_btn_);
    delete_btn_ = new QPushButton("DELETE");
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:8px 16px;"
                "font-size:11px;font-weight:600;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::NEGATIVE, ui::colors::BG_HOVER));
    acts->addWidget(delete_btn_);
    acts->addStretch();
    vl->addLayout(acts);
    vl->addStretch();

    scroll->setWidget(form_widget_);
    stack->addWidget(scroll); // index 0 = form

    // JSON editor view
    json_widget_ = new QWidget;
    auto* jl = new QVBoxLayout(json_widget_);
    jl->setContentsMargins(8, 8, 8, 8);
    json_editor_ = new QPlainTextEdit;
    json_editor_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;"
                                        "font-size:12px;font-family:'Consolas','Courier New',monospace;}")
                                    .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
    jl->addWidget(json_editor_);
    auto* apply_btn = new QPushButton("APPLY JSON");
    apply_btn->setCursor(Qt::PointingHandCursor);
    apply_btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                                     "font-weight:700;}QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    connect(apply_btn, &QPushButton::clicked, this, [this, stack]() {
        // Parse JSON from editor and switch back to form
        QJsonDocument doc = QJsonDocument::fromJson(json_editor_->toPlainText().toUtf8());
        if (!doc.isNull() && selected_agent_idx_ >= 0) {
            filtered_agents_[selected_agent_idx_].config = doc.object();
            load_agent_into_editor(filtered_agents_[selected_agent_idx_]);
        }
        json_toggle_btn_->setChecked(false);
        stack->setCurrentIndex(0);
    });
    jl->addWidget(apply_btn);
    stack->addWidget(json_widget_); // index 1 = JSON

    connect(json_toggle_btn_, &QPushButton::toggled, this, [this, stack](bool checked) {
        json_mode_ = checked;
        if (checked) {
            // Sync form → JSON
            json_editor_->setPlainText(QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Indented));
        }
        stack->setCurrentIndex(checked ? 1 : 0);
    });

    wl->addWidget(stack, 1);
    return wrapper;
}

// ── Right: query + results ───────────────────────────────────────────────────

QWidget* AgentsViewPanel::build_query_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(300);
    panel->setMaximumWidth(450);
    panel->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    vl->addWidget(section_hdr("TEST QUERY"));

    // Auto-route + output model
    auto* opts = new QHBoxLayout;
    auto_route_check_ = new QCheckBox("Auto-Route");
    auto_route_check_->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;}").arg(ui::colors::POSITIVE));
    opts->addWidget(auto_route_check_);

    auto* om_lbl = new QLabel("Output:");
    om_lbl->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    opts->addWidget(om_lbl);
    output_model_combo_ = new QComboBox;
    output_model_combo_->addItems({"text", "InvestmentReport", "StockAnalysis", "RiskAssessment", "TradeSignal"});
    output_model_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:10px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED, ui::colors::AMBER_DIM));
    opts->addWidget(output_model_combo_);
    opts->addStretch();
    vl->addLayout(opts);

    query_input_ = new QPlainTextEdit;
    query_input_->setPlaceholderText("Enter a query to test this agent...");
    query_input_->setMaximumHeight(80);
    query_input_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    vl->addWidget(query_input_);

    run_btn_ = new QPushButton("RUN AGENT");
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                                    "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE, ui::colors::BG_RAISED,
                                     ui::colors::TEXT_TERTIARY));
    vl->addWidget(run_btn_);

    // Routing info
    routing_info_label_ = new QLabel;
    routing_info_label_->setWordWrap(true);
    routing_info_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::CYAN));
    routing_info_label_->hide();
    vl->addWidget(routing_info_label_);

    result_status_ = new QLabel;
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(result_status_);

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

    return panel;
}

// ── Connections ──────────────────────────────────────────────────────────────

void AgentsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory> categories) {
                all_agents_ = agents;
                category_combo_->blockSignals(true);
                int prev = category_combo_->currentIndex();
                category_combo_->clear();
                category_combo_->addItem("All Categories", "");
                for (const auto& cat : categories)
                    category_combo_->addItem(QString("%1 (%2)").arg(cat.name).arg(cat.count), cat.name);
                if (prev >= 0 && prev < category_combo_->count())
                    category_combo_->setCurrentIndex(prev);
                category_combo_->blockSignals(false);
                on_category_changed(category_combo_->currentIndex());
            });

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        executing_ = false;
        run_btn_->setEnabled(true);
        run_btn_->setText("RUN AGENT");
        if (r.success) {
            result_display_->setPlainText(r.response);
            result_status_->setText(QString("Completed in %1ms").arg(r.execution_time_ms));
            result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
        } else {
            result_display_->setPlainText("Error: " + r.error);
            result_status_->setText("FAILED");
            result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
        }
    });

    connect(&svc, &services::AgentService::routing_result, this, [this](services::RoutingResult r) {
        if (r.success) {
            routing_info_label_->setText(QString("Routed → %1 (intent: %2, confidence: %3%)")
                                             .arg(r.agent_id, r.intent)
                                             .arg(int(r.confidence * 100)));
            routing_info_label_->show();
        }
    });

    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AgentsViewPanel::on_category_changed);
    connect(agent_list_, &QListWidget::currentRowChanged, this, &AgentsViewPanel::on_agent_selected);
    connect(run_btn_, &QPushButton::clicked, this, &AgentsViewPanel::run_query);
    connect(save_btn_, &QPushButton::clicked, this, &AgentsViewPanel::save_current_config);
    connect(delete_btn_, &QPushButton::clicked, this, &AgentsViewPanel::delete_current_config);
    connect(add_team_btn_, &QPushButton::clicked, this, [this]() {
        if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
            emit add_agent_to_team(filtered_agents_[selected_agent_idx_]);
    });
}

// ── Population ───────────────────────────────────────────────────────────────

void AgentsViewPanel::populate_agent_list(const QVector<services::AgentInfo>& agents) {
    agent_list_->clear();
    filtered_agents_ = agents;
    for (const auto& a : agents) {
        auto* item = new QListWidgetItem(a.name);
        item->setToolTip(a.description);
        item->setData(Qt::UserRole, a.id);
        agent_list_->addItem(item);
    }
    list_count_label_->setText(QString::number(agents.size()));
}

void AgentsViewPanel::on_category_changed(int index) {
    QString cat = category_combo_->itemData(index).toString();
    if (cat.isEmpty()) {
        populate_agent_list(all_agents_);
    } else {
        QVector<services::AgentInfo> f;
        for (const auto& a : all_agents_)
            if (a.category == cat)
                f.append(a);
        populate_agent_list(f);
    }
}

void AgentsViewPanel::on_agent_selected(int row) {
    if (row < 0 || row >= filtered_agents_.size()) {
        selected_agent_idx_ = -1;
        return;
    }
    selected_agent_idx_ = row;
    load_agent_into_editor(filtered_agents_[row]);
}

void AgentsViewPanel::load_agent_into_editor(const services::AgentInfo& agent) {
    agent_name_label_->setText(agent.name);
    agent_desc_label_->setText(agent.description);

    QJsonObject config = agent.config;
    QJsonObject model = config["model"].toObject();
    int pi = provider_combo_->findText(model["provider"].toString(agent.provider));
    if (pi >= 0)
        provider_combo_->setCurrentIndex(pi);
    model_id_edit_->setText(model["model_id"].toString());
    temperature_spin_->setValue(model["temperature"].toDouble(0.7));
    max_tokens_spin_->setValue(model["max_tokens"].toInt(4096));
    instructions_edit_->setPlainText(config["instructions"].toString());

    tools_list_->clear();
    QJsonArray tools = config["tools"].toArray();
    for (const auto& t : tools)
        tools_list_->addItem(new QListWidgetItem(t.toString()));

    reasoning_check_->setChecked(config["reasoning"].toBool());
    memory_check_->setChecked(config["memory"].toBool());
    knowledge_check_->setChecked(config["knowledge"].toBool());
    guardrails_check_->setChecked(config["guardrails"].toBool());
    tracing_check_->setChecked(config["tracing"].toBool());
    agentic_memory_check_->setChecked(config["agentic_memory"].toBool());

    // Sync JSON editor if open
    if (json_mode_)
        json_editor_->setPlainText(QJsonDocument(config).toJson(QJsonDocument::Indented));
}

QJsonObject AgentsViewPanel::build_config_from_editor() const {
    QJsonObject config;
    QJsonObject model;
    model["provider"] = provider_combo_->currentText();
    model["model_id"] = model_id_edit_->text();
    model["temperature"] = temperature_spin_->value();
    model["max_tokens"] = max_tokens_spin_->value();
    config["model"] = model;
    config["instructions"] = instructions_edit_->toPlainText();

    QJsonArray tools;
    for (int i = 0; i < tools_list_->count(); ++i)
        tools.append(tools_list_->item(i)->text());
    config["tools"] = tools;

    config["reasoning"] = reasoning_check_->isChecked();
    config["memory"] = memory_check_->isChecked();
    config["knowledge"] = knowledge_check_->isChecked();
    config["guardrails"] = guardrails_check_->isChecked();
    config["tracing"] = tracing_check_->isChecked();
    config["agentic_memory"] = agentic_memory_check_->isChecked();
    return config;
}

void AgentsViewPanel::save_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    const auto& agent = filtered_agents_[selected_agent_idx_];
    AgentConfig db;
    db.id = agent.id;
    db.name = agent.name;
    db.description = agent.description;
    db.category = agent.category;
    db.config_json = QString::fromUtf8(QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(db);
    result_status_->setText("Config saved");
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
}

void AgentsViewPanel::delete_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    services::AgentService::instance().delete_config(filtered_agents_[selected_agent_idx_].id);
    result_status_->setText("Config deleted");
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::WARNING));
}

void AgentsViewPanel::run_query() {
    QString q = query_input_->toPlainText().trimmed();
    if (q.isEmpty() || executing_)
        return;

    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText("RUNNING...");
    result_display_->clear();
    routing_info_label_->hide();
    result_status_->setText("Executing...");
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER));

    QJsonObject config = build_config_from_editor();
    auto& svc = services::AgentService::instance();

    if (auto_route_check_->isChecked()) {
        svc.route_query(q);
        // The routing_result signal handler will show routing info,
        // then execute_routed_query is called by the agent_result handler
        svc.execute_routed_query(q);
    } else {
        QString om = output_model_combo_->currentText();
        if (om != "text")
            svc.run_agent_structured(q, config, om);
        else
            svc.run_agent(q, config);
    }
}

void AgentsViewPanel::toggle_json_editor() {
    json_toggle_btn_->setChecked(!json_toggle_btn_->isChecked());
}

} // namespace fincept::screens
