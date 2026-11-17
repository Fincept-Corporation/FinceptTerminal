// src/services/portfolio/PortfolioValuation.h
//
// Values a book in its base currency from stored positions, live quotes, spot FX
// and the replayed ledger. Pure (Qt Core only).
//
// Rules
//   * A holding's currency is its quote's own `currency` when the quote carries
//     one, else the currency implied by the symbol (core/market/InstrumentCurrency),
//     else the base currency. Minor units (GBp, ZAc, ILA) are converted to the major
//     unit before any FX.
//   * Totals are sums of CONVERTED values only. A holding without a quote
//     (`priced` false) or without a spot rate (`fx_ok` false) is left out of every
//     total, its price-derived fields are NaN, and the summary is `complete = false`.
//     Nothing is ever valued at cost or at zero in its place.
//   * Cost basis: when the ledger reproduces the stored quantity (and its lots are in
//     the holding's currency) the open tax lots of the portfolio's lot method are
//     used — fees included, base cost at each purchase date's FX when known. Otherwise
//     the stored weighted-average price is used and converted at spot (no FX split).
//   * Weights are % of the valued holdings' base total (cash excluded).
#pragma once

#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataTypes.h"
#include "services/portfolio/PortfolioLots.h"

#include <QHash>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::portfolio {

struct ValuationInputs {
    Portfolio portfolio;
    QVector<PortfolioAsset> assets;
    QHash<QString, services::QuoteData> quotes; ///< keyed by PortfolioAsset::symbol
    const LedgerResult* ledger = nullptr;       ///< replayed with the portfolio's lot method (optional)
    /// Spot: base units per ONE major unit of `ccy_major`; nullopt = no rate yet.
    std::function<std::optional<double>(const QString& ccy_major)> spot_fx;
    /// Sector fallback for assets whose stored sector is empty (optional).
    std::function<QString(const QString& symbol)> sector_for;
};

PortfolioSummary value_portfolio(const ValuationInputs& in);

/// A portfolio's base (reporting) currency as a major code, from its stored currency. A
/// portfolio stored without one (portfolios.currency defaults to 'USD', so only an explicit
/// empty write) is valued in the user's display currency; that is logged once per portfolio.
/// Thread-safe.
QString base_currency_of(const QString& currency, const QString& portfolio_id);
inline QString base_currency_of(const Portfolio& p) {
    return base_currency_of(p.currency, p.id);
}

/// Every major currency `in` needs a spot rate for (holdings and cash), base excluded.
QStringList valuation_currencies(const ValuationInputs& in);

} // namespace fincept::portfolio
