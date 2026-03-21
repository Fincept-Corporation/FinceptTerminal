#include "screens/crypto_trading/CryptoOrderEntry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <cmath>

namespace fincept::screens::crypto {

CryptoOrderEntry::CryptoOrderEntry(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto* title = new QLabel("ORDER ENTRY");
    title->setStyleSheet("font-weight: bold; color: #00aaff; font-size: 13px;");
    layout->addWidget(title);

    mode_label_ = new QLabel("[PAPER]");
    mode_label_->setStyleSheet("color: #00ff88; font-weight: bold; font-size: 11px;");
    layout->addWidget(mode_label_);

    balance_label_ = new QLabel("Balance: $0.00");
    balance_label_->setStyleSheet("color: #00ff88; font-size: 12px;");
    layout->addWidget(balance_label_);

    layout->addSpacing(4);

    // Side buttons (Buy / Sell)
    auto* side_row = new QHBoxLayout;
    side_combo_ = new QComboBox;
    side_combo_->addItems({"Buy", "Sell"});
    // Style the combo with green/red indicator
    connect(side_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        submit_btn_->setStyleSheet(
            idx == 0
            ? "QPushButton { background: #00aa55; color: white; padding: 8px; border-radius: 4px; font-weight: bold; font-size: 14px; } QPushButton:hover { background: #00cc66; }"
            : "QPushButton { background: #cc2222; color: white; padding: 8px; border-radius: 4px; font-weight: bold; font-size: 14px; } QPushButton:hover { background: #ee3333; }"
        );
        submit_btn_->setText(idx == 0 ? "BUY" : "SELL");
    });
    side_row->addWidget(new QLabel("Side:"));
    side_row->addWidget(side_combo_, 1);
    layout->addLayout(side_row);

    // Order type
    type_combo_ = new QComboBox;
    type_combo_->addItems({"Market", "Limit", "Stop", "Stop Limit"});
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        price_edit_->setEnabled(idx == 1 || idx == 3);
        stop_price_edit_->setEnabled(idx == 2 || idx == 3);
        sl_edit_->setEnabled(idx != 0);
        tp_edit_->setEnabled(idx != 0);
        update_cost_preview();
    });
    layout->addWidget(new QLabel("Type:"));
    layout->addWidget(type_combo_);

    // Quantity
    qty_edit_ = new QLineEdit;
    qty_edit_->setPlaceholderText("0.00");
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    layout->addWidget(new QLabel("Quantity:"));
    layout->addWidget(qty_edit_);

    // Percentage buttons (25%, 50%, 75%, 100%)
    auto* pct_row = new QHBoxLayout;
    pct_row->setSpacing(2);
    for (int pct : {25, 50, 75, 100}) {
        auto* btn = new QPushButton(QString("%1%").arg(pct));
        btn->setFixedHeight(22);
        btn->setStyleSheet("QPushButton { background: #1a1a2e; color: #888; border: 1px solid #333; "
                            "border-radius: 2px; font-size: 11px; padding: 0 4px; }"
                            "QPushButton:hover { background: #222244; color: #ccc; }");
        connect(btn, &QPushButton::clicked, this, [this, pct]() { on_pct_clicked(pct); });
        pct_row->addWidget(btn);
    }
    layout->addLayout(pct_row);

    // Price
    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText("Limit price");
    price_edit_->setEnabled(false);
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    layout->addWidget(new QLabel("Price:"));
    layout->addWidget(price_edit_);

    // Stop price
    stop_price_edit_ = new QLineEdit;
    stop_price_edit_->setPlaceholderText("Stop price");
    stop_price_edit_->setEnabled(false);
    layout->addWidget(new QLabel("Stop:"));
    layout->addWidget(stop_price_edit_);

    // SL / TP
    auto* sltp_grid = new QGridLayout;
    sl_edit_ = new QLineEdit;
    sl_edit_->setPlaceholderText("Stop Loss");
    sl_edit_->setEnabled(false);
    tp_edit_ = new QLineEdit;
    tp_edit_->setPlaceholderText("Take Profit");
    tp_edit_->setEnabled(false);
    sltp_grid->addWidget(new QLabel("SL:"), 0, 0);
    sltp_grid->addWidget(sl_edit_, 0, 1);
    sltp_grid->addWidget(new QLabel("TP:"), 1, 0);
    sltp_grid->addWidget(tp_edit_, 1, 1);
    layout->addLayout(sltp_grid);

    // Cost preview
    cost_label_ = new QLabel("Est. cost: --");
    cost_label_->setStyleSheet("color: #666; font-size: 11px;");
    layout->addWidget(cost_label_);

    // Submit
    submit_btn_ = new QPushButton("BUY");
    submit_btn_->setStyleSheet(
        "QPushButton { background: #00aa55; color: white; padding: 8px; border-radius: 4px; "
        "font-weight: bold; font-size: 14px; } QPushButton:hover { background: #00cc66; }");
    connect(submit_btn_, &QPushButton::clicked, this, &CryptoOrderEntry::on_submit);
    layout->addWidget(submit_btn_);

    // Status
    status_label_ = new QLabel("");
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet("font-size: 11px;");
    layout->addWidget(status_label_);

    layout->addStretch();

    // Style all inputs
    QString input_style = "QLineEdit { background: #1a1a2e; border: 1px solid #333; border-radius: 3px; "
                           "color: #ccc; padding: 4px 6px; }";
    qty_edit_->setStyleSheet(input_style);
    price_edit_->setStyleSheet(input_style);
    stop_price_edit_->setStyleSheet(input_style);
    sl_edit_->setStyleSheet(input_style);
    tp_edit_->setStyleSheet(input_style);
}

void CryptoOrderEntry::set_balance(double balance) {
    balance_ = balance;
    balance_label_->setText(QString("Balance: $%1").arg(balance, 0, 'f', 2));
    update_cost_preview();
}

void CryptoOrderEntry::set_current_price(double price) {
    current_price_ = price;
    update_cost_preview();
}

void CryptoOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? "[PAPER]" : "[LIVE]");
    mode_label_->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 11px;")
            .arg(is_paper ? "#00ff88" : "#ff4444"));
}

void CryptoOrderEntry::on_submit() {
    QString side = side_combo_->currentIndex() == 0 ? "buy" : "sell";

    static const char* type_map[] = {"market", "limit", "stop", "stop_limit"};
    QString order_type = type_map[type_combo_->currentIndex()];

    double qty = qty_edit_->text().toDouble();
    if (qty <= 0) {
        status_label_->setText("Enter a valid quantity");
        status_label_->setStyleSheet("color: #ff4444; font-size: 11px;");
        return;
    }

    double price = price_edit_->text().toDouble();
    double stop_price = stop_price_edit_->text().toDouble();
    double sl = sl_edit_->text().toDouble();
    double tp = tp_edit_->text().toDouble();

    emit order_submitted(side, order_type, qty, price, stop_price, sl, tp);
}

void CryptoOrderEntry::on_pct_clicked(int pct) {
    if (balance_ <= 0 || current_price_ <= 0) return;
    double max_qty = balance_ / current_price_;
    double qty = max_qty * pct / 100.0;
    qty_edit_->setText(QString::number(qty, 'f', 6));
}

void CryptoOrderEntry::update_cost_preview() {
    double qty = qty_edit_->text().toDouble();
    double price = current_price_;
    if (type_combo_->currentIndex() == 1 || type_combo_->currentIndex() == 3) {
        double limit_p = price_edit_->text().toDouble();
        if (limit_p > 0) price = limit_p;
    }
    if (qty > 0 && price > 0) {
        cost_label_->setText(QString("Est. cost: $%1").arg(qty * price, 0, 'f', 2));
    } else {
        cost_label_->setText("Est. cost: --");
    }
}

} // namespace fincept::screens::crypto
