#pragma once
// Backtesting Tab — core types and enums

#include <string>
#include <vector>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace fincept::backtesting {

// =============================================================================
// Provider slugs — the 6 backtesting engines
// =============================================================================
enum class ProviderSlug {
    Fincept,        // Fincept Engine (400+ strategies)
    VectorBT,       // VectorBT Pro
    BacktestingPy,  // Backtesting.py
    FastTrade,      // Fast-Trade
    Zipline,        // Zipline-Reloaded
    BT,             // BT (portfolio backtesting)
    COUNT
};

// =============================================================================
// Command types — what the provider can do
// =============================================================================
enum class CommandType {
    Backtest,
    Optimize,
    WalkForward,
    Indicator,
    IndicatorSignals,
    Labels,
    Splits,
    Returns,
    Signals,
    COUNT
};

// =============================================================================
// Result view tabs
// =============================================================================
enum class ResultView {
    Summary,
    Metrics,
    Charts,
    Trades,
    Raw,
    COUNT
};

// =============================================================================
// Strategy parameter definition
// =============================================================================
struct StrategyParam {
    const char* name;
    const char* label;
    float default_val;
    float min_val;
    float max_val;
    float step;
};

// =============================================================================
// Strategy definition
// =============================================================================
struct Strategy {
    const char* id;
    const char* name;
    const char* description;
    std::vector<StrategyParam> params;
};

// =============================================================================
// Command definition (for command panel)
// =============================================================================
struct CommandDef {
    CommandType type;
    const char* label;
    const char* description;
    ImVec4 color;
};

// =============================================================================
// Category info
// =============================================================================
struct CategoryInfo {
    const char* key;
    const char* label;
    ImVec4 color;
};

// =============================================================================
// Provider configuration
// =============================================================================
struct ProviderConfig {
    ProviderSlug slug;
    const char* display_name;
    const char* script_dir;         // e.g. "vectorbt"
    ImVec4 color;
    std::vector<CommandType> commands;
    // command_map: CommandType -> python command string
    // strategies: grouped by category key
    // category_info: list of categories
};

// =============================================================================
// Indicator definition
// =============================================================================
struct IndicatorDef {
    const char* id;
    const char* label;
    std::vector<const char*> params;
};

// =============================================================================
// Position sizing option
// =============================================================================
struct PositionSizingOption {
    const char* id;
    const char* label;
};

// =============================================================================
// Signal mode
// =============================================================================
struct SignalMode {
    const char* id;
    const char* label;
    const char* description;
};

// =============================================================================
// Label type (ML)
// =============================================================================
struct LabelType {
    const char* id;
    const char* label;
    const char* description;
    std::vector<const char*> params;
};

// =============================================================================
// Splitter type (cross-validation)
// =============================================================================
struct SplitterType {
    const char* id;
    const char* label;
    const char* description;
    std::vector<const char*> params;
};

// =============================================================================
// Returns analysis type
// =============================================================================
struct ReturnsAnalysisType {
    const char* id;
    const char* label;
    const char* description;
};

// =============================================================================
// Signal generator type
// =============================================================================
struct SignalGeneratorType {
    const char* id;
    const char* label;
    const char* description;
    std::vector<const char*> params;
};

} // namespace fincept::backtesting
