// src/screens/agent_config/CreateAgentPanel_Layout.cpp
//
// UI construction: build_saved_panel (left list), build_form_panel (huge
// configuration form), build_test_panel (right side). Style helpers (input_style,
// combo_style, list_style, section_label, field_lbl) live here too — only this
// TU consumes them.
//
// Part of the partial-class split of CreateAgentPanel.cpp.

#include "screens/agent_config/CreateAgentPanel.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

using namespace fincept::ui::colors;

// ── Shared style helpers ─────────────────────────────────────────────────────

static QString input_style() {
    return QString("background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:12px;")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED());
}

static QString combo_style() {
    return QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:12px;}"
                   "QComboBox::drop-down{border:none;}"
                   "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;}")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), AMBER_DIM());
}

static QString list_style() {
    return QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                   "QListWidget::item{padding:3px 6px;border-bottom:1px solid %2;}"
                   "QListWidget::item:selected{background:%4;color:%3;}"
                   "QListWidget::item:hover{background:%5;}")
        .arg(BG_BASE(), BORDER_DIM(), TEXT_PRIMARY(), AMBER_DIM(), BG_HOVER());
}

static QLabel* section_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                             "padding:8px 0 3px 0;border-bottom:1px solid %2;margin-bottom:2px;")
                         .arg(AMBER(), BORDER_DIM()));
    return l;
}

static QLabel* field_lbl(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:10px;min-width:80px;").arg(TEXT_SECONDARY()));
    return l;
}

} // anonymous namespace


QWidget* CreateAgentPanel::build_saved_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(180);
    p->setMaximumWidth(260);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header bar
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel("SAVED AGENTS");
    t->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(t);
    hl->addStretch();
    saved_count_ = new QLabel("0");
    saved_count_->setStyleSheet(
        QString("color:%1;font-size:10px;background:%2;padding:1px 6px;").arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    hl->addWidget(saved_count_);
    vl->addWidget(hdr);

    saved_list_ = new QListWidget;
    saved_list_->setStyleSheet(list_style());
    vl->addWidget(saved_list_, 1);

    // Action bar
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8, 4, 8, 4);
    bl->setSpacing(4);

    auto make_bar_btn = [](const QString& text, const QString& col) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                 "padding:2px 8px;font-size:9px;font-weight:700;}"
                                 "QPushButton:hover{background:%1;color:%2;}")
                             .arg(col, ui::colors::BG_BASE()));
        return b;
    };

    auto* load_btn = make_bar_btn("LOAD", ui::colors::AMBER);
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
    bl->addWidget(load_btn);

    auto* del_btn = make_bar_btn("DEL", ui::colors::NEGATIVE);
    connect(del_btn, &QPushButton::clicked, this, &CreateAgentPanel::delete_agent);
    bl->addWidget(del_btn);

    bl->addStretch();

    auto* exp_btn = make_bar_btn("EXP", ui::colors::TEXT_SECONDARY);
    connect(exp_btn, &QPushButton::clicked, this, &CreateAgentPanel::export_json);
    bl->addWidget(exp_btn);

    auto* imp_btn = make_bar_btn("IMP", ui::colors::TEXT_SECONDARY);
    connect(imp_btn, &QPushButton::clicked, this, &CreateAgentPanel::import_json);
    bl->addWidget(imp_btn);

    vl->addWidget(bar);
    return p;
}

// ── Center: config form ──────────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                  "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                                  "QScrollBar::handle:vertical{background:%2;min-height:20px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(14, 10, 14, 14);
    vl->setSpacing(3);

    // ── IDENTITY ─────────────────────────────────────────────────────────────
    vl->addWidget(section_label("IDENTITY"));

    auto* r1 = new QHBoxLayout;
    r1->setSpacing(8);
    r1->addWidget(field_lbl("Name"));
    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("e.g. My Equity Analyst");
    name_edit_->setStyleSheet(input_style());
    r1->addWidget(name_edit_, 1);
    r1->addWidget(field_lbl("Category"));
    category_combo_ = new QComboBox;
    for (const auto& c : {"general", "trader", "hedge-fund", "economic", "geopolitics", "research", "custom"})
        category_combo_->addItem(c);
    category_combo_->setStyleSheet(combo_style());
    r1->addWidget(category_combo_);
    vl->addLayout(r1);

    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlaceholderText("Brief description of what this agent does...");
    desc_edit_->setFixedHeight(52);
    desc_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(desc_edit_);

    // ── MODEL ────────────────────────────────────────────────────────────────
    vl->addWidget(section_label("MODEL"));

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip("LLM profile for this agent. Configure profiles in Settings > LLM Config.");
    llm_profile_combo_->setStyleSheet(combo_style());
    vl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(llm_resolved_lbl_);

    // ── INSTRUCTIONS ─────────────────────────────────────────────────────────
    vl->addWidget(section_label("INSTRUCTIONS"));

    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText("System prompt — role, goals, constraints, persona...");
    instructions_edit_->setMinimumHeight(110);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(instructions_edit_);

    // ── TOOLS ────────────────────────────────────────────────────────────────
    vl->addWidget(section_label("TOOLS"));

    tool_search_edit_ = new QLineEdit;
    tool_search_edit_->setPlaceholderText("Filter tools...");
    tool_search_edit_->setStyleSheet(input_style());
    vl->addWidget(tool_search_edit_);

    tools_list_ = new QListWidget;
    tools_list_->setFixedHeight(100);
    tools_list_->setStyleSheet(list_style());
    vl->addWidget(tools_list_);

    // ── TERMINAL TOOLS ───────────────────────────────────────────────────────
    // Internal Fincept MCP tools exposed via the local HTTP bridge. The toggle
    // controls whether the bridge endpoint + tool catalog get injected into
    // the agent config; the category list is a category-whitelist filter
    // (empty = include everything except the default exclusions); the
    // destructive toggle issues the capability token that lets the agent
    // call tools tagged is_destructive=true.
    vl->addWidget(section_label("TERMINAL TOOLS"));

    terminal_tools_check_ = new QCheckBox("Enable internal terminal tools (markets, portfolio, news, edgar...)");
    terminal_tools_check_->setChecked(true);
    terminal_tools_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    vl->addWidget(terminal_tools_check_);

    terminal_sub_ = new QWidget;
    auto* sub_vl = new QVBoxLayout(terminal_sub_);
    sub_vl->setContentsMargins(12, 4, 0, 4);
    sub_vl->setSpacing(4);

    // Style helpers — defined locally so we can reuse them across the
    // sub-section without polluting the function scope.
    const QString neutral_check_style =
        QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_DIM(), ui::colors::AMBER());

    terminal_destructive_check_ = new QCheckBox("Allow destructive tools (place_order, delete_*, set_*, file ops)");
    terminal_destructive_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_DIM(), ui::colors::NEGATIVE()));
    sub_vl->addWidget(terminal_destructive_check_);

    terminal_external_check_ = new QCheckBox(
        "Include external MCP servers (Notion, Slack, etc., from MCP Servers tab)");
    terminal_external_check_->setChecked(true);
    terminal_external_check_->setStyleSheet(neutral_check_style);
    sub_vl->addWidget(terminal_external_check_);

    terminal_dry_run_check_ = new QCheckBox(
        "Dry-run mode (return synthetic results — no real execution)");
    terminal_dry_run_check_->setStyleSheet(neutral_check_style);
    sub_vl->addWidget(terminal_dry_run_check_);

    auto* cat_lbl = new QLabel("Category whitelist (none checked = all enabled categories except UI-only)");
    cat_lbl->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    sub_vl->addWidget(cat_lbl);

    terminal_categories_list_ = new QListWidget;
    terminal_categories_list_->setFixedHeight(110);
    terminal_categories_list_->setStyleSheet(list_style());
    // Categories sourced from src/mcp/tools/*.cpp registrations. Excludes
    // UI-only categories (navigation/system/settings) and recursive ones
    // (ai-chat/meta) — agents shouldn't drive the UI or call the chat LLM.
    for (const auto& cat : {"markets", "watchlist", "news", "portfolio", "notes",
                             "crypto-trading", "paper-trading", "sec-edgar",
                             "ma-analytics", "alt-investments", "data-sources",
                             "forum", "profile", "file_manager", "report-builder",
                             "python", "datahub", "analytics"}) {
        auto* item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        terminal_categories_list_->addItem(item);
    }
    sub_vl->addWidget(terminal_categories_list_);

    // Power-user filters — kept compact as single-line inputs. All optional.
    terminal_exclude_cats_edit_ = new QLineEdit;
    terminal_exclude_cats_edit_->setPlaceholderText(
        "Exclude categories (comma-separated, on top of UI-only defaults)");
    terminal_exclude_cats_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_exclude_cats_edit_);

    terminal_name_include_edit_ = new QLineEdit;
    terminal_name_include_edit_->setPlaceholderText(
        "Tool name include regex (optional, e.g. ^get_)");
    terminal_name_include_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_name_include_edit_);

    terminal_name_exclude_edit_ = new QLineEdit;
    terminal_name_exclude_edit_->setPlaceholderText(
        "Tool name exclude regex (optional, e.g. ^delete_)");
    terminal_name_exclude_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_name_exclude_edit_);

    auto* max_row = new QHBoxLayout;
    max_row->setSpacing(8);
    max_row->addWidget(field_lbl("Max tools (0 = no cap)"));
    terminal_max_tools_spin_ = new QSpinBox;
    terminal_max_tools_spin_->setRange(0, 500);
    terminal_max_tools_spin_->setValue(0);
    terminal_max_tools_spin_->setStyleSheet(input_style());
    max_row->addWidget(terminal_max_tools_spin_);
    max_row->addStretch();
    sub_vl->addLayout(max_row);

    vl->addWidget(terminal_sub_);
    connect(terminal_tools_check_, &QCheckBox::toggled, terminal_sub_, &QWidget::setVisible);

    // ── FEATURES ─────────────────────────────────────────────────────────────
    vl->addWidget(section_label("FEATURES"));

    // Tag-style checkboxes in a flow grid (2 columns)
    auto make_tag = [](const QString& label) -> QCheckBox* {
        auto* cb = new QCheckBox(label);
        cb->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;padding:3px 6px;"
                                  "border:1px solid %2;background:%3;}"
                                  "QCheckBox:checked{color:%4;border-color:%5;background:%6;}"
                                  "QCheckBox::indicator{width:8px;height:8px;}"
                                  "QCheckBox::indicator:unchecked{background:%3;border:1px solid %2;}"
                                  "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                                   ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_HOVER()));
        return cb;
    };

    struct {
        QCheckBox** ptr;
        const char* label;
    } toggles[] = {
        {&reasoning_check_, "Reasoning"},
        {&memory_check_, "Memory"},
        {&knowledge_check_, "Knowledge"},
        {&guardrails_check_, "Guardrails"},
        {&agentic_memory_check_, "Agentic Memory"},
        {&storage_check_, "Storage"},
        {&tracing_check_, "Tracing"},
        {&compression_check_, "Compression"},
        {&hooks_check_, "Hooks"},
        {&evaluation_check_, "Evaluation"},
    };

    auto* tag_row1 = new QHBoxLayout;
    tag_row1->setSpacing(4);
    auto* tag_row2 = new QHBoxLayout;
    tag_row2->setSpacing(4);
    for (int i = 0; i < 10; ++i) {
        *toggles[i].ptr = make_tag(toggles[i].label);
        (i < 5 ? tag_row1 : tag_row2)->addWidget(*toggles[i].ptr);
    }
    tag_row1->addStretch();
    tag_row2->addStretch();
    vl->addLayout(tag_row1);
    vl->addLayout(tag_row2);

    // ── KNOWLEDGE sub ────────────────────────────────────────────────────────
    knowledge_sub_ = new QWidget(this);
    knowledge_sub_->setVisible(false);
    {
        auto* sl = new QVBoxLayout(knowledge_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(4);
        auto* kr1 = new QHBoxLayout;
        kr1->addWidget(field_lbl("Type"));
        knowledge_type_combo_ = new QComboBox;
        knowledge_type_combo_->addItems({"url", "pdf", "text", "combined"});
        knowledge_type_combo_->setStyleSheet(combo_style());
        kr1->addWidget(knowledge_type_combo_, 1);
        sl->addLayout(kr1);
        knowledge_urls_edit_ = new QPlainTextEdit;
        knowledge_urls_edit_->setPlaceholderText("URLs (one per line)");
        knowledge_urls_edit_->setFixedHeight(52);
        knowledge_urls_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
        sl->addWidget(knowledge_urls_edit_);
        auto* kr2 = new QHBoxLayout;
        kr2->addWidget(field_lbl("Vector DB"));
        knowledge_vectordb_combo_ = new QComboBox;
        knowledge_vectordb_combo_->addItems({"lancedb", "pgvector", "qdrant"});
        knowledge_vectordb_combo_->setStyleSheet(combo_style());
        kr2->addWidget(knowledge_vectordb_combo_, 1);
        kr2->addWidget(field_lbl("Embedder"));
        knowledge_embedder_combo_ = new QComboBox;
        knowledge_embedder_combo_->addItems({"openai", "ollama", "sentence-transformers"});
        knowledge_embedder_combo_->setStyleSheet(combo_style());
        kr2->addWidget(knowledge_embedder_combo_, 1);
        sl->addLayout(kr2);
    }
    vl->addWidget(knowledge_sub_);

    // ── GUARDRAILS sub ───────────────────────────────────────────────────────
    guardrails_sub_ = new QWidget(this);
    guardrails_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(guardrails_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(12);
        auto make_sub_chk = [](const QString& lbl) -> QCheckBox* {
            auto* c = new QCheckBox(lbl);
            c->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        guardrails_pii_check_ = make_sub_chk("PII Detection");
        guardrails_injection_check_ = make_sub_chk("Injection Prevention");
        guardrails_compliance_check_ = make_sub_chk("Financial Compliance");
        sl->addWidget(guardrails_pii_check_);
        sl->addWidget(guardrails_injection_check_);
        sl->addWidget(guardrails_compliance_check_);
        sl->addStretch();
    }
    vl->addWidget(guardrails_sub_);

    // ── MEMORY sub ───────────────────────────────────────────────────────────
    memory_sub_ = new QWidget(this);
    memory_sub_->setVisible(false);
    {
        auto* sl = new QVBoxLayout(memory_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(4);
        auto* mr1 = new QHBoxLayout;
        mr1->addWidget(field_lbl("DB Path"));
        memory_db_path_edit_ = new QLineEdit;
        memory_db_path_edit_->setPlaceholderText("agent_memory.db");
        memory_db_path_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_db_path_edit_, 1);
        mr1->addWidget(field_lbl("Table"));
        memory_table_edit_ = new QLineEdit;
        memory_table_edit_->setPlaceholderText("agent_memory");
        memory_table_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_table_edit_, 1);
        sl->addLayout(mr1);
        auto* mr2 = new QHBoxLayout;
        auto make_sub_chk = [](const QString& lbl) -> QCheckBox* {
            auto* c = new QCheckBox(lbl);
            c->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        memory_user_memories_check_ = make_sub_chk("User Memories");
        memory_session_summary_check_ = make_sub_chk("Session Summary");
        mr2->addWidget(memory_user_memories_check_);
        mr2->addWidget(memory_session_summary_check_);
        mr2->addStretch();
        sl->addLayout(mr2);
    }
    vl->addWidget(memory_sub_);

    // ── STORAGE sub ──────────────────────────────────────────────────────────
    storage_sub_ = new QWidget(this);
    storage_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(storage_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(8);
        sl->addWidget(field_lbl("Type"));
        storage_type_combo_ = new QComboBox;
        storage_type_combo_->addItems({"sqlite", "postgres"});
        storage_type_combo_->setStyleSheet(combo_style());
        sl->addWidget(storage_type_combo_);
        sl->addWidget(field_lbl("DB Path"));
        storage_db_path_edit_ = new QLineEdit;
        storage_db_path_edit_->setStyleSheet(input_style());
        sl->addWidget(storage_db_path_edit_, 1);
        sl->addWidget(field_lbl("Table"));
        storage_table_edit_ = new QLineEdit;
        storage_table_edit_->setStyleSheet(input_style());
        sl->addWidget(storage_table_edit_, 1);
    }
    vl->addWidget(storage_sub_);

    // ── AGENTIC MEMORY sub ───────────────────────────────────────────────────
    agentic_memory_sub_ = new QWidget(this);
    agentic_memory_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(agentic_memory_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(8);
        sl->addWidget(field_lbl("User ID"));
        agentic_memory_user_id_edit_ = new QLineEdit;
        agentic_memory_user_id_edit_->setStyleSheet(input_style());
        sl->addWidget(agentic_memory_user_id_edit_, 1);
    }
    vl->addWidget(agentic_memory_sub_);

    // ── MCP SERVERS ──────────────────────────────────────────────────────────
    vl->addWidget(section_label("MCP SERVERS"));

    mcp_servers_list_ = new QListWidget;
    mcp_servers_list_->setFixedHeight(72);
    mcp_servers_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    mcp_servers_list_->setStyleSheet(list_style());
    vl->addWidget(mcp_servers_list_);

    // ── ACTIONS ──────────────────────────────────────────────────────────────
    vl->addSpacing(10);
    auto* act = new QHBoxLayout;
    act->setSpacing(6);

    save_btn_ = new QPushButton("SAVE AGENT");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px 20px;"
                                     "font-size:11px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::ORANGE()));
    act->addWidget(save_btn_);

    auto* clr_btn = new QPushButton("CLEAR");
    clr_btn->setCursor(Qt::PointingHandCursor);
    clr_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                   "padding:8px 14px;font-size:10px;font-weight:600;}"
                                   "QPushButton:hover{background:%3;}")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(clr_btn, &QPushButton::clicked, this, &CreateAgentPanel::clear_form);
    act->addWidget(clr_btn);

    act->addStretch();
    vl->addLayout(act);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(status_lbl_);
    vl->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ── Right: live test panel ───────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_test_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(260);
    p->setMaximumWidth(420);
    p->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel("LIVE TEST");
    t->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::CYAN()));
    hl->addWidget(t);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 10, 10, 10);
    bl->setSpacing(6);

    auto* qlbl = new QLabel("QUERY");
    qlbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(qlbl);

    test_query_edit_ = new QPlainTextEdit;
    test_query_edit_->setPlaceholderText("Enter test query...");
    test_query_edit_->setFixedHeight(72);
    test_query_edit_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:12px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    bl->addWidget(test_query_edit_);

    test_btn_ = new QPushButton("RUN TEST");
    test_btn_->setCursor(Qt::PointingHandCursor);
    test_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                     "padding:7px;font-size:10px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%1;color:%2;}"
                                     "QPushButton:disabled{color:%3;border-color:%3;}")
                                 .arg(ui::colors::CYAN(), ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY()));
    connect(test_btn_, &QPushButton::clicked, this, &CreateAgentPanel::test_agent);
    bl->addWidget(test_btn_);

    test_status_lbl_ = new QLabel;
    test_status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(test_status_lbl_);

    auto* rlbl = new QLabel("OUTPUT");
    rlbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                            .arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(rlbl);

    test_result_ = new QTextEdit;
    test_result_->setReadOnly(true);
    test_result_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));
    bl->addWidget(test_result_, 1);

    vl->addWidget(body, 1);
    return p;
}

// ── Connections ──────────────────────────────────────────────────────────────

} // namespace fincept::screens
