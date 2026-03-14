#include "mna_data.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

void MNAData::run_analysis(const std::string& script,
                           const std::vector<std::string>& args) {
    if (loading_.load()) return;  // Already running
    loading_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_result_ = false;
        error_.clear();
        result_ = json();
    }

    // Build full script path under Analytics/corporateFinance/
    std::string full_script = "Analytics/corporateFinance/" + script;

    std::thread([this, full_script, args]() {
        auto py_result = python::execute(full_script, args);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                parse_result(py_result.output);
            } else {
                error_ = py_result.error.empty()
                    ? "Python script failed (exit " + std::to_string(py_result.exit_code) + ")"
                    : py_result.error;
                has_result_ = false;
                LOG_ERROR("MNAData", "Script failed: %s — %s",
                          full_script.c_str(), error_.c_str());
            }
        }
        loading_.store(false);
    }).detach();
}

void MNAData::run_analysis_stdin(const std::string& script,
                                  const std::vector<std::string>& args,
                                  const std::string& stdin_json) {
    if (loading_.load()) return;
    loading_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_result_ = false;
        error_.clear();
        result_ = json();
    }

    std::string full_script = "Analytics/corporateFinance/" + script;

    std::thread([this, full_script, args, stdin_json]() {
        auto py_result = python::execute_with_stdin(full_script, args, stdin_json);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                parse_result(py_result.output);
            } else {
                error_ = py_result.error.empty()
                    ? "Python script failed (exit " + std::to_string(py_result.exit_code) + ")"
                    : py_result.error;
                has_result_ = false;
            }
        }
        loading_.store(false);
    }).detach();
}

bool MNAData::has_result() const {
    return has_result_;
}

const std::string& MNAData::error() const {
    return error_;
}

void MNAData::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = json();
    error_.clear();
    has_result_ = false;
}

void MNAData::parse_result(const std::string& output) {
    try {
        // Extract JSON from Python output (may have debug prints before JSON)
        std::string json_str = python::extract_json(output);
        if (json_str.empty()) json_str = output;

        auto parsed = json::parse(json_str);

        // Handle standard {success: true, data: {...}} wrapper
        if (parsed.contains("success") && parsed["success"].is_boolean()) {
            if (parsed["success"].get<bool>()) {
                result_ = parsed.contains("data") ? parsed["data"] : parsed;
                has_result_ = true;
            } else {
                error_ = parsed.value("error", "Unknown error from Python");
                has_result_ = false;
            }
        } else {
            // Raw JSON result
            result_ = parsed;
            has_result_ = true;
        }
    } catch (const json::exception& e) {
        error_ = std::string("JSON parse error: ") + e.what();
        has_result_ = false;
        LOG_ERROR("MNAData", "JSON parse failed: %s", e.what());
    }
}

} // namespace fincept::mna
