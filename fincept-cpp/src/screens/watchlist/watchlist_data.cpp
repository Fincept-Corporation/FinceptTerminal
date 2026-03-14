#include "watchlist_data.h"
#include "python/python_runner.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

using json = nlohmann::json;

namespace fincept::watchlist {

static std::string run_python(const std::string& args) {
    std::vector<std::string> arg_vec;
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) arg_vec.push_back(token);

    auto result = fincept::python::execute("yfinance_data.py", arg_vec);
    if (result.success) return result.output;
    return "";
}

const StockQuote* WatchlistData::quote(const std::string& symbol) const {
    auto it = quotes_.find(symbol);
    if (it == quotes_.end()) return nullptr;
    return &it->second;
}

void WatchlistData::fetch_quotes(const std::vector<std::string>& symbols) {
    if (loading_.load()) return;
    if (symbols.empty()) return;

    loading_ = true;
    error_.clear();

    // Build args: "batch_quotes SYM1 SYM2 ..."
    std::string args = "batch_quotes";
    for (auto& s : symbols) args += " " + s;

    std::thread([this, args]() {
        std::string out = run_python(args);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!out.empty()) {
                parse_quotes(out);
            } else {
                error_ = "Failed to fetch quotes";
            }
            has_data_ = true;
        }
        loading_ = false;
    }).detach();
}

void WatchlistData::parse_quotes(const std::string& json_str) {
    quotes_.clear();
    try {
        auto j = json::parse(json_str);
        if (j.is_array()) {
            for (auto& item : j) {
                std::string sym = item.value("symbol", "");
                if (sym.empty()) continue;

                StockQuote q;
                q.price          = item.value("price", 0.0);
                q.change         = item.value("change", 0.0);
                q.change_percent = item.value("change_percent", 0.0);
                q.high           = item.value("high", 0.0);
                q.low            = item.value("low", 0.0);
                q.open           = item.value("open", 0.0);
                q.previous_close = item.value("previous_close", 0.0);
                q.volume         = item.value("volume", 0.0);
                q.valid          = true;
                quotes_[sym]     = q;
            }
        }
    } catch (...) {
        error_ = "Parse error";
    }
}

} // namespace fincept::watchlist
