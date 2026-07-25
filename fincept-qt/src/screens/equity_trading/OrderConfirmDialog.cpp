#include "screens/equity_trading/OrderConfirmDialog.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::screens {

using trading::OrderSide;
using trading::OrderType;
using trading::UnifiedOrder;

namespace {

// Hard cap on the background margin call — the trader must never be blocked by
// a slow broker (same contract as screens/fno/OrderConfirmDialog).
constexpr int kMarginFetchTimeoutMs = 5000;

// A limit/trigger this far from the reference LTP is almost always a typo
// (missing decimal point, wrong tick). Raise a banner rather than silently
// sending it.
constexpr double kFatFingerPct = 10.0;

QString type_str(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return QStringLiteral("MARKET");
        case OrderType::Limit:
            return QStringLiteral("LIMIT");
        case OrderType::StopLoss:
            return QStringLiteral("SL-M");
        case OrderType::StopLossLimit:
            return QStringLiteral("SL");
    }
    return QStringLiteral("MARKET");
}

QString money(double v) {
    return QLocale(QLocale::English).toString(v, 'f', 2);
}

bool needs_limit(OrderType t) {
    return t == OrderType::Limit || t == OrderType::StopLossLimit;
}

bool needs_trigger(OrderType t) {
    return t == OrderType::StopLoss || t == OrderType::StopLossLimit;
}

} // namespace

OrderConfirmDialog::OrderConfirmDialog(QWidget* parent, const UnifiedOrder& order, const QString& account_label,
                                       double ref_price, const QString& account_id)
    : QDialog(parent), order_(order), account_id_(account_id) {
    setWindowTitle(tr("Confirm Order"));
    setModal(true);
    setMinimumWidth(430);

    const bool is_buy = order.side == OrderSide::Buy;
    const QString side = is_buy ? tr("BUY") : tr("SELL");
    const QString side_color = is_buy ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

    // Live vs paper is the single most consequential fact on this dialog, so it
    // is resolved from the account record rather than inferred from the caller.
    bool is_live = false;
    if (!account_id_.isEmpty()) {
        const auto acct = trading::AccountManager::instance().get_account(account_id_);
        is_live = (acct.trading_mode == QLatin1String("live"));
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(10);

    // ── Banner: LIVE MONEY vs PAPER ──────────────────────────────────────────
    auto* banner = new QLabel(is_live ? tr("\xe2\x9a\xa0  LIVE ORDER \xe2\x80\x94 real money. Review before sending.")
                                      : tr("PAPER ORDER \xe2\x80\x94 simulated. Review before sending."));
    banner->setWordWrap(true);
    banner->setStyleSheet(QString("color:%1;font-weight:700;")
                              .arg(is_live ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
    root->addWidget(banner);

    // ── Headline: SIDE QTY SYMBOL @ price ────────────────────────────────────
    const QString px_headline = needs_limit(order.order_type) ? money(order.price) : tr("MARKET");
    auto* headline = new QLabel(QStringLiteral("%1  %2  %3  @ %4")
                                    .arg(side)
                                    .arg(QString::number(order.quantity, 'f', 0))
                                    .arg(order.symbol)
                                    .arg(px_headline));
    headline->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(side_color));
    root->addWidget(headline);

    auto* rule = new QFrame;
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);
    rule->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
    root->addWidget(rule);

    // ── Detail grid — every field that reaches the broker ────────────────────
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(5);
    int r = 0;
    auto add_row = [&](const QString& k, const QString& v, const QString& color = {}) -> QLabel* {
        auto* kl = new QLabel(k);
        kl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_SECONDARY()));
        auto* vl = new QLabel(v);
        vl->setStyleSheet(
            QString("color:%1;font-weight:700;").arg(color.isEmpty() ? ui::colors::TEXT_PRIMARY() : color));
        grid->addWidget(kl, r, 0);
        grid->addWidget(vl, r, 1);
        ++r;
        return vl;
    };

    add_row(tr("Account"), account_label, is_live ? ui::colors::NEGATIVE() : ui::colors::POSITIVE());
    add_row(tr("Mode"), is_live ? tr("LIVE") : tr("PAPER"), is_live ? ui::colors::NEGATIVE() : ui::colors::POSITIVE());
    add_row(tr("Action"), side, side_color);
    add_row(tr("Symbol"), order.symbol);
    add_row(tr("Exchange"), order.exchange.isEmpty() ? tr("(broker default)") : order.exchange);
    // Product (MIS/CNC/NRML) decides overnight carry + leverage — it was
    // previously invisible here even though it always reaches the broker.
    add_row(tr("Product"), QString::fromLatin1(trading::product_to_broker_str(order.product_type)));
    add_row(tr("Quantity"), QString::number(order.quantity, 'f', 0));
    add_row(tr("Type"), type_str(order.order_type));
    if (needs_limit(order.order_type))
        add_row(tr("Limit price"), money(order.price));
    if (needs_trigger(order.order_type) || order.stop_price > 0)
        add_row(tr("Trigger"), money(order.stop_price));
    if (order.stop_loss > 0)
        add_row(tr("Stop loss"), money(order.stop_loss), ui::colors::NEGATIVE());
    if (order.take_profit > 0)
        add_row(tr("Take profit"), money(order.take_profit), ui::colors::POSITIVE());
    add_row(tr("Validity"), order.validity.isEmpty() ? QStringLiteral("DAY") : order.validity);
    if (ref_price > 0)
        add_row(tr("Reference LTP"), money(ref_price));

    // Estimated value uses the price that will actually be paid: the limit for a
    // limit order, the reference LTP for a market order. (Previously this always
    // used the LTP, so a limit order's estimate was simply wrong.)
    const double est_px = needs_limit(order.order_type) && order.price > 0 ? order.price : ref_price;
    if (est_px > 0)
        add_row(tr("Est. value"), money(est_px * order.quantity));
    margin_value_ = add_row(tr("Est. margin"), is_live ? tr("Loading\xe2\x80\xa6") : QStringLiteral("\xe2\x80\x94"));
    root->addLayout(grid);

    // ── Fat-finger guard ─────────────────────────────────────────────────────
    if (ref_price > 0) {
        double check_px = 0.0;
        QString what;
        if (needs_limit(order.order_type) && order.price > 0) {
            check_px = order.price;
            what = tr("limit price");
        } else if (needs_trigger(order.order_type) && order.stop_price > 0) {
            check_px = order.stop_price;
            what = tr("trigger price");
        }
        if (check_px > 0) {
            const double dev = std::fabs(check_px - ref_price) / ref_price * 100.0;
            if (dev >= kFatFingerPct) {
                auto* warn = new QLabel(tr("\xe2\x9a\xa0  The %1 (%2) is %3% away from the last traded price (%4). "
                                           "Check for a typo before sending.")
                                            .arg(what, money(check_px), QString::number(dev, 'f', 1), money(ref_price)));
                warn->setWordWrap(true);
                warn->setStyleSheet(QString("color:%1;font-weight:700;").arg(ui::colors::WARNING()));
                root->addWidget(warn);
            }
        }
    }

    // ── Buttons ──────────────────────────────────────────────────────────────
    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* cancel = new QPushButton(tr("CANCEL"));
    cancel->setObjectName(QStringLiteral("orderConfirmCancel"));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setAccessibleName(tr("Cancel order"));
    send_btn_ = new QPushButton(is_buy ? tr("SEND BUY ORDER") : tr("SEND SELL ORDER"));
    send_btn_->setObjectName(QStringLiteral("orderConfirmSend"));
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setAccessibleName(tr("Send order to broker"));
    send_btn_->setStyleSheet(QString("QPushButton{background:%1;color:#000;font-weight:700;"
                                     "padding:6px 20px;border:none;}")
                                 .arg(side_color));
    // CANCEL is the default and holds focus: a stray Enter (the trader just
    // pressed Enter in the quantity box) must never fire a live order. The send
    // button requires a deliberate click, Space on focus, or Alt+S.
    cancel->setDefault(true);
    cancel->setAutoDefault(true);
    send_btn_->setDefault(false);
    send_btn_->setAutoDefault(false);
    send_btn_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
    btns->addWidget(cancel);
    btns->addWidget(send_btn_);
    root->addLayout(btns);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    // Disable first, then accept — a double-click can otherwise land twice
    // before the modal loop unwinds.
    connect(send_btn_, &QPushButton::clicked, send_btn_, [this]() { send_btn_->setEnabled(false); });
    connect(send_btn_, &QPushButton::clicked, this, &QDialog::accept);

    setTabOrder(cancel, send_btn_);
    cancel->setFocus();

    if (is_live)
        start_margin_fetch();
}

void OrderConfirmDialog::start_margin_fetch() {
    if (!margin_value_ || account_id_.isEmpty())
        return;

    const auto creds = trading::AccountManager::instance().load_credentials(account_id_);
    if (creds.api_key.isEmpty()) {
        margin_value_->setText(QStringLiteral("\xe2\x80\x94"));
        margin_done_ = true;
        return;
    }
    auto* broker = trading::BrokerRegistry::instance().get(creds.broker_id);
    if (!broker) {
        margin_value_->setText(QStringLiteral("\xe2\x80\x94"));
        margin_done_ = true;
        return;
    }

    QPointer<OrderConfirmDialog> self = this;
    const auto order = order_;
    (void)QtConcurrent::run([self, broker, creds, order]() {
        auto resp = broker->get_order_margins(creds, order);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, resp]() {
                if (!self || !self->margin_value_)
                    return;
                self->margin_done_ = true;
                if (resp.success && resp.data.has_value() && resp.data->error.isEmpty()) {
                    self->margin_value_->setText(money(resp.data->total));
                } else {
                    self->margin_value_->setText(QStringLiteral("\xe2\x80\x94"));
                    self->margin_value_->setToolTip(resp.error.isEmpty() ? tr("Margin unavailable") : resp.error);
                    LOG_DEBUG("EquityConfirm", QString("order margin unavailable: %1").arg(resp.error));
                }
            },
            Qt::QueuedConnection);
    });

    // Never let a slow broker leave the value spinning on "Loading…".
    QPointer<OrderConfirmDialog> guard = this;
    QTimer::singleShot(kMarginFetchTimeoutMs, this, [guard]() {
        if (!guard || guard->margin_done_ || !guard->margin_value_)
            return;
        guard->margin_value_->setText(tr("\xe2\x80\x94 (timeout)"));
    });
}

bool OrderConfirmDialog::confirm(QWidget* parent, const UnifiedOrder& order, const QString& account_label,
                                 double ref_price, const QString& account_id) {
    OrderConfirmDialog dlg(parent, order, account_label, ref_price, account_id);
    return dlg.exec() == QDialog::Accepted;
}

} // namespace fincept::screens
