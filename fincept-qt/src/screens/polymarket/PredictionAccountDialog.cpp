#include "screens/polymarket/PredictionAccountDialog.h"

#include "core/logging/Logger.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/kalshi/KalshiCredentials.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

namespace pr = fincept::services::prediction;
namespace ks = fincept::services::prediction::kalshi_ns;

// ── Construction ────────────────────────────────────────────────────────────

PredictionAccountDialog::PredictionAccountDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Prediction Markets — Connect Account"));
    setModal(true);
    resize(640, 520);
    build_ui();
    load_existing();
}

void PredictionAccountDialog::set_active_exchange(const QString& exchange_id) {
    if (!tabs_) return;
    if (exchange_id == QStringLiteral("kalshi")) tabs_->setCurrentIndex(1);
    else tabs_->setCurrentIndex(0);
}

// ── UI ──────────────────────────────────────────────────────────────────────

void PredictionAccountDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    tabs_ = new QTabWidget(this);
    root->addWidget(tabs_);
    build_polymarket_tab();
    build_kalshi_tab();

    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    auto* close_btn = new QPushButton(tr("Close"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(close_btn);
    root->addLayout(bottom);
}

void PredictionAccountDialog::build_polymarket_tab() {
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);

    auto* intro = new QLabel(
        tr("<b>Polymarket (Polygon)</b><br>"
           "Trading requires a Polygon-compatible private key. The key is signed locally "
           "via <code>py_clob_client</code> and never leaves your machine in plaintext — "
           "it is stored encrypted in your OS credential manager.<br><br>"
           "<b>⚠ Security:</b> use a dedicated funding wallet, not your primary wallet."),
        page);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    auto* form = new QFormLayout;

    pm_private_key_ = new QLineEdit(page);
    pm_private_key_->setEchoMode(QLineEdit::Password);
    pm_private_key_->setPlaceholderText(tr("0x… (64 hex chars)"));
    form->addRow(tr("Private Key:"), pm_private_key_);

    pm_funder_ = new QLineEdit(page);
    pm_funder_->setPlaceholderText(tr("0x… (optional — derived via CREATE2 for proxy)"));
    form->addRow(tr("Funder Address:"), pm_funder_);

    pm_signature_type_ = new QComboBox(page);
    pm_signature_type_->addItem(tr("Polymarket Proxy Wallet (default)"), 1);
    pm_signature_type_->addItem(tr("Externally Owned Account (EOA)"), 0);
    pm_signature_type_->addItem(tr("Polymarket Gnosis Safe"), 2);
    form->addRow(tr("Signature Type:"), pm_signature_type_);

    pm_derived_status_ = new QLabel(tr("L2 API credentials: not derived"), page);
    pm_derived_status_->setStyleSheet("color: #9ca3af;");
    form->addRow(QString(), pm_derived_status_);

    vl->addLayout(form);

    pm_status_ = new QLabel(page);
    pm_status_->setWordWrap(true);
    vl->addWidget(pm_status_);

    auto* btns = new QHBoxLayout;
    pm_save_btn_ = new QPushButton(tr("Save"), page);
    pm_test_btn_ = new QPushButton(tr("Test Connection"), page);
    pm_clear_btn_ = new QPushButton(tr("Clear"), page);
    btns->addWidget(pm_save_btn_);
    btns->addWidget(pm_test_btn_);
    btns->addStretch(1);
    btns->addWidget(pm_clear_btn_);
    vl->addLayout(btns);

    connect(pm_save_btn_, &QPushButton::clicked, this, &PredictionAccountDialog::on_save_polymarket);
    connect(pm_clear_btn_, &QPushButton::clicked, this, &PredictionAccountDialog::on_clear_polymarket);
    connect(pm_test_btn_, &QPushButton::clicked, this, [this]() {
        emit test_requested(QStringLiteral("polymarket"));
    });

    vl->addStretch(1);
    tabs_->addTab(page, tr("Polymarket"));
}

void PredictionAccountDialog::build_kalshi_tab() {
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);

    auto* intro = new QLabel(
        tr("<b>Kalshi (CFTC-regulated)</b><br>"
           "Generate an API key + RSA private key in your Kalshi dashboard "
           "(<code>api.elections.kalshi.com</code>). Requests are signed with RSA-PSS "
           "(key stays local, encrypted in your OS credential manager).<br><br>"
           "Use <b>Demo mode</b> to target <code>demo-api.kalshi.co</code> for testing."),
        page);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    auto* form = new QFormLayout;

    ks_api_key_id_ = new QLineEdit(page);
    ks_api_key_id_->setPlaceholderText(tr("00000000-0000-0000-0000-000000000000"));
    form->addRow(tr("API Key ID:"), ks_api_key_id_);

    ks_private_key_pem_ = new QTextEdit(page);
    ks_private_key_pem_->setPlaceholderText(
        tr("-----BEGIN RSA PRIVATE KEY-----\n…paste PEM contents here…\n-----END RSA PRIVATE KEY-----"));
    ks_private_key_pem_->setFont(QFont("Courier New", 9));
    ks_private_key_pem_->setFixedHeight(160);
    form->addRow(tr("Private Key (PEM):"), ks_private_key_pem_);

    ks_load_pem_btn_ = new QPushButton(tr("Load from file…"), page);
    form->addRow(QString(), ks_load_pem_btn_);

    ks_use_demo_ = new QCheckBox(tr("Use demo (paper trading) environment"), page);
    form->addRow(QString(), ks_use_demo_);

    vl->addLayout(form);

    ks_status_ = new QLabel(page);
    ks_status_->setWordWrap(true);
    vl->addWidget(ks_status_);

    auto* btns = new QHBoxLayout;
    ks_save_btn_ = new QPushButton(tr("Save"), page);
    ks_test_btn_ = new QPushButton(tr("Test Connection"), page);
    ks_clear_btn_ = new QPushButton(tr("Clear"), page);
    btns->addWidget(ks_save_btn_);
    btns->addWidget(ks_test_btn_);
    btns->addStretch(1);
    btns->addWidget(ks_clear_btn_);
    vl->addLayout(btns);

    connect(ks_save_btn_, &QPushButton::clicked, this, &PredictionAccountDialog::on_save_kalshi);
    connect(ks_clear_btn_, &QPushButton::clicked, this, &PredictionAccountDialog::on_clear_kalshi);
    connect(ks_load_pem_btn_, &QPushButton::clicked, this, &PredictionAccountDialog::on_load_kalshi_pem);
    connect(ks_test_btn_, &QPushButton::clicked, this, [this]() {
        emit test_requested(QStringLiteral("kalshi"));
    });

    vl->addStretch(1);
    tabs_->addTab(page, tr("Kalshi"));
}

void PredictionAccountDialog::load_existing() {
    if (auto pm = pr::PredictionCredentialStore::load_polymarket()) {
        pm_private_key_->setText(pm->private_key);
        pm_funder_->setText(pm->funder_address);
        const int idx = pm_signature_type_->findData(pm->signature_type);
        if (idx >= 0) pm_signature_type_->setCurrentIndex(idx);
        if (!pm->api_key.isEmpty()) {
            pm_derived_status_->setText(
                tr("L2 API credentials: derived (%1…)").arg(pm->api_key.left(8)));
            pm_derived_status_->setStyleSheet("color: #16a34a;");
        }
        pm_status_->setText(tr("Polymarket credentials loaded from secure store."));
    }
    if (auto ks_creds = pr::PredictionCredentialStore::load_kalshi()) {
        ks_api_key_id_->setText(ks_creds->api_key_id);
        ks_private_key_pem_->setPlainText(ks_creds->private_key_pem);
        ks_use_demo_->setChecked(ks_creds->use_demo);
        ks_status_->setText(tr("Kalshi credentials loaded from secure store."));
    }
}

// ── Polymarket save/clear ───────────────────────────────────────────────────

void PredictionAccountDialog::on_save_polymarket() {
    pr::PolymarketCredentials c;
    c.private_key = pm_private_key_->text().trimmed();
    c.funder_address = pm_funder_->text().trimmed();
    c.signature_type = pm_signature_type_->currentData().toInt();

    if (c.private_key.isEmpty()) {
        pm_status_->setText(tr("<span style='color:#dc2626'>Private key is required.</span>"));
        return;
    }
    if (!c.private_key.startsWith(QStringLiteral("0x"))) {
        c.private_key.prepend(QStringLiteral("0x"));
    }
    if (c.private_key.size() != 66) {
        pm_status_->setText(tr(
            "<span style='color:#dc2626'>Private key should be 0x + 64 hex chars.</span>"));
        return;
    }

    // Preserve already-derived L2 creds if present.
    if (auto existing = pr::PredictionCredentialStore::load_polymarket()) {
        if (existing->private_key == c.private_key) {
            c.api_key = existing->api_key;
            c.api_secret = existing->api_secret;
            c.api_passphrase = existing->api_passphrase;
        }
    }

    if (pr::PredictionCredentialStore::save_polymarket(c)) {
        pm_status_->setText(tr("<span style='color:#16a34a'>Polymarket credentials saved.</span>"));
        emit credentials_saved(QStringLiteral("polymarket"));
    } else {
        pm_status_->setText(tr("<span style='color:#dc2626'>Save failed — see logs.</span>"));
    }
}

void PredictionAccountDialog::on_clear_polymarket() {
    if (QMessageBox::question(this, tr("Clear Polymarket credentials?"),
                              tr("This removes your stored Polymarket private key and API "
                                 "credentials from this machine. You will need to re-enter "
                                 "them to resume trading.")) != QMessageBox::Yes) {
        return;
    }
    pr::PredictionCredentialStore::clear_polymarket();
    pm_private_key_->clear();
    pm_funder_->clear();
    pm_derived_status_->setText(tr("L2 API credentials: not derived"));
    pm_derived_status_->setStyleSheet("color: #9ca3af;");
    pm_status_->setText(tr("Polymarket credentials cleared."));
    emit credentials_saved(QStringLiteral("polymarket"));
}

// ── Kalshi save/clear/load PEM ──────────────────────────────────────────────

void PredictionAccountDialog::on_save_kalshi() {
    ks::KalshiCredentials c;
    c.api_key_id = ks_api_key_id_->text().trimmed();
    c.private_key_pem = ks_private_key_pem_->toPlainText().trimmed();
    c.use_demo = ks_use_demo_->isChecked();

    if (c.api_key_id.isEmpty() || c.private_key_pem.isEmpty()) {
        ks_status_->setText(tr(
            "<span style='color:#dc2626'>Both API Key ID and PEM private key are required.</span>"));
        return;
    }
    if (!c.private_key_pem.contains(QStringLiteral("BEGIN"))) {
        ks_status_->setText(tr(
            "<span style='color:#dc2626'>Private key must be a PEM-encoded RSA key.</span>"));
        return;
    }

    if (pr::PredictionCredentialStore::save_kalshi(c)) {
        ks_status_->setText(tr("<span style='color:#16a34a'>Kalshi credentials saved.</span>"));
        emit credentials_saved(QStringLiteral("kalshi"));
    } else {
        ks_status_->setText(tr("<span style='color:#dc2626'>Save failed — see logs.</span>"));
    }
}

void PredictionAccountDialog::on_clear_kalshi() {
    if (QMessageBox::question(this, tr("Clear Kalshi credentials?"),
                              tr("This removes your stored Kalshi API key and RSA private key "
                                 "from this machine.")) != QMessageBox::Yes) {
        return;
    }
    pr::PredictionCredentialStore::clear_kalshi();
    ks_api_key_id_->clear();
    ks_private_key_pem_->clear();
    ks_use_demo_->setChecked(false);
    ks_status_->setText(tr("Kalshi credentials cleared."));
    emit credentials_saved(QStringLiteral("kalshi"));
}

void PredictionAccountDialog::on_load_kalshi_pem() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Kalshi private key (PEM)"), QString(),
        tr("PEM files (*.pem *.key);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ks_status_->setText(tr("<span style='color:#dc2626'>Could not read %1.</span>").arg(path));
        return;
    }
    const QString content = QString::fromUtf8(f.readAll());
    if (!content.contains(QStringLiteral("BEGIN"))) {
        ks_status_->setText(tr(
            "<span style='color:#dc2626'>%1 does not look like a PEM file.</span>").arg(path));
        return;
    }
    ks_private_key_pem_->setPlainText(content.trimmed());
    ks_status_->setText(tr("Loaded PEM from %1.").arg(QFileInfo(path).fileName()));
}

} // namespace fincept::screens::polymarket
