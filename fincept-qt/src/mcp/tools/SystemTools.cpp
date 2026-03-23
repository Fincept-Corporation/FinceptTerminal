// SystemTools.cpp — Auth status, cache, app info (Qt port)

#include "mcp/tools/SystemTools.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_system_tools() {
    std::vector<ToolDef> tools;

    // ── get_auth_status ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_auth_status";
        t.description = "Check if the user is logged in and get basic account info.";
        t.category = "system";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& am = auth::AuthManager::instance();
            if (!am.is_authenticated()) {
                return ToolResult::ok_data(QJsonObject{{"authenticated", false}, {"user_type", "none"}});
            }
            const auto& s = am.session();
            return ToolResult::ok_data(QJsonObject{{"authenticated", true},
                                                   {"username", s.user_info.username},
                                                   {"email", s.user_info.email},
                                                   {"account_type", s.user_info.account_type},
                                                   {"is_verified", s.user_info.is_verified},
                                                   {"credit_balance", s.user_info.credit_balance},
                                                   {"has_subscription", s.has_subscription}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_cache_stats ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_cache_stats";
        t.description = "Get cache statistics: total number of cached entries.";
        t.category = "system";
        t.handler = [](const QJsonObject&) -> ToolResult {
            return ToolResult::ok_data(QJsonObject{{"total_entries", CacheManager::instance().entry_count()}});
        };
        tools.push_back(std::move(t));
    }

    // ── clear_cache ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "clear_cache";
        t.description = "Clear all cache entries.";
        t.category = "system";
        t.handler = [](const QJsonObject&) -> ToolResult {
            CacheManager::instance().clear();
            return ToolResult::ok("Cache cleared");
        };
        tools.push_back(std::move(t));
    }

    // ── get_app_info ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_app_info";
        t.description = "Get application version, platform, number of MCP tools, and Python availability.";
        t.category = "system";
        t.handler = [](const QJsonObject&) -> ToolResult {
            return ToolResult::ok_data(
                QJsonObject{{"version", "4.0.0"},
                            {"platform",
#ifdef _WIN32
                             "windows"
#elif defined(__APPLE__)
                             "macos"
#else
                             "linux"
#endif
                            },
                            {"internal_tools", static_cast<int>(McpProvider::instance().tool_count())},
                            {"python_available", python::PythonRunner::instance().is_available()}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
