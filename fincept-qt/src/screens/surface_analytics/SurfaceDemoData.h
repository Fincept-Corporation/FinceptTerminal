#pragma once
#include "SurfaceTypes.h"
#include <string>
#include <vector>

namespace fincept::surface {

float demo_randf();

// Equity Derivatives
VolatilitySurfaceData generate_vol_surface(const std::string& symbol = "SPY", float spot = 450.0f);
GreeksSurfaceData generate_delta_surface(const std::string& symbol = "SPY", float spot = 450.0f);
GreeksSurfaceData generate_gamma_surface(const std::string& symbol = "SPY", float spot = 450.0f);
GreeksSurfaceData generate_vega_surface(const std::string& symbol = "SPY", float spot = 450.0f);
GreeksSurfaceData generate_theta_surface(const std::string& symbol = "SPY", float spot = 450.0f);
SkewSurfaceData generate_skew_surface(const std::string& symbol = "SPY");
LocalVolData generate_local_vol(const std::string& symbol = "SPY", float spot = 450.0f);

// Fixed Income
YieldCurveData generate_yield_curve();
SwaptionVolData generate_swaption_vol();
CapFloorVolData generate_capfloor_vol();
BondSpreadData generate_bond_spread();
OISBasisData generate_ois_basis();
RealYieldData generate_real_yield();
ForwardRateData generate_forward_rate();

// FX
FXVolData generate_fx_vol(const std::string& pair = "EUR/USD");
FXForwardPointsData generate_fx_forward_points();
CrossCurrencyBasisData generate_xccy_basis();

// Credit
CDSSpreadData generate_cds_spread();
CreditTransitionData generate_credit_transition();
RecoveryRateData generate_recovery_rate();

// Commodities
CommodityForwardData generate_commodity_forward();
CommodityVolData generate_commodity_vol(const std::string& commodity = "WTI Crude");
CrackSpreadData generate_crack_spread();
ContangoData generate_contango();

// Risk & Portfolio
CorrelationMatrixData generate_correlation(const std::vector<std::string>& assets);
PCAData generate_pca(const std::vector<std::string>& assets);
VaRData generate_var();
StressTestData generate_stress_test();
FactorExposureData generate_factor_exposure(const std::vector<std::string>& assets);
LiquidityData generate_liquidity(const std::string& symbol = "SPY", float spot = 450.0f);
DrawdownData generate_drawdown(const std::vector<std::string>& assets);
BetaData generate_beta(const std::vector<std::string>& assets);
ImpliedDividendData generate_implied_dividend(const std::string& symbol = "SPY", float spot = 450.0f);

// Macro
InflationExpData generate_inflation_expectations();
MonetaryPolicyData generate_monetary_policy();

} // namespace fincept::surface
