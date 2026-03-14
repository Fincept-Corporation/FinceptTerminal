#pragma once
// Node Editor — Data types for nodes, links, workflows, and parameters
// Mirrors Tauri's node-editor/types.ts

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace fincept::node_editor {

// ============================================================================
// Connection types — define what kind of data flows through a link
// ============================================================================
enum class ConnectionType {
    Main,              // General-purpose data
    MarketData,        // OHLCV price data
    PortfolioData,     // Portfolio holdings/weights
    PriceData,         // Single price or price series
    FundamentalData,   // Financial statements, ratios
    TechnicalData,     // Indicator values
    NewsData,          // News articles, sentiment
    EconomicData,      // Economic indicators
    OptionsData,       // Options chain, greeks
    BacktestData,      // Backtest results
};

// Number of connection types
constexpr int CONNECTION_TYPE_COUNT = 10;

// Get display name for a connection type
inline const char* connection_type_name(ConnectionType ct) {
    switch (ct) {
        case ConnectionType::Main:            return "Main";
        case ConnectionType::MarketData:      return "Market Data";
        case ConnectionType::PortfolioData:   return "Portfolio";
        case ConnectionType::PriceData:       return "Price";
        case ConnectionType::FundamentalData: return "Fundamentals";
        case ConnectionType::TechnicalData:   return "Technical";
        case ConnectionType::NewsData:        return "News";
        case ConnectionType::EconomicData:    return "Economic";
        case ConnectionType::OptionsData:     return "Options";
        case ConnectionType::BacktestData:    return "Backtest";
    }
    return "Unknown";
}

// Get color for a connection type (for pin and link rendering)
inline ImU32 connection_type_color(ConnectionType ct) {
    switch (ct) {
        case ConnectionType::Main:            return IM_COL32(200, 200, 200, 255); // white-gray
        case ConnectionType::MarketData:      return IM_COL32( 34, 197,  94, 255); // green
        case ConnectionType::PortfolioData:   return IM_COL32(  6, 182, 212, 255); // cyan
        case ConnectionType::PriceData:       return IM_COL32( 59, 130, 246, 255); // blue
        case ConnectionType::FundamentalData: return IM_COL32(168,  85, 247, 255); // purple
        case ConnectionType::TechnicalData:   return IM_COL32(245, 158,  11, 255); // amber
        case ConnectionType::NewsData:        return IM_COL32(236,  72, 153, 255); // pink
        case ConnectionType::EconomicData:    return IM_COL32(234, 179,   8, 255); // yellow
        case ConnectionType::OptionsData:     return IM_COL32(249, 115,  22, 255); // orange
        case ConnectionType::BacktestData:    return IM_COL32(139,  92, 246, 255); // violet
    }
    return IM_COL32(128, 128, 128, 255);
}

// Check if two connection types are compatible for linking
inline bool connection_types_compatible(ConnectionType out, ConnectionType in) {
    // Main is universal — connects to anything
    if (out == ConnectionType::Main || in == ConnectionType::Main) return true;
    // Same type always connects
    if (out == in) return true;
    // MarketData is compatible with PriceData and TechnicalData
    if (out == ConnectionType::MarketData &&
        (in == ConnectionType::PriceData || in == ConnectionType::TechnicalData))
        return true;
    // PriceData can feed into MarketData consumers
    if (out == ConnectionType::PriceData && in == ConnectionType::MarketData)
        return true;
    // BacktestData can feed into PortfolioData
    if (out == ConnectionType::BacktestData && in == ConnectionType::PortfolioData)
        return true;
    return false;
}

// ============================================================================
// Pin definition for NodeDef (template-level pin specification)
// ============================================================================
struct PinDef {
    std::string name;
    ConnectionType conn_type = ConnectionType::Main;
};

// ============================================================================
// Parameter value (string | int | double | bool)
// ============================================================================
using ParamValue = std::variant<std::string, int, double, bool>;

inline nlohmann::json param_to_json(const ParamValue& v) {
    return std::visit([](auto&& val) -> nlohmann::json { return val; }, v);
}

inline ParamValue param_from_json(const nlohmann::json& j) {
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer()) return j.get<int>();
    if (j.is_number_float()) return j.get<double>();
    return j.get<std::string>();
}

// ============================================================================
// Node parameter definition
// ============================================================================
struct NodeParam {
    std::string name;
    std::string display_name;
    std::string type;  // "string", "number", "boolean", "options"
    ParamValue default_value;
    std::vector<std::string> options;
    bool required = false;
    std::string description;
};

// ============================================================================
// Pin (connection point on a node)
// ============================================================================
struct Pin {
    int id = 0;
    std::string name;
    ConnectionType conn_type = ConnectionType::Main;
    ImU32 color = IM_COL32(255, 136, 0, 255);
};

// ============================================================================
// Node definition (template — what kind of node this is)
// ============================================================================
struct NodeDef {
    std::string type;
    std::string label;
    std::string category;
    ImU32 color;
    std::string description;
    int num_inputs = 0;
    int num_outputs = 0;
    std::vector<NodeParam> default_params;

    // Typed pin definitions (optional — if empty, uses Main type for all pins)
    std::vector<PinDef> input_defs;
    std::vector<PinDef> output_defs;

    // Get connection type for an input pin by index
    ConnectionType input_type(int index) const {
        if (index >= 0 && index < (int)input_defs.size()) return input_defs[index].conn_type;
        return ConnectionType::Main;
    }
    // Get connection type for an output pin by index
    ConnectionType output_type(int index) const {
        if (index >= 0 && index < (int)output_defs.size()) return output_defs[index].conn_type;
        return ConnectionType::Main;
    }
};

// ============================================================================
// Node instance (placed on canvas)
// ============================================================================
struct NodeInstance {
    int id = 0;
    std::string type;
    std::string label;
    ImVec2 position{0, 0};
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    std::map<std::string, ParamValue> params;

    // Runtime state
    std::string status = "idle";  // idle, running, completed, error
    std::string result;
    std::string error;
};

// ============================================================================
// Link (connection between an output pin and an input pin)
// ============================================================================
struct Link {
    int id = 0;
    int start_pin = 0;  // output pin id
    int end_pin = 0;    // input pin id
};

// ============================================================================
// Workflow (complete saveable state)
// ============================================================================
struct Workflow {
    std::string id;
    std::string name;
    std::string description;
    std::string status = "new";  // new, draft, deployed, completed
    std::vector<NodeInstance> nodes;
    std::vector<Link> links;
    std::string created_at;
    std::string updated_at;

    nlohmann::json to_json() const;
    static Workflow from_json(const nlohmann::json& j);
};

// ============================================================================
// Category definition (for palette grouping)
// ============================================================================
struct CategoryDef {
    std::string name;
    ImU32 color;
};

// ============================================================================
// Undo/Redo snapshot
// ============================================================================
struct EditorSnapshot {
    std::vector<NodeInstance> nodes;
    std::vector<Link> links;
};

} // namespace fincept::node_editor
