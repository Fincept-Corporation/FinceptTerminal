// src/screens/settings/LlmConfigSection_Providers.cpp
//
// Provider-tab implementation: build_providers_tab, the list/form/global
// panels, load_providers, populate_form, and all on_* slots for save / delete
// / test_connection / fetch_models.
//
// Part of the partial-class split of LlmConfigSection.cpp.

#include "screens/settings/LlmConfigSection.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

// Unity-build hygiene: this split TU shares an unstable unity bucket with
// LlmConfigSection.cpp (static TAG) and LlmConfigSection_Profiles.cpp
// (another anonymous-namespace TAG). Unique identifier prevents ambiguous
// lookup if all three land in one TU. Same convention applied across
// AiChatScreen_Sessions.cpp after a real-world collision surfaced.
namespace { constexpr const char* TAG_PROVIDERS = "LlmConfigSection"; }

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

QWidget* LlmConfigSection::build_providers_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->addWidget(build_provider_list_panel());
    splitter->addWidget(build_form_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 600});
    vl->addWidget(splitter, 1);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:" + QString(ui::colors::BORDER_DIM()) + ";");
    vl->addWidget(sep);
    vl->addWidget(build_global_panel());
    return w;
}

QWidget* LlmConfigSection::build_provider_list_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";border-right:1px solid " +
                         QString(ui::colors::BORDER_DIM()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* lbl = new QLabel("Providers");
    lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(lbl);

    provider_list_ = new QListWidget;
    provider_list_->setStyleSheet(
        "QListWidget{background:transparent;border:none;color:" + QString(ui::colors::TEXT_PRIMARY()) +
        ";}"
        "QListWidget::item{padding:8px 6px;border-radius:3px;}"
        "QListWidget::item:selected{background:" +
        QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";}"
        "QListWidget::item:hover{background:" +
        QString(ui::colors::BG_RAISED()) + ";}");
    connect(provider_list_, &QListWidget::currentRowChanged, this, &LlmConfigSection::on_provider_selected);
    vl->addWidget(provider_list_, 1);

    auto* btn_row = new QHBoxLayout;
    add_btn_ = new QPushButton("+ Add");
    add_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                            QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                            ";"
                            "border-radius:3px;padding:5px 10px;font-weight:600;}"
                            "QPushButton:hover{background:" +
                            QString(ui::colors::BG_RAISED()) + ";}");
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
    delete_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::NEGATIVE()) +
        ";border:1px solid " + QString(ui::colors::BG_RAISED()) +
        ";"
        "border-radius:3px;padding:5px 10px;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::BG_RAISED()) +
        ";}"
        "QPushButton:disabled{color:" +
        QString(ui::colors::BORDER_BRIGHT()) + ";border-color:" + QString(ui::colors::BORDER_MED()) + ";}");
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
    scroll->setStyleSheet("QScrollArea{background:" + QString(ui::colors::BG_BASE()) +
                          ";border:none;}"
                          "QScrollBar:vertical{background:" +
                          QString(ui::colors::BG_RAISED()) +
                          ";width:6px;}"
                          "QScrollBar::handle:vertical{background:" +
                          QString(ui::colors::BORDER_BRIGHT()) +
                          ";border-radius:3px;}"
                          "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* page = new QWidget(this);
    page->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";");
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(14);

    // Section title
    auto* form_title = new QLabel("Provider Configuration");
    form_title->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;");
    vl->addWidget(form_title);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:" + QString(ui::colors::BORDER_DIM()) + ";");
    vl->addWidget(sep);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto field_style = [](QLineEdit* le) {
        le->setStyleSheet("QLineEdit{background:" + QString(ui::colors::BG_RAISED()) +
                          ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                          QString(ui::colors::BORDER_MED()) +
                          ";"
                          "border-radius:3px;padding:6px;}"
                          "QLineEdit:focus{border:1px solid " +
                          QString(ui::colors::AMBER()) + ";}");
        le->setMinimumWidth(320);
    };
    auto lbl_style = [](QLabel* l) { l->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";"); };

    auto* p_lbl = new QLabel("Provider");
    lbl_style(p_lbl);
    provider_edit_ = new QLineEdit;
    provider_edit_->setPlaceholderText("e.g. openai");
    provider_edit_->setReadOnly(true); // set by selection
    field_style(provider_edit_);
    provider_edit_->setStyleSheet(provider_edit_->styleSheet() +
                                  "QLineEdit{background:" + QString(ui::colors::BG_SURFACE()) +
                                  ";color:" + QString(ui::colors::TEXT_TERTIARY()) + ";}");
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
    model_combo_->setStyleSheet("QComboBox{background:" + QString(ui::colors::BG_RAISED()) +
                                ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";"
                                "border-radius:3px;padding:6px;}"
                                "QComboBox:focus{border:1px solid " +
                                QString(ui::colors::AMBER()) +
                                ";}"
                                "QComboBox::drop-down{border:none;width:20px;}"
                                "QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
                                "border-right:4px solid transparent;border-top:5px solid " +
                                QString(ui::colors::TEXT_SECONDARY()) +
                                ";}"
                                "QComboBox QAbstractItemView{background:" +
                                QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::TEXT_PRIMARY()) +
                                ";"
                                "selection-background-color:" +
                                QString(ui::colors::BG_RAISED()) + ";selection-color:" + QString(ui::colors::AMBER()) +
                                ";"
                                "border:1px solid " +
                                QString(ui::colors::BORDER_MED()) + ";}");
    model_row->addWidget(model_combo_, 1);

    fetch_btn_ = new QPushButton("Fetch");
    fetch_btn_->setFixedHeight(30);
    fetch_btn_->setFixedWidth(60);
    fetch_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";border:1px solid " + QString(ui::colors::BORDER_BRIGHT()) +
        ";"
        "border-radius:3px;font-weight:600;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::BG_RAISED()) +
        ";}"
        "QPushButton:disabled{color:" +
        QString(ui::colors::TEXT_TERTIARY()) + ";border-color:" + QString(ui::colors::BORDER_MED()) + ";}");
    connect(fetch_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_fetch_models);
    model_row->addWidget(fetch_btn_);

    form->addRow(m_lbl, model_row);

    auto* b_lbl = new QLabel("Base URL");
    lbl_style(b_lbl);
    base_url_edit_ = new QLineEdit;
    base_url_edit_->setPlaceholderText("Optional — leave empty for default");
    field_style(base_url_edit_);
    form->addRow(b_lbl, base_url_edit_);

    // Tools toggle
    tools_check_ = new QCheckBox("Enable MCP Tools (navigation, market data, portfolio, etc.)");
    tools_check_->setChecked(true);
    tools_check_->setStyleSheet("QCheckBox{color:" + QString(ui::colors::TEXT_PRIMARY()) +
                                ";spacing:8px;}"
                                "QCheckBox::indicator{width:16px;height:16px;border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";border-radius:3px;background:" + QString(ui::colors::BG_RAISED()) +
                                ";}"
                                "QCheckBox::indicator:checked{background:" +
                                QString(ui::colors::AMBER()) + ";border-color:" + QString(ui::colors::AMBER()) + ";}");
    tools_check_->setToolTip("When enabled, the AI can interact with the terminal: navigate screens, fetch market "
                             "data, manage watchlists, etc.");
    form->addRow(new QLabel(""), tools_check_);

    vl->addLayout(form);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    save_btn_ = new QPushButton("Save & Set Active");
    save_btn_->setFixedHeight(34);
    save_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::AMBER()) + ";color:" + QString(ui::colors::BG_BASE()) +
        ";border:none;border-radius:4px;"
        "font-weight:700;padding:0 16px;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::AMBER_DIM()) +
        ";}"
        "QPushButton:disabled{background:" +
        QString(ui::colors::BORDER_BRIGHT()) + ";color:" + QString(ui::colors::TEXT_TERTIARY()) + ";}");
    connect(save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_provider);

    test_btn_ = new QPushButton("Test Connection");
    test_btn_->setFixedHeight(34);
    test_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) +
                             ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                             QString(ui::colors::BORDER_BRIGHT()) +
                             ";"
                             "border-radius:4px;padding:0 16px;}"
                             "QPushButton:hover{background:" +
                             QString(ui::colors::BG_HOVER()) + ";}");
    connect(test_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_test_connection);

    btn_row->addWidget(save_btn_);
    btn_row->addWidget(test_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("");
    status_lbl_->hide();
    vl->addWidget(status_lbl_);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* LlmConfigSection::build_global_panel() {
    auto* panel = new QWidget(this);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_SURFACE()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(24, 12, 24, 12);
    vl->setSpacing(10);

    auto* title = new QLabel("GLOBAL SETTINGS");
    title->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(title);

    auto* row = new QHBoxLayout;
    row->setSpacing(24);

    // Temperature
    auto* temp_grp = new QVBoxLayout;
    auto* temp_lbl = new QLabel("Temperature");
    temp_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    temp_spin_ = new QDoubleSpinBox;
    temp_spin_->setRange(0.0, 2.0);
    temp_spin_->setSingleStep(0.05);
    temp_spin_->setValue(0.7);
    temp_spin_->setDecimals(2);
    temp_spin_->setFixedWidth(100);
    temp_spin_->setStyleSheet("QDoubleSpinBox{background:" + QString(ui::colors::BG_RAISED()) +
                              ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                              QString(ui::colors::BORDER_MED()) +
                              ";"
                              "border-radius:3px;padding:4px;}");
    temp_grp->addWidget(temp_lbl);
    temp_grp->addWidget(temp_spin_);
    row->addLayout(temp_grp);

    // Max tokens
    auto* tok_grp = new QVBoxLayout;
    auto* tok_lbl = new QLabel("Max Tokens");
    tok_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    tokens_spin_ = new QSpinBox;
    tokens_spin_->setRange(100, 32000);
    tokens_spin_->setSingleStep(100);
    tokens_spin_->setValue(4096);
    tokens_spin_->setFixedWidth(100);
    tokens_spin_->setStyleSheet("QSpinBox{background:" + QString(ui::colors::BG_RAISED()) +
                                ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";"
                                "border-radius:3px;padding:4px;}");
    tok_grp->addWidget(tok_lbl);
    tok_grp->addWidget(tokens_spin_);
    row->addLayout(tok_grp);

    // System prompt
    auto* sp_grp = new QVBoxLayout;
    auto* sp_lbl = new QLabel("System Prompt");
    sp_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    system_prompt_ = new QPlainTextEdit;
    system_prompt_->setPlaceholderText("Optional system prompt for the LLM...");
    system_prompt_->setFixedHeight(60);
    system_prompt_->setStyleSheet("QPlainTextEdit{background:" + QString(ui::colors::BG_RAISED()) +
                                  ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                  QString(ui::colors::BORDER_MED()) +
                                  ";"
                                  "border-radius:3px;padding:4px;}"
                                  "QPlainTextEdit:focus{border:1px solid " +
                                  QString(ui::colors::AMBER()) + ";}");
    sp_grp->addWidget(sp_lbl);
    sp_grp->addWidget(system_prompt_);
    row->addLayout(sp_grp, 1);

    vl->addLayout(row);

    save_global_btn_ = new QPushButton("Save Global Settings");
    save_global_btn_->setFixedHeight(30);
    save_global_btn_->setFixedWidth(180);
    save_global_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                                    QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                                    ";"
                                    "border-radius:3px;font-weight:600;}"
                                    "QPushButton:hover{background:" +
                                    QString(ui::colors::BG_RAISED()) + ";}");
    connect(save_global_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_global);
    vl->addWidget(save_global_btn_);

    return panel;
}

// ============================================================================
// Data Loading
// ============================================================================

void LlmConfigSection::load_providers() {
    // Remember the currently-selected provider so a reload (e.g. after Save)
    // doesn't snap the user back to the first row. Fall back to the active
    // provider on first load, then row 0 if nothing else applies.
    QString prior_selection;
    if (provider_list_->currentItem())
        prior_selection = provider_list_->currentItem()->data(Qt::UserRole).toString();

    provider_list_->blockSignals(true);
    provider_list_->clear();

    QString active_provider;
    auto result = LlmConfigRepository::instance().list_providers();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            bool is_fincept = (p.provider.toLower() == "fincept");
            QString display = is_fincept ? "Fincept LLM" : p.provider;
            if (p.is_active) {
                display += "  ✓";
                active_provider = p.provider;
            }
            // Show model tag only for non-fincept providers
            if (!is_fincept && !p.model.isEmpty())
                display += "  [" + p.model + "]";
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.provider);
            // Only the active provider gets amber color
            if (p.is_active)
                item->setForeground(QColor(ui::colors::AMBER()));
            else
                item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
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

    // Selection precedence: prior selection (preserves UX across Save reloads)
    // > active provider (so first load lands on what the user is actually using)
    // > row 0 (fallback).
    int target_row = -1;
    const QString wanted = !prior_selection.isEmpty() ? prior_selection : active_provider;
    if (!wanted.isEmpty()) {
        for (int i = 0; i < provider_list_->count(); ++i) {
            if (provider_list_->item(i)->data(Qt::UserRole).toString() == wanted) {
                target_row = i;
                break;
            }
        }
    }
    if (target_row < 0 && provider_list_->count() > 0)
        target_row = 0;
    if (target_row >= 0)
        provider_list_->setCurrentRow(target_row);
}

void LlmConfigSection::populate_form(const QString& provider) {
    provider_edit_->setText(provider);

    bool is_fincept = (provider.toLower() == "fincept");
    bool is_ollama = (provider.toLower() == "ollama");

    // Populate model combo with fallback suggestions (empty for ollama — see fallback_models).
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

            // Tools toggle — always visible
            tools_check_->setChecked(p.tools_enabled);
            tools_check_->setVisible(true);

            if (is_fincept) {
                api_key_edit_->clear();
                auto stored = SettingsRepository::instance().get("fincept_api_key");
                if (stored.is_ok() && !stored.value().isEmpty()) {
                    QString masked = stored.value().left(8) + "...";
                    api_key_edit_->setPlaceholderText("Linked to your Fincept account: " + masked);
                } else {
                    api_key_edit_->setPlaceholderText("Login to your Fincept account to enable");
                }
                api_key_edit_->setEnabled(false);
                // Fincept is a managed service — hide model/base_url/fetch
                model_combo_->setVisible(false);
                fetch_btn_->setVisible(false);
                base_url_edit_->setVisible(false);
            } else if (is_ollama) {
                // Local provider — no API key needed. Mark the field clearly so users
                // don't think it's broken or required.
                api_key_edit_->clear();
                api_key_edit_->setEnabled(false);
                api_key_edit_->setPlaceholderText("Not required — local provider");
                model_combo_->setVisible(true);
                model_combo_->setEnabled(true);
                fetch_btn_->setVisible(true);
                fetch_btn_->setEnabled(true);
                base_url_edit_->setVisible(true);
                base_url_edit_->setEnabled(true);
                // Auto-fetch installed models from the local Ollama server so the
                // combo only shows what the user actually has, not hardcoded guesses.
                QTimer::singleShot(0, this, [this]() { on_fetch_models(); });
            } else {
                api_key_edit_->setText(p.api_key);
                api_key_edit_->setEnabled(true);
                api_key_edit_->setPlaceholderText("sk-...");
                model_combo_->setVisible(true);
                model_combo_->setEnabled(true);
                fetch_btn_->setVisible(true);
                fetch_btn_->setEnabled(true);
                base_url_edit_->setVisible(true);
                base_url_edit_->setEnabled(true);
            }
            return;
        }
    }

    // New provider — clear form
    api_key_edit_->clear();
    api_key_edit_->setEnabled(!is_fincept && !is_ollama);
    if (is_fincept) {
        auto stored = SettingsRepository::instance().get("fincept_api_key");
        if (stored.is_ok() && !stored.value().isEmpty())
            api_key_edit_->setPlaceholderText("Linked to your Fincept account: " + stored.value().left(8) + "...");
        else
            api_key_edit_->setPlaceholderText("Login to your Fincept account to enable");
        model_combo_->setVisible(false);
        fetch_btn_->setVisible(false);
        base_url_edit_->setVisible(false);
    } else if (is_ollama) {
        api_key_edit_->setPlaceholderText("Not required — local provider");
        model_combo_->setVisible(true);
        model_combo_->setEnabled(true);
        fetch_btn_->setVisible(true);
        fetch_btn_->setEnabled(true);
        base_url_edit_->setVisible(true);
        base_url_edit_->setEnabled(true);
        base_url_edit_->setText(def_url);
        QTimer::singleShot(0, this, [this]() { on_fetch_models(); });
    } else {
        api_key_edit_->setPlaceholderText("sk-...");
        model_combo_->setVisible(true);
        model_combo_->setEnabled(true);
        fetch_btn_->setVisible(true);
        fetch_btn_->setEnabled(true);
        base_url_edit_->setVisible(true);
        base_url_edit_->setEnabled(true);
        base_url_edit_->setText(def_url);
    }
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
    cfg.tools_enabled = tools_check_->isChecked();

    // Fincept defaults — endpoints are hardcoded in LlmService, base_url not needed
    if (is_fincept) {
        if (cfg.model.isEmpty())
            cfg.model = "MiniMax-M2.7";
        cfg.base_url = {}; // not used for fincept
    }

    // Basic validation
    if (!is_fincept && provider != "ollama" && cfg.api_key.isEmpty()) {
        show_status("API key is required for " + provider, true);
        return;
    }
    if (!is_fincept && cfg.model.isEmpty()) {
        show_status("Model name is required", true);
        return;
    }

    // Save first (INSERT OR REPLACE), THEN set active (deactivates others + activates this one)
    auto r2 = LlmConfigRepository::instance().save_provider(cfg);
    if (r2.is_err()) {
        show_status("Failed to save: " + QString::fromStdString(r2.error()), true);
        LOG_ERROR(TAG_PROVIDERS,"save_provider failed for " + provider + ": " + QString::fromStdString(r2.error()));
        return;
    }
    auto r3 = LlmConfigRepository::instance().set_active(provider);
    if (r3.is_err()) {
        show_status("Failed to activate: " + QString::fromStdString(r3.error()), true);
        LOG_ERROR(TAG_PROVIDERS,"set_active failed for " + provider + ": " + QString::fromStdString(r3.error()));
        return;
    }

    // Read-after-write verification — catches silent failures where the INSERT
    // reports success but the row never lands (e.g. autocommit off, FK rollback,
    // wrong DB path). Without this the old code would say "Saved" and the user
    // only finds out on restart.
    auto verify = LlmConfigRepository::instance().get_active_provider();
    if (!verify.is_ok() || verify.value().provider.toLower() != provider) {
        QString detail = verify.is_ok()
                             ? QString("active is '%1' not '%2'").arg(verify.value().provider, provider)
                             : QString::fromStdString(verify.error());
        show_status("Save verification failed: " + detail, true);
        LOG_ERROR(TAG_PROVIDERS,"Save verification failed — " + detail);
        return;
    }

    show_status("Saved and set as active provider", false);
    load_providers();
    ai_chat::LlmService::instance().reload_config();
    emit config_changed();

    LOG_INFO(TAG_PROVIDERS,"LLM provider saved and verified: " + provider + " / " + cfg.model);
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

    if (provider == "fincept") {
        // Fincept is a managed service — verify API key exists
        auto stored = SettingsRepository::instance().get("fincept_api_key");
        if (stored.is_ok() && !stored.value().isEmpty())
            show_status("Fincept connected — API key active", false);
        else
            show_status("Not connected — login to your Fincept account first", true);
        return;
    }

    if (provider != "ollama") {
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

    if (provider == "fincept") {
        show_status("Fincept manages models automatically", false);
        return;
    }

    if (provider != "ollama") {
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
        // Show the actual error so users can act on it (timeout, 401, host
        // not found, etc.) instead of guessing. For ollama add a hint about
        // the local server in case that's the cause.
        if (provider.toLower() == "ollama")
            show_status("Ollama fetch failed: " + error + " — check `ollama serve` and base URL", true);
        else
            show_status("Fetch failed: " + error, true);
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
    LOG_INFO(TAG_PROVIDERS,QString("Fetched %1 models for %2").arg(models.size()).arg(provider));
}

} // namespace fincept::screens
