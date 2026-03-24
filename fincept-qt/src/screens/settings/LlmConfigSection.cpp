// LlmConfigSection.cpp — LLM provider configuration panel (Qt port)

#include "screens/settings/LlmConfigSection.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"

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

const QStringList LlmConfigSection::KNOWN_PROVIDERS = {"openai",   "anthropic",  "gemini", "groq",
                                                       "deepseek", "openrouter", "ollama", "fincept"};

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
    if (p == "ollama")
        return "http://localhost:11434";
    if (p == "fincept")
        return "https://api.fincept.in/research/llm";
    return {};
}

QStringList LlmConfigSection::fallback_models(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini"};
    if (p == "anthropic")
        return {"claude-opus-4-6", "claude-sonnet-4-6", "claude-haiku-4-5-20251001"};
    if (p == "gemini")
        return {"gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash"};
    if (p == "groq")
        return {"llama-3.3-70b-versatile", "mixtral-8x7b-32768", "gemma2-9b-it"};
    if (p == "deepseek")
        return {"deepseek-chat", "deepseek-reasoner"};
    if (p == "openrouter")
        return {"openai/gpt-4o", "anthropic/claude-sonnet-4-6", "google/gemini-2.5-flash"};
    if (p == "ollama")
        return {"llama3:8b", "mistral:7b", "codellama:7b"};
    if (p == "fincept")
        return {"fincept-llm"};
    return {};
}

// ============================================================================
// Construction
// ============================================================================

LlmConfigSection::LlmConfigSection(QWidget* parent) : QWidget(parent) {
    build_ui();

    // Wire model fetch signal
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
            &LlmConfigSection::on_models_fetched);

    load_providers();
}

void LlmConfigSection::reload() {
    load_providers();
}

void LlmConfigSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget;
    title_bar->setFixedHeight(44);
    title_bar->setStyleSheet("background:#0d0d0d;border-bottom:1px solid #1e1e1e;");
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("LLM CONFIGURATION");
    title->setStyleSheet("color:#ff6600;font-size:13px;font-weight:700;letter-spacing:1px;");
    tbl->addWidget(title);
    tbl->addStretch();
    root->addWidget(title_bar);

    // Main area: left list + right form
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->addWidget(build_provider_list_panel());
    splitter->addWidget(build_form_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 600});
    root->addWidget(splitter, 1);

    // Global settings panel at bottom
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#1e1e1e;");
    root->addWidget(sep);
    root->addWidget(build_global_panel());
}

QWidget* LlmConfigSection::build_provider_list_panel() {
    auto* panel = new QWidget;
    panel->setFixedWidth(220);
    panel->setStyleSheet("background:#0a0a0a;border-right:1px solid #1e1e1e;");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* lbl = new QLabel("Providers");
    lbl->setStyleSheet("color:#888;font-size:11px;font-weight:700;letter-spacing:1px;");
    vl->addWidget(lbl);

    provider_list_ = new QListWidget;
    provider_list_->setStyleSheet("QListWidget{background:transparent;border:none;color:#ccc;font-size:12px;}"
                                  "QListWidget::item{padding:8px 6px;border-radius:3px;}"
                                  "QListWidget::item:selected{background:#1a1a1a;color:#ff6600;}"
                                  "QListWidget::item:hover{background:#111;}");
    connect(provider_list_, &QListWidget::currentRowChanged, this, &LlmConfigSection::on_provider_selected);
    vl->addWidget(provider_list_, 1);

    auto* btn_row = new QHBoxLayout;
    add_btn_ = new QPushButton("+ Add");
    add_btn_->setStyleSheet("QPushButton{background:#1a1a1a;color:#ff6600;border:1px solid #ff6600;"
                            "border-radius:3px;padding:5px 10px;font-size:11px;font-weight:600;}"
                            "QPushButton:hover{background:#2a1a0a;}");
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        // Show input dialog to pick provider
        QStringList choices = KNOWN_PROVIDERS;
        bool ok;
        QString provider = QInputDialog::getItem(this, "Add Provider", "Select provider:", choices, 0, false, &ok);
        if (!ok || provider.isEmpty())
            return;

        // Check if already exists
        auto existing = LlmConfigRepository::instance().list_providers();
        if (existing.is_ok()) {
            for (const auto& p : existing.value()) {
                if (p.provider.toLower() == provider.toLower()) {
                    show_status("Provider already configured", true);
                    return;
                }
            }
        }

        // Create new empty config
        LlmConfig cfg;
        cfg.provider = provider.toLower();
        cfg.model = "";
        cfg.is_active = false;
        LlmConfigRepository::instance().save_provider(cfg);
        load_providers();

        // Select newly added provider
        for (int i = 0; i < provider_list_->count(); ++i) {
            if (provider_list_->item(i)->data(Qt::UserRole).toString() == cfg.provider) {
                provider_list_->setCurrentRow(i);
                break;
            }
        }
    });

    delete_btn_ = new QPushButton("Remove");
    delete_btn_->setEnabled(false);
    delete_btn_->setStyleSheet("QPushButton{background:#1a0a0a;color:#cc3300;border:1px solid #330000;"
                               "border-radius:3px;padding:5px 10px;font-size:11px;}"
                               "QPushButton:hover{background:#2a1010;}"
                               "QPushButton:disabled{color:#333;border-color:#222;}");
    connect(delete_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_delete_provider);

    btn_row->addWidget(add_btn_);
    btn_row->addWidget(delete_btn_);
    vl->addLayout(btn_row);

    return panel;
}

QWidget* LlmConfigSection::build_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:#0a0a0a;border:none;}"
                          "QScrollBar:vertical{background:#111;width:6px;}"
                          "QScrollBar::handle:vertical{background:#333;border-radius:3px;}"
                          "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* page = new QWidget;
    page->setStyleSheet("background:#0a0a0a;");
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(14);

    // Section title
    auto* form_title = new QLabel("Provider Configuration");
    form_title->setStyleSheet("color:#ff6600;font-size:13px;font-weight:700;");
    vl->addWidget(form_title);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#1e1e1e;");
    vl->addWidget(sep);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto field_style = [](QLineEdit* le) {
        le->setStyleSheet("QLineEdit{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                          "border-radius:3px;padding:6px;font-size:12px;}"
                          "QLineEdit:focus{border:1px solid #ff6600;}");
        le->setMinimumWidth(320);
    };
    auto lbl_style = [](QLabel* l) { l->setStyleSheet("color:#888;font-size:12px;"); };

    auto* p_lbl = new QLabel("Provider");
    lbl_style(p_lbl);
    provider_edit_ = new QLineEdit;
    provider_edit_->setPlaceholderText("e.g. openai");
    provider_edit_->setReadOnly(true); // set by selection
    field_style(provider_edit_);
    provider_edit_->setStyleSheet(provider_edit_->styleSheet() + "QLineEdit{background:#0d0d0d;color:#666;}");
    form->addRow(p_lbl, provider_edit_);

    auto* k_lbl = new QLabel("API Key");
    lbl_style(k_lbl);
    api_key_edit_ = new QLineEdit;
    api_key_edit_->setPlaceholderText("sk-...");
    api_key_edit_->setEchoMode(QLineEdit::Password);
    field_style(api_key_edit_);
    form->addRow(k_lbl, api_key_edit_);

    auto* m_lbl = new QLabel("Model");
    lbl_style(m_lbl);
    auto* model_row = new QHBoxLayout;
    model_combo_ = new QComboBox;
    model_combo_->setEditable(true);
    model_combo_->setMinimumWidth(260);
    model_combo_->lineEdit()->setPlaceholderText("Select or type model...");
    model_combo_->setStyleSheet("QComboBox{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                                "border-radius:3px;padding:6px;font-size:12px;}"
                                "QComboBox:focus{border:1px solid #ff6600;}"
                                "QComboBox::drop-down{border:none;width:20px;}"
                                "QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
                                "border-right:4px solid transparent;border-top:5px solid #888;}"
                                "QComboBox QAbstractItemView{background:#111;color:#e0e0e0;"
                                "selection-background-color:#1a1a1a;selection-color:#ff6600;"
                                "border:1px solid #2a2a2a;}");
    model_row->addWidget(model_combo_, 1);

    fetch_btn_ = new QPushButton("Fetch");
    fetch_btn_->setFixedHeight(30);
    fetch_btn_->setFixedWidth(60);
    fetch_btn_->setStyleSheet("QPushButton{background:#1a1a1a;color:#ff6600;border:1px solid #333;"
                              "border-radius:3px;font-size:11px;font-weight:600;}"
                              "QPushButton:hover{background:#2a1a0a;}"
                              "QPushButton:disabled{color:#555;border-color:#222;}");
    connect(fetch_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_fetch_models);
    model_row->addWidget(fetch_btn_);

    form->addRow(m_lbl, model_row);

    auto* b_lbl = new QLabel("Base URL");
    lbl_style(b_lbl);
    base_url_edit_ = new QLineEdit;
    base_url_edit_->setPlaceholderText("Optional — leave empty for default");
    field_style(base_url_edit_);
    form->addRow(b_lbl, base_url_edit_);

    vl->addLayout(form);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    save_btn_ = new QPushButton("Save & Set Active");
    save_btn_->setFixedHeight(34);
    save_btn_->setStyleSheet("QPushButton{background:#ff6600;color:#000;border:none;border-radius:4px;"
                             "font-size:12px;font-weight:700;padding:0 16px;}"
                             "QPushButton:hover{background:#ff8800;}"
                             "QPushButton:disabled{background:#333;color:#666;}");
    connect(save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_provider);

    test_btn_ = new QPushButton("Test Connection");
    test_btn_->setFixedHeight(34);
    test_btn_->setStyleSheet("QPushButton{background:#1a1a1a;color:#ccc;border:1px solid #333;"
                             "border-radius:4px;font-size:12px;padding:0 16px;}"
                             "QPushButton:hover{background:#222;}");
    connect(test_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_test_connection);

    btn_row->addWidget(save_btn_);
    btn_row->addWidget(test_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("font-size:11px;");
    status_lbl_->hide();
    vl->addWidget(status_lbl_);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* LlmConfigSection::build_global_panel() {
    auto* panel = new QWidget;
    panel->setStyleSheet("background:#0d0d0d;");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(24, 12, 24, 12);
    vl->setSpacing(10);

    auto* title = new QLabel("GLOBAL SETTINGS");
    title->setStyleSheet("color:#ff6600;font-size:11px;font-weight:700;letter-spacing:1px;");
    vl->addWidget(title);

    auto* row = new QHBoxLayout;
    row->setSpacing(24);

    // Temperature
    auto* temp_grp = new QVBoxLayout;
    auto* temp_lbl = new QLabel("Temperature");
    temp_lbl->setStyleSheet("color:#888;font-size:11px;");
    temp_spin_ = new QDoubleSpinBox;
    temp_spin_->setRange(0.0, 2.0);
    temp_spin_->setSingleStep(0.05);
    temp_spin_->setValue(0.7);
    temp_spin_->setDecimals(2);
    temp_spin_->setFixedWidth(100);
    temp_spin_->setStyleSheet("QDoubleSpinBox{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                              "border-radius:3px;padding:4px;font-size:12px;}");
    temp_grp->addWidget(temp_lbl);
    temp_grp->addWidget(temp_spin_);
    row->addLayout(temp_grp);

    // Max tokens
    auto* tok_grp = new QVBoxLayout;
    auto* tok_lbl = new QLabel("Max Tokens");
    tok_lbl->setStyleSheet("color:#888;font-size:11px;");
    tokens_spin_ = new QSpinBox;
    tokens_spin_->setRange(100, 32000);
    tokens_spin_->setSingleStep(100);
    tokens_spin_->setValue(4096);
    tokens_spin_->setFixedWidth(100);
    tokens_spin_->setStyleSheet("QSpinBox{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                                "border-radius:3px;padding:4px;font-size:12px;}");
    tok_grp->addWidget(tok_lbl);
    tok_grp->addWidget(tokens_spin_);
    row->addLayout(tok_grp);

    // System prompt
    auto* sp_grp = new QVBoxLayout;
    auto* sp_lbl = new QLabel("System Prompt");
    sp_lbl->setStyleSheet("color:#888;font-size:11px;");
    system_prompt_ = new QPlainTextEdit;
    system_prompt_->setPlaceholderText("Optional system prompt for the LLM...");
    system_prompt_->setFixedHeight(60);
    system_prompt_->setStyleSheet("QPlainTextEdit{background:#111;color:#e0e0e0;border:1px solid #2a2a2a;"
                                  "border-radius:3px;padding:4px;font-size:12px;}"
                                  "QPlainTextEdit:focus{border:1px solid #ff6600;}");
    sp_grp->addWidget(sp_lbl);
    sp_grp->addWidget(system_prompt_);
    row->addLayout(sp_grp, 1);

    vl->addLayout(row);

    save_global_btn_ = new QPushButton("Save Global Settings");
    save_global_btn_->setFixedHeight(30);
    save_global_btn_->setFixedWidth(180);
    save_global_btn_->setStyleSheet("QPushButton{background:#1a1a1a;color:#ff6600;border:1px solid #ff6600;"
                                    "border-radius:3px;font-size:11px;font-weight:600;}"
                                    "QPushButton:hover{background:#2a1a0a;}");
    connect(save_global_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_global);
    vl->addWidget(save_global_btn_);

    return panel;
}

// ============================================================================
// Data Loading
// ============================================================================

void LlmConfigSection::load_providers() {
    provider_list_->blockSignals(true);
    provider_list_->clear();

    auto result = LlmConfigRepository::instance().list_providers();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            QString display = p.provider;
            if (p.is_active)
                display += "  ✓";
            if (!p.model.isEmpty())
                display += "  [" + p.model + "]";
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.provider);
            if (p.is_active)
                item->setForeground(QColor("#ff6600"));
            provider_list_->addItem(item);
        }
    }

    provider_list_->blockSignals(false);

    // Load global settings
    auto gs = LlmConfigRepository::instance().get_global_settings();
    if (gs.is_ok()) {
        temp_spin_->setValue(gs.value().temperature);
        tokens_spin_->setValue(gs.value().max_tokens);
        system_prompt_->setPlainText(gs.value().system_prompt);
    }

    delete_btn_->setEnabled(false);
    if (provider_list_->count() > 0)
        provider_list_->setCurrentRow(0);
}

void LlmConfigSection::populate_form(const QString& provider) {
    provider_edit_->setText(provider);

    bool is_fincept = (provider.toLower() == "fincept");

    // Populate model combo with fallback suggestions
    model_combo_->blockSignals(true);
    model_combo_->clear();
    model_combo_->addItems(fallback_models(provider));
    model_combo_->blockSignals(false);

    // Auto-fill base URL for known providers
    QString def_url = default_base_url(provider);

    auto result = LlmConfigRepository::instance().list_providers();
    if (result.is_err())
        return;

    for (const auto& p : result.value()) {
        if (p.provider.toLower() == provider.toLower()) {
            // Set model — try to select in combo, or set as editable text
            if (!p.model.isEmpty()) {
                int idx = model_combo_->findText(p.model);
                if (idx >= 0)
                    model_combo_->setCurrentIndex(idx);
                else
                    model_combo_->setCurrentText(p.model);
            }
            base_url_edit_->setText(p.base_url.isEmpty() ? def_url : p.base_url);

            if (is_fincept) {
                api_key_edit_->clear();
                auto stored = SettingsRepository::instance().get("fincept_api_key");
                if (stored.is_ok() && !stored.value().isEmpty()) {
                    QString masked = stored.value().left(8) + "...";
                    api_key_edit_->setPlaceholderText("From session: " + masked);
                } else {
                    api_key_edit_->setPlaceholderText("Login to auto-configure");
                }
                api_key_edit_->setEnabled(false);
                model_combo_->setEnabled(false);
                base_url_edit_->setEnabled(false);
                fetch_btn_->setEnabled(false);
            } else {
                api_key_edit_->setText(p.api_key);
                api_key_edit_->setEnabled(true);
                api_key_edit_->setPlaceholderText("sk-...");
                model_combo_->setEnabled(true);
                base_url_edit_->setEnabled(true);
                fetch_btn_->setEnabled(true);
            }
            return;
        }
    }

    // New provider — clear form
    api_key_edit_->clear();
    api_key_edit_->setEnabled(!is_fincept);
    base_url_edit_->setText(def_url);
    base_url_edit_->setEnabled(!is_fincept);
    model_combo_->setEnabled(!is_fincept);
    fetch_btn_->setEnabled(!is_fincept);
}

// ============================================================================
// Slots
// ============================================================================

void LlmConfigSection::on_provider_selected(int row) {
    if (row < 0 || row >= provider_list_->count()) {
        delete_btn_->setEnabled(false);
        return;
    }

    QString provider = provider_list_->item(row)->data(Qt::UserRole).toString();
    delete_btn_->setEnabled(provider.toLower() != "fincept");
    populate_form(provider);
}

void LlmConfigSection::on_save_provider() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("No provider selected", true);
        return;
    }

    bool is_fincept = (provider == "fincept");

    LlmConfig cfg;
    cfg.provider = provider;
    cfg.api_key = is_fincept ? QString() : api_key_edit_->text().trimmed();
    cfg.model = model_combo_->currentText().trimmed();
    cfg.base_url = base_url_edit_->text().trimmed();
    cfg.is_active = true;

    // Fincept defaults
    if (is_fincept) {
        if (cfg.model.isEmpty())
            cfg.model = "fincept-llm";
        if (cfg.base_url.isEmpty())
            cfg.base_url = "https://api.fincept.in/research/llm";
    }

    // Basic validation
    if (!is_fincept && provider != "ollama" && cfg.api_key.isEmpty()) {
        show_status("API key is required for " + provider, true);
        return;
    }
    if (cfg.model.isEmpty()) {
        show_status("Model name is required", true);
        return;
    }

    // Set this as active (deactivates others)
    LlmConfigRepository::instance().set_active(provider);
    auto r2 = LlmConfigRepository::instance().save_provider(cfg);

    if (r2.is_err()) {
        show_status("Failed to save: " + QString::fromStdString(r2.error()), true);
        return;
    }

    show_status("Saved and set as active provider", false);
    load_providers();
    emit config_changed();

    LOG_INFO(TAG, "LLM provider saved: " + provider + " / " + cfg.model);
}

void LlmConfigSection::on_delete_provider() {
    int row = provider_list_->currentRow();
    if (row < 0)
        return;

    QString provider = provider_list_->item(row)->data(Qt::UserRole).toString();

    if (provider.toLower() == "fincept") {
        show_status("Cannot remove built-in Fincept provider", true);
        return;
    }

    auto reply = QMessageBox::question(this, "Delete Provider", "Remove '" + provider + "' configuration?",
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    LlmConfigRepository::instance().delete_provider(provider);
    load_providers();
    emit config_changed();
}

void LlmConfigSection::on_save_global() {
    LlmGlobalSettings gs;
    gs.temperature = temp_spin_->value();
    gs.max_tokens = tokens_spin_->value();
    gs.system_prompt = system_prompt_->toPlainText().trimmed();

    auto r = LlmConfigRepository::instance().save_global_settings(gs);
    if (r.is_err()) {
        show_status("Failed to save global settings", true);
        return;
    }

    show_status("Global settings saved", false);
    emit config_changed();
}

void LlmConfigSection::on_test_connection() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("Select a provider first", true);
        return;
    }

    if (provider != "ollama" && provider != "fincept") {
        if (api_key_edit_->text().trimmed().isEmpty()) {
            show_status("API key required for test", true);
            return;
        }
    }

    show_status("Testing connection...", false);
    test_btn_->setEnabled(false);

    // Real test: fetch models list — if it succeeds, the connection works.
    // on_models_fetched will populate the combo; we just re-enable the button here.
    // Use a one-shot connection for the test status feedback.
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
                    [this, provider, conn](const QString& p, const QStringList& models, const QString& err) {
                        if (p.toLower() != provider.toLower())
                            return;
                        test_btn_->setEnabled(true);
                        if (err.isEmpty())
                            show_status("Connected — " + QString::number(models.size()) + " models available", false);
                        else
                            show_status("Connection failed: " + err, true);
                        disconnect(*conn);
                    });

    ai_chat::LlmService::instance().fetch_models(provider, api_key_edit_->text().trimmed(),
                                                 base_url_edit_->text().trimmed());
}

void LlmConfigSection::on_fetch_models() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("Select a provider first", true);
        return;
    }

    if (provider != "ollama" && provider != "fincept") {
        if (api_key_edit_->text().trimmed().isEmpty()) {
            show_status("Enter API key first, then fetch models", true);
            return;
        }
    }

    show_status("Fetching models...", false);
    fetch_btn_->setEnabled(false);

    ai_chat::LlmService::instance().fetch_models(provider, api_key_edit_->text().trimmed(),
                                                 base_url_edit_->text().trimmed());
}

void LlmConfigSection::on_models_fetched(const QString& provider, const QStringList& models, const QString& error) {
    fetch_btn_->setEnabled(true);

    // Only apply if still viewing this provider
    if (provider_edit_->text().trimmed().toLower() != provider.toLower())
        return;

    if (!error.isEmpty()) {
        show_status("Fetch failed: " + error + " — using suggestions", true);
        return;
    }

    // Save current selection
    QString current = model_combo_->currentText();

    model_combo_->blockSignals(true);
    model_combo_->clear();
    model_combo_->addItems(models);
    model_combo_->blockSignals(false);

    // Restore selection if still valid
    int idx = model_combo_->findText(current);
    if (idx >= 0)
        model_combo_->setCurrentIndex(idx);
    else if (!current.isEmpty())
        model_combo_->setCurrentText(current);
    else if (model_combo_->count() > 0)
        model_combo_->setCurrentIndex(0);

    show_status(QString::number(models.size()) + " models loaded for " + provider, false);
    LOG_INFO(TAG, QString("Fetched %1 models for %2").arg(models.size()).arg(provider));
}

void LlmConfigSection::show_status(const QString& msg, bool error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(error ? "color:#ff4444;font-size:11px;" : "color:#44cc44;font-size:11px;");
    status_lbl_->show();

    QTimer::singleShot(4000, status_lbl_, &QLabel::hide);
}

} // namespace fincept::screens
