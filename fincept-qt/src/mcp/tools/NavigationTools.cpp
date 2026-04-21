// NavigationTools.cpp — Tab switching MCP tools (Qt port)

#include "mcp/tools/NavigationTools.h"

#include "core/config/AppConfig.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"

#include <QVariantMap>

#include <algorithm>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "NavTools";

struct TabEntry {
    const char* name;
    const char* display_name;
    int index;
};

// Indices must match ScreenRouter screen IDs used in MainWindow
static const TabEntry TAB_MAP[] = {
    {"dashboard", "Dashboard", 0},
    {"markets", "Markets", 1},
    {"crypto_trading", "Crypto Trading", 2},
    {"news", "News", 3},
    {"watchlist", "Watchlist", 4},
    {"dbnomics", "DBnomics", 5},
    {"surface_analytics", "Surface Analytics", 6},
    {"notes", "Notes", 7},
    {"report_builder", "Report Builder", 8},
    {"profile", "Profile", 9},
    {"settings", "Settings", 10},
    {"about", "About", 11},
    {"support", "Support", 12},
    {"ai_chat", "AI Chat", 13},
    {"agent_config", "Agent Studio", 14},
    {"economics", "Economics", 15},
    {"geopolitics", "Geopolitics", 16},
    {"quantlib", "QuantLib Suite", 17},
    {"algo_trading", "Algo Trading", 18},
    {"backtesting", "Backtesting", 19},
    {"node_editor", "Node Editor", 20},
    {"code_editor", "Code Editor", 21},
    {"ma_analytics", "M&A Analytics", 22},
    {"data_sources", "Data Sources", 23},
    {"gov_data", "Government Data", 24},
    {"forum", "Forum", 25},
    {"docs", "Documentation", 26},
};

static constexpr int TAB_COUNT = static_cast<int>(sizeof(TAB_MAP) / sizeof(TAB_MAP[0]));

static bool tab_enabled(const TabEntry& entry) {
    if (QString::fromLatin1(entry.name) == QLatin1String("crypto_trading"))
        return fincept::AppConfig::instance().crypto_markets_enabled();
    return true;
}

static int find_tab_index(const QString& name) {
    QString lower = name.toLower();

    // Exact match on id
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (!tab_enabled(TAB_MAP[i]))
            continue;
        if (lower == TAB_MAP[i].name)
            return TAB_MAP[i].index;
    }

    // Partial match on display name
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (!tab_enabled(TAB_MAP[i]))
            continue;
        if (QString(TAB_MAP[i].display_name).toLower().contains(lower))
            return TAB_MAP[i].index;
    }

    return -1;
}

static QString find_tab_name(int idx) {
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (!tab_enabled(TAB_MAP[i]))
            continue;
        if (TAB_MAP[i].index == idx)
            return TAB_MAP[i].name;
    }
    return {};
}

std::vector<ToolDef> get_navigation_tools() {
    std::vector<ToolDef> tools;

    // ── navigate_to_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "navigate_to_tab";
        t.description = "Navigate to a specific terminal screen by name. "
                        "Use list_tabs to discover currently enabled screens.";
        t.category = "navigation";
        t.input_schema.properties =
            QJsonObject{{"tab", QJsonObject{{"type", "string"}, {"description", "Screen name to navigate to"}}}};
        t.input_schema.required = {"tab"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString tab = args["tab"].toString();
            if (tab.isEmpty())
                return ToolResult::fail("Missing 'tab' parameter");

            int idx = find_tab_index(tab);
            if (idx < 0) {
                QString valid;
                for (int i = 0; i < TAB_COUNT; ++i) {
                    if (!tab_enabled(TAB_MAP[i]))
                        continue;
                    if (!valid.isEmpty())
                        valid += ", ";
                    valid += TAB_MAP[i].name;
                }
                return ToolResult::fail("Unknown tab '" + tab + "'. Valid tabs: " + valid);
            }

            // Publish event — NavigationBar listens for this
            EventBus::instance().publish("nav.switch_screen",
                                         QVariantMap{{"screen_id", find_tab_name(idx)}, {"tab_index", idx}});

            LOG_INFO(TAG, "Navigate to tab: " + tab);
            return ToolResult::ok("Navigated to " + tab, QJsonObject{{"tab_index", idx}, {"tab", tab}});
        };
        tools.push_back(std::move(t));
    }

    // ── list_tabs ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_tabs";
        t.description = "List all available terminal screens with their names and indices.";
        t.category = "navigation";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray tabs;
            for (int i = 0; i < TAB_COUNT; ++i) {
                if (!tab_enabled(TAB_MAP[i]))
                    continue;
                tabs.append(QJsonObject{
                    {"name", TAB_MAP[i].name}, {"display_name", TAB_MAP[i].display_name}, {"index", TAB_MAP[i].index}});
            }
            return ToolResult::ok_data(tabs);
        };
        tools.push_back(std::move(t));
    }

    // ── get_current_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_current_tab";
        t.description = "Query the currently active screen (publishes event, screen responds).";
        t.category = "navigation";
        t.handler = [](const QJsonObject&) -> ToolResult {
            EventBus::instance().publish("nav.get_current_screen", {});
            return ToolResult::ok("Current screen query sent via event bus");
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
