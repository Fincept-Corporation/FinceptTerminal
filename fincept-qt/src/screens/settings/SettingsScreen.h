#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Full-featured settings screen.
/// Sections: Credentials | Appearance | Notifications | Storage & Cache |
///           Data Sources | LLM Config | MCP Servers
class SettingsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit SettingsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    QStackedWidget* sections_ = nullptr;

    // ── Section builders ─────────────────────────────────────────────────────
    QWidget* build_credentials();
    QWidget* build_appearance();
    QWidget* build_notifications();
    QWidget* build_storage();
    QWidget* build_data_sources();
    QWidget* build_llm_config();
    QWidget* build_mcp_servers();

    // ── Shared layout helper ──────────────────────────────────────────────────
    static QWidget* make_row(const QString& label, QWidget* control,
                             const QString& description = {});
    static QFrame*  make_sep();

    // ── Credentials state ─────────────────────────────────────────────────────
    QHash<QString, QLineEdit*> cred_fields_;   // key → password field
    QHash<QString, QLabel*>    cred_status_;   // key → status label

    // ── Appearance state ──────────────────────────────────────────────────────
    QComboBox* app_font_size_   = nullptr;
    QComboBox* app_font_family_ = nullptr;
    QComboBox* app_theme_       = nullptr;
    QComboBox* app_density_     = nullptr;

    // ── Notifications state ───────────────────────────────────────────────────
    QCheckBox* notif_email_     = nullptr;
    QLineEdit* notif_email_addr_= nullptr;
    QCheckBox* notif_inapp_     = nullptr;
    QCheckBox* notif_price_     = nullptr;
    QCheckBox* notif_news_      = nullptr;
    QCheckBox* notif_orders_    = nullptr;
    QCheckBox* notif_sound_     = nullptr;

    // ── Storage state ─────────────────────────────────────────────────────────
    QLabel*    storage_count_   = nullptr;

    // ── Data loaders (called from showEvent) ──────────────────────────────────
    void load_credentials();
    void load_appearance();
    void load_notifications();
    void refresh_storage_stats();
};

} // namespace fincept::screens
