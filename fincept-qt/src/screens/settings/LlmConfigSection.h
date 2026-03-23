#pragma once
// LlmConfigSection.h — LLM provider configuration panel for SettingsScreen

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QWidget>

namespace fincept::screens {

class LlmConfigSection : public QWidget {
    Q_OBJECT
  public:
    explicit LlmConfigSection(QWidget* parent = nullptr);

    void reload();  // called when section becomes visible

  signals:
    void config_changed();  // emitted when user saves — AiChatScreen reloads

  private slots:
    void on_provider_selected(int row);
    void on_save_provider();
    void on_delete_provider();
    void on_save_global();
    void on_test_connection();
    void on_fetch_models();
    void on_models_fetched(const QString& provider, const QStringList& models,
                           const QString& error);

  private:
    // Left: provider list
    QListWidget*    provider_list_  = nullptr;
    QPushButton*    add_btn_        = nullptr;
    QPushButton*    delete_btn_     = nullptr;

    // Right: config form
    QStackedWidget* form_stack_     = nullptr;   // 0=empty, 1=form, 2=fincept

    // Form fields
    QLineEdit*      provider_edit_  = nullptr;
    QLineEdit*      api_key_edit_   = nullptr;
    QLineEdit*      base_url_edit_  = nullptr;
    QComboBox*      model_combo_    = nullptr;
    QPushButton*    fetch_btn_      = nullptr;
    QPushButton*    save_btn_       = nullptr;
    QPushButton*    test_btn_       = nullptr;
    QLabel*         status_lbl_     = nullptr;

    // Global settings
    QDoubleSpinBox* temp_spin_      = nullptr;
    QSpinBox*       tokens_spin_    = nullptr;
    QPlainTextEdit* system_prompt_  = nullptr;
    QPushButton*    save_global_btn_= nullptr;

    void build_ui();
    QWidget* build_provider_list_panel();
    QWidget* build_form_panel();
    QWidget* build_global_panel();

    void load_providers();
    void populate_form(const QString& provider);
    void show_status(const QString& msg, bool error = false);

    static const QStringList KNOWN_PROVIDERS;

    // Default base URLs per provider (empty = provider's default)
    static QString default_base_url(const QString& provider);
    // Suggested models when fetch is not available
    static QStringList fallback_models(const QString& provider);
};

} // namespace fincept::screens
