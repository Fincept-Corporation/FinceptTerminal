#include "screens/algo_trading/AlgoDeployDialog.h"

#include "services/algo_trading/AlgoTradingTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::trading;
using namespace fincept::services::algo;

AlgoDeployDialog::AlgoDeployDialog(const QString& strategy_id, const QString& strategy_name, QWidget* parent)
    : QDialog(parent), strategy_id_(strategy_id), strategy_name_(strategy_name) {
    setWindowTitle(tr("Deploy Strategy"));
    setMinimumWidth(420);
    setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                          "QLabel { color: %3; font-size: 11px; background: transparent; }"
                          "QLineEdit, QComboBox, QDoubleSpinBox { background: %4; color: %2;"
                          " border: 1px solid %5; padding: 4px 8px; font-size: 11px; }"
                          "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { border-color: %6; }")
                      .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY(),
                           colors::BG_BASE(), colors::BORDER_DIM(), colors::CYAN()));
    build_ui();
}

AlgoDeployDialog::AlgoDeployDialog(const AlgoStrategy& strategy, QWidget* parent)
    : AlgoDeployDialog(strategy.id, strategy.name, parent) {
    result_.strategy_id = strategy.id;
    result_.strategy_name = strategy.name;
    result_.timeframe = strategy.timeframe;
    // Match the dialog's timeframe to the strategy so the deployment evaluates on
    // the same bars the strategy was designed/backtested on (build_ui already ran).
    if (timeframe_combo_ && !strategy.timeframe.isEmpty())
        timeframe_combo_->setCurrentText(strategy.timeframe);
}

void AlgoDeployDialog::set_symbol(const QString& symbol) {
    if (symbol_edit_)
        symbol_edit_->setText(symbol.trimmed().toUpper());
}

void AlgoDeployDialog::set_fno_context(const QString& instrument_type, const QString& underlying,
                                        const QString& expiry_rule) {
    fno_instrument_type_ = instrument_type;
    fno_underlying_      = underlying;
    fno_expiry_rule_     = expiry_rule;

    if (instrument_type == QLatin1String("equity"))
        return;

    // F&O deployment: the symbol field is not a free-text instrument symbol.
    // Show the underlying and expiry rule as read-only context; disable editing.
    if (symbol_edit_) {
        QString label = underlying;
        if (!underlying.isEmpty() && !expiry_rule.isEmpty())
            label = underlying + QStringLiteral(" · ") + expiry_rule;
        else if (underlying.isEmpty())
            label = expiry_rule;
        symbol_edit_->setText(label);
        symbol_edit_->setEnabled(false);
    }
    if (symbol_label_)
        symbol_label_->setText(tr("Underlying:"));

    // Default exchange to NFO for F&O if available.
    if (exchange_combo_) {
        const int nfo_idx = exchange_combo_->findText(QStringLiteral("NFO"));
        if (nfo_idx >= 0)
            exchange_combo_->setCurrentIndex(nfo_idx);
    }

    // Default product type to NRML/Margin if available (case-insensitive search).
    if (product_type_combo_) {
        int nrml_idx = product_type_combo_->findText(QStringLiteral("NRML"),
                                                      Qt::MatchFixedString | Qt::MatchCaseSensitive);
        if (nrml_idx < 0)
            nrml_idx = product_type_combo_->findText(QStringLiteral("Margin"),
                                                      Qt::MatchFixedString | Qt::MatchCaseSensitive);
        if (nrml_idx >= 0)
            product_type_combo_->setCurrentIndex(nrml_idx);
    }
}

void AlgoDeployDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    // strategy_name_ is user data; only the "DEPLOY:" prefix is translatable.
    title_label_ = new QLabel(tr("DEPLOY: %1").arg(strategy_name_), this);
    title_label_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 700; font-family: %2;")
                                    .arg(colors::AMBER(), fonts::DATA_FAMILY()));
    root->addWidget(title_label_);

    auto* id_label = new QLabel(strategy_id_, this);
    id_label->setStyleSheet(QString("color: %1; font-size: 9px; font-family: %2;")
                                .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY()));
    root->addWidget(id_label);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    symbol_edit_ = new QLineEdit(this);
    symbol_edit_->setPlaceholderText(tr("e.g. RELIANCE"));
    symbol_label_ = new QLabel(tr("Symbol:"), this);
    form->addRow(symbol_label_, symbol_edit_);

    mode_combo_ = new QComboBox(this);
    // Visible labels translatable; userData ("paper"/"live") drives logic.
    mode_combo_->addItem(tr("Paper"), "paper");
    mode_combo_->addItem(tr("Live"), "live");
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlgoDeployDialog::on_mode_changed);
    mode_label_ = new QLabel(tr("Mode:"), this);
    form->addRow(mode_label_, mode_combo_);

    // Entry Side — userData ("BUY"/"SELL") is the order side passed to the engine.
    side_combo_ = new QComboBox(this);
    side_combo_->addItem(tr("BUY"), "BUY");
    side_combo_->addItem(tr("SELL"), "SELL");
    side_label_ = new QLabel(tr("Entry Side:"), this);
    form->addRow(side_label_, side_combo_);

    account_label_ = new QLabel(tr("Account:"), this);
    account_combo_ = new QComboBox(this);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlgoDeployDialog::on_account_changed);
    form->addRow(account_label_, account_combo_);

    exchange_label_ = new QLabel(tr("Exchange:"), this);
    exchange_combo_ = new QComboBox(this);
    form->addRow(exchange_label_, exchange_combo_);

    product_type_label_ = new QLabel(tr("Product Type:"), this);
    product_type_combo_ = new QComboBox(this);
    form->addRow(product_type_label_, product_type_combo_);

    timeframe_combo_ = new QComboBox(this);
    for (const auto& tf : algo_timeframes())
        timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentIndex(4);
    timeframe_label_ = new QLabel(tr("Timeframe:"), this);
    form->addRow(timeframe_label_, timeframe_combo_);

    quantity_spin_ = new QDoubleSpinBox(this);
    quantity_spin_->setRange(0.01, 999999);
    quantity_spin_->setValue(1.0);
    quantity_spin_->setDecimals(2);
    quantity_label_ = new QLabel(tr("Quantity:"), this);
    form->addRow(quantity_label_, quantity_spin_);

    max_order_spin_ = new QDoubleSpinBox(this);
    max_order_spin_->setRange(0, 99999999);
    max_order_spin_->setValue(0);
    max_order_spin_->setDecimals(0);
    max_order_spin_->setSpecialValueText(tr("No limit"));
    max_order_label_ = new QLabel(tr("Max Order Value:"), this);
    form->addRow(max_order_label_, max_order_spin_);

    max_loss_spin_ = new QDoubleSpinBox(this);
    max_loss_spin_->setRange(0, 99999999);
    max_loss_spin_->setValue(0);
    max_loss_spin_->setDecimals(0);
    max_loss_spin_->setSpecialValueText(tr("No limit"));
    max_loss_label_ = new QLabel(tr("Max Daily Loss:"), this);
    form->addRow(max_loss_label_, max_loss_spin_);

    root->addLayout(form);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("DEPLOY"));
    buttons_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " font-size: 11px; font-weight: 700; padding: 6px 20px; }"
                "QPushButton:hover { border-color: %4; color: %4; }")
            .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::CYAN()));
    connect(buttons_, &QDialogButtonBox::accepted, this, &AlgoDeployDialog::on_ok);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons_);

    on_mode_changed(0);
    populate_accounts();
}

void AlgoDeployDialog::on_mode_changed(int index) {
    // The broker/account is shown in BOTH modes: it is the real market-data source
    // for the deployment (warm-up history + live quotes). Live additionally routes
    // real orders through it; paper simulates fills against the same data. Hiding it
    // for paper is what made users ask "which broker is it even using?".
    Q_UNUSED(index);
    account_label_->setVisible(true);
    account_combo_->setVisible(true);
    exchange_label_->setVisible(true);
    exchange_combo_->setVisible(true);
    product_type_label_->setVisible(true);
    product_type_combo_->setVisible(true);

    const bool is_live = (mode_combo_->currentData().toString() == "live");
    account_label_->setText(is_live ? tr("Broker (orders + data):") : tr("Broker (data source):"));
}

void AlgoDeployDialog::populate_accounts() {
    account_combo_->clear();
    auto accounts = AccountManager::instance().active_accounts();
    for (const auto& acct : accounts) {
        if (AccountManager::instance().connection_state(acct.account_id) == ConnectionState::Connected) {
            account_combo_->addItem(acct.display_name, acct.account_id);
        }
    }
    if (account_combo_->count() == 0) {
        account_combo_->addItem(tr("No connected accounts"), "");
    }
}

void AlgoDeployDialog::on_account_changed(int index) {
    if (index < 0)
        return;
    populate_broker_fields(account_combo_->itemData(index).toString());
}

void AlgoDeployDialog::populate_broker_fields(const QString& account_id) {
    exchange_combo_->clear();
    product_type_combo_->clear();

    if (account_id.isEmpty())
        return;

    auto* broker = AccountManager::instance().broker_for(account_id);
    if (!broker)
        return;

    auto profile = broker->profile();
    for (const auto& ex : profile.exchanges)
        exchange_combo_->addItem(ex);
    for (const auto& pt : profile.product_types)
        product_type_combo_->addItem(pt.label, static_cast<int>(pt.value));
}

void AlgoDeployDialog::on_ok() {
    // For F&O deployments the symbol field holds a read-only context label, not a
    // tradeable symbol — skip the "symbol required" guard.
    const bool is_fno = (fno_instrument_type_ != QLatin1String("equity"));
    if (!is_fno && symbol_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Symbol is required."));
        return;
    }
    if (is_fno && fno_underlying_.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Deploy"), tr("Select an underlying for the F&O deployment."));
        return;
    }
    if (quantity_spin_->value() <= 0) {
        QMessageBox::warning(this, tr("Validation"), tr("Quantity must be > 0."));
        return;
    }

    QString mode = mode_combo_->currentData().toString();
    if (mode == "live") {
        QString account_id = account_combo_->currentData().toString();
        if (account_id.isEmpty()) {
            QMessageBox::warning(this, tr("Validation"), tr("Select a connected broker account for live mode."));
            return;
        }
        auto creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            QMessageBox::warning(this, tr("Validation"), tr("Account credentials expired. Re-authenticate in Equity Trading."));
            return;
        }
    }

    // Mint a stable id for this deployment — used as the runner key, the
    // algo_deployments primary key, and the WHERE of every status UPDATE.
    if (result_.id.isEmpty())
        result_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result_.strategy_id = strategy_id_;
    result_.strategy_name = strategy_name_;
    result_.strategy_kind = kind_to_string(kind_from_id(strategy_id_));
    result_.symbol = symbol_edit_->text().trimmed().toUpper();
    result_.mode = mode;
    result_.entry_side = side_combo_->currentData().toString();
    result_.timeframe = timeframe_combo_->currentText();
    result_.quantity = quantity_spin_->value();
    result_.max_order_value = max_order_spin_->value();
    result_.max_daily_loss = max_loss_spin_->value();

    // Attach the chosen connected account in BOTH modes — it's the market-data
    // source. Live additionally routes real orders through it (backend=equity_broker);
    // paper simulates fills (backend=paper) but still sources real data from it.
    const QString account_id = account_combo_->currentData().toString();
    if (!account_id.isEmpty()) {
        auto account = AccountManager::instance().get_account(account_id);
        result_.broker_id = account.broker_id;
        result_.broker_account_id = account_id;
        result_.exchange = exchange_combo_->currentText();
        int pt_index = product_type_combo_->currentData().toInt();
        result_.product_type = product_type_str(static_cast<trading::ProductType>(pt_index));
    }
    result_.backend = backend_to_string(mode == "live" ? TradingBackend::EquityBroker
                                                       : TradingBackend::Paper);

    // F&O override: set instrument_type/underlying/resolved_expiry and clear symbol.
    // Concrete legs and expiry are resolved at entry-time by the runner (P3).
    // The equity branch (is_fno == false) is unaffected.
    if (is_fno) {
        result_.instrument_type  = fno_instrument_type_;
        result_.underlying       = fno_underlying_;
        result_.resolved_expiry  = fno_expiry_rule_; // expiry RULE; concrete expiry resolved at entry (P3)
        // Multi-leg F&O has no single symbol; concrete legs are resolved at entry by the runner (P3).
        result_.symbol.clear();
    }

    accept();
}

void AlgoDeployDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AlgoDeployDialog::retranslateUi() {
    setWindowTitle(tr("Deploy Strategy"));
    if (title_label_)        title_label_->setText(tr("DEPLOY: %1").arg(strategy_name_));
    if (symbol_edit_)        symbol_edit_->setPlaceholderText(tr("e.g. RELIANCE"));
    if (symbol_label_)       symbol_label_->setText(tr("Symbol:"));
    if (mode_label_)         mode_label_->setText(tr("Mode:"));
    if (side_label_)         side_label_->setText(tr("Entry Side:"));
    if (account_label_)      account_label_->setText(tr("Account:"));
    if (exchange_label_)     exchange_label_->setText(tr("Exchange:"));
    if (product_type_label_) product_type_label_->setText(tr("Product Type:"));
    if (timeframe_label_)    timeframe_label_->setText(tr("Timeframe:"));
    if (quantity_label_)     quantity_label_->setText(tr("Quantity:"));
    if (max_order_label_)    max_order_label_->setText(tr("Max Order Value:"));
    if (max_loss_label_)     max_loss_label_->setText(tr("Max Daily Loss:"));

    // Combo visible labels (userData keys unchanged).
    if (mode_combo_ && mode_combo_->count() >= 2) {
        mode_combo_->setItemText(0, tr("Paper"));
        mode_combo_->setItemText(1, tr("Live"));
    }
    if (side_combo_ && side_combo_->count() >= 2) {
        side_combo_->setItemText(0, tr("BUY"));
        side_combo_->setItemText(1, tr("SELL"));
    }
    if (max_order_spin_) max_order_spin_->setSpecialValueText(tr("No limit"));
    if (max_loss_spin_)  max_loss_spin_->setSpecialValueText(tr("No limit"));
    if (buttons_)
        buttons_->button(QDialogButtonBox::Ok)->setText(tr("DEPLOY"));
    // account_combo_ "No connected accounts" placeholder is rebuilt by populate_accounts().
}

AlgoDeployment AlgoDeployDialog::result() const {
    return result_;
}

} // namespace fincept::screens
