// EquityTickerBar.cpp — dense price ribbon with bid/ask/spread
#include "screens/equity_trading/EquityTickerBar.h"

#include "screens/equity_trading/EquityTypes.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStyle>

#include <cmath>

namespace fincept::screens::equity {

EquityTickerBar::EquityTickerBar(QWidget* parent) : QWidget(parent) {
    setObjectName("eqPriceRibbon");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    symbol_label_ = new QLabel("--");
    symbol_label_->setObjectName("eqCommandBarTitle");
    layout->addWidget(symbol_label_);

    // Source-level state chip (set_notice), hidden until a source has something to say.
    notice_label_ = new QLabel;
    notice_label_->setObjectName("eqRibbonNotice"); // styled by the screen sheet (EquityStyle)
    notice_label_->hide();
    layout->addWidget(notice_label_);

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
    // No stretch: the ribbon is as wide as its fields (a stretch would make it
    // expanding and take the command bar's spare width from the actions).

    // Settings > Currency & Number Format changed: re-format the last quote.
    fincept::ui::formatting::on_format_changed(this, [this]() { repaint_last_quote(); });
}

void EquityTickerBar::set_symbol(const QString& symbol) {
    const bool changed = symbol_label_->text() != symbol;
    symbol_label_->setText(symbol);
    if (!changed)
        return;
    // The price/change/H/L/volume/bid/ask belong to the PREVIOUS symbol. update_quote()
    // ignores empty (ltp <= 0) ticks, so without a reset the old symbol's numbers sit
    // under the new ticker until its first quote arrives (or forever if it never does).
    clear_quote();
}

void EquityTickerBar::clear_quote() {
    price_label_->setText(QStringLiteral("--"));
    change_label_->setText(QStringLiteral("--"));
    bid_label_->setText(QStringLiteral("B:--"));
    ask_label_->setText(QStringLiteral("A:--"));
    spread_label_->setText(QStringLiteral("S:--"));
    high_label_->setText(QStringLiteral("H:--"));
    low_label_->setText(QStringLiteral("L:--"));
    volume_label_->setText(QStringLiteral("Vol:--"));
}

void EquityTickerBar::set_notice(const QString& text, const QString& detail) {
    const bool show = !text.isEmpty();
    notice_label_->setText(text);
    notice_label_->setToolTip(detail);
    notice_label_->setVisible(show);
    // The fields would only read "--" beside the notice; the notice says why.
    set_price_fields_visible(!show);
}

void EquityTickerBar::set_price_fields_visible(bool visible) {
    fields_visible_ = visible;
    price_label_->setVisible(visible);
    change_label_->setVisible(visible);
    updateGeometry();
    refit();
}

void EquityTickerBar::set_symbol_visible(bool visible) {
    symbol_visible_ = visible;
    symbol_label_->setVisible(visible);
    updateGeometry();
    refit();
}

bool EquityTickerBar::field_wanted(const QLabel* l) const {
    if (!fields_visible_)
        return false;
    if (l == bid_label_ || l == ask_label_ || l == spread_label_)
        return bid_ask_visible_;
    return true;
}

QSize EquityTickerBar::minimumSizeHint() const {
    const int sp = layout()->spacing();
    int w = 0;
    int h = 0;
    auto add = [&](const QLabel* l) {
        const QSize s = l->sizeHint();
        w += (w > 0 ? sp : 0) + s.width();
        h = std::max(h, s.height());
    };
    if (symbol_visible_)
        add(symbol_label_);
    if (notice_label_->isVisibleTo(this))
        add(notice_label_);
    if (fields_visible_) {
        add(price_label_);
        add(change_label_);
    }
    return {w, h};
}

QSize EquityTickerBar::sizeHint() const {
    QSize s = minimumSizeHint();
    const int sp = layout()->spacing();
    for (const QLabel* l : {bid_label_, ask_label_, spread_label_, high_label_, low_label_, volume_label_}) {
        if (field_wanted(l))
            s.rwidth() += sp + l->sizeHint().width();
    }
    return s;
}

void EquityTickerBar::refit() {
    // Priority order: what a trader needs first stays longest.
    const int sp = layout()->spacing();
    int used = minimumSizeHint().width();
    bool room = true;
    for (QLabel* l : {bid_label_, ask_label_, spread_label_, high_label_, low_label_, volume_label_}) {
        bool show = false;
        if (field_wanted(l) && room) {
            const int w = sp + l->sizeHint().width();
            if (used + w <= width()) {
                used += w;
                show = true;
            } else {
                room = false; // keep the order: nothing after a field that did not fit
            }
        }
        if (l->isVisibleTo(this) != show)
            l->setVisible(show);
    }
}

void EquityTickerBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    refit();
}

void EquityTickerBar::set_price_format(const QString& currency, int digits) {
    price_currency_ = currency;
    price_digits_ = digits;
}

void EquityTickerBar::set_bid_ask_visible(bool visible) {
    bid_ask_visible_ = visible;
    updateGeometry();
    refit();
}

QString EquityTickerBar::px(double v) const {
    fincept::ui::formatting::InstrumentFmt inst;
    inst.currency = effective_currency();
    inst.decimals = price_currency_.isEmpty() ? -1 : price_digits_;
    return fincept::ui::formatting::price(v, inst);
}

void EquityTickerBar::repaint_last_quote() {
    if (last_.ltp > 0)
        update_quote(last_.ltp, last_.change, last_.change_pct, last_.high, last_.low, last_.volume, last_.bid,
                     last_.ask);
}

void EquityTickerBar::update_quote(double ltp, double change, double change_pct, double high, double low, double volume,
                                   double bid, double ask) {
    if (ltp <= 0)
        return;
    last_ = {ltp, change, change_pct, high, low, volume, bid, ask};

    // A price in its own currency, never converted ("$351.71", "117.70p", "₹1,344.80";
    // no unit while the currency is unknown).
    fincept::ui::formatting::InstrumentFmt inst;
    inst.currency = effective_currency();
    inst.decimals = price_currency_.isEmpty() ? -1 : price_digits_;
    price_label_->setText(inst.currency.isEmpty() ? fincept::ui::formatting::price(ltp, inst)
                                                  : fincept::ui::formatting::price_with_symbol(ltp, inst));

    // Direction by sign + arrow + colour (never colour alone).
    const int sign = change_pct > 0 ? 1 : (change_pct < 0 ? -1 : 0);
    const QString arrow = sign > 0 ? QStringLiteral("\xE2\x96\xB2 ") : (sign < 0 ? QStringLiteral("\xE2\x96\xBC ") : QString());
    change_label_->setText(arrow + fincept::ui::formatting::price_change(change, ltp, inst) + QStringLiteral(" (") +
                           fincept::ui::formatting::pct(change_pct, 2, true) + QStringLiteral(")"));

    if (sign != last_sign_) {
        // Direction colour from the screen sheet (#eqChange[dir=…]); a re-polish only
        // when the sign flips, never per tick.
        change_label_->setProperty("dir", sign > 0 ? "up" : (sign < 0 ? "down" : "flat"));
        change_label_->style()->unpolish(change_label_);
        change_label_->style()->polish(change_label_);
        last_sign_ = sign;
        last_positive_ = sign >= 0;
    }

    if (bid > 0)
        bid_label_->setText(QString("B:%1").arg(px(bid)));
    if (ask > 0)
        ask_label_->setText(QString("A:%1").arg(px(ask)));
    if (bid > 0 && ask > 0)
        spread_label_->setText(QString("S:%1").arg(px(std::abs(ask - bid))));

    // Like bid/ask above: a tick that carries no day high/low/volume (0) must not blank the
    // figures an earlier fuller quote already supplied.
    if (high > 0)
        high_label_->setText(QString("H:%1").arg(px(high)));
    if (low > 0)
        low_label_->setText(QString("L:%1").arg(px(low)));
    if (volume <= 0)
        return;

    // The user's compact scheme (Settings); on Automatic, lakh / crore for an
    // INR-family market and K / M / B elsewhere — keyed on the quote's currency.
    volume_label_->setText(
        QStringLiteral("Vol:%1")
            .arg(fincept::ui::formatting::compact(volume, fincept::ui::formatting::Compact::FromSettings,
                                                  fincept::ui::formatting::major_currency(effective_currency()))));
}

} // namespace fincept::screens::equity
