// src/screens/portfolio/PortfolioTypes.h
#pragma once
#include <QDateTime>
#include <QString>
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
};

struct PortfolioAsset {
    int     id = 0;
    QString portfolio_id;
    QString symbol;
    double  quantity       = 0;
    double  avg_buy_price  = 0;
    QString first_purchase_date;
    QString last_updated;
};

struct Transaction {
    QString id;
    QString portfolio_id;
    QString symbol;
    QString transaction_type; // BUY, SELL, DIVIDEND, SPLIT
    double  quantity    = 0;
    double  price      = 0;
    double  total_value = 0;
    QString transaction_date;
    QString notes;
    QString created_at;
};

// ── Live-enriched data ───────────────────────────────────────────────────────

struct HoldingWithQuote {
    // From PortfolioAsset
    QString symbol;
    double  quantity      = 0;
    double  avg_buy_price = 0;

    // Live market data
    double current_price          = 0;
    double market_value           = 0;
    double cost_basis             = 0;
    double unrealized_pnl         = 0;
    double unrealized_pnl_percent = 0;
    double day_change             = 0;
    double day_change_percent     = 0;
    double weight                 = 0; // % of total portfolio
};

struct PortfolioSummary {
    Portfolio                  portfolio;
    QVector<HoldingWithQuote>  holdings;

    double total_market_value          = 0;
    double total_cost_basis            = 0;
    double total_unrealized_pnl        = 0;
    double total_unrealized_pnl_percent = 0;
    double total_day_change            = 0;
    double total_day_change_percent    = 0;
    int    total_positions             = 0;
    int    gainers                     = 0;
    int    losers                      = 0;
    QString last_updated;
};

// ── Computed analytics ───────────────────────────────────────────────────────

struct ComputedMetrics {
    std::optional<double> sharpe;
    std::optional<double> beta;
    std::optional<double> volatility;       // annualized %
    std::optional<double> max_drawdown;     // %
    std::optional<double> var_95;           // 1-day VaR in currency
    std::optional<double> risk_score;       // 0-100 composite
    std::optional<double> concentration_top3; // sum of top 3 weights %
};

// ── Snapshot for performance history ─────────────────────────────────────────

struct PortfolioSnapshot {
    int     id = 0;
    QString portfolio_id;
    double  total_value       = 0;
    double  total_cost_basis  = 0;
    double  total_pnl         = 0;
    double  total_pnl_percent = 0;
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
    double  quantity    = 0;
    double  price      = 0;
    double  total_value = 0;
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
    QString     portfolio_id;
    QString     portfolio_name;
    int         transactions_replayed = 0;
    QStringList errors;
};

} // namespace fincept::portfolio
