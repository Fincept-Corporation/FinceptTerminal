// CryptoTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/crypto_trading/CryptoTickerBar.h"

#include "screens/crypto_trading/CryptoTypes.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
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

    symbol_label_ = new QLabel("BTC/USDT"); // the screen's initial pair (CryptoTradingScreen::selected_symbol_)
    price_market_ = symbol_label_->text();
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

    // A currency / number-format change re-renders the last values (no fetch).
    fincept::ui::formatting::on_format_changed(this, [this]() { render_from_cache(); });
}

void CryptoTickerBar::set_symbol(const QString& symbol) {
    if (symbol_label_->text() != symbol) {
        price_market_ = symbol; // until the screen names the market the prices come from
        price_ = change_pct_ = high_ = low_ = volume_ = 0.0;
        bid_ = ask_ = spread_ = mark_ = index_ = 0.0;
        has_quote_ = has_bid_ask_ = false;
        // New pair — blank the readouts so the previous pair's price / bid / ask
        // aren't shown under the new name until its first tick (update_data
        // ignores a non-positive price, so nothing else would clear them).
        price_label_->setText(QStringLiteral("--"));
        change_label_->setText(QStringLiteral("--"));
        bid_label_->setText(QStringLiteral("B:--"));
        ask_label_->setText(QStringLiteral("A:--"));
        spread_label_->setText(QStringLiteral("S:--"));
        high_label_->setText(QStringLiteral("H:--"));
        low_label_->setText(QStringLiteral("L:--"));
        volume_label_->setText(QStringLiteral("Vol:--"));
        mark_price_label_->setVisible(false);
        index_price_label_->setVisible(false);
        last_price_text_.clear();
        last_change_text_.clear();
        last_high_text_.clear();
        last_low_text_.clear();
        last_volume_text_.clear();
        last_bid_text_.clear();
        last_ask_text_.clear();
        last_spread_text_.clear();
    }
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

void CryptoTickerBar::set_price_market(const QString& market) {
    const QString m = market.trimmed().isEmpty() ? symbol_label_->text() : market.trimmed();
    if (m == price_market_)
        return;
    price_market_ = m;
    render_from_cache();
}

void CryptoTickerBar::update_data(double price, double change_pct, double high, double low, double volume,
                                  bool ws_connected) {
    Q_UNUSED(ws_connected);
    if (price <= 0)
        return;
    price_ = price;
    change_pct_ = change_pct;
    high_ = high;
    low_ = low;
    volume_ = volume;
    has_quote_ = true;
    render_quote();
}

void CryptoTickerBar::render_quote() {
    if (!has_quote_)
        return;
    namespace fmt = fincept::ui::formatting;
    // The hero price carries its quote asset ("95,012.50 USDT", "$95,012.50" for
    // a USD pair); the readouts beside it are in the same unit.
    set_if_changed(price_label_, last_price_text_, pair_price_with_unit(price_, price_market_));

    const bool positive = change_pct_ >= 0;
    set_if_changed(change_label_, last_change_text_, fmt::pct(change_pct_, 2)); // signed: never colour alone

    // Only update style on state flip — P7
    if (positive != last_positive_) {
        change_label_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; border: none;")
                .arg(positive ? colors::POSITIVE() : colors::NEGATIVE()));
        last_positive_ = positive;
    }

    set_if_changed(high_label_, last_high_text_, QStringLiteral("H:") + pair_price(high_, price_market_));
    set_if_changed(low_label_, last_low_text_, QStringLiteral("L:") + pair_price(low_, price_market_));
    // Base-asset volume (a quantity), abbreviated per the user's compact-number scheme.
    set_if_changed(volume_label_, last_volume_text_, QStringLiteral("Vol:") + fmt::volume(volume_));
}

void CryptoTickerBar::update_bid_ask(double bid, double ask, double spread) {
    bid_ = bid;
    ask_ = ask;
    spread_ = spread;
    has_bid_ask_ = true;
    render_bid_ask();
}

void CryptoTickerBar::render_bid_ask() {
    if (!has_bid_ask_)
        return;
    set_if_changed(bid_label_, last_bid_text_, QStringLiteral("B:") + pair_price(bid_, price_market_));
    set_if_changed(ask_label_, last_ask_text_, QStringLiteral("A:") + pair_price(ask_, price_market_));
    set_if_changed(spread_label_, last_spread_text_,
                   QStringLiteral("S:") + pair_price_diff(spread_, ask_ > 0.0 ? ask_ : bid_, price_market_));
}

void CryptoTickerBar::update_mark_price(double mark_price, double index_price) {
    mark_ = mark_price;
    index_ = index_price;
    render_mark();
}

void CryptoTickerBar::render_mark() {
    if (mark_ <= 0 && index_ <= 0) {
        mark_price_label_->setVisible(false);
        index_price_label_->setVisible(false);
        return;
    }
    if (mark_ > 0) {
        mark_price_label_->setText(QStringLiteral("Mk:") + pair_price(mark_, price_market_));
        mark_price_label_->setVisible(true);
    }
    if (index_ > 0) {
        index_price_label_->setText(QStringLiteral("Idx:") + pair_price(index_, price_market_));
        index_price_label_->setVisible(true);
    }
}

void CryptoTickerBar::render_from_cache() {
    // Drop the dedup caches: the same value renders differently now.
    last_price_text_.clear();
    last_change_text_.clear();
    last_high_text_.clear();
    last_low_text_.clear();
    last_volume_text_.clear();
    last_bid_text_.clear();
    last_ask_text_.clear();
    last_spread_text_.clear();
    render_quote();
    render_bid_ask();
    render_mark();
}

} // namespace fincept::screens::crypto
