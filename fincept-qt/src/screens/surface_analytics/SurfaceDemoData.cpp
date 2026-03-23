#include "SurfaceDemoData.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace fincept::surface {

float demo_randf() {
    return (float)rand() / (float)RAND_MAX;
}
static float randf() {
    return demo_randf();
}

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
    const int n_strikes = 15;
    for (int i = 0; i < n_strikes; i++) {
        float pct = 0.80f + ((float)i / (float)(n_strikes - 1)) * 0.40f;
        data.strikes.push_back(std::round(spot * pct));
    }
    data.expirations = {7, 14, 21, 30, 45, 60, 90, 120, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float iv = vol_smile(strike / spot, (float)dte, 0.18f);
            row.push_back(iv * 100.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

GreeksSurfaceData generate_delta_surface(const std::string& symbol, float spot) {
    GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = "Delta";
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {7, 14, 30, 60, 90, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float m = strike / spot;
            float d1 = (std::log(1.0f / m) + 0.5f * 0.20f * 0.20f * (dte / 365.0f)) /
                       (0.20f * std::sqrt(dte / 365.0f + 0.001f));
            float delta = 0.5f * (1.0f + std::erf(d1 / std::sqrt(2.0f)));
            row.push_back(delta + (randf() - 0.5f) * 0.01f);
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
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {7, 14, 30, 60, 90, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float m = strike / spot;
            float t = dte / 365.0f + 0.001f;
            float d1 = (std::log(1.0f / m) + 0.5f * 0.04f * t) / (0.20f * std::sqrt(t));
            float gamma = std::exp(-0.5f * d1 * d1) / (spot * 0.20f * std::sqrt(2.0f * 3.14159f * t));
            row.push_back(gamma + (randf() - 0.5f) * gamma * 0.05f);
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
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {7, 14, 30, 60, 90, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float m = strike / spot;
            float t = dte / 365.0f + 0.001f;
            float d1 = (std::log(1.0f / m) + 0.5f * 0.04f * t) / (0.20f * std::sqrt(t));
            float vega = spot * std::sqrt(t) * std::exp(-0.5f * d1 * d1) / std::sqrt(2.0f * 3.14159f);
            row.push_back(vega / 100.0f + (randf() - 0.5f) * vega * 0.02f / 100.0f);
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
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {7, 14, 30, 60, 90, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float t = dte / 365.0f + 0.001f;
            float m = strike / spot;
            float d1 = (std::log(1.0f / m) + 0.5f * 0.04f * t) / (0.20f * std::sqrt(t));
            float theta =
                -(spot * 0.20f * std::exp(-0.5f * d1 * d1)) / (2.0f * std::sqrt(2.0f * 3.14159f * t) * 365.0f);
            row.push_back(theta + (randf() - 0.5f) * std::abs(theta) * 0.05f);
        }
        data.z.push_back(row);
    }
    return data;
}

SkewSurfaceData generate_skew_surface(const std::string& symbol) {
    SkewSurfaceData data;
    data.underlying = symbol;
    data.deltas = {10, 25, 50, 75, 90};
    data.expirations = {7, 14, 30, 60, 90, 180, 365};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float delta : data.deltas) {
            float base = -0.5f * (50.0f - delta) / 50.0f;
            float term = 0.3f * std::exp(-(float)dte / 120.0f);
            row.push_back(base * term * (1.0f + (randf() - 0.5f) * 0.1f));
        }
        data.z.push_back(row);
    }
    return data;
}

LocalVolData generate_local_vol(const std::string& symbol, float spot) {
    LocalVolData data;
    data.underlying = symbol;
    data.spot_price = spot;
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {30, 60, 90, 120, 180, 360};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float iv = vol_smile(strike / spot, (float)dte, 0.18f);
            float lv = iv * (1.0f + 0.15f * std::pow(strike / spot - 1.0f, 2));
            row.push_back(lv * 100.0f);
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
            row.push_back(std::max(0.1f, y));
        }
        data.z.push_back(row);
    }
    return data;
}

SwaptionVolData generate_swaption_vol() {
    SwaptionVolData data;
    data.option_expiries = {1, 3, 6, 12, 24, 60};
    data.swap_tenors = {12, 24, 36, 60, 84, 120, 240, 360};
    for (int oe : data.option_expiries) {
        std::vector<float> row;
        for (int st : data.swap_tenors) {
            float base = 80.0f + 20.0f * std::exp(-(float)oe / 24.0f) + 10.0f * std::exp(-(float)st / 60.0f);
            row.push_back(base + (randf() - 0.5f) * 5.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

CapFloorVolData generate_capfloor_vol() {
    CapFloorVolData data;
    data.strikes = {3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    data.maturities = {12, 24, 36, 60, 84, 120};
    for (int mat : data.maturities) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float base = 75.0f + 15.0f * std::exp(-(float)mat / 60.0f) - 5.0f * (strike - 5.0f);
            row.push_back(std::max(20.0f, base + (randf() - 0.5f) * 8.0f));
        }
        data.z.push_back(row);
    }
    return data;
}

BondSpreadData generate_bond_spread() {
    BondSpreadData data;
    data.ratings = {"AAA", "AA", "A", "BBB", "BB", "B", "CCC"};
    data.maturities = {12, 24, 36, 60, 84, 120, 240};
    float base_spreads[] = {15, 30, 55, 120, 250, 450, 850};
    for (int i = 0; i < (int)data.ratings.size(); i++) {
        std::vector<float> row;
        for (int mat : data.maturities) {
            float term = 1.0f + 0.2f * std::log((float)mat / 12.0f);
            row.push_back(base_spreads[i] * term + (randf() - 0.5f) * base_spreads[i] * 0.1f);
        }
        data.z.push_back(row);
    }
    return data;
}

OISBasisData generate_ois_basis() {
    OISBasisData data;
    data.tenors = {1, 3, 6, 12, 24, 60};
    const int n_time = 30;
    for (int i = 0; i < n_time; i++)
        data.time_points.push_back(i);
    for (int t = 0; t < n_time; t++) {
        std::vector<float> row;
        float trend = std::sin((float)t / 10.0f) * 2.0f;
        for (int tenor : data.tenors) {
            row.push_back(3.0f + 0.5f * (float)tenor / 12.0f + trend + (randf() - 0.5f) * 0.5f);
        }
        data.z.push_back(row);
    }
    return data;
}

RealYieldData generate_real_yield() {
    RealYieldData data;
    data.maturities = {12, 24, 60, 120, 240, 360};
    const int n_time = 40;
    for (int i = 0; i < n_time; i++)
        data.time_points.push_back(i);
    float base[] = {1.80f, 1.90f, 2.10f, 2.15f, 2.05f, 1.95f};
    for (int t = 0; t < n_time; t++) {
        std::vector<float> row;
        float trend = (float)t / (float)n_time * 0.3f;
        for (int i = 0; i < (int)data.maturities.size(); i++)
            row.push_back(base[i] + trend + (randf() - 0.5f) * 0.08f);
        data.z.push_back(row);
    }
    return data;
}

ForwardRateData generate_forward_rate() {
    ForwardRateData data;
    data.start_tenors = {0, 3, 6, 12, 24, 36, 60};
    data.forward_periods = {3, 6, 12, 24, 36, 60};
    for (int st : data.start_tenors) {
        std::vector<float> row;
        float base = 5.2f - 0.3f * (float)st / 60.0f;
        for (int fp : data.forward_periods) {
            float fwd = base - 0.1f * (float)fp / 24.0f + (randf() - 0.5f) * 0.1f;
            row.push_back(std::max(1.0f, fwd));
        }
        data.z.push_back(row);
    }
    return data;
}

FXVolData generate_fx_vol(const std::string& pair) {
    FXVolData data;
    data.pair = pair;
    data.deltas = {10, 25, 50, 75, 90};
    data.tenors = {7, 14, 30, 60, 90, 180, 365};
    for (int tenor : data.tenors) {
        std::vector<float> row;
        for (float delta : data.deltas) {
            float atm = 7.0f + 2.0f * std::exp(-(float)tenor / 90.0f);
            float skew = -0.3f * (50.0f - delta) / 50.0f;
            float smile = 0.5f * std::pow((delta - 50.0f) / 50.0f, 2);
            row.push_back(atm + skew + smile + (randf() - 0.5f) * 0.3f);
        }
        data.z.push_back(row);
    }
    return data;
}

FXForwardPointsData generate_fx_forward_points() {
    FXForwardPointsData data;
    data.pairs = {"EUR/USD", "GBP/USD", "USD/JPY", "AUD/USD", "USD/CHF", "USD/CAD"};
    data.tenors = {7, 30, 60, 90, 180, 365};
    float base_fwd[] = {-15, -20, 120, -25, 8, 18};
    for (int i = 0; i < (int)data.pairs.size(); i++) {
        std::vector<float> row;
        for (int t = 0; t < (int)data.tenors.size(); t++) {
            float scale = (float)(t + 1);
            row.push_back(base_fwd[i] * scale * (1.0f + (randf() - 0.5f) * 0.1f));
        }
        data.z.push_back(row);
    }
    return data;
}

CrossCurrencyBasisData generate_xccy_basis() {
    CrossCurrencyBasisData data;
    data.pairs = {"EUR/USD", "GBP/USD", "USD/JPY", "AUD/USD", "CHF/USD"};
    data.tenors = {3, 6, 12, 24, 60, 120};
    float base_basis[] = {-15, -8, -25, -30, -12};
    for (int i = 0; i < (int)data.pairs.size(); i++) {
        std::vector<float> row;
        for (int t = 0; t < (int)data.tenors.size(); t++) {
            float curve = 1.0f + 0.3f * std::log(1.0f + (float)data.tenors[t] / 12.0f);
            row.push_back(base_basis[i] * curve + (randf() - 0.5f) * 3.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

CDSSpreadData generate_cds_spread() {
    CDSSpreadData data;
    data.entities = {"Apple", "JPMorgan", "Tesla", "Ford", "Brazil", "Turkey", "Italy", "Greece"};
    data.tenors = {12, 24, 36, 60, 84, 120};
    float base[] = {20, 35, 180, 280, 150, 380, 120, 210};
    for (int i = 0; i < (int)data.entities.size(); i++) {
        std::vector<float> row;
        for (int t = 0; t < (int)data.tenors.size(); t++) {
            float term = 1.0f + 0.1f * t;
            row.push_back(base[i] * term + (randf() - 0.5f) * base[i] * 0.08f);
        }
        data.z.push_back(row);
    }
    return data;
}

CreditTransitionData generate_credit_transition() {
    CreditTransitionData data;
    data.ratings = {"AAA", "AA", "A", "BBB", "BB", "B", "CCC", "D"};
    data.to_ratings = data.ratings;
    // Standard Moody's-like annual transition matrix
    std::vector<std::vector<float>> base = {
        {91.9f, 7.3f, 0.6f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f},   {0.8f, 91.0f, 7.3f, 0.6f, 0.2f, 0.1f, 0.0f, 0.0f},
        {0.1f, 2.2f, 91.2f, 5.7f, 0.5f, 0.2f, 0.1f, 0.0f},   {0.0f, 0.3f, 5.1f, 87.8f, 5.7f, 0.8f, 0.2f, 0.1f},
        {0.0f, 0.1f, 0.4f, 7.4f, 81.7f, 8.2f, 1.6f, 0.6f},   {0.0f, 0.1f, 0.2f, 0.5f, 6.5f, 82.1f, 6.5f, 4.1f},
        {0.0f, 0.0f, 0.2f, 0.8f, 2.1f, 11.2f, 64.5f, 21.2f}, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f},
    };
    for (auto& row : base) {
        std::vector<float> r;
        for (float v : row)
            r.push_back(v + (randf() - 0.5f) * 0.5f);
        data.z.push_back(r);
    }
    return data;
}

RecoveryRateData generate_recovery_rate() {
    RecoveryRateData data;
    data.sectors = {"Technology", "Financials", "Healthcare", "Energy",
                    "Utilities",  "Consumer",   "Industrial", "Real Estate"};
    data.seniorities = {"Senior Secured", "Senior Unsecured", "Subordinated", "Junior"};
    float base_sr[] = {65, 55, 45, 25};
    for (int i = 0; i < (int)data.sectors.size(); i++) {
        std::vector<float> row;
        for (int j = 0; j < (int)data.seniorities.size(); j++) {
            float sector_adj = (randf() - 0.5f) * 10.0f;
            row.push_back(std::max(5.0f, std::min(90.0f, base_sr[j] + sector_adj)));
        }
        data.z.push_back(row);
    }
    return data;
}

CommodityForwardData generate_commodity_forward() {
    CommodityForwardData data;
    data.commodities = {"WTI Crude", "Brent", "Natural Gas", "Gold", "Silver", "Copper", "Corn", "Wheat"};
    data.contract_months = {1, 2, 3, 6, 9, 12, 18, 24};
    float spot[] = {78, 82, 2.8f, 1950, 23, 3.8f, 4.5f, 5.8f};
    for (int i = 0; i < (int)data.commodities.size(); i++) {
        std::vector<float> row;
        float carry = (i < 3) ? 0.005f : -0.002f; // energy in contango, metals in backwardation
        for (int m : data.contract_months) {
            float fwd = spot[i] * std::exp(carry * m) * (1.0f + (randf() - 0.5f) * 0.02f);
            row.push_back(fwd);
        }
        data.z.push_back(row);
    }
    return data;
}

CommodityVolData generate_commodity_vol(const std::string& commodity) {
    CommodityVolData data;
    data.commodity = commodity;
    const int n_strikes = 11;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(0.70f + (float)i / (float)(n_strikes - 1) * 0.60f);
    data.expirations = {30, 60, 90, 120, 180, 360};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float iv = vol_smile(strike, (float)dte, 0.30f);
            row.push_back(iv * 100.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

CrackSpreadData generate_crack_spread() {
    CrackSpreadData data;
    data.spread_types = {"3-2-1 Crack", "5-3-2 Crack", "Soy Crush", "Spark Spread", "Dark Spread"};
    data.contract_months = {1, 2, 3, 6, 9, 12};
    float base[] = {22, 18, 1.2f, 15, 8};
    for (int i = 0; i < (int)data.spread_types.size(); i++) {
        std::vector<float> row;
        for (int m : data.contract_months) {
            float seasonal = std::sin((float)m * 3.14159f / 6.0f) * base[i] * 0.15f;
            row.push_back(base[i] + seasonal + (randf() - 0.5f) * base[i] * 0.1f);
        }
        data.z.push_back(row);
    }
    return data;
}

ContangoData generate_contango() {
    ContangoData data;
    data.commodities = {"WTI Crude", "Brent", "Natural Gas", "Gold", "Silver", "Copper", "Corn", "Wheat"};
    data.contract_months = {1, 2, 3, 6, 9, 12, 18, 24};
    float carry[] = {0.5f, 0.4f, 1.2f, -0.1f, -0.2f, -0.3f, 0.3f, 0.2f};
    for (int i = 0; i < (int)data.commodities.size(); i++) {
        std::vector<float> row;
        for (int m : data.contract_months) {
            row.push_back(carry[i] * m * (1.0f + (randf() - 0.5f) * 0.2f));
        }
        data.z.push_back(row);
    }
    return data;
}

CorrelationMatrixData generate_correlation(const std::vector<std::string>& assets) {
    CorrelationMatrixData data;
    data.assets = assets;
    data.window = 30;
    auto base_corr = [](const std::string& a, const std::string& b) -> float {
        if (a == b)
            return 1.0f;
        auto is_equity = [](const std::string& s) { return s == "SPY" || s == "QQQ" || s == "IWM" || s == "DIA"; };
        if (is_equity(a) && is_equity(b))
            return 0.85f + (demo_randf() - 0.5f) * 0.1f;
        if ((a == "GLD" || b == "GLD") && (is_equity(a) || is_equity(b)))
            return 0.05f + (demo_randf() - 0.5f) * 0.1f;
        if ((a == "TLT" || a == "IEF" || b == "TLT" || b == "IEF") && (is_equity(a) || is_equity(b)))
            return -0.30f + (demo_randf() - 0.5f) * 0.1f;
        if ((a == "TLT" || a == "IEF") && (b == "TLT" || b == "IEF"))
            return 0.90f;
        return 0.3f + (demo_randf() - 0.5f) * 0.2f;
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

PCAData generate_pca(const std::vector<std::string>& assets) {
    PCAData data;
    data.assets = assets;
    int n_factors = std::min((int)assets.size(), 5);
    for (int i = 0; i < n_factors; i++)
        data.factors.push_back("PC" + std::to_string(i + 1));
    struct {
        const char* asset;
        float loadings[5];
    } patterns[] = {
        {"SPY", {0.95f, 0.10f, -0.15f, 0.05f, 0.10f}},   {"QQQ", {0.90f, 0.70f, -0.20f, 0.00f, 0.05f}},
        {"IWM", {0.92f, -0.50f, -0.10f, 0.10f, 0.15f}},  {"GLD", {0.15f, -0.05f, 0.65f, 0.20f, -0.10f}},
        {"TLT", {-0.30f, 0.05f, 0.70f, -0.05f, -0.25f}}, {"DIA", {0.93f, 0.05f, -0.12f, 0.08f, 0.08f}},
        {"HYG", {0.60f, -0.15f, -0.35f, 0.25f, 0.75f}},  {"IEF", {-0.25f, 0.08f, 0.60f, -0.08f, -0.20f}},
    };
    int n_patterns = (int)(sizeof(patterns) / sizeof(patterns[0]));
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
        if (!found)
            for (int f = 0; f < n_factors; f++)
                row.push_back((randf() - 0.5f) * 0.5f);
        data.z.push_back(row);
    }
    data.variance_explained = {0.55f, 0.18f, 0.12f, 0.08f, 0.04f};
    data.variance_explained.resize(n_factors);
    return data;
}

VaRData generate_var() {
    VaRData data;
    data.confidence_levels = {90.0f, 95.0f, 99.0f, 99.5f, 99.9f};
    data.horizons = {1, 5, 10, 20, 60};
    for (float conf : data.confidence_levels) {
        std::vector<float> row;
        float z_score = 1.28f + (conf - 90.0f) * 0.1f;
        for (int h : data.horizons) {
            float var = z_score * 0.01f * std::sqrt((float)h) * (1.0f + (randf() - 0.5f) * 0.1f);
            row.push_back(var * 100.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

StressTestData generate_stress_test() {
    StressTestData data;
    data.scenarios = {"2008 Crisis",  "COVID Crash", "Rate Shock +300bp", "Tech Crash 2000",
                      "EM Contagion", "Oil Shock",   "Inflation Surge",   "Geopolitical"};
    data.portfolios = {"Portfolio A", "Portfolio B", "Portfolio C", "Benchmark"};
    float scenario_severity[] = {-45, -35, -20, -50, -30, -25, -15, -18};
    for (int i = 0; i < (int)data.scenarios.size(); i++) {
        std::vector<float> row;
        for (int j = 0; j < (int)data.portfolios.size(); j++) {
            float beta = 0.8f + (float)j * 0.15f;
            row.push_back(scenario_severity[i] * beta * (1.0f + (randf() - 0.5f) * 0.2f));
        }
        data.z.push_back(row);
    }
    return data;
}

FactorExposureData generate_factor_exposure(const std::vector<std::string>& assets) {
    FactorExposureData data;
    data.factors = {"Market", "Size", "Value", "Momentum", "Low Vol", "Quality", "Yield"};
    data.assets = assets;
    for (const auto& asset : assets) {
        std::vector<float> row;
        // Market factor: most equities high
        row.push_back(0.8f + (randf() - 0.5f) * 0.3f);
        // Size: varies
        row.push_back((randf() - 0.5f) * 1.5f);
        // Value
        row.push_back((randf() - 0.5f) * 1.2f);
        // Momentum
        row.push_back((randf() - 0.5f) * 1.0f);
        // Low Vol
        row.push_back((randf() - 0.5f) * 0.8f);
        // Quality
        row.push_back((randf() - 0.5f) * 0.9f);
        // Yield
        row.push_back((randf() - 0.5f) * 1.1f);
        data.z.push_back(row);
    }
    return data;
}

LiquidityData generate_liquidity(const std::string& symbol, float spot) {
    LiquidityData data;
    data.underlying = symbol;
    const int n_strikes = 13;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.80f + (float)i / (float)(n_strikes - 1) * 0.40f)));
    data.expirations = {7, 14, 30, 60, 90, 180};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float atm_liq = 1.0f - std::abs(strike / spot - 1.0f) * 3.0f;
            float term_decay = std::exp(-(float)dte / 90.0f) * 0.5f + 0.5f;
            float spread = (2.0f - std::max(0.0f, atm_liq)) * 10.0f * (1.0f + (randf() - 0.5f) * 0.3f) / term_decay;
            row.push_back(std::max(1.0f, spread));
        }
        data.z.push_back(row);
    }
    return data;
}

DrawdownData generate_drawdown(const std::vector<std::string>& assets) {
    DrawdownData data;
    data.assets = assets;
    data.windows = {5, 10, 21, 63, 126, 252};
    float asset_vol[] = {0.15f, 0.20f, 0.25f, 0.08f, 0.12f, 0.18f, 0.22f, 0.30f};
    for (int i = 0; i < (int)assets.size(); i++) {
        std::vector<float> row;
        float v = asset_vol[i % 8];
        for (int w : data.windows) {
            float dd = -v * std::sqrt((float)w / 252.0f) * 2.5f * (1.0f + (randf() - 0.5f) * 0.3f);
            row.push_back(dd * 100.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

BetaData generate_beta(const std::vector<std::string>& assets) {
    BetaData data;
    data.assets = assets;
    data.horizons = {5, 10, 21, 63, 126, 252};
    float base_betas[] = {1.0f, 1.15f, 1.25f, 0.10f, -0.15f, 1.05f, 1.30f, 0.60f};
    for (int i = 0; i < (int)assets.size(); i++) {
        std::vector<float> row;
        for (int h : data.horizons) {
            float mean_rev = 1.0f + (base_betas[i % 8] - 1.0f) * std::exp(-(float)h / 126.0f);
            row.push_back(mean_rev + (randf() - 0.5f) * 0.1f);
        }
        data.z.push_back(row);
    }
    return data;
}

ImpliedDividendData generate_implied_dividend(const std::string& symbol, float spot) {
    ImpliedDividendData data;
    data.underlying = symbol;
    const int n_strikes = 11;
    for (int i = 0; i < n_strikes; i++)
        data.strikes.push_back(std::round(spot * (0.85f + (float)i / (float)(n_strikes - 1) * 0.30f)));
    data.expirations = {30, 60, 90, 120, 180, 360};
    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float base_div = 1.5f + 0.5f * (float)dte / 365.0f;
            float skew = 0.2f * std::abs(strike / spot - 1.0f);
            row.push_back(base_div + skew + (randf() - 0.5f) * 0.2f);
        }
        data.z.push_back(row);
    }
    return data;
}

InflationExpData generate_inflation_expectations() {
    InflationExpData data;
    data.horizons = {1, 2, 3, 5, 7, 10, 20, 30};
    const int n_time = 40;
    for (int i = 0; i < n_time; i++)
        data.time_points.push_back(i);
    float base[] = {3.8f, 3.4f, 3.0f, 2.7f, 2.5f, 2.4f, 2.35f, 2.30f};
    for (int t = 0; t < n_time; t++) {
        std::vector<float> row;
        float trend = std::sin((float)t / 8.0f) * 0.2f;
        for (int i = 0; i < (int)data.horizons.size(); i++)
            row.push_back(base[i] + trend * std::exp(-(float)i / 4.0f) + (randf() - 0.5f) * 0.05f);
        data.z.push_back(row);
    }
    return data;
}

MonetaryPolicyData generate_monetary_policy() {
    MonetaryPolicyData data;
    data.central_banks = {"Fed", "ECB", "BoJ", "BoE", "PBoC"};
    data.meetings_ahead = {1, 2, 3, 4, 5, 6, 7, 8};
    float current_rates[] = {5.25f, 4.50f, 0.10f, 5.25f, 3.45f};
    float expected_cuts[] = {-0.25f, -0.25f, 0.0f, -0.25f, -0.10f};
    for (int i = 0; i < (int)data.central_banks.size(); i++) {
        std::vector<float> row;
        float rate = current_rates[i];
        for (int m : data.meetings_ahead) {
            rate += expected_cuts[i] * (randf() > 0.5f ? 1.0f : 0.0f);
            row.push_back(rate + (randf() - 0.5f) * 0.05f);
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
