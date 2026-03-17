#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



SwaptionVolData generate_swaption_vol() {
    SwaptionVolData data;
    data.option_expiries = {1, 3, 6, 12, 24, 36, 60, 120};   // months
    data.swap_tenors     = {12, 24, 36, 60, 84, 120, 180, 240, 360}; // months

    for (int exp : data.option_expiries) {
        std::vector<float> row;
        for (int tenor : data.swap_tenors) {
            // Humped structure: peak around 2-5Y option × 5-10Y swap
            float exp_factor = std::exp(-(float)(exp - 36) * (float)(exp - 36) / 2000.0f);
            float tenor_factor = std::log((float)tenor / 12.0f + 1.0f) * 0.3f;
            float vol = 50.0f + 30.0f * exp_factor + 20.0f * tenor_factor;
            vol += (demo_randf() - 0.5f) * 3.0f;
            row.push_back(std::max(20.0f, vol));
        }
        data.z.push_back(row);
    }
    return data;
}

CapFloorVolData generate_capfloor_vol() {
    CapFloorVolData data;
    data.strikes    = {2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f};
    data.maturities = {12, 24, 36, 60, 84, 120, 180, 240, 360};

    for (int mat : data.maturities) {
        std::vector<float> row;
        float mat_factor = std::sqrt((float)mat / 12.0f);
        for (float strike : data.strikes) {
            float atm_dist = std::abs(strike - 4.25f);
            float smile = 8.0f * atm_dist * atm_dist;
            float vol = 40.0f + 15.0f * mat_factor + smile;
            vol += (demo_randf() - 0.5f) * 2.0f;
            row.push_back(std::max(15.0f, vol));
        }
        data.z.push_back(row);
    }
    return data;
}

BondSpreadData generate_bond_spread() {
    BondSpreadData data;
    data.ratings    = {"AAA", "AA", "A", "BBB", "BB", "B", "CCC"};
    data.maturities = {12, 24, 36, 60, 84, 120, 180, 240, 360};

    // Base spreads by rating (bps)
    const float base_spreads[] = {15, 30, 55, 100, 200, 350, 700};

    for (int ri = 0; ri < (int)data.ratings.size(); ri++) {
        std::vector<float> row;
        for (int mat : data.maturities) {
            float term_premium = base_spreads[ri] * 0.15f * std::log((float)mat / 12.0f + 1.0f);
            float spread = base_spreads[ri] + term_premium;
            spread += (demo_randf() - 0.5f) * base_spreads[ri] * 0.1f;
            row.push_back(std::max(5.0f, spread));
        }
        data.z.push_back(row);
    }
    return data;
}

OISBasisData generate_ois_basis() {
    OISBasisData data;
    data.tenors      = {1, 3, 6, 12, 24, 36, 60, 120, 360};
    data.time_points = {};
    for (int i = 0; i < 60; i++) data.time_points.push_back(i);

    for (int day : data.time_points) {
        std::vector<float> row;
        float regime = std::sin((float)day / 15.0f) * 2.0f;
        for (int tenor : data.tenors) {
            float base = 3.0f + 2.0f * std::exp(-(float)tenor / 12.0f);
            float basis = base + regime + (demo_randf() - 0.5f) * 1.5f;
            row.push_back(basis);
        }
        data.z.push_back(row);
    }
    return data;
}

RealYieldData generate_real_yield() {
    RealYieldData data;
    data.maturities  = {24, 36, 60, 84, 120, 180, 240, 360};
    data.time_points = {};
    for (int i = 0; i < 60; i++) data.time_points.push_back(i);

    const float base_real[] = {-0.5f, -0.2f, 0.3f, 0.7f, 1.1f, 1.4f, 1.6f, 1.8f};

    for (int day : data.time_points) {
        std::vector<float> row;
        float trend = ((float)day - 30.0f) / 100.0f;
        for (int mi = 0; mi < (int)data.maturities.size(); mi++) {
            float y = base_real[mi] + trend + (demo_randf() - 0.5f) * 0.1f;
            row.push_back(y);
        }
        data.z.push_back(row);
    }
    return data;
}

ForwardRateData generate_forward_rate() {
    ForwardRateData data;
    data.start_tenors    = {0, 3, 6, 12, 24, 36, 60, 120};
    data.forward_periods = {3, 6, 12, 24, 36, 60, 120};

    for (int start : data.start_tenors) {
        std::vector<float> row;
        for (int fwd : data.forward_periods) {
            // Nelson-Siegel inspired hump
            float total = (float)(start + fwd) / 12.0f;
            float level = 4.0f;
            float slope = -1.5f * std::exp(-total / 3.0f);
            float hump = 2.0f * total * std::exp(-total / 2.0f);
            float rate = level + slope + hump;
            rate += (demo_randf() - 0.5f) * 0.15f;
            row.push_back(std::max(0.5f, rate));
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
