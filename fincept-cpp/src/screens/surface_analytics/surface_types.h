#pragma once
// Surface Analytics — data types for 35 financial surface visualizations
// Covers: equity derivatives, fixed income, FX, credit, commodities, risk, macro

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace fincept::surface {

// ============================================================================
// Chart type enum — all 35 surfaces
// ============================================================================
enum class ChartType {
    // Equity Derivatives (7)
    Volatility, DeltaSurface, GammaSurface, VegaSurface, ThetaSurface,
    SkewSurface, LocalVolSurface,
    // Fixed Income (7)
    YieldCurve, SwaptionVol, CapFloorVol, BondSpread, OISBasis, RealYield, ForwardRate,
    // FX (3)
    FXVol, FXForwardPoints, CrossCurrencyBasis,
    // Credit (3)
    CDSSpread, CreditTransition, RecoveryRate,
    // Commodities (4)
    CommodityForward, CommodityVol, CrackSpread, ContangoBackwardation,
    // Risk & Portfolio (9)
    Correlation, PCA, VaR, StressTestPnL, FactorExposure,
    LiquidityHeatmap, Drawdown, BetaSurface, ImpliedDividend,
    // Macro (2)
    InflationExpectations, MonetaryPolicyPath,
};

// ============================================================================
// Display name for each chart type
// ============================================================================
inline const char* chart_type_name(ChartType t) {
    switch (t) {
        case ChartType::Volatility:            return "VOL SURFACE";
        case ChartType::DeltaSurface:          return "DELTA";
        case ChartType::GammaSurface:          return "GAMMA";
        case ChartType::VegaSurface:           return "VEGA";
        case ChartType::ThetaSurface:          return "THETA";
        case ChartType::SkewSurface:           return "SKEW";
        case ChartType::LocalVolSurface:       return "LOCAL VOL";
        case ChartType::YieldCurve:            return "YIELD CURVE";
        case ChartType::SwaptionVol:           return "SWAPTION VOL";
        case ChartType::CapFloorVol:           return "CAP/FLOOR";
        case ChartType::BondSpread:            return "BOND SPREAD";
        case ChartType::OISBasis:              return "OIS BASIS";
        case ChartType::RealYield:             return "REAL YIELD";
        case ChartType::ForwardRate:           return "FWD RATE";
        case ChartType::FXVol:                 return "FX VOL";
        case ChartType::FXForwardPoints:       return "FX FORWARD";
        case ChartType::CrossCurrencyBasis:    return "XCCY BASIS";
        case ChartType::CDSSpread:             return "CDS SPREAD";
        case ChartType::CreditTransition:      return "TRANSITION";
        case ChartType::RecoveryRate:          return "RECOVERY";
        case ChartType::CommodityForward:      return "CMDTY FWD";
        case ChartType::CommodityVol:          return "CMDTY VOL";
        case ChartType::CrackSpread:           return "CRACK/CRUSH";
        case ChartType::ContangoBackwardation: return "CONTANGO";
        case ChartType::Correlation:           return "CORRELATION";
        case ChartType::PCA:                   return "PCA";
        case ChartType::VaR:                   return "VAR";
        case ChartType::StressTestPnL:         return "STRESS TEST";
        case ChartType::FactorExposure:        return "FACTOR EXP";
        case ChartType::LiquidityHeatmap:      return "LIQUIDITY";
        case ChartType::Drawdown:              return "DRAWDOWN";
        case ChartType::BetaSurface:           return "BETA";
        case ChartType::ImpliedDividend:       return "IMPL DIV";
        case ChartType::InflationExpectations: return "INFLATION";
        case ChartType::MonetaryPolicyPath:    return "RATE PATH";
    }
    return "UNKNOWN";
}

// ============================================================================
// Category grouping
// ============================================================================
struct SurfaceCategory {
    const char* name;
    std::vector<ChartType> types;
};

inline std::vector<SurfaceCategory> get_surface_categories() {
    return {
        {"EQUITY DERIV", {ChartType::Volatility, ChartType::DeltaSurface, ChartType::GammaSurface,
                          ChartType::VegaSurface, ChartType::ThetaSurface, ChartType::SkewSurface,
                          ChartType::LocalVolSurface}},
        {"FIXED INCOME", {ChartType::YieldCurve, ChartType::SwaptionVol, ChartType::CapFloorVol,
                          ChartType::BondSpread, ChartType::OISBasis, ChartType::RealYield,
                          ChartType::ForwardRate}},
        {"FX",           {ChartType::FXVol, ChartType::FXForwardPoints, ChartType::CrossCurrencyBasis}},
        {"CREDIT",       {ChartType::CDSSpread, ChartType::CreditTransition, ChartType::RecoveryRate}},
        {"COMMODITIES",  {ChartType::CommodityForward, ChartType::CommodityVol,
                          ChartType::CrackSpread, ChartType::ContangoBackwardation}},
        {"RISK",         {ChartType::Correlation, ChartType::PCA, ChartType::VaR,
                          ChartType::StressTestPnL, ChartType::FactorExposure, ChartType::LiquidityHeatmap,
                          ChartType::Drawdown, ChartType::BetaSurface, ChartType::ImpliedDividend}},
        {"MACRO",        {ChartType::InflationExpectations, ChartType::MonetaryPolicyPath}},
    };
}

// ============================================================================
// Volatility Surface
// ============================================================================
struct VolatilitySurfaceData {
    std::vector<float> strikes;
    std::vector<int> expirations;     // Days to expiry
    std::vector<std::vector<float>> z; // IV matrix [exp][strike]
    std::string underlying;
    float spot_price = 0;
};

// ============================================================================
// Correlation Matrix
// ============================================================================
struct CorrelationMatrixData {
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z; // [time][asset_i * n + asset_j]
    int window = 30;
};

// ============================================================================
// Yield Curve
// ============================================================================
struct YieldCurveData {
    std::vector<int> maturities;       // In months
    std::vector<std::vector<float>> z; // [time][maturity]
};

// ============================================================================
// PCA
// ============================================================================
struct PCAData {
    std::vector<std::string> factors;
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z; // [asset][factor] loadings
    std::vector<float> variance_explained;
};

// ============================================================================
// Greeks Surfaces (shared struct for Delta, Gamma, Vega, Theta)
// ============================================================================
struct GreeksSurfaceData {
    std::vector<float> strikes;
    std::vector<int> expirations;       // DTE
    std::vector<std::vector<float>> z;  // [exp][strike]
    std::string underlying;
    float spot_price = 0;
    std::string greek_name;             // "Delta", "Gamma", etc.
};

// ============================================================================
// Skew Surface (25-Delta Risk Reversal)
// ============================================================================
struct SkewSurfaceData {
    std::vector<int> expirations;       // DTE
    std::vector<float> deltas;          // e.g. 10, 25, 50, 75, 90
    std::vector<std::vector<float>> z;  // [exp][delta] skew values
    std::string underlying;
};

// ============================================================================
// Local Volatility (Dupire)
// ============================================================================
struct LocalVolData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;  // [exp][strike] local vol
    std::string underlying;
    float spot_price = 0;
};

// ============================================================================
// Swaption Volatility
// ============================================================================
struct SwaptionVolData {
    std::vector<int> option_expiries;   // months
    std::vector<int> swap_tenors;       // months
    std::vector<std::vector<float>> z;  // [expiry][tenor] bps
};

// ============================================================================
// Cap/Floor Volatility
// ============================================================================
struct CapFloorVolData {
    std::vector<float> strikes;         // rate strikes in %
    std::vector<int> maturities;        // months
    std::vector<std::vector<float>> z;  // [maturity][strike] vol bps
};

// ============================================================================
// Bond Spread Surface
// ============================================================================
struct BondSpreadData {
    std::vector<std::string> ratings;   // AAA, AA, A, BBB, BB, B, CCC
    std::vector<int> maturities;        // months
    std::vector<std::vector<float>> z;  // [rating][maturity] spread bps
};

// ============================================================================
// OIS vs SOFR Basis
// ============================================================================
struct OISBasisData {
    std::vector<int> tenors;            // months
    std::vector<int> time_points;       // days
    std::vector<std::vector<float>> z;  // [time][tenor] basis bps
};

// ============================================================================
// Real Yield (TIPS)
// ============================================================================
struct RealYieldData {
    std::vector<int> maturities;        // months
    std::vector<int> time_points;       // days
    std::vector<std::vector<float>> z;  // [time][maturity] real yield %
};

// ============================================================================
// Forward Rate Surface
// ============================================================================
struct ForwardRateData {
    std::vector<int> start_tenors;      // months
    std::vector<int> forward_periods;   // months
    std::vector<std::vector<float>> z;  // [start][fwd_period] rate %
};

// ============================================================================
// FX Volatility Surface
// ============================================================================
struct FXVolData {
    std::vector<float> deltas;          // 10, 25, ATM, 75, 90
    std::vector<int> tenors;            // days
    std::vector<std::vector<float>> z;  // [tenor][delta] vol %
    std::string pair;                   // e.g. "EUR/USD"
};

// ============================================================================
// FX Forward Points
// ============================================================================
struct FXForwardPointsData {
    std::vector<std::string> pairs;     // currency pairs
    std::vector<int> tenors;            // days
    std::vector<std::vector<float>> z;  // [pair][tenor] pips
};

// ============================================================================
// Cross-Currency Basis
// ============================================================================
struct CrossCurrencyBasisData {
    std::vector<std::string> pairs;
    std::vector<int> tenors;            // months
    std::vector<std::vector<float>> z;  // [pair][tenor] bps
};

// ============================================================================
// CDS Spread Surface
// ============================================================================
struct CDSSpreadData {
    std::vector<std::string> entities;  // company/sovereign names
    std::vector<int> tenors;            // months
    std::vector<std::vector<float>> z;  // [entity][tenor] bps
};

// ============================================================================
// Credit Transition Matrix
// ============================================================================
struct CreditTransitionData {
    std::vector<std::string> ratings;    // from ratings (rows)
    std::vector<std::string> to_ratings; // to ratings (cols)
    std::vector<std::vector<float>> z;   // [from][to] probability %
};

// ============================================================================
// Recovery Rate Surface
// ============================================================================
struct RecoveryRateData {
    std::vector<std::string> sectors;
    std::vector<std::string> seniorities; // Senior, Sub, Junior
    std::vector<std::vector<float>> z;    // [sector][seniority] recovery %
};

// ============================================================================
// Commodity Forward Curve
// ============================================================================
struct CommodityForwardData {
    std::vector<std::string> commodities;
    std::vector<int> contract_months;   // months forward
    std::vector<std::vector<float>> z;  // [commodity][month] price
};

// ============================================================================
// Commodity Volatility
// ============================================================================
struct CommodityVolData {
    std::vector<float> strikes;         // % of spot
    std::vector<int> expirations;       // DTE
    std::vector<std::vector<float>> z;  // [exp][strike] vol %
    std::string commodity;
};

// ============================================================================
// Crack/Crush Spread
// ============================================================================
struct CrackSpreadData {
    std::vector<std::string> spread_types; // "3-2-1 Crack", "Soy Crush", etc.
    std::vector<int> contract_months;
    std::vector<std::vector<float>> z;     // [type][month] spread $/unit
};

// ============================================================================
// Contango/Backwardation
// ============================================================================
struct ContangoData {
    std::vector<std::string> commodities;
    std::vector<int> contract_months;
    std::vector<std::vector<float>> z;  // [commodity][month] % from spot
};

// ============================================================================
// VaR Surface
// ============================================================================
struct VaRData {
    std::vector<float> confidence_levels; // 90, 95, 99, 99.5, 99.9
    std::vector<int> horizons;            // days
    std::vector<std::vector<float>> z;    // [confidence][horizon] VaR %
};

// ============================================================================
// Stress Test P&L
// ============================================================================
struct StressTestData {
    std::vector<std::string> scenarios;
    std::vector<std::string> portfolios;
    std::vector<std::vector<float>> z;  // [scenario][portfolio] P&L %
};

// ============================================================================
// Factor Exposure
// ============================================================================
struct FactorExposureData {
    std::vector<std::string> factors;   // Market, Size, Value, Momentum, Vol
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z;  // [asset][factor] exposure
};

// ============================================================================
// Liquidity Heatmap
// ============================================================================
struct LiquidityData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;  // [exp][strike] spread or volume
    std::string underlying;
};

// ============================================================================
// Drawdown Surface
// ============================================================================
struct DrawdownData {
    std::vector<std::string> assets;
    std::vector<int> windows;           // rolling window days
    std::vector<std::vector<float>> z;  // [asset][window] max drawdown %
};

// ============================================================================
// Beta Surface
// ============================================================================
struct BetaData {
    std::vector<std::string> assets;
    std::vector<int> horizons;          // rolling days
    std::vector<std::vector<float>> z;  // [asset][horizon] beta
};

// ============================================================================
// Implied Dividend Surface
// ============================================================================
struct ImpliedDividendData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;  // [exp][strike] implied div yield %
    std::string underlying;
};

// ============================================================================
// Inflation Expectations
// ============================================================================
struct InflationExpData {
    std::vector<int> horizons;          // years
    std::vector<int> time_points;       // days
    std::vector<std::vector<float>> z;  // [time][horizon] breakeven %
};

// ============================================================================
// Monetary Policy Rate Path
// ============================================================================
struct MonetaryPolicyData {
    std::vector<std::string> central_banks; // Fed, ECB, BoJ, BoE, PBoC
    std::vector<int> meetings_ahead;        // next N meetings
    std::vector<std::vector<float>> z;      // [bank][meeting] implied rate %
};

// ============================================================================
// Metric for display
// ============================================================================
struct ChartMetric {
    std::string label;
    std::string value;
    std::string change;
    int positive = 0; // 1=green, -1=red, 0=neutral
};

} // namespace fincept::surface
