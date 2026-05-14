// src/screens/agent_config/CreateAgentPanel_DataOps.cpp
//
// Persistence + JSON round-trip: load_saved_agents, load_agent_into_form,
// clear_form, build_config_json, save_agent, delete_agent, test_agent,
// export_json, import_json.
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

void CreateAgentPanel::load_saved_agents() {
    saved_list_->clear();
    auto result = AgentConfigRepository::instance().list_all();
    if (result.is_ok()) {
        saved_agents_ = result.value();
        for (const auto& a : saved_agents_)
            saved_list_->addItem(QString("%1  [%2]").arg(a.name, a.category));
        saved_count_->setText(QString::number(saved_agents_.size()));
    }
}

void CreateAgentPanel::load_agent_into_form(const AgentConfig& cfg) {
    editing_id_ = cfg.id;

    const QString assigned_id = LlmProfileRepository::instance().get_assignment("agent", cfg.id);
    llm_profile_combo_->blockSignals(true);
    int select_idx = 0;
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

    name_edit_->setText(cfg.name);
    desc_edit_->setPlainText(cfg.description);
    int ci = category_combo_->findText(cfg.category);
    if (ci >= 0)
        category_combo_->setCurrentIndex(ci);

    const QJsonObject c = QJsonDocument::fromJson(cfg.config_json.toUtf8()).object();
    instructions_edit_->setPlainText(c["instructions"].toString());

    QJsonArray ta = c["tools"].toArray();
    QSet<QString> selected;
    for (const auto& t : ta)
        selected.insert(t.toString());
    for (int i = 0; i < tools_list_->count(); ++i)
        tools_list_->item(i)->setCheckState(selected.contains(tools_list_->item(i)->text()) ? Qt::Checked
                                                                                            : Qt::Unchecked);

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

    QJsonObject kc = c["knowledge_config"].toObject();
    int ki = knowledge_type_combo_->findText(kc["type"].toString());
    if (ki >= 0)
        knowledge_type_combo_->setCurrentIndex(ki);
    QStringList url_list;
    for (const auto& u : kc["urls"].toArray())
        url_list.append(u.toString());
    knowledge_urls_edit_->setPlainText(url_list.join("\n"));
    int vi = knowledge_vectordb_combo_->findText(kc["vector_db"].toString());
    if (vi >= 0)
        knowledge_vectordb_combo_->setCurrentIndex(vi);
    int ei = knowledge_embedder_combo_->findText(kc["embedder"].toString());
    if (ei >= 0)
        knowledge_embedder_combo_->setCurrentIndex(ei);

    QJsonObject gc = c["guardrails_config"].toObject();
    guardrails_pii_check_->setChecked(gc["pii_detection"].toBool());
    guardrails_injection_check_->setChecked(gc["injection_check"].toBool());
    guardrails_compliance_check_->setChecked(gc["financial_compliance"].toBool());

    QJsonObject mc = c["memory_config"].toObject();
    memory_db_path_edit_->setText(mc["db_path"].toString());
    memory_table_edit_->setText(mc["table_name"].toString());
    memory_user_memories_check_->setChecked(mc["create_user_memories"].toBool());
    memory_session_summary_check_->setChecked(mc["create_session_summary"].toBool());

    QJsonObject sc = c["storage_config"].toObject();
    int sti = storage_type_combo_->findText(sc["type"].toString());
    if (sti >= 0)
        storage_type_combo_->setCurrentIndex(sti);
    storage_db_path_edit_->setText(sc["db_path"].toString());
    storage_table_edit_->setText(sc["table_name"].toString());

    agentic_memory_user_id_edit_->setText(c["agentic_memory_user_id"].toString());

    // Terminal MCP bridge — default ON if the key is absent (matches
    // build_payload's default-true semantics for legacy configs).
    const bool tt_enabled = c.contains("terminal_tools_enabled")
                                ? c["terminal_tools_enabled"].toBool()
                                : true;
    terminal_tools_check_->setChecked(tt_enabled);
    terminal_sub_->setVisible(tt_enabled);
    terminal_destructive_check_->setChecked(c["allow_destructive_tools"].toBool(false));
    // Default ON for legacy configs (key absent) — matches build_payload's
    // default-true semantics.
    terminal_external_check_->setChecked(c.contains("include_external_mcp")
                                              ? c["include_external_mcp"].toBool()
                                              : true);
    terminal_dry_run_check_->setChecked(c["tools_dry_run"].toBool(false));
    QSet<QString> cat_set;
    const QJsonObject tf = c["tool_filter"].toObject();
    for (const auto& v : tf["categories"].toArray())
        cat_set.insert(v.toString());
    for (int i = 0; i < terminal_categories_list_->count(); ++i) {
        auto* it = terminal_categories_list_->item(i);
        it->setCheckState(cat_set.contains(it->text()) ? Qt::Checked : Qt::Unchecked);
    }
    QStringList exc_cats;
    for (const auto& v : tf["exclude_categories"].toArray())
        exc_cats.append(v.toString());
    terminal_exclude_cats_edit_->setText(exc_cats.join(", "));
    const QJsonArray inc_pats = tf["name_patterns"].toArray();
    terminal_name_include_edit_->setText(inc_pats.isEmpty() ? QString() : inc_pats.first().toString());
    const QJsonArray exc_pats = tf["exclude_name_patterns"].toArray();
    terminal_name_exclude_edit_->setText(exc_pats.isEmpty() ? QString() : exc_pats.first().toString());
    terminal_max_tools_spin_->setValue(tf["max_tools"].toInt(0));

    status_lbl_->setText(QString("Loaded: %1").arg(cfg.name));
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::CYAN()));
}

void CreateAgentPanel::clear_form() {
    editing_id_.clear();
    name_edit_->clear();
    desc_edit_->clear();
    category_combo_->setCurrentIndex(0);
    instructions_edit_->clear();
    for (int i = 0; i < tools_list_->count(); ++i)
        tools_list_->item(i)->setCheckState(Qt::Unchecked);
    for (auto* cb : {reasoning_check_, memory_check_, knowledge_check_, guardrails_check_, agentic_memory_check_,
                     storage_check_, tracing_check_, compression_check_, hooks_check_, evaluation_check_})
        cb->setChecked(false);
    knowledge_urls_edit_->clear();
    memory_db_path_edit_->clear();
    memory_table_edit_->clear();
    storage_db_path_edit_->clear();
    storage_table_edit_->clear();
    agentic_memory_user_id_edit_->clear();
    guardrails_pii_check_->setChecked(false);
    guardrails_injection_check_->setChecked(false);
    guardrails_compliance_check_->setChecked(false);
    memory_user_memories_check_->setChecked(false);
    memory_session_summary_check_->setChecked(false);
    terminal_tools_check_->setChecked(true);
    terminal_sub_->setVisible(true);
    terminal_destructive_check_->setChecked(false);
    terminal_external_check_->setChecked(true);
    terminal_dry_run_check_->setChecked(false);
    for (int i = 0; i < terminal_categories_list_->count(); ++i)
        terminal_categories_list_->item(i)->setCheckState(Qt::Unchecked);
    terminal_exclude_cats_edit_->clear();
    terminal_name_include_edit_->clear();
    terminal_name_exclude_edit_->clear();
    terminal_max_tools_spin_->setValue(0);
    test_result_->clear();
    test_status_lbl_->clear();
    status_lbl_->setText("Form cleared");
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::TEXT_TERTIARY()));
}

QJsonObject CreateAgentPanel::build_config_json() const {
    QJsonObject config;

    const QString profile_id = llm_profile_combo_->currentData().toString();
    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        const auto pr3 = LlmProfileRepository::instance().list_profiles();
        const auto profiles3 = pr3.is_ok() ? pr3.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles3) {
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
    config["model"] = ai_chat::LlmService::profile_to_json(resolved);

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

    // Terminal MCP bridge — read by AgentService::build_payload to decide
    // whether to inject the bridge endpoint + tool catalog and whether to
    // grant the destructive capability token.
    config["terminal_tools_enabled"] = terminal_tools_check_->isChecked();
    config["allow_destructive_tools"] = terminal_destructive_check_->isChecked();
    config["include_external_mcp"] = terminal_external_check_->isChecked();
    config["tools_dry_run"] = terminal_dry_run_check_->isChecked();
    QJsonArray cat_whitelist;
    for (int i = 0; i < terminal_categories_list_->count(); ++i) {
        auto* it = terminal_categories_list_->item(i);
        if (it->checkState() == Qt::Checked)
            cat_whitelist.append(it->text());
    }
    // Comma-separated additional excludes — split, trim, drop empties.
    QJsonArray cat_blacklist;
    for (const auto& part : terminal_exclude_cats_edit_->text().split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            cat_blacklist.append(trimmed);
    }
    QJsonObject tf;
    if (!cat_whitelist.isEmpty())
        tf["categories"] = cat_whitelist;
    if (!cat_blacklist.isEmpty())
        tf["exclude_categories"] = cat_blacklist;
    const QString name_inc = terminal_name_include_edit_->text().trimmed();
    if (!name_inc.isEmpty())
        tf["name_patterns"] = QJsonArray{name_inc};
    const QString name_exc = terminal_name_exclude_edit_->text().trimmed();
    if (!name_exc.isEmpty())
        tf["exclude_name_patterns"] = QJsonArray{name_exc};
    if (terminal_max_tools_spin_->value() > 0)
        tf["max_tools"] = terminal_max_tools_spin_->value();
    if (!tf.isEmpty())
        config["tool_filter"] = tf;

    return config;
}

void CreateAgentPanel::save_agent() {
    const QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        status_lbl_->setText("Agent name is required");
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::NEGATIVE()));
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

    const QString profile_id = llm_profile_combo_->currentData().toString();
    auto& repo = LlmProfileRepository::instance();
    if (profile_id.isEmpty())
        repo.remove_assignment("agent", editing_id_);
    else
        repo.assign_profile("agent", editing_id_, profile_id);
}

void CreateAgentPanel::delete_agent() {
    int row = saved_list_->currentRow();
    if (row >= 0 && row < saved_agents_.size())
        services::AgentService::instance().delete_config(saved_agents_[row].id);
}

void CreateAgentPanel::test_agent() {
    const QString query = test_query_edit_->toPlainText().trimmed();
    if (query.isEmpty())
        return;
    test_btn_->setEnabled(false);
    test_btn_->setText("RUNNING...");
    test_result_->clear();
    test_status_lbl_->setText("Running...");
    test_status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER()));
    pending_request_id_ = services::AgentService::instance().run_agent_streaming(query, build_config_json());
}

void CreateAgentPanel::export_json() {
    const QString path =
        QFileDialog::getSaveFileName(this, "Export Agent Config", "agent_config.json", "JSON (*.json)");
    if (path.isEmpty())
        return;
    QJsonObject out;
    out["name"] = name_edit_->text();
    out["description"] = desc_edit_->toPlainText();
    out["category"] = category_combo_->currentText();
    out["config"] = build_config_json();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    status_lbl_->setText("Exported: " + path);
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::POSITIVE()));
}

void CreateAgentPanel::import_json() {
    const QString path = QFileDialog::getOpenFileName(this, "Import Agent Config", {}, "JSON (*.json)");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    AgentConfig cfg;
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg.name = obj["name"].toString();
    cfg.description = obj["description"].toString();
    cfg.category = obj["category"].toString("custom");
    cfg.config_json = QString::fromUtf8(QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact));
    load_agent_into_form(cfg);
    status_lbl_->setText("Imported from file");
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::CYAN()));
}

} // namespace fincept::screens
