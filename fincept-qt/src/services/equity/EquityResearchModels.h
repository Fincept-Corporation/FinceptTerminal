// src/services/equity/EquityResearchModels.h
#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::equity {

// ── Symbol search ─────────────────────────────────────────────────────────────
struct SearchResult {
    QString symbol;
    QString name;
    QString exchange;
    QString type;
    QString currency;
    QString industry;
};

// ── Real-time quote ───────────────────────────────────────────────────────────
struct QuoteData {
    QString symbol;
    double  price        = 0.0;
    double  change       = 0.0;
    double  change_pct   = 0.0;
    double  open         = 0.0;
    double  high         = 0.0;
    double  low          = 0.0;
    double  prev_close   = 0.0;
    double  volume       = 0.0;
    QString exchange;
    qint64  timestamp    = 0;
};

// ── Company fundamentals ──────────────────────────────────────────────────────
struct StockInfo {
    QString symbol;
    QString company_name;
    QString sector;
    QString industry;
    QString description;
    QString website;
    QString country;
    QString currency;
    QString exchange;
    int     employees            = 0;

    // Valuation
    double  market_cap           = 0.0;
    double  enterprise_value     = 0.0;
    double  pe_ratio             = 0.0;
    double  forward_pe           = 0.0;
    double  peg_ratio            = 0.0;
    double  price_to_book        = 0.0;
    double  ev_to_revenue        = 0.0;
    double  ev_to_ebitda         = 0.0;

    // Profitability
    double  gross_margins        = 0.0;
    double  operating_margins    = 0.0;
    double  ebitda_margins       = 0.0;
    double  profit_margins       = 0.0;
    double  roe                  = 0.0;
    double  roa                  = 0.0;
    double  gross_profits        = 0.0;

    // Per share / cash
    double  book_value           = 0.0;
    double  revenue_per_share    = 0.0;
    double  free_cashflow        = 0.0;
    double  operating_cashflow   = 0.0;
    double  total_cash           = 0.0;
    double  total_debt           = 0.0;
    double  total_revenue        = 0.0;

    // Growth
    double  earnings_growth      = 0.0;
    double  revenue_growth       = 0.0;

    // Share data
    double  shares_outstanding       = 0.0;
    double  float_shares             = 0.0;
    double  held_insiders_pct        = 0.0;
    double  held_institutions_pct    = 0.0;
    double  short_ratio              = 0.0;
    double  short_pct_of_float       = 0.0;

    // Price range / risk
    double  week52_high          = 0.0;
    double  week52_low           = 0.0;
    double  avg_volume           = 0.0;
    double  beta                 = 0.0;
    double  dividend_yield       = 0.0;
    double  current_price        = 0.0;

    // Analyst targets
    double  target_high          = 0.0;
    double  target_low           = 0.0;
    double  target_mean          = 0.0;
    double  recommendation_mean  = 0.0;
    QString recommendation_key;
    int     analyst_count        = 0;
};

// ── Historical OHLCV candle ───────────────────────────────────────────────────
struct Candle {
    qint64  timestamp = 0;
    double  open      = 0.0;
    double  high      = 0.0;
    double  low       = 0.0;
    double  close     = 0.0;
    qint64  volume    = 0;
};

// ── Financial statements ──────────────────────────────────────────────────────
// period → line_item → value; stored as raw JSON since yfinance returns
// hundreds of heterogeneous line-item names that vary by company.
struct FinancialsData {
    QString symbol;
    // Each entry: (period_string, QJsonObject of line items)
    QVector<QPair<QString, QJsonObject>> income_statement;
    QVector<QPair<QString, QJsonObject>> balance_sheet;
    QVector<QPair<QString, QJsonObject>> cash_flow;
};

// ── Technical indicator signal ────────────────────────────────────────────────
enum class TechSignal { StrongBuy, Buy, Neutral, Sell, StrongSell };

struct TechIndicator {
    QString    name;
    double     value   = 0.0;
    TechSignal signal  = TechSignal::Neutral;
    QString    category; // "trend" | "momentum" | "volatility" | "volume"
};

struct TechnicalsData {
    QString symbol;
    QVector<TechIndicator> trend;
    QVector<TechIndicator> momentum;
    QVector<TechIndicator> volatility;
    QVector<TechIndicator> volume;
    TechSignal overall_signal = TechSignal::Neutral;
    int strong_buy  = 0;
    int buy         = 0;
    int neutral     = 0;
    int sell        = 0;
    int strong_sell = 0;
};

// ── Peer comparison ───────────────────────────────────────────────────────────
struct PeerData {
    QString symbol;
    QString name;
    QString sector;
    double  market_cap      = 0.0;
    double  pe_ratio        = 0.0;
    double  forward_pe      = 0.0;
    double  price_to_book   = 0.0;
    double  price_to_sales  = 0.0;
    double  peg_ratio       = 0.0;
    double  roe             = 0.0;
    double  roa             = 0.0;
    double  profit_margin   = 0.0;
    double  operating_margin= 0.0;
    double  gross_margin    = 0.0;
    double  revenue_growth  = 0.0;
    double  earnings_growth = 0.0;
    double  debt_to_equity  = 0.0;
    double  current_ratio   = 0.0;
    double  quick_ratio     = 0.0;
    double  dividend_yield  = 0.0;
    double  beta            = 0.0;
    double  price           = 0.0;
    double  change_pct      = 0.0;
};

// ── News article ──────────────────────────────────────────────────────────────
struct NewsArticle {
    QString title;
    QString description;
    QString url;
    QString publisher;
    QString published_date;
};

} // namespace fincept::services::equity
