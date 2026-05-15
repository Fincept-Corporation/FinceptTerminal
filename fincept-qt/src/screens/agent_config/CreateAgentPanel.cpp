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
        status_lbl_->setText("Saved successfully");
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::POSITIVE()));
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, [this]() {
        status_lbl_->setText("Agent deleted");
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
        test_btn_->setText("RUN TEST");
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(QString("Done in %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText("Error: " + r.error);
            test_status_lbl_->setText("Failed");
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
        }
    });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        pending_request_id_.clear();
        test_btn_->setEnabled(true);
        test_btn_->setText("RUN TEST");
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(QString("Done in %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText("Error: " + r.error);
            test_status_lbl_->setText("Failed");
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
    llm_profile_combo_->addItem("Default (Global)", QString{});

    const auto pr = LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<LlmProfile>{};
    for (const auto& p : profiles) {
        QString label = p.name;
        if (p.is_default)
            label += " [default]";
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
            text += " (inherited)";
        llm_resolved_lbl_->setText(text);
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        llm_resolved_lbl_->setText("No provider — Settings > LLM Config");
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

} // namespace fincept::screens
