// Python MCP Tools — run analytics scripts, list scripts, check status

#include "python_tools.h"
#include "../../python/python_runner.h"
#include "../../core/logger.h"
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fincept::mcp::tools {

static constexpr const char* TAG_PYTHON = "PythonTools";

std::vector<ToolDef> get_python_tools() {
    std::vector<ToolDef> tools;

    // ── run_python_script ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_python_script";
        t.description = "Execute a Python analytics script by name. Scripts live in the scripts/ directory and return JSON. Examples: yfinance_quote, backtesting, portfolio_analysis, economics_fred.";
        t.category = "analytics";
        t.input_schema.properties = {
            {"script", {{"type", "string"}, {"description", "Script name without .py (e.g. yfinance_quote)"}}},
            {"args", {{"type", "array"}, {"description", "Array of string arguments to pass to the script"}}}
        };
        t.input_schema.required = {"script"};
        t.handler = [](const json& args) -> ToolResult {
            std::string script = args.value("script", "");
            if (script.empty()) return ToolResult::fail("Missing 'script'");

            // Security: validate script name — alphanumeric, underscore, hyphen only
            for (char c : script) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
                    return ToolResult::fail("Invalid script name — only alphanumeric, underscore, hyphen allowed");
            }

            if (!python::is_python_available())
                return ToolResult::fail("Python is not available — run setup first");

            std::vector<std::string> script_args;
            if (args.contains("args") && args["args"].is_array()) {
                for (const auto& a : args["args"]) {
                    if (a.is_string())
                        script_args.push_back(a.get<std::string>());
                    else
                        script_args.push_back(a.dump());
                }
            }

            LOG_INFO(TAG_PYTHON, "Running script: %s with %zu args",
                     script.c_str(), script_args.size());

            try {
                auto result = python::execute(script, script_args);
                if (!result.success) {
                    return ToolResult::fail("Script failed: " + result.error);
                }

                // Try to parse output as JSON
                try {
                    auto data = json::parse(result.output);
                    return ToolResult::ok_data(data);
                } catch (...) {
                    // Return raw output if not JSON
                    return ToolResult::ok(result.output);
                }
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── list_python_scripts ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_python_scripts";
        t.description = "List all available Python analytics scripts.";
        t.category = "analytics";
        t.handler = [](const json&) -> ToolResult {
            try {
                namespace fs = std::filesystem;
                auto scripts_dir = python::get_exe_dir() / "scripts";

                if (!fs::exists(scripts_dir)) {
                    return ToolResult::fail("Scripts directory not found: " + scripts_dir.string());
                }

                json result = json::array();
                for (const auto& entry : fs::directory_iterator(scripts_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".py") {
                        std::string name = entry.path().stem().string();
                        result.push_back({
                            {"name", name},
                            {"filename", entry.path().filename().string()}
                        });
                    }
                }

                // Sort by name
                std::sort(result.begin(), result.end(),
                    [](const json& a, const json& b) {
                        return a["name"].get<std::string>() < b["name"].get<std::string>();
                    });

                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── check_python_status ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "check_python_status";
        t.description = "Check if Python is available and which venvs exist.";
        t.category = "analytics";
        t.handler = [](const json&) -> ToolResult {
            bool available = python::is_python_available();

            json result = {
                {"python_available", available},
                {"install_dir", python::get_install_dir().string()},
                {"exe_dir", python::get_exe_dir().string()}
            };

            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
