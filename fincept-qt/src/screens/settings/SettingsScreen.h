#pragma once
#include "screens/IStatefulScreen.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Full-featured settings screen.
/// Sections: Credentials | Appearance | Notifications | Storage & Cache |
///           Data Sources | LLM Config | MCP Servers | Logging | Security | Profiles
class SettingsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit SettingsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "settings"; }

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    QStackedWidget* sections_ = nullptr;
    QWidget* nav_ = nullptr;

    // ── Section builders ─────────────────────────────────────────────────────
    QWidget* build_credentials();
    QWidget* build_appearance();
    QWidget* build_notifications();
    QWidget* build_storage();
    QWidget* build_data_sources();
    QWidget* build_llm_config();
    QWidget* build_mcp_servers();
    QWidget* build_logging();
    QWidget* build_security();
    QWidget* build_profiles();
    QWidget* build_keybindings();
    QWidget* build_python_env();
    QWidget* build_developer();

    // ── Shared layout helper ──────────────────────────────────────────────────
    static QWidget* make_row(const QString& label, QWidget* control, const QString& description = {});
    static QFrame* make_sep();

    // ── Credentials state ─────────────────────────────────────────────────────
    QHash<QString, QLineEdit*> cred_fields_; // key → password field
    QHash<QString, QLabel*> cred_status_;    // key → status label

    // ── Appearance state ──────────────────────────────────────────────────────
    QComboBox* app_font_size_ = nullptr;
    QComboBox* app_font_family_ = nullptr;
    QComboBox* app_density_ = nullptr;
    QCheckBox* chat_bubble_toggle_ = nullptr;
    QCheckBox* ticker_bar_toggle_ = nullptr;
    QCheckBox* animations_toggle_ = nullptr;
    QTimer* appearance_debounce_ = nullptr; // coalesces rapid font/density changes

    // ── Notifications state ───────────────────────────────────────────────────
    // Per-provider accordion widgets (keyed by provider_id)
    struct ProviderWidgets {
        QCheckBox* enabled = nullptr;
        QHash<QString, QLineEdit*> fields; // field_key → input widget
        QPushButton* test_btn = nullptr;
        QFrame* body_frame = nullptr; // collapsible config area
        QLabel* status_lbl = nullptr; // inline test result
    };
    QHash<QString, ProviderWidgets> provider_widgets_;

    // Alert trigger checkboxes
    QCheckBox* trigger_inapp_ = nullptr;
    QCheckBox* trigger_price_ = nullptr;
    QCheckBox* trigger_news_ = nullptr;
    QCheckBox* trigger_orders_ = nullptr;

    // News alert sub-options (visible only when trigger_news_ is checked)
    QCheckBox* news_breaking_ = nullptr;
    QCheckBox* news_monitors_ = nullptr;
    QCheckBox* news_deviations_ = nullptr;
    QCheckBox* news_flash_ = nullptr;
    QFrame* news_subopts_frame_ = nullptr;

    // ── Storage state ─────────────────────────────────────────────────────────
    QLabel* storage_count_ = nullptr;
    QLabel* storage_main_db_ = nullptr;
    QLabel* storage_cache_db_ = nullptr;
    QLabel* storage_log_size_ = nullptr;
    QLabel* storage_ws_size_ = nullptr;
    QLabel* storage_total_size_ = nullptr;
    QVBoxLayout* storage_categories_ = nullptr;
    // SQL console
    QLineEdit* sql_input_ = nullptr;
    QComboBox* sql_db_selector_ = nullptr;
    QLabel* sql_status_ = nullptr;
    QVBoxLayout* sql_results_layout_ = nullptr;

    // ── Logging state ─────────────────────────────────────────────────────────
    QComboBox* log_global_level_ = nullptr;
    QWidget* log_tag_list_ = nullptr; // VBox container for tag rows
    QVBoxLayout* log_tag_layout_ = nullptr;
    QCheckBox* log_json_mode_ = nullptr;
    QLabel* log_path_label_ = nullptr;

    // ── Security state ──────────────────────────────────────────────────────────
    QLabel* sec_pin_status_ = nullptr;
    QComboBox* sec_lock_timeout_ = nullptr;
    QCheckBox* sec_autolock_toggle_ = nullptr;
    QCheckBox* sec_lock_on_minimize_ = nullptr;
    QListWidget* sec_audit_list_ = nullptr;
    QLabel* sec_lockout_status_ = nullptr;
    QPushButton* sec_change_pin_btn_ = nullptr;
    // Change PIN sub-widgets (shown/hidden dynamically)
    QWidget* sec_change_pin_form_ = nullptr;
    QLineEdit* sec_current_pin_ = nullptr;
    QLineEdit* sec_new_pin_ = nullptr;
    QLineEdit* sec_confirm_pin_ = nullptr;
    QLabel* sec_pin_error_ = nullptr;
    QLabel* sec_pin_success_ = nullptr;

    // ── Data loaders (called from showEvent) ──────────────────────────────────
    void refresh_theme();
    void load_credentials();
    void load_appearance();
    void load_notifications();
    void load_security();
    void refresh_audit_log();
    void refresh_storage_stats();

    // ── Notification helpers ──────────────────────────────────────────────────
    void save_provider_fields(const QString& provider_id, const ProviderWidgets& pw);
};

} // namespace fincept::screens
