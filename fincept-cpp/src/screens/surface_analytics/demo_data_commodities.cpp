#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



CommodityForwardData generate_commodity_forward() {
    CommodityForwardData data;
    data.commodities    = {"WTI Crude", "Brent", "Natural Gas", "Gold", "Silver",
                           "Copper", "Corn", "Soybeans"};
    data.contract_months = {1, 2, 3, 6, 9, 12, 18, 24, 36};

    // Spot prices and contango/backwardation tendency
    const float spots[]  = {75.0f, 78.0f, 3.5f, 2050.0f, 25.0f, 4.2f, 4.5f, 13.0f};
    const float curves[] = {0.003f, 0.003f, -0.005f, 0.002f, 0.001f, 0.001f, -0.003f, -0.002f};

    for (int ci = 0; ci < (int)data.commodities.size(); ci++) {
        std::vector<float> row;
        for (int m : data.contract_months) {
            float fwd = spots[ci] * (1.0f + curves[ci] * (float)m);
            // Add seasonal effect for energy/ags
            if (ci < 3 || ci >= 6) {
                fwd += spots[ci] * 0.02f * std::sin(2.0f * 3.14159f * (float)m / 12.0f);
            }
            fwd += (demo_randf() - 0.5f) * spots[ci] * 0.01f;
            row.push_back(std::max(0.1f, fwd));
        }
        data.z.push_back(row);
    }
    return data;
}

CommodityVolData generate_commodity_vol(const std::string& commodity) {
    CommodityVolData data;
    data.commodity = commodity;
    data.strikes     = {80, 85, 90, 95, 100, 105, 110, 115, 120}; // % of spot
    data.expirations = {7, 14, 30, 60, 90, 120, 180, 270, 365};

    float base_vol = 35.0f; // energy is higher vol than equity
    if (commodity == "Gold") base_vol = 18.0f;
    if (commodity == "Natural Gas") base_vol = 55.0f;

    for (int dte : data.expirations) {
        std::vector<float> row;
        float term = std::sqrt((float)dte / 30.0f) * 2.0f;
        for (float strike : data.strikes) {
            float moneyness = strike / 100.0f;
            float skew = -8.0f * (moneyness - 1.0f); // OTM puts higher for energy
            float smile = 12.0f * std::pow(moneyness - 1.0f, 2);
            float vol = base_vol + term + skew + smile;
            vol += (demo_randf() - 0.5f) * 2.0f;
            row.push_back(std::max(8.0f, vol));
        }
        data.z.push_back(row);
    }
    return data;
}

CrackSpreadData generate_crack_spread() {
    CrackSpreadData data;
    data.spread_types    = {"3-2-1 Crack", "Gasoline Crack", "Heating Oil Crack",
                            "Soy Crush", "Corn Ethanol", "Frac Spread"};
    data.contract_months = {1, 2, 3, 6, 9, 12, 18, 24};

    const float base_spreads[] = {20.0f, 25.0f, 18.0f, 45.0f, 8.0f, 12.0f};

    for (int si = 0; si < (int)data.spread_types.size(); si++) {
        std::vector<float> row;
        for (int m : data.contract_months) {
            float seasonal = base_spreads[si] * 0.15f * std::sin(2.0f * 3.14159f * (float)m / 12.0f);
            float spread = base_spreads[si] + seasonal;
            spread += (demo_randf() - 0.5f) * base_spreads[si] * 0.1f;
            row.push_back(std::max(1.0f, spread));
        }
        data.z.push_back(row);
    }
    return data;
}

ContangoData generate_contango() {
    ContangoData data;
    data.commodities     = {"WTI Crude", "Brent", "Natural Gas", "Gold", "Silver",
                            "Copper", "Corn", "Soybeans"};
    data.contract_months = {1, 2, 3, 6, 9, 12, 18, 24, 36};

    // % deviation from spot: positive = contango, negative = backwardation
    const float tendencies[] = {2.0f, 1.5f, -3.0f, 1.0f, 0.5f, 0.8f, -2.0f, -1.5f};

    for (int ci = 0; ci < (int)data.commodities.size(); ci++) {
        std::vector<float> row;
        for (int m : data.contract_months) {
            float pct = tendencies[ci] * std::sqrt((float)m / 12.0f);
            // Seasonal for ags/energy
            if (ci == 2 || ci >= 6) {
                pct += 3.0f * std::sin(2.0f * 3.14159f * (float)m / 12.0f);
            }
            pct += (demo_randf() - 0.5f) * 1.0f;
            row.push_back(pct);
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
