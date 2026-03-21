#pragma once
#include "SurfaceTypes.h"
#include <QString>
#include <vector>
#include <string>

namespace fincept::surface {

// Parse a CSV file into rows of string columns
std::vector<std::vector<std::string>> parse_csv_file(const QString& path, std::string& out_error);

// Per-surface CSV loaders — all 35
bool load_vol_surface(    const std::vector<std::vector<std::string>>& rows, VolatilitySurfaceData& out, std::string& err);
bool load_greeks_surface( const std::vector<std::vector<std::string>>& rows, GreeksSurfaceData& out,     std::string& err, const std::string& greek_name);
bool load_skew_surface(   const std::vector<std::vector<std::string>>& rows, SkewSurfaceData& out,       std::string& err);
bool load_local_vol(      const std::vector<std::vector<std::string>>& rows, LocalVolData& out,          std::string& err);
bool load_yield_curve(    const std::vector<std::vector<std::string>>& rows, YieldCurveData& out,        std::string& err);
bool load_swaption_vol(   const std::vector<std::vector<std::string>>& rows, SwaptionVolData& out,       std::string& err);
bool load_capfloor_vol(   const std::vector<std::vector<std::string>>& rows, CapFloorVolData& out,       std::string& err);
bool load_bond_spread(    const std::vector<std::vector<std::string>>& rows, BondSpreadData& out,        std::string& err);
bool load_ois_basis(      const std::vector<std::vector<std::string>>& rows, OISBasisData& out,          std::string& err);
bool load_real_yield(     const std::vector<std::vector<std::string>>& rows, RealYieldData& out,         std::string& err);
bool load_forward_rate(   const std::vector<std::vector<std::string>>& rows, ForwardRateData& out,       std::string& err);
bool load_fx_vol(         const std::vector<std::vector<std::string>>& rows, FXVolData& out,             std::string& err);
bool load_fx_forward(     const std::vector<std::vector<std::string>>& rows, FXForwardPointsData& out,   std::string& err);
bool load_xccy_basis(     const std::vector<std::vector<std::string>>& rows, CrossCurrencyBasisData& out,std::string& err);
bool load_cds_spread(     const std::vector<std::vector<std::string>>& rows, CDSSpreadData& out,         std::string& err);
bool load_credit_trans(   const std::vector<std::vector<std::string>>& rows, CreditTransitionData& out,  std::string& err);
bool load_recovery_rate(  const std::vector<std::vector<std::string>>& rows, RecoveryRateData& out,      std::string& err);
bool load_cmdty_forward(  const std::vector<std::vector<std::string>>& rows, CommodityForwardData& out,  std::string& err);
bool load_cmdty_vol(      const std::vector<std::vector<std::string>>& rows, CommodityVolData& out,      std::string& err);
bool load_crack_spread(   const std::vector<std::vector<std::string>>& rows, CrackSpreadData& out,       std::string& err);
bool load_contango(       const std::vector<std::vector<std::string>>& rows, ContangoData& out,          std::string& err);
bool load_correlation(    const std::vector<std::vector<std::string>>& rows, CorrelationMatrixData& out, std::string& err);
bool load_pca(            const std::vector<std::vector<std::string>>& rows, PCAData& out,               std::string& err);
bool load_var(            const std::vector<std::vector<std::string>>& rows, VaRData& out,               std::string& err);
bool load_stress_test(    const std::vector<std::vector<std::string>>& rows, StressTestData& out,        std::string& err);
bool load_factor_exposure(const std::vector<std::vector<std::string>>& rows, FactorExposureData& out,    std::string& err);
bool load_liquidity(      const std::vector<std::vector<std::string>>& rows, LiquidityData& out,         std::string& err);
bool load_drawdown(       const std::vector<std::vector<std::string>>& rows, DrawdownData& out,          std::string& err);
bool load_beta(           const std::vector<std::vector<std::string>>& rows, BetaData& out,              std::string& err);
bool load_implied_div(    const std::vector<std::vector<std::string>>& rows, ImpliedDividendData& out,   std::string& err);
bool load_inflation(      const std::vector<std::vector<std::string>>& rows, InflationExpData& out,      std::string& err);
bool load_monetary(       const std::vector<std::vector<std::string>>& rows, MonetaryPolicyData& out,    std::string& err);

} // namespace fincept::surface
