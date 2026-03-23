#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Profile & Account — 5 sections matching Tauri's ProfileTab.
/// Sections: Overview, Usage, Security, Billing, Support
class ProfileScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ProfileScreen(QWidget* parent = nullptr);

  private:
    QStackedWidget* sections_ = nullptr;

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
    QLabel* ov_credits_big_ = nullptr;
    QLabel* ov_plan_ = nullptr;

    // Security
    QLabel* sec_api_key_ = nullptr;
    QLabel* sec_mfa_ = nullptr;
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

    // Helpers
    QWidget* make_panel(const QString& title);
    QWidget* make_data_row(const QString& label, QLabel*& value_out);
    QWidget* make_stat_box(const QString& label, QLabel*& value_out, const QString& color = "#0891b2");

    void show_edit_profile_dialog();
    void show_logout_confirm();
    void show_regen_confirm();

  private slots:
    void on_section_changed(int index);
};

} // namespace fincept::screens
