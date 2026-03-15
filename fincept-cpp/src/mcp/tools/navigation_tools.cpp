// Navigation MCP Tools — tab switching implementation

#include "navigation_tools.h"
#include "../../core/event_bus.h"
#include "../../core/logger.h"
#include <algorithm>

namespace fincept::mcp::tools {

static constexpr const char* TAG_NAV = "NavTools";

// Tab name → tab index mapping
struct TabEntry {
    const char* name;
    const char* display_name;
    int index;
};

// Indices must match App::active_tab_ in app.cpp render()
static const TabEntry TAB_MAP[] = {
    {"dashboard",         "Dashboard",          0},
    {"markets",           "Markets",            1},
    {"crypto_trading",    "Crypto Trading",      2},
    {"portfolio",         "Portfolio",           3},
    {"news",              "News",               4},
    {"ai_chat",           "AI Chat",             5},
    {"backtesting",       "Backtesting",         6},
    {"algo_trading",      "Algo Trading",        7},
    {"node_editor",       "Node Editor",         8},
    {"code_editor",       "Code Editor",         9},
    {"quantlib",          "QuantLib",            11},
    {"settings",          "Settings",            12},
    {"profile",           "Profile",             13},
    {"surface_analytics", "Surface Analytics",   14},
    {"geopolitics",       "Geopolitics",         15},
    {"watchlist",         "Watchlist",           16},
    {"economics",         "Economics",           17},
    {"dbnomics",          "DBnomics",            18},
    {"notes",             "Notes",               19},
    {"data_sources",      "Data Sources",        20},
    {"mna",               "M&A Analytics",       21},
    {"forum",             "Forum",               22},
    {"docs",              "Documentation",       23},
    {"support",           "Support",             24},
    {"about",             "About",               25},
    {"gov_data",          "Government Data",     26},
    {"agent_studio",      "Agent Studio",        27},
};

static constexpr int TAB_COUNT = sizeof(TAB_MAP) / sizeof(TAB_MAP[0]);

static int find_tab_index(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Exact match on id
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (lower == TAB_MAP[i].name) return TAB_MAP[i].index;
    }

    // Partial / fuzzy match on display name
    for (int i = 0; i < TAB_COUNT; ++i) {
        std::string display = TAB_MAP[i].display_name;
        std::transform(display.begin(), display.end(), display.begin(), ::tolower);
        if (display.find(lower) != std::string::npos) return TAB_MAP[i].index;
    }

    return -1;
}

std::vector<ToolDef> get_navigation_tools() {
    std::vector<ToolDef> tools;

    // ── navigate_to_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "navigate_to_tab";
        t.description = "Navigate to a specific terminal tab by name. Available tabs: dashboard, markets, news, watchlist, research, portfolio, crypto_trading, algo_trading, backtesting, economics, dbnomics, geopolitics, surface_analytics, quantlib, mna, agent_studio, ai_chat, node_editor, code_editor, notes, data_sources, gov_data, forum, profile, settings, docs, support, about.";
        t.category = "navigation";
        t.input_schema.properties = {
            {"tab", {
                {"type", "string"},
                {"description", "Tab name to navigate to"}
            }}
        };
        t.input_schema.required = {"tab"};
        t.handler = [](const json& args) -> ToolResult {
            std::string tab = args.value("tab", "");
            if (tab.empty()) return ToolResult::fail("Missing 'tab' parameter");

            int idx = find_tab_index(tab);
            if (idx < 0) {
                // Build list of valid tabs
                std::string valid;
                for (int i = 0; i < TAB_COUNT; ++i) {
                    if (!valid.empty()) valid += ", ";
                    valid += TAB_MAP[i].name;
                }
                return ToolResult::fail("Unknown tab '" + tab + "'. Valid tabs: " + valid);
            }

            core::EventBus::instance().publish("nav.switch_tab", {{"tab_index", idx}});
            LOG_INFO(TAG_NAV, "Navigating to tab: %s (index %d)", tab.c_str(), idx);
            return ToolResult::ok("Navigated to " + tab, {{"tab_index", idx}});
        };
        tools.push_back(std::move(t));
    }

    // ── list_tabs ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_tabs";
        t.description = "List all available terminal tabs with their names and indices.";
        t.category = "navigation";
        t.handler = [](const json&) -> ToolResult {
            json tabs = json::array();
            for (int i = 0; i < TAB_COUNT; ++i) {
                tabs.push_back({
                    {"name", TAB_MAP[i].name},
                    {"display_name", TAB_MAP[i].display_name},
                    {"index", TAB_MAP[i].index}
                });
            }
            return ToolResult::ok_data(tabs);
        };
        tools.push_back(std::move(t));
    }

    // ── get_current_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_current_tab";
        t.description = "Get the currently active tab name and index.";
        t.category = "navigation";
        t.handler = [](const json&) -> ToolResult {
            // The actual tab index is managed by App, so we publish an event to request it
            // For now, we return a message indicating the event was sent
            core::EventBus::instance().publish("nav.get_current_tab", {});
            return ToolResult::ok("Current tab query sent — check response via event bus");
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
