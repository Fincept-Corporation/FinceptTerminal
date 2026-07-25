// CryptoTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/crypto_trading/CryptoTickerBar.h"

#include "screens/crypto_trading/CryptoTypes.h"
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

// Set `label`'s text only when the rendered string actually changed, using
// `cache` as the last-rendered value. Replaces the old
// "round to 2 dp and compare doubles" dedup, which never fired an update for
// sub-cent assets because every tick rounded to the same 0.00.
static inline void set_if_changed(QLabel* label, QString& cache, const QString& text) {
    if (cache == text)
        return;
    cache = text;
    label->setText(text);
}

void CryptoTickerBar::update_data(double price, double change_pct, double high, double low, double volume,
                                  bool ws_connected) {
    if (price <= 0)
        return;

    set_if_changed(price_label_, last_price_text_, format_price_usd(price));

    const bool positive = change_pct >= 0;
    set_if_changed(change_label_, last_change_text_,
                   QString("%1%2%").arg(positive ? "+" : "").arg(change_pct, 0, 'f', 2));

    // Only update style on state flip — P7
    if (positive != last_positive_) {
        change_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent; border: none;")
                .arg(positive ? colors::POSITIVE() : colors::NEGATIVE()));
        last_positive_ = positive;
    }

    set_if_changed(high_label_, last_high_text_, QStringLiteral("H:") + format_price_plain(high));
    set_if_changed(low_label_, last_low_text_, QStringLiteral("L:") + format_price_plain(low));

    QString vol_text;
    if (volume >= 1e9)
        vol_text = QString("Vol:%1B").arg(volume / 1e9, 0, 'f', 2);
    else if (volume >= 1e6)
        vol_text = QString("Vol:%1M").arg(volume / 1e6, 0, 'f', 2);
    else
        vol_text = QString("Vol:%1").arg(volume, 0, 'f', 0);
    set_if_changed(volume_label_, last_volume_text_, vol_text);

    Q_UNUSED(ws_connected);
}

void CryptoTickerBar::update_bid_ask(double bid, double ask, double spread) {
    set_if_changed(bid_label_, last_bid_text_, QStringLiteral("B:") + format_price_plain(bid));
    set_if_changed(ask_label_, last_ask_text_, QStringLiteral("A:") + format_price_plain(ask));
    set_if_changed(spread_label_, last_spread_text_, QStringLiteral("S:") + format_price_plain(spread));
}

void CryptoTickerBar::update_mark_price(double mark_price, double index_price) {
    if (mark_price <= 0 && index_price <= 0) {
        mark_price_label_->setVisible(false);
        index_price_label_->setVisible(false);
        return;
    }
    if (mark_price > 0) {
        mark_price_label_->setText(QStringLiteral("Mk:") + format_price_plain(mark_price));
        mark_price_label_->setVisible(true);
    }
    if (index_price > 0) {
        index_price_label_->setText(QStringLiteral("Idx:") + format_price_plain(index_price));
        index_price_label_->setVisible(true);
    }
}

} // namespace fincept::screens::crypto
