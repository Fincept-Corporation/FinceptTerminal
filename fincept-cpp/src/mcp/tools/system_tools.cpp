// System MCP Tools — auth status, cache, app info

#include "system_tools.h"
#include "../mcp_provider.h"
#include "../../auth/auth_manager.h"
#include "../../storage/database.h"
#include "../../python/python_runner.h"
#include "../../core/logger.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_system_tools() {
    std::vector<ToolDef> tools;

    // ── get_auth_status ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_auth_status";
        t.description = "Check if the user is logged in and get basic account info.";
        t.category = "system";
        t.handler = [](const json&) -> ToolResult {
            auto& am = auth::AuthManager::instance();
            if (!am.has_session() || !am.is_authenticated()) {
                return ToolResult::ok_data({
                    {"authenticated", false},
                    {"user_type", "none"}
                });
            }
            auto& s = am.session();
            return ToolResult::ok_data({
                {"authenticated", true},
                {"user_type", s.user_type},
                {"username", s.user_info.username},
                {"email", s.user_info.email},
                {"is_guest", s.is_guest()},
                {"is_verified", s.user_info.is_verified},
                {"credit_balance", s.credit_balance},
                {"has_subscription", s.has_subscription}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_cache_stats ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_cache_stats";
        t.description = "Get cache statistics: total entries, size, expired count, per-category breakdown.";
        t.category = "system";
        t.handler = [](const json&) -> ToolResult {
            try {
                auto stats = db::ops::cache_stats();
                json cats = json::array();
                for (const auto& c : stats.categories) {
                    cats.push_back({
                        {"category", c.category},
                        {"entries", c.entry_count},
                        {"size_bytes", c.total_size}
                    });
                }
                return ToolResult::ok_data({
                    {"total_entries", stats.total_entries},
                    {"total_size_bytes", stats.total_size_bytes},
                    {"expired_entries", stats.expired_entries},
                    {"categories", cats}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── clear_cache ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "clear_cache";
        t.description = "Clean up expired cache entries and return how many were removed.";
        t.category = "system";
        t.handler = [](const json&) -> ToolResult {
            try {
                int64_t removed = db::ops::cache_cleanup();
                return ToolResult::ok("Cache cleaned", {{"entries_removed", removed}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── invalidate_cache_category ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "invalidate_cache_category";
        t.description = "Invalidate all cache entries in a specific category (e.g. market-quotes, news, research).";
        t.category = "system";
        t.input_schema.properties = {
            {"category", {{"type", "string"}, {"description", "Cache category to invalidate"}}}
        };
        t.input_schema.required = {"category"};
        t.handler = [](const json& args) -> ToolResult {
            std::string category = args.value("category", "");
            if (category.empty()) return ToolResult::fail("Missing 'category'");

            try {
                int64_t removed = db::ops::cache_invalidate_category(category);
                return ToolResult::ok("Category invalidated", {
                    {"category", category}, {"entries_removed", removed}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_app_info ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_app_info";
        t.description = "Get application version, platform, number of MCP tools, and Python availability.";
        t.category = "system";
        t.handler = [](const json&) -> ToolResult {
            return ToolResult::ok_data({
                {"version", "4.0.0"},
                {"platform",
#ifdef _WIN32
                    "windows"
#elif __APPLE__
                    "macos"
#else
                    "linux"
#endif
                },
                {"internal_tools", MCPProvider::instance().tool_count()},
                {"python_available", python::is_python_available()}
            });
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
