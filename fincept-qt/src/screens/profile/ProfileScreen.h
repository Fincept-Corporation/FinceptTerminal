#pragma once
#include "screens/common/ISectionLinked.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class BarChart;
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// Profile & Account — 5 sections.
/// Sections: Overview, Usage, Security, Billing, Support
class ProfileScreen : public QWidget, public ISectionLinked {
    Q_OBJECT
  public:
    explicit ProfileScreen(QWidget* parent = nullptr);

    /// ISectionLinked — "overview" | "usage" | "security" | "billing" | "support" (the tab buttons;
    /// signed out: the tab is kept and shown after sign-in). focus unused.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;
    /// Re-checks the sign-in (the gate also follows AuthManager's signals).
    void showEvent(QShowEvent* event) override;

  private:
    QStackedWidget* sections_ = nullptr;
    QWidget* tab_nav_ = nullptr;

    // ── Sign-in gate ──────────────────────────────────────────────────────────
    /// Shown instead of the tab bar and the sections while signed out (the
    /// sign-in requirement page) or while the saved sign-in is still being
    /// checked (skeleton). Both created on first use.
    fincept::ui::EmptyState* signin_state_ = nullptr;
    fincept::ui::LoadingSkeleton* signin_skeleton_ = nullptr;
    /// The account the sections show ("" = signed out; "~" = signed in, identity not known yet).
    QString account_key_;
    /// Bumped when the account changes: a Usage / Security / Billing reply
    /// requested for an earlier account is dropped.
    quint64 account_generation_ = 0;
    /// Signed in → the sections; otherwise the gate. A sign-out or another
    /// account clears every section (nothing of the previous account stays).
    void apply_auth_gate();
    QList<QPushButton*> nav_buttons_;
    /// Source-language keys for each nav button, aligned by index with
    /// nav_buttons_. Used by retranslateUi() to reapply tr() lookups.
    QList<QString> nav_source_keys_;
    QLabel* header_title_ = nullptr;
    QPushButton* header_refresh_btn_ = nullptr;

    /// One stylesheet for the whole screen (and its dialogs), keyed by object
    /// names and the "tone" / "active" properties. Re-run on theme change.
    void apply_theme();

    /// Re-translate widgets we keep handles to, and rebuild the section
    /// widgets so tr() calls inside their build_* helpers re-run against the
    /// new translator.
    void retranslateUi();
    void rebuild_sections();

    // Header labels
    QLabel* username_header_ = nullptr;
    QLabel* credits_badge_ = nullptr;
    QLabel* plan_badge_ = nullptr;

    // Overview
    QLabel* ov_username_ = nullptr;
    QLabel* ov_email_ = nullptr;
    QLabel* ov_user_type_ = nullptr;
    QLabel* ov_account_type_ = nullptr;
    QLabel* ov_phone_ = nullptr;
    QLabel* ov_country_ = nullptr;
    QLabel* ov_verified_ = nullptr;
    QLabel* ov_mfa_ = nullptr;
    QLabel* ov_member_since_ = nullptr;
    QLabel* ov_last_login_ = nullptr;
    QLabel* ov_credits_big_ = nullptr;
    QLabel* ov_plan_ = nullptr;
    QLabel* ov_credits_expire_ = nullptr;

    // Security
    QLabel* sec_api_key_ = nullptr;
    QLabel* sec_mfa_ = nullptr;
    /// ENABLE 2FA / DISABLE 2FA beside the 2FA row (text follows the session).
    QPushButton* sec_mfa_btn_ = nullptr;
    bool mfa_request_in_flight_ = false;
    QLabel* sec_verified_ = nullptr;
    QTableWidget* sec_login_hist_ = nullptr;
    bool api_key_visible_ = false;

    // Usage
    QLabel* usg_credits_ = nullptr;
    QLabel* usg_plan_ = nullptr;
    QLabel* usg_rate_ = nullptr;
    QLabel* usg_total_req_ = nullptr;
    QLabel* usg_cred_used_ = nullptr;
    QLabel* usg_avg_cred_ = nullptr;
    QLabel* usg_avg_resp_ = nullptr;
    QTableWidget* usg_daily_table_ = nullptr;
    fincept::ui::BarChart* usg_daily_chart_ = nullptr; // credits per day, the 30-day series
    QTableWidget* usg_endpoint_table_ = nullptr;

    // Billing
    QLabel* bill_plan_ = nullptr;
    QLabel* bill_credits_ = nullptr;
    QLabel* bill_support_ = nullptr;
    QTableWidget* bill_history_ = nullptr;

    void build_header(QVBoxLayout* root);
    void build_tab_nav(QVBoxLayout* root);
    QWidget* build_overview();
    QWidget* build_usage();
    QWidget* build_security();
    QWidget* build_billing();
    QWidget* build_support();

    void refresh_all();
    void fetch_usage_data();
    void fetch_billing_data();
    void fetch_login_history();

    /// Wall-clock ms each section's server data was last requested (index =
    /// section; 0 = never / failed). Switching back to a tab within
    /// kTabFreshMs re-shows what is loaded instead of re-requesting it;
    /// REFRESH always re-requests.
    qint64 tab_fetched_ms_[5] = {};
    static constexpr qint64 kTabFreshMs = 2 * 60 * 1000;

    // Helpers
    QWidget* make_panel(const QString& title);
    QWidget* make_data_row(const QString& label, QLabel*& value_out);
    /// `tone` selects the value colour: "cyan", "amber", "warn" or "" (primary text).
    QWidget* make_stat_box(const QString& label, QLabel*& value_out, const char* tone);
    QScrollArea* make_scroll_page(QVBoxLayout*& layout_out);

    void show_edit_profile_dialog();
    void show_logout_confirm();
    void show_regen_confirm();
    /// Turn email 2FA on (confirm) or off (asks for the account password).
    void toggle_mfa();
    void show_delete_account_dialog();

  private slots:
    /// Show a section and load its server data unless loaded < kTabFreshMs ago
    /// (`force`: REFRESH — always load).
    void on_section_changed(int index, bool force = false);
};

} // namespace fincept::screens
