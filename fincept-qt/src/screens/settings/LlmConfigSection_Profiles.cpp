// src/screens/settings/LlmConfigSection_Profiles.cpp
//
// Profile-tab implementation: build_profiles_tab, list/form panels, load_profiles,
// populate_profile_form, clear_profile_form, and all on_profile_* slots.
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

// Unity-build hygiene: see LlmConfigSection_Providers.cpp for the rationale.
namespace { constexpr const char* TAG_PROFILES = "LlmConfigSection"; }

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

QWidget* LlmConfigSection::build_profiles_tab() {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle{background:" + QString(ui::colors::BORDER_DIM()) + ";}");
    splitter->addWidget(build_profile_list_panel());
    splitter->addWidget(build_profile_form_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 580});
    hl->addWidget(splitter);
    return w;
}

QWidget* LlmConfigSection::build_profile_list_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(240);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";border-right:1px solid " +
                         QString(ui::colors::BORDER_DIM()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* lbl = new QLabel("PROFILES");
    lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(lbl);

    auto* hint = new QLabel("A profile = named LLM config you can assign to any agent or team.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color:" + QString(ui::colors::TEXT_TERTIARY()) + ";padding-bottom:4px;");
    vl->addWidget(hint);

    profile_list_ = new QListWidget;
    profile_list_->setStyleSheet(
        "QListWidget{background:transparent;border:none;color:" + QString(ui::colors::TEXT_PRIMARY()) +
        ";}"
        "QListWidget::item{padding:8px 6px;border-radius:3px;}"
        "QListWidget::item:selected{background:" +
        QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";}"
        "QListWidget::item:hover{background:" +
        QString(ui::colors::BG_RAISED()) + ";}");
    connect(profile_list_, &QListWidget::currentRowChanged, this, &LlmConfigSection::on_profile_selected);
    vl->addWidget(profile_list_, 1);

    auto* btn_row = new QHBoxLayout;
    auto* add_btn = new QPushButton("+ New");
    add_btn->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                           QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                           ";"
                           "border-radius:3px;padding:5px 10px;font-weight:600;}"
                           "QPushButton:hover{background:" +
                           QString(ui::colors::BG_RAISED()) + ";}");
    connect(add_btn, &QPushButton::clicked, this, [this]() {
        editing_profile_id_.clear();
        clear_profile_form();
    });
    btn_row->addWidget(add_btn);

    profile_delete_btn_ = new QPushButton("Delete");
    profile_delete_btn_->setEnabled(false);
    profile_delete_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::NEGATIVE()) +
                                       ";border:1px solid " + QString(ui::colors::NEGATIVE()) +
                                       ";"
                                       "border-radius:3px;padding:5px 10px;}"
                                       "QPushButton:hover{background:" +
                                       QString(ui::colors::BG_RAISED()) +
                                       ";}"
                                       "QPushButton:disabled{color:" +
                                       QString(ui::colors::BORDER_BRIGHT()) +
                                       ";border-color:" + QString(ui::colors::BORDER_BRIGHT()) + ";}");
    connect(profile_delete_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_delete_profile);
    btn_row->addWidget(profile_delete_btn_);
    vl->addLayout(btn_row);

    return panel;
}

QWidget* LlmConfigSection::build_profile_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:" + QString(ui::colors::BG_BASE()) +
                          ";}"
                          "QScrollBar:vertical{width:6px;background:" +
                          QString(ui::colors::BG_SURFACE()) +
                          ";}"
                          "QScrollBar::handle:vertical{background:" +
                          QString(ui::colors::BORDER_BRIGHT()) + ";}");

    auto* panel = new QWidget(this);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);

    static auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
        return l;
    };
    static auto field_style = []() {
        return QString("background:" + QString(ui::colors::BG_RAISED()) +
                       ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                       QString(ui::colors::BORDER_MED()) + ";padding:6px 10px;");
    };

    vl->addWidget(lbl("PROFILE NAME"));
    profile_name_edit_ = new QLineEdit;
    profile_name_edit_->setPlaceholderText("e.g. Fast Groq, Careful Claude, Coding minimax");
    profile_name_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_name_edit_);

    vl->addWidget(lbl("PROVIDER"));
    profile_provider_combo_ = new QComboBox;
    profile_provider_combo_->addItems(KNOWN_PROVIDERS);
    profile_provider_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    connect(profile_provider_combo_, &QComboBox::currentTextChanged, this,
            &LlmConfigSection::on_profile_provider_changed);
    vl->addWidget(profile_provider_combo_);

    vl->addWidget(lbl("MODEL"));
    profile_model_combo_ = new QComboBox;
    profile_model_combo_->setEditable(true);
    profile_model_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    vl->addWidget(profile_model_combo_);

    vl->addWidget(lbl("API KEY"));
    profile_api_key_edit_ = new QLineEdit;
    profile_api_key_edit_->setEchoMode(QLineEdit::Password);
    profile_api_key_edit_->setPlaceholderText("Leave blank to inherit from provider");
    profile_api_key_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_api_key_edit_);

    vl->addWidget(lbl("BASE URL (custom endpoint)"));
    profile_base_url_edit_ = new QLineEdit;
    profile_base_url_edit_->setPlaceholderText("Leave blank to use provider default");
    profile_base_url_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_base_url_edit_);

    auto* param_row = new QHBoxLayout;
    auto* temp_col = new QVBoxLayout;
    temp_col->addWidget(lbl("TEMPERATURE"));
    profile_temp_spin_ = new QDoubleSpinBox;
    profile_temp_spin_->setRange(0.0, 2.0);
    profile_temp_spin_->setSingleStep(0.1);
    profile_temp_spin_->setValue(0.7);
    profile_temp_spin_->setStyleSheet(field_style());
    temp_col->addWidget(profile_temp_spin_);
    param_row->addLayout(temp_col);

    auto* tok_col = new QVBoxLayout;
    tok_col->addWidget(lbl("MAX TOKENS"));
    profile_tokens_spin_ = new QSpinBox;
    profile_tokens_spin_->setRange(256, 128000);
    profile_tokens_spin_->setSingleStep(256);
    profile_tokens_spin_->setValue(4096);
    profile_tokens_spin_->setStyleSheet(field_style());
    tok_col->addWidget(profile_tokens_spin_);
    param_row->addLayout(tok_col);
    vl->addLayout(param_row);

    vl->addWidget(lbl("SYSTEM PROMPT OVERRIDE (optional)"));
    profile_prompt_edit_ = new QPlainTextEdit;
    profile_prompt_edit_->setPlaceholderText("Leave blank to use global system prompt");
    profile_prompt_edit_->setMaximumHeight(80);
    profile_prompt_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(field_style()));
    vl->addWidget(profile_prompt_edit_);

    auto* btn_row = new QHBoxLayout;
    profile_save_btn_ = new QPushButton("SAVE PROFILE");
    profile_save_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::AMBER()) +
                                     ";color:" + QString(ui::colors::BG_BASE()) +
                                     ";border:none;padding:8px 20px;"
                                     "font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:" +
                                     QString(ui::colors::AMBER_DIM()) + ";}");
    connect(profile_save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_profile);
    btn_row->addWidget(profile_save_btn_);

    profile_default_btn_ = new QPushButton("SET AS DEFAULT");
    profile_default_btn_->setEnabled(false);
    profile_default_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::AMBER()) +
                                        ";border:1px solid " + QString(ui::colors::AMBER()) +
                                        ";"
                                        "padding:8px 20px;font-weight:600;}"
                                        "QPushButton:hover{background:" +
                                        QString(ui::colors::BG_RAISED()) +
                                        ";}"
                                        "QPushButton:disabled{color:" +
                                        QString(ui::colors::BORDER_BRIGHT()) +
                                        ";border-color:" + QString(ui::colors::BORDER_BRIGHT()) + ";}");
    connect(profile_default_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_set_default_profile);
    btn_row->addWidget(profile_default_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    profile_status_lbl_ = new QLabel;
    profile_status_lbl_->hide();
    vl->addWidget(profile_status_lbl_);

    vl->addStretch();
    scroll->setWidget(panel);
    return scroll;
}

// ============================================================================
// Profiles tab — data helpers
// ============================================================================

void LlmConfigSection::load_profiles() {
    profile_list_->blockSignals(true);
    profile_list_->clear();

    auto result = LlmProfileRepository::instance().list_profiles();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            QString display = p.name;
            if (p.is_default)
                display += "  ★";
            display += QString("  [%1 / %2]").arg(p.provider, p.model_id);
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.id);
            if (p.is_default)
                item->setForeground(QColor("" + QString(ui::colors::AMBER()) + ""));
            profile_list_->addItem(item);
        }
    }

    profile_list_->blockSignals(false);
}

void LlmConfigSection::populate_profile_form(const LlmProfile& p) {
    profile_name_edit_->setText(p.name);

    int idx = profile_provider_combo_->findText(p.provider, Qt::MatchFixedString | Qt::MatchCaseSensitive);
    if (idx >= 0)
        profile_provider_combo_->setCurrentIndex(idx);
    else {
        profile_provider_combo_->addItem(p.provider);
        profile_provider_combo_->setCurrentText(p.provider);
    }

    // Populate model combo from fallback list, then select saved model
    profile_model_combo_->clear();
    profile_model_combo_->addItems(fallback_models(p.provider));
    profile_model_combo_->setCurrentText(p.model_id);

    profile_api_key_edit_->setText(p.api_key);
    profile_base_url_edit_->setText(p.base_url);
    profile_temp_spin_->setValue(p.temperature);
    profile_tokens_spin_->setValue(p.max_tokens);
    profile_prompt_edit_->setPlainText(p.system_prompt);

    editing_profile_id_ = p.id;
    profile_delete_btn_->setEnabled(true);
    profile_default_btn_->setEnabled(!p.is_default);
}

void LlmConfigSection::clear_profile_form() {
    profile_name_edit_->clear();
    profile_provider_combo_->setCurrentIndex(0);
    on_profile_provider_changed(profile_provider_combo_->currentText());
    profile_api_key_edit_->clear();
    profile_base_url_edit_->clear();
    profile_temp_spin_->setValue(0.7);
    profile_tokens_spin_->setValue(4096);
    profile_prompt_edit_->clear();
    profile_list_->clearSelection();
    profile_delete_btn_->setEnabled(false);
    profile_default_btn_->setEnabled(false);
    editing_profile_id_.clear();
}

void LlmConfigSection::show_profile_status(const QString& msg, bool error) {
    profile_status_lbl_->setText(msg);
    profile_status_lbl_->setStyleSheet(error ? "color:" + QString(ui::colors::NEGATIVE()) + ";"
                                             : "color:" + QString(ui::colors::POSITIVE()) + ";");
    profile_status_lbl_->show();
    QTimer::singleShot(4000, profile_status_lbl_, &QLabel::hide);
}

// ============================================================================
// Profiles tab — slots
// ============================================================================

void LlmConfigSection::on_profile_selected(int row) {
    if (row < 0)
        return;
    auto* item = profile_list_->item(row);
    if (!item)
        return;
    QString id = item->data(Qt::UserRole).toString();
    auto r = LlmProfileRepository::instance().get_profile(id);
    if (r.is_ok())
        populate_profile_form(r.value());
}

void LlmConfigSection::on_profile_provider_changed(const QString& provider) {
    const bool is_ollama = (provider.toLower() == "ollama");

    profile_model_combo_->clear();
    profile_model_combo_->addItems(fallback_models(provider));
    profile_base_url_edit_->setText(default_base_url(provider));

    if (is_ollama) {
        // Local provider — no API key needed. One-shot listener pulls live models
        // from the local Ollama server into the profile combo. Receiver is `this`,
        // so the connection is auto-severed if the section is destroyed.
        profile_api_key_edit_->clear();
        profile_api_key_edit_->setEnabled(false);
        profile_api_key_edit_->setPlaceholderText("Not required — local provider");

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
                        [this, conn](const QString& p, const QStringList& models, const QString& err) {
                            if (p.toLower() != "ollama")
                                return;
                            disconnect(*conn);
                            if (!err.isEmpty()) {
                                show_profile_status(
                                    "Cannot reach Ollama — is `ollama serve` running locally?", true);
                                return;
                            }
                            const QString current = profile_model_combo_->currentText();
                            profile_model_combo_->clear();
                            profile_model_combo_->addItems(models);
                            if (!current.isEmpty())
                                profile_model_combo_->setCurrentText(current);
                        });
        ai_chat::LlmService::instance().fetch_models("ollama", {}, profile_base_url_edit_->text().trimmed());
        return;
    }

    profile_api_key_edit_->setEnabled(true);
    profile_api_key_edit_->setPlaceholderText("Leave blank to inherit from provider");

    // Pre-fill api_key from saved provider if present and field is empty
    if (profile_api_key_edit_->text().isEmpty()) {
        auto providers = LlmConfigRepository::instance().list_providers();
        if (providers.is_ok()) {
            for (const auto& p : providers.value()) {
                if (p.provider.toLower() == provider.toLower() && !p.api_key.isEmpty()) {
                    profile_api_key_edit_->setText(p.api_key);
                    break;
                }
            }
        }
    }
}

void LlmConfigSection::on_save_profile() {
    QString name = profile_name_edit_->text().trimmed();
    if (name.isEmpty()) {
        show_profile_status("Profile name is required", true);
        return;
    }
    QString provider = profile_provider_combo_->currentText().trimmed();
    QString model = profile_model_combo_->currentText().trimmed();
    if (model.isEmpty()) {
        show_profile_status("Model is required", true);
        return;
    }

    // If api_key is empty, inherit from saved provider
    QString api_key = profile_api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        auto providers = LlmConfigRepository::instance().list_providers();
        if (providers.is_ok()) {
            for (const auto& p : providers.value()) {
                if (p.provider.toLower() == provider.toLower()) {
                    api_key = p.api_key;
                    break;
                }
            }
        }
    }

    LlmProfile profile;
    profile.id = editing_profile_id_; // empty = new (repo generates UUID)
    profile.name = name;
    profile.provider = provider.toLower();
    profile.model_id = model;
    profile.api_key = api_key;
    profile.base_url = profile_base_url_edit_->text().trimmed();
    profile.temperature = profile_temp_spin_->value();
    profile.max_tokens = profile_tokens_spin_->value();
    profile.system_prompt = profile_prompt_edit_->toPlainText().trimmed();
    profile.is_default = false;

    auto r = LlmProfileRepository::instance().save_profile(profile);
    if (r.is_err()) {
        show_profile_status("Save failed: " + QString::fromStdString(r.error()), true);
        return;
    }

    show_profile_status("Profile saved", false);
    load_profiles();
    emit config_changed();
    LOG_INFO(TAG_PROFILES,QString("LLM profile saved: %1 (%2 / %3)").arg(name, provider, model));
}

void LlmConfigSection::on_delete_profile() {
    if (editing_profile_id_.isEmpty())
        return;
    auto r = LlmProfileRepository::instance().delete_profile(editing_profile_id_);
    if (r.is_ok()) {
        clear_profile_form();
        load_profiles();
        emit config_changed();
        LOG_INFO(TAG_PROFILES,"LLM profile deleted: " + editing_profile_id_);
    } else {
        show_profile_status("Delete failed: " + QString::fromStdString(r.error()), true);
    }
}

void LlmConfigSection::on_set_default_profile() {
    if (editing_profile_id_.isEmpty())
        return;
    auto r = LlmProfileRepository::instance().set_default(editing_profile_id_);
    if (r.is_ok()) {
        profile_default_btn_->setEnabled(false);
        load_profiles();
        emit config_changed();
        LOG_INFO(TAG_PROFILES,"LLM default profile set: " + editing_profile_id_);
    } else {
        show_profile_status("Failed: " + QString::fromStdString(r.error()), true);
    }
}

} // namespace fincept::screens
