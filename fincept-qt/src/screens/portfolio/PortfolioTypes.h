// src/screens/portfolio/PortfolioTypes.h
#pragma once
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>
#include <limits>
#include <optional>

namespace fincept::portfolio {

/// "Not known" for the double fields documented as NaN-when-absent below. Test
/// with std::isfinite / std::isnan, never compare with 0 (a 0 is data).
inline constexpr double kAbsent = std::numeric_limits<double>::quiet_NaN();

// Money is formatted by ui/formatting (screens/portfolio/PortfolioFormat.h wraps it).

// ── Ledger vocabulary ────────────────────────────────────────────────────────
//
// Every portfolio_transactions.transaction_type (schema v055 widens the CHECK
// constraint to this list). Amount conventions — every money field of a row is
// in that row's `currency` (minor units allowed, e.g. GBp), see Transaction:
//
//   BUY        quantity = units bought, price = price per unit; fees/taxes extra
//   SELL       quantity = units sold,   price = price per unit; fees/taxes deducted
//   DIVIDEND   quantity = units entitled (held at the ex-date), price = gross amount
//              per unit; taxes = withholding tax, fees = charges
//   SPLIT      quantity = split FACTOR, new units per old unit (2 = 2-for-1,
//              0.1 = 1-for-10 reverse); price = 0. Cost basis is unchanged.
//   DEPOSIT    cash in:  quantity = amount, price = 1, symbol empty
//   WITHDRAWAL cash out: quantity = amount, price = 1, symbol empty
//   FEE        stand-alone charge (custody, platform): quantity = amount, price = 1;
//              symbol empty or the related holding
//   TAX        stand-alone tax payment: quantity = amount, price = 1
//   INTEREST   interest received on cash: quantity = amount, price = 1; taxes = withheld
namespace txn {
inline const QString kBuy = QStringLiteral("BUY");
inline const QString kSell = QStringLiteral("SELL");
inline const QString kDividend = QStringLiteral("DIVIDEND");
inline const QString kSplit = QStringLiteral("SPLIT");
inline const QString kDeposit = QStringLiteral("DEPOSIT");
inline const QString kWithdrawal = QStringLiteral("WITHDRAWAL");
inline const QString kFee = QStringLiteral("FEE");
inline const QString kTax = QStringLiteral("TAX");
inline const QString kInterest = QStringLiteral("INTEREST");

/// Every accepted type, in display order.
inline QStringList all_types() {
    return {kBuy, kSell, kDividend, kSplit, kDeposit, kWithdrawal, kFee, kTax, kInterest};
}
inline bool is_valid_type(const QString& t) {
    return all_types().contains(t.trimmed().toUpper());
}
/// Rows that move cash only and never a position (symbol may be empty).
inline bool is_cash_only(const QString& t) {
    const QString u = t.trimmed().toUpper();
    return u == kDeposit || u == kWithdrawal || u == kFee || u == kTax || u == kInterest;
}
} // namespace txn

// ── Core entities ────────────────────────────────────────────────────────────

struct Portfolio {
    QString id;
    QString name;
    QString owner;
    /// Base (reporting) currency, ISO major code. "USD" mirrors the portfolios.currency
    /// column default (migration v001), so a legacy row without one reads as it was stored.
    QString currency = "USD";
    QString description;
    QString created_at;
    QString updated_at;
    /// Empty for manually-created portfolios. Set when imported from a
    /// connected broker account — links back to broker_accounts.id so
    /// PortfolioService can route live quote fetches through the broker
    /// instead of yfinance. See migration v022.
    QString broker_account_id;

    // ── Analytics settings (migration v055) ──────────────────────────────────
    /// Benchmark spec. Empty = the default for `currency`
    /// (PortfolioService::default_benchmark_for_currency). One Yahoo symbol
    /// ("SPY") or a fixed-weight blend "SPY:60,AGG:40" (weights normalised,
    /// rebalanced daily). Benchmark returns are converted into `currency`.
    QString benchmark;
    /// User-set annual risk-free rate (fraction, 0.035 = 3.5 %). nullopt = automatic
    /// for the base currency (see PortfolioService::risk_free_for()).
    std::optional<double> risk_free_override;
    /// true: cash is part of the book. DEPOSIT/WITHDRAWAL are the external flows,
    /// trades and income move the per-currency cash balances, and cash counts in
    /// NAV. false (default, legacy books): every BUY is money brought in and every
    /// SELL / dividend money taken out; cash is reported but not valued.
    bool track_cash = false;
    /// Tax-lot relief method: "FIFO", "LIFO", "HIFO", "AVERAGE" or "SPECIFIC".
    QString lot_method = QStringLiteral("FIFO");
    /// Holding period (days) from which a lot counts as long-term. 365 = US rule.
    int long_term_days = 365;
};

struct PortfolioAsset {
    int id = 0;
    QString portfolio_id;
    QString symbol; // canonical: yfinance-format ("RELIANCE.NS"). Used by sparklines, replay, news, sectors.
    double quantity = 0;
    double avg_buy_price = 0; // weighted-average cost per unit, in the instrument's QUOTED units (GBp for .L)
    QString first_purchase_date;
    QString last_updated;
    QString sector; // empty = not yet resolved; filled from import JSON or SectorResolver
    /// Broker-native ticker (e.g. "RELIANCE", no exchange suffix). Empty for
    /// manually-imported assets. Combined with `exchange` to form the
    /// EXCHANGE:SYMBOL key brokers like Zerodha need for /quote calls.
    QString broker_symbol;
    /// Exchange code (e.g. "NSE", "BSE"). Empty for non-broker imports.
    QString exchange;
};

struct Transaction {
    QString id;
    QString portfolio_id;
    QString symbol;           ///< empty for cash-only rows (DEPOSIT, WITHDRAWAL, ...)
    QString transaction_type; ///< see namespace txn
    double quantity = 0;
    double price = 0;
    double total_value = 0; ///< quantity × price (gross, before fees/taxes)
    QString transaction_date;
    QString notes;
    QString created_at;

    // ── v055 ─────────────────────────────────────────────────────────────────
    double fees = 0;  ///< commissions / charges, in `currency`, >= 0
    double taxes = 0; ///< stamp duty / FTT / withholding, in `currency`, >= 0
    /// Currency of price, fees and taxes, as quoted (minor units allowed: "GBp").
    /// Empty (legacy rows) = the instrument's quoted currency; for cash-only rows
    /// empty means the portfolio's base currency.
    QString currency;
    /// Units of the portfolio's BASE currency per ONE MAJOR unit of `currency` on
    /// the trade date (1 when they are the same). 0 = not recorded: the service
    /// then uses the historical daily rate when it has one.
    double fx_rate = 0;
    /// SELL under the SPECIFIC lot method: id of the BUY row whose lot is sold
    /// first. Empty otherwise.
    QString lot_id;
};

// ── Live-enriched data ───────────────────────────────────────────────────────

/// One holding valued in the portfolio's BASE currency.
///
/// Units: `current_price`, `avg_buy_price`, `day_change` are per unit in
/// `price_currency` (as quoted — pence for "VOD.L"); `*_local` values are in the
/// MAJOR unit of `currency` (GBP); everything else money-like is in the base
/// currency (PortfolioSummary::base_currency).
///
/// Absent values: when `priced` is false the price-derived fields
/// (current_price, market_value, unrealized_*, day_change*, weight, price_pnl,
/// fx_pnl, market_value_local) are NaN. When `fx_ok` is false every base-currency
/// field derived from the price is NaN as well. Never 0, never the cost.
struct HoldingWithQuote {
    // From PortfolioAsset / ledger
    QString symbol;
    double quantity = 0;
    double avg_buy_price = 0; ///< per unit, price_currency units (cost incl. fees when the ledger explains the position)
    QString sector;

    // Live market data (see the units / absent-value notes above)
    double current_price = kAbsent;
    double market_value = kAbsent; ///< base currency
    double cost_basis = 0;         ///< base currency (trade-date FX when known, else spot — see cost_fx_historical)
    double unrealized_pnl = kAbsent;
    double unrealized_pnl_percent = kAbsent;
    double day_change = kAbsent; ///< per unit, price_currency units
    double day_change_percent = kAbsent;
    double weight = kAbsent; ///< % of the valued total (base); NaN when not valued

    // ── Instrument ───────────────────────────────────────────────────────────
    QString name;
    QString price_currency; ///< as quoted, minor units kept ("GBp")
    QString currency;       ///< ISO major code of price_currency ("GBP")
    QString quote_type;     ///< Yahoo quoteType (EQUITY, ETF, CRYPTOCURRENCY, ...); empty = unknown
    QString exchange;       ///< Yahoo exchange code; empty = unknown
    QString market_state;   ///< PRE / REGULAR / POST / CLOSED; empty = unknown
    QString as_of;          ///< session date of the price ("YYYY-MM-DD"); empty = unknown

    // ── Valuation state ──────────────────────────────────────────────────────
    bool priced = false; ///< a live quote with a positive price was received
    bool fx_ok = false;  ///< a spot rate currency → base was available (always true for the base currency)
    bool valued() const { return priced && fx_ok; }

    // ── Currency decomposition ───────────────────────────────────────────────
    double fx_rate = kAbsent;            ///< spot: base units per 1 major unit of `currency`
    double market_value_local = kAbsent; ///< quantity × price, major units of `currency`
    double cost_basis_local = 0;         ///< major units of `currency`
    double price_pnl = kAbsent;          ///< base: (market_value_local − cost_basis_local) × spot FX
    /// base: cost_basis_local × spot FX − cost_basis; 0 for a base-currency holding;
    /// NaN for a foreign one unless cost_fx_historical.
    double fx_pnl = kAbsent;
    /// true: cost_basis used the FX rate of each purchase date (from the trade row or
    /// the daily FX history). false: cost was converted at today's spot, so the P&L
    /// cannot be split into price and FX parts (fx_pnl NaN, price_pnl = unrealized).
    bool cost_fx_historical = false;
    double day_change_value = kAbsent; ///< base: quantity × day_change in major units × spot FX
    double previous_close = kAbsent;   ///< price_currency units

    // ── From the ledger (lot method of the portfolio) ────────────────────────
    double realized_pnl = 0;            ///< base, realised on this symbol (lots with known FX only)
    double dividends = 0;               ///< base, net dividends received on this symbol
    int open_lots = 0;                  ///< number of open tax lots
    double long_term_quantity = 0;      ///< units held longer than Portfolio::long_term_days
    bool ledger_explained = false;      ///< the ledger reproduces the stored position (lots/costs come from it)
};

struct PortfolioSummary {
    Portfolio portfolio;
    QVector<HoldingWithQuote> holdings;

    // Totals in the BASE currency over VALUED holdings only (see `complete`).
    double total_market_value = 0;
    double total_cost_basis = 0; ///< cost of the valued holdings
    double total_unrealized_pnl = 0;
    double total_unrealized_pnl_percent = 0;
    double total_day_change = 0;
    double total_day_change_percent = 0;
    int total_positions = 0;
    int gainers = 0; ///< valued holdings with unrealized P&L > 0
    int losers = 0;  ///< valued holdings with unrealized P&L < 0 (flat and unpriced count in neither)
    QString last_updated;

    // From the transaction ledger, replayed with the portfolio's lot method, in
    // the base currency. Realised P&L counts only sales whose cost and proceeds
    // have a known FX rate; dividends are net of withholding tax.
    double total_realized_pnl = 0;
    double total_dividends = 0;

    /// DEPRECATED — always empty now: holdings are converted to the base currency.
    /// A holding whose rate is missing is listed in `fx_missing` and left out of the totals.
    QStringList fx_unconverted;

    // ── Completeness ─────────────────────────────────────────────────────────
    QString base_currency;   ///< = portfolio.currency, upper case
    /// true when every holding is priced and converted. false: the totals cover only
    /// `valued_positions` of `total_positions`, no NAV snapshot was written, and
    /// the UI must say the figures are incomplete.
    bool complete = true;
    int valued_positions = 0;
    QStringList unpriced;   ///< symbols without a live quote
    QStringList fx_missing; ///< "SYM (CCY)": no spot rate CCY → base yet (FxRateService is fetching it)
    /// Major codes whose spot rate → base is older than the user's "FX stale after"
    /// setting. Such a rate is still used (flagged, never dropped or replaced by 1.0):
    /// the converted totals are approximate and the UI says how old the rate is.
    QStringList fx_stale;
    QDateTime fx_stale_as_of; ///< UTC market time of the oldest stale rate; invalid when none is stale
    QString as_of;         ///< latest quote session date across holdings ("YYYY-MM-DD")
    QString snapshot_date;  ///< trading date the NAV snapshot was stored under; empty when none was written

    // ── P&L decomposition (base) ─────────────────────────────────────────────
    double total_price_pnl = kAbsent; ///< NaN when no holding is valued
    double total_fx_pnl = kAbsent;    ///< NaN unless every valued holding has a finite fx_pnl
    double realized_short_term = 0;   ///< base, lots held < long_term_days
    double realized_long_term = 0;    ///< base
    double total_fees = 0;            ///< base, all fees in the ledger (known FX)
    double total_taxes = 0;           ///< base, all taxes incl. withholding (known FX)
    double total_interest = 0;        ///< base, net interest received

    // ── Cash (ledger) ────────────────────────────────────────────────────────
    QHash<QString, double> cash_balances; ///< major ISO currency → balance (local units)
    double cash_value = 0;                ///< base; NaN when a balance has no FX rate
    /// portfolio.track_cash: cash_value IS included in total_market_value.
    bool cash_in_nav = false;

    /// Ledger notes (unmatched sells, missing FX for a trade, bad rows, ...).
    QStringList ledger_warnings;
};

// ── Market analytics (scripts/portfolio_metrics.py) ─────────────────────────

/// Per-holding ex-ante risk from trailing daily returns (in the base currency).
struct HoldingRisk {
    std::optional<double> ann_vol;           // annualised volatility, fraction
    std::optional<double> beta;              // vs MarketAnalytics::benchmark
    std::optional<double> risk_contribution; // share of portfolio variance, fraction (Σ = 1)
    /// Parametric marginal VaR at MarketAnalytics::var_confidence / var_horizon_days:
    /// d(VaR)/d(weight), fraction of NAV per unit of weight.
    std::optional<double> marginal_var;
    /// Component VaR (Euler): weight × marginal VaR, fraction of NAV. Σ = parametric VaR.
    std::optional<double> component_var;
    std::optional<double> fx_vol; ///< annualised vol of the holding's currency vs base (0 for base-currency lines)
    int observations = 0;
};

/// One VaR estimate, all fractions of NAV as positive losses.
struct VarEstimate {
    double confidence = 0.95;
    int horizon_days = 1;
    std::optional<double> historical;  ///< empirical quantile of h-day returns (overlapping windows when enough data)
    std::optional<double> cvar;        ///< expected shortfall beyond the historical VaR
    std::optional<double> parametric;  ///< normal, mean-adjusted
    std::optional<double> monte_carlo; ///< multivariate-normal simulation of the holdings
    std::optional<double> monte_carlo_cvar;
    bool horizon_scaled = false; ///< historical figure scaled by √h (too few h-day windows)
};

/// One market-analytics run for a portfolio: benchmark history, risk-free rate,
/// correlation matrix and ex-ante risk at CURRENT weights. Produced by a single
/// Python call so a portfolio load costs one process, not four. Every return is
/// in the portfolio's BASE currency (prices converted with daily FX closes), so
/// currency risk is part of every figure.
struct MarketAnalytics {
    QString portfolio_id;
    QString as_of;     // last market date in the downloaded data (YYYY-MM-DD)
    QString benchmark; // spec as requested, e.g. "SPY" or "SPY:60,AGG:40"
    QString base_currency;
    QStringList benchmark_dates;
    QVector<double> benchmark_closes; ///< total-return level in the base currency (rebased, blend-aware)
    std::optional<double> risk_free_rate; // annual fraction (e.g. 0.0425)
    QString risk_free_source;             // human-readable source label
    QHash<QString, double> correlation;   // "A|B" → Pearson r of daily returns
    int correlation_window = 0;           // observations behind the matrix
    std::optional<double> ann_vol;        // ex-ante annualised volatility, fraction
    std::optional<double> var_95_1d;      // 1-day historical VaR (95%), fraction of NAV
    std::optional<double> cvar_95_1d;     // 1-day expected shortfall (95%), fraction of NAV
    std::optional<double> beta;           // ex-ante beta vs benchmark
    std::optional<double> param_var_95_1d;       // parametric (normal) 1-day VaR (95%), fraction of NAV
    std::optional<double> diversification_ratio; // Σ wᵢσᵢ / σₚ — >1 = risk cancelled by low correlation
    std::optional<double> tracking_error;        // ex-ante annualised stdev of (portfolio − benchmark), fraction
    std::optional<double> benchmark_corr;        // correlation of daily returns with the benchmark
    std::optional<double> fx_vol_contribution;   // share of ex-ante variance that disappears without FX moves (fraction)
    int observations = 0;                 // daily observations behind the ex-ante figures
    double coverage = 0;                  // share of portfolio weight that had price history
    double periods_per_year = 252;        // annualisation used (benchmark venue's trading days)
    QVector<VarEstimate> var;             // one per requested confidence, at var_horizon_days
    double var_confidence = 0.95;         // confidence behind HoldingRisk::marginal_var / component_var
    int var_horizon_days = 1;
    int mc_paths = 0;                     // Monte Carlo paths used (0 = not run)
    QHash<QString, HoldingRisk> holdings;
    QStringList missing;  // holdings with no price history
    QStringList warnings; // non-fatal notes from the run
};

/// Options for fetch_market_analytics().
struct RiskOptions {
    QVector<double> confidences = {0.95, 0.99}; ///< VaR confidence levels (0.5..0.999)
    int horizon_days = 1;                       ///< VaR horizon in trading days (1..60)
    int mc_paths = 10000;                       ///< Monte Carlo paths (0 = skip, max 200000)
    std::optional<qint64> seed;                 ///< nullopt = fresh randomness every run
};

// ── Time-weighted performance ────────────────────────────────────────────────

/// One point of the flow-adjusted (time-weighted) return series built from
/// NAV snapshots and the transaction ledger. External flows (see
/// PortfolioPerformance.h): inflows are assumed at the START of their day (trade
/// price), outflows at its END; a gap of several days between snapshots uses
/// Modified Dietz day weights. No period is silently dropped.
struct TwrPoint {
    QString date;             // snapshot date (YYYY-MM-DD)
    double nav = 0;           // market value on that date (base)
    double net_flow = 0;      // external money in (+) / out (−) since the previous point (base)
    double period_return = 0; // flow-adjusted return since the previous point (fraction)
    double cumulative = 0;    // cumulative TWR since the first point (fraction)
    bool valid = true;        // false = no capital at risk in the period (return undefined), or the base point
    bool suspect = false;     // |return| > 25 % in one period: kept, but likely an unrecorded flow or bad print
};

// ── Computed analytics ───────────────────────────────────────────────────────

/// Performance window of compute_metrics().
enum class PerfWindow { MTD, QTD, YTD, Y1, Y3, Y5, All };

/// One consistent set of definitions (also used by the Python scripts):
///   * observations = the TWR series compounded onto the benchmark venue's trading
///     days (MarketSessions), periods_per_year = that venue's trading days per year;
///   * volatility = sample stdev of those returns × √periods_per_year;
///   * Sharpe = (mean return − rf per period) / stdev × √periods_per_year (arithmetic),
///     rf per period = (1 + rf)^(1/periods_per_year) − 1;
///   * Sortino = same numerator / downside deviation vs rf over ALL periods;
///   * annualised return = (1 + total)^(365.25 / calendar days) − 1, only for windows ≥ 1 year.
/// Percent fields are percentages (5.2 = 5.2 %); ratios are plain numbers.
struct ComputedMetrics {
    std::optional<double> sharpe;             // realised, annualised (TWR series)
    std::optional<double> sortino;            // realised, annualised, downside deviation vs rf
    std::optional<double> beta;               // REALISED vs benchmark when ≥ 20 matched periods, else ex-ante
    std::optional<double> alpha;              // realised annualised Jensen's alpha vs benchmark, %
    std::optional<double> volatility;         // REALISED annualised %, else ex-ante (volatility_is_ex_ante)
    std::optional<double> max_drawdown;       // realised, % (negative), on the TWR index
    std::optional<double> var_95;             // 1-day VaR (95%) in base currency (ex-ante when risk_ex_ante)
    std::optional<double> cvar_95;            // 1-day CVaR / expected shortfall (95%) in base currency
    std::optional<double> risk_score;         // 0-100 composite
    std::optional<double> concentration_top3; // sum of top 3 weights %
    std::optional<double> annualized_return;  // realised TWR, annualised, % (windows ≥ 1 year only)
    std::optional<double> total_return;       // realised TWR over the window, %

    // Return quality (realised TWR series)
    std::optional<double> calmar;           // annualised return / |max drawdown|
    std::optional<double> current_drawdown; // %, latest TWR index vs its running peak (≤ 0)
    std::optional<double> win_rate;         // % of periods with a positive return
    std::optional<double> best_period;      // %, best single period
    std::optional<double> worst_period;     // %, worst single period

    // Benchmark-relative (periods matched to benchmark closes on the same dates, base currency)
    std::optional<double> benchmark_return;  // %, benchmark total return over the matched periods
    std::optional<double> excess_return;     // %, portfolio − benchmark total return, same periods
    std::optional<double> tracking_error;    // annualised %, stdev of active returns
    bool tracking_error_ex_ante = false;     // true = from current weights (no realised history)
    std::optional<double> information_ratio; // annualised active return / tracking error
    std::optional<double> treynor;           // annualised excess return over rf per unit beta, %
    std::optional<double> r_squared;         // share of variance explained by the benchmark (0..1)
    std::optional<double> up_capture;        // %, geometric mean return in benchmark-up periods / benchmark's
    std::optional<double> down_capture;      // %, same for benchmark-down periods (< 100 = loses less)

    // Diversification / tail
    std::optional<double> effective_n;           // 1 / Σw² — "how many equal positions" the book behaves like
    std::optional<double> diversification_ratio; // Σ wᵢσᵢ / σₚ (ex-ante)
    std::optional<double> var_95_10d;            // 10-day VaR (95%) in currency, √10 × 1-day

    // Money-weighted return over the window (XIRR of the external flows, start
    // NAV as an inflow and end NAV as an outflow).
    std::optional<double> mwr;            // annualised %, windows ≥ 1 year (else see mwr_period)
    std::optional<double> mwr_period;     // % over the window, de-annualised ((1+irr)^(days/365.25) − 1)

    // Separate realised and ex-ante figures, so a view can show either explicitly.
    std::optional<double> realised_volatility; // %, TWR series
    std::optional<double> ex_ante_volatility;  // %, current weights × trailing returns
    std::optional<double> realised_beta;
    std::optional<double> ex_ante_beta;
    bool volatility_is_ex_ante = false; // `volatility` / `beta` fell back to the ex-ante figure

    // Context, so every surface can say exactly what the numbers are.
    QString window = QStringLiteral("ALL"); // "MTD","QTD","YTD","1Y","3Y","5Y","ALL"
    QString benchmark_symbol;   // benchmark spec beta/alpha are measured against
    double risk_free_rate = 0;  // annual fraction used for Sharpe/Sortino/alpha
    QString risk_free_source;   // where risk_free_rate came from
    bool risk_free_known = true; // false: no rate for this currency and no override — 0 % used
    bool risk_ex_ante = false;  // VaR/CVaR = current weights × trailing daily returns
    int risk_observations = 0;  // daily observations behind the ex-ante figures
    int perf_observations = 0;  // observations behind Sharpe/Sortino/MDD/alpha
    int perf_excluded = 0;      // periods with no capital at risk (return undefined)
    int perf_suspect = 0;       // periods kept but flagged |r| > 25 % (likely an unrecorded flow)
    double periods_per_year = 252; // annualisation factor (calendar venue's trading days per year)
    QString calendar;           // venue id whose trading days define the observations ("NYSE", "LSE", ...)
    QString perf_start;         // first date of the TWR window
    QString perf_end;           // last date of the TWR window
    QStringList notes;          // human-readable caveats (short history, suspect periods, ...)
    QVector<TwrPoint> twr_series;
    QString portfolio_id;       // the book these metrics describe
    QString base_currency;      // currency of var_95 / cvar_95 / twr_series NAVs
};

// ── Snapshot for performance history ─────────────────────────────────────────

struct PortfolioSnapshot {
    int id = 0;
    QString portfolio_id;
    double total_value = 0;
    double total_cost_basis = 0;
    double total_pnl = 0;
    double total_pnl_percent = 0;
    QString snapshot_date;
};

// ── Enums ────────────────────────────────────────────────────────────────────

enum class HeatmapMode { Pnl, Weight, DayChange };

enum class SortColumn { Symbol, Price, Change, Pnl, PnlPct, Weight, MarketValue };

enum class SortDirection { Asc, Desc };

enum class DetailView {
    AnalyticsSectors,
    PerfRisk,
    Optimization,
    QuantStats,
    ReportsPme,
    Indices,
    RiskMgmt,
    Planning,
    Economics,
    TaxLots, ///< open lots, realised gains (short / long term)
    Income   ///< dividends / interest received, forward income, ex-dates
};

// ── Import/Export types ──────────────────────────────────────────────────────

struct PortfolioExportTransaction {
    QString date;
    QString symbol;
    QString type; // see namespace txn
    double quantity = 0;
    double price = 0;
    double total_value = 0;
    QString notes;
};

struct PortfolioExportData {
    QString format_version = "1.0";
    QString portfolio_name;
    QString owner;
    QString currency;
    QString export_date;
    QVector<PortfolioExportTransaction> transactions;
};

enum class ImportMode { New, Merge };

struct ImportResult {
    QString portfolio_id;
    QString portfolio_name;
    int transactions_replayed = 0;
    QStringList errors;  ///< user-facing (translated) problems
    QString details;     ///< raw database reasons behind `errors`, for a "details" view
};

} // namespace fincept::portfolio
