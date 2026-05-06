#include "screens/fno/OrderConfirmDialog.h"

#include "core/logging/Logger.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QMetaObject>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::Strategy;
using fincept::services::options::StrategyLeg;
using fincept::trading::BasketMargin;
using fincept::trading::BrokerCredentials;
using fincept::trading::BrokerRegistry;
using fincept::trading::IBroker;
using fincept::trading::InstrumentType;
using fincept::trading::OrderSide;
using fincept::trading::OrderType;
using fincept::trading::ProductType;
using fincept::trading::UnifiedOrder;
using namespace fincept::ui;

namespace {

constexpr int kMarginFetchTimeoutMs = 5000;

QString fmt_currency(double v) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("∞") : QStringLiteral("−∞");
    return QLocale(QLocale::English).toString(v, 'f', 0);
}

UnifiedOrder leg_to_unified(const StrategyLeg& leg) {
    UnifiedOrder o;
    o.symbol = leg.symbol;
    o.exchange = QStringLiteral("NFO");
    o.side = leg.lots >= 0 ? OrderSide::Buy : OrderSide::Sell;
    o.order_type = OrderType::Limit;
    o.quantity = std::abs(double(leg.lots) * double(leg.lot_size));
    o.price = leg.entry_price;
    o.product_type = ProductType::Margin;  // NRML — F&O positional
    o.validity = QStringLiteral("DAY");
    o.instrument_token = QString::number(leg.instrument_token);
    return o;
}

}  // namespace

OrderConfirmDialog::OrderConfirmDialog(const Strategy& strategy, const OptionChain& chain,
                                       double premium, double max_profit, double max_loss,
                                       QWidget* parent)
    : QDialog(parent), strategy_(strategy), chain_(chain) {
    setWindowTitle(QStringLiteral("Confirm Paper Orders"));
    setModal(true);
    resize(680, 520);
    setStyleSheet(QString("QDialog { background:%1; }"
                          "#confirmTitle { color:%2; font-size:13px; font-weight:700; background:transparent; }"
                          "#confirmSub { color:%3; font-size:10px; background:transparent; }"
                          "#confirmKey { color:%3; font-size:9px; font-weight:700; letter-spacing:0.4px; "
                          "               background:transparent; }"
                          "#confirmValue { color:%2; font-size:13px; font-weight:700; background:transparent; }"
                          "#confirmValuePos { color:%4; font-size:13px; font-weight:700; background:transparent; }"
                          "#confirmValueNeg { color:%5; font-size:13px; font-weight:700; background:transparent; }"
                          "QPushButton#confirmPlace { background:%4; color:%1; border:none; padding:7px 18px; "
                          "                          font-size:11px; font-weight:700; letter-spacing:0.4px; }"
                          "QPushButton#confirmPlace:hover { background:%2; }"
                          "QPushButton#confirmCancel { background:%6; color:%2; border:1px solid %7; "
                          "                            padding:7px 14px; font-size:11px; font-weight:700; }"
                          "QPushButton#confirmCancel:hover { background:%8; }"
                          "QTableWidget { background:%1; color:%2; border:1px solid %7; gridline-color:%7; }"
                          "QHeaderView::section { background:%6; color:%3; border:none; "
                          "                       border-bottom:1px solid %7; padding:5px 8px; "
                          "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }")
                      .arg(colors::BG_BASE(),         // %1
                           colors::TEXT_PRIMARY(),    // %2
                           colors::TEXT_SECONDARY(),  // %3
                           colors::POSITIVE(),        // %4
                           colors::NEGATIVE(),        // %5
                           colors::BG_RAISED(),       // %6
                           colors::BORDER_DIM(),      // %7
                           colors::BG_HOVER()));      // %8

    setup_ui(premium, max_profit, max_loss);
    populate_legs();
    start_margin_fetch();
}

void OrderConfirmDialog::setup_ui(double premium, double max_profit, double max_loss) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // ── Title ─────────────────────────────────────────────────────────────
    auto* title = new QLabel(strategy_.name + "  —  paper trade preview", this);
    title->setObjectName("confirmTitle");
    auto* sub = new QLabel(
        QString("%1   ·   Expiry %2   ·   Spot %3")
            .arg(strategy_.underlying)
            .arg(strategy_.expiry)
            .arg(chain_.spot, 0, 'f', 2),
        this);
    sub->setObjectName("confirmSub");
    root->addWidget(title);
    root->addWidget(sub);

    // ── Leg table ─────────────────────────────────────────────────────────
    legs_table_ = new QTableWidget(this);
    legs_table_->setColumnCount(5);
    legs_table_->setHorizontalHeaderLabels({"Symbol", "B/S", "Qty", "Type", "Entry"});
    legs_table_->verticalHeader()->setVisible(false);
    legs_table_->setSelectionMode(QAbstractItemView::NoSelection);
    legs_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    legs_table_->setShowGrid(false);
    legs_table_->setFocusPolicy(Qt::NoFocus);
    legs_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    legs_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    legs_table_->verticalHeader()->setDefaultSectionSize(24);
    root->addWidget(legs_table_, 1);

    // ── Summary block ─────────────────────────────────────────────────────
    auto add_kv = [this](QHBoxLayout* lay, QLabel*& v_out, const QString& key) {
        auto* wrap = new QWidget(this);
        auto* w = new QVBoxLayout(wrap);
        w->setContentsMargins(0, 0, 0, 0);
        w->setSpacing(0);
        auto* k = new QLabel(key.toUpper(), wrap);
        k->setObjectName("confirmKey");
        auto* v = new QLabel("—", wrap);
        v->setObjectName("confirmValue");
        w->addWidget(k);
        w->addWidget(v);
        lay->addWidget(wrap);
        v_out = v;
    };

    auto* summary = new QHBoxLayout();
    summary->setSpacing(28);
    add_kv(summary, lbl_premium_, "Net Premium");
    add_kv(summary, lbl_max_pnl_, "Max Profit / Loss");
    add_kv(summary, lbl_margin_, "Basket Margin");
    summary->addStretch(1);
    root->addLayout(summary);

    lbl_premium_->setText(
        QString("%1  %2").arg(fmt_currency(std::abs(premium)),
                              premium > 0 ? "Dr" : premium < 0 ? "Cr" : ""));
    lbl_premium_->setObjectName(premium > 0 ? "confirmValueNeg"
                                : premium < 0 ? "confirmValuePos" : "confirmValue");
    lbl_max_pnl_->setText(QString("%1  /  %2").arg(fmt_currency(max_profit), fmt_currency(max_loss)));
    lbl_margin_->setText("Loading…");

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);
    btn_row->addStretch(1);
    cancel_btn_ = new QPushButton("CANCEL", this);
    cancel_btn_->setObjectName("confirmCancel");
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    place_btn_ = new QPushButton("PLACE PAPER ORDERS", this);
    place_btn_->setObjectName("confirmPlace");
    place_btn_->setCursor(Qt::PointingHandCursor);
    btn_row->addWidget(cancel_btn_);
    btn_row->addWidget(place_btn_);
    root->addLayout(btn_row);

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(place_btn_, &QPushButton::clicked, this, &QDialog::accept);
}

void OrderConfirmDialog::populate_legs() {
    legs_table_->setRowCount(strategy_.legs.size());
    for (int i = 0; i < strategy_.legs.size(); ++i) {
        const StrategyLeg& leg = strategy_.legs.at(i);
        auto set_cell = [&](int col, const QString& txt, const QColor& fg = QColor()) {
            auto* item = new QTableWidgetItem(txt);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (fg.isValid())
                item->setForeground(fg);
            legs_table_->setItem(i, col, item);
        };
        set_cell(0, leg.symbol);
        const bool buy = leg.lots >= 0;
        set_cell(1, buy ? "BUY" : "SELL", buy ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE()));
        set_cell(2, QString::number(std::abs(leg.lots) * leg.lot_size));
        set_cell(3, leg.type == InstrumentType::CE ? "CE"
                  : leg.type == InstrumentType::PE ? "PE"
                                                    : "FUT");
        set_cell(4, QString::number(leg.entry_price, 'f', 2));
    }
}

void OrderConfirmDialog::start_margin_fetch() {
    auto* broker = BrokerRegistry::instance().get(chain_.broker_id);
    if (!broker) {
        on_margin_loaded(false, {}, "Broker " + chain_.broker_id + " not registered");
        return;
    }
    BrokerCredentials creds = broker->load_credentials();

    QVector<UnifiedOrder> orders;
    orders.reserve(strategy_.legs.size());
    for (const auto& leg : strategy_.legs) {
        if (!leg.is_active)
            continue;
        orders.append(leg_to_unified(leg));
    }
    if (orders.isEmpty()) {
        on_margin_loaded(false, {}, "no active legs");
        return;
    }

    QPointer<OrderConfirmDialog> self = this;
    (void)QtConcurrent::run([self, broker, creds, orders]() {
        auto resp = broker->get_basket_margins(creds, orders);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, resp]() {
                if (!self)
                    return;
                if (resp.success && resp.data.has_value())
                    self->on_margin_loaded(true, *resp.data, {});
                else
                    self->on_margin_loaded(false, {}, resp.error);
            },
            Qt::QueuedConnection);
    });

    // Hard timeout — never block the user from placing if the broker is slow.
    QTimer::singleShot(kMarginFetchTimeoutMs, this, [this]() {
        if (margin_)
            return;
        if (lbl_margin_->text() == QStringLiteral("Loading…"))
            lbl_margin_->setText("— (timeout)");
    });
}

void OrderConfirmDialog::on_margin_loaded(bool ok, BasketMargin margin, QString error) {
    if (ok) {
        margin_ = margin;
        const double m = margin.final_margin > 0 ? margin.final_margin : margin.initial_margin;
        lbl_margin_->setText(QStringLiteral("₹ ") + fmt_currency(m));
        lbl_margin_->setToolTip(QString("Initial: ₹ %1   ·   Final (after netting): ₹ %2")
                                    .arg(fmt_currency(margin.initial_margin),
                                         fmt_currency(margin.final_margin)));
    } else {
        lbl_margin_->setText("—");
        lbl_margin_->setToolTip(error.isEmpty() ? QStringLiteral("Margin unavailable")
                                                 : error);
        LOG_DEBUG("FnoBuilder", QString("basket margin failed: %1").arg(error));
    }
}

} // namespace fincept::screens::fno
