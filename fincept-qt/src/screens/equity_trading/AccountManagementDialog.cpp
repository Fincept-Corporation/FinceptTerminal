// AccountManagementDialog.cpp — multi-account credential management
#include "screens/equity_trading/AccountManagementDialog.h"

#include "core/logging/Logger.h"

#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/auth/RedirectServer.h"
#include "trading/brokers/zerodha/ZerodhaBroker.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QRadioButton>
#include <QScrollArea>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

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

    // Zerodha-specific page (populated on-demand by build_zerodha_form())
    zerodha_page_ = new QWidget(this);
    right_stack_->addWidget(zerodha_page_);

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

    // Zerodha gets its own dedicated form; all others use the generic builder.
    if (account.broker_id == QStringLiteral("zerodha")) {
        build_zerodha_form();
        const QString state_str = connection_state_str(account.state);
        if (z_title_)
            z_title_->setText(QString("%1 — %2 | %3").arg(account.display_name, prof.region, prof.currency));
        if (z_status_) {
            z_status_->setText(QString("Status: %1").arg(state_str));
            if (account.state == ConnectionState::Connected)
                z_status_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
            else if (account.state == ConnectionState::Error || account.state == ConnectionState::TokenExpired)
                z_status_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
            else
                z_status_->setStyleSheet(QString("color: %1;").arg(colors::TEXT_SECONDARY()));
        }
        load_saved_credentials(selected_account_id_);
        right_stack_->setCurrentWidget(zerodha_page_);
        return;
    }

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

    // Zerodha branch — populate the dedicated form.
    if (creds.broker_id == QStringLiteral("zerodha") && z_api_key_) {
        z_api_key_->setText(creds.api_key);
        z_api_secret_->setText(creds.api_secret);
        z_user_id_->setText(creds.user_id);
        const auto extra = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
        z_password_->setText(extra.value("password").toString());
        z_totp_secret_->setText(extra.value("totp_secret").toString());
        return;
    }

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
    if (account.account_id.isEmpty()) {
        // AccountManager returns an empty account on persistence failure (e.g. FK
        // violation, paper-portfolio creation failed). Surface this instead of
        // silently pretending the account was added.
        QMessageBox::critical(this, "Add Account",
                              "Could not save the account to the database. "
                              "Check the application log for details.");
        return;
    }
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

    // Attempt token exchange asynchronously — exchange_token() makes a blocking
    // HTTP call (BrokerHttp::execute uses QEventLoop), so running it on the UI
    // thread freezes the entire terminal until the request completes. Offload
    // to a worker thread and post results back via QMetaObject::invokeMethod.
    form_status_->setText("Connecting...");
    form_status_->setStyleSheet(QString("color: %1;").arg(colors::AMBER()));
    connect_btn_->setEnabled(false);

    const QString account_id = selected_account_id_;
    QPointer<AccountManagementDialog> self = this;
    const QString api_key_val = creds.api_key;
    const QString api_secret_val = creds.api_secret;
    const QString auth_code_val = auth_code;
    const QString existing_additional = creds.additional_data;

    QtConcurrent::run([self, broker, account_id, api_key_val, api_secret_val, auth_code_val, existing_additional]() {
        auto result = broker->exchange_token(api_key_val, api_secret_val, auth_code_val);

        QMetaObject::invokeMethod(
            qApp,
            [self, account_id, api_key_val, api_secret_val, auth_code_val, existing_additional, result]() {
                // Always update AccountManager — it's thread-safe and survives dialog close
                if (!result.success) {
                    AccountManager::instance().set_connection_state(account_id, ConnectionState::Error, result.error);
                    if (self) {
                        self->form_status_->setText(QString("Error: %1").arg(result.error));
                        self->form_status_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
                        self->connect_btn_->setEnabled(true);
                        if (self->selected_account_id_ == account_id)
                            self->refresh_account_list();
                    }
                    return;
                }

                // Build merged credentials
                BrokerCredentials creds;
                creds.broker_id = AccountManager::instance().get_account(account_id).broker_id;
                creds.api_key = api_key_val;
                creds.api_secret = api_secret_val;
                creds.access_token = result.access_token;
                if (!result.user_id.isEmpty())
                    creds.user_id = result.user_id;

                // Merge additional_data (keep existing like client_code, add new from token response)
                auto existing = QJsonDocument::fromJson(existing_additional.toUtf8()).object();
                auto extra = QJsonDocument::fromJson(result.additional_data.toUtf8()).object();
                for (auto it = extra.begin(); it != extra.end(); ++it)
                    existing[it.key()] = it.value();
                if (!existing.isEmpty())
                    creds.additional_data = QJsonDocument(existing).toJson(QJsonDocument::Compact);

                AccountManager::instance().save_credentials(account_id, creds);
                AccountManager::instance().set_connection_state(account_id, ConnectionState::Connected);

                // Emit signal so EquityTradingScreen starts the data stream, even if the
                // dialog has been closed in the meantime.
                if (self) {
                    emit self->credentials_saved(account_id);
                    self->form_status_->setText(QString("Connected as %1")
                                                     .arg(creds.user_id.isEmpty() ? creds.api_key.left(8) + "..."
                                                                                  : creds.user_id));
                    self->form_status_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
                    self->connect_btn_->setEnabled(true);
                    if (self->selected_account_id_ == account_id)
                        self->refresh_account_list();
                }
            },
            Qt::QueuedConnection);
    });
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

// ── Zerodha-specific form ───────────────────────────────────────────────────

void AccountManagementDialog::build_zerodha_form() {
    // Idempotent — rebuild only if not yet built.
    if (z_api_key_ != nullptr)
        return;

    auto* outer = new QVBoxLayout(zerodha_page_);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(zerodha_page_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* container = new QWidget;
    scroll->setWidget(container);
    auto* v = new QVBoxLayout(container);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(10);

    // Title + status
    z_title_ = new QLabel("Zerodha");
    z_title_->setObjectName("titleLabel");
    v->addWidget(z_title_);
    z_status_ = new QLabel(" ");
    z_status_->setObjectName("statusLabel");
    v->addWidget(z_status_);

    // Collapsible "First-time setup" panel
    z_setup_toggle_ = new QPushButton(QString::fromUtf8("\xe2\x96\xb8") + QStringLiteral(" First-time setup (4 steps)"));
    z_setup_toggle_->setStyleSheet(QString("text-align:left;background:transparent;color:%1;"
                                           "border:none;padding:4px 0;font-weight:700;")
                                       .arg(colors::AMBER()));
    v->addWidget(z_setup_toggle_);

    z_setup_panel_ = new QWidget;
    auto* setup = new QVBoxLayout(z_setup_panel_);
    setup->setContentsMargins(12, 4, 12, 12);
    setup->setSpacing(4);
    auto add_step = [&](const QString& text) {
        auto* l = new QLabel(text);
        l->setWordWrap(true);
        l->setTextFormat(Qt::RichText);
        l->setOpenExternalLinks(true);
        l->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::TEXT_SECONDARY()));
        setup->addWidget(l);
    };
    add_step("1. Create a Kite Connect app at <a href='https://developers.kite.trade/apps'>developers.kite.trade/apps</a>.");
    add_step("2. Register redirect URL <b>http://127.0.0.1:5010/</b> on that app.");
    add_step("3. Enable TOTP 2FA on your Zerodha account (<a href='https://support.zerodha.com/category/your-zerodha-account/login-credentials/articles/time-based-otp'>support.zerodha.com</a>).");
    add_step("4. For auto-login, copy the Base32 TOTP secret during 2FA setup (\"Can't scan?\" link).");
    z_setup_panel_->setVisible(false);
    v->addWidget(z_setup_panel_);

    QPointer<AccountManagementDialog> self = this;
    connect(z_setup_toggle_, &QPushButton::clicked, this, [self]() {
        if (!self) return;
        const bool vis = !self->z_setup_panel_->isVisible();
        self->z_setup_panel_->setVisible(vis);
        self->z_setup_toggle_->setText((vis ? QString::fromUtf8("\xe2\x96\xbe") : QString::fromUtf8("\xe2\x96\xb8"))
                                       + QStringLiteral(" First-time setup (4 steps)"));
    });

    // Mode radio
    auto* mode_row = new QHBoxLayout;
    mode_row->setSpacing(16);
    z_mode_totp_ = new QRadioButton("Auto-login (TOTP)");
    z_mode_browser_ = new QRadioButton("Browser login");
    z_mode_totp_->setChecked(true);
    mode_row->addWidget(z_mode_totp_);
    mode_row->addWidget(z_mode_browser_);
    mode_row->addStretch();
    v->addLayout(mode_row);

    auto add_labeled_field = [&](QBoxLayout* lay, const QString& label, QLineEdit*& out,
                                 const QString& placeholder, const QString& hint, bool password) {
        auto* hdr = make_field_label(label);
        lay->addWidget(hdr);
        out = make_field(placeholder, password);
        lay->addWidget(out);
        if (!hint.isEmpty()) {
            auto* h = new QLabel(hint);
            h->setStyleSheet(QString("color:%1;font-size:10px;margin-bottom:4px;")
                                 .arg(colors::TEXT_SECONDARY()));
            lay->addWidget(h);
        }
    };

    // Always-visible
    add_labeled_field(v, "API KEY", z_api_key_, "Enter API Key...",
                      "from developers.kite.trade -> My Apps", false);
    add_labeled_field(v, "API SECRET", z_api_secret_, "Enter API Secret...",
                      "same console, shown once - regenerate if lost", true);

    // TOTP group
    z_totp_group_ = new QWidget;
    auto* tv = new QVBoxLayout(z_totp_group_);
    tv->setContentsMargins(0, 0, 0, 0);
    tv->setSpacing(6);
    add_labeled_field(tv, "KITE USER ID", z_user_id_, "e.g. AB1234", "", false);
    add_labeled_field(tv, "PASSWORD", z_password_, "Zerodha login password", "", true);
    add_labeled_field(tv, "TOTP SECRET", z_totp_secret_, "Base32 string",
                      "Base32 secret from Zerodha 2FA setup", true);
    v->addWidget(z_totp_group_);

    // Browser group
    z_browser_group_ = new QWidget;
    auto* bv = new QVBoxLayout(z_browser_group_);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(6);
    z_browser_btn_ = new QPushButton("Open Kite login in browser");
    z_browser_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; padding: 10px; font-weight: 700; }")
                                       .arg(colors::AMBER(), colors::BG_BASE()));
    bv->addWidget(z_browser_btn_);
    z_manual_toggle_ = new QPushButton("Redirect didn't work? Paste request_token manually");
    z_manual_toggle_->setStyleSheet(QString("text-align:left;background:transparent;color:%1;"
                                            "border:none;padding:4px 0;font-size:11px;")
                                        .arg(colors::TEXT_SECONDARY()));
    bv->addWidget(z_manual_toggle_);
    z_manual_panel_ = new QWidget;
    auto* mv = new QVBoxLayout(z_manual_panel_);
    mv->setContentsMargins(0, 0, 0, 0);
    mv->setSpacing(4);
    z_manual_token_ = make_field("Paste request_token here", false);
    mv->addWidget(z_manual_token_);
    z_manual_connect_btn_ = new QPushButton("Connect with pasted token");
    z_manual_connect_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                              .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY()));
    mv->addWidget(z_manual_connect_btn_);
    z_manual_panel_->setVisible(false);
    bv->addWidget(z_manual_panel_);
    z_browser_group_->setVisible(false);
    v->addWidget(z_browser_group_);

    connect(z_manual_toggle_, &QPushButton::clicked, this, [self]() {
        if (!self) return;
        self->z_manual_panel_->setVisible(!self->z_manual_panel_->isVisible());
    });

    v->addStretch();

    // Bottom action row
    auto* btn_row = new QHBoxLayout;
    z_rename_btn_ = new QPushButton("RENAME");
    z_rename_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                      .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY()));
    btn_row->addWidget(z_rename_btn_);
    z_connect_btn_ = new QPushButton("CONNECT");
    z_connect_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; padding: 8px 24px; }")
                                       .arg(colors::AMBER(), colors::BG_BASE()));
    btn_row->addWidget(z_connect_btn_);
    v->addLayout(btn_row);

    // Wire signals
    connect(z_mode_totp_, &QRadioButton::toggled, this, &AccountManagementDialog::on_zerodha_mode_changed);
    connect(z_connect_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_connect_zerodha_totp);
    connect(z_browser_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_connect_zerodha_browser);
    connect(z_manual_connect_btn_, &QPushButton::clicked, this,
            &AccountManagementDialog::on_connect_zerodha_manual_paste);
    connect(z_rename_btn_, &QPushButton::clicked, this, &AccountManagementDialog::on_rename_account);

    on_zerodha_mode_changed();
}

void AccountManagementDialog::on_zerodha_mode_changed() {
    if (!z_mode_totp_)
        return;
    const bool totp = z_mode_totp_->isChecked();
    z_totp_group_->setVisible(totp);
    z_browser_group_->setVisible(!totp);
    z_connect_btn_->setVisible(totp);
}

void AccountManagementDialog::persist_zerodha_creds_before_auth() {
    BrokerCredentials creds;
    creds.broker_id = "zerodha";
    creds.api_key = z_api_key_->text().trimmed();
    creds.api_secret = z_api_secret_->text();
    creds.user_id = z_user_id_->text().trimmed();

    QJsonObject extra;
    extra.insert("password", z_password_->text());
    extra.insert("totp_secret", z_totp_secret_->text().trimmed());
    creds.additional_data = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));

    AccountManager::instance().save_credentials(selected_account_id_, creds);
}

void AccountManagementDialog::on_connect_zerodha_totp() {
    if (selected_account_id_.isEmpty())
        return;

    const QString api_key = z_api_key_->text().trimmed();
    const QString api_secret = z_api_secret_->text();
    const QString user_id = z_user_id_->text().trimmed();
    const QString password = z_password_->text();
    const QString totp_secret = z_totp_secret_->text().trimmed();

    auto show_err = [this](const QString& msg) {
        z_status_->setText(msg);
        z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
    };
    if (api_key.isEmpty())    { show_err("Missing field: API Key"); return; }
    if (api_secret.isEmpty()) { show_err("Missing field: API Secret"); return; }
    if (user_id.isEmpty())    { show_err("Missing field: Kite User ID"); return; }
    if (password.isEmpty())   { show_err("Missing field: Password"); return; }
    if (totp_secret.isEmpty()){ show_err("Missing field: TOTP Secret"); return; }

    // BUG 1 FIX: persist immediately so failure doesn't wipe the form.
    persist_zerodha_creds_before_auth();

    z_status_->setText("Logging in...");
    z_status_->setStyleSheet(QString("color:%1;").arg(colors::AMBER()));
    z_connect_btn_->setEnabled(false);

    const QString acct = selected_account_id_;
    QPointer<AccountManagementDialog> self = this;

    QtConcurrent::run([self, acct, user_id, password, api_key, api_secret, totp_secret]() {
        trading::ZerodhaBroker broker;
        auto stage_cb = [self](const QString& stage) {
            QMetaObject::invokeMethod(qApp, [self, stage]() {
                if (!self || !self->z_status_) return;
                self->z_status_->setText(stage);
            }, Qt::QueuedConnection);
        };
        auto result = broker.login_with_totp(user_id, password, api_key, api_secret,
                                             totp_secret, stage_cb);

        QMetaObject::invokeMethod(qApp, [self, acct, result]() {
            if (!self) return;
            auto& am = AccountManager::instance();

            if (result.success) {
                auto creds = am.load_credentials(acct);
                creds.access_token = result.access_token;
                if (!result.user_id.isEmpty())
                    creds.user_id = result.user_id;
                // Merge broker-returned extras into saved additional_data JSON.
                auto saved_obj = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
                const auto returned_obj =
                    QJsonDocument::fromJson(result.additional_data.toUtf8()).object();
                for (auto it = returned_obj.begin(); it != returned_obj.end(); ++it)
                    saved_obj.insert(it.key(), it.value());
                creds.additional_data =
                    QString::fromUtf8(QJsonDocument(saved_obj).toJson(QJsonDocument::Compact));
                am.save_credentials(acct, creds);
                am.set_connection_state(acct, ConnectionState::Connected, {});
                self->z_status_->setText(QString("Connected as %1").arg(result.user_id));
                self->z_status_->setStyleSheet(QString("color:%1;").arg(colors::POSITIVE()));
                emit self->credentials_saved(acct);
            } else {
                am.set_connection_state(acct, ConnectionState::Error, result.error);
                self->z_status_->setText(result.error);
                self->z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
            }
            self->z_connect_btn_->setEnabled(true);
            self->refresh_account_list();
        }, Qt::QueuedConnection);
    });
}

void AccountManagementDialog::on_connect_zerodha_browser() {
    if (selected_account_id_.isEmpty())
        return;

    const QString api_key = z_api_key_->text().trimmed();
    const QString api_secret = z_api_secret_->text();
    if (api_key.isEmpty() || api_secret.isEmpty()) {
        z_status_->setText("Enter API Key and API Secret first");
        z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
        return;
    }

    // BUG 1 FIX: persist key+secret now.
    persist_zerodha_creds_before_auth();

    // Tear down any prior server
    if (z_redirect_server_) {
        z_redirect_server_->deleteLater();
        z_redirect_server_ = nullptr;
    }
    z_redirect_server_ = new trading::auth::RedirectServer(this);

    if (!z_redirect_server_->start(5010, 120)) {
        z_status_->setText("Port 5010 busy - use manual paste fallback");
        z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
        z_redirect_server_->deleteLater();
        z_redirect_server_ = nullptr;
        z_manual_panel_->setVisible(true);
        return;
    }

    QPointer<AccountManagementDialog> self = this;
    connect(z_redirect_server_, &trading::auth::RedirectServer::request_token_received, this,
            [self, api_key, api_secret](const QString& token) {
                if (!self) return;
                self->z_status_->setText("Exchanging token...");
                self->exchange_and_store_token_async(api_key, api_secret, token);
                if (self->z_redirect_server_) {
                    self->z_redirect_server_->deleteLater();
                    self->z_redirect_server_ = nullptr;
                }
            });
    connect(z_redirect_server_, &trading::auth::RedirectServer::timeout, this, [self]() {
        if (!self) return;
        self->z_status_->setText("Browser login timed out - try again or paste manually");
        self->z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
        if (self->z_redirect_server_) {
            self->z_redirect_server_->deleteLater();
            self->z_redirect_server_ = nullptr;
        }
    });

    z_status_->setText(QString("Waiting for browser login on port %1 (120s)...")
                           .arg(z_redirect_server_->port()));
    z_status_->setStyleSheet(QString("color:%1;").arg(colors::AMBER()));
    QDesktopServices::openUrl(QUrl(trading::ZerodhaBroker::kite_login_url(api_key)));
}

void AccountManagementDialog::on_connect_zerodha_manual_paste() {
    if (selected_account_id_.isEmpty())
        return;
    const QString api_key = z_api_key_->text().trimmed();
    const QString api_secret = z_api_secret_->text();
    QString token = z_manual_token_->text().trimmed();
    if (api_key.isEmpty() || api_secret.isEmpty() || token.isEmpty()) {
        z_status_->setText("Enter API Key, API Secret, and paste request_token");
        z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
        return;
    }
    // Accept either a raw token or the full redirect URL (e.g. "https://127.0.0.1/?request_token=...&status=success").
    LOG_INFO("Zerodha", QString("manual paste input (%1 chars): %2").arg(token.length()).arg(token));
    if (token.contains(QStringLiteral("request_token"), Qt::CaseInsensitive)) {
        const QUrl url = token.startsWith(QStringLiteral("http"), Qt::CaseInsensitive)
                             ? QUrl(token)
                             : QUrl(QStringLiteral("http://x/?") + token);
        const QString extracted = QUrlQuery(url).queryItemValue(QStringLiteral("request_token"));
        if (!extracted.isEmpty())
            token = extracted;
    }
    LOG_INFO("Zerodha", QString("manual paste extracted token: %1").arg(token));
    if (token.contains(QLatin1Char('/')) || token.contains(QLatin1Char('?')) || token.contains(QLatin1Char('='))) {
        z_status_->setText("Could not find request_token in pasted text");
        z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
        return;
    }
    persist_zerodha_creds_before_auth();
    z_status_->setText("Exchanging token...");
    z_status_->setStyleSheet(QString("color:%1;").arg(colors::AMBER()));
    exchange_and_store_token_async(api_key, api_secret, token);
}

void AccountManagementDialog::exchange_and_store_token_async(const QString& api_key,
                                                             const QString& api_secret,
                                                             const QString& request_token) {
    z_connect_btn_->setEnabled(false);
    z_browser_btn_->setEnabled(false);
    z_manual_connect_btn_->setEnabled(false);

    const QString acct = selected_account_id_;
    QPointer<AccountManagementDialog> self = this;

    QtConcurrent::run([self, acct, api_key, api_secret, request_token]() {
        trading::ZerodhaBroker broker;
        auto result = broker.exchange_token(api_key, api_secret, request_token);

        QMetaObject::invokeMethod(qApp, [self, acct, result]() {
            if (!self) return;
            auto& am = AccountManager::instance();

            if (result.success) {
                auto creds = am.load_credentials(acct);
                creds.access_token = result.access_token;
                if (!result.user_id.isEmpty())
                    creds.user_id = result.user_id;
                am.save_credentials(acct, creds);
                am.set_connection_state(acct, ConnectionState::Connected, {});
                self->z_status_->setText(QString("Connected as %1").arg(result.user_id));
                self->z_status_->setStyleSheet(QString("color:%1;").arg(colors::POSITIVE()));
                emit self->credentials_saved(acct);
            } else {
                am.set_connection_state(acct, ConnectionState::Error, result.error);
                self->z_status_->setText(result.error);
                self->z_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
            }
            self->z_connect_btn_->setEnabled(true);
            self->z_browser_btn_->setEnabled(true);
            self->z_manual_connect_btn_->setEnabled(true);
            self->refresh_account_list();
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::screens::equity
