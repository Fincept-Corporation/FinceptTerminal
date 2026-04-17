// AccountManagementDialog.cpp — multi-account credential management
#include "screens/equity_trading/AccountManagementDialog.h"

#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QVBoxLayout>

namespace fincept::screens::equity {

using namespace fincept::ui;
using namespace fincept::trading;

// ── helpers ──────────────────────────────────────────────────────────────────

static QLineEdit* make_field(const QString& placeholder, bool password = false) {
    auto* f = new QLineEdit;
    f->setPlaceholderText(placeholder);
    if (password)
        f->setEchoMode(QLineEdit::Password);
    return f;
}

static QLabel* make_field_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("fieldLabel");
    return l;
}

// ── constructor ──────────────────────────────────────────────────────────────

AccountManagementDialog::AccountManagementDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Manage Broker Accounts");
    setMinimumSize(700, 480);
    setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                          "QLabel#fieldLabel { color: %3; font-size: 11px; font-weight: 700; }"
                          "QLabel#titleLabel { color: %4; font-size: 14px; font-weight: 700; }"
                          "QLabel#statusLabel { font-size: 11px; }"
                          "QLineEdit { background: %5; border: 1px solid %6; color: %2;"
                          "  padding: 6px; font-size: 12px; border-radius: 2px; }"
                          "QLineEdit:focus { border-color: %4; }"
                          "QComboBox { background: %5; border: 1px solid %6; color: %2;"
                          "  padding: 4px 6px; font-size: 12px; }"
                          "QComboBox::drop-down { border: none; }"
                          "QComboBox QAbstractItemView { background: %7; color: %2;"
                          "  selection-background-color: rgba(217,119,6,0.3); }"
                          "QPushButton { padding: 8px 16px; font-weight: 700; font-size: 12px; border-radius: 2px; }"
                          "QListWidget { background: %5; border: 1px solid %6; color: %2; font-size: 12px; }"
                          "QListWidget::item { padding: 6px 8px; }"
                          "QListWidget::item:selected { background: rgba(217,119,6,0.2); color: %4; }")
                      .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY(),
                           colors::AMBER(), colors::BG_BASE(), colors::BORDER_MED(), colors::BG_RAISED()));
    setup_ui();
    refresh_account_list();
}

// ── UI setup ────────────────────────────────────────────────────────────────

void AccountManagementDialog::setup_ui() {
    auto* root = new QHBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(12, 12, 12, 12);

    // ── Left panel: account list + add/remove ──
    auto* left = new QVBoxLayout;
    left->setSpacing(6);

    auto* list_label = new QLabel("ACCOUNTS");
    list_label->setObjectName("fieldLabel");
    left->addWidget(list_label);

    account_list_ = new QListWidget;
    connect(account_list_, &QListWidget::currentRowChanged, this, &AccountManagementDialog::on_account_selected);
    left->addWidget(account_list_, 1);

    // Add account controls
    auto* add_row = new QHBoxLayout;
    broker_picker_ = new QComboBox;
    const auto brokers = BrokerRegistry::instance().list_brokers();
    for (const auto& bid : brokers) {
        auto* b = BrokerRegistry::instance().get(bid);
        if (b)
            broker_picker_->addItem(b->profile().display_name, bid);
    }
    add_row->addWidget(broker_picker_, 1);

    display_name_input_ = new QLineEdit;
    display_name_input_->setPlaceholderText("Account name...");
    add_row->addWidget(display_name_input_, 1);
    left->addLayout(add_row);

    auto* btn_row = new QHBoxLayout;
    add_btn_ = new QPushButton("+ ADD");
    add_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                .arg(colors::AMBER(), colors::BG_BASE()));
    connect(add_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_add_account);
    btn_row->addWidget(add_btn_);

    remove_btn_ = new QPushButton("REMOVE");
    remove_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                   .arg(colors::NEGATIVE(), colors::TEXT_PRIMARY()));
    remove_btn_->setEnabled(false);
    connect(remove_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_remove_account);
    btn_row->addWidget(remove_btn_);
    left->addLayout(btn_row);

    root->addLayout(left, 1);

    // ── Right panel: credential form (stacked) ──
    right_stack_ = new QStackedWidget;

    // Empty page (no selection)
    empty_page_ = new QWidget(this);
    auto* empty_layout = new QVBoxLayout(empty_page_);
    auto* empty_label = new QLabel("Select an account to configure credentials");
    empty_label->setAlignment(Qt::AlignCenter);
    empty_label->setStyleSheet(QString("color: %1; font-size: 12px;").arg(colors::TEXT_TERTIARY()));
    empty_layout->addWidget(empty_label);
    right_stack_->addWidget(empty_page_);

    // Form page
    form_page_ = new QWidget(this);
    auto* form_layout = new QVBoxLayout(form_page_);
    form_layout->setSpacing(8);
    form_layout->setContentsMargins(0, 0, 0, 0);

    form_title_ = new QLabel;
    form_title_->setObjectName("titleLabel");
    form_layout->addWidget(form_title_);

    form_status_ = new QLabel;
    form_status_->setObjectName("statusLabel");
    form_layout->addWidget(form_status_);

    // Dynamic fields container
    auto* fields_widget = new QWidget(this);
    fields_layout_ = new QVBoxLayout(fields_widget);
    fields_layout_->setSpacing(4);
    fields_layout_->setContentsMargins(0, 0, 0, 0);
    form_layout->addWidget(fields_widget);

    form_layout->addStretch();

    // Action buttons
    auto* action_row = new QHBoxLayout;
    rename_btn_ = new QPushButton("RENAME");
    rename_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                   .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY()));
    connect(rename_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_rename_account);
    action_row->addWidget(rename_btn_);

    connect_btn_ = new QPushButton("CONNECT");
    connect_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                    .arg(colors::AMBER(), colors::BG_BASE()));
    connect(connect_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_connect_account);
    action_row->addWidget(connect_btn_);
    form_layout->addLayout(action_row);

    right_stack_->addWidget(form_page_);
    right_stack_->setCurrentWidget(empty_page_);

    root->addWidget(right_stack_, 2);
}

// ── Account list ────────────────────────────────────────────────────────────

void AccountManagementDialog::refresh_account_list() {
    account_list_->clear();
    const auto accounts = AccountManager::instance().list_accounts();
    for (const auto& account : accounts) {
        auto* broker = BrokerRegistry::instance().get(account.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : account.broker_id;
        const QString state_icon = account.state == ConnectionState::Connected ? QString::fromUtf8("\xe2\x97\x8f") // ●
                                 : account.state == ConnectionState::Error     ? QString::fromUtf8("\xe2\x97\x8f") // ●
                                                                               : QString::fromUtf8("\xe2\x97\x8b"); // ○
        const QString text = QString("%1 %2 [%3]").arg(state_icon, account.display_name, broker_name);
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, account.account_id);
        if (account.state == ConnectionState::Connected)
            item->setForeground(QColor(colors::POSITIVE()));
        else if (account.state == ConnectionState::Error || account.state == ConnectionState::TokenExpired)
            item->setForeground(QColor(colors::NEGATIVE()));
        account_list_->addItem(item);
    }
    remove_btn_->setEnabled(false);
}

void AccountManagementDialog::on_account_selected(int row) {
    if (row < 0) {
        right_stack_->setCurrentWidget(empty_page_);
        remove_btn_->setEnabled(false);
        selected_account_id_.clear();
        return;
    }

    auto* item = account_list_->item(row);
    selected_account_id_ = item->data(Qt::UserRole).toString();
    remove_btn_->setEnabled(true);

    auto account = AccountManager::instance().get_account(selected_account_id_);
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker) {
        right_stack_->setCurrentWidget(empty_page_);
        return;
    }

    const auto prof = broker->profile();
    form_title_->setText(QString("%1 — %2 | %3").arg(account.display_name, prof.region, prof.currency));

    // Update status
    const QString state_str = connection_state_str(account.state);
    form_status_->setText(QString("Status: %1").arg(state_str));
    if (account.state == ConnectionState::Connected)
        form_status_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
    else if (account.state == ConnectionState::Error || account.state == ConnectionState::TokenExpired)
        form_status_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
    else
        form_status_->setStyleSheet(QString("color: %1;").arg(colors::TEXT_SECONDARY()));

    build_credential_form(prof);
    load_saved_credentials(selected_account_id_);

    right_stack_->setCurrentWidget(form_page_);
}

// ── Credential form ─────────────────────────────────────────────────────────

void AccountManagementDialog::build_credential_form(const trading::BrokerProfile& profile) {
    // Clear existing fields
    for (auto* f : cred_fields_)
        f->deleteLater();
    cred_fields_.clear();
    cred_field_defs_ = profile.credential_fields;

    // Remove old widgets from layout
    while (fields_layout_->count() > 0) {
        auto* item = fields_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // Build fields from BrokerProfile::credential_fields
    for (const auto& def : profile.credential_fields) {
        if (def.field == CredentialField::Environment)
            continue; // Environment is handled by per-account trading mode, not here
        fields_layout_->addWidget(make_field_label(def.label));
        auto* field = make_field(def.placeholder, def.secret);
        fields_layout_->addWidget(field);
        cred_fields_.append(field);
    }
}

void AccountManagementDialog::load_saved_credentials(const QString& account_id) {
    auto creds = AccountManager::instance().load_credentials(account_id);
    int idx = 0;
    for (const auto& def : cred_field_defs_) {
        if (def.field == CredentialField::Environment)
            continue;
        if (idx >= cred_fields_.size())
            break;
        switch (def.field) {
        case CredentialField::ApiKey:
            cred_fields_[idx]->setText(creds.api_key);
            break;
        case CredentialField::ApiSecret:
            cred_fields_[idx]->setText(creds.api_secret);
            break;
        case CredentialField::AuthCode:
            // Auth codes are ephemeral — don't pre-fill
            break;
        case CredentialField::ClientCode: {
            // Parse from additional_data JSON
            auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
            if (doc.isObject())
                cred_fields_[idx]->setText(doc.object().value("client_code").toString());
            break;
        }
        default:
            break;
        }
        ++idx;
    }
}

// ── Actions ─────────────────────────────────────────────────────────────────

void AccountManagementDialog::on_add_account() {
    const QString broker_id = broker_picker_->currentData().toString();
    QString name = display_name_input_->text().trimmed();
    if (name.isEmpty()) {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        name = broker ? broker->profile().display_name : broker_id;
        // Append count if multiple accounts for same broker
        const auto existing = AccountManager::instance().list_accounts(broker_id);
        if (!existing.isEmpty())
            name += QString(" #%1").arg(existing.size() + 1);
    }

    auto account = AccountManager::instance().add_account(broker_id, name);
    display_name_input_->clear();
    refresh_account_list();

    // Select the new account
    for (int i = 0; i < account_list_->count(); ++i) {
        if (account_list_->item(i)->data(Qt::UserRole).toString() == account.account_id) {
            account_list_->setCurrentRow(i);
            break;
        }
    }

    emit account_added(account.account_id);
}

void AccountManagementDialog::on_remove_account() {
    if (selected_account_id_.isEmpty())
        return;

    auto account = AccountManager::instance().get_account(selected_account_id_);
    auto reply = QMessageBox::warning(this, "Remove Account",
                                      QString("Are you sure you want to remove \"%1\"?\n\n"
                                              "This will delete all saved credentials and the linked paper portfolio.\n"
                                              "This action cannot be undone.")
                                          .arg(account.display_name),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    const QString removed_id = selected_account_id_;
    selected_account_id_.clear();
    AccountManager::instance().remove_account(removed_id);
    refresh_account_list();
    right_stack_->setCurrentWidget(empty_page_);

    emit account_removed(removed_id);
}

void AccountManagementDialog::on_connect_account() {
    if (selected_account_id_.isEmpty())
        return;

    auto account = AccountManager::instance().get_account(selected_account_id_);
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return;

    // Collect credentials from form fields
    BrokerCredentials creds;
    creds.broker_id = account.broker_id;
    QString auth_code;
    int idx = 0;
    for (const auto& def : cred_field_defs_) {
        if (def.field == CredentialField::Environment)
            continue;
        if (idx >= cred_fields_.size())
            break;
        const QString val = cred_fields_[idx]->text().trimmed();
        switch (def.field) {
        case CredentialField::ApiKey:
            creds.api_key = val;
            break;
        case CredentialField::ApiSecret:
            creds.api_secret = val;
            break;
        case CredentialField::AuthCode:
            auth_code = val;
            break;
        case CredentialField::ClientCode:
            // Store in additional_data as JSON
            creds.additional_data = QJsonDocument(QJsonObject{{"client_code", val}}).toJson(QJsonDocument::Compact);
            break;
        default:
            break;
        }
        ++idx;
    }

    // Attempt token exchange
    form_status_->setText("Connecting...");
    form_status_->setStyleSheet(QString("color: %1;").arg(colors::AMBER()));

    auto result = broker->exchange_token(creds.api_key, creds.api_secret, auth_code);
    if (!result.success) {
        form_status_->setText(QString("Error: %1").arg(result.error));
        form_status_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        AccountManager::instance().set_connection_state(selected_account_id_, ConnectionState::Error, result.error);
        refresh_account_list();
        return;
    }

    // Merge token exchange results into credentials
    creds.access_token = result.access_token;
    if (!result.user_id.isEmpty())
        creds.user_id = result.user_id;
    if (!result.additional_data.isEmpty()) {
        // Merge additional_data (keep existing fields like client_code)
        auto existing = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        auto extra = QJsonDocument::fromJson(result.additional_data.toUtf8()).object();
        for (auto it = extra.begin(); it != extra.end(); ++it)
            existing[it.key()] = it.value();
        creds.additional_data = QJsonDocument(existing).toJson(QJsonDocument::Compact);
    }

    // Save credentials and update state
    AccountManager::instance().save_credentials(selected_account_id_, creds);
    AccountManager::instance().set_connection_state(selected_account_id_, ConnectionState::Connected);

    form_status_->setText(QString("Connected as %1").arg(creds.user_id.isEmpty() ? creds.api_key.left(8) + "..." : creds.user_id));
    form_status_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
    refresh_account_list();

    emit credentials_saved(selected_account_id_);
}

void AccountManagementDialog::on_rename_account() {
    if (selected_account_id_.isEmpty())
        return;
    const QString new_name = display_name_input_->text().trimmed();
    if (new_name.isEmpty())
        return;
    AccountManager::instance().update_display_name(selected_account_id_, new_name);
    display_name_input_->clear();
    refresh_account_list();
    // Re-select the account
    for (int i = 0; i < account_list_->count(); ++i) {
        if (account_list_->item(i)->data(Qt::UserRole).toString() == selected_account_id_) {
            account_list_->setCurrentRow(i);
            break;
        }
    }
}

} // namespace fincept::screens::equity
