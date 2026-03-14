#pragma once
// Backtesting Tab — provider configs, strategies, colors, UI helpers

#include "backtesting_types.h"
#include "ui/theme.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <ctime>

namespace fincept::backtesting {

// =============================================================================
// Provider accent colors
// =============================================================================
namespace provider_colors {
    constexpr ImVec4 FINCEPT      = {1.0f, 0.533f, 0.0f, 1.0f};    // #FF8800 orange
    constexpr ImVec4 VECTORBT     = {0.0f, 0.898f, 1.0f, 1.0f};    // #00E5FF cyan
    constexpr ImVec4 BACKTESTINGPY= {0.0f, 0.839f, 0.435f, 1.0f};  // #00D66F green
    constexpr ImVec4 FASTTRADE    = {1.0f, 0.843f, 0.0f, 1.0f};    // #FFD700 yellow
    constexpr ImVec4 ZIPLINE      = {0.914f, 0.118f, 0.388f, 1.0f}; // #E91E63 pink
    constexpr ImVec4 BT           = {1.0f, 0.420f, 0.208f, 1.0f};  // #FF6B35 orange-red
}

inline ImVec4 provider_color(ProviderSlug s) {
    switch (s) {
        case ProviderSlug::Fincept:       return provider_colors::FINCEPT;
        case ProviderSlug::VectorBT:      return provider_colors::VECTORBT;
        case ProviderSlug::BacktestingPy: return provider_colors::BACKTESTINGPY;
        case ProviderSlug::FastTrade:     return provider_colors::FASTTRADE;
        case ProviderSlug::Zipline:       return provider_colors::ZIPLINE;
        case ProviderSlug::BT:            return provider_colors::BT;
        default: return provider_colors::FINCEPT;
    }
}

// =============================================================================
// Provider display names and script directories
// =============================================================================
struct ProviderInfo {
    ProviderSlug slug;
    const char* display_name;
    const char* script_dir;  // folder under Analytics/backtesting/
};

inline const std::vector<ProviderInfo>& provider_list() {
    static const std::vector<ProviderInfo> p = {
        {ProviderSlug::Fincept,       "FINCEPT ENGINE", "fincept"},
        {ProviderSlug::VectorBT,      "VECTORBT",       "vectorbt"},
        {ProviderSlug::BacktestingPy, "BACKTESTING.PY", "backtestingpy"},
        {ProviderSlug::FastTrade,     "FAST-TRADE",     "fasttrade"},
        {ProviderSlug::Zipline,       "ZIPLINE",        "zipline"},
        {ProviderSlug::BT,            "BT",             "bt"},
    };
    return p;
}

inline const ProviderInfo& get_provider_info(ProviderSlug s) {
    for (auto& p : provider_list()) {
        if (p.slug == s) return p;
    }
    return provider_list()[0];
}

// =============================================================================
// Command definitions
// =============================================================================
inline const std::vector<CommandDef>& all_commands() {
    using namespace theme::colors;
    static const std::vector<CommandDef> c = {
        {CommandType::Backtest,         "Backtest",       "Run strategy backtest",          ACCENT},
        {CommandType::Optimize,         "Optimize",       "Parameter optimization",         SUCCESS},
        {CommandType::WalkForward,      "Walk Forward",   "Walk-forward validation",        INFO},
        {CommandType::Indicator,        "Indicators",     "Calculate indicators",           {0.616f, 0.306f, 0.871f, 1.0f}},
        {CommandType::IndicatorSignals, "Ind. Signals",   "Indicator-based signals",        WARNING},
        {CommandType::Labels,           "ML Labels",      "Generate ML labels",             {0.0f, 0.898f, 1.0f, 1.0f}},
        {CommandType::Splits,           "Splitters",      "Cross-validation splits",        INFO},
        {CommandType::Returns,          "Returns",        "Returns analysis",               SUCCESS},
        {CommandType::Signals,          "Rand. Signals",  "Random signal generators",       WARNING},
    };
    return c;
}

// =============================================================================
// Commands per provider
// =============================================================================
inline std::vector<CommandType> provider_commands(ProviderSlug s) {
    switch (s) {
        case ProviderSlug::VectorBT:
            return {CommandType::Backtest, CommandType::Optimize, CommandType::WalkForward,
                    CommandType::Indicator, CommandType::IndicatorSignals,
                    CommandType::Labels, CommandType::Splits, CommandType::Returns, CommandType::Signals};
        case ProviderSlug::BacktestingPy:
            return {CommandType::Backtest, CommandType::Optimize, CommandType::WalkForward,
                    CommandType::Indicator};
        case ProviderSlug::FastTrade:
            return {CommandType::Backtest};
        case ProviderSlug::Zipline:
            return {CommandType::Backtest, CommandType::Optimize, CommandType::WalkForward,
                    CommandType::Indicator, CommandType::IndicatorSignals};
        case ProviderSlug::BT:
            return {CommandType::Backtest, CommandType::Optimize, CommandType::WalkForward,
                    CommandType::Indicator, CommandType::IndicatorSignals};
        case ProviderSlug::Fincept:
            return {CommandType::Backtest};
        default:
            return {CommandType::Backtest};
    }
}

// =============================================================================
// Command -> Python action string mapping
// =============================================================================
inline const char* command_to_python(ProviderSlug /*provider*/, CommandType cmd) {
    switch (cmd) {
        case CommandType::Backtest:         return "run_backtest";
        case CommandType::Optimize:         return "optimize";
        case CommandType::WalkForward:      return "walk_forward";
        case CommandType::Indicator:        return "indicator_param_sweep";
        case CommandType::IndicatorSignals: return "indicator_signals";
        case CommandType::Labels:           return "generate_labels";
        case CommandType::Splits:           return "generate_splits";
        case CommandType::Returns:          return "analyze_returns";
        case CommandType::Signals:          return "generate_signals";
        default: return "run_backtest";
    }
}

inline const char* command_label(CommandType cmd) {
    for (auto& c : all_commands()) {
        if (c.type == cmd) return c.label;
    }
    return "Backtest";
}

// =============================================================================
// Strategy categories
// =============================================================================
inline const std::vector<CategoryInfo>& strategy_categories() {
    static const std::vector<CategoryInfo> cats = {
        {"trend",          "Trend Following",  {0.0f, 0.533f, 1.0f, 1.0f}},
        {"meanReversion",  "Mean Reversion",   {0.616f, 0.306f, 0.871f, 1.0f}},
        {"momentum",       "Momentum",         {1.0f, 0.843f, 0.0f, 1.0f}},
        {"breakout",       "Breakout",         {0.0f, 0.839f, 0.435f, 1.0f}},
        {"multiIndicator", "Multi-Indicator",  {0.0f, 0.898f, 1.0f, 1.0f}},
        {"other",          "Other",            {0.471f, 0.471f, 0.471f, 1.0f}},
        {"custom",         "Custom",           {1.0f, 0.533f, 0.0f, 1.0f}},
        {"portfolio",      "Portfolio Alloc.",  {1.0f, 0.533f, 0.0f, 1.0f}},
        {"pipeline",       "Pipeline",         {1.0f, 0.533f, 0.0f, 1.0f}},
    };
    return cats;
}

// =============================================================================
// Shared strategies (used by multiple providers)
// =============================================================================
inline const std::vector<Strategy>& shared_trend() {
    static const std::vector<Strategy> s = {
        {"sma_crossover", "SMA Crossover", "Fast SMA crosses Slow SMA",
         {{"fastPeriod","Fast Period",10,2,100,1}, {"slowPeriod","Slow Period",20,5,200,1}}},
        {"ema_crossover", "EMA Crossover", "Fast EMA crosses Slow EMA",
         {{"fastPeriod","Fast Period",10,2,100,1}, {"slowPeriod","Slow Period",20,5,200,1}}},
        {"macd", "MACD Crossover", "MACD line crosses Signal line",
         {{"fastPeriod","Fast Period",12,2,50,1}, {"slowPeriod","Slow Period",26,10,100,1},
          {"signalPeriod","Signal Period",9,2,50,1}}},
        {"adx_trend", "ADX Trend Filter", "ADX-based trend following",
         {{"adxPeriod","ADX Period",14,5,50,1}, {"adxThreshold","ADX Threshold",25,10,50,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& shared_mean_reversion() {
    static const std::vector<Strategy> s = {
        {"mean_reversion", "Z-Score Reversion", "Z-score mean reversion",
         {{"window","Window",20,5,100,1}, {"threshold","Z-Score Threshold",2.0f,0.5f,5.0f,0.1f}}},
        {"bollinger_bands", "Bollinger Bands", "Bollinger band mean reversion",
         {{"period","Period",20,5,100,1}, {"stdDev","Std Dev",2.0f,1.0f,4.0f,0.1f}}},
        {"rsi", "RSI Mean Reversion", "RSI oversold/overbought",
         {{"period","Period",14,2,50,1}, {"oversold","Oversold",30,10,40,1}, {"overbought","Overbought",70,60,90,1}}},
        {"stochastic", "Stochastic", "Stochastic oscillator",
         {{"kPeriod","K Period",14,5,50,1}, {"dPeriod","D Period",3,2,20,1},
          {"oversold","Oversold",20,10,30,1}, {"overbought","Overbought",80,70,90,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& shared_momentum() {
    static const std::vector<Strategy> s = {
        {"momentum", "Momentum (ROC)", "Rate of change momentum",
         {{"period","Period",12,5,50,1}, {"threshold","Threshold",0.02f,0.01f,0.1f,0.01f}}},
    };
    return s;
}

inline const std::vector<Strategy>& shared_breakout() {
    static const std::vector<Strategy> s = {
        {"breakout", "Donchian Breakout", "Donchian channel breakout",
         {{"period","Period",20,5,100,1}}},
    };
    return s;
}

// =============================================================================
// Provider-specific extra strategies
// =============================================================================
inline const std::vector<Strategy>& vbt_extra_trend() {
    static const std::vector<Strategy> s = {
        {"keltner_breakout", "Keltner Channel", "Keltner channel breakout",
         {{"period","Period",20,5,100,1}, {"atrMultiplier","ATR Mult",2.0f,0.5f,5.0f,0.1f}}},
        {"triple_ma", "Triple MA", "Three moving average system",
         {{"fastPeriod","Fast",10,2,50,1}, {"mediumPeriod","Medium",20,5,100,1}, {"slowPeriod","Slow",50,10,200,1}}},
        {"williams_r", "Williams %R", "Williams %R trend indicator",
         {{"period","Period",14,5,50,1}, {"oversold","Oversold",-80,-90,-70,1}, {"overbought","Overbought",-20,-30,-10,1}}},
        {"cci", "CCI", "Commodity Channel Index trend",
         {{"period","Period",20,5,50,1}, {"oversold","Oversold",-100,-200,-50,1}, {"overbought","Overbought",100,50,200,1}}},
        {"obv_trend", "OBV Trend", "On-Balance Volume trend",
         {{"maPeriod","MA Period",20,5,100,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& vbt_extra_momentum() {
    static const std::vector<Strategy> s = {
        {"dual_momentum", "Dual Momentum", "Absolute + relative momentum",
         {{"absolutePeriod","Absolute",12,3,24,1}, {"relativePeriod","Relative",12,3,24,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& vbt_extra_breakout() {
    static const std::vector<Strategy> s = {
        {"volatility_breakout", "Volatility Breakout", "ATR-based volatility breakout",
         {{"atrPeriod","ATR Period",14,5,50,1}, {"atrMultiplier","ATR Mult",2.0f,0.5f,5.0f,0.1f}}},
        {"donchian_breakout", "Donchian Breakout", "Donchian channel breakout",
         {{"period","Period",20,5,100,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& vbt_multi_indicator() {
    static const std::vector<Strategy> s = {
        {"rsi_macd", "RSI + MACD", "RSI and MACD confluence",
         {{"rsiPeriod","RSI Period",14,5,30,1}, {"macdFast","MACD Fast",12,5,30,1}, {"macdSlow","MACD Slow",26,10,50,1}}},
        {"macd_adx", "MACD + ADX Filter", "MACD with ADX trend filter",
         {{"macdFast","MACD Fast",12,5,30,1}, {"macdSlow","MACD Slow",26,10,50,1}, {"adxPeriod","ADX Period",14,5,30,1}}},
        {"bollinger_rsi", "Bollinger + RSI", "Bollinger bands with RSI",
         {{"bbPeriod","BB Period",20,10,50,1}, {"rsiPeriod","RSI Period",14,5,30,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& vbt_custom() {
    static const std::vector<Strategy> s = {
        {"code", "Custom Code", "Python custom strategy", {}},
    };
    return s;
}

// BT portfolio strategies
inline const std::vector<Strategy>& bt_portfolio() {
    static const std::vector<Strategy> s = {
        {"equal_weight", "Equal Weight", "Equal-weight allocation across all assets",
         {{"rebalancePeriod","Rebalance Period",21,5,63,1}}},
        {"inv_vol", "Inverse Volatility", "Weight assets inversely by volatility",
         {{"lookback","Lookback (days)",20,10,120,1}}},
        {"mean_var", "Mean-Variance", "Markowitz mean-variance optimized portfolio",
         {{"lookback","Lookback (days)",60,20,252,1}}},
        {"risk_parity", "Risk Parity", "Equal risk contribution (ERC) portfolio",
         {{"lookback","Lookback (days)",60,20,252,1}}},
        {"target_vol", "Target Volatility", "Target a specific portfolio volatility",
         {{"targetVol","Target Vol (%)",10,1,50,1}, {"lookback","Lookback (days)",20,10,120,1}}},
        {"min_var", "Minimum Variance", "Minimum variance portfolio (lowest risk)",
         {{"lookback","Lookback (days)",60,20,252,1}}},
    };
    return s;
}

inline const std::vector<Strategy>& bt_extra_momentum() {
    static const std::vector<Strategy> s = {
        {"momentum_topn", "Momentum Top-N", "Select top N assets by momentum",
         {{"topN","Top N",5,1,50,1}, {"lookback","Momentum Lookback",60,10,252,1}}},
        {"momentum_inv_vol", "Momentum + Inv Vol", "Top-N momentum with inverse vol weighting",
         {{"topN","Top N",5,1,50,1}, {"lookback","Lookback",60,10,252,1}}},
    };
    return s;
}

// Zipline pipeline strategies
inline const std::vector<Strategy>& zipline_pipeline() {
    static const std::vector<Strategy> s = {
        {"pipeline", "Custom Pipeline", "Factor-based screening and ranking",
         {{"topN","Top N",5,1,50,1}, {"rebalanceDays","Rebalance Days",21,5,63,1}}},
        {"scheduled_rebalance", "Scheduled Rebalance", "Monthly rebalance using schedule_function",
         {{"momentumWindow","Momentum Window",63,21,252,1}}},
    };
    return s;
}

// FastTrade extra
inline const std::vector<Strategy>& fasttrade_extra_trend() {
    static const std::vector<Strategy> s = {
        {"ichimoku", "Ichimoku Cloud", "Ichimoku Kinko Hyo trend system",
         {{"tenkanPeriod","Tenkan",9,5,30,1}, {"kijunPeriod","Kijun",26,10,60,1}, {"senkouBPeriod","Senkou B",52,20,120,1}}},
        {"psar", "Parabolic SAR", "Parabolic stop and reverse",
         {{"afStep","AF Step",0.02f,0.01f,0.1f,0.01f}, {"afMax","AF Max",0.2f,0.1f,0.5f,0.01f}}},
        {"ma_ribbon", "MA Ribbon", "Multiple moving average ribbon",
         {{"shortestPeriod","Shortest MA",5,2,20,1}, {"longestPeriod","Longest MA",50,20,200,1}, {"count","MA Count",6,3,12,1}}},
        {"keltner_channel", "Keltner Channel", "Keltner channel trend system",
         {{"period","EMA Period",20,5,100,1}, {"atrMultiplier","ATR Mult",2.0f,0.5f,5.0f,0.1f}}},
    };
    return s;
}

inline const std::vector<Strategy>& fasttrade_extra_multi() {
    static const std::vector<Strategy> s = {
        {"trend_momentum", "Trend + Momentum", "MA trend filter with RSI momentum",
         {{"maPeriod","MA Period",50,10,200,1}, {"rsiPeriod","RSI Period",14,5,30,1}, {"rsiThreshold","RSI Threshold",50,30,70,1}}},
    };
    return s;
}

// =============================================================================
// Get strategies for provider + category
// =============================================================================
inline std::vector<Strategy> get_strategies(ProviderSlug provider, const std::string& category) {
    std::vector<Strategy> result;

    auto append = [&](const std::vector<Strategy>& src) {
        result.insert(result.end(), src.begin(), src.end());
    };

    if (category == "trend") {
        append(shared_trend());
        if (provider == ProviderSlug::VectorBT) append(vbt_extra_trend());
        if (provider == ProviderSlug::FastTrade) append(fasttrade_extra_trend());
    }
    else if (category == "meanReversion") {
        append(shared_mean_reversion());
    }
    else if (category == "momentum") {
        append(shared_momentum());
        if (provider == ProviderSlug::VectorBT) append(vbt_extra_momentum());
        if (provider == ProviderSlug::BT) append(bt_extra_momentum());
    }
    else if (category == "breakout") {
        append(shared_breakout());
        if (provider == ProviderSlug::VectorBT) append(vbt_extra_breakout());
    }
    else if (category == "multiIndicator") {
        if (provider == ProviderSlug::VectorBT || provider == ProviderSlug::BacktestingPy
            || provider == ProviderSlug::Zipline) append(vbt_multi_indicator());
        if (provider == ProviderSlug::FastTrade) append(fasttrade_extra_multi());
    }
    else if (category == "custom") {
        if (provider == ProviderSlug::VectorBT) append(vbt_custom());
    }
    else if (category == "portfolio") {
        if (provider == ProviderSlug::BT) append(bt_portfolio());
    }
    else if (category == "pipeline") {
        if (provider == ProviderSlug::Zipline) append(zipline_pipeline());
    }

    return result;
}

// =============================================================================
// Get categories for provider
// =============================================================================
inline std::vector<CategoryInfo> get_categories(ProviderSlug provider) {
    std::vector<CategoryInfo> result;
    for (auto& cat : strategy_categories()) {
        auto strats = get_strategies(provider, cat.key);
        if (!strats.empty()) result.push_back(cat);
    }
    return result;
}

// =============================================================================
// Indicator definitions
// =============================================================================
inline const std::vector<IndicatorDef>& indicators() {
    static const std::vector<IndicatorDef> ind = {
        {"ma",        "Moving Average (SMA)", {"period"}},
        {"ema",       "Moving Average (EMA)", {"period"}},
        {"mstd",      "Moving Std Dev",       {"period"}},
        {"rsi",       "RSI",                  {"period"}},
        {"stoch",     "Stochastic",           {"k_period", "d_period"}},
        {"cci",       "CCI",                  {"period"}},
        {"williams_r","Williams %R",          {"period"}},
        {"macd",      "MACD",                 {"fast", "slow", "signal"}},
        {"adx",       "ADX",                  {"period"}},
        {"momentum",  "Momentum",             {"lookback"}},
        {"atr",       "ATR",                  {"period"}},
        {"bbands",    "Bollinger Bands",       {"period"}},
        {"keltner",   "Keltner Channel",       {"period"}},
        {"donchian",  "Donchian Channel",      {"period"}},
        {"zscore",    "Z-Score",              {"period"}},
        {"obv",       "OBV",                  {}},
        {"vwap",      "VWAP",                 {}},
    };
    return ind;
}

// =============================================================================
// Position sizing options
// =============================================================================
inline const std::vector<PositionSizingOption>& position_sizing_options() {
    static const std::vector<PositionSizingOption> ps = {
        {"fixed",      "Fixed Size"},
        {"percent",    "Percent of Capital"},
        {"kelly",      "Kelly Criterion"},
        {"volatility", "Volatility Target"},
        {"risk",       "Risk-Based"},
    };
    return ps;
}

// =============================================================================
// Signal modes (for indicator signals command)
// =============================================================================
inline const std::vector<SignalMode>& signal_modes() {
    static const std::vector<SignalMode> sm = {
        {"crossover_signals",     "Crossover",      "MA/EMA crossover signals"},
        {"threshold_signals",     "Threshold",      "RSI/CCI threshold signals"},
        {"breakout_signals",      "Breakout",       "Channel breakout signals"},
        {"mean_reversion_signals","Mean Reversion",  "Z-score mean reversion"},
        {"signal_filter",         "Signal Filter",  "Filter signals with indicators"},
    };
    return sm;
}

// =============================================================================
// Label types (ML)
// =============================================================================
inline const std::vector<LabelType>& label_types() {
    static const std::vector<LabelType> lt = {
        {"FIXLB",  "Fixed Horizon",   "Fixed-time horizon labeling",       {"horizon", "threshold"}},
        {"MEANLB", "Mean Reversion",   "Mean-reversion labeling",          {"window", "threshold"}},
        {"LEXLB",  "Local Extrema",    "Local extrema labeling",           {"window"}},
        {"TRENDLB","Trend Scan",       "Trend-scanning labeling",          {"window", "threshold"}},
        {"BOLB",   "Bollinger Labels", "Bollinger band labeling",          {"window", "alpha"}},
    };
    return lt;
}

// =============================================================================
// Splitter types (CV)
// =============================================================================
inline const std::vector<SplitterType>& splitter_types() {
    static const std::vector<SplitterType> st = {
        {"RollingSplitter",   "Rolling Window",   "Fixed-size rolling train/test splits",    {"window_len", "test_len", "step"}},
        {"ExpandingSplitter", "Expanding Window",  "Expanding train window",                 {"min_len", "test_len", "step"}},
        {"PurgedKFold",       "Purged K-Fold",     "K-fold with purge & embargo",            {"n_splits", "purge_len", "embargo_len"}},
    };
    return st;
}

// =============================================================================
// Returns analysis types
// =============================================================================
inline const std::vector<ReturnsAnalysisType>& returns_analysis_types() {
    static const std::vector<ReturnsAnalysisType> rt = {
        {"full",                 "Full Analysis",  "Complete returns summary"},
        {"rolling",              "Rolling Metrics", "Rolling Sharpe, vol, returns"},
        {"distribution",         "Distribution",   "Returns distribution analysis"},
        {"benchmark_comparison", "Benchmark",      "Compare vs benchmark"},
    };
    return rt;
}

// =============================================================================
// Signal generator types
// =============================================================================
inline const std::vector<SignalGeneratorType>& signal_generator_types() {
    static const std::vector<SignalGeneratorType> sg = {
        {"RAND",   "Random",             "Uniform random entry/exit",              {"n", "seed"}},
        {"RANDX",  "Random (Exclusive)",  "Non-overlapping random signals",        {"n", "seed"}},
        {"RANDNX", "Random N (Excl.)",    "N random signals with hold range",      {"n", "min_hold", "max_hold"}},
        {"RPROB",  "Probability",         "Probability-based entry/exit",           {"entry_prob", "exit_prob", "cooldown"}},
        {"RPROBX", "Probability (Excl.)", "Non-overlapping probability signals",    {"entry_prob", "exit_prob", "cooldown"}},
    };
    return sg;
}

// =============================================================================
// Optimize objectives
// =============================================================================
inline const char* const OPTIMIZE_OBJECTIVES[] = {"sharpe", "sortino", "calmar", "return"};
inline const char* const OPTIMIZE_OBJECTIVE_LABELS[] = {"Sharpe Ratio", "Sortino Ratio", "Calmar Ratio", "Total Return"};
inline const char* const OPTIMIZE_METHODS[] = {"grid", "random"};
inline const char* const OPTIMIZE_METHOD_LABELS[] = {"Grid Search", "Random Search"};

// =============================================================================
// UI Helpers — same pattern as gov_data_constants.h
// =============================================================================

// Colored tab button
inline bool ProviderTab(const char* label, ImVec4 color, bool active) {
    using namespace theme::colors;
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    }
    bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleColor(3);
    return clicked;
}

// Command item with left border
inline bool CommandItem(const char* label, const char* desc, ImVec4 color, bool active) {
    using namespace theme::colors;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    // Background
    ImVec4 bg = active ? ImVec4(color.x, color.y, color.z, 0.15f) : ImVec4(0,0,0,0);
    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.20f));

    bool clicked = ImGui::Button(("##cmd_" + std::string(label)).c_str(), ImVec2(w, 36));
    ImGui::PopStyleColor(3);

    // Left accent bar
    if (active) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            cursor, ImVec2(cursor.x + 3, cursor.y + 36),
            ImGui::ColorConvertFloat4ToU32(color));
    }

    // Text overlay
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 10, cursor.y + 4));
    ImGui::TextColored(active ? TEXT_PRIMARY : TEXT_SECONDARY, "%s", label);
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 10, cursor.y + 20));
    ImGui::TextColored(TEXT_DIM, "%s", desc);

    // Reset cursor
    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + 38));

    return clicked;
}

// Status message with fade
inline void StatusMsg(const std::string& msg, double time) {
    if (msg.empty()) return;
    float alpha = 1.0f;
    double elapsed = ImGui::GetTime() - time;
    if (elapsed > 5.0) alpha = std::max(0.0f, 1.0f - (float)(elapsed - 5.0) / 3.0f);
    if (alpha <= 0.01f) return;
    ImVec4 c = theme::colors::TEXT_DIM;
    c.w = alpha;
    ImGui::TextColored(c, "%s", msg.c_str());
}

// Empty state placeholder
inline void EmptyState(const char* title, const char* subtitle) {
    ImGui::Spacing(); ImGui::Spacing();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w * 0.25f);
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", title);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w * 0.2f);
    ImGui::TextColored(theme::colors::TEXT_DISABLED, "%s", subtitle);
}

// Metric row (label + value with color)
inline void MetricRow(const char* label, const char* value, ImVec4 value_color) {
    using namespace theme::colors;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild(("##mr_" + std::string(label)).c_str(), ImVec2(0, 24), ImGuiChildFlags_Borders);
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(value).x);
    ImGui::TextColored(value_color, "%s", value);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Section header with collapsible toggle
inline bool SectionHeader(const char* label, bool* expanded) {
    using namespace theme::colors;
    ImGui::PushStyleColor(ImGuiCol_Header, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, BG_HOVER);
    bool open = ImGui::CollapsingHeader(label, *expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor(3);
    *expanded = open;
    return open;
}

// Labeled input field
inline bool InputField(const char* label, char* buf, size_t buf_size) {
    using namespace theme::colors;
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(-1);
    bool changed = ImGui::InputText(("##" + std::string(label)).c_str(), buf, buf_size);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    return changed;
}

// Labeled float input
inline bool InputFloat(const char* label, float* v, float step = 0, float step_fast = 0, const char* fmt = "%.2f") {
    using namespace theme::colors;
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(-1);
    bool changed = ImGui::InputFloat(("##" + std::string(label)).c_str(), v, step, step_fast, fmt);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    return changed;
}

// Labeled int input
inline bool InputInt(const char* label, int* v, int step = 0) {
    using namespace theme::colors;
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(-1);
    bool changed = ImGui::InputInt(("##" + std::string(label)).c_str(), v, step);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    return changed;
}

// Run button
inline bool RunButton(const char* label, bool loading, ImVec4 color) {
    using namespace theme::colors;
    if (loading) {
        ImGui::PushStyleColor(ImGuiCol_Button, TEXT_DISABLED);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, TEXT_DISABLED);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x*1.1f, color.y*1.1f, color.z*1.1f, 1.0f));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
    bool clicked = ImGui::Button(loading ? "EXECUTING..." : label, ImVec2(140, 28));
    ImGui::PopStyleColor(3);
    return clicked && !loading;
}

} // namespace fincept::backtesting
