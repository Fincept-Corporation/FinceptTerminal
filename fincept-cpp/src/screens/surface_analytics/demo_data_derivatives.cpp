#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



static constexpr int N_STRIKES = 15;
static constexpr float STRIKE_LO = 0.80f;
static constexpr float STRIKE_HI = 1.20f;
static constexpr float BASE_VOL = 0.20f;
static constexpr float PI = 3.14159265f;

static void fill_strikes(std::vector<float>& strikes, float spot) {
    for (int i = 0; i < N_STRIKES; i++) {
        float pct = STRIKE_LO + ((float)i / (float)(N_STRIKES - 1)) * (STRIKE_HI - STRIKE_LO);
        strikes.push_back(std::round(spot * pct));
    }
}

static const std::vector<int> STANDARD_DTES = {7, 14, 21, 30, 45, 60, 90, 120, 180};

GreeksSurfaceData generate_delta_surface(const std::string& symbol, float spot) {
    GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = "Delta";
    fill_strikes(data.strikes, spot);
    data.expirations = STANDARD_DTES;

    for (int dte : data.expirations) {
        std::vector<float> row;
        float t = (float)dte / 365.0f;
        float vol_sqrt_t = BASE_VOL * std::sqrt(t);
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float d1 = std::log(1.0f / moneyness) / vol_sqrt_t + 0.5f * vol_sqrt_t;
            float delta = 0.5f * (1.0f + std::erf(d1 / std::sqrt(2.0f)));
            delta += (demo_randf() - 0.5f) * 0.02f;
            row.push_back(std::max(0.0f, std::min(1.0f, delta)));
        }
        data.z.push_back(row);
    }
    return data;
}

GreeksSurfaceData generate_gamma_surface(const std::string& symbol, float spot) {
    GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = "Gamma";
    fill_strikes(data.strikes, spot);
    data.expirations = STANDARD_DTES;

    for (int dte : data.expirations) {
        std::vector<float> row;
        float t = (float)dte / 365.0f;
        float vol_sqrt_t = BASE_VOL * std::sqrt(t);
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float d1 = std::log(1.0f / moneyness) / vol_sqrt_t + 0.5f * vol_sqrt_t;
            float pdf = std::exp(-0.5f * d1 * d1) / std::sqrt(2.0f * PI);
            float gamma = pdf / (spot * vol_sqrt_t);
            gamma += (demo_randf() - 0.5f) * gamma * 0.05f;
            row.push_back(std::max(0.0f, gamma * 100.0f));
        }
        data.z.push_back(row);
    }
    return data;
}

GreeksSurfaceData generate_vega_surface(const std::string& symbol, float spot) {
    GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = "Vega";
    fill_strikes(data.strikes, spot);
    data.expirations = STANDARD_DTES;

    for (int dte : data.expirations) {
        std::vector<float> row;
        float t = (float)dte / 365.0f;
        float vol_sqrt_t = BASE_VOL * std::sqrt(t);
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float d1 = std::log(1.0f / moneyness) / vol_sqrt_t + 0.5f * vol_sqrt_t;
            float pdf = std::exp(-0.5f * d1 * d1) / std::sqrt(2.0f * PI);
            float vega = spot * pdf * std::sqrt(t);
            vega += (demo_randf() - 0.5f) * vega * 0.05f;
            row.push_back(std::max(0.0f, vega * 0.01f));
        }
        data.z.push_back(row);
    }
    return data;
}

GreeksSurfaceData generate_theta_surface(const std::string& symbol, float spot) {
    GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = "Theta";
    fill_strikes(data.strikes, spot);
    data.expirations = STANDARD_DTES;

    for (int dte : data.expirations) {
        std::vector<float> row;
        float t = (float)dte / 365.0f;
        float vol_sqrt_t = BASE_VOL * std::sqrt(t);
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float d1 = std::log(1.0f / moneyness) / vol_sqrt_t + 0.5f * vol_sqrt_t;
            float pdf = std::exp(-0.5f * d1 * d1) / std::sqrt(2.0f * PI);
            float theta = -(spot * BASE_VOL * pdf) / (2.0f * std::sqrt(t));
            theta /= 365.0f;
            theta += (demo_randf() - 0.5f) * std::abs(theta) * 0.05f;
            row.push_back(theta);
        }
        data.z.push_back(row);
    }
    return data;
}

SkewSurfaceData generate_skew_surface(const std::string& symbol) {
    SkewSurfaceData data;
    data.underlying = symbol;
    data.deltas = {10, 15, 25, 35, 50, 65, 75, 85, 90};
    data.expirations = {7, 14, 21, 30, 45, 60, 90, 120, 180, 365};

    for (int dte : data.expirations) {
        std::vector<float> row;
        float term_factor = 1.0f / std::sqrt((float)dte / 30.0f);
        for (float d : data.deltas) {
            float x = (d - 50.0f) / 50.0f;
            float skew = -5.0f * x * term_factor;
            skew += 2.0f * x * x;
            skew += (demo_randf() - 0.5f) * 0.5f;
            row.push_back(skew);
        }
        data.z.push_back(row);
    }
    return data;
}

LocalVolData generate_local_vol(const std::string& symbol, float spot) {
    LocalVolData data;
    data.underlying = symbol;
    data.spot_price = spot;
    fill_strikes(data.strikes, spot);
    data.expirations = STANDARD_DTES;

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float base = 22.0f;
            float skew = -8.0f * (moneyness - 1.0f);
            float smile = 15.0f * std::pow(moneyness - 1.0f, 2);
            float term = -2.0f * std::log((float)dte / 30.0f);
            float lv = base + skew + smile + term + (demo_randf() - 0.5f) * 1.5f;
            row.push_back(std::max(5.0f, lv));
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
