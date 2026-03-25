// EquityCredentials.cpp — broker credential dialog driven by BrokerProfile
#include "screens/equity_trading/EquityCredentials.h"

#include "trading/BrokerRegistry.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens::equity {

// ── helpers ──────────────────────────────────────────────────────────────────

static QGroupBox* make_section(const QString& title, const QString& color) {
    auto* box = new QGroupBox(title);
    box->setStyleSheet(QString(
        "QGroupBox { border: 1px solid %1; border-radius: 3px; margin-top: 8px;"
        "  color: %1; font-size: 11px; font-weight: 700; padding: 8px 6px 6px 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }").arg(color));
    return box;
}

static QLineEdit* make_field(const QString& placeholder, bool password = false) {
    auto* f = new QLineEdit;
    f->setPlaceholderText(placeholder);
    if (password) f->setEchoMode(QLineEdit::Password);
    return f;
}

static QLabel* make_field_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("fieldLabel");
    return l;
}

// ── constructor ──────────────────────────────────────────────────────────────

EquityCredentials::EquityCredentials(const QString& broker_id,
                                     const trading::BrokerProfile& profile,
                                     QWidget* parent)
    : QDialog(parent), broker_id_(broker_id), profile_(profile) {

    setWindowTitle(QString("Connect — %1").arg(profile.display_name));
    setMinimumWidth(420);
    setStyleSheet(
        "QDialog { background: #0a0a0a; color: #e5e5e5; }"
        "QLabel#fieldLabel { color: #808080; font-size: 11px; font-weight: 700; }"
        "QLabel#titleLabel { color: #d97706; font-size: 14px; font-weight: 700; }"
        "QLabel#infoLabel  { color: #525252; font-size: 10px; }"
        "QLabel#savedLabel { color: #16a34a; font-size: 10px; font-weight: 700; }"
        "QLineEdit { background: #080808; border: 1px solid #222222; color: #e5e5e5;"
        "  padding: 6px; font-size: 12px; border-radius: 2px; }"
        "QLineEdit:focus { border-color: #d97706; }"
        "QComboBox { background: #080808; border: 1px solid #222222; color: #e5e5e5;"
        "  padding: 4px 6px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #111111; color: #e5e5e5;"
        "  selection-background-color: rgba(217,119,6,0.3); }"
        "QPushButton { padding: 8px 16px; font-weight: 700; font-size: 12px; border-radius: 2px; }");

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(16, 16, 16, 16);

    // Title row
    auto* title = new QLabel(QString("%1  |  %2  |  %3")
                                 .arg(profile.display_name, profile.region, profile.currency));
    title->setObjectName("titleLabel");
    root->addWidget(title);

    if (!profile.brokerage_info.isEmpty()) {
        auto* info = new QLabel(QString("Brokerage: %1").arg(profile.brokerage_info));
        info->setObjectName("infoLabel");
        root->addWidget(info);
    }
    root->addSpacing(4);

    // Load existing saved credentials
    auto* broker = trading::BrokerRegistry::instance().get(broker_id_);

    if (profile.has_native_paper) {
        // ── Dual section: LIVE + PAPER side by side ───────────────────────
        setMinimumWidth(560);
        auto* cols = new QHBoxLayout;
        cols->setSpacing(12);

        // --- LIVE section ---
        auto* live_box   = make_section("LIVE ACCOUNT", "#d97706");
        auto* live_lay   = new QVBoxLayout(live_box);
        live_lay->setSpacing(4);

        live_lay->addWidget(make_field_label("API KEY ID"));
        field_live_key_ = make_field("Enter live API Key ID...");
        live_lay->addWidget(field_live_key_);

        live_lay->addWidget(make_field_label("API SECRET KEY"));
        field_live_secret_ = make_field("Enter live Secret Key...", true);
        live_lay->addWidget(field_live_secret_);

        live_status_ = new QLabel;
        live_status_->setObjectName("savedLabel");
        live_lay->addWidget(live_status_);

        auto* live_btn = new QPushButton("CONNECT LIVE");
        live_btn->setStyleSheet(
            "background: rgba(217,119,6,0.15); color: #d97706; border: 1px solid #92400e;");
        live_btn->setCursor(Qt::PointingHandCursor);
        connect(live_btn, &QPushButton::clicked, this, [this]() { on_connect("live"); });
        live_lay->addWidget(live_btn);

        auto* live_clear = new QPushButton("CLEAR");
        live_clear->setStyleSheet(
            "background: rgba(220,38,38,0.1); color: #dc2626; border: 1px solid #7f1d1d;");
        live_clear->setCursor(Qt::PointingHandCursor);
        connect(live_clear, &QPushButton::clicked, this, [this]() { on_clear("live"); });
        live_lay->addWidget(live_clear);

        // --- PAPER section ---
        auto* paper_box   = make_section("PAPER ACCOUNT", "#3b82f6");
        auto* paper_lay   = new QVBoxLayout(paper_box);
        paper_lay->setSpacing(4);

        paper_lay->addWidget(make_field_label("API KEY ID"));
        field_paper_key_ = make_field("Enter paper API Key ID (PK...)...");
        paper_lay->addWidget(field_paper_key_);

        paper_lay->addWidget(make_field_label("API SECRET KEY"));
        field_paper_secret_ = make_field("Enter paper Secret Key...", true);
        paper_lay->addWidget(field_paper_secret_);

        paper_status_ = new QLabel;
        paper_status_->setObjectName("savedLabel");
        paper_lay->addWidget(paper_status_);

        auto* paper_btn = new QPushButton("CONNECT PAPER");
        paper_btn->setStyleSheet(
            "background: rgba(59,130,246,0.15); color: #3b82f6; border: 1px solid #1d4ed8;");
        paper_btn->setCursor(Qt::PointingHandCursor);
        connect(paper_btn, &QPushButton::clicked, this, [this]() { on_connect("paper"); });
        paper_lay->addWidget(paper_btn);

        auto* paper_clear = new QPushButton("CLEAR");
        paper_clear->setStyleSheet(
            "background: rgba(220,38,38,0.1); color: #dc2626; border: 1px solid #7f1d1d;");
        paper_clear->setCursor(Qt::PointingHandCursor);
        connect(paper_clear, &QPushButton::clicked, this, [this]() { on_clear("paper"); });
        paper_lay->addWidget(paper_clear);

        cols->addWidget(live_box);
        cols->addWidget(paper_box);
        root->addLayout(cols);

        // Pre-fill from saved credentials
        if (broker) {
            auto live_creds  = broker->load_credentials_for_env("live");
            auto paper_creds = broker->load_credentials_for_env("paper");
            if (!live_creds.api_key.isEmpty()) {
                field_live_key_->setText(live_creds.api_key);
                field_live_secret_->setText(live_creds.api_secret);
                live_status_->setText(QString("● Saved: %1").arg(
                    live_creds.user_id.isEmpty() ? live_creds.api_key.left(8) + "..." : live_creds.user_id));
            }
            if (!paper_creds.api_key.isEmpty()) {
                field_paper_key_->setText(paper_creds.api_key);
                field_paper_secret_->setText(paper_creds.api_secret);
                paper_status_->setText(QString("● Saved: %1").arg(
                    paper_creds.user_id.isEmpty() ? paper_creds.api_key.left(8) + "..." : paper_creds.user_id));
            }
        }

    } else {
        // ── Single section for regular brokers ───────────────────────────
        for (const auto& field_def : profile.credential_fields) {
            root->addWidget(make_field_label(field_def.label));
            switch (field_def.field) {
                case trading::CredentialField::ClientCode:
                    field_client_code_ = make_field(field_def.placeholder);
                    root->addWidget(field_client_code_);
                    break;
                case trading::CredentialField::ApiKey:
                    field_key_ = make_field(field_def.placeholder);
                    root->addWidget(field_key_);
                    break;
                case trading::CredentialField::ApiSecret:
                    field_secret_ = make_field(field_def.placeholder, field_def.secret);
                    root->addWidget(field_secret_);
                    break;
                case trading::CredentialField::AuthCode:
                    field_auth_ = make_field(field_def.placeholder);
                    root->addWidget(field_auth_);
                    break;
                case trading::CredentialField::Environment:
                    field_env_ = new QComboBox;
                    field_env_->addItem("Paper Trading", "paper");
                    field_env_->addItem("Live Trading",  "live");
                    root->addWidget(field_env_);
                    break;
            }
        }

        // Pre-fill saved creds
        if (broker) {
            auto creds = broker->load_credentials();
            if (!creds.api_key.isEmpty() && field_key_)
                field_key_->setText(creds.api_key);
            if (!creds.api_secret.isEmpty() && field_secret_)
                field_secret_->setText(creds.api_secret);
            if (!creds.additional_data.isEmpty() && field_env_) {
                int idx = field_env_->findData(creds.additional_data);
                if (idx >= 0) field_env_->setCurrentIndex(idx);
            }
            // Pre-fill client_code and totp_secret from additional_data JSON
            if (field_client_code_ && !creds.additional_data.isEmpty()) {
                auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
                if (doc.isObject()) {
                    auto obj = doc.object();
                    if (!obj.value("client_code").toString().isEmpty())
                        field_client_code_->setText(obj.value("client_code").toString());
                    if (field_auth_ && !obj.value("totp_secret").toString().isEmpty())
                        field_auth_->setText(obj.value("totp_secret").toString());
                }
            }
        }

        root->addSpacing(4);
        auto* btn_row  = new QHBoxLayout;
        auto* save_btn = new QPushButton("CONNECT");
        save_btn->setStyleSheet(
            "background: rgba(217,119,6,0.15); color: #d97706; border: 1px solid #92400e;");
        save_btn->setCursor(Qt::PointingHandCursor);
        connect(save_btn, &QPushButton::clicked, this, [this]() { on_connect(""); });

        auto* clear_btn = new QPushButton("CLEAR");
        clear_btn->setStyleSheet(
            "background: rgba(220,38,38,0.1); color: #dc2626; border: 1px solid #7f1d1d;");
        clear_btn->setCursor(Qt::PointingHandCursor);
        connect(clear_btn, &QPushButton::clicked, this, [this]() { on_clear(""); });

        btn_row->addWidget(save_btn);
        btn_row->addWidget(clear_btn);
        root->addLayout(btn_row);
    }

    status_label_ = new QLabel("");
    status_label_->setWordWrap(true);
    root->addWidget(status_label_);
    root->addStretch();
    adjustSize();
}

// ── slots ─────────────────────────────────────────────────────────────────────

void EquityCredentials::on_connect(const QString& env) {
    QString key, secret, auth;

    if (env == "live") {
        key    = field_live_key_    ? field_live_key_->text().trimmed()    : QString();
        secret = field_live_secret_ ? field_live_secret_->text().trimmed() : QString();
        auth   = "live";
    } else if (env == "paper") {
        key    = field_paper_key_    ? field_paper_key_->text().trimmed()    : QString();
        secret = field_paper_secret_ ? field_paper_secret_->text().trimmed() : QString();
        auth   = "paper";
    } else {
        // Regular broker
        key    = field_key_    ? field_key_->text().trimmed()    : QString();
        secret = field_secret_ ? field_secret_->text().trimmed() : QString();
        if (field_env_)       auth = field_env_->currentData().toString();
        else if (field_auth_) auth = field_auth_->text().trimmed();

        // If broker has a separate client code field, pack it into auth as JSON
        if (field_client_code_ && !field_client_code_->text().trimmed().isEmpty()) {
            QJsonObject extra;
            extra["client_code"] = field_client_code_->text().trimmed();
            extra["totp_secret"] = auth;
            auth = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        }
    }

    if (key.isEmpty()) {
        status_label_->setText("API Key is required");
        status_label_->setStyleSheet("color: #dc2626;");
        return;
    }
    if (secret.isEmpty() && (field_secret_ || field_live_secret_ || field_paper_secret_)) {
        status_label_->setText("API Secret is required");
        status_label_->setStyleSheet("color: #dc2626;");
        return;
    }

    emit credentials_saved(broker_id_, key, secret, auth);
    status_label_->setText("Connecting...");
    status_label_->setStyleSheet("color: #ca8a04;");
    // Don't close — let the screen update the status label and close when done
}

void EquityCredentials::on_clear(const QString& env) {
    if (env == "live") {
        if (field_live_key_)    field_live_key_->clear();
        if (field_live_secret_) field_live_secret_->clear();
        if (live_status_)       live_status_->clear();
    } else if (env == "paper") {
        if (field_paper_key_)    field_paper_key_->clear();
        if (field_paper_secret_) field_paper_secret_->clear();
        if (paper_status_)       paper_status_->clear();
    } else {
        if (field_key_)    field_key_->clear();
        if (field_secret_) field_secret_->clear();
        if (field_auth_)   field_auth_->clear();
    }
    status_label_->setText("Cleared");
    status_label_->setStyleSheet("color: #ca8a04;");
}

void EquityCredentials::mark_connected(const QString& env, const QString& account_id) {
    const QString label = QString("● Connected: %1").arg(account_id);
    if (env == "live" && live_status_) {
        live_status_->setText(label);
    } else if (env == "paper" && paper_status_) {
        paper_status_->setText(label);
    }
    status_label_->setText("Connected successfully");
    status_label_->setStyleSheet("color: #16a34a;");
}

void EquityCredentials::mark_error(const QString& error) {
    status_label_->setText(QString("Error: %1").arg(error));
    status_label_->setStyleSheet("color: #dc2626;");
}

} // namespace fincept::screens::equity
