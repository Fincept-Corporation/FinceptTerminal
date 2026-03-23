// src/screens/agent_config/CreateAgentPanel.cpp
#include "screens/agent_config/CreateAgentPanel.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace {
static const QString kIn =
    QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:12px;")
        .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_MED);
static const QString kChk = QString("QCheckBox{color:%1;font-size:11px;spacing:6px;}").arg(fincept::ui::colors::TEXT_PRIMARY);
} // namespace

namespace fincept::screens {

static QGroupBox* make_section(const QString& title, bool collapsible = true) {
    auto* box = new QGroupBox(title);
    box->setCheckable(collapsible);
    box->setChecked(!collapsible); // collapsed by default if collapsible
    box->setStyleSheet(
        QString("QGroupBox{color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                "border:1px solid %2;border-radius:0;margin-top:12px;padding-top:16px;}"
                "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;}"
                "QGroupBox::indicator{width:10px;height:10px;}")
            .arg(ui::colors::AMBER, ui::colors::BORDER_DIM));
    return box;
}

static QLabel* field_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:11px;min-width:90px;").arg(ui::colors::TEXT_SECONDARY));
    return l;
}

CreateAgentPanel::CreateAgentPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("CreateAgentPanel");
    build_ui();
    setup_connections();
}

void CreateAgentPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM));
    splitter->addWidget(build_saved_list_panel());
    splitter->addWidget(build_form_panel());
    splitter->setSizes({280, 600});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter);
}

// ── Left panel ───────────────────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_saved_list_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(240);
    panel->setMaximumWidth(340);
    panel->setStyleSheet(QString("background:%1;border-right:1px solid %2;")
                             .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    auto* title = new QLabel("SAVED AGENTS");
    title->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hdr->addWidget(title);
    saved_count_ = new QLabel("0");
    saved_count_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                    .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    hdr->addWidget(saved_count_);
    hdr->addStretch();
    vl->addLayout(hdr);

    saved_list_ = new QListWidget;
    saved_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                "QListWidget::item:selected{background:%4;}"
                "QListWidget::item:hover{background:%5;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                 ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(saved_list_, 1);

    auto* btns = new QHBoxLayout;
    auto* load_btn = new QPushButton("LOAD");
    load_btn->setCursor(Qt::PointingHandCursor);
    load_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:5px 12px;"
                "font-size:10px;font-weight:600;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::BG_RAISED, ui::colors::AMBER, ui::colors::AMBER_DIM));
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
    btns->addWidget(load_btn);

    auto* del_btn = new QPushButton("DELETE");
    del_btn->setCursor(Qt::PointingHandCursor);
    del_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:5px 12px;"
                "font-size:10px;font-weight:600;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::NEGATIVE, ui::colors::BG_HOVER));
    connect(del_btn, &QPushButton::clicked, this, &CreateAgentPanel::delete_agent);
    btns->addWidget(del_btn);
    btns->addStretch();
    vl->addLayout(btns);

    // Export / Import
    auto* io = new QHBoxLayout;
    auto* exp_btn = new QPushButton("EXPORT");
    exp_btn->setCursor(Qt::PointingHandCursor);
    exp_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:4px 10px;"
                "font-size:9px;font-weight:600;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(exp_btn, &QPushButton::clicked, this, &CreateAgentPanel::export_json);
    io->addWidget(exp_btn);

    auto* imp_btn = new QPushButton("IMPORT");
    imp_btn->setCursor(Qt::PointingHandCursor);
    imp_btn->setStyleSheet(exp_btn->styleSheet());
    connect(imp_btn, &QPushButton::clicked, this, &CreateAgentPanel::import_json);
    io->addWidget(imp_btn);
    io->addStretch();
    vl->addLayout(io);

    return panel;
}

// ── Right: full form ─────────────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%2;min-height:30px;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(4);

    // ── BASIC ────────────────────────────────────────────────────────────────
    auto* basic = make_section("BASIC", false);
    auto* bl = new QVBoxLayout(basic);
    bl->setSpacing(6);

    auto* r1 = new QHBoxLayout; r1->addWidget(field_label("Name:"));
    name_edit_ = new QLineEdit; name_edit_->setPlaceholderText("Agent name"); name_edit_->setStyleSheet(kIn);
    r1->addWidget(name_edit_, 1); bl->addLayout(r1);

    auto* r2 = new QHBoxLayout; r2->addWidget(field_label("Category:"));
    category_combo_ = new QComboBox;
    for (const auto& c : {"general", "trader", "hedge-fund", "economic", "geopolitics", "research", "custom"})
        category_combo_->addItem(c);
    category_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    r2->addWidget(category_combo_, 1); bl->addLayout(r2);

    bl->addWidget(field_label("Description:"));
    desc_edit_ = new QPlainTextEdit; desc_edit_->setPlaceholderText("What does this agent do?");
    desc_edit_->setMaximumHeight(60); desc_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kIn));
    bl->addWidget(desc_edit_);
    vl->addWidget(basic);

    // ── MODEL ────────────────────────────────────────────────────────────────
    auto* model = make_section("MODEL", false);
    auto* ml = new QVBoxLayout(model); ml->setSpacing(6);

    auto* r3 = new QHBoxLayout; r3->addWidget(field_label("Provider:"));
    provider_combo_ = new QComboBox;
    for (const auto& p : {"openai", "anthropic", "google", "groq", "deepseek", "ollama", "openrouter", "fincept"})
        provider_combo_->addItem(p);
    provider_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    r3->addWidget(provider_combo_, 1); ml->addLayout(r3);

    auto* r4 = new QHBoxLayout; r4->addWidget(field_label("Model ID:"));
    model_id_edit_ = new QLineEdit; model_id_edit_->setPlaceholderText("e.g. gpt-4o"); model_id_edit_->setStyleSheet(kIn);
    r4->addWidget(model_id_edit_, 1); ml->addLayout(r4);

    auto* r5 = new QHBoxLayout;
    r5->addWidget(field_label("Temp:")); temperature_spin_ = new QDoubleSpinBox;
    temperature_spin_->setRange(0.0, 2.0); temperature_spin_->setSingleStep(0.1);
    temperature_spin_->setValue(0.7); temperature_spin_->setStyleSheet(kIn); r5->addWidget(temperature_spin_);
    r5->addWidget(field_label("Max Tokens:")); max_tokens_spin_ = new QSpinBox;
    max_tokens_spin_->setRange(100, 128000); max_tokens_spin_->setSingleStep(500);
    max_tokens_spin_->setValue(4096); max_tokens_spin_->setStyleSheet(kIn); r5->addWidget(max_tokens_spin_);
    r5->addStretch(); ml->addLayout(r5);
    vl->addWidget(model);

    // ── INSTRUCTIONS ─────────────────────────────────────────────────────────
    auto* instr = make_section("INSTRUCTIONS", false);
    auto* il = new QVBoxLayout(instr);
    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText("System prompt / role instructions...");
    instructions_edit_->setMinimumHeight(100);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kIn));
    il->addWidget(instructions_edit_);
    vl->addWidget(instr);

    // ── TOOLS (multi-select) ─────────────────────────────────────────────────
    auto* tools_sec = make_section("TOOLS", false);
    auto* tl2 = new QVBoxLayout(tools_sec); tl2->setSpacing(4);
    tool_search_edit_ = new QLineEdit; tool_search_edit_->setPlaceholderText("Search tools...");
    tool_search_edit_->setStyleSheet(kIn); tl2->addWidget(tool_search_edit_);
    tools_list_ = new QListWidget; tools_list_->setMaximumHeight(120);
    tools_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                "QListWidget::item{padding:2px 6px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY));
    tl2->addWidget(tools_list_);
    vl->addWidget(tools_sec);

    // ── FEATURES ─────────────────────────────────────────────────────────────
    auto* feat = make_section("FEATURES", false);
    auto* fg = new QGridLayout(feat); fg->setSpacing(4);
    reasoning_check_ = new QCheckBox("Reasoning"); reasoning_check_->setStyleSheet(kChk); fg->addWidget(reasoning_check_, 0, 0);
    memory_check_ = new QCheckBox("Memory"); memory_check_->setStyleSheet(kChk); fg->addWidget(memory_check_, 0, 1);
    knowledge_check_ = new QCheckBox("Knowledge"); knowledge_check_->setStyleSheet(kChk); fg->addWidget(knowledge_check_, 1, 0);
    guardrails_check_ = new QCheckBox("Guardrails"); guardrails_check_->setStyleSheet(kChk); fg->addWidget(guardrails_check_, 1, 1);
    agentic_memory_check_ = new QCheckBox("Agentic Memory"); agentic_memory_check_->setStyleSheet(kChk); fg->addWidget(agentic_memory_check_, 2, 0);
    storage_check_ = new QCheckBox("Storage"); storage_check_->setStyleSheet(kChk); fg->addWidget(storage_check_, 2, 1);
    tracing_check_ = new QCheckBox("Tracing"); tracing_check_->setStyleSheet(kChk); fg->addWidget(tracing_check_, 3, 0);
    compression_check_ = new QCheckBox("Compression"); compression_check_->setStyleSheet(kChk); fg->addWidget(compression_check_, 3, 1);
    hooks_check_ = new QCheckBox("Hooks"); hooks_check_->setStyleSheet(kChk); fg->addWidget(hooks_check_, 4, 0);
    evaluation_check_ = new QCheckBox("Evaluation"); evaluation_check_->setStyleSheet(kChk); fg->addWidget(evaluation_check_, 4, 1);
    vl->addWidget(feat);

    // ── KNOWLEDGE CONFIG (collapsible) ───────────────────────────────────────
    auto* know = make_section("KNOWLEDGE CONFIG");
    auto* kl = new QVBoxLayout(know); kl->setSpacing(4);
    auto* kr1 = new QHBoxLayout; kr1->addWidget(field_label("Type:"));
    knowledge_type_combo_ = new QComboBox;
    knowledge_type_combo_->addItems({"url", "pdf", "text", "combined"});
    knowledge_type_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    kr1->addWidget(knowledge_type_combo_, 1); kl->addLayout(kr1);
    kl->addWidget(field_label("URLs (one per line):"));
    knowledge_urls_edit_ = new QPlainTextEdit; knowledge_urls_edit_->setMaximumHeight(60);
    knowledge_urls_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kIn)); kl->addWidget(knowledge_urls_edit_);
    auto* kr2 = new QHBoxLayout; kr2->addWidget(field_label("Vector DB:"));
    knowledge_vectordb_combo_ = new QComboBox; knowledge_vectordb_combo_->addItems({"lancedb", "pgvector", "qdrant"});
    knowledge_vectordb_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    kr2->addWidget(knowledge_vectordb_combo_, 1); kr2->addWidget(field_label("Embedder:"));
    knowledge_embedder_combo_ = new QComboBox; knowledge_embedder_combo_->addItems({"openai", "ollama", "sentence-transformers"});
    knowledge_embedder_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    kr2->addWidget(knowledge_embedder_combo_, 1); kl->addLayout(kr2);
    vl->addWidget(know);

    // ── GUARDRAILS CONFIG (collapsible) ──────────────────────────────────────
    auto* guard = make_section("GUARDRAILS CONFIG");
    auto* gl = new QVBoxLayout(guard); gl->setSpacing(4);
    guardrails_pii_check_ = new QCheckBox("PII Detection"); guardrails_pii_check_->setStyleSheet(kChk); gl->addWidget(guardrails_pii_check_);
    guardrails_injection_check_ = new QCheckBox("Injection Prevention"); guardrails_injection_check_->setStyleSheet(kChk); gl->addWidget(guardrails_injection_check_);
    guardrails_compliance_check_ = new QCheckBox("Financial Compliance"); guardrails_compliance_check_->setStyleSheet(kChk); gl->addWidget(guardrails_compliance_check_);
    vl->addWidget(guard);

    // ── MEMORY CONFIG (collapsible) ──────────────────────────────────────────
    auto* mem = make_section("MEMORY CONFIG");
    auto* mel = new QVBoxLayout(mem); mel->setSpacing(4);
    auto* mr1 = new QHBoxLayout; mr1->addWidget(field_label("DB Path:"));
    memory_db_path_edit_ = new QLineEdit; memory_db_path_edit_->setPlaceholderText("agent_memory.db");
    memory_db_path_edit_->setStyleSheet(kIn); mr1->addWidget(memory_db_path_edit_, 1); mel->addLayout(mr1);
    auto* mr2 = new QHBoxLayout; mr2->addWidget(field_label("Table:"));
    memory_table_edit_ = new QLineEdit; memory_table_edit_->setPlaceholderText("agent_memory");
    memory_table_edit_->setStyleSheet(kIn); mr2->addWidget(memory_table_edit_, 1); mel->addLayout(mr2);
    memory_user_memories_check_ = new QCheckBox("Create User Memories"); memory_user_memories_check_->setStyleSheet(kChk); mel->addWidget(memory_user_memories_check_);
    memory_session_summary_check_ = new QCheckBox("Create Session Summary"); memory_session_summary_check_->setStyleSheet(kChk); mel->addWidget(memory_session_summary_check_);
    vl->addWidget(mem);

    // ── STORAGE CONFIG (collapsible) ─────────────────────────────────────────
    auto* stor = make_section("STORAGE CONFIG");
    auto* sl = new QVBoxLayout(stor); sl->setSpacing(4);
    auto* sr1 = new QHBoxLayout; sr1->addWidget(field_label("Type:"));
    storage_type_combo_ = new QComboBox; storage_type_combo_->addItems({"sqlite", "postgres"});
    storage_type_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(kIn));
    sr1->addWidget(storage_type_combo_, 1); sl->addLayout(sr1);
    auto* sr2 = new QHBoxLayout; sr2->addWidget(field_label("DB Path:"));
    storage_db_path_edit_ = new QLineEdit; storage_db_path_edit_->setStyleSheet(kIn);
    sr2->addWidget(storage_db_path_edit_, 1); sl->addLayout(sr2);
    auto* sr3 = new QHBoxLayout; sr3->addWidget(field_label("Table:"));
    storage_table_edit_ = new QLineEdit; storage_table_edit_->setStyleSheet(kIn);
    sr3->addWidget(storage_table_edit_, 1); sl->addLayout(sr3);
    vl->addWidget(stor);

    // ── AGENTIC MEMORY (collapsible) ─────────────────────────────────────────
    auto* am = make_section("AGENTIC MEMORY CONFIG");
    auto* aml = new QVBoxLayout(am); aml->setSpacing(4);
    auto* amr = new QHBoxLayout; amr->addWidget(field_label("User ID:"));
    agentic_memory_user_id_edit_ = new QLineEdit; agentic_memory_user_id_edit_->setStyleSheet(kIn);
    amr->addWidget(agentic_memory_user_id_edit_, 1); aml->addLayout(amr);
    vl->addWidget(am);

    // ── MCP SERVERS (collapsible) ────────────────────────────────────────────
    auto* mcp = make_section("MCP SERVERS");
    auto* mcl = new QVBoxLayout(mcp); mcl->setSpacing(4);
    mcp_servers_list_ = new QListWidget; mcp_servers_list_->setMaximumHeight(80);
    mcp_servers_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    mcp_servers_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                "QListWidget::item{padding:2px 6px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY));
    mcl->addWidget(mcp_servers_list_);
    vl->addWidget(mcp);

    // ── ACTIONS ──────────────────────────────────────────────────────────────
    vl->addSpacing(12);
    auto* btns = new QHBoxLayout;
    save_btn_ = new QPushButton("SAVE AGENT");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:10px 20px;"
                "font-size:11px;font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    btns->addWidget(save_btn_);

    test_btn_ = new QPushButton("TEST");
    test_btn_->setCursor(Qt::PointingHandCursor);
    test_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:10px 20px;"
                "font-size:11px;font-weight:700;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::CYAN, ui::colors::BG_HOVER));
    btns->addWidget(test_btn_);

    auto* clr_btn = new QPushButton("CLEAR");
    clr_btn->setCursor(Qt::PointingHandCursor);
    clr_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:10px 16px;"
                "font-size:11px;font-weight:600;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(clr_btn, &QPushButton::clicked, this, &CreateAgentPanel::clear_form);
    btns->addWidget(clr_btn);
    btns->addStretch();
    vl->addLayout(btns);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(status_label_);

    auto* th = new QLabel("TEST RESULT");
    th->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:8px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(th);
    test_result_ = new QTextEdit; test_result_->setReadOnly(true); test_result_->setMaximumHeight(150);
    test_result_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
    vl->addWidget(test_result_);
    vl->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ── Connections ──────────────────────────────────────────────────────────────

void CreateAgentPanel::setup_connections() {
    connect(save_btn_, &QPushButton::clicked, this, &CreateAgentPanel::save_agent);
    connect(test_btn_, &QPushButton::clicked, this, &CreateAgentPanel::test_agent);

    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::config_saved, this, [this]() {
        status_label_->setText("Agent saved successfully");
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::POSITIVE));
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, [this]() {
        status_label_->setText("Agent deleted");
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::WARNING));
        editing_id_.clear();
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        test_btn_->setEnabled(true);
        test_btn_->setText("TEST");
        if (r.success) {
            test_result_->setPlainText(r.response);
            status_label_->setText(QString("Test completed in %1ms").arg(r.execution_time_ms));
            status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::POSITIVE));
        } else {
            test_result_->setPlainText("Error: " + r.error);
            status_label_->setText("Test failed");
            status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::NEGATIVE));
        }
    });
    connect(&svc, &services::AgentService::tools_loaded, this, [this](services::AgentToolsInfo info) {
        available_tools_.clear();
        for (const auto& cat : info.categories) {
            QJsonArray arr = info.tools[cat].toArray();
            for (const auto& t : arr) available_tools_.append(t.toString());
        }
        // Populate checkable tools list
        tools_list_->clear();
        for (const auto& t : available_tools_) {
            auto* item = new QListWidgetItem(t);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            tools_list_->addItem(item);
        }
    });
    // Filter tools
    connect(tool_search_edit_, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < tools_list_->count(); ++i) {
            auto* item = tools_list_->item(i);
            item->setHidden(!q.isEmpty() && !item->text().contains(q, Qt::CaseInsensitive));
        }
    });
}

// ── Data ─────────────────────────────────────────────────────────────────────

void CreateAgentPanel::load_saved_agents() {
    saved_list_->clear();
    auto result = AgentConfigRepository::instance().list_all();
    if (result.is_ok()) {
        saved_agents_ = result.value();
        for (const auto& a : saved_agents_) {
            saved_list_->addItem(QString("%1 [%2]").arg(a.name, a.category));
        }
        saved_count_->setText(QString::number(saved_agents_.size()));
    }
}

void CreateAgentPanel::load_agent_into_form(const AgentConfig& cfg) {
    editing_id_ = cfg.id;
    name_edit_->setText(cfg.name);
    desc_edit_->setPlainText(cfg.description);
    int ci = category_combo_->findText(cfg.category);
    if (ci >= 0) category_combo_->setCurrentIndex(ci);

    QJsonObject c = QJsonDocument::fromJson(cfg.config_json.toUtf8()).object();
    QJsonObject m = c["model"].toObject();
    int pi = provider_combo_->findText(m["provider"].toString());
    if (pi >= 0) provider_combo_->setCurrentIndex(pi);
    model_id_edit_->setText(m["model_id"].toString());
    temperature_spin_->setValue(m["temperature"].toDouble(0.7));
    max_tokens_spin_->setValue(m["max_tokens"].toInt(4096));
    instructions_edit_->setPlainText(c["instructions"].toString());

    // Tools
    QJsonArray ta = c["tools"].toArray();
    QSet<QString> selected;
    for (const auto& t : ta) selected.insert(t.toString());
    for (int i = 0; i < tools_list_->count(); ++i)
        tools_list_->item(i)->setCheckState(selected.contains(tools_list_->item(i)->text()) ? Qt::Checked : Qt::Unchecked);

    reasoning_check_->setChecked(c["reasoning"].toBool());
    memory_check_->setChecked(c["memory"].toBool());
    knowledge_check_->setChecked(c["knowledge"].toBool());
    guardrails_check_->setChecked(c["guardrails"].toBool());
    agentic_memory_check_->setChecked(c["agentic_memory"].toBool());
    storage_check_->setChecked(c["storage"].toBool());
    tracing_check_->setChecked(c["tracing"].toBool());
    compression_check_->setChecked(c["compression"].toBool());
    hooks_check_->setChecked(c["hooks"].toBool());
    evaluation_check_->setChecked(c["evaluation"].toBool());

    // Knowledge
    QJsonObject kc = c["knowledge_config"].toObject();
    int ki = knowledge_type_combo_->findText(kc["type"].toString());
    if (ki >= 0) knowledge_type_combo_->setCurrentIndex(ki);
    QJsonArray urls = kc["urls"].toArray();
    QStringList url_list;
    for (const auto& u : urls) url_list.append(u.toString());
    knowledge_urls_edit_->setPlainText(url_list.join("\n"));
    int vi = knowledge_vectordb_combo_->findText(kc["vector_db"].toString());
    if (vi >= 0) knowledge_vectordb_combo_->setCurrentIndex(vi);
    int ei = knowledge_embedder_combo_->findText(kc["embedder"].toString());
    if (ei >= 0) knowledge_embedder_combo_->setCurrentIndex(ei);

    // Guardrails
    QJsonObject gc = c["guardrails_config"].toObject();
    guardrails_pii_check_->setChecked(gc["pii_detection"].toBool());
    guardrails_injection_check_->setChecked(gc["injection_check"].toBool());
    guardrails_compliance_check_->setChecked(gc["financial_compliance"].toBool());

    // Memory config
    QJsonObject mc = c["memory_config"].toObject();
    memory_db_path_edit_->setText(mc["db_path"].toString());
    memory_table_edit_->setText(mc["table_name"].toString());
    memory_user_memories_check_->setChecked(mc["create_user_memories"].toBool());
    memory_session_summary_check_->setChecked(mc["create_session_summary"].toBool());

    // Storage config
    QJsonObject sc = c["storage_config"].toObject();
    int sti = storage_type_combo_->findText(sc["type"].toString());
    if (sti >= 0) storage_type_combo_->setCurrentIndex(sti);
    storage_db_path_edit_->setText(sc["db_path"].toString());
    storage_table_edit_->setText(sc["table_name"].toString());

    agentic_memory_user_id_edit_->setText(c["agentic_memory_user_id"].toString());

    status_label_->setText(QString("Loaded: %1").arg(cfg.name));
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::CYAN));
}

void CreateAgentPanel::clear_form() {
    editing_id_.clear();
    name_edit_->clear(); desc_edit_->clear(); category_combo_->setCurrentIndex(0);
    provider_combo_->setCurrentIndex(0); model_id_edit_->clear();
    temperature_spin_->setValue(0.7); max_tokens_spin_->setValue(4096);
    instructions_edit_->clear();
    for (int i = 0; i < tools_list_->count(); ++i) tools_list_->item(i)->setCheckState(Qt::Unchecked);
    reasoning_check_->setChecked(false); memory_check_->setChecked(false);
    knowledge_check_->setChecked(false); guardrails_check_->setChecked(false);
    agentic_memory_check_->setChecked(false); storage_check_->setChecked(false);
    tracing_check_->setChecked(false); compression_check_->setChecked(false);
    hooks_check_->setChecked(false); evaluation_check_->setChecked(false);
    knowledge_urls_edit_->clear(); memory_db_path_edit_->clear(); memory_table_edit_->clear();
    storage_db_path_edit_->clear(); storage_table_edit_->clear(); agentic_memory_user_id_edit_->clear();
    guardrails_pii_check_->setChecked(false); guardrails_injection_check_->setChecked(false);
    guardrails_compliance_check_->setChecked(false);
    memory_user_memories_check_->setChecked(false); memory_session_summary_check_->setChecked(false);
    test_result_->clear();
    status_label_->setText("Form cleared");
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::TEXT_TERTIARY));
}

QJsonObject CreateAgentPanel::build_config_json() const {
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
        if (tools_list_->item(i)->checkState() == Qt::Checked)
            tools.append(tools_list_->item(i)->text());
    config["tools"] = tools;

    config["reasoning"] = reasoning_check_->isChecked();
    config["memory"] = memory_check_->isChecked();
    config["knowledge"] = knowledge_check_->isChecked();
    config["guardrails"] = guardrails_check_->isChecked();
    config["agentic_memory"] = agentic_memory_check_->isChecked();
    config["storage"] = storage_check_->isChecked();
    config["tracing"] = tracing_check_->isChecked();
    config["compression"] = compression_check_->isChecked();
    config["hooks"] = hooks_check_->isChecked();
    config["evaluation"] = evaluation_check_->isChecked();

    if (knowledge_check_->isChecked()) {
        QJsonObject kc;
        kc["type"] = knowledge_type_combo_->currentText();
        QJsonArray urls;
        for (const auto& u : knowledge_urls_edit_->toPlainText().split('\n', Qt::SkipEmptyParts))
            urls.append(u.trimmed());
        kc["urls"] = urls;
        kc["vector_db"] = knowledge_vectordb_combo_->currentText();
        kc["embedder"] = knowledge_embedder_combo_->currentText();
        config["knowledge_config"] = kc;
    }
    if (guardrails_check_->isChecked()) {
        QJsonObject gc;
        gc["pii_detection"] = guardrails_pii_check_->isChecked();
        gc["injection_check"] = guardrails_injection_check_->isChecked();
        gc["financial_compliance"] = guardrails_compliance_check_->isChecked();
        config["guardrails_config"] = gc;
    }
    if (memory_check_->isChecked()) {
        QJsonObject mc;
        mc["db_path"] = memory_db_path_edit_->text();
        mc["table_name"] = memory_table_edit_->text();
        mc["create_user_memories"] = memory_user_memories_check_->isChecked();
        mc["create_session_summary"] = memory_session_summary_check_->isChecked();
        config["memory_config"] = mc;
    }
    if (storage_check_->isChecked()) {
        QJsonObject sc;
        sc["type"] = storage_type_combo_->currentText();
        sc["db_path"] = storage_db_path_edit_->text();
        sc["table_name"] = storage_table_edit_->text();
        config["storage_config"] = sc;
    }
    if (agentic_memory_check_->isChecked())
        config["agentic_memory_user_id"] = agentic_memory_user_id_edit_->text();

    return config;
}

void CreateAgentPanel::save_agent() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        status_label_->setText("Agent name is required");
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::NEGATIVE));
        return;
    }
    AgentConfig db;
    db.id = editing_id_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : editing_id_;
    db.name = name;
    db.description = desc_edit_->toPlainText().trimmed();
    db.category = category_combo_->currentText();
    db.config_json = QString::fromUtf8(QJsonDocument(build_config_json()).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(db);
    editing_id_ = db.id;
}

void CreateAgentPanel::delete_agent() {
    int row = saved_list_->currentRow();
    if (row >= 0 && row < saved_agents_.size())
        services::AgentService::instance().delete_config(saved_agents_[row].id);
}

void CreateAgentPanel::test_agent() {
    test_btn_->setEnabled(false);
    test_btn_->setText("RUNNING...");
    test_result_->clear();
    status_label_->setText("Testing agent...");
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::AMBER));
    services::AgentService::instance().run_agent("Hello, introduce yourself briefly.", build_config_json());
}

void CreateAgentPanel::export_json() {
    QString path = QFileDialog::getSaveFileName(this, "Export Agent Config", "agent_config.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QJsonObject out;
    out["name"] = name_edit_->text();
    out["description"] = desc_edit_->toPlainText();
    out["category"] = category_combo_->currentText();
    out["config"] = build_config_json();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    status_label_->setText("Exported to " + path);
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::POSITIVE));
}

void CreateAgentPanel::import_json() {
    QString path = QFileDialog::getOpenFileName(this, "Import Agent Config", {}, "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    AgentConfig cfg;
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg.name = obj["name"].toString();
    cfg.description = obj["description"].toString();
    cfg.category = obj["category"].toString("custom");
    cfg.config_json = QString::fromUtf8(QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact));
    load_agent_into_form(cfg);
    status_label_->setText("Imported from " + path);
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::CYAN));
}

void CreateAgentPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        load_saved_agents();
        services::AgentService::instance().list_tools();
        // Load MCP servers
        auto servers = McpServerRepository::instance().list_all();
        if (servers.is_ok()) {
            for (const auto& s : servers.value()) {
                auto* item = new QListWidgetItem(s.name);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, s.id);
                mcp_servers_list_->addItem(item);
            }
        }
    }
}

} // namespace fincept::screens
