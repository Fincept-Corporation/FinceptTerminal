#pragma once
// Navigation MCP Tools — tab switching and screen navigation
// Registers tools that allow AI to navigate between terminal screens.

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Tab IDs matching App::active_tab_ indices (from app.cpp render())
enum class TabId : int {
    Dashboard        = 0,
    Markets          = 1,
    CryptoTrading    = 2,
    Portfolio        = 3,
    News             = 4,
    AIChat           = 5,
    Backtesting      = 6,
    AlgoTrading      = 7,
    NodeEditor       = 8,
    CodeEditor       = 9,
    // 10 unused
    QuantLib         = 11,
    Settings         = 12,
    Profile          = 13,
    SurfaceAnalytics = 14,
    Geopolitics      = 15,
    Watchlist        = 16,
    Economics        = 17,
    DBnomics         = 18,
    Notes            = 19,
    DataSources      = 20,
    MNA              = 21,
    Forum            = 22,
    Docs             = 23,
    Support          = 24,
    About            = 25,
    GovData          = 26,
    AgentStudio      = 27
};

/// Get all navigation tool definitions
std::vector<ToolDef> get_navigation_tools();

} // namespace fincept::mcp::tools
