// CryptoTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/crypto_trading/CryptoTickerBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>

#include <cmath>

using namespace fincept::ui;

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

    mark_price_label_ = new QLabel("Mk:--");
    mark_price_label_->setObjectName("cryptoStatLabel");
    mark_price_label_->setVisible(false);
    layout->addWidget(mark_price_label_);

    index_price_label_ = new QLabel("Idx:--");
    index_price_label_->setObjectName("cryptoStatLabel");
    index_price_label_->setVisible(false);
    layout->addWidget(index_price_label_);

    layout->addStretch();
}

void CryptoTickerBar::set_symbol(const QString& symbol) {
    symbol_label_->setText(symbol);
}

// Round a value to the display precision (here 2 decimals). Two inputs that
// round identically mean identical rendered text — no setText needed.
static inline double round_pp(double v, int places = 2) {
    double mul = 1.0;
    for (int i = 0; i < places; ++i)
        mul *= 10.0;
    return std::round(v * mul) / mul;
}

void CryptoTickerBar::update_data(double price, double change_pct, double high, double low, double volume,
                                  bool ws_connected) {
    if (price <= 0)
        return;

    const double price_disp = round_pp(price);
    if (price_disp != last_price_display_) {
        price_label_->setText(QString("$%1").arg(price_disp, 0, 'f', 2));
        last_price_display_ = price_disp;
    }

    const bool positive = change_pct >= 0;
    const double change_disp = round_pp(change_pct);
    if (change_disp != last_change_display_) {
        change_label_->setText(QString("%1%2%").arg(positive ? "+" : "").arg(change_disp, 0, 'f', 2));
        last_change_display_ = change_disp;
    }

    // Only update style on state flip — P7
    if (positive != last_positive_) {
        change_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent; border: none;")
                .arg(positive ? colors::POSITIVE() : colors::NEGATIVE()));
        last_positive_ = positive;
    }

    const double high_disp = round_pp(high);
    if (high_disp != last_high_display_) {
        high_label_->setText(QString("H:%1").arg(high_disp, 0, 'f', 2));
        last_high_display_ = high_disp;
    }
    const double low_disp = round_pp(low);
    if (low_disp != last_low_display_) {
        low_label_->setText(QString("L:%1").arg(low_disp, 0, 'f', 2));
        last_low_display_ = low_disp;
    }

    int vol_unit;
    double vol_disp;
    QString vol_text;
    if (volume >= 1e9) {
        vol_unit = 2;
        vol_disp = round_pp(volume / 1e9);
        vol_text = QString("Vol:%1B").arg(vol_disp, 0, 'f', 2);
    } else if (volume >= 1e6) {
        vol_unit = 1;
        vol_disp = round_pp(volume / 1e6);
        vol_text = QString("Vol:%1M").arg(vol_disp, 0, 'f', 2);
    } else {
        vol_unit = 0;
        vol_disp = std::round(volume);
        vol_text = QString("Vol:%1").arg(vol_disp, 0, 'f', 0);
    }
    if (vol_unit != last_volume_unit_ || vol_disp != last_volume_display_) {
        volume_label_->setText(vol_text);
        last_volume_unit_ = vol_unit;
        last_volume_display_ = vol_disp;
    }

    Q_UNUSED(ws_connected);
}

void CryptoTickerBar::update_bid_ask(double bid, double ask, double spread) {
    const double b = round_pp(bid);
    const double a = round_pp(ask);
    const double s = round_pp(spread);
    if (b != last_bid_display_) {
        bid_label_->setText(QString("B:%1").arg(b, 0, 'f', 2));
        last_bid_display_ = b;
    }
    if (a != last_ask_display_) {
        ask_label_->setText(QString("A:%1").arg(a, 0, 'f', 2));
        last_ask_display_ = a;
    }
    if (s != last_spread_display_) {
        spread_label_->setText(QString("S:%1").arg(s, 0, 'f', 2));
        last_spread_display_ = s;
    }
}

void CryptoTickerBar::update_mark_price(double mark_price, double index_price) {
    if (mark_price <= 0 && index_price <= 0) {
        mark_price_label_->setVisible(false);
        index_price_label_->setVisible(false);
        return;
    }
    if (mark_price > 0) {
        mark_price_label_->setText(QString("Mk:%1").arg(mark_price, 0, 'f', 2));
        mark_price_label_->setVisible(true);
    }
    if (index_price > 0) {
        index_price_label_->setText(QString("Idx:%1").arg(index_price, 0, 'f', 2));
        index_price_label_->setVisible(true);
    }
}

} // namespace fincept::screens::crypto
