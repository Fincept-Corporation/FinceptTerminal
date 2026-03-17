#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



FXVolData generate_fx_vol(const std::string& pair) {
    FXVolData data;
    data.pair = pair;
    data.deltas = {10, 15, 25, 35, 50, 65, 75, 85, 90};
    data.tenors = {7, 14, 30, 60, 90, 180, 270, 365};

    // Base ATM vol depends on pair
    float base_vol = 8.5f; // EUR/USD typical
    if (pair == "USD/JPY") base_vol = 10.0f;
    if (pair == "GBP/USD") base_vol = 9.5f;
    if (pair == "USD/MXN") base_vol = 14.0f;

    for (int tenor : data.tenors) {
        std::vector<float> row;
        float term_adj = std::sqrt((float)tenor / 30.0f) * 0.5f;
        for (float d : data.deltas) {
            float x = (d - 50.0f) / 50.0f;
            // FX smile is more symmetric than equity
            float smile = 2.5f * x * x;
            // Slight risk reversal (puts slightly more expensive)
            float rr = -0.3f * x;
            float vol = base_vol + term_adj + smile + rr;
            vol += (demo_randf() - 0.5f) * 0.3f;
            row.push_back(std::max(3.0f, vol));
        }
        data.z.push_back(row);
    }
    return data;
}

FXForwardPointsData generate_fx_forward_points() {
    FXForwardPointsData data;
    data.pairs  = {"EUR/USD", "USD/JPY", "GBP/USD", "AUD/USD", "USD/CAD",
                   "USD/CHF", "NZD/USD", "USD/MXN"};
    data.tenors = {7, 14, 30, 60, 90, 180, 270, 365};

    // Forward points driven by rate differentials (pips)
    const float base_points[] = {2, -30, 5, -15, 10, 8, -12, -80};

    for (int pi = 0; pi < (int)data.pairs.size(); pi++) {
        std::vector<float> row;
        for (int tenor : data.tenors) {
            float scale = (float)tenor / 365.0f;
            float pts = base_points[pi] * scale;
            pts += (demo_randf() - 0.5f) * std::abs(base_points[pi]) * 0.1f * scale;
            row.push_back(pts);
        }
        data.z.push_back(row);
    }
    return data;
}

CrossCurrencyBasisData generate_xccy_basis() {
    CrossCurrencyBasisData data;
    data.pairs  = {"EUR/USD", "JPY/USD", "GBP/USD", "CHF/USD", "AUD/USD",
                   "CAD/USD", "KRW/USD", "CNY/USD"};
    data.tenors = {3, 6, 12, 24, 36, 60, 120, 360};

    // Negative basis = dollar funding premium
    const float base_basis[] = {-15, -25, -8, -20, -5, -3, -35, -30};

    for (int pi = 0; pi < (int)data.pairs.size(); pi++) {
        std::vector<float> row;
        for (int tenor : data.tenors) {
            float term = base_basis[pi] * (1.0f + 0.3f * std::log((float)tenor / 12.0f + 1.0f));
            term += (demo_randf() - 0.5f) * 5.0f;
            row.push_back(term);
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
