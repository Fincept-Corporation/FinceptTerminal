#pragma once
// Surface Analytics — data types for 35 financial surface visualizations
// Equity derivatives, fixed income, FX, credit, commodities, risk, macro

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fincept::surface {

// ============================================================================
// Chart type enum — all 35 surfaces
// ============================================================================
enum class ChartType {
    // Equity Derivatives (7)
    Volatility,
    DeltaSurface,
    GammaSurface,
    VegaSurface,
    ThetaSurface,
    SkewSurface,
    LocalVolSurface,
    // Fixed Income (7)
    YieldCurve,
    SwaptionVol,
    CapFloorVol,
    BondSpread,
    OISBasis,
    RealYield,
    ForwardRate,
    // FX (3)
    FXVol,
    FXForwardPoints,
    CrossCurrencyBasis,
    // Credit (3)
    CDSSpread,
    CreditTransition,
    RecoveryRate,
    // Commodities (4)
    CommodityForward,
    CommodityVol,
    CrackSpread,
    ContangoBackwardation,
    // Risk & Portfolio (9)
    Correlation,
    PCA,
    VaR,
    StressTestPnL,
    FactorExposure,
    LiquidityHeatmap,
    Drawdown,
    BetaSurface,
    ImpliedDividend,
    // Macro (2)
    InflationExpectations,
    MonetaryPolicyPath,
};

inline const char* chart_type_name(ChartType t) {
    switch (t) {
        case ChartType::Volatility:
            return "VOL SURFACE";
        case ChartType::DeltaSurface:
            return "DELTA";
        case ChartType::GammaSurface:
            return "GAMMA";
        case ChartType::VegaSurface:
            return "VEGA";
        case ChartType::ThetaSurface:
            return "THETA";
        case ChartType::SkewSurface:
            return "SKEW";
        case ChartType::LocalVolSurface:
            return "LOCAL VOL";
        case ChartType::YieldCurve:
            return "YIELD CURVE";
        case ChartType::SwaptionVol:
            return "SWAPTION VOL";
        case ChartType::CapFloorVol:
            return "CAP/FLOOR";
        case ChartType::BondSpread:
            return "BOND SPREAD";
        case ChartType::OISBasis:
            return "OIS BASIS";
        case ChartType::RealYield:
            return "REAL YIELD";
        case ChartType::ForwardRate:
            return "FWD RATE";
        case ChartType::FXVol:
            return "FX VOL";
        case ChartType::FXForwardPoints:
            return "FX FORWARD";
        case ChartType::CrossCurrencyBasis:
            return "XCCY BASIS";
        case ChartType::CDSSpread:
            return "CDS SPREAD";
        case ChartType::CreditTransition:
            return "TRANSITION";
        case ChartType::RecoveryRate:
            return "RECOVERY";
        case ChartType::CommodityForward:
            return "CMDTY FWD";
        case ChartType::CommodityVol:
            return "CMDTY VOL";
        case ChartType::CrackSpread:
            return "CRACK/CRUSH";
        case ChartType::ContangoBackwardation:
            return "CONTANGO";
        case ChartType::Correlation:
            return "CORRELATION";
        case ChartType::PCA:
            return "PCA";
        case ChartType::VaR:
            return "VAR";
        case ChartType::StressTestPnL:
            return "STRESS TEST";
        case ChartType::FactorExposure:
            return "FACTOR EXP";
        case ChartType::LiquidityHeatmap:
            return "LIQUIDITY";
        case ChartType::Drawdown:
            return "DRAWDOWN";
        case ChartType::BetaSurface:
            return "BETA";
        case ChartType::ImpliedDividend:
            return "IMPL DIV";
        case ChartType::InflationExpectations:
            return "INFLATION";
        case ChartType::MonetaryPolicyPath:
            return "RATE PATH";
    }
    return "UNKNOWN";
}

struct SurfaceCategory {
    const char* name;
    std::vector<ChartType> types;
};

inline std::vector<SurfaceCategory> get_surface_categories() {
    return {
        {"EQUITY DERIV",
         {ChartType::Volatility, ChartType::DeltaSurface, ChartType::GammaSurface, ChartType::VegaSurface,
          ChartType::ThetaSurface, ChartType::SkewSurface, ChartType::LocalVolSurface}},
        {"FIXED INCOME",
         {ChartType::YieldCurve, ChartType::SwaptionVol, ChartType::CapFloorVol, ChartType::BondSpread,
          ChartType::OISBasis, ChartType::RealYield, ChartType::ForwardRate}},
        {"FX", {ChartType::FXVol, ChartType::FXForwardPoints, ChartType::CrossCurrencyBasis}},
        {"CREDIT", {ChartType::CDSSpread, ChartType::CreditTransition, ChartType::RecoveryRate}},
        {"COMMODITIES",
         {ChartType::CommodityForward, ChartType::CommodityVol, ChartType::CrackSpread,
          ChartType::ContangoBackwardation}},
        {"RISK",
         {ChartType::Correlation, ChartType::PCA, ChartType::VaR, ChartType::StressTestPnL, ChartType::FactorExposure,
          ChartType::LiquidityHeatmap, ChartType::Drawdown, ChartType::BetaSurface, ChartType::ImpliedDividend}},
        {"MACRO", {ChartType::InflationExpectations, ChartType::MonetaryPolicyPath}},
    };
}

// ============================================================================
// Data structs for all 35 surfaces
// ============================================================================
struct VolatilitySurfaceData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string underlying;
    float spot_price = 0;
};

struct GreeksSurfaceData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string underlying;
    float spot_price = 0;
    std::string greek_name;
};

struct SkewSurfaceData {
    std::vector<int> expirations;
    std::vector<float> deltas;
    std::vector<std::vector<float>> z;
    std::string underlying;
};

struct LocalVolData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string underlying;
    float spot_price = 0;
};

struct YieldCurveData {
    std::vector<int> time_points;
    std::vector<int> maturities;
    std::vector<std::vector<float>> z;
};

struct SwaptionVolData {
    std::vector<int> option_expiries;
    std::vector<int> swap_tenors;
    std::vector<std::vector<float>> z;
};

struct CapFloorVolData {
    std::vector<float> strikes;
    std::vector<int> maturities;
    std::vector<std::vector<float>> z;
};

struct BondSpreadData {
    std::vector<std::string> ratings;
    std::vector<int> maturities;
    std::vector<std::vector<float>> z;
};

struct OISBasisData {
    std::vector<int> tenors;
    std::vector<int> time_points;
    std::vector<std::vector<float>> z;
};

struct RealYieldData {
    std::vector<int> maturities;
    std::vector<int> time_points;
    std::vector<std::vector<float>> z;
};

struct ForwardRateData {
    std::vector<int> start_tenors;
    std::vector<int> forward_periods;
    std::vector<std::vector<float>> z;
};

struct FXVolData {
    std::vector<float> deltas;
    std::vector<int> tenors;
    std::vector<std::vector<float>> z;
    std::string pair;
};

struct FXForwardPointsData {
    std::vector<std::string> pairs;
    std::vector<int> tenors;
    std::vector<std::vector<float>> z;
};

struct CrossCurrencyBasisData {
    std::vector<std::string> pairs;
    std::vector<int> tenors;
    std::vector<std::vector<float>> z;
};

struct CDSSpreadData {
    std::vector<std::string> entities;
    std::vector<int> tenors;
    std::vector<std::vector<float>> z;
};

struct CreditTransitionData {
    std::vector<std::string> ratings;
    std::vector<std::string> to_ratings;
    std::vector<std::vector<float>> z;
};

struct RecoveryRateData {
    std::vector<std::string> sectors;
    std::vector<std::string> seniorities;
    std::vector<std::vector<float>> z;
};

struct CommodityForwardData {
    std::vector<std::string> commodities;
    std::vector<int> contract_months;
    std::vector<std::vector<float>> z;
};

struct CommodityVolData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string commodity;
};

struct CrackSpreadData {
    std::vector<std::string> spread_types;
    std::vector<int> contract_months;
    std::vector<std::vector<float>> z;
};

struct ContangoData {
    std::vector<std::string> commodities;
    std::vector<int> contract_months;
    std::vector<std::vector<float>> z;
};

struct CorrelationMatrixData {
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z;
    int window = 30;
};

struct PCAData {
    std::vector<std::string> factors;
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z;
    std::vector<float> variance_explained;
};

struct VaRData {
    std::vector<float> confidence_levels;
    std::vector<int> horizons;
    std::vector<std::vector<float>> z;
};

struct StressTestData {
    std::vector<std::string> scenarios;
    std::vector<std::string> portfolios;
    std::vector<std::vector<float>> z;
};

struct FactorExposureData {
    std::vector<std::string> factors;
    std::vector<std::string> assets;
    std::vector<std::vector<float>> z;
};

struct LiquidityData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string underlying;
};

struct DrawdownData {
    std::vector<std::string> assets;
    std::vector<int> windows;
    std::vector<std::vector<float>> z;
};

struct BetaData {
    std::vector<std::string> assets;
    std::vector<int> horizons;
    std::vector<std::vector<float>> z;
};

struct ImpliedDividendData {
    std::vector<float> strikes;
    std::vector<int> expirations;
    std::vector<std::vector<float>> z;
    std::string underlying;
};

struct InflationExpData {
    std::vector<int> horizons;
    std::vector<int> time_points;
    std::vector<std::vector<float>> z;
};

struct MonetaryPolicyData {
    std::vector<std::string> central_banks;
    std::vector<int> meetings_ahead;
    std::vector<std::vector<float>> z;
};

} // namespace fincept::surface
