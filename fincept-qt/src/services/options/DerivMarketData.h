#pragma once
// DerivMarketData — the Derivatives tab's free market data as plain values, the
// parsers of scripts/derivatives/deriv_market.py replies, and the chain analysis
// (time to expiry, implied forward, local IV and Greeks from the mid price, smile
// statistics, SVI fits, arbitrage flags, open-interest figures, surface grid).
//
// Qt Core only, no widgets, no network: parse_* and analyse_chain are pure
// functions meant to run on a worker thread (a 30,000-contract SPX chain takes tens
// of milliseconds). FreeOptionChainService / CurveDataService call them; the unit
// tests feed them the recorded fixtures in tests/data/derivatives/.
//
// Method notes (shown in the UI):
//   * time to expiry: from the feed's quote time to 16:00 New York time on the
//     expiry date, ACT/365;
//   * forward: implied from put-call parity (median of the five strikes nearest
//     spot with both a call and a put quoted), else spot grown at the risk-free rate;
//   * implied volatility and Greeks: Black-Scholes-Merton on that forward from the
//     bid/ask mid, computed locally for every quoted contract (European
//     approximation of American contracts; the smile uses out-of-the-money options,
//     where early exercise is worth next to nothing);
//   * smile: out-of-the-money quotes (puts below the forward, calls above) with a
//     mid of at least 0.05 and a bid/ask spread no wider than the mid.

#include "services/options/RatesMath.h"
#include "services/options/VolSurface.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services::options::market {

constexpr double kNoValue = std::numeric_limits<double>::quiet_NaN();

struct FreeOptionQuote {
    int expiry = 0; ///< index into FreeChain::expiries
    double strike = 0;
    bool is_call = true;
    double bid = kNoValue; ///< NaN = no bid
    double ask = kNoValue;
    double last = kNoValue;
    qint64 volume = 0;
    qint64 open_interest = 0;
    double feed_iv = kNoValue; ///< the source's own IV (decimal), when it gives one
    // Computed by analyse_chain():
    double mid = kNoValue;
    double iv = kNoValue;    ///< local IV from the mid (decimal)
    double delta = kNoValue; ///< per share
    double gamma = kNoValue;
    double theta = kNoValue; ///< per calendar day
    double vega = kNoValue;  ///< per vol point
    bool quoted() const { return bid > 0 && ask > 0 && ask >= bid; }
};

struct FreeExpiry {
    QDate date;
    double years = 0; ///< time to expiry from the quote time
    int days = 0;     ///< calendar days from the quote date
    int begin = 0;    ///< quotes [begin, end) belong to this expiry (sorted by strike, put before call)
    int end = 0;
    double rate = 0;          ///< continuous risk-free rate used
    double forward = kNoValue;
    double carry_q = 0;       ///< implied dividend / borrow yield r − ln(F/S)/T
    bool forward_from_parity = false;
    double atm_strike = 0;
    vol::SmileStats smile;
    double straddle = kNoValue; ///< ATM straddle mid (the market's expected move)
    vol::SviFit svi;
    double k_lo = 0, k_hi = 0;  ///< log-moneyness range of the smile points
    vol::ArbitrageCheck butterfly;
    vol::ArbitrageCheck calendar; ///< against the previous expiry with a fit
    bool calendar_checked = false;
    double forward_vol = kNoValue; ///< ATM forward vol from the previous expiry (−1: calendar arbitrage)
    double call_oi = 0, put_oi = 0, call_volume = 0, put_volume = 0;
    double max_pain = kNoValue;
    int smile_points = 0;
};

struct FreeChain {
    QString status = QStringLiteral("ok"); ///< ok | no_options | feed_unavailable | bad_symbol | bad_request | parse_error
    QString message;
    QString symbol;
    QString source;       ///< cboe | yahoo
    QString source_label; ///< "Cboe delayed quotes"
    bool delayed = true;
    QDateTime fetched_at;  ///< UTC
    QDateTime quote_time;  ///< UTC; invalid when the feed gave none
    double spot = kNoValue, bid = kNoValue, ask = kNoValue, prev_close = kNoValue;
    double change = kNoValue, change_pct = kNoValue, iv30_pct = kNoValue;
    QString currency = QStringLiteral("USD");
    QString market_state;
    QVector<FreeExpiry> expiries;
    QVector<FreeOptionQuote> quotes;
    QStringList notes;
    QString fallback_reason;
    int partial_loaded = 0, partial_total = 0;
    bool analysed = false;
    // Surface grid (analyse_chain): rows = moneyness K/F, columns = expiries; NaN = no data.
    QVector<double> surface_moneyness;
    QVector<QVector<double>> surface_iv;

    bool ok() const { return status == QLatin1String("ok"); }
    /// The quotes of expiry `e`.
    int count(int e) const { return (e >= 0 && e < expiries.size()) ? expiries[e].end - expiries[e].begin : 0; }
};

/// Parse a `deriv_market.py chain` reply. Never throws; a reply with "ok": false keeps
/// its status / message, malformed JSON gives status "parse_error".
FreeChain parse_free_chain(const QByteArray& json);

/// Everything derived from the quotes (see the method notes above). `rates` may be
/// empty: a flat `fallback_rate` (continuous decimal) is used then.
void analyse_chain(FreeChain& chain, const rates::ZeroCurve& rates, double fallback_rate);

/// Moneyness rows of the surface grid: 0.80 … 1.20 in steps of 0.025.
QVector<double> surface_rows();

/// Smile of one expiry (out-of-the-money quotes that pass the filters), by k.
QVector<vol::SmileQuote> smile_quotes(const FreeChain& chain, int expiry);

/// Mid and IV of a contract (false when not listed / not quoted).
bool find_quote(const FreeChain& chain, int expiry, double strike, bool is_call, double* mid, double* iv);

/// Strikes listed for an expiry, ascending, unique.
QVector<double> strikes(const FreeChain& chain, int expiry);

struct TreasuryCurve {
    QString status = QStringLiteral("ok");
    QString message;
    QDate date;
    QDate prev_date;
    QString source_label;
    QDateTime fetched_at;
    QVector<double> tenors;    ///< years
    QStringList labels;        ///< "1 Mo", "10 Yr" ...
    QVector<double> yields;    ///< percent (par, bond-equivalent)
    QVector<double> prev_yields; ///< NaN when absent
    bool ok() const { return status == QLatin1String("ok") && tenors.size() >= 2; }
};

TreasuryCurve parse_treasury_curve(const QByteArray& json);
/// Bootstrapped zero curve of a Treasury par curve.
rates::ZeroCurve zero_curve(const TreasuryCurve& c);

struct FuturesContract {
    QString symbol;
    QDate month; ///< first day of the contract month
    double last = kNoValue;
    double prev = kNoValue;
    qint64 volume = 0;
    QDate date;
};

struct FuturesCurve {
    QString status = QStringLiteral("ok");
    QString message;
    QString root, name, unit, source_label;
    QDateTime fetched_at;
    double continuous = kNoValue; ///< front continuous contract (ROOT=F)
    QVector<FuturesContract> contracts;
    QStringList missing;
    bool ok() const { return status == QLatin1String("ok") && !contracts.isEmpty(); }
};

FuturesCurve parse_futures_curve(const QByteArray& json);

} // namespace fincept::services::options::market
