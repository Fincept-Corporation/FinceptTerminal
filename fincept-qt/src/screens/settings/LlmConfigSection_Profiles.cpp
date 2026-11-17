// src/screens/settings/LlmConfigSection_Profiles.cpp
//
// Profile-tab implementation: build_profiles_tab, list/form panels, load_profiles,
// populate_profile_form, clear_profile_form, and all on_profile_* slots.
//
// Part of the partial-class split of LlmConfigSection.cpp.

#include "core/logging/Logger.h"
#include "screens/settings/LlmConfigSection.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelCatalog.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

// Unity-build hygiene: see LlmConfigSection_Providers.cpp for the rationale.
namespace {
constexpr const char* TAG_PROFILES = "LlmConfigSection";
// Per-profile Max Tokens ceiling for providers other than Fincept.
constexpr int kLlmProfilesTokensMax = 128000;
} // namespace

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

namespace {
// The Fincept API rejects max_tokens above ModelCatalog::kFinceptMaxOutputTokens
// and profile_to_json() caps a Fincept profile there, so its field must not
// offer more — the same ceiling the main LLM settings section applies. Setting
// the maximum also brings a stored larger value down to what is really sent.
void llm_profiles_apply_tokens_cap(QSpinBox* spin, const QString& provider) {
    if (!spin)
        return;
    const bool fincept = provider.trimmed().compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0;
    const int cap = fincept ? ai_chat::ModelCatalog::kFinceptMaxOutputTokens : kLlmProfilesTokensMax;
    spin->setMaximum(cap);
    spin->setToolTip(fincept ? LlmConfigSection::tr("The Fincept AI service accepts at most %1 output tokens.").arg(cap)
                             : QString());
}
} // namespace

// ── Harness profiles (provider "harness:<id>") ───────────────────────────────
// A harness profile names an agent harness (Claude Code, Codex) answering
// terminal AI: its model and reasoning effort, never a key or an endpoint (the
// harness authenticates with the user's own CLI setup). Widgets that the
// header does not declare are found by objectName.
namespace llm_profiles_harness_detail {

constexpr const char* kLphEffortCombo = "profileEffortCombo";
constexpr const char* kLphEffortLabel = "profileEffortLabel";
constexpr const char* kLphHint = "profileHarnessHint";

bool lph_is_harness(const QString& provider) {
    return ai_chat::ProviderCatalog::is_harness(provider);
}

/// "harness:<id>" of every harness that can answer terminal AI, registration order.
QStringList lph_harness_providers() {
    QStringList out;
    auto& reg = harness::HarnessRegistry::instance();
    for (const QString& id : reg.ids()) {
        const harness::IHarnessAdapter* a = reg.adapter(id);
        if (a && a->descriptor().caps.chat)
            out << QStringLiteral("harness:") + id;
    }
    return out;
}

/// The model currently chosen: the item's id (its data), or what was typed.
QString lph_model_of(const QComboBox* combo) {
    const int i = combo->currentIndex();
    if (i >= 0 && combo->itemText(i) == combo->currentText())
        return combo->itemData(i).toString();
    return combo->currentText().trimmed();
}

/// "CLI default" first (data ""), then the harness's models (text = id).
void lph_fill_models(QComboBox* combo, const QString& harness_id, const QString& current) {
    const QSignalBlocker block(combo);
    combo->clear();
    combo->addItem(LlmConfigSection::tr("CLI default"), QString());
    const harness::HarnessStatus st = harness::HarnessRegistry::instance().status(harness_id);
    for (const harness::ModelInfo& m : st.models) {
        combo->addItem(m.id, m.id);
        if (!m.label.isEmpty() && m.label != m.id)
            combo->setItemData(combo->count() - 1, m.label, Qt::ToolTipRole);
    }
    int idx = combo->findData(current);
    if (idx < 0 && !current.isEmpty()) {
        combo->addItem(current, current);
        idx = combo->count() - 1;
    }
    combo->setCurrentIndex(qMax(0, idx));
}

void lph_fill_efforts(QComboBox* combo, const QString& harness_id, const QString& model, const QString& current) {
    if (!combo)
        return;
    const QSignalBlocker block(combo);
    combo->clear();
    combo->addItem(LlmConfigSection::tr("CLI default"), QString());
    for (const QString& e : harness::HarnessRegistry::instance().efforts(harness_id, model))
        combo->addItem(e, e);
    int idx = combo->findData(current);
    if (idx < 0 && !current.isEmpty()) {
        combo->addItem(current, current);
        idx = combo->count() - 1;
    }
    combo->setCurrentIndex(qMax(0, idx));
}

bool lph_has_effort(const QString& harness_id) {
    const harness::IHarnessAdapter* a = harness::HarnessRegistry::instance().adapter(harness_id);
    return a && a->descriptor().caps.effort;
}

} // namespace llm_profiles_harness_detail

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

    profile_list_title_ = new QLabel(tr("PROFILES"));
    profile_list_title_->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) +
                                       ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(profile_list_title_);

    profile_list_hint_ = new QLabel(tr("A profile = named LLM config you can assign to any agent or team."));
    profile_list_hint_->setWordWrap(true);
    profile_list_hint_->setStyleSheet("color:" + QString(ui::colors::TEXT_TERTIARY()) + ";padding-bottom:4px;");
    vl->addWidget(profile_list_hint_);

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
    profile_add_btn_ = new QPushButton(tr("+ New"));
    profile_add_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                                    QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                                    ";"
                                    "border-radius:3px;padding:5px 10px;font-weight:600;}"
                                    "QPushButton:hover{background:" +
                                    QString(ui::colors::BG_RAISED()) + ";}");
    connect(profile_add_btn_, &QPushButton::clicked, this, [this]() {
        editing_profile_id_.clear();
        clear_profile_form();
    });
    btn_row->addWidget(profile_add_btn_);

    profile_delete_btn_ = new QPushButton(tr("Delete"));
    profile_delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the model profile
    profile_delete_btn_->setEnabled(false);
    profile_delete_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::NEGATIVE()) +
                                       ";border:1px solid " + QString(ui::colors::NEGATIVE()) +
                                       ";"
                                       "border-radius:3px;padding:5px 10px;}"
                                       "QPushButton:hover{background:" +
                                       QString(ui::colors::BG_RAISED()) +
                                       ";}"
                                       "QPushButton:disabled{color:" +
                                       QString(ui::colors::TEXT_TERTIARY()) +
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

    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
        return l;
    };
    auto field_style = []() {
        return QString("background:" + QString(ui::colors::BG_RAISED()) +
                       ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                       QString(ui::colors::BORDER_MED()) + ";padding:6px 10px;");
    };

    profile_name_field_lbl_ = lbl(tr("PROFILE NAME"));
    vl->addWidget(profile_name_field_lbl_);
    profile_name_edit_ = new QLineEdit;
    profile_name_edit_->setPlaceholderText(tr("e.g. Fast Groq, Careful Claude, Coding minimax"));
    profile_name_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_name_edit_);

    profile_provider_field_lbl_ = lbl(tr("PROVIDER"));
    vl->addWidget(profile_provider_field_lbl_);
    profile_provider_combo_ = new QComboBox;
    // Formatted names, alphabetical; the provider id rides in itemData.
    for (const auto& id : providers_sorted())
        profile_provider_combo_->addItem(provider_display_name(id), id);
    // Agent harnesses that can answer terminal AI ("Claude Code (harness)").
    const QStringList harness_providers = llm_profiles_harness_detail::lph_harness_providers();
    if (!harness_providers.isEmpty()) {
        profile_provider_combo_->insertSeparator(profile_provider_combo_->count());
        for (const QString& hp : harness_providers)
            profile_provider_combo_->addItem(provider_display_name(hp), hp);
    }
    profile_provider_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    connect(profile_provider_combo_, &QComboBox::currentIndexChanged, this,
            [this](int) { on_profile_provider_changed(profile_provider_combo_->currentData().toString()); });
    vl->addWidget(profile_provider_combo_);

    profile_model_field_lbl_ = lbl(tr("MODEL"));
    vl->addWidget(profile_model_field_lbl_);
    profile_model_combo_ = new QComboBox;
    profile_model_combo_->setEditable(true);
    profile_model_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    vl->addWidget(profile_model_combo_);

    // Harness profiles only: reasoning effort, and what such a profile is.
    auto* effort_lbl = lbl(tr("REASONING EFFORT"));
    effort_lbl->setObjectName(QString::fromLatin1(llm_profiles_harness_detail::kLphEffortLabel));
    effort_lbl->setVisible(false);
    vl->addWidget(effort_lbl);
    auto* effort_combo = new QComboBox;
    effort_combo->setObjectName(QString::fromLatin1(llm_profiles_harness_detail::kLphEffortCombo));
    effort_combo->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    effort_combo->setVisible(false);
    vl->addWidget(effort_combo);
    auto* harness_hint = new QLabel(tr("Authenticates with your own Claude Code / Codex setup — no key is stored. It "
                                       "answers terminal AI only when signed in with an API key (Settings › LLM "
                                       "Configuration › Terminal AI); Agent Studio cannot use it yet."));
    harness_hint->setObjectName(QString::fromLatin1(llm_profiles_harness_detail::kLphHint));
    harness_hint->setWordWrap(true);
    harness_hint->setStyleSheet("color:" + QString(ui::colors::TEXT_TERTIARY()) + ";");
    harness_hint->setVisible(false);
    vl->addWidget(harness_hint);
    // A harness's efforts depend on the model.
    connect(profile_model_combo_, &QComboBox::currentTextChanged, this, [this, effort_combo](const QString&) {
        const QString provider = profile_provider_combo_->currentData().toString();
        if (!llm_profiles_harness_detail::lph_is_harness(provider))
            return;
        const QString id = ai_chat::ProviderCatalog::harness_id(provider);
        if (llm_profiles_harness_detail::lph_has_effort(id))
            llm_profiles_harness_detail::lph_fill_efforts(
                effort_combo, id, llm_profiles_harness_detail::lph_model_of(profile_model_combo_),
                effort_combo->currentData().toString());
    });

    profile_api_key_field_lbl_ = lbl(tr("API KEY"));
    vl->addWidget(profile_api_key_field_lbl_);
    profile_api_key_edit_ = new QLineEdit;
    profile_api_key_edit_->setEchoMode(QLineEdit::Password);
    profile_api_key_edit_->setPlaceholderText(tr("Leave blank to inherit from provider"));
    profile_api_key_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_api_key_edit_);

    profile_base_url_field_lbl_ = lbl(tr("BASE URL (custom endpoint)"));
    vl->addWidget(profile_base_url_field_lbl_);
    profile_base_url_edit_ = new QLineEdit;
    profile_base_url_edit_->setPlaceholderText(tr("Leave blank to use provider default"));
    profile_base_url_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_base_url_edit_);

    auto* param_row = new QHBoxLayout;
    auto* temp_col = new QVBoxLayout;
    profile_temp_field_lbl_ = lbl(tr("TEMPERATURE"));
    temp_col->addWidget(profile_temp_field_lbl_);
    profile_temp_spin_ = new QDoubleSpinBox;
    profile_temp_spin_->setRange(0.0, 2.0);
    profile_temp_spin_->setSingleStep(0.1);
    profile_temp_spin_->setValue(0.7);
    profile_temp_spin_->setStyleSheet(field_style());
    temp_col->addWidget(profile_temp_spin_);
    param_row->addLayout(temp_col);

    auto* tok_col = new QVBoxLayout;
    profile_tokens_field_lbl_ = lbl(tr("MAX TOKENS"));
    tok_col->addWidget(profile_tokens_field_lbl_);
    profile_tokens_spin_ = new QSpinBox;
    profile_tokens_spin_->setRange(256, kLlmProfilesTokensMax); // per provider: llm_profiles_apply_tokens_cap
    profile_tokens_spin_->setSingleStep(256);
    profile_tokens_spin_->setValue(4096);
    profile_tokens_spin_->setStyleSheet(field_style());
    tok_col->addWidget(profile_tokens_spin_);
    param_row->addLayout(tok_col);
    vl->addLayout(param_row);

    profile_prompt_field_lbl_ = lbl(tr("SYSTEM PROMPT OVERRIDE (optional)"));
    vl->addWidget(profile_prompt_field_lbl_);
    profile_prompt_edit_ = new QPlainTextEdit;
    profile_prompt_edit_->setPlaceholderText(tr("Leave blank to use global system prompt"));
    profile_prompt_edit_->setMaximumHeight(80);
    profile_prompt_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(field_style()));
    vl->addWidget(profile_prompt_edit_);

    auto* btn_row = new QHBoxLayout;
    profile_save_btn_ = new QPushButton(tr("SAVE PROFILE"));
    profile_save_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::AMBER()) +
                                     ";color:" + QString(ui::colors::TEXT_ON_ACCENT()) +
                                     ";border:none;padding:8px 20px;"
                                     "font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:" +
                                     QString(ui::colors::AMBER_DIM()) + ";}");
    connect(profile_save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_profile);
    btn_row->addWidget(profile_save_btn_);

    profile_default_btn_ = new QPushButton(tr("SET AS DEFAULT"));
    profile_default_btn_->setEnabled(false);
    profile_default_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::AMBER()) +
                                        ";border:1px solid " + QString(ui::colors::AMBER()) +
                                        ";"
                                        "padding:8px 20px;font-weight:600;}"
                                        "QPushButton:hover{background:" +
                                        QString(ui::colors::BG_RAISED()) +
                                        ";}"
                                        "QPushButton:disabled{color:" +
                                        QString(ui::colors::TEXT_TERTIARY()) +
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

    // Every profile, harness profiles included (list_profiles() leaves those out
    // for the agent runtime's pickers).
    auto result = LlmProfileRepository::instance().list_all_profiles();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            QString display = p.name;
            if (p.is_default)
                display += "  ★";
            if (p.is_harness()) {
                QString what = p.model_id.isEmpty() ? tr("CLI default") : p.model_id;
                if (!p.reasoning_effort.isEmpty())
                    what += QStringLiteral(" · ") + p.reasoning_effort;
                display += QString("  [%1 / %2]  · %3").arg(provider_display_name(p.provider), what, tr("terminal AI only"));
            } else {
                display += QString("  [%1 / %2]").arg(p.provider, p.model_id);
            }
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.id);
            item->setData(Qt::UserRole + 1, p.is_default); // read by the theme_changed recolour
            if (p.is_default)
                item->setForeground(QColor(ui::colors::AMBER()));
            profile_list_->addItem(item);
            // Keep the profile being edited highlighted across a reload (signals
            // are blocked, so this does not re-populate the form).
            if (!editing_profile_id_.isEmpty() && p.id == editing_profile_id_)
                profile_list_->setCurrentItem(item);
        }
    }

    profile_list_->blockSignals(false);
}

void LlmConfigSection::populate_profile_form(const LlmProfile& p) {
    profile_name_edit_->setText(p.name);

    int idx = profile_provider_combo_->findData(p.provider, Qt::UserRole, Qt::MatchFixedString);
    if (idx >= 0)
        profile_provider_combo_->setCurrentIndex(idx);
    else {
        // Custom/unknown provider: add it with the id in itemData.
        profile_provider_combo_->addItem(provider_display_name(p.provider), p.provider);
        profile_provider_combo_->setCurrentIndex(profile_provider_combo_->count() - 1);
    }

    const bool is_harness = llm_profiles_harness_detail::lph_is_harness(p.provider);
    if (is_harness) {
        // The provider combo may not have changed: put the harness form in place, then its values.
        on_profile_provider_changed(p.provider);
        const QString hid = ai_chat::ProviderCatalog::harness_id(p.provider);
        llm_profiles_harness_detail::lph_fill_models(profile_model_combo_, hid, p.model_id);
        if (auto* effort = findChild<QComboBox*>(QString::fromLatin1(llm_profiles_harness_detail::kLphEffortCombo)))
            llm_profiles_harness_detail::lph_fill_efforts(effort, hid, p.model_id, p.reasoning_effort);
    } else {
        // Populate model combo from fallback list, then select saved model
        profile_model_combo_->clear();
        profile_model_combo_->addItems(fallback_models(p.provider));
        profile_model_combo_->setCurrentText(p.model_id);
    }

    profile_api_key_edit_->setText(is_harness ? QString() : p.api_key);
    profile_base_url_edit_->setText(is_harness ? QString() : p.base_url);
    profile_temp_spin_->setValue(p.temperature);
    // The provider combo may not have changed (no provider_changed signal), so
    // apply this profile's ceiling before its value.
    llm_profiles_apply_tokens_cap(profile_tokens_spin_, p.provider);
    profile_tokens_spin_->setValue(p.max_tokens);
    profile_prompt_edit_->setPlainText(p.system_prompt);

    editing_profile_id_ = p.id;
    profile_delete_btn_->setEnabled(true);
    // A harness profile cannot be the default (that also serves Agent Studio).
    profile_default_btn_->setEnabled(!p.is_default && !is_harness);
}

void LlmConfigSection::clear_profile_form() {
    profile_name_edit_->clear();
    profile_provider_combo_->setCurrentIndex(0);
    on_profile_provider_changed(profile_provider_combo_->currentData().toString());
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
    namespace lph = llm_profiles_harness_detail;
    const bool is_harness = lph::lph_is_harness(provider);
    auto* effort_combo = findChild<QComboBox*>(QString::fromLatin1(lph::kLphEffortCombo));
    auto* effort_lbl = findChild<QLabel*>(QString::fromLatin1(lph::kLphEffortLabel));
    auto* harness_hint = findChild<QLabel*>(QString::fromLatin1(lph::kLphHint));
    const QString harness_id = ai_chat::ProviderCatalog::harness_id(provider);
    const bool show_effort = is_harness && lph::lph_has_effort(harness_id);
    if (effort_combo)
        effort_combo->setVisible(show_effort);
    if (effort_lbl)
        effort_lbl->setVisible(show_effort);
    if (harness_hint)
        harness_hint->setVisible(is_harness);
    // A harness has no key, endpoint, temperature or token cap of ours.
    profile_base_url_edit_->setEnabled(!is_harness);
    profile_temp_spin_->setEnabled(!is_harness);
    profile_tokens_spin_->setEnabled(!is_harness);
    if (is_harness) {
        profile_api_key_edit_->clear();
        profile_api_key_edit_->setEnabled(false);
        profile_api_key_edit_->setPlaceholderText(tr("Not used — the harness authenticates itself"));
        profile_base_url_edit_->clear();
        // Keep the model only when it is one of this harness's (not the previous provider's).
        QString model = lph::lph_model_of(profile_model_combo_);
        bool known = false;
        for (const harness::ModelInfo& m : harness::HarnessRegistry::instance().status(harness_id).models)
            known = known || m.id == model;
        if (!known)
            model.clear();
        lph::lph_fill_models(profile_model_combo_, harness_id, model);
        if (show_effort)
            lph::lph_fill_efforts(effort_combo, harness_id, lph::lph_model_of(profile_model_combo_),
                                  effort_combo->currentData().toString());
        profile_default_btn_->setEnabled(false);
        return;
    }

    const bool is_ollama = (provider.toLower() == "ollama");
    llm_profiles_apply_tokens_cap(profile_tokens_spin_, provider);

    profile_model_combo_->clear();
    profile_model_combo_->addItems(fallback_models(provider));
    profile_base_url_edit_->setText(default_base_url(provider));

    if (is_ollama) {
        // Local provider — no API key needed. One-shot listener pulls live models
        // from the local Ollama server into the profile combo. Receiver is `this`,
        // so the connection is auto-severed if the section is destroyed.
        profile_api_key_edit_->clear();
        profile_api_key_edit_->setEnabled(false);
        profile_api_key_edit_->setPlaceholderText(tr("Not required — local provider"));

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn =
            connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
                    [this, conn](const QString& p, const QStringList& models, const QString& err) {
                        if (p.toLower() != "ollama")
                            return;
                        disconnect(*conn);
                        if (!err.isEmpty()) {
                            show_profile_status(tr("Cannot reach Ollama — is `ollama serve` running locally?"), true);
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
    profile_api_key_edit_->setPlaceholderText(tr("Leave blank to inherit from provider"));

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
        show_profile_status(tr("Profile name is required"), true);
        return;
    }
    QString provider = profile_provider_combo_->currentData().toString().trimmed();
    const bool is_harness = llm_profiles_harness_detail::lph_is_harness(provider);
    // A harness may run its CLI's default model ("CLI default" = empty).
    QString model = is_harness ? llm_profiles_harness_detail::lph_model_of(profile_model_combo_)
                               : profile_model_combo_->currentText().trimmed();
    if (model.isEmpty() && !is_harness) {
        show_profile_status(tr("Model is required"), true);
        return;
    }

    // If api_key is empty, inherit from saved provider (a harness never stores one).
    QString api_key = is_harness ? QString() : profile_api_key_edit_->text().trimmed();
    if (api_key.isEmpty() && !is_harness) {
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

    // A custom endpoint must be a real http(s) URL — a typo otherwise only surfaces
    // as an opaque connection error the first time an agent uses the profile.
    const QString base_url = is_harness ? QString() : profile_base_url_edit_->text().trimmed();
    if (!base_url.isEmpty()) {
        const QUrl u(base_url, QUrl::StrictMode);
        const QString scheme = u.scheme().toLower();
        if (!u.isValid() || u.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
            show_profile_status(tr("Base URL must be a full http:// or https:// address"), true);
            return;
        }
    }

    // save_profile() is INSERT OR REPLACE, so is_default has to be carried over
    // from the stored row: hard-coding false silently cleared the default flag
    // every time the default profile was edited and saved.
    bool was_default = false;
    if (!editing_profile_id_.isEmpty()) {
        const auto existing = LlmProfileRepository::instance().get_profile(editing_profile_id_);
        if (existing.is_ok())
            was_default = existing.value().is_default;
    }

    LlmProfile profile;
    // New profiles get their id here (same format the repository would mint) so the
    // form keeps editing that row afterwards — otherwise a second click on SAVE
    // inserted a duplicate profile.
    profile.id = editing_profile_id_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                               : editing_profile_id_;
    profile.name = name;
    profile.provider = provider.toLower();
    profile.model_id = model;
    profile.api_key = api_key;
    profile.base_url = base_url;
    profile.temperature = profile_temp_spin_->value();
    profile.max_tokens = profile_tokens_spin_->value();
    profile.system_prompt = profile_prompt_edit_->toPlainText().trimmed();
    profile.is_default = was_default && !is_harness;
    if (is_harness) {
        if (auto* effort =
                findChild<QComboBox*>(QString::fromLatin1(llm_profiles_harness_detail::kLphEffortCombo));
            effort && !effort->isHidden())
            profile.reasoning_effort = effort->currentData().toString();
    }

    auto r = LlmProfileRepository::instance().save_profile(profile);
    if (r.is_err()) {
        show_profile_status(tr("Save failed: ") + QString::fromStdString(r.error()), true);
        return;
    }

    editing_profile_id_ = profile.id;
    profile_delete_btn_->setEnabled(true);
    profile_default_btn_->setEnabled(!profile.is_default && !is_harness);

    show_profile_status(tr("Profile saved"), false);
    load_profiles();
    emit config_changed();
    LOG_INFO(TAG_PROFILES, QString("LLM profile saved: %1 (%2 / %3)").arg(name, provider, model));
}

void LlmConfigSection::on_delete_profile() {
    if (editing_profile_id_.isEmpty())
        return;

    // Deleting a profile is irreversible and silently detaches every agent /
    // team assigned to it — the only destructive action on this tab that had
    // no confirmation step (the provider Remove button already has one).
    const QString name = profile_name_edit_ ? profile_name_edit_->text().trimmed() : QString();
    const auto reply = QMessageBox::question(
        this, tr("Delete Profile"),
        name.isEmpty() ? tr("Delete this LLM profile?\n\nThis cannot be undone.")
                       : tr("Delete the LLM profile \"%1\"?\n\nAgents assigned to it fall back to the "
                            "default profile. This cannot be undone.")
                             .arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    auto r = LlmProfileRepository::instance().delete_profile(editing_profile_id_);
    if (r.is_ok()) {
        clear_profile_form();
        load_profiles();
        emit config_changed();
        LOG_INFO(TAG_PROFILES, "LLM profile deleted: " + editing_profile_id_);
    } else {
        show_profile_status(tr("Delete failed: ") + QString::fromStdString(r.error()), true);
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
        LOG_INFO(TAG_PROFILES, "LLM default profile set: " + editing_profile_id_);
    } else {
        show_profile_status(tr("Failed: ") + QString::fromStdString(r.error()), true);
    }
}

} // namespace fincept::screens
