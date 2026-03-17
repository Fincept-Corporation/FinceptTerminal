#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {

float demo_randf() { return (float)rand() / (float)RAND_MAX; }

static float randf() { return demo_randf(); }

// Generate realistic vol smile/skew
static float vol_smile(float moneyness, float dte, float base_vol = 0.20f) {
    float atm_vol = base_vol * (1.0f + 0.1f * std::exp(-dte / 60.0f));
    float skew = -0.15f * (1.0f - moneyness);
    float smile = 0.08f * std::pow(moneyness - 1.0f, 2) * (1.0f + 30.0f / dte);
    float noise = (randf() - 0.5f) * 0.02f;
    return std::max(0.05f, atm_vol + skew + smile + noise);
}

VolatilitySurfaceData generate_vol_surface(const std::string& symbol, float spot) {
    VolatilitySurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;

    // Strikes: 80% to 120% of spot
    const int n_strikes = 15;
    for (int i = 0; i < n_strikes; i++) {
        float pct = 0.80f + ((float)i / (float)(n_strikes - 1)) * 0.40f;
        data.strikes.push_back(std::round(spot * pct));
    }

    // Expirations in DTE
    data.expirations = {7, 14, 21, 30, 45, 60, 90, 120, 180};

    // IV surface
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float iv = vol_smile(moneyness, (float)dte, 0.18f);
            row.push_back(iv * 100.0f); // percentage
        }
        data.z.push_back(row);
    }

    return data;
}

CorrelationMatrixData generate_correlation(const std::vector<std::string>& assets) {
    CorrelationMatrixData data;
    data.assets = assets;
    data.window = 30;

    // Base correlations for known pairs
    auto base_corr = [](const std::string& a, const std::string& b) -> float {
        if (a == b) return 1.0f;
        // Equity-equity: high positive
        auto is_equity = [](const std::string& s) {
            return s == "SPY" || s == "QQQ" || s == "IWM" || s == "DIA" || s == "EEM" || s == "EFA";
        };
        if (is_equity(a) && is_equity(b)) return 0.85f + (randf() - 0.5f) * 0.1f;
        // Gold-equity: low
        if ((a == "GLD" || b == "GLD") && (is_equity(a) || is_equity(b))) return 0.05f + (randf() - 0.5f) * 0.1f;
        // Bond-equity: negative
        if ((a == "TLT" || a == "IEF" || b == "TLT" || b == "IEF") && (is_equity(a) || is_equity(b)))
            return -0.30f + (randf() - 0.5f) * 0.1f;
        // Bond-bond: positive
        if ((a == "TLT" || a == "IEF") && (b == "TLT" || b == "IEF")) return 0.90f;
        // Default
        return 0.3f + (randf() - 0.5f) * 0.2f;
    };

    int n = (int)assets.size();
    const int n_time = 30;
    for (int t = 0; t < n_time; t++) {
        float regime = std::sin((float)t / 10.0f) * 0.1f;
        std::vector<float> row;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                float c = base_corr(assets[i], assets[j]);
                if (i != j) {
                    c += regime * (1.0f - std::abs(c));
                    c += (randf() - 0.5f) * 0.05f;
                    c = std::max(-0.99f, std::min(0.99f, c));
                }
                row.push_back(c);
            }
        }
        data.z.push_back(row);
    }

    return data;
}

YieldCurveData generate_yield_curve() {
    YieldCurveData data;
    data.maturities = {1, 3, 6, 12, 24, 36, 60, 84, 120, 240, 360};

    float base[] = {5.25f, 5.30f, 5.15f, 4.85f, 4.30f, 4.15f, 4.10f, 4.20f, 4.35f, 4.50f, 4.55f};
    int n_mat = (int)data.maturities.size();
    const int n_days = 60;

    for (int day = 0; day < n_days; day++) {
        float trend = ((float)day - (float)n_days / 2.0f) / (float)n_days * 0.3f;
        float vol = std::sin((float)day / 5.0f) * 0.1f;
        std::vector<float> row;
        for (int i = 0; i < n_mat; i++) {
            float y = base[i] + trend + vol * (1.0f - (float)i / (float)n_mat);
            y += (randf() - 0.5f) * 0.05f;
            y = std::max(0.1f, y);
            row.push_back(y);
        }
        data.z.push_back(row);
    }

    return data;
}

PCAData generate_pca(const std::vector<std::string>& assets) {
    PCAData data;
    data.assets = assets;
    int n_factors = std::min((int)assets.size(), 5);
    for (int i = 0; i < n_factors; i++)
        data.factors.push_back("PC" + std::to_string(i + 1));

    // Realistic factor loading patterns
    struct { const char* asset; float loadings[5]; } patterns[] = {
        {"SPY",  { 0.95f,  0.10f, -0.15f,  0.05f,  0.10f}},
        {"QQQ",  { 0.90f,  0.70f, -0.20f,  0.00f,  0.05f}},
        {"IWM",  { 0.92f, -0.50f, -0.10f,  0.10f,  0.15f}},
        {"GLD",  { 0.15f, -0.05f,  0.65f,  0.20f, -0.10f}},
        {"TLT",  {-0.30f,  0.05f,  0.70f, -0.05f, -0.25f}},
        {"DIA",  { 0.93f,  0.05f, -0.12f,  0.08f,  0.08f}},
        {"EEM",  { 0.85f, -0.30f, -0.25f,  0.80f,  0.15f}},
        {"HYG",  { 0.60f, -0.15f, -0.35f,  0.25f,  0.75f}},
        {"IEF",  {-0.25f,  0.08f,  0.60f, -0.08f, -0.20f}},
        {"EFA",  { 0.82f, -0.20f, -0.18f,  0.70f,  0.10f}},
        {"SLV",  { 0.25f, -0.10f,  0.55f,  0.30f, -0.05f}},
        {"VNQ",  { 0.70f, -0.25f,  0.10f,  0.15f,  0.40f}},
        {"USO",  { 0.40f, -0.15f, -0.05f,  0.50f,  0.30f}},
        {"UNG",  { 0.15f, -0.08f,  0.10f,  0.35f,  0.20f}},
        {"LQD",  { 0.30f,  0.10f,  0.45f, -0.05f,  0.65f}},
    };
    int n_patterns = sizeof(patterns) / sizeof(patterns[0]);

    for (const auto& asset : assets) {
        std::vector<float> row;
        bool found = false;
        for (int p = 0; p < n_patterns; p++) {
            if (asset == patterns[p].asset) {
                for (int f = 0; f < n_factors; f++)
                    row.push_back(patterns[p].loadings[f] + (randf() - 0.5f) * 0.05f);
                found = true;
                break;
            }
        }
        if (!found) {
            for (int f = 0; f < n_factors; f++)
                row.push_back((randf() - 0.5f) * 0.5f);
        }
        data.z.push_back(row);
    }

    data.variance_explained = {0.55f, 0.18f, 0.12f, 0.08f, 0.04f};
    data.variance_explained.resize(n_factors);

    return data;
}

} // namespace fincept::surface
