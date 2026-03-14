#pragma once
// Government Data — async Python execution layer with mutex-guarded results
// Same pattern as MNAData but for government data scripts

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class GovData {
public:
    // Run a Python script asynchronously (background thread)
    // script: filename in scripts/ (e.g. "government_us_data.py")
    // args: command-line arguments to the script
    void run_script(const std::string& script, const std::vector<std::string>& args);

    // Run with --country and --action args (for universal_ckan_api.py)
    void run_ckan(const std::string& country, const std::string& action,
                  const std::vector<std::string>& extra_args = {});

    // State queries
    bool is_loading() const { return loading_.load(); }
    bool has_result() const;
    const std::string& error() const;

    // Access the JSON result (caller must hold mutex)
    std::mutex& mutex() { return mutex_; }
    const nlohmann::json& result() const { return result_; }

    // Clear previous result
    void clear();

private:
    std::mutex mutex_;
    std::atomic<bool> loading_{false};
    nlohmann::json result_;
    std::string error_;
    bool has_result_ = false;

    void parse_result(const std::string& output);
};

} // namespace fincept::gov_data
