#include "markets_data.h"
#include "python/python_runner.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

using json = nlohmann::json;

namespace fincept::markets {

const std::string MarketsData::empty_string_;
const std::vector<MarketQuote> MarketsData::empty_quotes_;

// Run python via the unified python_runner module
static std::string run_python(const std::string& args) {
    std::vector<std::string> arg_vec;
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) arg_vec.push_back(token);

    auto result = fincept::python::execute("yfinance_data.py", arg_vec);
    if (result.success) return result.output;
    return "";
}

bool MarketsData::is_loading(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return false;
    return it->second.loading.load();
}

bool MarketsData::has_data(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return false;
    return it->second.has_data;
}

const std::string& MarketsData::error(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return empty_string_;
    return it->second.error;
}

const std::vector<MarketQuote>& MarketsData::quotes(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return empty_quotes_;
    return it->second.quotes;
}

void MarketsData::fetch_category(const std::string& category, const std::vector<std::string>& symbols) {
    auto& state = categories_[category];
    if (state.loading.load()) return;
    state.loading = true;
    state.error.clear();

    // Build symbol args: "batch_quotes SYM1 SYM2 SYM3..."
    std::string args = "batch_quotes";
    for (auto& s : symbols) args += " " + s;

    std::string cat = category;
    std::thread([this, cat, args]() {
        std::string out = run_python(args);
        if (!out.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            parse_quotes(cat, out);
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            categories_[cat].error = "Failed to fetch data";
        }
        categories_[cat].has_data = true;
        categories_[cat].loading = false;
    }).detach();
}

void MarketsData::fetch_all(const std::vector<MarketCategory>& categories) {
    for (auto& cat : categories) {
        fetch_category(cat.title, cat.symbols);
    }
}

void MarketsData::parse_quotes(const std::string& category, const std::string& json_str) {
    auto& state = categories_[category];
    state.quotes.clear();
    try {
        auto j = json::parse(json_str);
        if (j.is_array()) {
            for (auto& item : j) {
                MarketQuote q;
                q.symbol = item.value("symbol", "");
                q.price = item.value("price", 0.0);
                q.change = item.value("change", 0.0);
                q.change_percent = item.value("change_percent", 0.0);
                q.high = item.value("high", 0.0);
                q.low = item.value("low", 0.0);
                q.open = item.value("open", 0.0);
                q.previous_close = item.value("previous_close", 0.0);
                q.volume = item.value("volume", 0.0);
                if (!q.symbol.empty())
                    state.quotes.push_back(q);
            }
        }
    } catch (...) {
        state.error = "Parse error";
    }
}

} // namespace fincept::markets
