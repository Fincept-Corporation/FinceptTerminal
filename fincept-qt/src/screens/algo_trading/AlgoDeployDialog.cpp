#include "screens/algo_trading/AlgoDeployDialog.h"

#include "services/algo_trading/AlgoTradingTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::trading;
using namespace fincept::services::algo;

AlgoDeployDialog::AlgoDeployDialog(const QString& strategy_id, const QString& strategy_name, QWidget* parent)
    : QDialog(parent), strategy_id_(strategy_id), strategy_name_(strategy_name) {
    setWindowTitle("Deploy Strategy");
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
}

void AlgoDeployDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel(QString("DEPLOY: %1").arg(strategy_name_), this);
    title->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 700; font-family: %2;")
                             .arg(colors::AMBER(), fonts::DATA_FAMILY()));
    root->addWidget(title);

    auto* id_label = new QLabel(strategy_id_, this);
    id_label->setStyleSheet(QString("color: %1; font-size: 9px; font-family: %2;")
                                .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY()));
    root->addWidget(id_label);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    symbol_edit_ = new QLineEdit(this);
    symbol_edit_->setPlaceholderText("e.g. RELIANCE");
    form->addRow("Symbol:", symbol_edit_);

    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Paper", "paper");
    mode_combo_->addItem("Live", "live");
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlgoDeployDialog::on_mode_changed);
    form->addRow("Mode:", mode_combo_);

    // Entry Side
    side_combo_ = new QComboBox(this);
    side_combo_->addItem("BUY", "BUY");
    side_combo_->addItem("SELL", "SELL");
    form->addRow("Entry Side:", side_combo_);

    account_label_ = new QLabel("Account:", this);
    account_combo_ = new QComboBox(this);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlgoDeployDialog::on_account_changed);
    form->addRow(account_label_, account_combo_);

    exchange_label_ = new QLabel("Exchange:", this);
    exchange_combo_ = new QComboBox(this);
    form->addRow(exchange_label_, exchange_combo_);

    product_type_label_ = new QLabel("Product Type:", this);
    product_type_combo_ = new QComboBox(this);
    form->addRow(product_type_label_, product_type_combo_);

    timeframe_combo_ = new QComboBox(this);
    for (const auto& tf : algo_timeframes())
        timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentIndex(4);
    form->addRow("Timeframe:", timeframe_combo_);

    quantity_spin_ = new QDoubleSpinBox(this);
    quantity_spin_->setRange(0.01, 999999);
    quantity_spin_->setValue(1.0);
    quantity_spin_->setDecimals(2);
    form->addRow("Quantity:", quantity_spin_);

    max_order_spin_ = new QDoubleSpinBox(this);
    max_order_spin_->setRange(0, 99999999);
    max_order_spin_->setValue(0);
    max_order_spin_->setDecimals(0);
    max_order_spin_->setSpecialValueText("No limit");
    form->addRow("Max Order Value:", max_order_spin_);

    max_loss_spin_ = new QDoubleSpinBox(this);
    max_loss_spin_->setRange(0, 99999999);
    max_loss_spin_->setValue(0);
    max_loss_spin_->setDecimals(0);
    max_loss_spin_->setSpecialValueText("No limit");
    form->addRow("Max Daily Loss:", max_loss_spin_);

    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("DEPLOY");
    buttons->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " font-size: 11px; font-weight: 700; padding: 6px 20px; }"
                "QPushButton:hover { border-color: %4; color: %4; }")
            .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::CYAN()));
    connect(buttons, &QDialogButtonBox::accepted, this, &AlgoDeployDialog::on_ok);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    on_mode_changed(0);
    populate_accounts();
}

void AlgoDeployDialog::on_mode_changed(int index) {
    bool is_live = (mode_combo_->itemData(index).toString() == "live");
    account_label_->setVisible(is_live);
    account_combo_->setVisible(is_live);
    exchange_label_->setVisible(is_live);
    exchange_combo_->setVisible(is_live);
    product_type_label_->setVisible(is_live);
    product_type_combo_->setVisible(is_live);
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
        account_combo_->addItem("No connected accounts", "");
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
    if (symbol_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Symbol is required.");
        return;
    }
    if (quantity_spin_->value() <= 0) {
        QMessageBox::warning(this, "Validation", "Quantity must be > 0.");
        return;
    }

    QString mode = mode_combo_->currentData().toString();
    if (mode == "live") {
        QString account_id = account_combo_->currentData().toString();
        if (account_id.isEmpty()) {
            QMessageBox::warning(this, "Validation", "Select a connected broker account for live mode.");
            return;
        }
        auto creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            QMessageBox::warning(this, "Validation", "Account credentials expired. Re-authenticate in Equity Trading.");
            return;
        }
    }

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

    if (mode == "live") {
        auto account_id = account_combo_->currentData().toString();
        auto account = AccountManager::instance().get_account(account_id);
        result_.backend = backend_to_string(TradingBackend::EquityBroker);
        result_.broker_id = account.broker_id;
        result_.broker_account_id = account_id;
        result_.exchange = exchange_combo_->currentText();
        int pt_index = product_type_combo_->currentData().toInt();
        result_.product_type = product_type_str(static_cast<trading::ProductType>(pt_index));
    } else {
        result_.backend = backend_to_string(TradingBackend::Paper);
    }

    accept();
}

AlgoDeployment AlgoDeployDialog::result() const {
    return result_;
}

} // namespace fincept::screens
