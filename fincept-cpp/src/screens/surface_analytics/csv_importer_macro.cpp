// SKIP_UNITY_BUILD_INCLUSION
#include "csv_importer.h"
#include "csv_importer_helpers.h"
#include <sstream>
#include <map>
#include <algorithm>
#include <stdexcept>

namespace fincept::surface {

bool load_liquidity(
    const std::vector<std::vector<std::string>>& rows,
    LiquidityData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_dte    = col_idx(hdr, "dte");
    int ci_strike = col_idx(hdr, "strike");
    int ci_spread = col_idx(hdr, "spread");
    if (ci_dte < 0 || ci_strike < 0 || ci_spread < 0) {
        err = "Missing required columns: dte, strike, spread"; return false;
    }
    int max_ci = std::max({ci_dte, ci_strike, ci_spread});

    std::map<int, std::map<float, float>> grid; // dte -> strike -> spread
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   dte    = to_i(row[ci_dte]);
            float strike = to_f(row[ci_strike]);
            float spread = to_f(row[ci_spread]);
            grid[dte][strike] = spread;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<float> strike_keys;
    for (auto& [dte, sm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [s, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), s) == strike_keys.end())
                strike_keys.push_back(s);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.expirations = exp_keys;
    out.strikes     = strike_keys;
    out.underlying  = "IMPORTED";
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
// 28. Drawdown
// ============================================================================
bool load_drawdown(
    const std::vector<std::vector<std::string>>& rows,
    DrawdownData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_asset  = col_idx(hdr, "asset");
    int ci_window = col_idx(hdr, "window_days");
    int ci_dd     = col_idx(hdr, "max_dd_pct");
    if (ci_asset < 0 || ci_window < 0 || ci_dd < 0) {
        err = "Missing required columns: asset, window_days, max_dd_pct"; return false;
    }
    int max_ci = std::max({ci_asset, ci_window, ci_dd});

    std::vector<std::string> asset_order;
    std::map<std::string, std::map<int, float>> grid; // asset -> window -> dd
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string asset  = row[ci_asset];
            int   window = to_i(row[ci_window]);
            float dd     = to_f(row[ci_dd]);
            if (grid.find(asset) == grid.end()) asset_order.push_back(asset);
            grid[asset][window] = dd;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> window_keys;
    for (auto& [a, wm] : grid) {
        for (auto& [w, v] : wm) {
            if (std::find(window_keys.begin(), window_keys.end(), w) == window_keys.end())
                window_keys.push_back(w);
        }
    }
    std::sort(window_keys.begin(), window_keys.end());

    out.assets  = asset_order;
    out.windows = window_keys;
    out.z.assign(asset_order.size(), std::vector<float>(window_keys.size(), 0.f));
    for (std::size_t ai = 0; ai < asset_order.size(); ++ai) {
        const std::string& a = asset_order[ai];
        for (std::size_t wi = 0; wi < window_keys.size(); ++wi) {
            auto it = grid[a].find(window_keys[wi]);
            if (it != grid[a].end()) out.z[ai][wi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 29. Beta Surface
// ============================================================================
bool load_beta(
    const std::vector<std::vector<std::string>>& rows,
    BetaData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_asset   = col_idx(hdr, "asset");
    int ci_horizon = col_idx(hdr, "horizon_days");
    int ci_beta    = col_idx(hdr, "beta");
    if (ci_asset < 0 || ci_horizon < 0 || ci_beta < 0) {
        err = "Missing required columns: asset, horizon_days, beta"; return false;
    }
    int max_ci = std::max({ci_asset, ci_horizon, ci_beta});

    std::vector<std::string> asset_order;
    std::map<std::string, std::map<int, float>> grid; // asset -> horizon -> beta
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string asset   = row[ci_asset];
            int   horizon = to_i(row[ci_horizon]);
            float beta    = to_f(row[ci_beta]);
            if (grid.find(asset) == grid.end()) asset_order.push_back(asset);
            grid[asset][horizon] = beta;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> horizon_keys;
    for (auto& [a, hm] : grid) {
        for (auto& [h, v] : hm) {
            if (std::find(horizon_keys.begin(), horizon_keys.end(), h) == horizon_keys.end())
                horizon_keys.push_back(h);
        }
    }
    std::sort(horizon_keys.begin(), horizon_keys.end());

    out.assets   = asset_order;
    out.horizons = horizon_keys;
    out.z.assign(asset_order.size(), std::vector<float>(horizon_keys.size(), 0.f));
    for (std::size_t ai = 0; ai < asset_order.size(); ++ai) {
        const std::string& a = asset_order[ai];
        for (std::size_t hi = 0; hi < horizon_keys.size(); ++hi) {
            auto it = grid[a].find(horizon_keys[hi]);
            if (it != grid[a].end()) out.z[ai][hi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 30. Implied Dividend
// ============================================================================
bool load_implied_div(
    const std::vector<std::vector<std::string>>& rows,
    ImpliedDividendData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_dte    = col_idx(hdr, "dte");
    int ci_strike = col_idx(hdr, "strike");
    int ci_div    = col_idx(hdr, "div_yield_pct");
    if (ci_dte < 0 || ci_strike < 0 || ci_div < 0) {
        err = "Missing required columns: dte, strike, div_yield_pct"; return false;
    }
    int max_ci = std::max({ci_dte, ci_strike, ci_div});

    std::map<int, std::map<float, float>> grid; // dte -> strike -> div
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   dte    = to_i(row[ci_dte]);
            float strike = to_f(row[ci_strike]);
            float div    = to_f(row[ci_div]);
            grid[dte][strike] = div;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<float> strike_keys;
    for (auto& [dte, sm] : grid) {
        exp_keys.push_back(dte);
        for (auto& [s, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), s) == strike_keys.end())
                strike_keys.push_back(s);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.expirations = exp_keys;
    out.strikes     = strike_keys;
    out.underlying  = "IMPORTED";
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
// 31. Inflation Expectations
// ============================================================================
bool load_inflation(
    const std::vector<std::vector<std::string>>& rows,
    InflationExpData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_day     = col_idx(hdr, "day");
    int ci_horizon = col_idx(hdr, "horizon_years");
    int ci_bei     = col_idx(hdr, "bei_pct");
    if (ci_day < 0 || ci_horizon < 0 || ci_bei < 0) {
        err = "Missing required columns: day, horizon_years, bei_pct"; return false;
    }
    int max_ci = std::max({ci_day, ci_horizon, ci_bei});

    std::map<int, std::map<int, float>> grid; // day -> horizon -> bei
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   day     = to_i(row[ci_day]);
            int   horizon = to_i(row[ci_horizon]);
            float bei     = to_f(row[ci_bei]);
            grid[day][horizon] = bei;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> day_keys;
    std::vector<int> horizon_keys;
    for (auto& [d, hm] : grid) {
        day_keys.push_back(d);
        for (auto& [h, v] : hm) {
            if (std::find(horizon_keys.begin(), horizon_keys.end(), h) == horizon_keys.end())
                horizon_keys.push_back(h);
        }
    }
    std::sort(horizon_keys.begin(), horizon_keys.end());

    out.time_points = day_keys;
    out.horizons    = horizon_keys;
    out.z.assign(day_keys.size(), std::vector<float>(horizon_keys.size(), 0.f));
    for (std::size_t di = 0; di < day_keys.size(); ++di) {
        int d = day_keys[di];
        for (std::size_t hi = 0; hi < horizon_keys.size(); ++hi) {
            auto it = grid[d].find(horizon_keys[hi]);
            if (it != grid[d].end()) out.z[di][hi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 32. Monetary Policy Path
// ============================================================================
bool load_monetary(
    const std::vector<std::vector<std::string>>& rows,
    MonetaryPolicyData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_bank    = col_idx(hdr, "central_bank");
    int ci_meeting = col_idx(hdr, "meeting_num");
    int ci_rate    = col_idx(hdr, "rate_pct");
    if (ci_bank < 0 || ci_meeting < 0 || ci_rate < 0) {
        err = "Missing required columns: central_bank, meeting_num, rate_pct"; return false;
    }
    int max_ci = std::max({ci_bank, ci_meeting, ci_rate});

    std::vector<std::string> bank_order;
    std::map<std::string, std::map<int, float>> grid; // bank -> meeting -> rate
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string bank    = row[ci_bank];
            int   meeting = to_i(row[ci_meeting]);
            float rate    = to_f(row[ci_rate]);
            if (grid.find(bank) == grid.end()) bank_order.push_back(bank);
            grid[bank][meeting] = rate;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> meeting_keys;
    for (auto& [b, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(meeting_keys.begin(), meeting_keys.end(), m) == meeting_keys.end())
                meeting_keys.push_back(m);
        }
    }
    std::sort(meeting_keys.begin(), meeting_keys.end());

    out.central_banks   = bank_order;
    out.meetings_ahead  = meeting_keys;
    out.z.assign(bank_order.size(), std::vector<float>(meeting_keys.size(), 0.f));
    for (std::size_t bi = 0; bi < bank_order.size(); ++bi) {
        const std::string& b = bank_order[bi];
        for (std::size_t mi = 0; mi < meeting_keys.size(); ++mi) {
            auto it = grid[b].find(meeting_keys[mi]);
            if (it != grid[b].end()) out.z[bi][mi] = it->second;
        }
    }
    return true;
}


} // namespace fincept::surface
