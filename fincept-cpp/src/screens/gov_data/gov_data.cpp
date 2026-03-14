#include "gov_data.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

using json = nlohmann::json;

void GovData::run_script(const std::string& script,
                          const std::vector<std::string>& args) {
    if (loading_.load()) return;
    loading_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_result_ = false;
        error_.clear();
        result_ = json();
    }

    std::thread([this, script, args]() {
        auto py_result = python::execute(script, args);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                parse_result(py_result.output);
            } else {
                error_ = py_result.error.empty()
                    ? "Script failed (exit " + std::to_string(py_result.exit_code) + ")"
                    : py_result.error;
                has_result_ = false;
                LOG_ERROR("GovData", "Script failed: %s — %s",
                          script.c_str(), error_.c_str());
            }
        }
        loading_.store(false);
    }).detach();
}

void GovData::run_ckan(const std::string& country, const std::string& action,
                        const std::vector<std::string>& extra_args) {
    std::vector<std::string> args = {"--country", country, "--action", action};
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    run_script("universal_ckan_api.py", args);
}

bool GovData::has_result() const {
    return has_result_;
}

const std::string& GovData::error() const {
    return error_;
}

void GovData::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = json();
    error_.clear();
    has_result_ = false;
}

void GovData::parse_result(const std::string& output) {
    try {
        std::string json_str = python::extract_json(output);
        if (json_str.empty()) json_str = output;

        auto parsed = json::parse(json_str);

        // Handle {success: true, data: {...}} wrapper
        if (parsed.contains("success") && parsed["success"].is_boolean()) {
            if (parsed["success"].get<bool>()) {
                result_ = parsed.contains("data") ? parsed["data"] : parsed;
                has_result_ = true;
            } else {
                error_ = parsed.value("error", "Unknown error from Python");
                has_result_ = false;
            }
        }
        // Handle {data: ..., metadata: ..., error: null} wrapper (CKAN-style)
        else if (parsed.contains("data") && parsed.contains("error")) {
            if (parsed["error"].is_null() || parsed["error"].get<std::string>().empty()) {
                result_ = parsed;  // Keep full response including metadata
                has_result_ = true;
            } else {
                error_ = parsed["error"].get<std::string>();
                has_result_ = false;
            }
        }
        else {
            // Raw JSON result
            result_ = parsed;
            has_result_ = true;
        }
    } catch (const json::exception& e) {
        error_ = std::string("JSON parse error: ") + e.what();
        has_result_ = false;
        LOG_ERROR("GovData", "JSON parse failed: %s", e.what());
    }
}

} // namespace fincept::gov_data
