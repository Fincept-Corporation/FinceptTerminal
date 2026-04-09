#pragma once
// AccountManagementDialog — multi-account management dialog for equity trading.
// Allows adding, removing, renaming, and connecting broker accounts.
// Replaces the single-account EquityCredentials dialog.

#include "trading/BrokerAccount.h"
#include "trading/BrokerInterface.h"

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>

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
    QLabel* form_title_ = nullptr;
    QLabel* form_status_ = nullptr;
    QVBoxLayout* fields_layout_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;

    // Dynamic credential fields (rebuilt per broker profile)
    QVector<QLineEdit*> cred_fields_;
    QVector<trading::CredentialFieldDef> cred_field_defs_;

    // Currently selected account
    QString selected_account_id_;
};

} // namespace fincept::screens::equity
