// CredentialsSection.cpp — API credential entry cards backed by SecureStorage.

#include "screens/settings/CredentialsSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

using CredDef = QPair<QString, QString>; // {env_key, display_name}
const QList<CredDef> CRED_KEYS = {
    {"ALPHA_VANTAGE_API_KEY", "Alpha Vantage"},
    {"POLYGON_API_KEY",       "Polygon.io"},
    {"DATABENTO_API_KEY",     "Databento"},
    {"FRED_API_KEY",          "FRED (Federal Reserve)"},
    {"NEWSAPI_KEY",           "NewsAPI"},
    {"BINANCE_API_KEY",       "Binance API Key"},
    {"BINANCE_SECRET_KEY",    "Binance Secret Key"},
    {"KRAKEN_API_KEY",        "Kraken API Key"},
    {"KRAKEN_SECRET_KEY",     "Kraken Secret Key"},
    {"IEX_CLOUD_TOKEN",       "IEX Cloud Token"},
    {"FINNHUB_API_KEY",       "Finnhub API Key"},
    {"TIINGO_API_KEY",        "Tiingo API Key"},
    {"QUANDL_API_KEY",        "Quandl"},
    {"POLYMARKET_API_KEY",    "Polymarket API Key"},
    {"POLYMARKET_SECRET",     "Polymarket Secret"},
    {"POLYMARKET_PASSPHRASE", "Polymarket Passphrase"},
    {"POLYMARKET_WALLET",     "Polymarket Wallet Address"},
};

} // namespace

CredentialsSection::CredentialsSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void CredentialsSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void CredentialsSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    title_ = new QLabel(tr("API CREDENTIALS"));
    title_->setStyleSheet(section_title_ss());
    vl->addWidget(title_);
    vl->addSpacing(4);

    info_ = new QLabel(
        tr("Store API keys securely in the OS keychain. Keys are never written to disk in plain text."));
    info_->setWordWrap(true);
    info_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info_);
    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(16);

    for (const auto& def : CRED_KEYS) {
        const QString& key  = def.first;
        const QString& name = def.second;

        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:4px;}")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(34);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(
            QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        hhl->addWidget(name_lbl);
        hhl->addStretch();

        auto* status_lbl = new QLabel(tr("Not set"));
        status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        hhl->addWidget(status_lbl);
        cred_status_[key] = status_lbl;

        cvl->addWidget(hdr);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* bhl = new QHBoxLayout(body);
        bhl->setContentsMargins(12, 10, 12, 10);
        bhl->setSpacing(8);

        auto* field = new QLineEdit;
        field->setEchoMode(QLineEdit::Password);
        field->setPlaceholderText(tr("Not configured"));
        field->setStyleSheet(input_ss());
        cred_fields_[key] = field;
        bhl->addWidget(field, 1);

        auto* save_btn = new QPushButton(tr("Save"));
        save_btn->setFixedHeight(30);
        save_btn->setFixedWidth(70);
        save_btn->setStyleSheet(btn_primary_ss());
        bhl->addWidget(save_btn);
        cred_save_btns_[key] = save_btn;

        connect(save_btn, &QPushButton::clicked, this, [key, field, status_lbl]() {
            QString val = field->text().trimmed();
            if (val.isEmpty()) {
                SecureStorage::instance().remove(key);
                field->setPlaceholderText(tr("Not configured"));
                status_lbl->setText(tr("Cleared"));
                status_lbl->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
                LOG_INFO("Credentials", "Cleared key: " + key);
            } else {
                auto r = SecureStorage::instance().store(key, val);
                if (r.is_ok()) {
                    field->clear();
                    field->setPlaceholderText(tr("•••••••• (saved)"));
                    status_lbl->setText(tr("Saved ✓"));
                    status_lbl->setStyleSheet(
                        QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                    LOG_INFO("Credentials", "Stored key: " + key);
                } else {
                    status_lbl->setText(tr("Save failed"));
                    status_lbl->setStyleSheet(
                        QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("Credentials", "Failed to store " + key);
                }
            }
        });

        cvl->addWidget(body);
        vl->addWidget(card);
        vl->addSpacing(8);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

void CredentialsSection::reload() {
    for (auto it = cred_fields_.constBegin(); it != cred_fields_.constEnd(); ++it) {
        const QString& key = it.key();
        auto* field  = it.value();
        auto* status = cred_status_.value(key, nullptr);
        if (!field || !status) continue;

        auto r = SecureStorage::instance().retrieve(key);
        if (r.is_ok() && !r.value().isEmpty()) {
            field->clear();
            field->setPlaceholderText(tr("•••••••• (saved)"));
            status->setText(tr("Saved ✓"));
            status->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
        } else {
            field->clear();
            field->setPlaceholderText(tr("Not configured"));
            status->setText(tr("Not set"));
            status->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        }
    }
}

void CredentialsSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CredentialsSection::retranslateUi() {
    if (title_) title_->setText(tr("API CREDENTIALS"));
    if (info_)
        info_->setText(
            tr("Store API keys securely in the OS keychain. Keys are never written to disk in plain text."));

    // Per-credential Save buttons. (Credential names are product/brand names —
    // not translated. Provider name labels stay as-is.)
    for (auto it = cred_save_btns_.constBegin(); it != cred_save_btns_.constEnd(); ++it) {
        if (it.value()) it.value()->setText(tr("Save"));
    }

    // reload() re-applies state-dependent placeholders ("Not configured" /
    // "•••••••• (saved)") and status labels ("Not set" / "Saved ✓") in the
    // current language.
    reload();
}

} // namespace fincept::screens
