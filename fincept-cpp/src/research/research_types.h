#pragma once
// Equity Research — Data types mirroring the React terminal's types/index.ts
// StockInfo, QuoteData, HistoricalData, FinancialsData, TechnicalsData

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <limits>

namespace fincept::research {

// NaN sentinel for "no data" — distinct from a valid 0 value
constexpr double NODATA = std::numeric_limits<double>::quiet_NaN();

struct StockInfo {
    std::string symbol;
    std::string company_name;
    std::string sector;
    std::string industry;
    std::string description;
    std::string website;
    std::string country;
    std::string currency;
    std::string exchange;
    std::string recommendation_key; // "buy", "hold", "sell", "strong_buy", "strong_sell"

    double market_cap         = NODATA;
    double pe_ratio           = NODATA;
    double forward_pe         = NODATA;
    double dividend_yield     = NODATA;
    double beta               = NODATA;
    double fifty_two_week_high= NODATA;
    double fifty_two_week_low = NODATA;
    double average_volume     = NODATA;
    double current_price      = NODATA;
    double target_high_price  = NODATA;
    double target_low_price   = NODATA;
    double target_mean_price  = NODATA;
    double recommendation_mean= NODATA;
    int    number_of_analysts = 0;
    int    employees          = 0;

    // Financial health
    double total_cash         = NODATA;
    double total_debt         = NODATA;
    double total_revenue      = NODATA;
    double revenue_per_share  = NODATA;
    double return_on_assets   = NODATA;
    double return_on_equity   = NODATA;
    double gross_profits      = NODATA;
    double free_cashflow      = NODATA;
    double operating_cashflow = NODATA;
    double earnings_growth    = NODATA;
    double revenue_growth     = NODATA;
    double gross_margins      = NODATA;
    double operating_margins  = NODATA;
    double ebitda_margins     = NODATA;
    double profit_margins     = NODATA;
    double book_value         = NODATA;
    double price_to_book      = NODATA;
    double enterprise_value   = NODATA;
    double enterprise_to_revenue = NODATA;
    double enterprise_to_ebitda  = NODATA;
    double shares_outstanding = NODATA;
    double float_shares       = NODATA;
    double held_percent_insiders     = NODATA;
    double held_percent_institutions = NODATA;
    double short_ratio        = NODATA;
    double short_percent_of_float    = NODATA;
    double peg_ratio          = NODATA;
};

struct QuoteData {
    std::string symbol;
    double price           = 0;
    double change          = 0;
    double change_percent  = 0;
    double volume          = 0;
    double high            = 0;
    double low             = 0;
    double open            = 0;
    double previous_close  = 0;
};

struct HistoricalDataPoint {
    double timestamp = 0; // unix seconds
    double open      = 0;
    double high      = 0;
    double low       = 0;
    double close     = 0;
    double volume    = 0;
};

// Period-based historical data
struct FinancialStatement {
    // period label -> (metric name -> value)
    // e.g. "2024-12-31" -> { "TotalRevenue": 123456, ... }
    std::map<std::string, std::map<std::string, double>> data;
};

struct FinancialsData {
    FinancialStatement income_statement;
    FinancialStatement balance_sheet;
    FinancialStatement cash_flow;
};

// Technical indicator signal
enum class Signal { BUY, SELL, NEUTRAL };

struct IndicatorValue {
    std::string name;
    double      value      = 0;
    double      prev_value = 0;
    double      min_val    = 0;
    double      max_val    = 0;
    double      avg_val    = 0;
    Signal      signal     = Signal::NEUTRAL;
    std::string category;  // "trend", "momentum", "volatility", "volume"
};

struct TechnicalsData {
    std::vector<IndicatorValue> indicators;
    int buy_count    = 0;
    int sell_count   = 0;
    int neutral_count= 0;
    std::string recommendation; // "STRONG BUY", "BUY", "NEUTRAL", "SELL", "STRONG SELL"
};

// News article (symbol-specific)
struct ResearchNewsArticle {
    std::string title;
    std::string description;
    std::string url;
    std::string publisher;
    std::string published_date;
};

// Chart period
enum class ChartPeriod { M1, M3, M6, Y1, Y5 };

inline const char* chart_period_label(ChartPeriod p) {
    switch (p) {
        case ChartPeriod::M1: return "1M";
        case ChartPeriod::M3: return "3M";
        case ChartPeriod::M6: return "6M";
        case ChartPeriod::Y1: return "1Y";
        case ChartPeriod::Y5: return "5Y";
    }
    return "6M";
}

inline const char* chart_period_yf(ChartPeriod p) {
    switch (p) {
        case ChartPeriod::M1: return "1mo";
        case ChartPeriod::M3: return "3mo";
        case ChartPeriod::M6: return "6mo";
        case ChartPeriod::Y1: return "1y";
        case ChartPeriod::Y5: return "5y";
    }
    return "6mo";
}

} // namespace fincept::research
