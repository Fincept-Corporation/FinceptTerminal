#pragma once
// Demo data generators for Surface Analytics
// Generates realistic synthetic data when no API is available

#include "surface_types.h"

namespace fincept::surface {

VolatilitySurfaceData generate_vol_surface(const std::string& symbol = "SPY", float spot = 450.0f);
CorrelationMatrixData generate_correlation(const std::vector<std::string>& assets);
YieldCurveData generate_yield_curve();
PCAData generate_pca(const std::vector<std::string>& assets);

} // namespace fincept::surface
