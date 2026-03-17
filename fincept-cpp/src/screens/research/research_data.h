#pragma once
// Equity Research — Data fetching layer
// Fetches stock info, quote, historical, financials, technicals, news via public APIs
// All fetching is async (background thread) with mutex-protected results

#include "research_types.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <string>
#include <vector>

namespace fincept::research {

class ResearchData {
public:
    // Fetch all data for a symbol (runs in background thread)
    void fetch(const std::string& symbol, ChartPeriod period);

    // Fetch news for current symbol
    void fetch_news(const std::string& symbol);

    // Access data (call under lock or copy)
    std::shared_mutex& mutex() { return mutex_; }

    bool is_loading() const { return loading_.load(); }
    bool is_news_loading() const { return news_loading_.load(); }
    bool has_data() const { return has_data_; }
    const std::string& error() const { return error_; }

    const StockInfo&    stock_info()    const { return stock_info_; }
    const QuoteData&    quote_data()    const { return quote_data_; }
    const std::vector<HistoricalDataPoint>& historical() const { return historical_; }
    const FinancialsData& financials()  const { return financials_; }
    const TechnicalsData& technicals()  const { return technicals_; }
    const std::vector<ResearchNewsArticle>& news() const { return news_; }

    const std::string& current_symbol() const { return current_symbol_; }

private:
    std::shared_mutex mutex_;
    std::atomic<bool> loading_{false};
    std::atomic<bool> news_loading_{false};
    bool has_data_ = false;
    std::string error_;
    std::string current_symbol_;

    StockInfo    stock_info_;
    QuoteData    quote_data_;
    std::vector<HistoricalDataPoint> historical_;
    FinancialsData financials_;
    TechnicalsData technicals_;
    std::vector<ResearchNewsArticle> news_;

    // Parse helpers
    void parse_stock_info(const std::string& json_str);
    void parse_quote(const std::string& json_str);
    void parse_historical(const std::string& json_str);
    void parse_financials(const std::string& json_str);
    void compute_technicals();
    void parse_news(const std::string& json_str);
};

} // namespace fincept::research
