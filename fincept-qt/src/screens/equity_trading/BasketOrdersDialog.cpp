#include "screens/equity_trading/BasketOrdersDialog.h"

#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens::equity {

using namespace fincept::ui;
using trading::OrderSide;
using trading::OrderType;
using trading::ProductType;
using trading::UnifiedOrder;

namespace {
enum LegCol { LSymbol = 0, LExchange, LSide, LQty, LType, LPrice, LProduct, LRemove, LColCount };
} // namespace

BasketOrdersDialog::BasketOrdersDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Order Baskets"));
    setMinimumSize(860, 540);
    setStyleSheet(QString("QDialog{background:%1;}").arg(colors::BG_BASE()));

    auto* root = new QHBoxLayout(this);
    auto* split = new QSplitter(Qt::Horizontal);
    root->addWidget(split);

    // ── Left: saved baskets ──────────────────────────────────────────────────
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(8, 8, 4, 8);
    auto* title = new QLabel(tr("BASKETS"));
    title->setStyleSheet("color:#808080;font-size:10px;font-weight:700;");
    lv->addWidget(title);
    basket_list_ = new QListWidget;
    basket_list_->setStyleSheet("QListWidget{background:#080808;border:1px solid #1a1a1a;font-size:12px;}"
                                "QListWidget::item{padding:6px 8px;}"
                                "QListWidget::item:selected{background:rgba(217,119,6,0.15);color:#d97706;}");
    connect(basket_list_, &QListWidget::currentRowChanged, this, &BasketOrdersDialog::on_basket_selected);
    lv->addWidget(basket_list_, 1);
    auto* lbtns = new QHBoxLayout;
    new_btn_ = new QPushButton(tr("+ NEW"));
    delete_btn_ = new QPushButton(tr("DELETE"));
    for (auto* b : {new_btn_, delete_btn_}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton{background:#111;border:1px solid #2a2a2a;color:#d97706;"
                         "padding:5px 10px;font-size:11px;font-weight:600;}"
                         "QPushButton:hover{border-color:#d97706;}");
        lbtns->addWidget(b);
    }
    connect(new_btn_, &QPushButton::clicked, this, &BasketOrdersDialog::on_new_basket);
    connect(delete_btn_, &QPushButton::clicked, this, &BasketOrdersDialog::on_delete_basket);
    lv->addLayout(lbtns);
    split->addWidget(left);

    // ── Right: editor + execute ──────────────────────────────────────────────
    auto* right = new QWidget;
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(4, 8, 8, 8);

    auto* name_row = new QHBoxLayout;
    name_row->addWidget(new QLabel(tr("Name:")));
    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText(tr("e.g. Nifty Core 5"));
    name_row->addWidget(name_edit_, 1);
    save_btn_ = new QPushButton(tr("SAVE BASKET"));
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet("QPushButton{background:#111;border:1px solid #d97706;color:#d97706;"
                             "padding:5px 12px;font-size:11px;font-weight:700;}"
                             "QPushButton:hover{background:#d97706;color:#080808;}");
    connect(save_btn_, &QPushButton::clicked, this, &BasketOrdersDialog::on_save_basket);
    name_row->addWidget(save_btn_);
    rv->addLayout(name_row);

    legs_table_ = new QTableWidget(0, LColCount);
    legs_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Exch"), tr("Side"), tr("Qty"), tr("Type"),
                                            tr("Price"), tr("Product"), QString()});
    legs_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    legs_table_->horizontalHeader()->setStretchLastSection(false);
    legs_table_->setColumnWidth(LSymbol, 140);
    legs_table_->setColumnWidth(LRemove, 36);
    legs_table_->verticalHeader()->setVisible(false);
    legs_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    legs_table_->setStyleSheet("QTableWidget{background:#080808;border:1px solid #1a1a1a;font-size:12px;}");
    rv->addWidget(legs_table_, 1);

    // Add-leg row
    auto* add_row = new QHBoxLayout;
    leg_symbol_ = new QLineEdit;
    leg_symbol_->setPlaceholderText(tr("SYMBOL"));
    leg_symbol_->setMaximumWidth(140);
    leg_exchange_ = new QComboBox;
    leg_exchange_->addItems({"NSE", "BSE", "NFO", "BFO", "MCX", "CDS"});
    leg_side_ = new QComboBox;
    leg_side_->addItems({tr("BUY"), tr("SELL")});
    leg_qty_ = new QSpinBox;
    leg_qty_->setRange(1, 1'000'000);
    leg_type_ = new QComboBox;
    leg_type_->addItems({tr("MARKET"), tr("LIMIT")});
    leg_price_ = new QDoubleSpinBox;
    leg_price_->setRange(0.0, 10'000'000.0);
    leg_price_->setDecimals(2);
    leg_price_->setEnabled(false);
    connect(leg_type_, &QComboBox::currentIndexChanged, leg_price_,
            [this](int idx) { leg_price_->setEnabled(idx == 1); });
    leg_product_ = new QComboBox;
    leg_product_->addItems({tr("INTRADAY"), tr("DELIVERY"), tr("MARGIN")});
    auto* add_btn = new QPushButton(tr("+ ADD"));
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setStyleSheet("QPushButton{background:#111;border:1px solid #2a2a2a;color:#d97706;"
                           "padding:4px 10px;font-size:11px;font-weight:600;}"
                           "QPushButton:hover{border-color:#d97706;}");
    connect(add_btn, &QPushButton::clicked, this, &BasketOrdersDialog::on_add_leg);
    connect(leg_symbol_, &QLineEdit::returnPressed, this, &BasketOrdersDialog::on_add_leg);
    for (QWidget* w : std::initializer_list<QWidget*>{leg_symbol_, leg_exchange_, leg_side_, leg_qty_,
                                                      leg_type_, leg_price_, leg_product_, add_btn})
        add_row->addWidget(w);
    rv->addLayout(add_row);

    // Execute: account multi-select + button
    build_accounts_box(rv);

    status_label_ = new QLabel;
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet("color:#808080;font-size:11px;");
    rv->addWidget(status_label_);

    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({220, 640});

    reload_baskets();
}

void BasketOrdersDialog::build_accounts_box(QVBoxLayout* into) {
    auto* cap = new QLabel(tr("EXECUTE ON"));
    cap->setStyleSheet("color:#808080;font-size:10px;font-weight:700;margin-top:6px;");
    into->addWidget(cap);

    auto* row = new QHBoxLayout;
    const auto accounts = trading::AccountManager::instance().active_accounts();
    for (const auto& account : accounts) {
        auto* broker = trading::BrokerRegistry::instance().get(account.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : account.broker_id;
        const QString mode_tag = account.trading_mode == "live" ? tr("LIVE") : tr("PAPER");
        auto* cb = new QCheckBox(QString("%1 [%2·%3]").arg(account.display_name, broker_name, mode_tag));
        cb->setStyleSheet("QCheckBox{color:#9ca3af;font-size:11px;}");
        account_cbs_.append(cb);
        account_ids_.append(account.account_id);
        row->addWidget(cb);
    }
    row->addStretch(1);

    execute_btn_ = new QPushButton(tr("EXECUTE BASKET"));
    execute_btn_->setCursor(Qt::PointingHandCursor);
    execute_btn_->setStyleSheet("QPushButton{background:#052e16;border:1px solid #16a34a;color:#22c55e;"
                                "padding:6px 16px;font-size:12px;font-weight:700;}"
                                "QPushButton:hover{background:#16a34a;color:#080808;}");
    connect(execute_btn_, &QPushButton::clicked, this, &BasketOrdersDialog::on_execute);
    row->addWidget(execute_btn_);
    into->addLayout(row);
}

// ── Basket CRUD ──────────────────────────────────────────────────────────────

void BasketOrdersDialog::reload_baskets(const QString& select_id) {
    baskets_ = OrderBasketRepository::instance().list_all();
    basket_list_->clear();
    int select_row = baskets_.isEmpty() ? -1 : 0;
    for (int i = 0; i < baskets_.size(); ++i) {
        basket_list_->addItem(QString("%1  (%2)").arg(baskets_[i].name).arg(baskets_[i].legs.size()));
        if (!select_id.isEmpty() && baskets_[i].id == select_id)
            select_row = i;
    }
    basket_list_->setCurrentRow(select_row); // triggers on_basket_selected
    if (select_row < 0) {
        current_row_ = -1;
        name_edit_->clear();
        refresh_legs_table();
    }
}

OrderBasket* BasketOrdersDialog::current_basket() {
    return current_row_ >= 0 && current_row_ < baskets_.size() ? &baskets_[current_row_] : nullptr;
}

void BasketOrdersDialog::on_basket_selected(int row) {
    current_row_ = row;
    if (auto* b = current_basket())
        name_edit_->setText(b->name);
    refresh_legs_table();
}

void BasketOrdersDialog::on_new_basket() {
    OrderBasket b;
    b.name = tr("New Basket");
    b = OrderBasketRepository::instance().save(b);
    reload_baskets(b.id);
    name_edit_->setFocus();
    name_edit_->selectAll();
}

void BasketOrdersDialog::on_delete_basket() {
    auto* b = current_basket();
    if (!b)
        return;
    if (QMessageBox::question(this, tr("Delete Basket"), tr("Delete basket '%1'?").arg(b->name)) !=
        QMessageBox::Yes)
        return;
    OrderBasketRepository::instance().remove(b->id);
    reload_baskets();
}

void BasketOrdersDialog::on_save_basket() {
    auto* b = current_basket();
    if (!b)
        return;
    b->name = name_edit_->text().trimmed().isEmpty() ? tr("Unnamed") : name_edit_->text().trimmed();
    *b = OrderBasketRepository::instance().save(*b);
    const QString id = b->id;
    reload_baskets(id);
    status_label_->setText(tr("Basket saved"));
    status_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::POSITIVE()));
}

// ── Legs ─────────────────────────────────────────────────────────────────────

void BasketOrdersDialog::refresh_legs_table() {
    legs_table_->setRowCount(0);
    auto* b = current_basket();
    if (!b)
        return;
    legs_table_->setRowCount(int(b->legs.size()));
    for (int i = 0; i < b->legs.size(); ++i) {
        const UnifiedOrder& leg = b->legs[i];
        auto set = [&](int col, const QString& text) {
            legs_table_->setItem(i, col, new QTableWidgetItem(text));
        };
        set(LSymbol, leg.symbol);
        set(LExchange, leg.exchange);
        set(LSide, leg.side == OrderSide::Buy ? tr("BUY") : tr("SELL"));
        legs_table_->item(i, LSide)->setForeground(leg.side == OrderSide::Buy
                                                       ? QColor(colors::POSITIVE())
                                                       : QColor(colors::NEGATIVE()));
        set(LQty, QString::number(leg.quantity));
        set(LType, leg.order_type == OrderType::Market ? tr("MKT") : tr("LMT"));
        set(LPrice, leg.order_type == OrderType::Market ? QStringLiteral("—")
                                                        : QString::number(leg.price, 'f', 2));
        set(LProduct, leg.product_type == ProductType::Delivery   ? tr("CNC")
                      : leg.product_type == ProductType::Margin   ? tr("NRML")
                                                                  : tr("MIS"));
        auto* rm = new QPushButton(QStringLiteral("✕"));
        rm->setCursor(Qt::PointingHandCursor);
        rm->setStyleSheet("QPushButton{background:transparent;border:none;color:#ef4444;font-size:12px;}");
        connect(rm, &QPushButton::clicked, this, [this, i]() {
            if (auto* bb = current_basket()) {
                if (i < bb->legs.size()) {
                    bb->legs.removeAt(i);
                    OrderBasketRepository::instance().save(*bb);
                    refresh_legs_table();
                }
            }
        });
        legs_table_->setCellWidget(i, LRemove, rm);
    }
}

void BasketOrdersDialog::on_add_leg() {
    auto* b = current_basket();
    if (!b) {
        status_label_->setText(tr("Create or select a basket first"));
        status_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::NEGATIVE()));
        return;
    }
    const QString symbol = leg_symbol_->text().trimmed().toUpper();
    if (symbol.isEmpty())
        return;
    const bool is_limit = leg_type_->currentIndex() == 1;
    if (is_limit && leg_price_->value() <= 0.0) {
        status_label_->setText(tr("Enter a limit price for %1").arg(symbol));
        status_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::NEGATIVE()));
        return;
    }

    UnifiedOrder leg;
    leg.symbol = symbol;
    leg.exchange = leg_exchange_->currentText();
    leg.side = leg_side_->currentIndex() == 0 ? OrderSide::Buy : OrderSide::Sell;
    leg.order_type = is_limit ? OrderType::Limit : OrderType::Market;
    leg.quantity = leg_qty_->value();
    leg.price = is_limit ? leg_price_->value() : 0.0;
    leg.product_type = leg_product_->currentIndex() == 0   ? ProductType::Intraday
                       : leg_product_->currentIndex() == 1 ? ProductType::Delivery
                                                           : ProductType::Margin;
    b->legs.append(leg);
    OrderBasketRepository::instance().save(*b);
    leg_symbol_->clear();
    refresh_legs_table();
    // Keep the list's leg-count label current without rebuilding selection.
    if (auto* item = basket_list_->item(current_row_))
        item->setText(QString("%1  (%2)").arg(b->name).arg(b->legs.size()));
}

// ── Execute ──────────────────────────────────────────────────────────────────

void BasketOrdersDialog::on_execute() {
    auto* b = current_basket();
    if (!b || b->legs.isEmpty()) {
        status_label_->setText(tr("Basket is empty — add legs first"));
        status_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::NEGATIVE()));
        return;
    }
    QStringList selected;
    QStringList labels;
    for (int i = 0; i < account_cbs_.size(); ++i) {
        if (account_cbs_[i]->isChecked()) {
            selected.append(account_ids_[i]);
            labels << account_cbs_[i]->text();
        }
    }
    if (selected.isEmpty()) {
        status_label_->setText(tr("Select at least one account"));
        status_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::NEGATIVE()));
        return;
    }

    const auto ret = QMessageBox::question(
        this, tr("Execute Basket"),
        tr("Execute '%1' (%2 leg(s)) on %3 account(s)?\n\n%4")
            .arg(b->name)
            .arg(b->legs.size())
            .arg(selected.size())
            .arg(labels.join("\n")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    execute_btn_->setEnabled(false);
    status_label_->setText(tr("Executing on %1 account(s)…").arg(selected.size()));
    status_label_->setStyleSheet("color:#d97706;font-size:11px;");

    trading::BasketOrderRequest req;
    req.orders = b->legs;
    req.strategy_name = b->name;

    // place_basket_orders runs on its own background thread per call and
    // invokes the callback on this (caller's) thread. Aggregate across accounts.
    struct Tally {
        int pending = 0;
        int ok = 0;
        int fail = 0;
        QStringList errors;
    };
    auto tally = std::make_shared<Tally>();
    tally->pending = int(selected.size());

    QPointer<BasketOrdersDialog> self(this);
    for (const QString& acct_id : selected) {
        const QString label = trading::AccountManager::instance().get_account(acct_id).display_name;
        trading::UnifiedTrading::instance().place_basket_orders(
            acct_id, req, [self, tally, label](const trading::BasketOrderResult& r) {
                if (!self)
                    return;
                tally->ok += r.successful;
                tally->fail += r.failed;
                for (const auto& or_ : r.results)
                    if (!or_.success)
                        tally->errors << QString("%1/%2: %3").arg(label, or_.symbol, or_.error);
                if (--tally->pending == 0) {
                    QString msg = tr("✓ %1 leg(s) placed").arg(tally->ok);
                    if (tally->fail > 0)
                        msg += tr(" — ✗ %1 failed: %2").arg(tally->fail).arg(tally->errors.join("; "));
                    self->status_label_->setText(msg);
                    self->status_label_->setStyleSheet(QString("color:%1;font-size:11px;")
                                                           .arg(tally->fail == 0 ? colors::POSITIVE()
                                                                                 : colors::NEGATIVE()));
                    self->execute_btn_->setEnabled(true);
                }
            });
    }
}

} // namespace fincept::screens::equity
