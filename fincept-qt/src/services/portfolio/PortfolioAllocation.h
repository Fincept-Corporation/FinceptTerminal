// src/services/portfolio/PortfolioAllocation.h
//
// Allocation of a valued book along several dimensions and concentration
// statistics. Pure (Qt Core only). Inputs are the base-currency holdings of a
// PortfolioSummary plus the `market:info:<SYM>` fundamentals (InfoData).
//
// Weights are % of the VALUED holdings' base total (cash excluded). A holding the
// summary could not value is left out (and listed in `unvalued`); a holding whose
// attribute is unknown goes to the slice with an EMPTY key ("Unknown" in the UI).
#pragma once

#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataTypes.h"

#include <QHash>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::portfolio {

enum class AllocationDimension { Sector, Industry, AssetClass, Country, Region, Currency, Exchange, MarketCap };

/// "sector", "industry", "asset_class", "country", "region", "currency", "exchange", "market_cap".
QString allocation_dimension_key(AllocationDimension d);
QVector<AllocationDimension> all_allocation_dimensions();

struct AllocationSlice {
    QString key;      ///< value of the attribute ("Technology", "GBP", "large", "north_america"); "" = unknown
    double value = 0; ///< base currency
    double weight = 0; ///< % of the valued holdings' total
    int count = 0;
    QStringList symbols;
};

struct AllocationBreakdown {
    AllocationDimension dimension = AllocationDimension::Sector;
    QString key;                    ///< allocation_dimension_key(dimension)
    QVector<AllocationSlice> slices; ///< by value, descending; the unknown slice (if any) last
    double unknown_weight = 0;      ///< % with no value for this attribute
};

struct ConcentrationStats {
    int positions = 0;              ///< valued holdings
    double hhi = kAbsent;           ///< Herfindahl Σw² of fractional weights (1/N .. 1)
    double effective_n = kAbsent;   ///< 1 / hhi
    double top1 = kAbsent;          ///< %, largest weight
    double top3 = kAbsent;          ///< %
    double top5 = kAbsent;          ///< %
    double top10 = kAbsent;         ///< %
    QString largest_symbol;
};

struct AllocationReport {
    QString portfolio_id;
    QString base_currency;
    double valued_total = 0; ///< base
    QVector<AllocationBreakdown> breakdowns; ///< one per dimension, in all_allocation_dimensions() order
    ConcentrationStats concentration;
    QStringList unvalued;     ///< holdings left out (no price or no FX)
    QStringList info_missing; ///< holdings with no market:info data yet (sector/country/... unknown)
    QStringList class_guessed; ///< asset class guessed from the ticker (no quoteType)
    bool complete = false;     ///< every holding valued and described
};

/// Market-cap buckets on the USD-equivalent cap: "mega" ≥ 200 bn, "large" ≥ 10 bn,
/// "mid" ≥ 2 bn, "small" ≥ 300 m, "micro" below; "" when unknown (also for funds).
QString market_cap_bucket_usd(double market_cap_usd);

/// `usd_per` = USD per one major unit of a currency (nullopt = unknown), used to
/// bucket market caps reported in other currencies.
AllocationReport compute_allocation(const PortfolioSummary& summary, const QHash<QString, services::InfoData>& info,
                                    const std::function<std::optional<double>(const QString& ccy)>& usd_per);

/// Concentration of a weight vector in % (any order; NaN entries ignored).
ConcentrationStats concentration_of(const QVector<QPair<QString, double>>& weights_pct);

} // namespace fincept::portfolio
