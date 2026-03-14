#pragma once
// Surface Analytics — data types for volatility surfaces, correlation, yield curves, PCA

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace fincept::surface {

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
// Chart type enum
// ============================================================================
enum class ChartType { Volatility, Correlation, YieldCurve, PCA };

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
