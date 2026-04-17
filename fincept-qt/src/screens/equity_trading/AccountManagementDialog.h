#pragma once
// AccountManagementDialog — multi-account management dialog for equity trading.
// Allows adding, removing, renaming, and connecting broker accounts.
// Replaces the single-account EquityCredentials dialog.

#include "trading/BrokerAccount.h"
#include "trading/BrokerInterface.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::trading::auth {
class RedirectServer;
}

namespace fincept::screens::equity {

class AccountManagementDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AccountManagementDialog(QWidget* parent = nullptr);

  signals:
    void account_added(const QString& account_id);
    void account_removed(const QString& account_id);
    void credentials_saved(const QString& account_id);

  private:
    void setup_ui();
    void refresh_account_list();
    void on_account_selected(int row);
    void on_add_account();
    void on_remove_account();
    void on_connect_account();
    void on_rename_account();
    void build_credential_form(const trading::BrokerProfile& profile);
    void load_saved_credentials(const QString& account_id);

    // Zerodha-specific form + handlers
    void build_zerodha_form();
    void on_zerodha_mode_changed();
    void on_connect_zerodha_totp();
    void on_connect_zerodha_browser();
    void on_connect_zerodha_manual_paste();
    void persist_zerodha_creds_before_auth();
    void exchange_and_store_token_async(const QString& api_key, const QString& api_secret,
                                        const QString& request_token);

    // Left panel — account list
    QListWidget* account_list_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QComboBox* broker_picker_ = nullptr;
    QLineEdit* display_name_input_ = nullptr;

    // Right panel — credential form
    QStackedWidget* right_stack_ = nullptr;
    QWidget* empty_page_ = nullptr;
    QWidget* form_page_ = nullptr;
    QWidget* zerodha_page_ = nullptr;
    QLabel* form_title_ = nullptr;
    QLabel* form_status_ = nullptr;
    QVBoxLayout* fields_layout_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;

    // Dynamic credential fields (rebuilt per broker profile)
    QVector<QLineEdit*> cred_fields_;
    QVector<trading::CredentialFieldDef> cred_field_defs_;

    // Zerodha-specific widgets
    QLabel*        z_title_ = nullptr;
    QLabel*        z_status_ = nullptr;
    QPushButton*   z_setup_toggle_ = nullptr;
    QWidget*       z_setup_panel_ = nullptr;
    QRadioButton*  z_mode_totp_ = nullptr;
    QRadioButton*  z_mode_browser_ = nullptr;
    QLineEdit*     z_api_key_ = nullptr;
    QLineEdit*     z_api_secret_ = nullptr;
    QWidget*       z_totp_group_ = nullptr;
    QLineEdit*     z_user_id_ = nullptr;
    QLineEdit*     z_password_ = nullptr;
    QLineEdit*     z_totp_secret_ = nullptr;
    QWidget*       z_browser_group_ = nullptr;
    QPushButton*   z_browser_btn_ = nullptr;
    QPushButton*   z_manual_toggle_ = nullptr;
    QWidget*       z_manual_panel_ = nullptr;
    QLineEdit*     z_manual_token_ = nullptr;
    QPushButton*   z_manual_connect_btn_ = nullptr;
    QPushButton*   z_connect_btn_ = nullptr;
    QPushButton*   z_rename_btn_ = nullptr;

    trading::auth::RedirectServer* z_redirect_server_ = nullptr;

    // Currently selected account
    QString selected_account_id_;
};

} // namespace fincept::screens::equity
