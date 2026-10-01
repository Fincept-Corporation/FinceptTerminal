// EquityTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/equity_trading/EquityTickerBar.h"

#include "screens/equity_trading/EquityTypes.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>

#include <cmath>

namespace fincept::screens::equity {

EquityTickerBar::EquityTickerBar(QWidget* parent) : QWidget(parent) {
    setObjectName("eqPriceRibbon");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    symbol_label_ = new QLabel("--");
    symbol_label_->setObjectName("eqCommandBarTitle");
    layout->addWidget(symbol_label_);

    price_label_ = new QLabel("--");
    price_label_->setObjectName("eqHeroPrice");
    layout->addWidget(price_label_);

    change_label_ = new QLabel("--");
    change_label_->setObjectName("eqChange");
    layout->addWidget(change_label_);

    bid_label_ = new QLabel("B:--");
    bid_label_->setObjectName("eqBid");
    layout->addWidget(bid_label_);

    ask_label_ = new QLabel("A:--");
    ask_label_->setObjectName("eqAsk");
    layout->addWidget(ask_label_);

    spread_label_ = new QLabel("S:--");
    spread_label_->setObjectName("eqSpreadInline");
    layout->addWidget(spread_label_);

    high_label_ = new QLabel("H:--");
    high_label_->setObjectName("eqStatLabel");
    layout->addWidget(high_label_);

    low_label_ = new QLabel("L:--");
    low_label_->setObjectName("eqStatLabel");
    layout->addWidget(low_label_);

    volume_label_ = new QLabel("Vol:--");
    volume_label_->setObjectName("eqStatLabel");
    layout->addWidget(volume_label_);

    layout->addStretch();
}

void EquityTickerBar::set_symbol(const QString& symbol) {
    const bool changed = symbol_label_->text() != symbol;
    symbol_label_->setText(symbol);
    if (!changed)
        return;
    // The price/change/H/L/volume/bid/ask belong to the PREVIOUS symbol. update_quote()
    // ignores empty (ltp <= 0) ticks, so without a reset the old symbol's numbers sit
    // under the new ticker until its first quote arrives (or forever if it never does).
    price_label_->setText(QStringLiteral("--"));
    change_label_->setText(QStringLiteral("--"));
    bid_label_->setText(QStringLiteral("B:--"));
    ask_label_->setText(QStringLiteral("A:--"));
    spread_label_->setText(QStringLiteral("S:--"));
    high_label_->setText(QStringLiteral("H:--"));
    low_label_->setText(QStringLiteral("L:--"));
    volume_label_->setText(QStringLiteral("Vol:--"));
}

void EquityTickerBar::update_quote(double ltp, double change, double change_pct, double high, double low, double volume,
                                   double bid, double ask) {
    if (ltp <= 0)
        return;

    price_label_->setText(QString("%1%2").arg(currency_sym_).arg(ltp, 0, 'f', 2));

    const bool positive = change_pct >= 0;
    change_label_->setText(QString("%1%2 (%3%4%)")
                               .arg(positive ? "+" : "")
                               .arg(change, 0, 'f', 2)
                               .arg(positive ? "+" : "")
                               .arg(change_pct, 0, 'f', 2));

    if (positive != last_positive_) {
        change_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent; border: none;")
                .arg(positive ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
        last_positive_ = positive;
    }

    if (bid > 0)
        bid_label_->setText(QString("B:%1").arg(bid, 0, 'f', 2));
    if (ask > 0)
        ask_label_->setText(QString("A:%1").arg(ask, 0, 'f', 2));
    if (bid > 0 && ask > 0)
        spread_label_->setText(QString("S:%1").arg(std::abs(ask - bid), 0, 'f', 2));

    // Like bid/ask above: a tick that carries no day high/low/volume (0) must not blank the
    // figures an earlier fuller quote already supplied.
    if (high > 0)
        high_label_->setText(QString("H:%1").arg(high, 0, 'f', 2));
    if (low > 0)
        low_label_->setText(QString("L:%1").arg(low, 0, 'f', 2));
    if (volume <= 0)
        return;

    // Crore / lakh are Indian-market units; a US/EU broker's volume reads in M/K.
    const bool indian = currency_sym_ == currency_symbol(QStringLiteral("INR"));
    if (volume >= 1e9)
        volume_label_->setText(QString("Vol:%1B").arg(volume / 1e9, 0, 'f', 2));
    else if (indian && volume >= 1e7)
        volume_label_->setText(QString("Vol:%1Cr").arg(volume / 1e7, 0, 'f', 2));
    else if (indian && volume >= 1e5)
        volume_label_->setText(QString("Vol:%1L").arg(volume / 1e5, 0, 'f', 2));
    else if (!indian && volume >= 1e6)
        volume_label_->setText(QString("Vol:%1M").arg(volume / 1e6, 0, 'f', 2));
    else if (volume >= 1e3)
        volume_label_->setText(QString("Vol:%1K").arg(volume / 1e3, 0, 'f', 1));
    else
        volume_label_->setText(QString("Vol:%1").arg(volume, 0, 'f', 0));
}

} // namespace fincept::screens::equity
