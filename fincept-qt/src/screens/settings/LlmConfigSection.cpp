// LlmConfigSection.cpp — LLM provider configuration panel (Qt port)

//
// Core lifecycle: provider catalog statics (KNOWN_PROVIDERS, default_base_url,
// fallback_models), reload, build_ui (tab layout), and the small show_status
// helper. Tab implementations live in:
//   - LlmConfigSection_Providers.cpp — provider panel + save/test handlers
//   - LlmConfigSection_Profiles.cpp  — profile panel + CRUD handlers
#include "screens/settings/LlmConfigSection.h"

#include "services/llm/LlmService.h"
#include "core/logging/Logger.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

static constexpr const char* TAG = "LlmConfigSection";


const QStringList LlmConfigSection::KNOWN_PROVIDERS = {"openai",  "anthropic", "gemini",   "groq",  "deepseek",
                                                       "openrouter", "minimax", "kimi", "ollama", "xai",   "fincept"};

QString LlmConfigSection::default_base_url(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {}; // uses default
    if (p == "anthropic")
        return {};
    if (p == "gemini")
        return {};
    if (p == "groq")
        return {};
    if (p == "deepseek")
        return {};
    if (p == "openrouter")
        return {};
    if (p == "minimax")
        return "https://api.minimax.io/v1";
    if (p == "kimi")
        return {}; // defaults to https://api.moonshot.ai
    if (p == "ollama")
        return "http://localhost:11434";
    if (p == "xai")
        return {};
    if (p == "fincept")
        return {}; // endpoints are hardcoded in LlmService, no base_url needed
    return {};
}

QStringList LlmConfigSection::fallback_models(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini"};
    if (p == "anthropic")
        return {"claude-sonnet-4-5-20250514", "claude-opus-4-5", "claude-3-5-sonnet-20241022",
                "claude-3-haiku-20240307"};
    if (p == "gemini")
        return {"gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash"};
    if (p == "groq")
        return {"llama-3.3-70b-versatile", "mixtral-8x7b-32768", "gemma2-9b-it"};
    if (p == "deepseek")
        return {"deepseek-chat", "deepseek-reasoner"};
    if (p == "openrouter")
        return {"openai/gpt-4o", "anthropic/claude-sonnet-4-5", "google/gemini-2.5-flash"};
    if (p == "minimax")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    if (p == "kimi")
        return {"moonshot-v1-auto",
                "moonshot-v1-8k",
                "moonshot-v1-32k",
                "moonshot-v1-128k",
                "kimi-k2.5",
                "kimi-k2.6",
                "kimi-k2-thinking",
                "kimi-k2-thinking-turbo",
                "kimi-k2-0905-preview",
                "kimi-k2-turbo-preview",
                "kimi-k2-0711-preview",
                "moonshot-v1-8k-vision-preview",
                "moonshot-v1-32k-vision-preview",
                "moonshot-v1-128k-vision-preview"};
    if (p == "ollama")
        return {}; // Local provider — models fetched live from /api/tags. No fallback so the
                   // combo only shows what the user actually has installed locally.
    if (p == "xai")
        return {"grok-4-latest", "grok-4", "grok-3", "grok-3-mini"};
    if (p == "fincept")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    return {};
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
    auto* title_lbl = new QLabel("LLM CONFIGURATION");
    title_lbl->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;letter-spacing:1px;");
    tbl->addWidget(title_lbl);
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
    tab_widget_->addTab(build_providers_tab(), "PROVIDERS");
    tab_widget_->addTab(build_profiles_tab(), "PROFILES");
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
// Profiles tab — builder
// ============================================================================

} // namespace fincept::screens
