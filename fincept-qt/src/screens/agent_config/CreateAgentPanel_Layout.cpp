// src/screens/agent_config/CreateAgentPanel_Layout.cpp
//
// UI construction: build_saved_panel (left list), build_form_panel (huge
// configuration form), build_test_panel (right side). Style helpers (input_style,
// combo_style, list_style, section_label, field_lbl) live here too — only this
// TU consumes them.
//
// Part of the partial-class split of CreateAgentPanel.cpp.

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "screens/agent_config/CreateAgentPanel.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"

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
#include <QStackedWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

using namespace fincept::ui::colors;

// ── Shared style helpers ─────────────────────────────────────────────────────

static QString input_style() {
    return QString("background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:" + fincept::ui::fonts::px(-2) + "px;")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED());
}

static QString combo_style() {
    return QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                   "QComboBox::drop-down{border:none;}"
                   "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;selection-color:%5;}")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), AMBER_DIM(), TEXT_ON_ACCENT());
}

static QString list_style() {
    return QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                   "QListWidget::item{padding:3px 6px;border-bottom:1px solid %2;}"
                   "QListWidget::item:selected{background:%4;color:%6;}"
                   "QListWidget::item:hover{background:%5;}")
        .arg(BG_BASE(), BORDER_DIM(), TEXT_PRIMARY(), AMBER_DIM(), BG_HOVER(), TEXT_ON_ACCENT());
}

static QLabel* section_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;"
                             "padding:8px 0 3px 0;border-bottom:1px solid %2;margin-bottom:2px;")
                         .arg(AMBER(), BORDER_DIM()));
    return l;
}

static QLabel* field_lbl(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;min-width:80px;").arg(TEXT_SECONDARY()));
    return l;
}

} // anonymous namespace

QWidget* CreateAgentPanel::build_saved_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(180);
    p->setMaximumWidth(260);
    // Scoped to the panel: a selector-less sheet drew its border on every child.
    p->setObjectName(QStringLiteral("createSavedPanel"));
    p->setAttribute(Qt::WA_StyledBackground, true);
    p->setStyleSheet(QString("QWidget#createSavedPanel{background:%1;border-right:1px solid %2;}")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header bar
    auto* hdr = new QWidget(this);
    hdr->setMinimumHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    saved_title_ = new QLabel(tr("SAVED AGENTS"));
    saved_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(saved_title_);
    hl->addStretch();
    saved_count_ = new QLabel("0");
    saved_count_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;padding:1px 6px;")
                                    .arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    hl->addWidget(saved_count_);
    vl->addWidget(hdr);

    // The saved list, or — with nothing saved yet — what to do about it.
    saved_stack_ = new QStackedWidget(p);
    saved_list_ = new QListWidget(saved_stack_);
    saved_list_->setStyleSheet(list_style());
    saved_stack_->addWidget(saved_list_);
    saved_empty_ = new ui::EmptyState(saved_stack_);
    saved_empty_->set_compact(true);
    saved_empty_->set_alignment(Qt::AlignTop);
    saved_stack_->addWidget(saved_empty_);
    connect(saved_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (saved_empty_->property("retry").toBool())
            load_saved_agents(); // RETRY after a read failure
        else
            import_json();
    });
    vl->addWidget(saved_stack_, 1);

    // Action bar
    auto* bar = new QWidget(this);
    bar->setMinimumHeight(36);
    bar->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8, 4, 8, 4);
    bl->setSpacing(4);

    auto make_bar_btn = [](const QString& text, const QString& col) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                 "padding:2px 8px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;}"
                                 "QPushButton:hover{background:%1;color:%2;}")
                             .arg(col, ui::colors::TEXT_ON_ACCENT()));
        return b;
    };

    load_btn_ = make_bar_btn(tr("LOAD"), ui::colors::AMBER);
    connect(load_btn_, &QPushButton::clicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
    bl->addWidget(load_btn_);

    del_btn_ = make_bar_btn(tr("DEL"), ui::colors::DANGER);
    connect(del_btn_, &QPushButton::clicked, this, &CreateAgentPanel::delete_agent);
    bl->addWidget(del_btn_);

    bl->addStretch();

    exp_btn_ = make_bar_btn(tr("EXP"), ui::colors::TEXT_SECONDARY);
    connect(exp_btn_, &QPushButton::clicked, this, &CreateAgentPanel::export_json);
    bl->addWidget(exp_btn_);

    imp_btn_ = make_bar_btn(tr("IMP"), ui::colors::TEXT_SECONDARY);
    connect(imp_btn_, &QPushButton::clicked, this, &CreateAgentPanel::import_json);
    bl->addWidget(imp_btn_);

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
    identity_hdr_ = section_label(tr("IDENTITY"));
    vl->addWidget(identity_hdr_);

    auto* r1 = new QHBoxLayout;
    r1->setSpacing(8);
    name_field_lbl_ = field_lbl(tr("Name"));
    r1->addWidget(name_field_lbl_);
    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText(tr("e.g. My Equity Analyst"));
    name_edit_->setStyleSheet(input_style());
    r1->addWidget(name_edit_, 1);
    category_field_lbl_ = field_lbl(tr("Category"));
    r1->addWidget(category_field_lbl_);
    category_combo_ = new QComboBox;
    for (const auto& c : {"general", "trader", "hedge-fund", "economic", "geopolitics", "research", "custom"})
        category_combo_->addItem(c);
    category_combo_->setStyleSheet(combo_style());
    r1->addWidget(category_combo_);
    vl->addLayout(r1);

    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlaceholderText(tr("Brief description of what this agent does..."));
    desc_edit_->setFixedHeight(52);
    desc_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(desc_edit_);

    // ── MODEL ────────────────────────────────────────────────────────────────
    model_hdr_ = section_label(tr("MODEL"));
    vl->addWidget(model_hdr_);

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip(tr("LLM profile for this agent. Configure profiles in Settings > LLM Config."));
    llm_profile_combo_->setStyleSheet(combo_style());
    vl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(llm_resolved_lbl_);

    // ── INSTRUCTIONS ─────────────────────────────────────────────────────────
    instructions_hdr_ = section_label(tr("INSTRUCTIONS"));
    vl->addWidget(instructions_hdr_);

    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText(tr("System prompt — role, goals, constraints, persona..."));
    instructions_edit_->setMinimumHeight(110);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(instructions_edit_);

    // ── TOOLS ────────────────────────────────────────────────────────────────
    tools_hdr_ = section_label(tr("TOOLS"));
    vl->addWidget(tools_hdr_);

    tool_search_edit_ = new QLineEdit;
    tool_search_edit_->setPlaceholderText(tr("Filter tools..."));
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
    terminal_tools_hdr_ = section_label(tr("TERMINAL TOOLS"));
    vl->addWidget(terminal_tools_hdr_);

    terminal_tools_check_ = new QCheckBox(tr("Enable internal terminal tools (markets, portfolio, news, edgar...)"));
    terminal_tools_check_->setChecked(true);
    terminal_tools_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    vl->addWidget(terminal_tools_check_);

    terminal_sub_ = new QWidget;
    auto* sub_vl = new QVBoxLayout(terminal_sub_);
    sub_vl->setContentsMargins(12, 4, 0, 4);
    sub_vl->setSpacing(4);

    // Style helpers — defined locally so we can reuse them across the
    // sub-section without polluting the function scope.
    const QString neutral_check_style =
        QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER());

    terminal_destructive_check_ = new QCheckBox(tr("Allow destructive tools (place_order, delete_*, set_*, file ops)"));
    // agents: grants this agent destructive terminal tools — a permission change
    terminal_destructive_check_->setProperty("fincept_agent_risk", "account");
    terminal_destructive_check_->setStyleSheet(
        QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;}"
                "QCheckBox::indicator{width:8px;height:8px;}"
                "QCheckBox::indicator:unchecked{background:%2;border:1px solid %3;}"
                "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
            .arg(ui::colors::DANGER(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::DANGER()));
    sub_vl->addWidget(terminal_destructive_check_);

    terminal_external_check_ =
        new QCheckBox(tr("Include external MCP servers (Notion, Slack, etc., from MCP Servers tab)"));
    terminal_external_check_->setChecked(true);
    terminal_external_check_->setStyleSheet(neutral_check_style);
    sub_vl->addWidget(terminal_external_check_);

    terminal_dry_run_check_ = new QCheckBox(tr("Dry-run mode (return synthetic results — no real execution)"));
    terminal_dry_run_check_->setStyleSheet(neutral_check_style);
    sub_vl->addWidget(terminal_dry_run_check_);

    terminal_cat_lbl_ = new QLabel(tr("Category whitelist (none checked = every tool an agent may use: reading the "
                                      "terminal's screens and running the bundled Python analytics)"));
    terminal_cat_lbl_->setWordWrap(true);
    terminal_cat_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    sub_vl->addWidget(terminal_cat_lbl_);

    terminal_categories_list_ = new QListWidget;
    terminal_categories_list_->setFixedHeight(110);
    terminal_categories_list_->setStyleSheet(list_style());
    // The core tool categories an agent may use (mcp/core/CoreTools.h). The
    // UI-driving ones ("ui-control", "navigation") are never available to
    // agents — they run in the background and must not move the user's screens.
    for (const auto& cat : {"ui-read", "analytics"}) {
        auto* item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        terminal_categories_list_->addItem(item);
    }
    sub_vl->addWidget(terminal_categories_list_);

    // Power-user filters — kept compact as single-line inputs. All optional.
    terminal_exclude_cats_edit_ = new QLineEdit;
    terminal_exclude_cats_edit_->setPlaceholderText(
        tr("Exclude categories (comma-separated, on top of UI-only defaults)"));
    terminal_exclude_cats_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_exclude_cats_edit_);

    terminal_name_include_edit_ = new QLineEdit;
    terminal_name_include_edit_->setPlaceholderText(tr("Tool name include regex (optional, e.g. ^get_)"));
    terminal_name_include_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_name_include_edit_);

    terminal_name_exclude_edit_ = new QLineEdit;
    terminal_name_exclude_edit_->setPlaceholderText(tr("Tool name exclude regex (optional, e.g. ^delete_)"));
    terminal_name_exclude_edit_->setStyleSheet(input_style());
    sub_vl->addWidget(terminal_name_exclude_edit_);

    auto* max_row = new QHBoxLayout;
    max_row->setSpacing(8);
    terminal_max_lbl_ = field_lbl(tr("Max tools for a whitelist / regex selection (0 = no cap)"));
    max_row->addWidget(terminal_max_lbl_);
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
    features_hdr_ = section_label(tr("FEATURES"));
    vl->addWidget(features_hdr_);

    // Tag-style checkboxes in a flow grid (2 columns)
    auto make_tag = [](const QString& label) -> QCheckBox* {
        auto* cb = new QCheckBox(label);
        cb->setStyleSheet(QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;padding:3px 6px;"
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
        *toggles[i].ptr = make_tag(tr(toggles[i].label));
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
        knowledge_type_lbl_ = field_lbl(tr("Type"));
        kr1->addWidget(knowledge_type_lbl_);
        knowledge_type_combo_ = new QComboBox;
        knowledge_type_combo_->addItems({"url", "pdf", "text", "combined"});
        knowledge_type_combo_->setStyleSheet(combo_style());
        kr1->addWidget(knowledge_type_combo_, 1);
        sl->addLayout(kr1);
        knowledge_urls_edit_ = new QPlainTextEdit;
        knowledge_urls_edit_->setPlaceholderText(tr("URLs (one per line)"));
        knowledge_urls_edit_->setFixedHeight(52);
        knowledge_urls_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
        sl->addWidget(knowledge_urls_edit_);
        auto* kr2 = new QHBoxLayout;
        knowledge_vectordb_lbl_ = field_lbl(tr("Vector DB"));
        kr2->addWidget(knowledge_vectordb_lbl_);
        knowledge_vectordb_combo_ = new QComboBox;
        knowledge_vectordb_combo_->addItems({"lancedb", "pgvector", "qdrant"});
        knowledge_vectordb_combo_->setStyleSheet(combo_style());
        kr2->addWidget(knowledge_vectordb_combo_, 1);
        knowledge_embedder_lbl_ = field_lbl(tr("Embedder"));
        kr2->addWidget(knowledge_embedder_lbl_);
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
            c->setStyleSheet(
                QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        guardrails_pii_check_ = make_sub_chk(tr("PII Detection"));
        guardrails_injection_check_ = make_sub_chk(tr("Injection Prevention"));
        guardrails_compliance_check_ = make_sub_chk(tr("Financial Compliance"));
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
        memory_dbpath_lbl_ = field_lbl(tr("DB Path"));
        mr1->addWidget(memory_dbpath_lbl_);
        memory_db_path_edit_ = new QLineEdit;
        memory_db_path_edit_->setPlaceholderText("agent_memory.db");
        memory_db_path_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_db_path_edit_, 1);
        memory_table_lbl_ = field_lbl(tr("Table"));
        mr1->addWidget(memory_table_lbl_);
        memory_table_edit_ = new QLineEdit;
        memory_table_edit_->setPlaceholderText("agent_memory");
        memory_table_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_table_edit_, 1);
        sl->addLayout(mr1);
        auto* mr2 = new QHBoxLayout;
        auto make_sub_chk = [](const QString& lbl) -> QCheckBox* {
            auto* c = new QCheckBox(lbl);
            c->setStyleSheet(
                QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        memory_user_memories_check_ = make_sub_chk(tr("User Memories"));
        memory_session_summary_check_ = make_sub_chk(tr("Session Summary"));
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
        storage_type_lbl_ = field_lbl(tr("Type"));
        sl->addWidget(storage_type_lbl_);
        storage_type_combo_ = new QComboBox;
        storage_type_combo_->addItems({"sqlite", "postgres"});
        storage_type_combo_->setStyleSheet(combo_style());
        sl->addWidget(storage_type_combo_);
        storage_dbpath_lbl_ = field_lbl(tr("DB Path"));
        sl->addWidget(storage_dbpath_lbl_);
        storage_db_path_edit_ = new QLineEdit;
        storage_db_path_edit_->setStyleSheet(input_style());
        sl->addWidget(storage_db_path_edit_, 1);
        storage_table_lbl_ = field_lbl(tr("Table"));
        sl->addWidget(storage_table_lbl_);
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
        agentic_userid_lbl_ = field_lbl(tr("User ID"));
        sl->addWidget(agentic_userid_lbl_);
        agentic_memory_user_id_edit_ = new QLineEdit;
        agentic_memory_user_id_edit_->setStyleSheet(input_style());
        sl->addWidget(agentic_memory_user_id_edit_, 1);
    }
    vl->addWidget(agentic_memory_sub_);

    // ── MCP SERVERS ──────────────────────────────────────────────────────────
    mcp_servers_hdr_ = section_label(tr("MCP SERVERS"));
    vl->addWidget(mcp_servers_hdr_);

    mcp_servers_list_ = new QListWidget;
    mcp_servers_list_->setFixedHeight(72);
    mcp_servers_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    mcp_servers_list_->setStyleSheet(list_style());
    vl->addWidget(mcp_servers_list_);

    // ── ACTIONS ──────────────────────────────────────────────────────────────
    vl->addSpacing(10);
    auto* act = new QHBoxLayout;
    act->setSpacing(6);

    save_btn_ = new QPushButton(tr("SAVE AGENT"));
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px 20px;"
                                     "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM()));
    act->addWidget(save_btn_);

    clear_btn_ = new QPushButton(tr("CLEAR"));
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                      "padding:8px 14px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;}"
                                      "QPushButton:hover{background:%3;}")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(clear_btn_, &QPushButton::clicked, this, &CreateAgentPanel::clear_form);
    act->addWidget(clear_btn_);

    act->addStretch();
    vl->addLayout(act);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:3px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(status_lbl_);
    vl->addStretch();

    // ── Accessibility ────────────────────────────────────────────────────────
    // The form is a long scroll of unlabelled inputs; screen readers announced
    // only the placeholder (or nothing, for the combos and lists).
    name_edit_->setAccessibleName(tr("Agent name"));
    category_combo_->setAccessibleName(tr("Agent category"));
    desc_edit_->setAccessibleName(tr("Agent description"));
    llm_profile_combo_->setAccessibleName(tr("LLM profile"));
    instructions_edit_->setAccessibleName(tr("System instructions"));
    tool_search_edit_->setAccessibleName(tr("Filter tools"));
    tools_list_->setAccessibleName(tr("Available tools"));
    terminal_tools_check_->setAccessibleName(tr("Enable internal terminal tools"));
    terminal_destructive_check_->setAccessibleName(tr("Allow destructive tools"));
    terminal_destructive_check_->setAccessibleDescription(
        tr("Lets this agent call tools that place orders, delete data or change settings."));
    terminal_external_check_->setAccessibleName(tr("Include external MCP servers"));
    terminal_dry_run_check_->setAccessibleName(tr("Dry-run mode"));
    terminal_categories_list_->setAccessibleName(tr("Terminal tool category whitelist"));
    terminal_exclude_cats_edit_->setAccessibleName(tr("Excluded tool categories"));
    terminal_name_include_edit_->setAccessibleName(tr("Tool name include pattern"));
    terminal_name_exclude_edit_->setAccessibleName(tr("Tool name exclude pattern"));
    terminal_max_tools_spin_->setAccessibleName(tr("Maximum tools"));
    mcp_servers_list_->setAccessibleName(tr("MCP servers"));
    save_btn_->setAccessibleName(tr("Save agent"));
    clear_btn_->setAccessibleName(tr("Clear form"));

    scroll->setWidget(content);

    // Explicit tab order — set after the final reparent so the focus chain is
    // built on the widgets' real hierarchy. Creation order alone puts the
    // terminal-tool filters between the feature tags and their sub-sections.
    setTabOrder(name_edit_, category_combo_);
    setTabOrder(category_combo_, desc_edit_);
    setTabOrder(desc_edit_, llm_profile_combo_);
    setTabOrder(llm_profile_combo_, instructions_edit_);
    setTabOrder(instructions_edit_, tool_search_edit_);
    setTabOrder(tool_search_edit_, tools_list_);
    setTabOrder(tools_list_, terminal_tools_check_);
    setTabOrder(terminal_tools_check_, terminal_destructive_check_);
    setTabOrder(terminal_destructive_check_, terminal_external_check_);
    setTabOrder(terminal_external_check_, terminal_dry_run_check_);
    setTabOrder(terminal_dry_run_check_, terminal_categories_list_);
    setTabOrder(terminal_categories_list_, terminal_exclude_cats_edit_);
    setTabOrder(terminal_exclude_cats_edit_, terminal_name_include_edit_);
    setTabOrder(terminal_name_include_edit_, terminal_name_exclude_edit_);
    setTabOrder(terminal_name_exclude_edit_, terminal_max_tools_spin_);
    setTabOrder(terminal_max_tools_spin_, reasoning_check_);
    setTabOrder(reasoning_check_, memory_check_);
    setTabOrder(memory_check_, knowledge_check_);
    setTabOrder(knowledge_check_, guardrails_check_);
    setTabOrder(guardrails_check_, agentic_memory_check_);
    setTabOrder(agentic_memory_check_, storage_check_);
    setTabOrder(storage_check_, tracing_check_);
    setTabOrder(tracing_check_, compression_check_);
    setTabOrder(compression_check_, hooks_check_);
    setTabOrder(hooks_check_, evaluation_check_);
    setTabOrder(evaluation_check_, mcp_servers_list_);
    setTabOrder(mcp_servers_list_, save_btn_);
    setTabOrder(save_btn_, clear_btn_);

    return scroll;
}

// ── Right: live test panel ───────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_test_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(260);
    p->setMaximumWidth(420);
    // Scoped to the panel: a selector-less sheet drew its border on every child.
    p->setObjectName(QStringLiteral("createTestPanel"));
    p->setAttribute(Qt::WA_StyledBackground, true);
    p->setStyleSheet(QString("QWidget#createTestPanel{background:%1;border-left:1px solid %2;}")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setMinimumHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    test_panel_hdr_ = new QLabel(tr("LIVE TEST"));
    test_panel_hdr_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::CYAN()));
    hl->addWidget(test_panel_hdr_);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 10, 10, 10);
    bl->setSpacing(6);

    test_query_hdr_ = new QLabel(tr("QUERY"));
    test_query_hdr_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(test_query_hdr_);

    test_query_edit_ = new QPlainTextEdit;
    test_query_edit_->setPlaceholderText(tr("Enter test query..."));
    test_query_edit_->setAccessibleName(tr("Test query"));
    test_query_edit_->setFixedHeight(72);
    test_query_edit_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:" + fincept::ui::fonts::px(-2) + "px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    bl->addWidget(test_query_edit_);

    test_btn_ = new QPushButton(tr("RUN TEST"));
    test_btn_->setProperty("fincept_agent_risk", "spend"); // agents: test-runs the agent (model quota)
    test_btn_->setCursor(Qt::PointingHandCursor);
    test_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                     "padding:7px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%1;color:%2;}"
                                     "QPushButton:disabled{color:%3;border-color:%3;}")
                                 .arg(ui::colors::CYAN(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_TERTIARY()));
    test_btn_->setAccessibleName(tr("Run test query"));
    connect(test_btn_, &QPushButton::clicked, this, &CreateAgentPanel::test_agent);
    bl->addWidget(test_btn_);

    test_status_lbl_ = new QLabel;
    test_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(test_status_lbl_);

    test_output_hdr_ = new QLabel(tr("OUTPUT"));
    test_output_hdr_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
            .arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(test_output_hdr_);

    // Output, or — before the first test — what a test run does. A run sends the
    // query to the LLM (tokens on the user's provider): it never starts by itself.
    test_stack_ = new QStackedWidget(body);
    test_empty_ = new ui::EmptyState(test_stack_);
    test_empty_->apply(ui::EmptyState::preview_spec(
        tr("Try the draft agent"),
        tr("Runs the query above against the agent as it is in the form — saved or not — and streams the answer "
           "here."),
        tr("RUN TEST"), tr("Uses your LLM provider — billed as tokens there"), ui::EmptyState::Tone::Warning,
        ui::LoadingSkeleton::Shape::Text));
    test_empty_->set_compact(true);
    connect(test_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (test_query_edit_->toPlainText().trimmed().isEmpty()) {
            test_query_edit_->setFocus();
            test_status_lbl_->setText(tr("Type a test query first"));
            return;
        }
        test_agent();
    });
    test_stack_->addWidget(test_empty_);
    test_result_ = new QTextEdit(test_stack_);
    test_result_->setReadOnly(true);
    test_result_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_BRIGHT()));
    test_stack_->addWidget(test_result_);
    test_stack_->setCurrentWidget(test_empty_);
    bl->addWidget(test_stack_, 1);

    vl->addWidget(body, 1);
    setTabOrder(test_query_edit_, test_btn_);
    return p;
}

// ── Connections ──────────────────────────────────────────────────────────────

} // namespace fincept::screens
