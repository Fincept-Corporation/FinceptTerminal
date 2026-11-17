#include "screens/fno/OrderConfirmDialog.h"

#include "core/logging/Logger.h"
#include "screens/fno/FnoFormat.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
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

namespace fmt = fincept::ui::formatting;

/// A payoff bound, signed ("+₹1,240" / "-₹820"); "+∞" / "−∞" when unlimited.
QString ocd_fmt_bound(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("+∞") : QStringLiteral("−∞");
    return fno_signed_money(v, ccy);
}

UnifiedOrder leg_to_unified(const StrategyLeg& leg) {
    UnifiedOrder o;
    o.symbol = leg.symbol;
    o.exchange = QStringLiteral("NFO");
    o.side = leg.lots >= 0 ? OrderSide::Buy : OrderSide::Sell;
    o.order_type = OrderType::Limit;
    o.quantity = std::abs(double(leg.lots) * double(leg.lot_size));
    o.price = leg.entry_price;
    o.product_type = ProductType::Margin; // NRML — F&O positional
    o.validity = QStringLiteral("DAY");
    o.instrument_token = QString::number(leg.instrument_token);
    return o;
}

} // namespace

OrderConfirmDialog::OrderConfirmDialog(const Strategy& strategy, const OptionChain& chain, double premium,
                                       double max_profit, double max_loss, QWidget* parent)
    : QDialog(parent), strategy_(strategy), chain_(chain), premium_(premium), max_profit_(max_profit),
      max_loss_(max_loss), ccy_(fno_chain_ccy(chain.broker_id)) {
    setWindowTitle(tr("Confirm Paper Orders"));
    setModal(true);
    resize(680, 520);
    setStyleSheet(QString("QDialog { background:%1; }"
                          "#confirmTitle { color:%2; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
                          "#confirmSub { color:%3; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; }"
                          "#confirmKey { color:%3; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.4px; "
                          "               background:transparent; }"
                          "#confirmValue { color:%2; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
                          "#confirmValuePos { color:%4; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
                          "#confirmValueNeg { color:%5; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
                          "QPushButton#confirmPlace { background:%4; color:%9; border:none; padding:7px 18px; "
                          "                          font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:0.4px; }"
                          "QPushButton#confirmPlace:hover { background:%2; }"
                          "QPushButton#confirmCancel { background:%6; color:%2; border:1px solid %7; "
                          "                            padding:7px 14px; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; }"
                          "QPushButton#confirmCancel:hover { background:%8; }"
                          "QTableWidget { background:%1; color:%2; border:1px solid %7; gridline-color:%7; }"
                          "QHeaderView::section { background:%6; color:%3; border:none; "
                          "                       border-bottom:1px solid %7; padding:5px 8px; "
                          "                       font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.4px; }")
                      .arg(colors::BG_BASE(),        // %1
                           colors::TEXT_PRIMARY(),   // %2
                           colors::TEXT_SECONDARY(), // %3
                           colors::POSITIVE(),       // %4
                           colors::NEGATIVE(),       // %5
                           colors::BG_RAISED(),      // %6
                           colors::BORDER_DIM(),     // %7
                           colors::BG_HOVER(),       // %8
                           colors::TEXT_ON_ACCENT())); // %9

    setup_ui(premium, max_profit, max_loss);
    populate_legs();
    start_margin_fetch();
    // The leg cells carry their colours as item brushes and the rows are sized
    // for the font; rebuild them for a new theme or font size.
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        legs_table_->verticalHeader()->setDefaultSectionSize(qMax(24, QFontMetrics(fonts::font()).height() + 8));
        populate_legs();
    });
    // A currency / number-format change re-formats the figures already held.
    fmt::on_format_changed(this, [this]() {
        if (legs_table_)
            legs_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("B/S"), tr("Lots"), tr("Qty"), tr("Type"),
                                                    fmt::header(tr("Entry"), ccy_)});
        populate_legs();
        render_summary();
        render_margin();
    });
}

void OrderConfirmDialog::render_summary() {
    if (sub_label_)
        sub_label_->setText(tr("%1   ·   Expiry %2   ·   Spot %3")
                                .arg(strategy_.underlying)
                                .arg(strategy_.expiry)
                                .arg(fmt::number(chain_.spot, 2)));
    if (lbl_premium_) {
        // Dr / Cr are the debit / credit markers of a net premium (not crores).
        const QString premium = fno_money(std::abs(premium_), ccy_);
        lbl_premium_->setText(premium_ > 0   ? tr("%1  Dr", "net debit: premium paid").arg(premium)
                              : premium_ < 0 ? tr("%1  Cr", "net credit: premium received").arg(premium)
                                             : premium);
    }
    if (lbl_max_pnl_)
        lbl_max_pnl_->setText(
            QString("%1  /  %2").arg(ocd_fmt_bound(max_profit_, ccy_), ocd_fmt_bound(max_loss_, ccy_)));
}

void OrderConfirmDialog::render_margin() {
    if (!margin_ || !lbl_margin_)
        return;
    const BasketMargin& margin = *margin_;
    const double m = margin.final_margin > 0 ? margin.final_margin : margin.initial_margin;
    // The basket margin the chain's broker quotes, in that broker's (the chain's) currency.
    lbl_margin_->setText(fno_money(m, ccy_));
    lbl_margin_->setToolTip(tr("Initial: %1   ·   Final (after netting): %2")
                                .arg(fno_money(margin.initial_margin, ccy_), fno_money(margin.final_margin, ccy_)));
}

void OrderConfirmDialog::setup_ui(double premium, double max_profit, double max_loss) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // ── Title ─────────────────────────────────────────────────────────────
    title_label_ = new QLabel(tr("%1  —  paper trade preview").arg(strategy_.name), this);
    title_label_->setObjectName("confirmTitle");
    sub_label_ = new QLabel(this); // text: render_summary()
    sub_label_->setObjectName("confirmSub");
    root->addWidget(title_label_);
    root->addWidget(sub_label_);

    // ── Leg table ─────────────────────────────────────────────────────────
    legs_table_ = new QTableWidget(this);
    legs_table_->setColumnCount(6);
    legs_table_->setHorizontalHeaderLabels(
        {tr("Symbol"), tr("B/S"), tr("Lots"), tr("Qty"), tr("Type"), fmt::header(tr("Entry"), ccy_)});
    legs_table_->verticalHeader()->setVisible(false);
    legs_table_->setSelectionMode(QAbstractItemView::NoSelection);
    legs_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    legs_table_->setShowGrid(false);
    legs_table_->setFocusPolicy(Qt::NoFocus);
    legs_table_->setAccessibleName(tr("Order legs"));
    legs_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // Symbol column: sized once after the legs are filled (populate_legs), not per row.
    legs_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    legs_table_->verticalHeader()->setDefaultSectionSize(qMax(24, QFontMetrics(fonts::font()).height() + 8));
    root->addWidget(legs_table_, 1);

    legs_note_ = new QLabel(this);
    legs_note_->setObjectName("confirmSub");
    root->addWidget(legs_note_);

    // ── Summary block ─────────────────────────────────────────────────────
    auto add_kv = [this](QHBoxLayout* lay, QLabel*& v_out, QLabel*& k_out, const QString& key) {
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
        k_out = k;
    };

    auto* summary = new QHBoxLayout();
    summary->setSpacing(28);
    add_kv(summary, lbl_premium_, key_premium_, tr("Net Premium"));
    add_kv(summary, lbl_max_pnl_, key_max_pnl_, tr("Max Profit / Loss"));
    add_kv(summary, lbl_margin_, key_margin_, tr("Basket Margin"));
    summary->addStretch(1);
    root->addLayout(summary);

    lbl_premium_->setObjectName(premium > 0 ? "confirmValueNeg" : premium < 0 ? "confirmValuePos" : "confirmValue");
    Q_UNUSED(max_profit);
    Q_UNUSED(max_loss);
    render_summary(); // premium_ / max_profit_ / max_loss_ hold the figures passed in
    lbl_margin_->setText(tr("Loading…"));

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);
    btn_row->addStretch(1);
    cancel_btn_ = new QPushButton(tr("CANCEL"), this);
    cancel_btn_->setObjectName("confirmCancel");
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setAccessibleName(tr("Cancel without placing orders"));
    place_btn_ = new QPushButton(tr("PLACE PAPER ORDERS"), this);
    place_btn_->setObjectName("confirmPlace");
    place_btn_->setProperty("fincept_agent_risk", "order");
    place_btn_->setCursor(Qt::PointingHandCursor);
    place_btn_->setAccessibleName(tr("Place the listed legs as paper orders"));
    btn_row->addWidget(cancel_btn_);
    btn_row->addWidget(place_btn_);
    root->addLayout(btn_row);

    // Cancel is the default: a stray Return on a modal that dispatches a
    // leveraged multi-leg basket must not place it. Placing takes a click
    // (or an explicit Tab to the button, then Space).
    cancel_btn_->setDefault(true);
    cancel_btn_->setAutoDefault(true);
    place_btn_->setAutoDefault(false);
    setTabOrder(cancel_btn_, place_btn_);

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(place_btn_, &QPushButton::clicked, this, &QDialog::accept);
}

void OrderConfirmDialog::populate_legs() {
    // Every leg the strategy carries is listed, but a leg that will NOT be
    // submitted (toggled off, or zero lots) is rendered dim and explicitly
    // tagged so the preview can never claim more than the dispatcher places.
    // The caller's submit loop uses exactly the same predicate.
    legs_table_->setRowCount(strategy_.legs.size());
    int placeable = 0;
    for (int i = 0; i < strategy_.legs.size(); ++i) {
        const StrategyLeg& leg = strategy_.legs.at(i);
        const bool will_place = leg.is_active && leg.lots != 0;
        if (will_place)
            ++placeable;
        const QColor dim(colors::TEXT_DIM());
        auto set_cell = [&](int col, const QString& txt, const QColor& fg = QColor()) {
            auto* item = new QTableWidgetItem(txt);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (!will_place)
                item->setForeground(dim);
            else if (fg.isValid())
                item->setForeground(fg);
            if (!will_place)
                item->setToolTip(tr("Not submitted — leg is switched off or has zero lots."));
            legs_table_->setItem(i, col, item);
        };
        const int lot_size = leg.lot_size > 0 ? leg.lot_size : 0;
        const qint64 qty = qint64(std::abs(leg.lots)) * qint64(lot_size);
        const bool buy = leg.lots >= 0;
        set_cell(0, will_place ? leg.symbol : tr("%1  (skipped)").arg(leg.symbol));
        set_cell(1, buy ? tr("BUY") : tr("SELL"), buy ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE()));
        set_cell(2, fmt::quantity(std::abs(leg.lots)));
        // Lot size is spelled out: an F&O quantity is meaningless without it,
        // and a leg whose lot size never resolved (0) must be visibly broken
        // rather than quietly submitted as a 1-unit order.
        set_cell(3, lot_size > 0 ? tr("%1  (×%2)").arg(fmt::quantity(double(qty)), fmt::quantity(lot_size))
                                 : tr("— lot size unknown"),
                 lot_size > 0 ? QColor() : QColor(colors::NEGATIVE()));
        set_cell(4, leg.type == InstrumentType::CE ? "CE" : leg.type == InstrumentType::PE ? "PE" : "FUT");
        // Premium per unit in rupees; the column header names the unit.
        set_cell(5, fno_price(leg.entry_price, ccy_));
    }
    legs_table_->resizeColumnToContents(0);
    if (legs_note_) {
        legs_note_->setText(tr("%1 of %2 legs will be placed as market orders at the shown entry premium.")
                                .arg(placeable)
                                .arg(strategy_.legs.size()));
    }
}

void OrderConfirmDialog::start_margin_fetch() {
    auto* broker = BrokerRegistry::instance().get(chain_.broker_id);
    if (!broker) {
        on_margin_loaded(false, {}, tr("Broker %1 not registered").arg(chain_.broker_id));
        return;
    }
    // Credentials live under the ACCOUNT's scoped keys (account.<uuid>.*) —
    // IBroker::load_credentials() only reads the legacy single-broker keys, which
    // are empty for any account added through the multi-account flow, so the margin
    // request went out unauthenticated and the field always showed "—". Resolve the
    // account the same way the chain assembly does (active first, else first), and
    // only fall back to the legacy keys when no account credentials exist.
    BrokerCredentials creds;
    {
        auto& accounts_mgr = fincept::trading::AccountManager::instance();
        const auto accounts = accounts_mgr.list_accounts(chain_.broker_id);
        QString account_id;
        for (const auto& acct : accounts) {
            if (acct.is_active) {
                account_id = acct.account_id;
                break;
            }
        }
        if (account_id.isEmpty() && !accounts.isEmpty())
            account_id = accounts.first().account_id;
        if (!account_id.isEmpty())
            creds = accounts_mgr.load_credentials(account_id);
        if (creds.access_token.isEmpty())
            creds = broker->load_credentials();
    }

    QVector<UnifiedOrder> orders;
    orders.reserve(strategy_.legs.size());
    for (const auto& leg : strategy_.legs) {
        // Same predicate the dispatcher uses — margin must be quoted for the
        // basket that is actually going to be sent, not for the full leg list.
        if (!leg.is_active || leg.lots == 0)
            continue;
        orders.append(leg_to_unified(leg));
    }
    if (orders.isEmpty()) {
        on_margin_loaded(false, {}, tr("no active legs"));
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
        if (lbl_margin_->text() == tr("Loading…"))
            lbl_margin_->setText(tr("— (timeout)"));
    });
}

void OrderConfirmDialog::on_margin_loaded(bool ok, BasketMargin margin, QString error) {
    if (ok) {
        margin_ = margin;
        render_margin();
    } else {
        lbl_margin_->setText("—");
        lbl_margin_->setToolTip(error.isEmpty() ? tr("Margin unavailable") : error);
        LOG_DEBUG("FnoBuilder", QString("basket margin failed: %1").arg(error));
    }
}

void OrderConfirmDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void OrderConfirmDialog::retranslateUi() {
    setWindowTitle(tr("Confirm Paper Orders"));
    if (title_label_)
        title_label_->setText(tr("%1  —  paper trade preview").arg(strategy_.name));
    render_summary(); // spot line and the Dr / Cr markers are translated
    if (legs_table_) {
        legs_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("B/S"), tr("Lots"), tr("Qty"), tr("Type"), fmt::header(tr("Entry"), ccy_)});
        populate_legs(); // re-renders the per-row "(skipped)" / lot-size strings
    }
    if (key_premium_)
        key_premium_->setText(tr("Net Premium").toUpper());
    if (key_max_pnl_)
        key_max_pnl_->setText(tr("Max Profit / Loss").toUpper());
    if (key_margin_)
        key_margin_->setText(tr("Basket Margin").toUpper());
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (place_btn_)
        place_btn_->setText(tr("PLACE PAPER ORDERS"));
}

} // namespace fincept::screens::fno
