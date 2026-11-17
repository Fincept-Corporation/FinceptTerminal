#pragma once
// BookMath — marks, P&L and Greeks of the Derivatives position book (Qt Core +
// the tab's maths; no widgets, no I/O). Pure functions meant to run on a worker
// thread: the BOOK page snapshots the positions and the market data it has
// (delayed chains, spots, futures prices, the Treasury zero curve) into a
// MarketView and calls value_book() + summarise() off the GUI thread.
//
// How each leg is marked (the "MARKED FROM" column says which applied):
//   * option  — the chain's bid/ask mid for that expiry, strike and type when it is
//               quoted (ChainMid); else Black-Scholes-Merton at the chain's IV for that
//               strike (raw-SVI fit inside its range, else the expiry's ATM IV:
//               ModelChainIv); else at the IV recorded when the leg was booked
//               (ModelEntryIv). Greeks: BSM at that IV on the chain's rate and implied
//               carry (European approximation of American contracts). Past its expiry
//               a leg is worth its intrinsic value at today's spot (Intrinsic).
//   * stock   — the spot (Spot).
//   * future  — the contract's last price (FuturesLast); its "value" is the open P&L
//               (futures are marked to market daily), its delta the contract size.
//   * swap    — re-valued off the Treasury zero curve (Curve) with RatesMath
//               (single curve; the elapsed time since the entry date shortens the
//               swap; the fixing of a running period is not tracked). Rho = DV01 × 100.
//   * no market data for a leg — it is carried at its entry price (Entry) and the
//     position is flagged stale.
//
// Greeks are position totals in the price currency: delta in underlying units
// (shares; futures: units of the contract's underlying), dollar delta = delta ×
// spot, gamma per 1.00 of spot, dollar gamma = change of dollar delta for a 1 %
// spot move (gamma × S² / 100), vega per volatility point, theta per calendar day,
// rho per 1 % of rates.

#include "services/options/BookTypes.h"
#include "services/options/DerivMarketData.h"
#include "services/options/RatesMath.h"
#include "services/options/StrategyMath.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

namespace fincept::services::options::book {

/// Normalised key of an underlying ("^SPX" → "SPX", "^GSPC" → "SPX", upper case);
/// the same rule as FreeOptionChainService::key_of.
QString underlying_key(const QString& symbol);

struct UnderlyingQuote {
    double spot = market::kNoValue;
    double r = 0.04; ///< continuous, used when there is no chain
    double q = 0.0;  ///< continuous carry, used when there is no chain
    std::shared_ptr<const market::FreeChain> chain; ///< may be null
    QDateTime as_of;                                ///< UTC
    QString source;                                 ///< "Cboe delayed quotes" …
};

struct MarketView {
    QDate today;
    QHash<QString, UnderlyingQuote> underlyings; ///< by underlying_key()
    QHash<QString, double> futures;              ///< contract symbol (upper case) → last price
    QHash<QString, QDateTime> futures_as_of;
    rates::ZeroCurve curve; ///< for swaps; empty = swaps carried at entry
    QDate curve_date;
};

enum class MarkSource { ChainMid, ModelChainIv, ModelEntryIv, Intrinsic, Spot, FuturesLast, Curve, Entry };

struct LegResult {
    double mark = market::kNoValue; ///< per share / per unit; swaps: PV of the whole swap
    double value = 0;               ///< market value (futures: open P&L; swaps: PV)
    double pnl = 0;                 ///< since entry
    double delta = 0;
    double dollar_delta = 0;
    double gamma = 0;
    double dollar_gamma = 0;
    double vega = 0;  ///< per vol point
    double theta = 0; ///< per calendar day
    double rho = 0;   ///< per 1 % rate
    double iv = market::kNoValue;
    double years = 0; ///< to expiry (options)
    double spot = market::kNoValue;
    MarkSource source = MarkSource::Entry;
    bool expired = false;
};

struct PositionResult {
    QString id;
    QString key; ///< underlying_key(), or "RATES:<CCY>" for swaps
    LegResult total; ///< summed; mark = the single leg's mark, NaN for several legs
    QVector<LegResult> legs;
    QDate nearest_expiry; ///< earliest live option / future expiry (invalid when none)
    QDateTime as_of;      ///< oldest market time used (UTC); invalid when none
    QString source;       ///< data source label of the marks
    bool stale = false;   ///< some leg had no market data (carried at entry)
    bool all_expired = false;
};

LegResult value_leg(const BookLeg& leg, const BookPosition& p, const MarketView& m);
PositionResult value_position(const BookPosition& p, const MarketView& m);
QVector<PositionResult> value_book(const QVector<BookPosition>& positions, const MarketView& m);

/// Grouping key of a position: underlying_key(underlying), or "RATES:<CCY>" for swaps.
QString exposure_key(const BookPosition& p);

struct Exposure {
    QString key;
    QString label;
    QString currency;
    int positions = 0;
    double value = 0, pnl = 0;
    double delta = 0; ///< meaningful within one underlying only
    double dollar_delta = 0, gamma = 0, dollar_gamma = 0, vega = 0, theta = 0, rho = 0;
};

/// 0 expired, 1 ≤ 7 days, 2 8–30, 3 31–90, 4 91–180, 5 181–365, 6 > 1 year.
int expiry_bucket(int days);
constexpr int kExpiryBuckets = 7;

struct LadderRow {
    QDate expiry;
    int days = 0;
    int bucket = 0;
    int legs = 0;
    double value = 0, dollar_delta = 0, gamma = 0, dollar_gamma = 0, vega = 0, theta = 0;
};

struct BookSummary {
    Exposure total;              ///< delta left at 0 (shares of different underlyings do not add)
    QVector<Exposure> by_underlying; ///< largest |dollar delta| first
    QVector<LadderRow> ladder;   ///< by expiry date, ascending (options and futures)
    QStringList currencies;      ///< distinct currencies (totals are only meaningful for one)
    int stale = 0;               ///< positions carried (partly) at entry
};

BookSummary summarise(const QVector<BookPosition>& positions, const QVector<PositionResult>& results,
                      const QDate& today);

/// Years from `today` to an expiry date (ACT/365, never negative).
double years_to(const QDate& today, const QDate& expiry);

/// The option and stock legs of a position as StrategyMath legs (expiry_years from
/// `today`, iv = the leg's entry IV or 0.2). Futures and swaps are not strategy legs:
/// `skipped` counts them.
std::vector<strategy::Leg> strategy_legs(const BookPosition& p, const QDate& today, int* skipped = nullptr);

} // namespace fincept::services::options::book
