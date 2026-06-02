// src/screens/agent_config/CreateAgentPanel.cpp
//
// Core lifecycle: ctor, build_ui, setup_connections, load_profile_combo,
// refresh_llm_pill, apply_tools_selection, showEvent. Other concerns:
//   - CreateAgentPanel_Layout.cpp   — build_saved/form/test panels + styles
//   - CreateAgentPanel_DataOps.cpp  — load/save/build_config_json/IO
#include "screens/agent_config/CreateAgentPanel.h"

#include "services/llm/LlmService.h"
#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

CreateAgentPanel::CreateAgentPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("CreateAgentPanel");
    build_ui();
    setup_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void CreateAgentPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));
    sp->addWidget(build_saved_panel());
    sp->addWidget(build_form_panel());
    sp->addWidget(build_test_panel());
    sp->setSizes({220, 480, 320});
    sp->setStretchFactor(0, 0);
    sp->setStretchFactor(1, 1);
    sp->setStretchFactor(2, 0);
    root->addWidget(sp);
}

// ── Left: saved agents ───────────────────────────────────────────────────────


void CreateAgentPanel::setup_connections() {
    connect(save_btn_, &QPushButton::clicked, this, &CreateAgentPanel::save_agent);

    // Feature toggle → show/hide sub-config panels
    connect(knowledge_check_, &QCheckBox::toggled, knowledge_sub_, &QWidget::setVisible);
    connect(guardrails_check_, &QCheckBox::toggled, guardrails_sub_, &QWidget::setVisible);
    connect(memory_check_, &QCheckBox::toggled, memory_sub_, &QWidget::setVisible);
    connect(storage_check_, &QCheckBox::toggled, storage_sub_, &QWidget::setVisible);
    connect(agentic_memory_check_, &QCheckBox::toggled, agentic_memory_sub_, &QWidget::setVisible);

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &CreateAgentPanel::load_profile_combo);
    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CreateAgentPanel::refresh_llm_pill);
    load_profile_combo();

    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::config_saved, this, [this]() {
        status_lbl_->setText(tr("Saved successfully"));
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::POSITIVE()));
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, [this]() {
        status_lbl_->setText(tr("Agent deleted"));
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::WARNING()));
        editing_id_.clear();
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_)
                    return;
                test_status_lbl_->setText(status);
            });
    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_)
                    return;
                QTextCursor cursor = test_result_->textCursor();
                cursor.movePosition(QTextCursor::End);
                test_result_->setTextCursor(cursor);
                test_result_->insertPlainText(token + " ");
            });
    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        pending_request_id_.clear();
        test_btn_->setEnabled(true);
        test_btn_->setText(tr("RUN TEST"));
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(tr("Done in %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText(tr("Error: %1").arg(r.error));
            test_status_lbl_->setText(tr("Failed"));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
        }
    });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        pending_request_id_.clear();
        test_btn_->setEnabled(true);
        test_btn_->setText(tr("RUN TEST"));
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(tr("Done in %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText(tr("Error: %1").arg(r.error));
            test_status_lbl_->setText(tr("Failed"));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
        }
    });
    connect(&svc, &services::AgentService::tools_loaded, this, [this](services::AgentToolsInfo info) {
        available_tools_.clear();
        for (const auto& cat : info.categories) {
            QJsonArray arr = info.tools[cat].toArray();
            for (const auto& t : arr)
                available_tools_.append(t.toString());
        }
        tools_list_->clear();
        for (const auto& t : available_tools_) {
            auto* item = new QListWidgetItem(t);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            tools_list_->addItem(item);
        }
    });
    connect(tool_search_edit_, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < tools_list_->count(); ++i) {
            auto* item = tools_list_->item(i);
            item->setHidden(!q.isEmpty() && !item->text().contains(q, Qt::CaseInsensitive));
        }
    });
    // Double-click in saved list loads immediately
    connect(saved_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
}

// ── Data helpers ─────────────────────────────────────────────────────────────

void CreateAgentPanel::load_profile_combo() {
    const QString prev_id = llm_profile_combo_->currentData().toString();
    llm_profile_combo_->blockSignals(true);
    llm_profile_combo_->clear();
    llm_profile_combo_->addItem(tr("Default (Global)"), QString{});

    const auto pr = LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<LlmProfile>{};
    for (const auto& p : profiles) {
        QString label = p.name;
        if (p.is_default)
            label += tr(" [default]");
        llm_profile_combo_->addItem(label, p.id);
    }
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
    refresh_llm_pill();
}

void CreateAgentPanel::refresh_llm_pill() {
    const QString profile_id = llm_profile_combo_->currentData().toString();
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
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", editing_id_);

    if (!resolved.provider.isEmpty()) {
        QString text = resolved.provider.toUpper() + " / " + resolved.model_id;
        if (text.length() > 55)
            text = text.left(53) + "..";
        if (profile_id.isEmpty())
            text += tr(" (inherited)");
        llm_resolved_lbl_->setText(text);
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        llm_resolved_lbl_->setText(tr("No provider — Settings > LLM Config"));
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::NEGATIVE()));
    }
}


void CreateAgentPanel::apply_tools_selection(const QStringList& tools) {
    for (int i = 0; i < tools_list_->count(); ++i) {
        auto* item = tools_list_->item(i);
        item->setCheckState(tools.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }
}

void CreateAgentPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        load_saved_agents();
        services::AgentService::instance().list_tools();
        auto servers = McpServerRepository::instance().list_all();
        if (servers.is_ok()) {
            mcp_servers_list_->clear();
            for (const auto& s : servers.value())
                mcp_servers_list_->addItem(s.name);
        }
    }
}

// ── Re-translation ───────────────────────────────────────────────────────────
// Covers widgets built in both this TU and the CreateAgentPanel_Layout.cpp split
// TU (members are shared via the class). The split TU has no own retranslateUi.

void CreateAgentPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CreateAgentPanel::retranslateUi() {
    // ── Left: saved agents ────────────────────────────────────────────────────
    if (saved_title_) saved_title_->setText(tr("SAVED AGENTS"));
    if (load_btn_)    load_btn_->setText(tr("LOAD"));
    if (del_btn_)     del_btn_->setText(tr("DEL"));
    if (exp_btn_)     exp_btn_->setText(tr("EXP"));
    if (imp_btn_)     imp_btn_->setText(tr("IMP"));

    // ── Center: form section headers ──────────────────────────────────────────
    if (identity_hdr_)       identity_hdr_->setText(tr("IDENTITY"));
    if (name_field_lbl_)     name_field_lbl_->setText(tr("Name"));
    if (category_field_lbl_) category_field_lbl_->setText(tr("Category"));
    if (model_hdr_)          model_hdr_->setText(tr("MODEL"));
    if (instructions_hdr_)   instructions_hdr_->setText(tr("INSTRUCTIONS"));
    if (tools_hdr_)          tools_hdr_->setText(tr("TOOLS"));
    if (terminal_tools_hdr_) terminal_tools_hdr_->setText(tr("TERMINAL TOOLS"));
    if (features_hdr_)       features_hdr_->setText(tr("FEATURES"));
    if (mcp_servers_hdr_)    mcp_servers_hdr_->setText(tr("MCP SERVERS"));

    // Placeholders + tooltips.
    if (name_edit_)         name_edit_->setPlaceholderText(tr("e.g. My Equity Analyst"));
    if (desc_edit_)         desc_edit_->setPlaceholderText(tr("Brief description of what this agent does..."));
    if (llm_profile_combo_) llm_profile_combo_->setToolTip(tr("LLM profile for this agent. Configure profiles in Settings > LLM Config."));
    if (instructions_edit_) instructions_edit_->setPlaceholderText(tr("System prompt — role, goals, constraints, persona..."));
    if (tool_search_edit_)  tool_search_edit_->setPlaceholderText(tr("Filter tools..."));

    // Terminal tools sub-section.
    if (terminal_tools_check_)
        terminal_tools_check_->setText(tr("Enable internal terminal tools (markets, portfolio, news, edgar...)"));
    if (terminal_destructive_check_)
        terminal_destructive_check_->setText(tr("Allow destructive tools (place_order, delete_*, set_*, file ops)"));
    if (terminal_external_check_)
        terminal_external_check_->setText(tr("Include external MCP servers (Notion, Slack, etc., from MCP Servers tab)"));
    if (terminal_dry_run_check_)
        terminal_dry_run_check_->setText(tr("Dry-run mode (return synthetic results — no real execution)"));
    if (terminal_cat_lbl_)
        terminal_cat_lbl_->setText(tr("Category whitelist (none checked = all enabled categories except UI-only)"));
    if (terminal_exclude_cats_edit_)
        terminal_exclude_cats_edit_->setPlaceholderText(tr("Exclude categories (comma-separated, on top of UI-only defaults)"));
    if (terminal_name_include_edit_)
        terminal_name_include_edit_->setPlaceholderText(tr("Tool name include regex (optional, e.g. ^get_)"));
    if (terminal_name_exclude_edit_)
        terminal_name_exclude_edit_->setPlaceholderText(tr("Tool name exclude regex (optional, e.g. ^delete_)"));
    if (terminal_max_lbl_)
        terminal_max_lbl_->setText(tr("Max tools (0 = no cap)"));

    // Feature toggles.
    if (reasoning_check_)      reasoning_check_->setText(tr("Reasoning"));
    if (memory_check_)         memory_check_->setText(tr("Memory"));
    if (knowledge_check_)      knowledge_check_->setText(tr("Knowledge"));
    if (guardrails_check_)     guardrails_check_->setText(tr("Guardrails"));
    if (agentic_memory_check_) agentic_memory_check_->setText(tr("Agentic Memory"));
    if (storage_check_)        storage_check_->setText(tr("Storage"));
    if (tracing_check_)        tracing_check_->setText(tr("Tracing"));
    if (compression_check_)    compression_check_->setText(tr("Compression"));
    if (hooks_check_)          hooks_check_->setText(tr("Hooks"));
    if (evaluation_check_)     evaluation_check_->setText(tr("Evaluation"));

    // Knowledge sub-fields.
    if (knowledge_type_lbl_)     knowledge_type_lbl_->setText(tr("Type"));
    if (knowledge_urls_edit_)    knowledge_urls_edit_->setPlaceholderText(tr("URLs (one per line)"));
    if (knowledge_vectordb_lbl_) knowledge_vectordb_lbl_->setText(tr("Vector DB"));
    if (knowledge_embedder_lbl_) knowledge_embedder_lbl_->setText(tr("Embedder"));

    // Guardrails sub-fields.
    if (guardrails_pii_check_)        guardrails_pii_check_->setText(tr("PII Detection"));
    if (guardrails_injection_check_)  guardrails_injection_check_->setText(tr("Injection Prevention"));
    if (guardrails_compliance_check_) guardrails_compliance_check_->setText(tr("Financial Compliance"));

    // Memory sub-fields.
    if (memory_dbpath_lbl_)          memory_dbpath_lbl_->setText(tr("DB Path"));
    if (memory_table_lbl_)           memory_table_lbl_->setText(tr("Table"));
    if (memory_user_memories_check_) memory_user_memories_check_->setText(tr("User Memories"));
    if (memory_session_summary_check_) memory_session_summary_check_->setText(tr("Session Summary"));

    // Storage sub-fields.
    if (storage_type_lbl_)   storage_type_lbl_->setText(tr("Type"));
    if (storage_dbpath_lbl_) storage_dbpath_lbl_->setText(tr("DB Path"));
    if (storage_table_lbl_)  storage_table_lbl_->setText(tr("Table"));

    // Agentic memory sub-field.
    if (agentic_userid_lbl_) agentic_userid_lbl_->setText(tr("User ID"));

    // Form actions (status_lbl_ holds live state — not re-applied).
    if (save_btn_)  save_btn_->setText(tr("SAVE AGENT"));
    if (clear_btn_) clear_btn_->setText(tr("CLEAR"));

    // ── Right: test panel ─────────────────────────────────────────────────────
    if (test_panel_hdr_)  test_panel_hdr_->setText(tr("LIVE TEST"));
    if (test_query_hdr_)  test_query_hdr_->setText(tr("QUERY"));
    if (test_output_hdr_) test_output_hdr_->setText(tr("OUTPUT"));
    if (test_query_edit_) test_query_edit_->setPlaceholderText(tr("Enter test query..."));
    // test_btn_ flips to "RUNNING..." mid-test — only re-apply the idle label.
    if (test_btn_ && pending_request_id_.isEmpty()) test_btn_->setText(tr("RUN TEST"));

    // The LLM profile combo / resolved pill re-derive their text from data.
    refresh_llm_pill();
}

} // namespace fincept::screens
