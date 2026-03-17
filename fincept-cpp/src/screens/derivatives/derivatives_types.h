#pragma once
// Derivatives Types — instrument types, parameter structs, and result types
// for the derivatives pricing & valuation screen.

#include <string>
#include <vector>

namespace fincept::derivatives {

// ============================================================================
// Instrument categories
// ============================================================================

enum class InstrumentType {
    bonds, equity_options, fx_options, swaps, credit, vollib
};

inline const char* instrument_label(InstrumentType t) {
    switch (t) {
        case InstrumentType::bonds:          return "BONDS";
        case InstrumentType::equity_options: return "EQUITY OPTIONS";
        case InstrumentType::fx_options:     return "FX OPTIONS";
        case InstrumentType::swaps:          return "SWAPS";
        case InstrumentType::credit:         return "CREDIT (CDS)";
        case InstrumentType::vollib:         return "VOLLIB";
    }
    return "BONDS";
}

// ============================================================================
// Bond parameters
// ============================================================================

struct BondParams {
    char issue_date[16]      = "2023-01-01";
    char settlement_date[16] = "2026-03-15";
    char maturity_date[16]   = "2029-01-01";
    double coupon_rate   = 5.0;
    double ytm           = 4.5;
    int    frequency      = 2;      // semi-annual
    double clean_price   = 102.0;
};

struct BondResult {
    double clean_price     = 0.0;
    double dirty_price     = 0.0;
    double accrued_interest = 0.0;
    double duration        = 0.0;
    double convexity       = 0.0;
    double ytm             = 0.0;
    bool   valid           = false;
};

// ============================================================================
// Equity option parameters
// ============================================================================

struct EquityOptionParams {
    char valuation_date[16] = "2026-03-15";
    char expiry_date[16]    = "2027-03-15";
    double strike        = 100.0;
    double spot          = 105.0;
    double volatility    = 25.0;    // %
    double risk_free_rate = 5.0;    // %
    double dividend_yield = 2.0;    // %
    int    option_type    = 0;      // 0=call, 1=put
};

struct GreeksResult {
    double price = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega  = 0.0;
    double theta = 0.0;
    double rho   = 0.0;
    double implied_vol = 0.0;
    bool   valid = false;
};

// ============================================================================
// FX option parameters
// ============================================================================

struct FXOptionParams {
    char valuation_date[16] = "2026-03-15";
    char expiry_date[16]    = "2027-03-15";
    double strike        = 1.10;
    double spot          = 1.08;
    double volatility    = 12.0;    // %
    double domestic_rate = 2.0;     // %
    double foreign_rate  = 1.5;     // %
    int    option_type   = 0;       // 0=call, 1=put
    double notional      = 1000000.0;
};

// ============================================================================
// Swap parameters
// ============================================================================

struct SwapParams {
    char effective_date[16] = "2026-03-15";
    char maturity_date[16]  = "2031-03-15";
    double fixed_rate    = 3.0;     // %
    int    frequency     = 2;
    double notional      = 1000000.0;
    double discount_rate = 3.5;     // %
};

struct SwapResult {
    double fixed_leg_pv  = 0.0;
    double float_leg_pv  = 0.0;
    double swap_value    = 0.0;
    double dv01          = 0.0;
    bool   valid         = false;
};

// ============================================================================
// CDS parameters
// ============================================================================

struct CDSParams {
    char valuation_date[16] = "2026-03-15";
    char maturity_date[16]  = "2031-03-15";
    double recovery_rate = 40.0;    // %
    double notional      = 10000000.0;
    double spread_bps    = 150.0;
};

struct CDSResult {
    double upfront_pv    = 0.0;
    double running_pv    = 0.0;
    double clean_price   = 0.0;
    double risky_pv01    = 0.0;
    bool   valid         = false;
};

// ============================================================================
// Vollib parameters (py_vollib Black-Scholes)
// ============================================================================

enum class VollibModel { black, bs, bsm };

inline const char* vollib_model_label(VollibModel m) {
    switch (m) {
        case VollibModel::black: return "Black";
        case VollibModel::bs:    return "Black-Scholes";
        case VollibModel::bsm:   return "Black-Scholes-Merton";
    }
    return "Black-Scholes";
}

struct VollibParams {
    double S     = 100.0;   // spot
    double K     = 100.0;   // strike
    double t     = 0.25;    // time to expiry (years)
    double r     = 0.05;    // risk-free rate
    double sigma = 0.20;    // volatility
    double q     = 0.02;    // dividend yield
    int    flag  = 0;       // 0=call, 1=put
    double option_price = 5.0;
    VollibModel model = VollibModel::bs;
};

// ============================================================================
// Payoff diagram point
// ============================================================================

struct PayoffPoint {
    double underlying = 0.0;
    double payoff     = 0.0;
    double pnl        = 0.0;
};

} // namespace fincept::derivatives
