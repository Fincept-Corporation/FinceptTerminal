#include "screens/equity_trading/OrderConfirmDialog.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {

using trading::OrderSide;
using trading::OrderType;
using trading::UnifiedOrder;

namespace {
QString type_str(OrderType t) {
    switch (t) {
        case OrderType::Market: return QStringLiteral("MARKET");
        case OrderType::Limit: return QStringLiteral("LIMIT");
        case OrderType::StopLoss: return QStringLiteral("SL-M");
        case OrderType::StopLossLimit: return QStringLiteral("SL");
    }
    return QStringLiteral("MARKET");
}
} // namespace

OrderConfirmDialog::OrderConfirmDialog(QWidget* parent, const UnifiedOrder& order,
                                       const QString& account_label, double ref_price)
    : QDialog(parent) {
    setWindowTitle(tr("Confirm Order"));
    setModal(true);
    setMinimumWidth(360);

    const bool is_buy = order.side == OrderSide::Buy;
    const QString side = is_buy ? tr("BUY") : tr("SELL");
    const QString side_color = is_buy ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto* banner = new QLabel(tr("\xe2\x9a\xa0  SEMI-AUTO — review before sending"));
    banner->setStyleSheet(QString("color:%1;font-weight:700;").arg(ui::colors::AMBER()));
    root->addWidget(banner);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(6);
    int r = 0;
    auto add_row = [&](const QString& k, const QString& v, const QString& color = {}) {
        auto* kl = new QLabel(k);
        kl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_SECONDARY()));
        auto* vl = new QLabel(v);
        vl->setStyleSheet(QString("color:%1;font-weight:700;")
                              .arg(color.isEmpty() ? ui::colors::TEXT_PRIMARY() : color));
        grid->addWidget(kl, r, 0);
        grid->addWidget(vl, r, 1);
        ++r;
    };
    add_row(tr("Account"), account_label);
    add_row(tr("Action"), side, side_color);
    add_row(tr("Symbol"), order.symbol);
    add_row(tr("Exchange"), order.exchange);
    add_row(tr("Quantity"), QString::number(order.quantity));
    add_row(tr("Type"), type_str(order.order_type));
    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
        add_row(tr("Price"), QString::number(order.price, 'f', 2));
    if (order.stop_price > 0)
        add_row(tr("Trigger"), QString::number(order.stop_price, 'f', 2));
    if (ref_price > 0)
        add_row(tr("Est. value"), QString::number(ref_price * order.quantity, 'f', 2));
    root->addLayout(grid);

    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* send = new QPushButton(tr("Send Order"));
    send->setDefault(true);
    send->setStyleSheet(QString("QPushButton{background:%1;color:#000;font-weight:700;"
                                "padding:5px 18px;border-radius:2px;}")
                            .arg(side_color));
    btns->addWidget(cancel);
    btns->addWidget(send);
    root->addLayout(btns);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(send, &QPushButton::clicked, this, &QDialog::accept);
}

bool OrderConfirmDialog::confirm(QWidget* parent, const UnifiedOrder& order,
                                 const QString& account_label, double ref_price) {
    OrderConfirmDialog dlg(parent, order, account_label, ref_price);
    return dlg.exec() == QDialog::Accepted;
}

} // namespace fincept::screens
