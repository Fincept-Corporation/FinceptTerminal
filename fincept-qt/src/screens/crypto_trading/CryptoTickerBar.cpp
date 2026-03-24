// CryptoTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/crypto_trading/CryptoTickerBar.h"

#include <QHBoxLayout>

namespace fincept::screens::crypto {

CryptoTickerBar::CryptoTickerBar(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoPriceRibbon");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    symbol_label_ = new QLabel("BTC/USDT");
    symbol_label_->setObjectName("cryptoCommandBarTitle");
    layout->addWidget(symbol_label_);

    price_label_ = new QLabel("--");
    price_label_->setObjectName("cryptoHeroPrice");
    layout->addWidget(price_label_);

    change_label_ = new QLabel("--");
    change_label_->setObjectName("cryptoChange");
    layout->addWidget(change_label_);

    // Bid / Ask / Spread cluster
    bid_label_ = new QLabel("B:--");
    bid_label_->setObjectName("cryptoBid");
    layout->addWidget(bid_label_);

    ask_label_ = new QLabel("A:--");
    ask_label_->setObjectName("cryptoAsk");
    layout->addWidget(ask_label_);

    spread_label_ = new QLabel("S:--");
    spread_label_->setObjectName("cryptoSpreadInline");
    layout->addWidget(spread_label_);

    // High / Low / Volume
    high_label_ = new QLabel("H:--");
    high_label_->setObjectName("cryptoStatLabel");
    layout->addWidget(high_label_);

    low_label_ = new QLabel("L:--");
    low_label_->setObjectName("cryptoStatLabel");
    layout->addWidget(low_label_);

    volume_label_ = new QLabel("Vol:--");
    volume_label_->setObjectName("cryptoStatLabel");
    layout->addWidget(volume_label_);

    layout->addStretch();
}

void CryptoTickerBar::set_symbol(const QString& symbol) {
    symbol_label_->setText(symbol);
}

void CryptoTickerBar::update_data(double price, double change_pct, double high, double low, double volume,
                                  bool ws_connected) {
    if (price <= 0)
        return;

    price_label_->setText(QString("$%1").arg(price, 0, 'f', 2));

    const bool positive = change_pct >= 0;
    change_label_->setText(QString("%1%2%").arg(positive ? "+" : "").arg(change_pct, 0, 'f', 2));

    // Only update style on state flip — P7
    if (positive != last_positive_) {
        change_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent; border: none;")
                .arg(positive ? "#16a34a" : "#dc2626"));
        last_positive_ = positive;
    }

    high_label_->setText(QString("H:%1").arg(high, 0, 'f', 2));
    low_label_->setText(QString("L:%1").arg(low, 0, 'f', 2));

    if (volume >= 1e9)
        volume_label_->setText(QString("Vol:%1B").arg(volume / 1e9, 0, 'f', 2));
    else if (volume >= 1e6)
        volume_label_->setText(QString("Vol:%1M").arg(volume / 1e6, 0, 'f', 2));
    else
        volume_label_->setText(QString("Vol:%1").arg(volume, 0, 'f', 0));

    Q_UNUSED(ws_connected);
}

void CryptoTickerBar::update_bid_ask(double bid, double ask, double spread) {
    bid_label_->setText(QString("B:%1").arg(bid, 0, 'f', 2));
    ask_label_->setText(QString("A:%1").arg(ask, 0, 'f', 2));
    spread_label_->setText(QString("S:%1").arg(spread, 0, 'f', 2));
}

} // namespace fincept::screens::crypto
