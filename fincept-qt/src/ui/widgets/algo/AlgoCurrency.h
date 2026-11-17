#pragma once
// AlgoCurrency — which currency the algo-trading screens print an amount in, and
// money spin boxes whose currency follows the chosen symbol.
//
// A deployment's or backtest's P&L, order value, capital and risk limits are the
// instrument's price x quantity, so they are in its QUOTE currency, minor units
// included (an LSE listing is quoted in pence: "GBp"). Nothing here converts a
// price; algo_sort_key() normalises only to ORDER a column that mixes currencies.

#include "algo_engine/AlgoMarket.h"
#include "services/markets/FxRateService.h"
#include "trading/BrokerRegistry.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSpinBox>
#include <QString>

#include <limits>

namespace fincept::ui::algo {

/// Quote currency of `symbol` for display: a pair's quote asset ("BTC-USD" -> USD,
/// "ETH/USDT" -> USDT, "EURUSD=X" -> USD), else the listing venue's currency for
/// the symbol and exchange hint ("RELIANCE" + "NSE" -> INR, "VOD.L" -> GBp), else
/// the broker's account currency. Empty when none is known: amounts then print
/// without a unit rather than in a guessed currency (never the display currency).
inline QString algo_currency_for(const QString& symbol, const QString& exchange_hint = {},
                                 const QString& broker_id = {}) {
    const QString sym = symbol.trimmed();
    if (!sym.isEmpty()) {
        const QString pair_ccy = fincept::ui::formatting::instrument_for_symbol(sym).currency;
        if (!pair_ccy.isEmpty())
            return pair_ccy;
        const QString venue_ccy = fincept::algo::venue_currency_of(fincept::algo::algo_venue(sym, exchange_hint));
        if (!venue_ccy.isEmpty())
            return venue_ccy;
    }
    if (!broker_id.isEmpty())
        if (auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id))
            return broker->profile().currency; // may be empty: then unknown
    return {};
}

/// Puts `ccy`'s unit on a money spin box (prefix or suffix per the user's
/// settings) and keeps it in step with format changes; an empty `ccy` (unknown)
/// shows no unit. Call it again whenever the currency changes: a new binding
/// replaces the previous one, and an unknown currency ends it.
inline void algo_bind_money_spin(QAbstractSpinBox* spin, const QString& ccy) {
    namespace fmt = fincept::ui::formatting;
    if (!spin)
        return;
    if (!ccy.isEmpty()) {
        fmt::bind_currency_affix(spin, ccy);
        return;
    }
    fmt::unbind_currency(spin);
    if (auto* d = qobject_cast<QDoubleSpinBox*>(spin)) {
        d->setPrefix(QString());
        d->setSuffix(QString());
    } else if (auto* i = qobject_cast<QSpinBox*>(spin)) {
        i->setPrefix(QString());
        i->setSuffix(QString());
    }
}

/// Sort key of an amount in a table column that can mix currencies (amounts in
/// different currencies are never compared raw): `v` itself when the column holds
/// one currency, else `v` in USD. No currency or no rate in a mixed column ->
/// lowest(), so the row sorts at one end instead of among amounts it cannot be
/// compared with. The missing rate is requested; the next render re-sorts.
inline double algo_sort_key(double v, const QString& ccy, bool column_mixed) {
    if (!column_mixed)
        return v;
    if (ccy.isEmpty())
        return std::numeric_limits<double>::lowest();
    const auto c = fincept::services::fx::convert_for_display(v, ccy, QStringLiteral("USD"));
    return c.ok() ? c.amount : std::numeric_limits<double>::lowest();
}

} // namespace fincept::ui::algo
