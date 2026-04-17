// EquityOrderEntry.cpp — BUY/SELL form configurable per broker profile
#include "screens/equity_trading/EquityOrderEntry.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/EquityTypes.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QMetaObject>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::screens::equity {

using namespace fincept::ui;

EquityOrderEntry::EquityOrderEntry(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderEntry");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("eqOeHeader");
    header->setFixedHeight(30);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    auto* title = new QLabel("ORDER ENTRY");
    title->setObjectName("eqOeTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    mode_label_ = new QLabel("PAPER");
    mode_label_->setObjectName("eqOeMode");
    mode_label_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
    h_layout->addWidget(mode_label_);

    layout->addWidget(header);

    // Content
    auto* content = new QWidget(this);
    auto* form = new QVBoxLayout(content);
    form->setContentsMargins(8, 6, 8, 6);
    form->setSpacing(4);

    // BUY / SELL tabs
    auto* side_row = new QHBoxLayout;
    side_row->setSpacing(0);

    buy_tab_ = new QPushButton("BUY");
    buy_tab_->setObjectName("eqBuyTab");
    buy_tab_->setProperty("active", true);
    buy_tab_->setCursor(Qt::PointingHandCursor);
    buy_tab_->setFixedHeight(28);
    connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
    side_row->addWidget(buy_tab_, 1);

    sell_tab_ = new QPushButton("SELL");
    sell_tab_->setObjectName("eqSellTab");
    sell_tab_->setProperty("active", false);
    sell_tab_->setCursor(Qt::PointingHandCursor);
    sell_tab_->setFixedHeight(28);
    connect(sell_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(false); });
    side_row->addWidget(sell_tab_, 1);
    form->addLayout(side_row);

    // Order type toggle buttons
    auto* type_row = new QHBoxLayout;
    type_row->setSpacing(2);
    const char* type_labels[] = {"MKT", "LMT", "SL", "SL-L"};
    for (int i = 0; i < 4; ++i) {
        type_btns_[i] = new QPushButton(type_labels[i]);
        type_btns_[i]->setObjectName("eqOeTypeBtn");
        type_btns_[i]->setFixedHeight(20);
        type_btns_[i]->setCursor(Qt::PointingHandCursor);
        if (i == 0)
            type_btns_[i]->setProperty("active", true);
        connect(type_btns_[i], &QPushButton::clicked, this, [this, i]() { set_order_type(i); });
        type_row->addWidget(type_btns_[i]);
    }
    form->addLayout(type_row);

    // Product type
    auto* prod_lbl = new QLabel("PRODUCT");
    prod_lbl->setObjectName("eqOeLabel");
    form->addWidget(prod_lbl);

    product_combo_ = new QComboBox;
    product_combo_->setObjectName("eqOeCombo");
    product_combo_->addItems({"Intraday (MIS)", "Delivery (CNC)", "Margin (NRML)"});
    product_combo_->setFixedHeight(26);
    form->addWidget(product_combo_);

    // Exchange
    auto* exch_lbl = new QLabel("EXCHANGE");
    exch_lbl->setObjectName("eqOeLabel");
    form->addWidget(exch_lbl);

    exchange_combo_ = new QComboBox;
    exchange_combo_->setObjectName("eqOeCombo");
    exchange_combo_->addItems({"NSE", "BSE", "NYSE", "NASDAQ"});
    exchange_combo_->setFixedHeight(26);
    form->addWidget(exchange_combo_);

    // Brokerage info
    brokerage_label_ = new QLabel("");
    brokerage_label_->setObjectName("eqOeBrokerage");
    brokerage_label_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(colors::TEXT_TERTIARY()));
    form->addWidget(brokerage_label_);

    form->addSpacing(2);

    // Balance
    balance_label_ = new QLabel("--");
    balance_label_->setObjectName("eqOeBalance");
    form->addWidget(balance_label_);

    // Market price
    market_price_label_ = new QLabel("MKT: --");
    market_price_label_->setObjectName("eqOeMarketPrice");
    form->addWidget(market_price_label_);

    // Quantity
    auto* qty_lbl = new QLabel("QTY");
    qty_lbl->setObjectName("eqOeLabel");
    form->addWidget(qty_lbl);

    qty_edit_ = new QLineEdit;
    qty_edit_->setObjectName("eqOeInput");
    qty_edit_->setPlaceholderText("0");
    qty_edit_->setFixedHeight(26);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    form->addWidget(qty_edit_);

    // Price
    auto* price_lbl = new QLabel("PRICE");
    price_lbl->setObjectName("eqOeLabel");
    form->addWidget(price_lbl);

    price_edit_ = new QLineEdit;
    price_edit_->setObjectName("eqOeInput");
    price_edit_->setPlaceholderText("Limit price");
    price_edit_->setEnabled(false);
    price_edit_->setFixedHeight(26);
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    form->addWidget(price_edit_);

    // Trigger/Stop price
    auto* stop_lbl = new QLabel("TRIGGER");
    stop_lbl->setObjectName("eqOeLabel");
    form->addWidget(stop_lbl);

    stop_price_edit_ = new QLineEdit;
    stop_price_edit_->setObjectName("eqOeInput");
    stop_price_edit_->setPlaceholderText("Trigger price");
    stop_price_edit_->setEnabled(false);
    stop_price_edit_->setFixedHeight(26);
    form->addWidget(stop_price_edit_);

    // Advanced toggle
    advanced_toggle_ = new QPushButton("+ ADVANCED");
    advanced_toggle_->setObjectName("eqAdvToggle");
    advanced_toggle_->setCursor(Qt::PointingHandCursor);
    advanced_toggle_->setFixedHeight(18);
    form->addWidget(advanced_toggle_);

    // SL/TP section
    advanced_section_ = new QWidget(this);
    advanced_section_->setVisible(false);
    auto* adv_layout = new QVBoxLayout(advanced_section_);
    adv_layout->setContentsMargins(0, 0, 0, 0);
    adv_layout->setSpacing(4);

    auto* sl_lbl = new QLabel("SL");
    sl_lbl->setObjectName("eqOeLabel");
    sl_edit_ = new QLineEdit;
    sl_edit_->setObjectName("eqOeInput");
    sl_edit_->setPlaceholderText("Stop Loss");
    sl_edit_->setFixedHeight(26);

    auto* tp_lbl = new QLabel("TP");
    tp_lbl->setObjectName("eqOeLabel");
    tp_edit_ = new QLineEdit;
    tp_edit_->setObjectName("eqOeInput");
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

    // Cost preview
    cost_label_ = new QLabel("Est: --");
    cost_label_->setObjectName("eqOeCost");
    form->addWidget(cost_label_);

    // Required margin (live broker only)
    margin_label_ = new QLabel("");
    margin_label_->setObjectName("eqOeMargin");
    margin_label_->hide();
    form->addWidget(margin_label_);

    // Submit row: main submit + broadcast
    auto* submit_row = new QHBoxLayout;
    submit_row->setSpacing(4);

    submit_btn_ = new QPushButton("BUY RELIANCE");
    submit_btn_->setObjectName("eqBuySubmit");
    submit_btn_->setFixedHeight(34);
    submit_btn_->setCursor(Qt::PointingHandCursor);
    connect(submit_btn_, &QPushButton::clicked, this, &EquityOrderEntry::on_submit);
    submit_row->addWidget(submit_btn_, 3);

    broadcast_btn_ = new QPushButton("ALL");
    broadcast_btn_->setObjectName("eqBroadcastBtn");
    broadcast_btn_->setFixedHeight(34);
    broadcast_btn_->setCursor(Qt::PointingHandCursor);
    broadcast_btn_->setToolTip("Broadcast this order to multiple accounts");
    broadcast_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; font-weight: 700; font-size: 11px; border-radius: 2px; }"
                "QPushButton:hover { background: %3; }")
            .arg(colors::BG_RAISED(), colors::AMBER(), colors::BORDER_MED()));
    connect(broadcast_btn_, &QPushButton::clicked, this, [this]() {
        const double qty = qty_edit_->text().toDouble();
        if (qty <= 0) {
            status_label_->setText("Enter a valid quantity");
            status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
            return;
        }
        trading::UnifiedOrder order;
        order.symbol = current_symbol_;
        order.exchange = exchange_combo_->currentText();
        order.side = is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell;
        static const trading::OrderType type_map[] = {trading::OrderType::Market, trading::OrderType::Limit,
                                                      trading::OrderType::StopLoss, trading::OrderType::StopLossLimit};
        order.order_type = type_map[active_type_];
        order.quantity = qty;
        order.price = price_edit_->text().toDouble();
        order.stop_price = stop_price_edit_->text().toDouble();
        order.stop_loss = sl_edit_ ? sl_edit_->text().toDouble() : 0.0;
        order.take_profit = tp_edit_ ? tp_edit_->text().toDouble() : 0.0;
        const int prod_idx = product_combo_->currentIndex();
        if (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
            order.product_type = product_types_[prod_idx].value;
        else
            order.product_type = trading::ProductType::Intraday;
        emit broadcast_requested(order);
    });
    submit_row->addWidget(broadcast_btn_, 1);
    form->addLayout(submit_row);

    // Status
    status_label_ = new QLabel("");
    status_label_->setObjectName("eqOeStatus");
    status_label_->setWordWrap(true);
    form->addWidget(status_label_);

    form->addStretch();
    layout->addWidget(content, 1);

    // Debounce timer: fetch margin 600ms after last qty/price change (live broker only)
    margin_timer_ = new QTimer(this);
    margin_timer_->setSingleShot(true);
    margin_timer_->setInterval(600);
    connect(margin_timer_, &QTimer::timeout, this, &EquityOrderEntry::fetch_margin_async);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { margin_timer_->start(); });
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { margin_timer_->start(); });
}

void EquityOrderEntry::configure_for_broker(const trading::BrokerProfile& profile) {
    // Cache product types for order construction
    product_types_ = profile.product_types;
    current_currency_ = profile.currency;

    // Rebuild product combo
    product_combo_->clear();
    for (const auto& pt : profile.product_types)
        product_combo_->addItem(pt.label);

    // Rebuild exchange combo
    exchange_combo_->clear();
    for (const auto& ex : profile.exchanges)
        exchange_combo_->addItem(ex);

    // Set default exchange
    if (!profile.default_exchange.isEmpty()) {
        int idx = exchange_combo_->findText(profile.default_exchange);
        if (idx >= 0)
            exchange_combo_->setCurrentIndex(idx);
        current_exchange_ = profile.default_exchange;
    }

    // Update brokerage label
    brokerage_label_->setText(profile.brokerage_info.isEmpty() ? "" : QString("Fee: %1").arg(profile.brokerage_info));

    // Reset form state
    qty_edit_->clear();
    price_edit_->clear();
    stop_price_edit_->clear();
    if (sl_edit_)
        sl_edit_->clear();
    if (tp_edit_)
        tp_edit_->clear();
    status_label_->clear();
    current_price_ = 0;
    balance_ = 0;
    update_cost_preview();
}

void EquityOrderEntry::set_buy_side(bool is_buy) {
    is_buy_side_ = is_buy;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);
    buy_tab_->style()->unpolish(buy_tab_);
    buy_tab_->style()->polish(buy_tab_);
    sell_tab_->style()->unpolish(sell_tab_);
    sell_tab_->style()->polish(sell_tab_);

    const QString label = QString("%1 %2").arg(is_buy ? "BUY" : "SELL", current_symbol_);
    submit_btn_->setText(label);
    submit_btn_->setObjectName(is_buy ? "eqBuySubmit" : "eqSellSubmit");
    submit_btn_->style()->unpolish(submit_btn_);
    submit_btn_->style()->polish(submit_btn_);
}

void EquityOrderEntry::set_order_type(int idx) {
    active_type_ = idx;
    for (int i = 0; i < 4; ++i) {
        type_btns_[i]->setProperty("active", i == idx);
        type_btns_[i]->style()->unpolish(type_btns_[i]);
        type_btns_[i]->style()->polish(type_btns_[i]);
    }
    price_edit_->setEnabled(idx == 1 || idx == 3);      // Limit, SL-Limit
    stop_price_edit_->setEnabled(idx == 2 || idx == 3); // SL, SL-Limit
    update_cost_preview();
}

void EquityOrderEntry::set_balance(double balance) {
    balance_ = balance;
    balance_label_->setText(QString("%1%2").arg(currency_symbol(current_currency_)).arg(balance, 0, 'f', 2));
    update_cost_preview();
}

void EquityOrderEntry::set_current_price(double price) {
    current_price_ = price;
    market_price_label_->setText(QString("MKT: %1%2").arg(currency_symbol(current_currency_)).arg(price, 0, 'f', 2));
    update_cost_preview();
}

void EquityOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? "PAPER" : "LIVE");
    mode_label_->setStyleSheet(QString("color: %1;").arg(is_paper ? colors::POSITIVE() : colors::NEGATIVE()));
    if (is_paper)
        margin_label_->hide();
}

void EquityOrderEntry::set_symbol(const QString& symbol) {
    current_symbol_ = symbol;
    submit_btn_->setText(QString("%1 %2").arg(is_buy_side_ ? "BUY" : "SELL", symbol));
}

void EquityOrderEntry::set_exchange(const QString& exchange) {
    current_exchange_ = exchange;
    for (int i = 0; i < exchange_combo_->count(); ++i) {
        if (exchange_combo_->itemText(i) == exchange) {
            exchange_combo_->setCurrentIndex(i);
            break;
        }
    }
}

void EquityOrderEntry::on_submit() {
    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0) {
        status_label_->setText("Enter a valid quantity");
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        return;
    }

    trading::UnifiedOrder order;
    order.symbol = current_symbol_;
    order.exchange = exchange_combo_->currentText();
    order.side = is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell;

    static const trading::OrderType type_map[] = {trading::OrderType::Market, trading::OrderType::Limit,
                                                  trading::OrderType::StopLoss, trading::OrderType::StopLossLimit};
    order.order_type = type_map[active_type_];
    order.quantity = qty;
    order.price = price_edit_->text().toDouble();
    order.stop_price = stop_price_edit_->text().toDouble();
    order.stop_loss = sl_edit_ ? sl_edit_->text().toDouble() : 0.0;
    order.take_profit = tp_edit_ ? tp_edit_->text().toDouble() : 0.0;

    // Product type from profile-driven combo
    const int prod_idx = product_combo_->currentIndex();
    if (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
        order.product_type = product_types_[prod_idx].value;
    else
        order.product_type = trading::ProductType::Intraday;

    emit order_submitted(order);
}

void EquityOrderEntry::show_order_status(const QString& msg, bool success) {
    status_label_->setText(msg);
    status_label_->setStyleSheet(QString("color: %1;").arg(success ? colors::POSITIVE() : colors::NEGATIVE()));
    QTimer::singleShot(5000, status_label_, [this]() { status_label_->clear(); });
}

void EquityOrderEntry::update_cost_preview() {
    const double qty = qty_edit_->text().toDouble();
    double price = current_price_;
    if (active_type_ == 1 || active_type_ == 3) {
        const double limit_p = price_edit_->text().toDouble();
        if (limit_p > 0)
            price = limit_p;
    }
    const QString sym = currency_symbol(current_currency_);
    if (qty > 0 && price > 0)
        cost_label_->setText(QString("Est: %1%2").arg(sym).arg(qty * price, 0, 'f', 2));
    else
        cost_label_->setText("Est: --");
}

void EquityOrderEntry::set_broker_id(const QString& broker_id) {
    broker_id_ = broker_id;
}

void EquityOrderEntry::fetch_margin_async() {
    // Only fetch for live mode with a real broker
    if (is_paper_ || broker_id_.isEmpty())
        return;

    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0)
        return;

    // Guard against concurrent fetches
    bool expected = false;
    if (!margin_fetching_.compare_exchange_strong(expected, true))
        return;

    trading::UnifiedOrder order;
    order.symbol = current_symbol_;
    order.exchange = exchange_combo_->currentText();
    order.side = is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.quantity = qty;
    order.price = price_edit_->text().toDouble();
    order.stop_price = stop_price_edit_->text().toDouble();
    static const trading::OrderType type_map[] = {trading::OrderType::Market, trading::OrderType::Limit,
                                                  trading::OrderType::StopLoss, trading::OrderType::StopLossLimit};
    order.order_type = type_map[active_type_];
    const int prod_idx = product_combo_->currentIndex();
    order.product_type = (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
                             ? product_types_[prod_idx].value
                             : trading::ProductType::Intraday;

    const QString bid = broker_id_;
    QPointer<EquityOrderEntry> self = this;

    QtConcurrent::run([self, bid, order]() {
        auto* broker = trading::BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self)
                self->margin_fetching_ = false;
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) {
            if (self)
                self->margin_fetching_ = false;
            return;
        }
        auto result = broker->get_order_margins(creds, order);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->margin_fetching_ = false;
                if (result.success && result.data) {
                    const auto& m = *result.data;
                    const QString sym = currency_symbol(self->current_currency_);
                    self->margin_label_->setText(QString("Margin: %1%2").arg(sym).arg(m.total, 0, 'f', 2));
                    self->margin_label_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(colors::AMBER()));
                    self->margin_label_->show();
                } else {
                    self->margin_label_->hide();
                }
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens::equity
