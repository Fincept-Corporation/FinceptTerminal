#pragma once
// AI Quant Lab Types — Shared data types for the 18-module quant research platform
// Mirrors the Tauri AIQuantLabTab's module definitions and state types

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::ai_quant_lab {

using json = nlohmann::json;

// ============================================================================
// View modes — one per sub-panel (matches Tauri ViewMode type)
// ============================================================================
enum class ViewMode {
    FactorDiscovery,
    ModelLibrary,
    Backtesting,
    LiveSignals,
    Functime,
    Fortitudo,
    Statsmodels,
    CFAQuant,
    DeepAgent,
    RLTrading,
    OnlineLearning,
    HFT,
    MetaLearning,
    RollingRetraining,
    AdvancedModels,
    GluonTS,
    GSQuant,
    PatternIntelligence,
};

// ============================================================================
// Module categories (matches Tauri's 4 categories)
// ============================================================================
enum class ModuleCategory {
    Core,
    AIML,
    Advanced,
    Analytics,
};

inline const char* category_label(ModuleCategory c) {
    switch (c) {
        case ModuleCategory::Core:     return "CORE";
        case ModuleCategory::AIML:     return "AI / ML";
        case ModuleCategory::Advanced: return "ADVANCED";
        case ModuleCategory::Analytics: return "ANALYTICS";
    }
    return "";
}

// ============================================================================
// Module definition — matches Tauri's ModuleItem interface
// ============================================================================
struct ModuleItem {
    ViewMode view;
    const char* label;
    const char* description;
    ModuleCategory category;
};

inline std::vector<ModuleItem> get_modules() {
    return {
        // Core
        {ViewMode::FactorDiscovery,    "Factor Discovery",     "RD-Agent automated factor mining",     ModuleCategory::Core},
        {ViewMode::ModelLibrary,       "Model Library",        "Microsoft Qlib ML models",             ModuleCategory::Core},
        {ViewMode::Backtesting,        "Backtesting",          "Qlib strategy backtesting engine",     ModuleCategory::Core},
        {ViewMode::LiveSignals,        "Live Signals",         "Real-time model predictions",          ModuleCategory::Core},
        // AI/ML
        {ViewMode::DeepAgent,          "Deep Agent",           "LLM-powered autonomous research",      ModuleCategory::AIML},
        {ViewMode::RLTrading,          "RL Trading",           "Reinforcement learning strategies",    ModuleCategory::AIML},
        {ViewMode::OnlineLearning,     "Online Learning",      "Continuous model adaptation",          ModuleCategory::AIML},
        {ViewMode::MetaLearning,       "Meta Learning",        "Automated model selection",            ModuleCategory::AIML},
        {ViewMode::AdvancedModels,     "Advanced Models",      "LSTM, GRU, Transformer networks",     ModuleCategory::AIML},
        // Advanced
        {ViewMode::HFT,               "HFT Analytics",        "Order book & microstructure",          ModuleCategory::Advanced},
        {ViewMode::RollingRetraining,  "Rolling Retraining",   "Scheduled model updates",             ModuleCategory::Advanced},
        {ViewMode::PatternIntelligence,"Pattern Intelligence", "VisionQuant K-line recognition",      ModuleCategory::Advanced},
        // Analytics
        {ViewMode::GluonTS,            "GluonTS",              "Deep learning time series",            ModuleCategory::Analytics},
        {ViewMode::GSQuant,            "GS Quant",             "Goldman Sachs-style analytics",        ModuleCategory::Analytics},
        {ViewMode::Statsmodels,        "Statsmodels",          "ARIMA, SARIMAX, decomposition",        ModuleCategory::Analytics},
        {ViewMode::Functime,           "Functime",             "Fast time series forecasting",         ModuleCategory::Analytics},
        {ViewMode::Fortitudo,          "Fortitudo",            "Entropy pooling & optimization",       ModuleCategory::Analytics},
        {ViewMode::CFAQuant,           "CFA Quant",            "CFA-level quantitative analysis",      ModuleCategory::Analytics},
    };
}

// ============================================================================
// System status — matches Tauri's SystemStatus state machine
// ============================================================================
enum class SystemStatus {
    Idle,
    Loading,
    Ready,
    Error,
};

// ============================================================================
// Async operation state — tracks a single async task (API call / Python exec)
// ============================================================================
struct AsyncOp {
    std::atomic<bool> loading{false};
    json result;
    std::string error;
    bool has_result = false;
    std::mutex mutex;

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        result = json();
        error.clear();
        has_result = false;
    }

    void set_result(const json& data) {
        std::lock_guard<std::mutex> lock(mutex);
        result = data;
        has_result = true;
        error.clear();
        loading.store(false);
    }

    void set_error(const std::string& err) {
        std::lock_guard<std::mutex> lock(mutex);
        error = err;
        has_result = false;
        loading.store(false);
    }
};

// ============================================================================
// Factor Discovery types
// ============================================================================
struct DiscoveredFactor {
    std::string name;
    std::string code;
    double ic = 0.0;
    double sharpe = 0.0;
    double annual_return = 0.0;
    std::string description;
};

// ============================================================================
// Model Library types
// ============================================================================
struct QlibModel {
    std::string name;
    std::string type;       // tree_based, neural_network, linear, ensemble
    std::string description;
    std::vector<std::string> features;
    std::vector<std::string> use_cases;
};

// ============================================================================
// Live Signals types
// ============================================================================
struct SignalEntry {
    std::string symbol;
    std::string signal;     // BUY, SELL, HOLD
    double score = 0.0;
    double price = 0.0;
    double confidence = 0.0;
    std::string model;
    std::string timestamp;
};

// ============================================================================
// Deep Agent types
// ============================================================================
enum class AgentType {
    Research,
    StrategyBuilder,
    PortfolioManager,
    RiskAnalyzer,
    General,
};

inline const char* agent_type_label(AgentType t) {
    switch (t) {
        case AgentType::Research:         return "Research";
        case AgentType::StrategyBuilder:  return "Strategy Builder";
        case AgentType::PortfolioManager: return "Portfolio Manager";
        case AgentType::RiskAnalyzer:     return "Risk Analyzer";
        case AgentType::General:          return "General";
    }
    return "";
}

struct AgentStep {
    int index = 0;
    std::string description;
    std::string status;     // pending, running, completed, failed
    std::string result;
};

// ============================================================================
// RL Trading types
// ============================================================================
enum class RLAlgorithm {
    PPO, A2C, DQN, SAC, TD3,
};

inline const char* rl_algo_label(RLAlgorithm a) {
    switch (a) {
        case RLAlgorithm::PPO: return "PPO";
        case RLAlgorithm::A2C: return "A2C";
        case RLAlgorithm::DQN: return "DQN";
        case RLAlgorithm::SAC: return "SAC";
        case RLAlgorithm::TD3: return "TD3";
    }
    return "";
}

// ============================================================================
// HFT Order Book types
// ============================================================================
struct OrderLevel {
    double price = 0.0;
    double size = 0.0;
    int orders = 0;
};

struct MicrostructureMetrics {
    double depth_imbalance = 0.0;
    double vwap_bid = 0.0;
    double vwap_ask = 0.0;
    double toxicity_score = 0.0;
    double spread = 0.0;
    double mid_price = 0.0;
};

// ============================================================================
// GluonTS model types
// ============================================================================
struct GluonTSModel {
    const char* name;
    const char* label;
    const char* category;   // "deep_learning" or "baseline"
};

inline std::vector<GluonTSModel> get_gluonts_models() {
    return {
        {"deepar",    "DeepAR",          "deep_learning"},
        {"tft",       "TFT",             "deep_learning"},
        {"wavenet",   "WaveNet",         "deep_learning"},
        {"patchtst",  "PatchTST",        "deep_learning"},
        {"nbeats",    "N-BEATS",         "deep_learning"},
        {"nhits",     "N-HiTS",          "deep_learning"},
        {"informer",  "Informer",        "deep_learning"},
        {"autoformer","Autoformer",       "deep_learning"},
        {"fedformer", "FEDformer",       "deep_learning"},
        {"naive",     "Naive (Baseline)","baseline"},
        {"seasonal",  "Seasonal Naive",  "baseline"},
        {"ets",       "ETS",             "baseline"},
    };
}

// ============================================================================
// GS Quant analysis modes
// ============================================================================
enum class GSQuantMode {
    RiskMetrics,
    PortfolioAnalytics,
    Greeks,
    VaR,
    StressTesting,
    Backtesting,
    Statistics,
};

inline const char* gs_quant_mode_label(GSQuantMode m) {
    switch (m) {
        case GSQuantMode::RiskMetrics:       return "Risk Metrics";
        case GSQuantMode::PortfolioAnalytics: return "Portfolio Analytics";
        case GSQuantMode::Greeks:            return "Greeks";
        case GSQuantMode::VaR:               return "VaR";
        case GSQuantMode::StressTesting:     return "Stress Testing";
        case GSQuantMode::Backtesting:       return "Backtesting";
        case GSQuantMode::Statistics:        return "Statistics";
    }
    return "";
}

// ============================================================================
// Statsmodels analysis types
// ============================================================================
enum class StatsAnalysis {
    ARIMA, SARIMAX, ExpSmoothing, STLDecompose,
    ADFTest, KPSSTest, ACF, PACF,
};

inline const char* stats_analysis_label(StatsAnalysis a) {
    switch (a) {
        case StatsAnalysis::ARIMA:         return "ARIMA";
        case StatsAnalysis::SARIMAX:       return "SARIMAX";
        case StatsAnalysis::ExpSmoothing:  return "Exp Smoothing";
        case StatsAnalysis::STLDecompose:  return "STL Decompose";
        case StatsAnalysis::ADFTest:       return "ADF Test";
        case StatsAnalysis::KPSSTest:      return "KPSS Test";
        case StatsAnalysis::ACF:           return "ACF";
        case StatsAnalysis::PACF:          return "PACF";
    }
    return "";
}

// ============================================================================
// Fortitudo modes
// ============================================================================
enum class FortitudoMode {
    EntropyPooling,
    Optimization,
    OptionPricing,
    PortfolioRisk,
};

inline const char* fortitudo_mode_label(FortitudoMode m) {
    switch (m) {
        case FortitudoMode::EntropyPooling: return "Entropy Pooling";
        case FortitudoMode::Optimization:   return "Optimization";
        case FortitudoMode::OptionPricing:  return "Option Pricing";
        case FortitudoMode::PortfolioRisk:  return "Portfolio Risk";
    }
    return "";
}

} // namespace fincept::ai_quant_lab
