// CryptoOrderEntry.cpp — BUY/SELL tab form with toggle type buttons
#include "screens/crypto_trading/CryptoOrderEntry.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::crypto {

CryptoOrderEntry::CryptoOrderEntry(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoOrderEntry");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("cryptoOeHeader");
    header->setFixedHeight(30);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    auto* title = new QLabel("ORDER ENTRY");
    title->setObjectName("cryptoOeTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    mode_label_ = new QLabel("PAPER");
    mode_label_->setObjectName("cryptoOeMode");
    mode_label_->setProperty("mode", "paper");
    h_layout->addWidget(mode_label_);

    layout->addWidget(header);

    // Content area
    auto* content = new QWidget(this);
    auto* form = new QVBoxLayout(content);
    form->setContentsMargins(8, 6, 8, 6);
    form->setSpacing(4);

    // BUY / SELL tabs — side by side
    auto* side_row = new QHBoxLayout;
    side_row->setSpacing(0);

    buy_tab_ = new QPushButton("BUY");
    buy_tab_->setObjectName("cryptoBuyTab");
    buy_tab_->setProperty("active", true);
    buy_tab_->setCursor(Qt::PointingHandCursor);
    buy_tab_->setFixedHeight(28);
    connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
    side_row->addWidget(buy_tab_, 1);

    sell_tab_ = new QPushButton("SELL");
    sell_tab_->setObjectName("cryptoSellTab");
    sell_tab_->setProperty("active", false);
    sell_tab_->setCursor(Qt::PointingHandCursor);
    sell_tab_->setFixedHeight(28);
    connect(sell_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(false); });
    side_row->addWidget(sell_tab_, 1);
    form->addLayout(side_row);

    // Order type toggle buttons
    auto* type_row = new QHBoxLayout;
    type_row->setSpacing(2);
    const char* type_labels[] = {"MKT", "LMT", "STP", "S/L"};
    for (int i = 0; i < 4; ++i) {
        type_btns_[i] = new QPushButton(type_labels[i]);
        type_btns_[i]->setObjectName("cryptoOeTypeBtn");
        type_btns_[i]->setFixedHeight(20);
        type_btns_[i]->setCursor(Qt::PointingHandCursor);
        if (i == 0)
            type_btns_[i]->setProperty("active", true);
        connect(type_btns_[i], &QPushButton::clicked, this, [this, i]() { set_order_type(i); });
        type_row->addWidget(type_btns_[i]);
    }
    form->addLayout(type_row);

    form->addSpacing(2);

    // Balance
    balance_label_ = new QLabel("$0.00");
    balance_label_->setObjectName("cryptoOeBalance");
    form->addWidget(balance_label_);

    // Market price
    market_price_label_ = new QLabel("MKT: --");
    market_price_label_->setObjectName("cryptoOeMarketPrice");
    form->addWidget(market_price_label_);

    // Quantity
    auto* qty_lbl = new QLabel("QTY");
    qty_lbl->setObjectName("cryptoOeLabel");
    form->addWidget(qty_lbl);

    qty_edit_ = new QLineEdit;
    qty_edit_->setObjectName("cryptoOeInput");
    qty_edit_->setPlaceholderText("0.00");
    qty_edit_->setFixedHeight(26);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    form->addWidget(qty_edit_);

    // Percentage buttons
    auto* pct_row = new QHBoxLayout;
    pct_row->setSpacing(2);
    for (int pct : {25, 50, 75, 100}) {
        auto* btn = new QPushButton(QString("%1%").arg(pct));
        btn->setObjectName("cryptoOePctBtn");
        btn->setFixedHeight(18);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, pct]() { on_pct_clicked(pct); });
        pct_row->addWidget(btn);
    }
    form->addLayout(pct_row);

    // Price
    auto* price_lbl = new QLabel("PRICE");
    price_lbl->setObjectName("cryptoOeLabel");
    form->addWidget(price_lbl);

    price_edit_ = new QLineEdit;
    price_edit_->setObjectName("cryptoOeInput");
    price_edit_->setPlaceholderText("Limit price");
    price_edit_->setEnabled(false);
    price_edit_->setFixedHeight(26);
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    form->addWidget(price_edit_);

    // Stop price
    auto* stop_lbl = new QLabel("STOP");
    stop_lbl->setObjectName("cryptoOeLabel");
    form->addWidget(stop_lbl);

    stop_price_edit_ = new QLineEdit;
    stop_price_edit_->setObjectName("cryptoOeInput");
    stop_price_edit_->setPlaceholderText("Stop price");
    stop_price_edit_->setEnabled(false);
    stop_price_edit_->setFixedHeight(26);
    form->addWidget(stop_price_edit_);

    // Advanced toggle
    advanced_toggle_ = new QPushButton("+ ADVANCED");
    advanced_toggle_->setObjectName("cryptoAdvToggle");
    advanced_toggle_->setCursor(Qt::PointingHandCursor);
    advanced_toggle_->setFixedHeight(18);
    form->addWidget(advanced_toggle_);

    // Collapsible SL/TP section
    advanced_section_ = new QWidget(this);
    advanced_section_->setVisible(false);
    auto* adv_layout = new QVBoxLayout(advanced_section_);
    adv_layout->setContentsMargins(0, 0, 0, 0);
    adv_layout->setSpacing(4);

    auto* sl_lbl = new QLabel("SL");
    sl_lbl->setObjectName("cryptoOeLabel");
    sl_edit_ = new QLineEdit;
    sl_edit_->setObjectName("cryptoOeInput");
    sl_edit_->setPlaceholderText("Stop Loss");
    sl_edit_->setFixedHeight(26);

    auto* tp_lbl = new QLabel("TP");
    tp_lbl->setObjectName("cryptoOeLabel");
    tp_edit_ = new QLineEdit;
    tp_edit_->setObjectName("cryptoOeInput");
    tp_edit_->setPlaceholderText("Take Profit");
    tp_edit_->setFixedHeight(26);

    adv_layout->addWidget(sl_lbl);
    adv_layout->addWidget(sl_edit_);
    adv_layout->addWidget(tp_lbl);
    adv_layout->addWidget(tp_edit_);
    form->addWidget(advanced_section_);

    connect(advanced_toggle_, &QPushButton::clicked, this, [this]() {
        const bool show = !advanced_section_->isVisible();
        advanced_section_->setVisible(show);
        advanced_toggle_->setText(show ? "- ADVANCED" : "+ ADVANCED");
    });

    // Futures controls (leverage + margin mode) — hidden for spot markets
    futures_section_ = new QWidget(this);
    futures_section_->setVisible(false);
    auto* futures_layout = new QVBoxLayout(futures_section_);
    futures_layout->setContentsMargins(0, 0, 0, 0);
    futures_layout->setSpacing(4);

    auto* lev_row = new QHBoxLayout;
    lev_row->setSpacing(4);
    auto* lev_lbl = new QLabel("LEVERAGE");
    lev_lbl->setObjectName("cryptoOeLabel");
    leverage_spin_ = new QSpinBox;
    leverage_spin_->setObjectName("cryptoOeSpinBox");
    leverage_spin_->setRange(1, 125);
    leverage_spin_->setValue(1);
    leverage_spin_->setSuffix("x");
    leverage_spin_->setFixedHeight(26);
    lev_row->addWidget(lev_lbl);
    lev_row->addStretch();
    lev_row->addWidget(leverage_spin_);
    futures_layout->addLayout(lev_row);

    auto* margin_row = new QHBoxLayout;
    margin_row->setSpacing(4);
    auto* margin_lbl = new QLabel("MARGIN");
    margin_lbl->setObjectName("cryptoOeLabel");
    margin_mode_combo_ = new QComboBox;
    margin_mode_combo_->setObjectName("cryptoOeCombo");
    margin_mode_combo_->addItem("Cross", "cross");
    margin_mode_combo_->addItem("Isolated", "isolated");
    margin_mode_combo_->setFixedHeight(26);
    margin_row->addWidget(margin_lbl);
    margin_row->addStretch();
    margin_row->addWidget(margin_mode_combo_);
    futures_layout->addLayout(margin_row);

    form->addWidget(futures_section_);

    connect(leverage_spin_, &QSpinBox::valueChanged, this, [this](int val) { emit leverage_changed(val); });
    connect(margin_mode_combo_, &QComboBox::currentIndexChanged, this,
            [this](int idx) { emit margin_mode_changed(margin_mode_combo_->itemData(idx).toString()); });

    // Cost preview
    cost_label_ = new QLabel("Est: --");
    cost_label_->setObjectName("cryptoOeCost");
    form->addWidget(cost_label_);

    // Submit button
    submit_btn_ = new QPushButton("BUY BTC/USDT");
    submit_btn_->setObjectName("cryptoBuySubmit");
    submit_btn_->setFixedHeight(34);
    submit_btn_->setCursor(Qt::PointingHandCursor);
    connect(submit_btn_, &QPushButton::clicked, this, &CryptoOrderEntry::on_submit);
    form->addWidget(submit_btn_);

    // Status
    status_label_ = new QLabel("");
    status_label_->setObjectName("cryptoOeStatus");
    status_label_->setWordWrap(true);
    form->addWidget(status_label_);

    form->addStretch();
    layout->addWidget(content, 1);
}

void CryptoOrderEntry::set_buy_side(bool is_buy) {
    if (is_buy == is_buy_side_)
        return;  // no-op — avoids repoling children on redundant clicks
    is_buy_side_ = is_buy;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);

    // Update submit button
    const QString label = QString("%1 %2").arg(is_buy ? "BUY" : "SELL", current_symbol_);
    submit_btn_->setText(label);
    submit_btn_->setObjectName(is_buy ? "cryptoBuySubmit" : "cryptoSellSubmit");

    // Narrow style refresh — only the widgets whose dynamic property or
    // objectName actually changed, not the entire order-entry subtree.
    const auto repolish = [](QWidget* w) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    };
    repolish(buy_tab_);
    repolish(sell_tab_);
    repolish(submit_btn_);
}

void CryptoOrderEntry::set_order_type(int idx) {
    if (idx == active_type_) {
        // Still update enabled state + cost preview for defensive callers
        price_edit_->setEnabled(idx == 1 || idx == 3);
        stop_price_edit_->setEnabled(idx == 2 || idx == 3);
        update_cost_preview();
        return;
    }
    const int prev = active_type_;
    active_type_ = idx;
    for (int i = 0; i < 4; ++i)
        type_btns_[i]->setProperty("active", i == idx);

    // Only repoll the two buttons whose "active" property changed.
    const auto repolish = [](QWidget* w) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    };
    if (prev >= 0 && prev < 4)
        repolish(type_btns_[prev]);
    repolish(type_btns_[idx]);

    price_edit_->setEnabled(idx == 1 || idx == 3);
    stop_price_edit_->setEnabled(idx == 2 || idx == 3);
    update_cost_preview();
}

void CryptoOrderEntry::set_balance(double balance) {
    balance_ = balance;
    balance_label_->setText(QString("$%1").arg(balance, 0, 'f', 2));
    update_cost_preview();
}

void CryptoOrderEntry::set_current_price(double price) {
    current_price_ = price;
    market_price_label_->setText(QString("MKT: $%1").arg(price, 0, 'f', 2));
    update_cost_preview();
}

void CryptoOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? "PAPER" : "LIVE");
    mode_label_->setProperty("mode", is_paper ? "paper" : "live");
    mode_label_->style()->unpolish(mode_label_);
    mode_label_->style()->polish(mode_label_);
}

void CryptoOrderEntry::set_symbol(const QString& symbol) {
    current_symbol_ = symbol;
    submit_btn_->setText(QString("%1 %2").arg(is_buy_side_ ? "BUY" : "SELL", symbol));
}

void CryptoOrderEntry::on_submit() {
    const QString side = is_buy_side_ ? "buy" : "sell";
    static const char* type_map[] = {"market", "limit", "stop", "stop_limit"};
    const QString order_type = type_map[active_type_];

    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0) {
        status_label_->setText("Enter a valid quantity");
        status_label_->setProperty("error", true);
        status_label_->style()->unpolish(status_label_);
        status_label_->style()->polish(status_label_);
        return;
    }

    const double price = price_edit_->text().toDouble();
    const double stop_price = stop_price_edit_->text().toDouble();
    const double sl = sl_edit_->text().toDouble();
    const double tp = tp_edit_->text().toDouble();

    emit order_submitted(side, order_type, qty, price, stop_price, sl, tp);
}

void CryptoOrderEntry::on_pct_clicked(int pct) {
    if (balance_ <= 0 || current_price_ <= 0)
        return;
    const double max_qty = balance_ / current_price_;
    const double qty = max_qty * pct / 100.0;
    qty_edit_->setText(QString::number(qty, 'f', 6));
}

void CryptoOrderEntry::set_futures_mode(bool is_futures) {
    is_futures_ = is_futures;
    futures_section_->setVisible(is_futures);
}

void CryptoOrderEntry::update_cost_preview() {
    const double qty = qty_edit_->text().toDouble();
    double price = current_price_;
    if (active_type_ == 1 || active_type_ == 3) {
        const double limit_p = price_edit_->text().toDouble();
        if (limit_p > 0)
            price = limit_p;
    }
    if (qty > 0 && price > 0)
        cost_label_->setText(QString("Est: $%1").arg(qty * price, 0, 'f', 2));
    else
        cost_label_->setText("Est: --");
}

} // namespace fincept::screens::crypto
