#pragma once
// LlmConfigSection.h — LLM provider + profile configuration panel

#include "storage/repositories/LlmProfileRepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>

namespace fincept::screens {

class LlmConfigSection : public QWidget {
    Q_OBJECT
  public:
    explicit LlmConfigSection(QWidget* parent = nullptr);

    void reload(); // called when section becomes visible

  signals:
    void config_changed(); // emitted when user saves — AiChatScreen reloads

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    // Provider tab
    void on_provider_selected(int row);
    void on_save_provider();
    void on_delete_provider();
    void on_save_global();
    void on_test_connection();
    void on_fetch_models();
    void on_models_fetched(const QString& provider, const QStringList& models, const QString& error);

    // Profiles tab
    void on_profile_selected(int row);
    void on_save_profile();
    void on_delete_profile();
    void on_set_default_profile();
    void on_profile_provider_changed(const QString& provider);

  private:
    // ── Tab widget ────────────────────────────────────────────────────────────
    QTabWidget* tab_widget_ = nullptr;
    QLabel*     title_lbl_   = nullptr; // "LLM CONFIGURATION" title-bar label

    // ── Provider tab widgets ──────────────────────────────────────────────────
    QListWidget* provider_list_ = nullptr;
    QLabel* provider_list_title_ = nullptr; // "Providers"
    QPushButton* add_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QStackedWidget* form_stack_ = nullptr; // 0=empty, 1=form, 2=fincept
    QLabel* form_title_ = nullptr;         // "Provider Configuration"
    QLabel* provider_field_lbl_ = nullptr;
    QLabel* api_key_field_lbl_ = nullptr;
    QLabel* model_field_lbl_ = nullptr;
    QLabel* base_url_field_lbl_ = nullptr;
    QLineEdit* provider_edit_ = nullptr;
    QLineEdit* api_key_edit_ = nullptr;
    QLineEdit* base_url_edit_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QCheckBox* tools_check_ = nullptr;
    QLabel* status_lbl_ = nullptr;

    // Global settings (lives at bottom of provider tab)
    QLabel* global_title_ = nullptr; // "GLOBAL SETTINGS"
    QLabel* temp_lbl_ = nullptr;
    QLabel* tokens_lbl_ = nullptr;
    QLabel* tool_rounds_lbl_ = nullptr;
    QLabel* system_prompt_lbl_ = nullptr;
    QDoubleSpinBox* temp_spin_ = nullptr;
    QSpinBox* tokens_spin_ = nullptr;
    QSpinBox* tool_rounds_spin_ = nullptr;
    QPlainTextEdit* system_prompt_ = nullptr;
    QPushButton* save_global_btn_ = nullptr;

    // ── Profiles tab widgets ──────────────────────────────────────────────────
    QListWidget* profile_list_ = nullptr;
    QLabel* profile_list_title_ = nullptr; // "PROFILES"
    QLabel* profile_list_hint_ = nullptr;
    QPushButton* profile_add_btn_ = nullptr; // "+ New"
    QLabel* profile_name_field_lbl_ = nullptr;
    QLabel* profile_provider_field_lbl_ = nullptr;
    QLabel* profile_model_field_lbl_ = nullptr;
    QLabel* profile_api_key_field_lbl_ = nullptr;
    QLabel* profile_base_url_field_lbl_ = nullptr;
    QLabel* profile_temp_field_lbl_ = nullptr;
    QLabel* profile_tokens_field_lbl_ = nullptr;
    QLabel* profile_prompt_field_lbl_ = nullptr;
    QLineEdit* profile_name_edit_ = nullptr;
    QComboBox* profile_provider_combo_ = nullptr;
    QComboBox* profile_model_combo_ = nullptr;
    QLineEdit* profile_api_key_edit_ = nullptr;
    QLineEdit* profile_base_url_edit_ = nullptr;
    QDoubleSpinBox* profile_temp_spin_ = nullptr;
    QSpinBox* profile_tokens_spin_ = nullptr;
    QPlainTextEdit* profile_prompt_edit_ = nullptr;
    QPushButton* profile_save_btn_ = nullptr;
    QPushButton* profile_delete_btn_ = nullptr;
    QPushButton* profile_default_btn_ = nullptr;
    QLabel* profile_status_lbl_ = nullptr;

    // Profile state
    QString editing_profile_id_; // empty = new profile

    // ── Builders ──────────────────────────────────────────────────────────────
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange. Covers widgets built
    /// across all three TUs (this file + _Providers + _Profiles).
    void retranslateUi();

    QWidget* build_providers_tab();
    QWidget* build_provider_list_panel();
    QWidget* build_form_panel();
    QWidget* build_global_panel();
    QWidget* build_profiles_tab();
    QWidget* build_profile_list_panel();
    QWidget* build_profile_form_panel();

    // ── Provider tab helpers ──────────────────────────────────────────────────
    void load_providers();
    void populate_form(const QString& provider);
    void show_status(const QString& msg, bool error = false);

    // ── Profiles tab helpers ──────────────────────────────────────────────────
    void load_profiles();
    void populate_profile_form(const LlmProfile& p);
    void clear_profile_form();
    void show_profile_status(const QString& msg, bool error = false);

    static const QStringList KNOWN_PROVIDERS;
    static QString default_base_url(const QString& provider);
    static QStringList fallback_models(const QString& provider);

    // Human-readable label for a provider id (e.g. "aihubmix" → "AIHubMix").
    static QString provider_display_name(const QString& provider_id);
    // KNOWN_PROVIDERS ids sorted alphabetically by their display name.
    static QStringList providers_sorted();
};

} // namespace fincept::screens
