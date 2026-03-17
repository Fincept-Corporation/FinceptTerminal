#include "research_data.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <array>
#include <string>
#include <sstream>
#include <limits>

using json = nlohmann::json;

namespace fincept::research {

// ─────────────────────────────────────────────────────────────────────────────
// Run yfinance via the unified python_runner module
// ─────────────────────────────────────────────────────────────────────────────
static std::string run_python(const std::string& args) {
    std::vector<std::string> arg_vec;
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) arg_vec.push_back(token);

    auto result = fincept::python::execute("yfinance_data.py", arg_vec);
    if (result.success) return result.output;
    return "";
}

// Helper: safely get double from flat json (yfinance returns flat keys)
static const double NAN_VAL = std::numeric_limits<double>::quiet_NaN();

static double jd(const json& j, const char* key, double def = 0.0) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_number()) return j[key].get<double>();
    }
    return def;
}

// Same but returns NaN for missing fields (for display-only data)
static double jd_nan(const json& j, const char* key) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_number()) return j[key].get<double>();
    }
    return NAN_VAL;
}

static std::string js(const json& j, const char* key, const std::string& def = "") {
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    return def;
}

static int ji(const json& j, const char* key, int def = 0) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_number()) return j[key].get<int>();
    }
    return def;
}

// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::fetch(const std::string& symbol, ChartPeriod period) {
    if (loading_.load()) return;
    loading_ = true;
    error_.clear();
    current_symbol_ = symbol;

    std::string sym = symbol;
    std::string per = chart_period_yf(period);

    std::thread([this, sym, per]() {
        auto& cache = CacheService::instance();

        // Helper: try cache, fallback to Python, cache on success
        auto cached_python = [&](const std::string& args, const std::string& cache_cat,
                                  const std::string& cache_sub, int64_t ttl) -> std::string {
            std::string key = CacheService::make_key(cache_cat, cache_sub, sym);
            auto cached = cache.get(key);
            if (cached && !cached->empty()) return *cached;
            std::string out = run_python(args);
            if (!out.empty()) cache.set(key, out, cache_cat, ttl);
            return out;
        };

        // 1. Fetch quote
        {
            std::string out = cached_python("quote " + sym, "market-quotes", "research-quote", CacheTTL::FIVE_MIN);
            if (!out.empty()) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                parse_quote(out);
            }
        }

        // 2. Fetch stock info
        {
            std::string out = cached_python("info " + sym, "reference", "research-info", CacheTTL::ONE_HOUR);
            if (!out.empty()) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                parse_stock_info(out);
            }
        }

        // 3. Fetch historical data
        {
            std::string hist_key = CacheService::make_key("market-quotes", "research-hist", sym + "_" + per);
            std::string out;
            auto cached_hist = cache.get(hist_key);
            if (cached_hist && !cached_hist->empty()) {
                out = *cached_hist;
            } else {
                out = run_python("historical_period " + sym + " " + per + " 1d");
                if (!out.empty()) cache.set(hist_key, out, "market-quotes", CacheTTL::FIVE_MIN);
            }
            if (!out.empty()) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                parse_historical(out);
            }
        }

        // 4. Fetch financials
        {
            std::string out = cached_python("financials " + sym, "reference", "research-financials", CacheTTL::ONE_HOUR);
            if (!out.empty()) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                parse_financials(out);
            }
        }

        // 5. Compute technicals from historical data
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            compute_technicals();
        }

        has_data_ = true;
        loading_ = false;
    }).detach();
}

// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::fetch_news(const std::string& symbol) {
    if (news_loading_.load()) return;
    news_loading_ = true;

    std::string sym = symbol;
    std::thread([this, sym]() {
        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("news", "research-news", sym);
        std::string out;
        auto cached = cache.get(cache_key);
        if (cached && !cached->empty()) {
            out = *cached;
        } else {
            out = run_python("news " + sym + " 20");
            if (!out.empty()) cache.set(cache_key, out, "news", CacheTTL::TEN_MIN);
        }
        if (!out.empty()) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            parse_news(out);
        }
        news_loading_ = false;
    }).detach();
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse quote from yfinance: flat JSON with price, change, volume, etc.
// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::parse_quote(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        if (j.contains("error")) return;

        quote_data_.symbol         = js(j, "symbol");
        quote_data_.price          = jd(j, "price");
        quote_data_.change         = jd(j, "change");
        quote_data_.change_percent = jd(j, "change_percent");
        quote_data_.volume         = jd(j, "volume");
        quote_data_.high           = jd(j, "high");
        quote_data_.low            = jd(j, "low");
        quote_data_.open           = jd(j, "open");
        quote_data_.previous_close = jd(j, "previous_close");
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse historical from yfinance: array of {timestamp, open, high, low, close, volume}
// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::parse_historical(const std::string& json_str) {
    historical_.clear();
    try {
        auto j = json::parse(json_str);
        if (!j.is_array()) return;

        for (auto& item : j) {
            HistoricalDataPoint dp;
            dp.timestamp = jd(item, "timestamp");
            dp.open      = jd(item, "open");
            dp.high      = jd(item, "high");
            dp.low       = jd(item, "low");
            dp.close     = jd(item, "close");
            dp.volume    = jd(item, "volume");
            if (dp.close > 0)
                historical_.push_back(dp);
        }
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse stock info from yfinance: flat JSON with all info fields
// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::parse_stock_info(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        if (j.contains("error")) return;

        auto& si = stock_info_;
        si.symbol       = current_symbol_;
        si.company_name = js(j, "company_name", current_symbol_);
        si.sector       = js(j, "sector");
        si.industry     = js(j, "industry");
        si.description  = js(j, "description");
        si.website      = js(j, "website");
        si.country      = js(j, "country");
        si.currency     = js(j, "currency");
        si.exchange     = js(j, "exchange");
        si.employees    = ji(j, "employees");

        // Prices & targets (use jd_nan so 0 vs missing is distinguishable)
        si.current_price       = jd_nan(j, "current_price");
        si.target_high_price   = jd_nan(j, "target_high_price");
        si.target_low_price    = jd_nan(j, "target_low_price");
        si.target_mean_price   = jd_nan(j, "target_mean_price");
        si.recommendation_mean = jd_nan(j, "recommendation_mean");
        si.recommendation_key  = js(j, "recommendation_key");
        si.number_of_analysts  = ji(j, "number_of_analyst_opinions");

        // Financial data
        si.total_cash          = jd_nan(j, "total_cash");
        si.total_debt          = jd_nan(j, "total_debt");
        si.total_revenue       = jd_nan(j, "total_revenue");
        si.revenue_per_share   = jd_nan(j, "revenue_per_share");
        si.return_on_assets    = jd_nan(j, "return_on_assets");
        si.return_on_equity    = jd_nan(j, "return_on_equity");
        si.gross_profits       = jd_nan(j, "gross_profits");
        si.free_cashflow       = jd_nan(j, "free_cashflow");
        si.operating_cashflow  = jd_nan(j, "operating_cashflow");
        si.earnings_growth     = jd_nan(j, "earnings_growth");
        si.revenue_growth      = jd_nan(j, "revenue_growth");
        si.gross_margins       = jd_nan(j, "gross_margins");
        si.operating_margins   = jd_nan(j, "operating_margins");
        si.ebitda_margins      = jd_nan(j, "ebitda_margins");
        si.profit_margins      = jd_nan(j, "profit_margins");

        // Key statistics
        si.beta                       = jd_nan(j, "beta");
        si.book_value                 = jd_nan(j, "book_value");
        si.price_to_book              = jd_nan(j, "price_to_book");
        si.enterprise_value           = jd_nan(j, "enterprise_value");
        si.enterprise_to_revenue      = jd_nan(j, "enterprise_to_revenue");
        si.enterprise_to_ebitda       = jd_nan(j, "enterprise_to_ebitda");
        si.shares_outstanding         = jd_nan(j, "shares_outstanding");
        si.float_shares               = jd_nan(j, "float_shares");
        si.held_percent_insiders      = jd_nan(j, "held_percent_insiders");
        si.held_percent_institutions  = jd_nan(j, "held_percent_institutions");
        si.short_ratio                = jd_nan(j, "short_ratio");
        si.short_percent_of_float     = jd_nan(j, "short_percent_of_float");
        si.peg_ratio                  = jd_nan(j, "peg_ratio");
        si.forward_pe                 = jd_nan(j, "forward_pe");

        // Summary
        si.pe_ratio            = jd_nan(j, "pe_ratio");
        si.dividend_yield      = jd_nan(j, "dividend_yield");
        si.fifty_two_week_high = jd_nan(j, "fifty_two_week_high");
        si.fifty_two_week_low  = jd_nan(j, "fifty_two_week_low");
        si.average_volume      = jd_nan(j, "average_volume");
        si.market_cap          = jd_nan(j, "market_cap");

        if ((std::isnan(si.current_price) || si.current_price == 0) && quote_data_.price > 0)
            si.current_price = quote_data_.price;
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse financials from yfinance
// Format: { "income_statement": { "2025-09-30 00:00:00": { "Total Revenue": 1234, ... } }, ... }
// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::parse_financials(const std::string& json_str) {
    financials_ = FinancialsData{};
    try {
        auto j = json::parse(json_str);
        if (j.contains("error")) return;

        auto parse_statement = [](const json& stmt_json, FinancialStatement& out) {
            if (!stmt_json.is_object()) return;
            for (auto& [period_key, metrics_json] : stmt_json.items()) {
                if (!metrics_json.is_object()) continue;
                // Clean up period key: "2025-09-30 00:00:00" -> "2025-09-30"
                std::string period = period_key;
                auto space_pos = period.find(' ');
                if (space_pos != std::string::npos)
                    period = period.substr(0, space_pos);

                std::map<std::string, double> metrics;
                for (auto& [metric_key, val] : metrics_json.items()) {
                    if (val.is_number()) {
                        metrics[metric_key] = val.get<double>();
                    }
                }
                if (!metrics.empty())
                    out.data[period] = metrics;
            }
        };

        if (j.contains("income_statement"))
            parse_statement(j["income_statement"], financials_.income_statement);
        if (j.contains("balance_sheet"))
            parse_statement(j["balance_sheet"], financials_.balance_sheet);
        if (j.contains("cash_flow"))
            parse_statement(j["cash_flow"], financials_.cash_flow);
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse news from yfinance
// Format: { "articles": [ { "title": ..., "url": ..., "publisher": ..., ... } ] }
// ─────────────────────────────────────────────────────────────────────────────
void ResearchData::parse_news(const std::string& json_str) {
    news_.clear();
    try {
        auto j = json::parse(json_str);
        if (!j.contains("articles")) return;
        for (auto& item : j["articles"]) {
            ResearchNewsArticle a;
            a.title          = js(item, "title");
            a.url            = js(item, "url");
            a.publisher      = js(item, "publisher");
            a.description    = js(item, "description");
            a.published_date = js(item, "published_date");
            if (!a.title.empty())
                news_.push_back(a);
        }
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Compute technical indicators from historical data
// ─────────────────────────────────────────────────────────────────────────────
static Signal get_signal(const std::string& name, double value, double prev) {
    if (name.find("RSI") != std::string::npos) {
        if (value <= 30) return Signal::BUY;
        if (value >= 70) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name.find("Stoch") != std::string::npos) {
        if (value <= 20) return Signal::BUY;
        if (value >= 80) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name == "MACD") {
        if (prev < 0 && value > 0) return Signal::BUY;
        if (prev > 0 && value < 0) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name.find("CCI") != std::string::npos) {
        if (value <= -100) return Signal::BUY;
        if (value >= 100) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name.find("Williams") != std::string::npos) {
        if (value <= -80) return Signal::BUY;
        if (value >= -20) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name.find("ROC") != std::string::npos) {
        if (value > 5) return Signal::BUY;
        if (value < -5) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name.find("BB %B") != std::string::npos) {
        if (value < 0.2) return Signal::BUY;
        if (value > 0.8) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    if (name == "ADX") {
        if (value > 25) return Signal::BUY;
        return Signal::NEUTRAL;
    }
    if (name.find("SMA") != std::string::npos || name.find("EMA") != std::string::npos) {
        if (value > 0) return Signal::BUY;
        if (value < 0) return Signal::SELL;
        return Signal::NEUTRAL;
    }
    return Signal::NEUTRAL;
}

void ResearchData::compute_technicals() {
    technicals_ = TechnicalsData{};
    if (historical_.size() < 30) return;

    size_t n = historical_.size();
    std::vector<double> closes(n);
    std::vector<double> highs(n);
    std::vector<double> lows(n);
    std::vector<double> volumes(n);
    for (size_t i = 0; i < n; i++) {
        closes[i]  = historical_[i].close;
        highs[i]   = historical_[i].high;
        lows[i]    = historical_[i].low;
        volumes[i] = historical_[i].volume;
    }

    auto prev_idx = [&](size_t idx) -> size_t { return idx > 0 ? idx - 1 : 0; };

    auto add_indicator = [&](const std::string& name, double val, double prev,
                             double mn, double mx, double avg, const std::string& cat) {
        IndicatorValue iv;
        iv.name = name;
        iv.value = val;
        iv.prev_value = prev;
        iv.min_val = mn;
        iv.max_val = mx;
        iv.avg_val = avg;
        iv.category = cat;
        iv.signal = get_signal(name, val, prev);
        technicals_.indicators.push_back(iv);
    };

    // ── SMA ──
    auto calc_sma = [&](size_t period) -> std::vector<double> {
        std::vector<double> sma(n, 0);
        if (period == 0 || period > n) return sma;
        for (size_t i = period - 1; i < n; i++) {
            double sum = 0;
            for (size_t j = 0; j < period; j++) sum += closes[i - j];
            sma[i] = sum / (double)period;
        }
        return sma;
    };

    double last_price = closes.back();

    if (n >= 20) {
        auto sma20 = calc_sma(20);
        if (sma20.back() > 0)
            add_indicator("SMA 20", last_price - sma20.back(), last_price - sma20[prev_idx(n-1)],
                          sma20.back(), sma20.back(), sma20.back(), "trend");
    }
    if (n >= 50) {
        auto sma50 = calc_sma(50);
        if (sma50.back() > 0)
            add_indicator("SMA 50", last_price - sma50.back(), last_price - sma50[prev_idx(n-1)],
                          sma50.back(), sma50.back(), sma50.back(), "trend");
    }
    if (n >= 200) {
        auto sma200 = calc_sma(200);
        if (sma200.back() > 0)
            add_indicator("SMA 200", last_price - sma200.back(), last_price - sma200[prev_idx(n-1)],
                          sma200.back(), sma200.back(), sma200.back(), "trend");
    }

    // ── EMA ──
    auto calc_ema = [&](int period) -> std::vector<double> {
        std::vector<double> ema(n, 0);
        double mult = 2.0 / (period + 1);
        ema[0] = closes[0];
        for (size_t i = 1; i < n; i++)
            ema[i] = closes[i] * mult + ema[i-1] * (1 - mult);
        return ema;
    };

    auto ema12 = calc_ema(12);
    auto ema26 = calc_ema(26);

    add_indicator("EMA 12", last_price - ema12.back(), last_price - ema12[prev_idx(n-1)],
                  ema12.back(), ema12.back(), ema12.back(), "trend");
    add_indicator("EMA 26", last_price - ema26.back(), last_price - ema26[prev_idx(n-1)],
                  ema26.back(), ema26.back(), ema26.back(), "trend");

    // ── MACD ──
    {
        std::vector<double> macd_line(n);
        for (size_t i = 0; i < n; i++) macd_line[i] = ema12[i] - ema26[i];

        std::vector<double> signal_line(n, 0);
        double mult = 2.0 / 10.0;
        signal_line[0] = macd_line[0];
        for (size_t i = 1; i < n; i++)
            signal_line[i] = macd_line[i] * mult + signal_line[i-1] * (1 - mult);

        double histogram = macd_line.back() - signal_line.back();
        double prev_hist = macd_line[prev_idx(n-1)] - signal_line[prev_idx(n-1)];

        add_indicator("MACD", histogram, prev_hist,
                      -std::abs(histogram), std::abs(histogram), 0, "momentum");
    }

    // ── RSI(14) ──
    if (n > 16) {
        int period = 14;
        std::vector<double> rsi_vals(n, 50);
        double avg_gain = 0, avg_loss = 0;
        for (int i = 1; i <= period; i++) {
            double d = closes[i] - closes[i-1];
            if (d > 0) avg_gain += d; else avg_loss += std::abs(d);
        }
        avg_gain /= period;
        avg_loss /= period;

        for (size_t i = (size_t)period; i < n; i++) {
            double d = closes[i] - closes[i-1];
            avg_gain = (avg_gain * (period - 1) + (d > 0 ? d : 0)) / period;
            avg_loss = (avg_loss * (period - 1) + (d < 0 ? std::abs(d) : 0)) / period;
            double rs = avg_loss > 0 ? avg_gain / avg_loss : 100;
            rsi_vals[i] = 100.0 - (100.0 / (1.0 + rs));
        }

        size_t start = (size_t)period;
        double mn = rsi_vals[start], mx = rsi_vals[start], sum = 0;
        for (size_t i = start; i < n; i++) {
            mn = std::min(mn, rsi_vals[i]);
            mx = std::max(mx, rsi_vals[i]);
            sum += rsi_vals[i];
        }
        double avg = sum / (double)(n - start);
        add_indicator("RSI (14)", rsi_vals.back(), rsi_vals[prev_idx(n-1)], mn, mx, avg, "momentum");
    }

    // ── Stochastic %K(14,3) ──
    if (n > 17) {
        size_t k_period = 14;
        std::vector<double> stoch_k(n, 50);
        for (size_t i = k_period - 1; i < n; i++) {
            size_t start = i - k_period + 1;
            double hi = *std::max_element(highs.begin() + (ptrdiff_t)start, highs.begin() + (ptrdiff_t)(i + 1));
            double lo = *std::min_element(lows.begin() + (ptrdiff_t)start, lows.begin() + (ptrdiff_t)(i + 1));
            stoch_k[i] = (hi != lo) ? ((closes[i] - lo) / (hi - lo)) * 100.0 : 50.0;
        }
        std::vector<double> stoch_d(n, 50);
        for (size_t i = k_period + 2; i < n; i++) {
            stoch_d[i] = (stoch_k[i] + stoch_k[i-1] + stoch_k[i-2]) / 3.0;
        }

        add_indicator("Stoch %K", stoch_k.back(), stoch_k[prev_idx(n-1)], 0, 100, 50, "momentum");
        add_indicator("Stoch %D", stoch_d.back(), stoch_d[prev_idx(n-1)], 0, 100, 50, "momentum");
    }

    // ── CCI(20) ──
    if (n > 21) {
        size_t period = 20;
        std::vector<double> cci_vals(n, 0);
        for (size_t i = period - 1; i < n; i++) {
            double tp = (highs[i] + lows[i] + closes[i]) / 3.0;
            double tp_sma = 0;
            for (size_t j = 0; j < period; j++)
                tp_sma += (highs[i-j] + lows[i-j] + closes[i-j]) / 3.0;
            tp_sma /= (double)period;

            double mean_dev = 0;
            for (size_t j = 0; j < period; j++) {
                double tp_j = (highs[i-j] + lows[i-j] + closes[i-j]) / 3.0;
                mean_dev += std::abs(tp_j - tp_sma);
            }
            mean_dev /= (double)period;
            cci_vals[i] = mean_dev > 0 ? (tp - tp_sma) / (0.015 * mean_dev) : 0;
        }
        add_indicator("CCI (20)", cci_vals.back(), cci_vals[prev_idx(n-1)], -200, 200, 0, "momentum");
    }

    // ── Williams %R(14) ──
    if (n > 15) {
        size_t period = 14;
        std::vector<double> wr(n, -50);
        for (size_t i = period - 1; i < n; i++) {
            size_t start = i - period + 1;
            double hi = *std::max_element(highs.begin() + (ptrdiff_t)start, highs.begin() + (ptrdiff_t)(i + 1));
            double lo = *std::min_element(lows.begin() + (ptrdiff_t)start, lows.begin() + (ptrdiff_t)(i + 1));
            wr[i] = (hi != lo) ? ((hi - closes[i]) / (hi - lo)) * -100.0 : -50.0;
        }
        add_indicator("Williams %R", wr.back(), wr[prev_idx(n-1)], -100, 0, -50, "momentum");
    }

    // ── Bollinger Bands %B ──
    if (n > 21) {
        size_t period = 20;
        double sma = 0;
        for (size_t i = n - period; i < n; i++) sma += closes[i];
        sma /= (double)period;

        double variance = 0;
        for (size_t i = n - period; i < n; i++)
            variance += (closes[i] - sma) * (closes[i] - sma);
        double stddev = std::sqrt(variance / (double)period);

        double upper = sma + 2 * stddev;
        double lower = sma - 2 * stddev;
        double pct_b = (upper != lower) ? (closes.back() - lower) / (upper - lower) : 0.5;

        add_indicator("BB %B", pct_b, 0.5, 0, 1, 0.5, "volatility");
        add_indicator("BB Width", sma > 0 ? (upper - lower) / sma : 0, 0, 0, 0.5, 0.1, "volatility");
    }

    // ── ROC(12) ──
    if (n > 14) {
        size_t period = 12;
        size_t ref = n - 1 - period;
        size_t prev_ref = n - 2 - period;
        if (ref < n && prev_ref < n && closes[ref] != 0 && closes[prev_ref] != 0) {
            double roc = ((closes[n-1] - closes[ref]) / closes[ref]) * 100.0;
            double prev_roc = ((closes[n-2] - closes[prev_ref]) / closes[prev_ref]) * 100.0;
            add_indicator("ROC (12)", roc, prev_roc, -20, 20, 0, "momentum");
        }
    }

    // ── ADX(14) — simplified ──
    if (n > 29) {
        size_t period = 14;
        double sum = 0;
        for (size_t i = n - period; i < n; i++) {
            double up = highs[i] - highs[i-1];
            double dn = lows[i-1] - lows[i];
            sum += (up > dn && up > 0) ? up : (dn > up && dn > 0) ? dn : 0;
        }
        double avg_dm = sum / (double)period;
        double avg_tr = 0;
        for (size_t i = n - period; i < n; i++)
            avg_tr += std::max({highs[i] - lows[i],
                                std::abs(highs[i] - closes[i-1]),
                                std::abs(lows[i] - closes[i-1])});
        avg_tr /= (double)period;
        double adx = avg_tr > 0 ? (avg_dm / avg_tr) * 100.0 : 0;
        add_indicator("ADX", std::min(adx, 100.0), 0, 0, 100, 25, "trend");
    }

    // Count signals
    for (auto& ind : technicals_.indicators) {
        if (ind.signal == Signal::BUY) technicals_.buy_count++;
        else if (ind.signal == Signal::SELL) technicals_.sell_count++;
        else technicals_.neutral_count++;
    }

    int total = technicals_.buy_count + technicals_.sell_count + technicals_.neutral_count;
    if (total > 0) {
        double buy_pct = (double)technicals_.buy_count / total * 100.0;
        double sell_pct = (double)technicals_.sell_count / total * 100.0;
        if (buy_pct >= 70) technicals_.recommendation = "STRONG BUY";
        else if (buy_pct >= 50) technicals_.recommendation = "BUY";
        else if (sell_pct >= 70) technicals_.recommendation = "STRONG SELL";
        else if (sell_pct >= 50) technicals_.recommendation = "SELL";
        else technicals_.recommendation = "NEUTRAL";
    } else {
        technicals_.recommendation = "NEUTRAL";
    }
}

} // namespace fincept::research
