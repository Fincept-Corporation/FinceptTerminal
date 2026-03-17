#include "mna_data.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "core/logger.h"
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

void MNAData::run_analysis(const std::string& script,
                           const std::vector<std::string>& args) {
    printf("[MNA] run_analysis called: script=%s, args=%zu\n", script.c_str(), args.size());
    for (size_t i = 0; i < args.size(); i++) {
        printf("[MNA]   arg[%zu] = %.200s%s\n", i, args[i].c_str(), args[i].size() > 200 ? "..." : "");
    }
    fflush(stdout);

    if (loading_.load()) {
        printf("[MNA] SKIPPED — already loading\n"); fflush(stdout);
        return;
    }
    loading_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_result_ = false;
        error_.clear();
        result_ = json();
    }

    // Build full script path under Analytics/corporateFinance/
    std::string full_script = "Analytics/corporateFinance/" + script;
    printf("[MNA] full_script = %s\n", full_script.c_str()); fflush(stdout);

    std::thread([this, full_script, args]() {
      try {
        printf("[MNA][thread] Starting python::execute for %s\n", full_script.c_str()); fflush(stdout);

        auto& cache = CacheService::instance();
        printf("[MNA][thread] CacheService OK\n"); fflush(stdout);
        std::string args_key;
        for (const auto& a : args) args_key += a + "_";
        std::string cache_key = CacheService::make_key("reference", "mna", full_script + "_" + args_key);
        printf("[MNA][thread] cache_key = %.200s\n", cache_key.c_str()); fflush(stdout);

        // Skip cache for now — debug: always run fresh
        // auto cached = cache.get(cache_key);
        printf("[MNA][thread] Cache SKIPPED (debug mode)\n"); fflush(stdout);

        printf("[MNA][thread] Calling python::execute...\n"); fflush(stdout);
        auto py_result = python::execute(full_script, args);
        printf("[MNA][thread] python::execute returned: success=%d, exit_code=%d\n",
               py_result.success, py_result.exit_code); fflush(stdout);
        printf("[MNA][thread] output (first 500): %.500s\n", py_result.output.c_str()); fflush(stdout);
        if (!py_result.error.empty()) {
            printf("[MNA][thread] error: %.500s\n", py_result.error.c_str()); fflush(stdout);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                parse_result(py_result.output);
                printf("[MNA][thread] parse_result done: has_result=%d\n", has_result_.load()); fflush(stdout);
                if (has_result_ && !py_result.output.empty()) {
                    cache.set(cache_key, py_result.output, "reference", CacheTTL::FIFTEEN_MIN);
                }
            } else {
                std::string err_msg;
                if (!py_result.error.empty()) {
                    try {
                        std::string json_str = python::extract_json(py_result.error);
                        if (!json_str.empty()) {
                            auto err_json = json::parse(json_str);
                            if (err_json.contains("error"))
                                err_msg = err_json["error"].get<std::string>();
                        }
                    } catch (...) {}
                }
                if (err_msg.empty()) {
                    err_msg = py_result.error.empty()
                        ? "Python script failed (exit " + std::to_string(py_result.exit_code) + ")"
                        : py_result.error;
                }
                error_ = err_msg;
                has_result_ = false;
                printf("[MNA][thread] FAILED: %s\n", error_.c_str()); fflush(stdout);
                LOG_ERROR("MNAData", "Script failed: %s — %s",
                          full_script.c_str(), error_.c_str());
            }
        }
        loading_.store(false);
        printf("[MNA][thread] Done, loading=false\n"); fflush(stdout);
      } catch (const std::exception& ex) {
        printf("[MNA][thread] EXCEPTION: %s\n", ex.what()); fflush(stdout);
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = std::string("Internal error: ") + ex.what();
        has_result_ = false;
        loading_.store(false);
      } catch (...) {
        printf("[MNA][thread] UNKNOWN EXCEPTION\n"); fflush(stdout);
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = "Internal error (unknown exception)";
        has_result_ = false;
        loading_.store(false);
      }
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
        auto& cache = CacheService::instance();
        std::string args_key;
        for (const auto& a : args) args_key += a + "_";
        std::string cache_key = CacheService::make_key("reference", "mna-stdin",
            full_script + "_" + args_key + stdin_json.substr(0, std::min((size_t)64, stdin_json.size())));

        // Skip cache for now — debug mode
        // auto cached = cache.get(cache_key);

        auto py_result = python::execute_with_stdin(full_script, args, stdin_json);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                parse_result(py_result.output);
                if (has_result_ && !py_result.output.empty()) {
                    cache.set(cache_key, py_result.output, "reference", CacheTTL::FIFTEEN_MIN);
                }
            } else {
                std::string err_msg;
                if (!py_result.error.empty()) {
                    try {
                        std::string json_str = python::extract_json(py_result.error);
                        if (!json_str.empty()) {
                            auto err_json = json::parse(json_str);
                            if (err_json.contains("error"))
                                err_msg = err_json["error"].get<std::string>();
                        }
                    } catch (...) {}
                }
                if (err_msg.empty()) {
                    err_msg = py_result.error.empty()
                        ? "Python script failed (exit " + std::to_string(py_result.exit_code) + ")"
                        : py_result.error;
                }
                error_ = err_msg;
                has_result_ = false;
            }
        }
        loading_.store(false);
    }).detach();
}

std::string MNAData::error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

void MNAData::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = json();
    error_.clear();
    has_result_ = false;
}

void MNAData::parse_result(const std::string& output) {
    printf("[MNA] parse_result input (first 500): %.500s\n", output.c_str()); fflush(stdout);
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
