#pragma once
// M&A Analytics — async Python execution layer with mutex-guarded results

#include "mna_types.h"
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::mna {

class MNAData {
public:
    // Run a Python script asynchronously (background thread)
    // script: relative path under corporateFinance/ (e.g. "valuation/dcf_model.py")
    // args: command-line arguments to the script
    void run_analysis(const std::string& script, const std::vector<std::string>& args);

    // Run a Python script with stdin JSON data
    void run_analysis_stdin(const std::string& script,
                            const std::vector<std::string>& args,
                            const std::string& stdin_json);

    // State queries
    bool is_loading() const { return loading_.load(); }
    bool has_result() const { return has_result_.load(); }
    std::string error() const;

    // Access the JSON result (caller must hold mutex)
    std::mutex& mutex() { return mutex_; }
    const nlohmann::json& result() const { return result_; }

    // Clear previous result
    void clear();

private:
    mutable std::mutex mutex_;
    std::atomic<bool> loading_{false};
    nlohmann::json result_;
    std::string error_;
    std::atomic<bool> has_result_{false};

    void parse_result(const std::string& output);
};

} // namespace fincept::mna
