#pragma once
// QuantLib Types — Shared data types for the QuantLib screen

#include <string>
#include <vector>
#include <map>
#include <future>
#include <nlohmann/json.hpp>
#include "quantlib_api.h"

namespace fincept::quantlib {

using json = nlohmann::json;

// ============================================================================
// Module and section definitions for sidebar
// ============================================================================

struct SectionDef {
    std::string id;
    std::string label;
};

struct ModuleDef {
    std::string id;
    std::string label;
    int endpoint_count;
    std::vector<SectionDef> sections;
};

// ============================================================================
// Endpoint state — tracks input, loading, result per endpoint card
// ============================================================================

struct EndpointState {
    bool loading = false;
    bool has_result = false;
    json result_data;
    std::string error;
    double elapsed_ms = 0;
    std::future<ApiResult> future;
    bool expanded = false;

    // Input values stored as string map (field_name -> value)
    std::map<std::string, std::string> inputs;

    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = inputs.find(key);
        return it != inputs.end() ? it->second : def;
    }
    double getd(const std::string& key, double def = 0.0) const {
        auto it = inputs.find(key);
        if (it == inputs.end() || it->second.empty()) return def;
        try { return std::stod(it->second); } catch (...) { return def; }
    }
    int geti(const std::string& key, int def = 0) const {
        auto it = inputs.find(key);
        if (it == inputs.end() || it->second.empty()) return def;
        try { return std::stoi(it->second); } catch (...) { return def; }
    }
    std::string& buf(const std::string& key, const std::string& def = "") {
        auto it = inputs.find(key);
        if (it == inputs.end()) {
            inputs[key] = def;
        }
        return inputs[key];
    }
};

// ============================================================================
// Module definitions — matches Tauri's MODULES array
// ============================================================================

inline std::vector<ModuleDef> get_module_defs() {
    return {
        {"core", "CORE", 51, {
            {"types", "Types"}, {"conventions", "Conventions"},
            {"autodiff", "AutoDiff"}, {"distributions", "Distributions"},
            {"math", "Math"}, {"operations", "Operations"},
            {"legs", "Legs"}, {"periods", "Periods"},
        }},
        {"regulatory", "REGULATORY", 11, {
            {"basel", "Basel III"}, {"saccr", "SA-CCR"},
            {"ifrs9", "IFRS 9"}, {"liquidity", "Liquidity"},
            {"stress", "Stress Test"},
        }},
        {"volatility", "VOLATILITY", 14, {
            {"surface", "Surface"}, {"sabr", "SABR"}, {"localvol", "Local Vol"},
        }},
        {"models", "MODELS", 14, {
            {"shortrate", "Short Rate"}, {"hullwhite", "Hull-White"},
            {"heston", "Heston"}, {"jumpdiff", "Jump Diffusion"},
            {"volmodels", "Dupire / SVI"},
        }},
        {"stochastic", "STOCHASTIC", 36, {
            {"processes", "Processes"}, {"exact", "Exact"},
            {"simulation", "Simulation"}, {"sampling", "Sampling"},
            {"theory", "Theory"},
        }},
        {"ml", "MACHINE LEARNING", 48, {
            {"ml-credit", "Credit"}, {"ml-regression", "Regression"},
            {"ml-clustering", "Clustering"}, {"ml-preprocessing", "Preprocessing"},
            {"ml-features", "Features"}, {"ml-validation", "Validation"},
            {"ml-timeseries", "Time Series"}, {"ml-gpnn", "GP / Neural Net"},
            {"ml-factor", "Factor / Cov"},
        }},
        {"statistics", "STATISTICS", 52, {
            {"stat-continuous", "Continuous Dist"}, {"stat-discrete", "Discrete Dist"},
            {"stat-timeseries", "Time Series"},
        }},
        {"portfolio", "PORTFOLIO", 15, {
            {"port-optimize", "Optimization"}, {"port-risk", "Risk Metrics"},
        }},
        {"scheduling", "SCHEDULING", 14, {
            {"sched-calendar", "Calendars"}, {"sched-daycount", "Day Count"},
        }},
        {"physics", "PHYSICS", 24, {
            {"phys-entropy", "Entropy"}, {"phys-thermo", "Thermodynamics"},
        }},
        {"curves", "CURVES", 31, {
            {"curves-build", "Build & Query"}, {"curves-transform", "Transforms"},
            {"curves-fitting", "NS / NSS Fitting"}, {"curves-specialized", "Specialized"},
        }},
        {"pricing", "PRICING", 29, {
            {"pricing-bs", "Black-Scholes"}, {"pricing-black76", "Black76"},
            {"pricing-bachelier", "Bachelier"}, {"pricing-numerical", "Numerical"},
        }},
        {"risk", "RISK", 25, {
            {"risk-var", "VaR / Stress"}, {"risk-evt-xva", "EVT / XVA"},
            {"risk-sensitivities", "Sensitivities"},
        }},
        {"economics", "ECONOMICS", 25, {
            {"econ-equilibrium", "Equilibrium"}, {"econ-gametheory", "Game Theory"},
            {"econ-auctions", "Auctions"}, {"econ-utility", "Utility Theory"},
        }},
        {"solver", "SOLVER", 25, {
            {"solver-bond", "Bond Analytics"}, {"solver-rates", "Rates / IV"},
            {"solver-cashflow", "Cashflows"},
        }},
        {"numerical", "NUMERICAL", 28, {
            {"num-difffft", "Diff / FFT / Int"}, {"num-interplinalg", "Interp / LinAlg"},
            {"num-solvers", "ODE / Roots / Opt"},
        }},
        {"instruments", "INSTRUMENTS", 26, {
            {"instr-bonds", "Bonds"}, {"instr-swaps", "Swaps / FRA"},
            {"instr-markets", "Markets"}, {"instr-creditfut", "Credit / Futures"},
        }},
        {"analysis", "ANALYSIS", 122, {
            {"analysis-fundamentals", "Fundamentals"},
            {"analysis-profit-liq", "Profit / Liquidity"},
            {"analysis-eff-growth", "Efficiency / Growth"},
            {"analysis-lev-cf", "Leverage / CF"},
            {"analysis-val-qual", "Val Ratios / Quality"},
            {"analysis-industry", "Industry"},
            {"analysis-dcf", "DCF Valuation"},
            {"analysis-models", "Valuation Models"},
        }},
    };
}

} // namespace fincept::quantlib
