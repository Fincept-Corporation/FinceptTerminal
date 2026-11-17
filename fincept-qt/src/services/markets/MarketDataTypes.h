#pragma once
// Market data value types — Qt Core only, so pure code (reply parsing,
// performance maths, unit tests) can use them without MarketDataService.
// MarketDataService.h includes this header; existing includes keep working.

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services {

/// "Not supplied by the source" for the optional numeric fields below. Test with
/// std::isnan / std::isfinite — never compare with 0: a 0 is data (0 % yield).
inline constexpr double kMarketDataAbsent = std::numeric_limits<double>::quiet_NaN();

/// One quote, as published on `market:quote:<SYM>` and returned by fetch_quotes().
///
/// The first eight fields are the original ones and keep their historic
/// convention: 0 when unknown. Every field after them is optional: NaN
/// (kMarketDataAbsent), an empty string, 0 for a qint64 time, or -1 for an int
/// means the source did not supply it. Brace-initialised so positional
/// initialisers of the first eight fields stay valid under
/// -Wmissing-field-initializers.
struct QuoteData {
    QString symbol;
    QString name;
    double price = 0;
    double change = 0;
    double change_pct = 0;
    double high = 0;
    double low = 0;
    double volume = 0;

    // ── "As of" ──────────────────────────────────────────────────────────────
    /// Trading session the price belongs to, "YYYY-MM-DD" in the exchange's own
    /// calendar (the last daily bar). Lets a consumer say "as of Nov 13".
    QString market_date{};
    /// Last regular-session trade time, epoch seconds (Yahoo regularMarketTime). 0 = unknown.
    qint64 market_time = 0;
    /// Session state: "PRE", "REGULAR", "POST" or "CLOSED" (Yahoo's PREPRE and
    /// POSTPOST are reported as CLOSED). Empty = unknown.
    QString market_state{};

    // ── Instrument ───────────────────────────────────────────────────────────
    /// Price currency exactly as reported, minor units included: "USD", "GBp"
    /// (pence), "ZAc" (cents), "ILA" (agorot). For CURRENCY it is the quote
    /// currency of the pair, for INDEX the index's nominal currency. Empty = unknown.
    QString currency{};
    /// Decimals to display prices with (Yahoo's priceHint; at least four
    /// significant digits below 1). -1 = unknown.
    int price_digits = -1;
    /// Yahoo quoteType: EQUITY, ETF, INDEX, CURRENCY, CRYPTOCURRENCY, FUTURE,
    /// MUTUALFUND, ... (upper case). Empty = unknown.
    QString quote_type{};
    /// Yahoo exchange code ("NMS", "LSE", "JPX"). Empty = unknown.
    QString exchange{};
    /// Exchange display name ("NasdaqGS", "LSE", "Tokyo"). Empty = unknown.
    QString exchange_name{};
    /// IANA time zone of the exchange ("America/New_York"); feed it to QTimeZone. Empty = unknown.
    QString exchange_timezone{};
    /// How many minutes the source delays this exchange's quotes (0 = real time). -1 = unknown.
    int exchange_delay_min = -1;

    // ── Session prices ───────────────────────────────────────────────────────
    double open = kMarketDataAbsent;
    /// The close `change` / `change_pct` are measured from.
    double previous_close = kMarketDataAbsent;
    /// Extended-hours prices. Yahoo keeps reporting the last pre/post print after
    /// that session ends, so show them by market_state (PRE -> pre, POST/CLOSED
    /// -> post) and/or by their *_time.
    double pre_market_price = kMarketDataAbsent;
    double pre_market_change_pct = kMarketDataAbsent; ///< percent vs the regular close
    qint64 pre_market_time = 0;                       ///< epoch seconds, 0 = unknown
    double post_market_price = kMarketDataAbsent;
    double post_market_change_pct = kMarketDataAbsent;
    qint64 post_market_time = 0;

    // ── Context ──────────────────────────────────────────────────────────────
    /// Average daily volume over 3 months / 10 days. Relative volume = volume / average_volume.
    double average_volume = kMarketDataAbsent;
    double average_volume_10d = kMarketDataAbsent;
    double fifty_two_week_high = kMarketDataAbsent;
    double fifty_two_week_low = kMarketDataAbsent;
    double fifty_day_average = kMarketDataAbsent;
    double two_hundred_day_average = kMarketDataAbsent;

    // ── Provenance ───────────────────────────────────────────────────────────
    /// When Yahoo actually answered for this quote, epoch seconds (the reply's
    /// "timestamp"). A quote the Python layer served from its cache keeps its
    /// original fetch time, so this can be well before the hub published it —
    /// use MarketDataService::quote_fetched_ms() for the quote's real age.
    /// 0 = unknown (older script / cache row).
    qint64 fetched_at = 0;
};

/// Company / instrument fundamentals.
///
/// Two producers fill it:
///   * `market:info:<SYM>` (MarketDataService, batched): EVERY numeric field is
///     NaN (kMarketDataAbsent) and every string empty when Yahoo did not report
///     it — including the original fields below.
///   * fetch_info() (legacy, one symbol): the original numeric fields keep their
///     historic 0-when-unknown and `currency` defaults to "USD"; the fields
///     added after them are not filled (NaN / empty / invalid date).
/// Ratios are fractions (0.25 = 25 %) unless noted.
struct InfoData {
    QString symbol;
    QString name;
    QString sector;
    QString industry;
    QString country;
    QString currency; ///< trading currency as reported (minor units included: "GBp")
    double market_cap = 0;
    double pe_ratio = 0; ///< trailing
    double forward_pe = 0;
    double price_to_book = 0;
    double dividend_yield = 0; ///< fraction (0.003 = 0.3 %)
    double beta = 0;
    double week52_high = 0;
    double week52_low = 0;
    double avg_volume = 0; ///< 3-month average daily volume
    /// DEPRECATED name: holds revenue per share on both paths (historic). Use
    /// trailing_eps / forward_eps / revenue_per_share.
    double eps = 0;
    double roe = 0;
    double profit_margin = 0;
    double debt_to_equity = 0; ///< as Yahoo reports it: percent (150 = 1.5x)
    double current_ratio = 0;

    // ── Added fields: absent = NaN / empty / invalid QDate on every path ────
    QString quote_type{};         ///< EQUITY, ETF, INDEX, CURRENCY, CRYPTOCURRENCY, FUTURE, MUTUALFUND, ...
    QString financial_currency{}; ///< currency of the financial statements (may differ: ASML USD/EUR)
    QString exchange{};           ///< Yahoo exchange code
    QString exchange_name{};      ///< exchange display name
    /// Analysts' consensus: "strong_buy", "buy", "hold", "underperform", "sell". Empty = no coverage.
    QString recommendation_key{};
    /// Most recent or next ex-dividend date (exchange calendar). Invalid = none known.
    QDate ex_dividend_date{};
    /// Next scheduled earnings report (start of Yahoo's window; a report earlier
    /// today counts). Invalid = none scheduled / unknown.
    QDate earnings_date{};
    double trailing_eps = kMarketDataAbsent;
    double forward_eps = kMarketDataAbsent;
    double revenue_per_share = kMarketDataAbsent;
    double price_to_sales = kMarketDataAbsent;
    double peg_ratio = kMarketDataAbsent;
    double enterprise_value = kMarketDataAbsent;
    double ev_to_ebitda = kMarketDataAbsent;
    /// Annual dividend per share as Yahoo reports it. For minor-unit listings
    /// (GBp) Yahoo gives it in the MAJOR unit (VOD.L: 0.04 GBP vs a price in pence).
    double dividend_rate = kMarketDataAbsent;
    double payout_ratio = kMarketDataAbsent;
    double average_volume_10d = kMarketDataAbsent;
    double shares_outstanding = kMarketDataAbsent;
    double float_shares = kMarketDataAbsent;
    double short_ratio = kMarketDataAbsent;         ///< days to cover
    double short_percent_float = kMarketDataAbsent; ///< fraction
    double gross_margin = kMarketDataAbsent;
    double operating_margin = kMarketDataAbsent;
    double revenue_growth = kMarketDataAbsent;  ///< YoY, latest quarter
    double earnings_growth = kMarketDataAbsent; ///< YoY, latest quarter
    double target_mean_price = kMarketDataAbsent;
    double recommendation_mean = kMarketDataAbsent; ///< 1 = strong buy ... 5 = sell
};

struct HistoryPoint {
    qint64 timestamp = 0; // seconds since epoch
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    qint64 volume = 0;
};

struct TickerDef {
    QString symbol;
    QString name;
};

struct MarketCategory {
    QString category;
    QStringList tickers;
};

struct RegionalMarket {
    QString region;
    QVector<TickerDef> tickers;
};

} // namespace fincept::services
