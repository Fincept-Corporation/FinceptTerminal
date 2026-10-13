// src/screens/portfolio/PortfolioTypes.h
#pragma once
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::portfolio {

// ── Core entities ────────────────────────────────────────────────────────────

struct Portfolio {
    QString id;
    QString name;
    QString owner;
    QString currency = "USD";
    QString description;
    QString created_at;
    QString updated_at;
    /// Empty for manually-created portfolios. Set when imported from a
    /// connected broker account — links back to broker_accounts.id so
    /// PortfolioService can route live quote fetches through the broker
    /// instead of yfinance. See migration v022.
    QString broker_account_id;
};

struct PortfolioAsset {
    int id = 0;
    QString portfolio_id;
    QString symbol; // canonical: yfinance-format ("RELIANCE.NS"). Used by sparklines, replay, news, sectors.
    double quantity = 0;
    double avg_buy_price = 0;
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
    QString symbol;
    QString transaction_type; // BUY, SELL, DIVIDEND, SPLIT
    double quantity = 0;
    double price = 0;
    double total_value = 0;
    QString transaction_date;
    QString notes;
    QString created_at;
};

// ── Live-enriched data ───────────────────────────────────────────────────────

struct HoldingWithQuote {
    // From PortfolioAsset
    QString symbol;
    double quantity = 0;
    double avg_buy_price = 0;
    QString sector;

    // Live market data
    double current_price = 0;
    double market_value = 0;
    double cost_basis = 0;
    double unrealized_pnl = 0;
    double unrealized_pnl_percent = 0;
    double day_change = 0;
    double day_change_percent = 0;
    double weight = 0; // % of total portfolio
};

struct PortfolioSummary {
    Portfolio portfolio;
    QVector<HoldingWithQuote> holdings;

    double total_market_value = 0;
    double total_cost_basis = 0;
    double total_unrealized_pnl = 0;
    double total_unrealized_pnl_percent = 0;
    double total_day_change = 0;
    double total_day_change_percent = 0;
    int total_positions = 0;
    int gainers = 0;
    int losers = 0;
    QString last_updated;

    // From the transaction ledger (weighted-average cost method), in the
    // portfolio currency as entered. Realised P&L = Σ (sell price − average cost
    // at the time of the sale) × quantity sold. Dividends are cash received and
    // are NOT part of the market value (the book does not model a cash line).
    double total_realized_pnl = 0;
    double total_dividends = 0;

    /// Holdings whose exchange suffix implies a trading currency other than the
    /// portfolio currency, e.g. "RELIANCE.NS (INR)" or "VOD.L (GBp)". Their
    /// prices are summed into the totals WITHOUT FX conversion, so the UI must
    /// say so instead of presenting the total as clean.
    QStringList fx_unconverted;
};

// ── Market analytics (scripts/portfolio_metrics.py) ─────────────────────────

/// Per-holding ex-ante risk from trailing daily returns.
struct HoldingRisk {
    std::optional<double> ann_vol;           // annualised volatility, fraction
    std::optional<double> beta;              // vs MarketAnalytics::benchmark
    std::optional<double> risk_contribution; // share of portfolio variance, fraction (Σ = 1)
    int observations = 0;
};

/// One market-analytics run for a portfolio: benchmark history, risk-free rate,
/// correlation matrix and ex-ante risk at CURRENT weights. Produced by a single
/// Python call so a portfolio load costs one process, not four.
struct MarketAnalytics {
    QString portfolio_id;
    QString as_of;     // last market date in the downloaded data (YYYY-MM-DD)
    QString benchmark; // e.g. "SPY", "^NSEI"
    QStringList benchmark_dates;
    QVector<double> benchmark_closes;
    std::optional<double> risk_free_rate; // annual fraction (e.g. 0.0425)
    QString risk_free_source;             // human-readable source label
    QHash<QString, double> correlation;   // "A|B" → Pearson r of daily returns
    int correlation_window = 0;           // observations behind the matrix
    std::optional<double> ann_vol;        // ex-ante annualised volatility, fraction
    std::optional<double> var_95_1d;      // 1-day historical VaR (95%), fraction of NAV
    std::optional<double> cvar_95_1d;     // 1-day expected shortfall (95%), fraction of NAV
    std::optional<double> beta;           // ex-ante beta vs benchmark
    int observations = 0;                 // daily observations behind the ex-ante figures
    double coverage = 0;                  // share of portfolio weight that had price history
    QHash<QString, HoldingRisk> holdings;
    QStringList missing;  // holdings with no price history
    QStringList warnings; // non-fatal notes from the run
};

// ── Time-weighted performance ────────────────────────────────────────────────

/// One point of the flow-adjusted (time-weighted) return series built from
/// NAV snapshots and the transaction ledger. A BUY is capital coming in and a
/// SELL is capital going out — neither is performance.
struct TwrPoint {
    QString date;             // snapshot date (YYYY-MM-DD)
    double nav = 0;           // market value on that date
    double net_flow = 0;      // BUY cost in minus SELL proceeds out since the previous point
    double period_return = 0; // flow-adjusted return since the previous point (fraction)
    double cumulative = 0;    // cumulative TWR since the first point (fraction)
    bool valid = true;        // false = excluded (no base value / implausible jump)
};

// ── Computed analytics ───────────────────────────────────────────────────────

struct ComputedMetrics {
    std::optional<double> sharpe;             // realised, annualised (TWR series)
    std::optional<double> sortino;            // realised, annualised, downside deviation vs rf
    std::optional<double> beta;               // ex-ante when risk_ex_ante, else realised
    std::optional<double> alpha;              // realised annualised Jensen's alpha vs benchmark, %
    std::optional<double> volatility;         // annualised %, ex-ante when risk_ex_ante
    std::optional<double> max_drawdown;       // realised, % (negative), on the TWR index
    std::optional<double> var_95;             // 1-day VaR (95%) in portfolio currency
    std::optional<double> cvar_95;            // 1-day CVaR / expected shortfall (95%) in currency
    std::optional<double> risk_score;         // 0-100 composite
    std::optional<double> concentration_top3; // sum of top 3 weights %
    std::optional<double> annualized_return;  // realised TWR, annualised, %
    std::optional<double> total_return;       // realised TWR over the window, %

    // Context, so every surface can say exactly what the numbers are.
    QString benchmark_symbol;   // index beta/alpha are measured against
    double risk_free_rate = 0;  // annual fraction used for Sharpe/Sortino/alpha
    QString risk_free_source;   // where risk_free_rate came from
    bool risk_ex_ante = false;  // vol/beta/VaR = current weights × trailing daily returns
    int risk_observations = 0;  // daily observations behind the ex-ante figures
    int perf_observations = 0;  // TWR periods behind Sharpe/Sortino/MDD/alpha
    int perf_excluded = 0;      // TWR periods dropped as implausible (unrecorded flows)
    QString perf_start;         // first date of the TWR window
    QString perf_end;           // last date of the TWR window
    QVector<TwrPoint> twr_series;
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
    Economics
};

// ── Import/Export types ──────────────────────────────────────────────────────

struct PortfolioExportTransaction {
    QString date;
    QString symbol;
    QString type; // BUY, SELL, DIVIDEND, SPLIT
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
    QStringList errors;
};

} // namespace fincept::portfolio
