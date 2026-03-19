// SKIP_UNITY_BUILD_INCLUSION
#include "csv_importer.h"
#include "csv_importer_helpers.h"
#include <sstream>
#include <map>
#include <algorithm>
#include <stdexcept>

namespace fincept::surface {

bool load_yield_curve(
    const std::vector<std::vector<std::string>>& rows,
    YieldCurveData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_mat   = col_idx(hdr, "maturity_months");
    int ci_date  = col_idx(hdr, "date");
    int ci_yield = col_idx(hdr, "yield_pct");
    if (ci_mat < 0 || ci_date < 0 || ci_yield < 0) {
        err = "Missing required columns: maturity_months, date, yield_pct"; return false;
    }
    int max_ci = std::max({ci_mat, ci_date, ci_yield});

    // date string -> maturity -> yield
    std::vector<std::string> date_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   mat   = to_i(row[ci_mat]);
            std::string date = row[ci_date];
            float yield = to_f(row[ci_yield]);
            if (grid.find(date) == grid.end()) {
                date_order.push_back(date);
            }
            grid[date][mat] = yield;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    // Collect all maturities
    std::vector<int> mat_keys;
    for (auto& [date, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(mat_keys.begin(), mat_keys.end(), m) == mat_keys.end())
                mat_keys.push_back(m);
        }
    }
    std::sort(mat_keys.begin(), mat_keys.end());

    out.maturities = mat_keys;
    out.z.assign(date_order.size(), std::vector<float>(mat_keys.size(), 0.f));
    for (std::size_t ti = 0; ti < date_order.size(); ++ti) {
        const std::string& date = date_order[ti];
        for (std::size_t mi = 0; mi < mat_keys.size(); ++mi) {
            auto it = grid[date].find(mat_keys[mi]);
            if (it != grid[date].end()) out.z[ti][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 6. Swaption Volatility
// ============================================================================
bool load_swaption_vol(
    const std::vector<std::vector<std::string>>& rows,
    SwaptionVolData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_exp   = col_idx(hdr, "option_expiry_months");
    int ci_tenor = col_idx(hdr, "swap_tenor_months");
    int ci_vol   = col_idx(hdr, "vol_bps");
    if (ci_exp < 0 || ci_tenor < 0 || ci_vol < 0) {
        err = "Missing required columns: option_expiry_months, swap_tenor_months, vol_bps"; return false;
    }
    int max_ci = std::max({ci_exp, ci_tenor, ci_vol});

    std::map<int, std::map<int, float>> grid; // expiry -> tenor -> vol
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   exp   = to_i(row[ci_exp]);
            int   tenor = to_i(row[ci_tenor]);
            float vol   = to_f(row[ci_vol]);
            grid[exp][tenor] = vol;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> exp_keys;
    std::vector<int> tenor_keys;
    for (auto& [e, tm] : grid) {
        exp_keys.push_back(e);
        for (auto& [t, v] : tm) {
            if (std::find(tenor_keys.begin(), tenor_keys.end(), t) == tenor_keys.end())
                tenor_keys.push_back(t);
        }
    }
    std::sort(tenor_keys.begin(), tenor_keys.end());

    out.option_expiries = exp_keys;
    out.swap_tenors     = tenor_keys;
    out.z.assign(exp_keys.size(), std::vector<float>(tenor_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < exp_keys.size(); ++ei) {
        int e = exp_keys[ei];
        for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
            auto it = grid[e].find(tenor_keys[ti]);
            if (it != grid[e].end()) out.z[ei][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 7. Cap/Floor Volatility
// ============================================================================
bool load_capfloor_vol(
    const std::vector<std::vector<std::string>>& rows,
    CapFloorVolData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_mat    = col_idx(hdr, "maturity_months");
    int ci_strike = col_idx(hdr, "strike_pct");
    int ci_vol    = col_idx(hdr, "vol_bps");
    if (ci_mat < 0 || ci_strike < 0 || ci_vol < 0) {
        err = "Missing required columns: maturity_months, strike_pct, vol_bps"; return false;
    }
    int max_ci = std::max({ci_mat, ci_strike, ci_vol});

    std::map<int, std::map<float, float>> grid; // maturity -> strike -> vol
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   mat    = to_i(row[ci_mat]);
            float strike = to_f(row[ci_strike]);
            float vol    = to_f(row[ci_vol]);
            grid[mat][strike] = vol;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> mat_keys;
    std::vector<float> strike_keys;
    for (auto& [m, sm] : grid) {
        mat_keys.push_back(m);
        for (auto& [s, v] : sm) {
            if (std::find(strike_keys.begin(), strike_keys.end(), s) == strike_keys.end())
                strike_keys.push_back(s);
        }
    }
    std::sort(strike_keys.begin(), strike_keys.end());

    out.maturities = mat_keys;
    out.strikes    = strike_keys;
    out.z.assign(mat_keys.size(), std::vector<float>(strike_keys.size(), 0.f));
    for (std::size_t mi = 0; mi < mat_keys.size(); ++mi) {
        int m = mat_keys[mi];
        for (std::size_t si = 0; si < strike_keys.size(); ++si) {
            auto it = grid[m].find(strike_keys[si]);
            if (it != grid[m].end()) out.z[mi][si] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 8. Bond Spread
// ============================================================================
bool load_bond_spread(
    const std::vector<std::vector<std::string>>& rows,
    BondSpreadData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_rating = col_idx(hdr, "rating");
    int ci_mat    = col_idx(hdr, "maturity_months");
    int ci_spread = col_idx(hdr, "spread_bps");
    if (ci_rating < 0 || ci_mat < 0 || ci_spread < 0) {
        err = "Missing required columns: rating, maturity_months, spread_bps"; return false;
    }
    int max_ci = std::max({ci_rating, ci_mat, ci_spread});

    // rating -> maturity -> spread (preserve rating insertion order)
    std::vector<std::string> rating_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string rating = row[ci_rating];
            int   mat    = to_i(row[ci_mat]);
            float spread = to_f(row[ci_spread]);
            if (grid.find(rating) == grid.end()) rating_order.push_back(rating);
            grid[rating][mat] = spread;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> mat_keys;
    for (auto& [rat, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(mat_keys.begin(), mat_keys.end(), m) == mat_keys.end())
                mat_keys.push_back(m);
        }
    }
    std::sort(mat_keys.begin(), mat_keys.end());

    out.ratings    = rating_order;
    out.maturities = mat_keys;
    out.z.assign(rating_order.size(), std::vector<float>(mat_keys.size(), 0.f));
    for (std::size_t ri = 0; ri < rating_order.size(); ++ri) {
        const std::string& rat = rating_order[ri];
        for (std::size_t mi = 0; mi < mat_keys.size(); ++mi) {
            auto it = grid[rat].find(mat_keys[mi]);
            if (it != grid[rat].end()) out.z[ri][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 9. OIS Basis
// ============================================================================
bool load_ois_basis(
    const std::vector<std::vector<std::string>>& rows,
    OISBasisData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_day   = col_idx(hdr, "day");
    int ci_tenor = col_idx(hdr, "tenor_months");
    int ci_basis = col_idx(hdr, "basis_bps");
    if (ci_day < 0 || ci_tenor < 0 || ci_basis < 0) {
        err = "Missing required columns: day, tenor_months, basis_bps"; return false;
    }
    int max_ci = std::max({ci_day, ci_tenor, ci_basis});

    std::map<int, std::map<int, float>> grid; // day -> tenor -> basis
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   day   = to_i(row[ci_day]);
            int   tenor = to_i(row[ci_tenor]);
            float basis = to_f(row[ci_basis]);
            grid[day][tenor] = basis;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> day_keys;
    std::vector<int> tenor_keys;
    for (auto& [d, tm] : grid) {
        day_keys.push_back(d);
        for (auto& [t, v] : tm) {
            if (std::find(tenor_keys.begin(), tenor_keys.end(), t) == tenor_keys.end())
                tenor_keys.push_back(t);
        }
    }
    std::sort(tenor_keys.begin(), tenor_keys.end());

    out.time_points = day_keys;
    out.tenors      = tenor_keys;
    out.z.assign(day_keys.size(), std::vector<float>(tenor_keys.size(), 0.f));
    for (std::size_t di = 0; di < day_keys.size(); ++di) {
        int d = day_keys[di];
        for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
            auto it = grid[d].find(tenor_keys[ti]);
            if (it != grid[d].end()) out.z[di][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 10. Real Yield
// ============================================================================
bool load_real_yield(
    const std::vector<std::vector<std::string>>& rows,
    RealYieldData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_day   = col_idx(hdr, "day");
    int ci_mat   = col_idx(hdr, "maturity_months");
    int ci_yield = col_idx(hdr, "yield_pct");
    if (ci_day < 0 || ci_mat < 0 || ci_yield < 0) {
        err = "Missing required columns: day, maturity_months, yield_pct"; return false;
    }
    int max_ci = std::max({ci_day, ci_mat, ci_yield});

    std::map<int, std::map<int, float>> grid; // day -> maturity -> yield
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   day   = to_i(row[ci_day]);
            int   mat   = to_i(row[ci_mat]);
            float yield = to_f(row[ci_yield]);
            grid[day][mat] = yield;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> day_keys;
    std::vector<int> mat_keys;
    for (auto& [d, mm] : grid) {
        day_keys.push_back(d);
        for (auto& [m, v] : mm) {
            if (std::find(mat_keys.begin(), mat_keys.end(), m) == mat_keys.end())
                mat_keys.push_back(m);
        }
    }
    std::sort(mat_keys.begin(), mat_keys.end());

    out.time_points = day_keys;
    out.maturities  = mat_keys;
    out.z.assign(day_keys.size(), std::vector<float>(mat_keys.size(), 0.f));
    for (std::size_t di = 0; di < day_keys.size(); ++di) {
        int d = day_keys[di];
        for (std::size_t mi = 0; mi < mat_keys.size(); ++mi) {
            auto it = grid[d].find(mat_keys[mi]);
            if (it != grid[d].end()) out.z[di][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 11. Forward Rate
// ============================================================================
bool load_forward_rate(
    const std::vector<std::vector<std::string>>& rows,
    ForwardRateData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_start  = col_idx(hdr, "start_months");
    int ci_period = col_idx(hdr, "fwd_period_months");
    int ci_rate   = col_idx(hdr, "rate_pct");
    if (ci_start < 0 || ci_period < 0 || ci_rate < 0) {
        err = "Missing required columns: start_months, fwd_period_months, rate_pct"; return false;
    }
    int max_ci = std::max({ci_start, ci_period, ci_rate});

    std::map<int, std::map<int, float>> grid; // start -> period -> rate
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   start  = to_i(row[ci_start]);
            int   period = to_i(row[ci_period]);
            float rate   = to_f(row[ci_rate]);
            grid[start][period] = rate;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> start_keys;
    std::vector<int> period_keys;
    for (auto& [s, pm] : grid) {
        start_keys.push_back(s);
        for (auto& [p, v] : pm) {
            if (std::find(period_keys.begin(), period_keys.end(), p) == period_keys.end())
                period_keys.push_back(p);
        }
    }
    std::sort(period_keys.begin(), period_keys.end());

    out.start_tenors    = start_keys;
    out.forward_periods = period_keys;
    out.z.assign(start_keys.size(), std::vector<float>(period_keys.size(), 0.f));
    for (std::size_t si = 0; si < start_keys.size(); ++si) {
        int s = start_keys[si];
        for (std::size_t pi = 0; pi < period_keys.size(); ++pi) {
            auto it = grid[s].find(period_keys[pi]);
            if (it != grid[s].end()) out.z[si][pi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 12. FX Volatility
// ============================================================================
bool load_fx_vol(
    const std::vector<std::vector<std::string>>& rows,
    FXVolData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_tenor = col_idx(hdr, "tenor_days");
    int ci_delta = col_idx(hdr, "delta");
    int ci_vol   = col_idx(hdr, "vol_pct");
    if (ci_tenor < 0 || ci_delta < 0 || ci_vol < 0) {
        err = "Missing required columns: tenor_days, delta, vol_pct"; return false;
    }
    int max_ci = std::max({ci_tenor, ci_delta, ci_vol});

    std::map<int, std::map<float, float>> grid; // tenor -> delta -> vol
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   tenor = to_i(row[ci_tenor]);
            float delta = to_f(row[ci_delta]);
            float vol   = to_f(row[ci_vol]);
            grid[tenor][delta] = vol;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> tenor_keys;
    std::vector<float> delta_keys;
    for (auto& [t, dm] : grid) {
        tenor_keys.push_back(t);
        for (auto& [d, v] : dm) {
            if (std::find(delta_keys.begin(), delta_keys.end(), d) == delta_keys.end())
                delta_keys.push_back(d);
        }
    }
    std::sort(delta_keys.begin(), delta_keys.end());

    out.tenors = tenor_keys;
    out.deltas = delta_keys;
    out.pair   = "IMPORTED";
    out.z.assign(tenor_keys.size(), std::vector<float>(delta_keys.size(), 0.f));
    for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
        int t = tenor_keys[ti];
        for (std::size_t di = 0; di < delta_keys.size(); ++di) {
            auto it = grid[t].find(delta_keys[di]);
            if (it != grid[t].end()) out.z[ti][di] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 13. FX Forward Points
// ============================================================================
bool load_fx_forward(
    const std::vector<std::vector<std::string>>& rows,
    FXForwardPointsData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_pair  = col_idx(hdr, "pair");
    int ci_tenor = col_idx(hdr, "tenor_days");
    int ci_pips  = col_idx(hdr, "pips");
    if (ci_pair < 0 || ci_tenor < 0 || ci_pips < 0) {
        err = "Missing required columns: pair, tenor_days, pips"; return false;
    }
    int max_ci = std::max({ci_pair, ci_tenor, ci_pips});

    std::vector<std::string> pair_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string pair = row[ci_pair];
            int   tenor = to_i(row[ci_tenor]);
            float pips  = to_f(row[ci_pips]);
            if (grid.find(pair) == grid.end()) pair_order.push_back(pair);
            grid[pair][tenor] = pips;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> tenor_keys;
    for (auto& [p, tm] : grid) {
        for (auto& [t, v] : tm) {
            if (std::find(tenor_keys.begin(), tenor_keys.end(), t) == tenor_keys.end())
                tenor_keys.push_back(t);
        }
    }
    std::sort(tenor_keys.begin(), tenor_keys.end());

    out.pairs  = pair_order;
    out.tenors = tenor_keys;
    out.z.assign(pair_order.size(), std::vector<float>(tenor_keys.size(), 0.f));
    for (std::size_t pi = 0; pi < pair_order.size(); ++pi) {
        const std::string& p = pair_order[pi];
        for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
            auto it = grid[p].find(tenor_keys[ti]);
            if (it != grid[p].end()) out.z[pi][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 14. Cross-Currency Basis
// ============================================================================
bool load_xccy_basis(
    const std::vector<std::vector<std::string>>& rows,
    CrossCurrencyBasisData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_pair  = col_idx(hdr, "pair");
    int ci_tenor = col_idx(hdr, "tenor_months");
    int ci_basis = col_idx(hdr, "basis_bps");
    if (ci_pair < 0 || ci_tenor < 0 || ci_basis < 0) {
        err = "Missing required columns: pair, tenor_months, basis_bps"; return false;
    }
    int max_ci = std::max({ci_pair, ci_tenor, ci_basis});

    std::vector<std::string> pair_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string pair  = row[ci_pair];
            int   tenor = to_i(row[ci_tenor]);
            float basis = to_f(row[ci_basis]);
            if (grid.find(pair) == grid.end()) pair_order.push_back(pair);
            grid[pair][tenor] = basis;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> tenor_keys;
    for (auto& [p, tm] : grid) {
        for (auto& [t, v] : tm) {
            if (std::find(tenor_keys.begin(), tenor_keys.end(), t) == tenor_keys.end())
                tenor_keys.push_back(t);
        }
    }
    std::sort(tenor_keys.begin(), tenor_keys.end());

    out.pairs  = pair_order;
    out.tenors = tenor_keys;
    out.z.assign(pair_order.size(), std::vector<float>(tenor_keys.size(), 0.f));
    for (std::size_t pi = 0; pi < pair_order.size(); ++pi) {
        const std::string& p = pair_order[pi];
        for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
            auto it = grid[p].find(tenor_keys[ti]);
            if (it != grid[p].end()) out.z[pi][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 15. CDS Spread
// ============================================================================
bool load_cds_spread(
    const std::vector<std::vector<std::string>>& rows,
    CDSSpreadData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_entity = col_idx(hdr, "entity");
    int ci_tenor  = col_idx(hdr, "tenor_months");
    int ci_spread = col_idx(hdr, "spread_bps");
    if (ci_entity < 0 || ci_tenor < 0 || ci_spread < 0) {
        err = "Missing required columns: entity, tenor_months, spread_bps"; return false;
    }
    int max_ci = std::max({ci_entity, ci_tenor, ci_spread});

    std::vector<std::string> entity_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string entity = row[ci_entity];
            int   tenor  = to_i(row[ci_tenor]);
            float spread = to_f(row[ci_spread]);
            if (grid.find(entity) == grid.end()) entity_order.push_back(entity);
            grid[entity][tenor] = spread;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> tenor_keys;
    for (auto& [e, tm] : grid) {
        for (auto& [t, v] : tm) {
            if (std::find(tenor_keys.begin(), tenor_keys.end(), t) == tenor_keys.end())
                tenor_keys.push_back(t);
        }
    }
    std::sort(tenor_keys.begin(), tenor_keys.end());

    out.entities = entity_order;
    out.tenors   = tenor_keys;
    out.z.assign(entity_order.size(), std::vector<float>(tenor_keys.size(), 0.f));
    for (std::size_t ei = 0; ei < entity_order.size(); ++ei) {
        const std::string& e = entity_order[ei];
        for (std::size_t ti = 0; ti < tenor_keys.size(); ++ti) {
            auto it = grid[e].find(tenor_keys[ti]);
            if (it != grid[e].end()) out.z[ei][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 16. Credit Transition Matrix
// ============================================================================
bool load_credit_trans(
    const std::vector<std::vector<std::string>>& rows,
    CreditTransitionData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_from = col_idx(hdr, "from_rating");
    int ci_to   = col_idx(hdr, "to_rating");
    int ci_prob = col_idx(hdr, "probability_pct");
    if (ci_from < 0 || ci_to < 0 || ci_prob < 0) {
        err = "Missing required columns: from_rating, to_rating, probability_pct"; return false;
    }
    int max_ci = std::max({ci_from, ci_to, ci_prob});

    std::vector<std::string> from_order;
    std::vector<std::string> to_order;
    std::map<std::string, std::map<std::string, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string from = row[ci_from];
            std::string to   = row[ci_to];
            float prob = to_f(row[ci_prob]);
            if (grid.find(from) == grid.end()) from_order.push_back(from);
            if (std::find(to_order.begin(), to_order.end(), to) == to_order.end())
                to_order.push_back(to);
            grid[from][to] = prob;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    out.ratings    = from_order;
    out.to_ratings = to_order;
    out.z.assign(from_order.size(), std::vector<float>(to_order.size(), 0.f));
    for (std::size_t fi = 0; fi < from_order.size(); ++fi) {
        const std::string& from = from_order[fi];
        for (std::size_t ti = 0; ti < to_order.size(); ++ti) {
            auto it = grid[from].find(to_order[ti]);
            if (it != grid[from].end()) out.z[fi][ti] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 17. Recovery Rate
// ============================================================================

} // namespace fincept::surface
