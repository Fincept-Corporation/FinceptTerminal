// SKIP_UNITY_BUILD_INCLUSION
#include "csv_importer.h"
#include "csv_importer_helpers.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace fincept::surface {


// ============================================================================
// Schema table — one entry per ChartType (35 total)
// ============================================================================
static const CsvSchema SCHEMAS[] = {
    {
        ChartType::Volatility, "VOL SURFACE",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"iv_pct","Implied volatility %"}},
        "450.0,30,22.5"
    },
    {
        ChartType::DeltaSurface, "DELTA",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"delta","Option delta"}},
        "450.0,30,0.52"
    },
    {
        ChartType::GammaSurface, "GAMMA",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"gamma","Option gamma"}},
        "450.0,30,0.0031"
    },
    {
        ChartType::VegaSurface, "VEGA",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"vega","Option vega"}},
        "450.0,30,0.85"
    },
    {
        ChartType::ThetaSurface, "THETA",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"theta","Option theta"}},
        "450.0,30,-0.12"
    },
    {
        ChartType::SkewSurface, "SKEW",
        {{"dte","Days to expiry"},{"delta","Option delta"},{"skew_pct","Skew value %"}},
        "30,25,-1.2"
    },
    {
        ChartType::LocalVolSurface, "LOCAL VOL",
        {{"strike","Underlying strike price"},{"dte","Days to expiry"},{"local_vol_pct","Local vol %"}},
        "450.0,30,21.0"
    },
    {
        ChartType::YieldCurve, "YIELD CURVE",
        {{"maturity_months","Maturity in months"},{"date","Observation date"},{"yield_pct","Yield %"}},
        "24,2024-01-15,4.35"
    },
    {
        ChartType::SwaptionVol, "SWAPTION VOL",
        {{"option_expiry_months","Option expiry in months"},{"swap_tenor_months","Swap tenor in months"},{"vol_bps","Volatility in bps"}},
        "12,60,85"
    },
    {
        ChartType::CapFloorVol, "CAP/FLOOR VOL",
        {{"maturity_months","Maturity in months"},{"strike_pct","Strike rate %"},{"vol_bps","Volatility in bps"}},
        "24,4.5,62"
    },
    {
        ChartType::BondSpread, "BOND SPREAD",
        {{"rating","Credit rating"},{"maturity_months","Maturity in months"},{"spread_bps","Spread in bps"}},
        "BBB,60,120"
    },
    {
        ChartType::OISBasis, "OIS BASIS",
        {{"day","Observation day index"},{"tenor_months","Tenor in months"},{"basis_bps","Basis in bps"}},
        "0,12,2.5"
    },
    {
        ChartType::RealYield, "REAL YIELD",
        {{"day","Observation day index"},{"maturity_months","Maturity in months"},{"yield_pct","Real yield %"}},
        "0,60,1.82"
    },
    {
        ChartType::ForwardRate, "FWD RATE",
        {{"start_months","Forward start in months"},{"fwd_period_months","Forward period in months"},{"rate_pct","Forward rate %"}},
        "12,12,4.8"
    },
    {
        ChartType::FXVol, "FX VOL",
        {{"tenor_days","Tenor in days"},{"delta","Option delta"},{"vol_pct","Volatility %"}},
        "30,25,7.2"
    },
    {
        ChartType::FXForwardPoints, "FX FORWARD",
        {{"pair","Currency pair"},{"tenor_days","Tenor in days"},{"pips","Forward points in pips"}},
        "EUR/USD,30,-12.5"
    },
    {
        ChartType::CrossCurrencyBasis, "XCCY BASIS",
        {{"pair","Currency pair"},{"tenor_months","Tenor in months"},{"basis_bps","Basis in bps"}},
        "EUR,12,-22"
    },
    {
        ChartType::CDSSpread, "CDS SPREAD",
        {{"entity","Entity name"},{"tenor_months","Tenor in months"},{"spread_bps","CDS spread in bps"}},
        "Apple Inc,60,45"
    },
    {
        ChartType::CreditTransition, "TRANSITION MATRIX",
        {{"from_rating","Source credit rating"},{"to_rating","Destination credit rating"},{"probability_pct","Transition probability %"}},
        "AAA,AA,8.2"
    },
    {
        ChartType::RecoveryRate, "RECOVERY RATE",
        {{"sector","Industry sector"},{"seniority","Debt seniority"},{"recovery_pct","Recovery rate %"}},
        "Technology,Senior,52.0"
    },
    {
        ChartType::CommodityForward, "CMDTY FORWARD",
        {{"commodity","Commodity name"},{"contract_months","Contract months forward"},{"price","Forward price"}},
        "WTI Crude,3,78.50"
    },
    {
        ChartType::CommodityVol, "CMDTY VOL",
        {{"dte","Days to expiry"},{"strike_pct","Strike % of spot"},{"vol_pct","Volatility %"}},
        "30,100,32.5"
    },
    {
        ChartType::CrackSpread, "CRACK/CRUSH SPREAD",
        {{"spread_type","Spread type name"},{"contract_months","Contract months forward"},{"spread_usd","Spread in USD/unit"}},
        "3-2-1 Crack,3,24.5"
    },
    {
        ChartType::ContangoBackwardation, "CONTANGO/BACKWARDATION",
        {{"commodity","Commodity name"},{"contract_months","Contract months forward"},{"pct_from_spot","Percent from spot"}},
        "WTI Crude,3,1.8"
    },
    {
        ChartType::Correlation, "CORRELATION",
        {{"asset_i","First asset"},{"asset_j","Second asset"},{"correlation","Correlation coefficient"}},
        "SPY,QQQ,0.92"
    },
    {
        ChartType::PCA, "PCA LOADINGS",
        {{"asset","Asset name"},{"factor","PCA factor"},{"loading","Factor loading"}},
        "SPY,PC1,0.45"
    },
    {
        ChartType::VaR, "VAR",
        {{"confidence_pct","Confidence level %"},{"horizon_days","Horizon in days"},{"var_pct","VaR %"}},
        "99,1,2.35"
    },
    {
        ChartType::StressTestPnL, "STRESS TEST",
        {{"scenario","Stress scenario name"},{"portfolio","Portfolio name"},{"pnl_pct","P&L %"}},
        "2008 Crisis,Core Portfolio,-18.5"
    },
    {
        ChartType::FactorExposure, "FACTOR EXPOSURE",
        {{"asset","Asset name"},{"factor","Risk factor"},{"exposure","Factor exposure"}},
        "AAPL,Market,1.12"
    },
    {
        ChartType::LiquidityHeatmap, "LIQUIDITY",
        {{"dte","Days to expiry"},{"strike","Strike price"},{"spread","Bid-ask spread"}},
        "30,450,0.15"
    },
    {
        ChartType::Drawdown, "DRAWDOWN",
        {{"asset","Asset name"},{"window_days","Rolling window in days"},{"max_dd_pct","Max drawdown %"}},
        "SPY,252,-19.4"
    },
    {
        ChartType::BetaSurface, "BETA",
        {{"asset","Asset name"},{"horizon_days","Rolling horizon in days"},{"beta","Beta value"}},
        "AAPL,60,1.18"
    },
    {
        ChartType::ImpliedDividend, "IMPLIED DIVIDEND",
        {{"dte","Days to expiry"},{"strike","Strike price"},{"div_yield_pct","Implied dividend yield %"}},
        "60,450,0.58"
    },
    {
        ChartType::InflationExpectations, "INFLATION EXPECTATIONS",
        {{"day","Observation day index"},{"horizon_years","Horizon in years"},{"bei_pct","Breakeven inflation %"}},
        "0,5,2.38"
    },
    {
        ChartType::MonetaryPolicyPath, "RATE PATH",
        {{"central_bank","Central bank name"},{"meeting_num","Meeting number ahead"},{"rate_pct","Implied rate %"}},
        "Fed,1,5.25"
    },
};

static constexpr int SCHEMAS_COUNT = static_cast<int>(sizeof(SCHEMAS) / sizeof(SCHEMAS[0]));

// ============================================================================
// get_csv_schema
// ============================================================================
const CsvSchema& get_csv_schema(ChartType type) {
    for (int i = 0; i < SCHEMAS_COUNT; ++i) {
        if (SCHEMAS[i].type == type) {
            return SCHEMAS[i];
        }
    }
    return SCHEMAS[0]; // fallback
}

// ============================================================================
// parse_csv_file
// ============================================================================
std::vector<std::vector<std::string>> parse_csv_file(
    const std::string& path,
    std::string& out_error)
{
    std::vector<std::vector<std::string>> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        out_error = "Cannot open file: " + path;
        return result;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        result.push_back(split_csv_line(t));
    }
    if (result.empty()) {
        out_error = "File is empty or has no valid rows: " + path;
    }
    return result;
}

// ============================================================================
// 1. Volatility Surface
// ============================================================================
bool load_vol_surface(
    const std::vector<std::vector<std::string>>& rows,
    VolatilitySurfaceData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_strike = col_idx(hdr, "strike");
    int ci_dte    = col_idx(hdr, "dte");
    int ci_iv     = col_idx(hdr, "iv_pct");
    if (ci_strike < 0 || ci_dte < 0 || ci_iv < 0) {
        err = "Missing required columns: strike, dte, iv_pct"; return false;
    }
    int max_ci = std::max({ci_strike, ci_dte, ci_iv});

    std::map<int, std::map<float, float>> grid; // dte -> strike -> iv
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            float strike = to_f(row[ci_strike]);
            int   dte    = to_i(row[ci_dte]);
            float iv     = to_f(row[ci_iv]);
            grid[dte][strike] = iv;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    // Collect sorted unique keys
    std::vector<int> exp_keys;
    std::vector<float> strike_keys;
    for (auto& [dte, sm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [k, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), k) == strike_keys.end())
                strike_keys.push_back(k);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.expirations = exp_keys;
    out.strikes     = strike_keys;
    out.underlying  = "IMPORTED";
    out.spot_price  = strike_keys.empty() ? 0.f : strike_keys[strike_keys.size() / 2];
    out.z.assign(exp_keys.size(), std::vector<float>(strike_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < exp_keys.size(); ++ei) {
        int dte = exp_keys[ei];
        for (std::size_t si = 0; si < strike_keys.size(); ++si) {
            auto it = grid[dte].find(strike_keys[si]);
            if (it != grid[dte].end()) out.z[ei][si] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 2. Greeks Surfaces (Delta, Gamma, Vega, Theta)
// ============================================================================
bool load_greeks_surface(
    const std::vector<std::vector<std::string>>& rows,
    GreeksSurfaceData& out,
    std::string& err,
    const std::string& greek_name)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_strike = col_idx(hdr, "strike");
    int ci_dte    = col_idx(hdr, "dte");
    // Value column name matches greek: delta, gamma, vega, theta
    std::string val_col = greek_name;
    // lowercase
    for (char& c : val_col) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    int ci_val = col_idx(hdr, val_col);
    if (ci_strike < 0 || ci_dte < 0 || ci_val < 0) {
        err = "Missing required columns: strike, dte, " + val_col; return false;
    }
    int max_ci = std::max({ci_strike, ci_dte, ci_val});

    std::map<int, std::map<float, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            float strike = to_f(row[ci_strike]);
            int   dte    = to_i(row[ci_dte]);
            float val    = to_f(row[ci_val]);
            grid[dte][strike] = val;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<float> strike_keys;
    for (auto& [dte, sm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [k, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), k) == strike_keys.end())
                strike_keys.push_back(k);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.expirations = exp_keys;
    out.strikes     = strike_keys;
    out.underlying  = "IMPORTED";
    out.spot_price  = strike_keys.empty() ? 0.f : strike_keys[strike_keys.size() / 2];
    out.greek_name  = greek_name;
    out.z.assign(exp_keys.size(), std::vector<float>(strike_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < exp_keys.size(); ++ei) {
        int dte = exp_keys[ei];
        for (std::size_t si = 0; si < strike_keys.size(); ++si) {
            auto it = grid[dte].find(strike_keys[si]);
            if (it != grid[dte].end()) out.z[ei][si] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 3. Skew Surface
// ============================================================================
bool load_skew_surface(
    const std::vector<std::vector<std::string>>& rows,
    SkewSurfaceData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_dte   = col_idx(hdr, "dte");
    int ci_delta = col_idx(hdr, "delta");
    int ci_skew  = col_idx(hdr, "skew_pct");
    if (ci_dte < 0 || ci_delta < 0 || ci_skew < 0) {
        err = "Missing required columns: dte, delta, skew_pct"; return false;
    }
    int max_ci = std::max({ci_dte, ci_delta, ci_skew});

    std::map<int, std::map<float, float>> grid; // dte -> delta -> skew
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   dte   = to_i(row[ci_dte]);
            float delta = to_f(row[ci_delta]);
            float skew  = to_f(row[ci_skew]);
            grid[dte][delta] = skew;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<float> delta_keys;
    for (auto& [dte, dm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [d, v] : dm) {
            if (std::find(delta_keys.begin(), delta_keys.end(), d) == delta_keys.end())
                delta_keys.push_back(d);
        }
    }
    std::sort(delta_keys.begin(), delta_keys.end());

    out.expirations = exp_keys;
    out.deltas      = delta_keys;
    out.underlying  = "IMPORTED";
    out.z.assign(exp_keys.size(), std::vector<float>(delta_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < exp_keys.size(); ++ei) {
        int dte = exp_keys[ei];
        for (std::size_t di = 0; di < delta_keys.size(); ++di) {
            auto it = grid[dte].find(delta_keys[di]);
            if (it != grid[dte].end()) out.z[ei][di] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 4. Local Volatility
// ============================================================================
bool load_local_vol(
    const std::vector<std::vector<std::string>>& rows,
    LocalVolData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_strike = col_idx(hdr, "strike");
    int ci_dte    = col_idx(hdr, "dte");
    int ci_lv     = col_idx(hdr, "local_vol_pct");
    if (ci_strike < 0 || ci_dte < 0 || ci_lv < 0) {
        err = "Missing required columns: strike, dte, local_vol_pct"; return false;
    }
    int max_ci = std::max({ci_strike, ci_dte, ci_lv});

    std::map<int, std::map<float, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            float strike = to_f(row[ci_strike]);
            int   dte    = to_i(row[ci_dte]);
            float lv     = to_f(row[ci_lv]);
            grid[dte][strike] = lv;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<float> strike_keys;
    for (auto& [dte, sm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [k, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), k) == strike_keys.end())
                strike_keys.push_back(k);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.expirations = exp_keys;
    out.strikes     = strike_keys;
    out.underlying  = "IMPORTED";
    out.spot_price  = strike_keys.empty() ? 0.f : strike_keys[strike_keys.size() / 2];
    out.z.assign(exp_keys.size(), std::vector<float>(strike_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < exp_keys.size(); ++ei) {
        int dte = exp_keys[ei];
        for (std::size_t si = 0; si < strike_keys.size(); ++si) {
            auto it = grid[dte].find(strike_keys[si]);
            if (it != grid[dte].end()) out.z[ei][si] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 5. Yield Curve
// ============================================================================

} // namespace fincept::surface
