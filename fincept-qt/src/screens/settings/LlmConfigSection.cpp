// LlmConfigSection.cpp — LLM provider configuration panel (Qt port)

//
// Core lifecycle: provider catalog statics (KNOWN_PROVIDERS, default_base_url,
// fallback_models), reload, build_ui (tab layout), and the small show_status
// helper. Tab implementations live in:
//   - LlmConfigSection_Providers.cpp — provider panel + save/test handlers
//   - LlmConfigSection_Profiles.cpp  — profile panel + CRUD handlers
#include "screens/settings/LlmConfigSection.h"

#include "services/llm/LlmService.h"
#include "services/llm/ProviderCatalog.h"
#include "core/logging/Logger.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace fincept::screens {

[[maybe_unused]] static constexpr const char* TAG = "LlmConfigSection";


const QStringList LlmConfigSection::KNOWN_PROVIDERS = fincept::ai_chat::ProviderCatalog::known_providers();

QString LlmConfigSection::provider_display_name(const QString& provider_id) {
    return fincept::ai_chat::ProviderCatalog::display_name(provider_id);
}

QStringList LlmConfigSection::providers_sorted() {
    QStringList ids = KNOWN_PROVIDERS;
    std::sort(ids.begin(), ids.end(), [](const QString& a, const QString& b) {
        return provider_display_name(a).compare(provider_display_name(b), Qt::CaseInsensitive) < 0;
    });
    return ids;
}

QString LlmConfigSection::default_base_url(const QString& provider) {
    return fincept::ai_chat::ProviderCatalog::default_base_url(provider);
}

QStringList LlmConfigSection::fallback_models(const QString& provider) {
    return fincept::ai_chat::ProviderCatalog::fallback_models(provider);
}

// ============================================================================
// Construction
// ============================================================================

LlmConfigSection::LlmConfigSection(QWidget* parent) : QWidget(parent) {
    build_ui();

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
            &LlmConfigSection::on_models_fetched);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    });
    setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    load_providers();
    load_profiles();
}

void LlmConfigSection::reload() {
    load_providers();
    load_profiles();
}

void LlmConfigSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget(this);
    title_bar->setFixedHeight(44);
    title_bar->setStyleSheet("background:" + QString(ui::colors::BG_SURFACE()) + ";border-bottom:1px solid " +
                             QString(ui::colors::BORDER_DIM()) + ";");
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(16, 0, 16, 0);
    title_lbl_ = new QLabel(tr("LLM CONFIGURATION"));
    title_lbl_->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;letter-spacing:1px;");
    tbl->addWidget(title_lbl_);
    tbl->addStretch();
    root->addWidget(title_bar);

    tab_widget_ = new QTabWidget;
    tab_widget_->setDocumentMode(true);
    tab_widget_->setStyleSheet("QTabWidget::pane{border:none;background:" + QString(ui::colors::BG_BASE()) +
                               ";}"
                               "QTabBar::tab{background:" +
                               QString(ui::colors::BG_SURFACE()) + ";color:" + QString(ui::colors::TEXT_SECONDARY()) +
                               ";padding:8px 20px;"
                               "font-weight:600;letter-spacing:1px;border-bottom:2px solid transparent;}"
                               "QTabBar::tab:selected{color:" +
                               QString(ui::colors::AMBER()) + ";border-bottom:2px solid " +
                               QString(ui::colors::AMBER()) +
                               ";}"
                               "QTabBar::tab:hover{color:" +
                               QString(ui::colors::TEXT_PRIMARY()) + ";}");
    tab_widget_->addTab(build_providers_tab(), tr("PROVIDERS"));
    tab_widget_->addTab(build_profiles_tab(), tr("PROFILES"));
    root->addWidget(tab_widget_, 1);
}


void LlmConfigSection::show_status(const QString& msg, bool error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(error ? "color:" + QString(ui::colors::NEGATIVE()) + ";"
                                     : "color:" + QString(ui::colors::POSITIVE()) + ";");
    status_lbl_->show();
    QTimer::singleShot(4000, status_lbl_, &QLabel::hide);
}

// ============================================================================
// Retranslation — covers widgets built across all three TUs (this file +
// LlmConfigSection_Providers.cpp + LlmConfigSection_Profiles.cpp). The split
// TUs do not own a retranslateUi; this single override re-applies every fixed
// label there.
// ============================================================================

void LlmConfigSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void LlmConfigSection::retranslateUi() {
    // Title bar + tabs.
    if (title_lbl_) title_lbl_->setText(tr("LLM CONFIGURATION"));
    if (tab_widget_) {
        tab_widget_->setTabText(0, tr("PROVIDERS"));
        tab_widget_->setTabText(1, tr("PROFILES"));
    }

    // ── Providers tab ─────────────────────────────────────────────────────────
    if (provider_list_title_) provider_list_title_->setText(tr("Providers"));
    if (add_btn_)             add_btn_->setText(tr("+ Add"));
    if (delete_btn_)          delete_btn_->setText(tr("Remove"));

    if (form_title_)          form_title_->setText(tr("Provider Configuration"));
    if (provider_field_lbl_)  provider_field_lbl_->setText(tr("Provider"));
    if (api_key_field_lbl_)   api_key_field_lbl_->setText(tr("API Key"));
    if (model_field_lbl_)     model_field_lbl_->setText(tr("Model"));
    if (base_url_field_lbl_)  base_url_field_lbl_->setText(tr("Base URL"));

    if (provider_edit_)       provider_edit_->setPlaceholderText(tr("e.g. openai"));
    if (model_combo_ && model_combo_->lineEdit())
        model_combo_->lineEdit()->setPlaceholderText(tr("Select or type model..."));
    if (base_url_edit_)       base_url_edit_->setPlaceholderText(tr("Optional — leave empty for default"));

    if (fetch_btn_) fetch_btn_->setText(tr("Fetch"));
    if (tools_check_) {
        tools_check_->setText(tr("Enable MCP Tools (navigation, market data, portfolio, etc.)"));
        tools_check_->setToolTip(tr("When enabled, the AI can interact with the terminal: navigate screens, fetch market "
                                    "data, manage watchlists, etc."));
    }
    if (save_btn_) save_btn_->setText(tr("Save & Set Active"));
    if (test_btn_) test_btn_->setText(tr("Test Connection"));

    // Global settings panel.
    if (global_title_)      global_title_->setText(tr("GLOBAL SETTINGS"));
    if (temp_lbl_)          temp_lbl_->setText(tr("Temperature"));
    if (tokens_lbl_)        tokens_lbl_->setText(tr("Max Tokens"));
    if (tool_rounds_lbl_) {
        tool_rounds_lbl_->setText(tr("Max Tool Rounds"));
        tool_rounds_lbl_->setToolTip(tr("Ceiling on tool-call rounds per chat turn. Default 40.\n"
                                        "Range 1-200. Raise for long workflows (e.g. populating multi-section reports)."));
    }
    if (system_prompt_lbl_) system_prompt_lbl_->setText(tr("System Prompt"));
    if (system_prompt_)     system_prompt_->setPlaceholderText(tr("Optional system prompt for the LLM..."));
    if (save_global_btn_)   save_global_btn_->setText(tr("Save Global Settings"));

    // ── Profiles tab ──────────────────────────────────────────────────────────
    if (profile_list_title_) profile_list_title_->setText(tr("PROFILES"));
    if (profile_list_hint_)  profile_list_hint_->setText(tr("A profile = named LLM config you can assign to any agent or team."));
    if (profile_add_btn_)    profile_add_btn_->setText(tr("+ New"));
    if (profile_delete_btn_) profile_delete_btn_->setText(tr("Delete"));

    if (profile_name_field_lbl_)     profile_name_field_lbl_->setText(tr("PROFILE NAME"));
    if (profile_provider_field_lbl_) profile_provider_field_lbl_->setText(tr("PROVIDER"));
    if (profile_model_field_lbl_)    profile_model_field_lbl_->setText(tr("MODEL"));
    if (profile_api_key_field_lbl_)  profile_api_key_field_lbl_->setText(tr("API KEY"));
    if (profile_base_url_field_lbl_) profile_base_url_field_lbl_->setText(tr("BASE URL (custom endpoint)"));
    if (profile_temp_field_lbl_)     profile_temp_field_lbl_->setText(tr("TEMPERATURE"));
    if (profile_tokens_field_lbl_)   profile_tokens_field_lbl_->setText(tr("MAX TOKENS"));
    if (profile_prompt_field_lbl_)   profile_prompt_field_lbl_->setText(tr("SYSTEM PROMPT OVERRIDE (optional)"));

    if (profile_name_edit_)     profile_name_edit_->setPlaceholderText(tr("e.g. Fast Groq, Careful Claude, Coding minimax"));
    if (profile_api_key_edit_)  profile_api_key_edit_->setPlaceholderText(tr("Leave blank to inherit from provider"));
    if (profile_base_url_edit_) profile_base_url_edit_->setPlaceholderText(tr("Leave blank to use provider default"));
    if (profile_prompt_edit_)   profile_prompt_edit_->setPlaceholderText(tr("Leave blank to use global system prompt"));

    if (profile_save_btn_)    profile_save_btn_->setText(tr("SAVE PROFILE"));
    if (profile_default_btn_) profile_default_btn_->setText(tr("SET AS DEFAULT"));
}

} // namespace fincept::screens
