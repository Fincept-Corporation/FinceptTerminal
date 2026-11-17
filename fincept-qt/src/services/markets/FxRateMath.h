#pragma once
// FxRateMath — the pure half of FxRateService: which Yahoo symbol carries a
// currency's USD leg, how legs (or a direct cross) combine into a rate, and how
// old a rate may be. Qt Core only, no DataHub: the service feeds it hub samples,
// the unit test (tests/tst_fx_rate_math.cpp) feeds it injected ones.
//
// Legs: every currency is held against USD. Fiat uses Yahoo's FX pairs in
// market convention (EURUSD=X, GBPUSD=X, AUDUSD=X, NZDUSD=X; USDxxx=X for the
// rest); crypto and stablecoins use their USD instrument ("BTC-USD",
// "USDT-USD") — Yahoo has no "USDBTC=X". Minor units (GBp, ZAc, ILA, USX, KWF)
// are scaled.
//
// Freshness: a leg whose last hub publish is older than `stale_after` is still
// used but the quote is flagged stale; beyond the hard cap (7 days) it is
// dropped. "as of" is the rate's own market time when Yahoo reports one, else
// the time the hub received it. A stablecoin without a rate is taken 1:1 to USD
// and flagged approximate (decision: stablecoins are their own currencies).
// Any other missing leg gives no rate — never 1.0.

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace fincept::services {

struct FxQuote {
    double rate = 0;           ///< units of `to` per one unit of `from` (minor units applied)
    QDateTime as_of;           ///< UTC time of the rate: the oldest leg's market time, else its receipt time
    bool triangulated = false; ///< computed through USD from two legs
    bool stale = false;        ///< the oldest leg reached the hub longer ago than the stale threshold
    qint64 received_ms = 0;    ///< when the oldest leg reached the hub (epoch ms); 0 = no leg was needed
    bool approximate = false;  ///< a stablecoin had no rate and was taken 1:1 to USD
    QString approx_reason;     ///< why it is approximate (tooltip text)
};

namespace fxmath {

/// Beyond this a leg is dropped (offline for a week: no rate rather than a wrong one).
constexpr qint64 kFxHardMaxAgeSec = 7 * 24 * 3600;

/// One Yahoo quote as the hub holds it.
struct FxLegSample {
    double price = 0;         ///< last price of the Yahoo symbol
    qint64 received_ms = 0;   ///< hub publish time (epoch ms)
    qint64 market_time_s = 0; ///< QuoteData.market_time (epoch s); 0 = unknown
};
/// Hub lookup by Yahoo symbol; nullopt when the hub has nothing for it.
using FxLegLookup = std::function<std::optional<FxLegSample>(const QString& yahoo_symbol)>;

/// A convertible major code: three capital letters (fiat; not the precious
/// metals, which Yahoo has no spot pair for) or a crypto / stablecoin code of
/// the currency table (BTC, USDT, DOGE, FDUSD).
bool fx_valid_code(const QString& major_code);
/// Crypto and stablecoins: their leg is "<CODE>-USD".
bool fx_is_crypto_leg(const QString& major_code);
/// Yahoo symbol of a currency's USD leg: "EURUSD=X", "USDJPY=X", "GBPUSD=X" for
/// GBp, "BTC-USD", "USDT-USD"; empty for USD and invalid codes.
QString fx_leg_symbol(const QString& ccy);
/// USD per one unit of `major_code` from its leg's price.
double fx_usd_per_unit(const QString& major_code, double leg_price);

struct FxCombine {
    std::optional<FxQuote> quote;
    QStringList missing; ///< major codes whose USD leg is absent or too old (subscribe them)
};
/// Rate from `from_ccy` to `to_ccy` (codes as produced, minor units allowed).
/// A fresh direct cross (EURGBP=X / GBPEUR=X) is preferred over two legs.
FxCombine fx_combine(const QString& from_ccy, const QString& to_ccy, const FxLegLookup& lookup, qint64 now_ms,
                     qint64 stale_after_s, qint64 hard_max_s = kFxHardMaxAgeSec);

} // namespace fxmath
} // namespace fincept::services
