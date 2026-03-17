#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



VaRData generate_var() {
    VaRData data;
    data.confidence_levels = {90.0f, 95.0f, 97.5f, 99.0f, 99.5f, 99.9f};
    data.horizons          = {1, 2, 5, 10, 21, 63, 126, 252};

    for (float conf : data.confidence_levels) {
        std::vector<float> row;
        // z-score approximation for confidence level
        float z = 1.28f; // 90%
        if (conf >= 95.0f)  z = 1.645f;
        if (conf >= 97.5f)  z = 1.96f;
        if (conf >= 99.0f)  z = 2.33f;
        if (conf >= 99.5f)  z = 2.58f;
        if (conf >= 99.9f)  z = 3.09f;

        for (int h : data.horizons) {
            float daily_vol = 0.012f; // ~1.2% daily vol
            float var = z * daily_vol * std::sqrt((float)h) * 100.0f;
            var += (demo_randf() - 0.5f) * var * 0.05f;
            row.push_back(var);
        }
        data.z.push_back(row);
    }
    return data;
}

StressTestData generate_stress_test() {
    StressTestData data;
    data.scenarios  = {"2008 GFC", "COVID Mar20", "Rate Shock +300bp",
                       "Tech Crash -40%", "Inflation Surge", "Geopolitical",
                       "Liquidity Crisis", "Credit Contagion"};
    data.portfolios = {"60/40 Balanced", "All Equity", "Risk Parity",
                       "HF Multi-Strat", "Credit Long", "Macro", "Vol Short"};

    // P&L impact matrix (%)
    const float impacts[8][7] = {
        {-28.0f, -45.0f, -20.0f, -18.0f, -35.0f, -5.0f,  -60.0f}, // GFC
        {-15.0f, -30.0f, -12.0f, -10.0f, -20.0f,  8.0f,  -45.0f}, // COVID
        {-12.0f,  -8.0f, -10.0f,  -5.0f, -15.0f,  3.0f,  -15.0f}, // Rate Shock
        {-18.0f, -35.0f, -15.0f, -12.0f,  -5.0f, -2.0f,  -25.0f}, // Tech Crash
        { -8.0f,  -5.0f,  -6.0f,  -3.0f, -10.0f, 12.0f,  -10.0f}, // Inflation
        {-10.0f, -15.0f,  -8.0f,  -6.0f,  -8.0f,  5.0f,  -20.0f}, // Geopolitical
        {-20.0f, -25.0f, -15.0f, -22.0f, -30.0f, -8.0f,  -50.0f}, // Liquidity
        {-15.0f, -10.0f, -12.0f, -14.0f, -40.0f, -3.0f,  -30.0f}, // Credit
    };

    for (int si = 0; si < 8; si++) {
        std::vector<float> row;
        for (int pi = 0; pi < 7; pi++) {
            float pnl = impacts[si][pi] + (demo_randf() - 0.5f) * 3.0f;
            row.push_back(pnl);
        }
        data.z.push_back(row);
    }
    return data;
}

FactorExposureData generate_factor_exposure(const std::vector<std::string>& assets) {
    FactorExposureData data;
    data.factors = {"Market", "Size", "Value", "Momentum", "Quality", "Volatility"};
    data.assets = assets;

    // Fama-French style loadings
    struct { const char* asset; float loadings[6]; } patterns[] = {
        {"SPY",  { 1.00f,  0.00f,  0.00f,  0.05f,  0.10f, -0.05f}},
        {"QQQ",  { 1.10f, -0.30f, -0.40f,  0.20f,  0.15f,  0.10f}},
        {"IWM",  { 1.15f,  0.80f,  0.30f, -0.10f, -0.15f,  0.20f}},
        {"DIA",  { 0.95f,  0.10f,  0.25f, -0.05f,  0.20f, -0.10f}},
        {"GLD",  { 0.05f,  0.00f,  0.10f, -0.05f,  0.00f,  0.30f}},
        {"TLT",  {-0.20f, -0.05f,  0.00f,  0.05f,  0.10f,  0.15f}},
        {"IEF",  {-0.10f, -0.02f,  0.00f,  0.03f,  0.08f,  0.08f}},
        {"HYG",  { 0.40f,  0.10f,  0.20f, -0.05f, -0.25f,  0.15f}},
    };
    constexpr int n_patterns = sizeof(patterns) / sizeof(patterns[0]);

    for (const auto& asset : assets) {
        std::vector<float> row;
        bool found = false;
        for (int p = 0; p < n_patterns; p++) {
            if (asset == patterns[p].asset) {
                for (int f = 0; f < 6; f++)
                    row.push_back(patterns[p].loadings[f] + (demo_randf() - 0.5f) * 0.05f);
                found = true;
                break;
            }
        }
        if (!found) {
            for (int f = 0; f < 6; f++)
                row.push_back((demo_randf() - 0.5f) * 0.5f);
        }
        data.z.push_back(row);
    }
    return data;
}

LiquidityData generate_liquidity(const std::string& symbol, float spot) {
    LiquidityData data;
    data.underlying = symbol;
    data.expirations = {7, 14, 21, 30, 45, 60, 90, 120, 180};

    constexpr int n_strikes = 15;
    for (int i = 0; i < n_strikes; i++) {
        float pct = 0.80f + ((float)i / (float)(n_strikes - 1)) * 0.40f;
        data.strikes.push_back(std::round(spot * pct));
    }

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            float otm_dist = std::abs(moneyness - 1.0f);
            // Tighter spreads at ATM, wider OTM; tighter at longer DTE
            float base_spread = 0.5f + 8.0f * otm_dist * otm_dist;
            float dte_factor = 1.0f + 2.0f * std::exp(-(float)dte / 20.0f);
            float spread = base_spread * dte_factor;
            spread += (demo_randf() - 0.5f) * 0.3f;
            row.push_back(std::max(0.1f, spread));
        }
        data.z.push_back(row);
    }
    return data;
}

DrawdownData generate_drawdown(const std::vector<std::string>& assets) {
    DrawdownData data;
    data.assets  = assets;
    data.windows = {5, 10, 21, 63, 126, 252, 504, 756};

    // Base max drawdown by asset type
    auto base_dd = [](const std::string& a) -> float {
        if (a == "GLD" || a == "TLT" || a == "IEF") return 8.0f;
        if (a == "HYG") return 12.0f;
        return 18.0f; // equities
    };

    for (const auto& asset : assets) {
        std::vector<float> row;
        float base = base_dd(asset);
        for (int w : data.windows) {
            float dd = base * std::sqrt((float)w / 252.0f);
            dd = std::min(dd, base * 2.5f);
            dd += (demo_randf() - 0.5f) * base * 0.1f;
            row.push_back(std::max(0.5f, dd));
        }
        data.z.push_back(row);
    }
    return data;
}

BetaData generate_beta(const std::vector<std::string>& assets) {
    BetaData data;
    data.assets   = assets;
    data.horizons = {21, 42, 63, 126, 252, 504, 756};

    auto base_beta = [](const std::string& a) -> float {
        if (a == "QQQ") return 1.15f;
        if (a == "IWM") return 1.20f;
        if (a == "DIA") return 0.95f;
        if (a == "GLD") return 0.05f;
        if (a == "TLT") return -0.25f;
        if (a == "IEF") return -0.15f;
        if (a == "HYG") return 0.40f;
        return 1.0f; // SPY
    };

    for (const auto& asset : assets) {
        std::vector<float> row;
        float base = base_beta(asset);
        for (int h : data.horizons) {
            float beta = base + (demo_randf() - 0.5f) * 0.15f;
            // Slightly mean-reverting at longer horizons
            beta += (1.0f - base) * 0.05f * std::log((float)h / 21.0f);
            row.push_back(beta);
        }
        data.z.push_back(row);
    }
    return data;
}

ImpliedDividendData generate_implied_dividend(const std::string& symbol, float spot) {
    ImpliedDividendData data;
    data.underlying = symbol;
    data.expirations = {30, 60, 90, 120, 150, 180, 270, 365};

    constexpr int n_strikes = 12;
    for (int i = 0; i < n_strikes; i++) {
        float pct = 0.85f + ((float)i / (float)(n_strikes - 1)) * 0.30f;
        data.strikes.push_back(std::round(spot * pct));
    }

    float base_yield = 1.4f; // ~1.4% annualized div yield
    if (symbol == "AAPL") base_yield = 0.5f;
    if (symbol == "TSLA") base_yield = 0.0f;

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float strike : data.strikes) {
            float moneyness = strike / spot;
            // Implied div is higher near ATM, noisy OTM
            float otm_noise = 0.3f * std::abs(moneyness - 1.0f);
            float div = base_yield + otm_noise + (demo_randf() - 0.5f) * 0.15f;
            // Higher near ex-date months (quarterly pattern)
            float quarterly = 0.2f * std::sin(2.0f * 3.14159f * (float)dte / 90.0f);
            div += quarterly;
            row.push_back(std::max(0.0f, div));
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
