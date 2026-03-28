// src/screens/agent_config/AgentsViewPanel.cpp
#include "screens/agent_config/AgentsViewPanel.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmProfileRepository.h"
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

static const QString kInput =
    QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:12px;")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED);

AgentsViewPanel::AgentsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentsViewPanel");
    build_ui();
    setup_connections();
}

// ── Left: agent list ──────────────────────────────────────────────────────────

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
    title->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hdr->addWidget(title);
    list_count_label_ = new QLabel("0");
    list_count_label_->setStyleSheet(
        QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
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
    agent_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                "QListWidget::item:selected{background:%4;}"
                "QListWidget::item:hover{background:%5;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                 ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(agent_list_, 1);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:6px;"
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

// ── Center: config editor ─────────────────────────────────────────────────────

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
        QString("QPushButton{background:transparent;color:%1;border:none;font-size:10px;font-weight:600;padding:4px 8px;}"
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

    // ── Form view ──
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%2;min-height:30px;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    form_widget_ = new QWidget;
    auto* vl = new QVBoxLayout(form_widget_);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    agent_name_label_ = new QLabel("Select an agent");
    agent_name_label_->setStyleSheet(
        QString("color:%1;font-size:16px;font-weight:700;").arg(ui::colors::TEXT_PRIMARY));
    vl->addWidget(agent_name_label_);

    agent_desc_label_ = new QLabel;
    agent_desc_label_->setWordWrap(true);
    agent_desc_label_->setStyleSheet(
        QString("color:%1;font-size:12px;padding-bottom:8px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(agent_desc_label_);

    // ── LLM PROFILE — per-agent profile picker ────────────────────────────────
    vl->addWidget(section_hdr("LLM PROFILE"));

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip("Select which LLM profile this agent uses. Create profiles in Settings > LLM Config > Profiles.");
    llm_profile_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 10px;font-size:12px;}"
                "QComboBox::drop-down{border:none;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    vl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(llm_resolved_lbl_);

    // ── INSTRUCTIONS ──
    vl->addWidget(section_hdr("INSTRUCTIONS"));
    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText("System prompt / instructions...");
    instructions_edit_->setMaximumHeight(120);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kInput));
    vl->addWidget(instructions_edit_);

    // ── TOOLS ──
    vl->addWidget(section_hdr("TOOLS"));
    tools_list_ = new QListWidget;
    tools_list_->setMaximumHeight(100);
    tools_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                "QListWidget::item{padding:2px 6px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY));
    vl->addWidget(tools_list_);

    // ── FEATURES ──
    vl->addWidget(section_hdr("FEATURES"));
    const QString chk_style =
        QString("QCheckBox{color:%1;font-size:11px;spacing:6px;}").arg(ui::colors::TEXT_PRIMARY);
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

    // ── Actions ──
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

    // ── JSON editor view ──
    json_widget_ = new QWidget;
    auto* jl = new QVBoxLayout(json_widget_);
    jl->setContentsMargins(8, 8, 8, 8);
    json_editor_ = new QPlainTextEdit;
    json_editor_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;"
                "font-size:12px;font-family:'Consolas','Courier New',monospace;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
    jl->addWidget(json_editor_);
    auto* apply_btn = new QPushButton("APPLY JSON");
    apply_btn->setCursor(Qt::PointingHandCursor);
    apply_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                "font-weight:700;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    connect(apply_btn, &QPushButton::clicked, this, [this, stack]() {
        const QJsonDocument doc = QJsonDocument::fromJson(json_editor_->toPlainText().toUtf8());
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
        if (checked)
            json_editor_->setPlainText(
                QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Indented));
        stack->setCurrentIndex(checked ? 1 : 0);
    });

    wl->addWidget(stack, 1);
    return wrapper;
}

// ── Right: query + results ────────────────────────────────────────────────────

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

    auto* opts = new QHBoxLayout;
    auto_route_check_ = new QCheckBox("Auto-Route");
    auto_route_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:10px;}").arg(ui::colors::POSITIVE));
    opts->addWidget(auto_route_check_);

    auto* om_lbl = new QLabel("Output:");
    om_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
    opts->addWidget(om_lbl);

    output_model_combo_ = new QComboBox;
    output_model_combo_->addItems({"text", "InvestmentReport", "StockAnalysis", "RiskAssessment", "TradeSignal"});
    output_model_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:10px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED,
                 ui::colors::AMBER_DIM));
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
    run_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE,
                 ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    vl->addWidget(run_btn_);

    routing_info_label_ = new QLabel;
    routing_info_label_->setWordWrap(true);
    routing_info_label_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::CYAN));
    routing_info_label_->hide();
    vl->addWidget(routing_info_label_);

    result_status_ = new QLabel;
    result_status_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(result_status_);

    auto* rh = new QLabel("RESULT");
    rh->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
            .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(rh);

    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                 ui::colors::BORDER_BRIGHT));
    // Document stylesheet controls rendered markdown colors (headers, code, links, etc.)
    result_display_->document()->setDefaultStyleSheet(
        QString("body { color: %1; background: %2; font-size: 12px; }"
                "h1, h2, h3, h4 { color: %3; margin: 8px 0 4px 0; }"
                "h1 { font-size: 16px; border-bottom: 1px solid %4; padding-bottom: 4px; }"
                "h2 { font-size: 14px; }"
                "h3 { font-size: 13px; color: %5; }"
                "code { background: %6; color: %7; padding: 1px 4px; border-radius: 3px; font-family: monospace; }"
                "pre  { background: %6; color: %7; padding: 8px; border-radius: 4px; font-family: monospace; }"
                "blockquote { border-left: 3px solid %3; margin: 4px 0; padding-left: 10px; color: %8; }"
                "a { color: %5; }"
                "ul, ol { margin: 4px 0; padding-left: 20px; }"
                "li { margin: 2px 0; }"
                "strong { color: %1; }"
                "hr { border: none; border-top: 1px solid %4; }")
            .arg(ui::colors::TEXT_PRIMARY,   // body color          %1
                 ui::colors::BG_BASE,        // body background     %2
                 ui::colors::AMBER,          // h1/h2 + blockquote  %3
                 ui::colors::BORDER_DIM,     // hr / h1 border      %4
                 ui::colors::CYAN,           // h3 / links          %5
                 ui::colors::BG_RAISED,      // code background     %6
                 ui::colors::TEXT_PRIMARY,   // code color          %7
                 ui::colors::TEXT_SECONDARY  // blockquote text     %8
            ));
    vl->addWidget(result_display_, 1);

    return panel;
}

// ── Layout ────────────────────────────────────────────────────────────────────

void AgentsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(
        QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM));
    splitter->addWidget(build_agent_list_panel());
    splitter->addWidget(build_config_panel());
    splitter->addWidget(build_query_panel());
    splitter->setSizes({220, 500, 350});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter);
}

// ── Connections ───────────────────────────────────────────────────────────────

void AgentsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory> categories) {
                all_agents_ = agents;
                category_combo_->blockSignals(true);
                const int prev = category_combo_->currentIndex();
                category_combo_->clear();
                category_combo_->addItem("All Categories", "");
                for (const auto& cat : categories)
                    category_combo_->addItem(QString("%1 (%2)").arg(cat.name).arg(cat.count), cat.name);
                if (prev >= 0 && prev < category_combo_->count())
                    category_combo_->setCurrentIndex(prev);
                category_combo_->blockSignals(false);
                on_category_changed(category_combo_->currentIndex());
            });

    connect(&svc, &services::AgentService::agent_result, this,
            [this](services::AgentExecutionResult r) {
                if (r.request_id != pending_request_id_) return;
                executing_ = false;
                pending_request_id_.clear();
                run_btn_->setEnabled(true);
                run_btn_->setText("RUN AGENT");
                if (r.success) {
                    result_display_->setMarkdown(r.response);
                    result_status_->setText(QString("Completed in %1ms").arg(r.execution_time_ms));
                    result_status_->setStyleSheet(
                        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
                } else {
                    result_display_->setPlainText("Error: " + r.error);
                    result_status_->setText("FAILED");
                    result_status_->setStyleSheet(
                        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
                }
            });

    // ── Streaming signals ─────────────────────────────────────────────────────
    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_) return;
                result_status_->setText(status);
            });

    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_) return;
                QTextCursor cursor = result_display_->textCursor();
                cursor.movePosition(QTextCursor::End);
                result_display_->setTextCursor(cursor);
                result_display_->insertPlainText(token + " ");
            });

    connect(&svc, &services::AgentService::agent_stream_done, this,
            [this](services::AgentExecutionResult r) {
                if (r.request_id != pending_request_id_) return;
                executing_ = false;
                pending_request_id_.clear();
                run_btn_->setEnabled(true);
                run_btn_->setText("RUN AGENT");
                if (r.success) {
                    result_display_->setMarkdown(r.response);
                    result_status_->setText(QString("Completed in %1ms").arg(r.execution_time_ms));
                    result_status_->setStyleSheet(
                        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
                } else {
                    result_display_->setPlainText("Error: " + r.error);
                    result_status_->setText("FAILED");
                    result_status_->setStyleSheet(
                        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
                }
            });

    connect(&svc, &services::AgentService::routing_result, this,
            [this](services::RoutingResult r) {
                if (r.request_id != pending_request_id_) return;
                if (r.success) {
                    routing_info_label_->setText(
                        QString("Routed → %1 (intent: %2, confidence: %3%)")
                            .arg(r.agent_id, r.intent)
                            .arg(static_cast<int>(r.confidence * 100)));
                    routing_info_label_->show();
                }
            });

    // Reload profile combo when LLM config changes
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed,
            this, &AgentsViewPanel::load_profile_combo);

    // Update resolved label when user picks a different profile
    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentsViewPanel::refresh_llm_pill);

    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentsViewPanel::on_category_changed);
    connect(agent_list_, &QListWidget::currentRowChanged,
            this, &AgentsViewPanel::on_agent_selected);
    connect(run_btn_,    &QPushButton::clicked, this, &AgentsViewPanel::run_query);
    connect(save_btn_,   &QPushButton::clicked, this, &AgentsViewPanel::save_current_config);
    connect(delete_btn_, &QPushButton::clicked, this, &AgentsViewPanel::delete_current_config);
    connect(add_team_btn_, &QPushButton::clicked, this, [this]() {
        if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
            emit add_agent_to_team(filtered_agents_[selected_agent_idx_]);
    });

    // Populate profile combo on first show
    load_profile_combo();
}

// ── LLM profile combo ─────────────────────────────────────────────────────────

void AgentsViewPanel::load_profile_combo() {
    // Preserve current selection
    const QString prev_id = llm_profile_combo_->currentData().toString();
    llm_profile_combo_->blockSignals(true);
    llm_profile_combo_->clear();

    // First item = use global default (no explicit assignment)
    llm_profile_combo_->addItem("Default (Global)", QString{});

    const auto pr = LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<LlmProfile>{};
    for (const auto& p : profiles) {
        QString label = p.name;
        if (p.is_default)
            label += " [default]";
        llm_profile_combo_->addItem(label, p.id);
    }

    // Restore previous selection if still present
    int restore = 0;
    if (!prev_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == prev_id) {
                restore = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(restore);
    // Trigger resolved label update
    refresh_llm_pill();
}

void AgentsViewPanel::refresh_llm_pill() {
    const QString profile_id = llm_profile_combo_->currentData().toString();

    // Get agent_id for current selection (may be empty if none selected yet)
    QString agent_id;
    if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
        agent_id = filtered_agents_[selected_agent_idx_].id;

    // Resolve: if user picked "Default (Global)" use agent-id-based resolution,
    // otherwise resolve the explicitly chosen profile
    ResolvedLlmProfile resolved;
    if (profile_id.isEmpty()) {
        // Resolve via assignment chain (entity → type-default → global → legacy)
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);
    } else {
        // User explicitly chose a profile — look it up directly
        const auto pr2 = LlmProfileRepository::instance().list_profiles();
        const auto profiles = pr2.is_ok() ? pr2.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles) {
            if (p.id == profile_id) {
                resolved.profile_id   = p.id;
                resolved.profile_name = p.name;
                resolved.provider     = p.provider;
                resolved.model_id     = p.model_id;
                resolved.api_key      = p.api_key;
                resolved.base_url     = p.base_url;
                resolved.temperature  = p.temperature;
                resolved.max_tokens   = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
        // Fallback to resolution if not found
        if (resolved.profile_id.isEmpty())
            resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);
    }

    if (!resolved.provider.isEmpty()) {
        QString text = resolved.provider.toUpper() + " / " + resolved.model_id;
        if (text.length() > 60)
            text = text.left(58) + "..";
        if (profile_id.isEmpty())
            text += " (inherited)";
        llm_resolved_lbl_->setText(text);
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    } else {
        llm_resolved_lbl_->setText("No provider configured — go to Settings > LLM Config");
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
    }
}

// ── Population ────────────────────────────────────────────────────────────────

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
    const QString cat = category_combo_->itemData(index).toString();
    if (cat.isEmpty()) {
        populate_agent_list(all_agents_);
    } else {
        QVector<services::AgentInfo> filtered;
        for (const auto& a : all_agents_)
            if (a.category == cat)
                filtered.append(a);
        populate_agent_list(filtered);
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

    // Restore the profile assignment for this agent in the combo
    const QString assigned_id = LlmProfileRepository::instance()
                                    .get_assignment("agent", agent.id);
    llm_profile_combo_->blockSignals(true);
    int select_idx = 0; // default = "Default (Global)"
    if (!assigned_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == assigned_id) {
                select_idx = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(select_idx);
    refresh_llm_pill();

    const QJsonObject config = agent.config;
    instructions_edit_->setPlainText(config["instructions"].toString());

    tools_list_->clear();
    const QJsonArray tools = config["tools"].toArray();
    for (const auto& t : tools)
        tools_list_->addItem(new QListWidgetItem(t.toString()));

    reasoning_check_->setChecked(config["reasoning"].toBool());
    memory_check_->setChecked(config["memory"].toBool());
    knowledge_check_->setChecked(config["knowledge"].toBool());
    guardrails_check_->setChecked(config["guardrails"].toBool());
    tracing_check_->setChecked(config["tracing"].toBool());
    agentic_memory_check_->setChecked(config["agentic_memory"].toBool());

    if (json_mode_)
        json_editor_->setPlainText(QJsonDocument(config).toJson(QJsonDocument::Indented));
}

QJsonObject AgentsViewPanel::build_config_from_editor() const {
    QJsonObject config;

    // Resolve the LLM profile for this agent.
    // If the user explicitly chose a profile in the combo, use that.
    // Otherwise fall back to the assignment chain.
    const QString profile_id = llm_profile_combo_->currentData().toString();
    QString agent_id;
    if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
        agent_id = filtered_agents_[selected_agent_idx_].id;

    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        // Explicit profile selection: look it up by ID
        const auto pr3 = LlmProfileRepository::instance().list_profiles();
        const auto profiles = pr3.is_ok() ? pr3.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles) {
            if (p.id == profile_id) {
                resolved.profile_id    = p.id;
                resolved.profile_name  = p.name;
                resolved.provider      = p.provider;
                resolved.model_id      = p.model_id;
                resolved.api_key       = p.api_key;
                resolved.base_url      = p.base_url;
                resolved.temperature   = p.temperature;
                resolved.max_tokens    = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
    }
    if (resolved.provider.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);

    config["model"] = ai_chat::LlmService::profile_to_json(resolved);

    config["instructions"] = instructions_edit_->toPlainText();

    QJsonArray tools;
    for (int i = 0; i < tools_list_->count(); ++i)
        tools.append(tools_list_->item(i)->text());
    config["tools"] = tools;

    config["reasoning"]      = reasoning_check_->isChecked();
    config["memory"]         = memory_check_->isChecked();
    config["knowledge"]      = knowledge_check_->isChecked();
    config["guardrails"]     = guardrails_check_->isChecked();
    config["tracing"]        = tracing_check_->isChecked();
    config["agentic_memory"] = agentic_memory_check_->isChecked();
    return config;
}

// ── Actions ───────────────────────────────────────────────────────────────────

void AgentsViewPanel::save_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    const auto& agent = filtered_agents_[selected_agent_idx_];

    // Persist profile assignment for this agent
    const QString profile_id = llm_profile_combo_->currentData().toString();
    auto& repo = LlmProfileRepository::instance();
    if (profile_id.isEmpty())
        repo.remove_assignment("agent", agent.id);          // revert to global default
    else
        repo.assign_profile("agent", agent.id, profile_id); // pin to chosen profile

    AgentConfig db;
    db.id          = agent.id;
    db.name        = agent.name;
    db.description = agent.description;
    db.category    = agent.category;
    db.config_json =
        QString::fromUtf8(QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(db);
    result_status_->setText("Config saved");
    result_status_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
}

void AgentsViewPanel::delete_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    services::AgentService::instance().delete_config(filtered_agents_[selected_agent_idx_].id);
    result_status_->setText("Config deleted");
    result_status_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::WARNING));
}

void AgentsViewPanel::run_query() {
    const QString q = query_input_->toPlainText().trimmed();
    if (q.isEmpty() || executing_)
        return;

    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText("RUNNING...");
    result_display_->clear();
    routing_info_label_->hide();
    result_status_->setText("Executing...");
    result_status_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER));

    const QJsonObject config = build_config_from_editor();
    auto& svc = services::AgentService::instance();

    if (auto_route_check_->isChecked()) {
        pending_request_id_ = svc.route_query(q);
        svc.execute_routed_query(q, config);
    } else {
        const QString om = output_model_combo_->currentText();
        if (om != "text")
            pending_request_id_ = svc.run_agent_structured(q, config, om);
        else
            pending_request_id_ = svc.run_agent_streaming(q, config);
    }
}

void AgentsViewPanel::toggle_json_editor() {
    json_toggle_btn_->setChecked(!json_toggle_btn_->isChecked());
}

void AgentsViewPanel::apply_tools_selection(const QStringList& tools) {
    for (int i = 0; i < tools_list_->count(); ++i) {
        auto* item = tools_list_->item(i);
        item->setCheckState(tools.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }
}

} // namespace fincept::screens
