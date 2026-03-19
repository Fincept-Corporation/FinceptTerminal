// SKIP_UNITY_BUILD_INCLUSION
#include "csv_importer.h"
#include "csv_importer_helpers.h"
#include <sstream>
#include <map>
#include <algorithm>
#include <stdexcept>

namespace fincept::surface {

bool load_recovery_rate(
    const std::vector<std::vector<std::string>>& rows,
    RecoveryRateData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_sector   = col_idx(hdr, "sector");
    int ci_seniority= col_idx(hdr, "seniority");
    int ci_recovery = col_idx(hdr, "recovery_pct");
    if (ci_sector < 0 || ci_seniority < 0 || ci_recovery < 0) {
        err = "Missing required columns: sector, seniority, recovery_pct"; return false;
    }
    int max_ci = std::max({ci_sector, ci_seniority, ci_recovery});

    std::vector<std::string> sector_order;
    std::vector<std::string> seniority_order;
    std::map<std::string, std::map<std::string, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string sector   = row[ci_sector];
            std::string seniority= row[ci_seniority];
            float recovery = to_f(row[ci_recovery]);
            if (grid.find(sector) == grid.end()) sector_order.push_back(sector);
            if (std::find(seniority_order.begin(), seniority_order.end(), seniority) == seniority_order.end())
                seniority_order.push_back(seniority);
            grid[sector][seniority] = recovery;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    out.sectors    = sector_order;
    out.seniorities= seniority_order;
    out.z.assign(sector_order.size(), std::vector<float>(seniority_order.size(), 0.f));
    for (std::size_t si = 0; si < sector_order.size(); ++si) {
        const std::string& sec = sector_order[si];
        for (std::size_t ni = 0; ni < seniority_order.size(); ++ni) {
            auto it = grid[sec].find(seniority_order[ni]);
            if (it != grid[sec].end()) out.z[si][ni] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 18. Commodity Forward
// ============================================================================
bool load_cmdty_forward(
    const std::vector<std::vector<std::string>>& rows,
    CommodityForwardData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_cmdty   = col_idx(hdr, "commodity");
    int ci_months  = col_idx(hdr, "contract_months");
    int ci_price   = col_idx(hdr, "price");
    if (ci_cmdty < 0 || ci_months < 0 || ci_price < 0) {
        err = "Missing required columns: commodity, contract_months, price"; return false;
    }
    int max_ci = std::max({ci_cmdty, ci_months, ci_price});

    std::vector<std::string> cmdty_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string cmdty = row[ci_cmdty];
            int   months = to_i(row[ci_months]);
            float price  = to_f(row[ci_price]);
            if (grid.find(cmdty) == grid.end()) cmdty_order.push_back(cmdty);
            grid[cmdty][months] = price;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> month_keys;
    for (auto& [c, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(month_keys.begin(), month_keys.end(), m) == month_keys.end())
                month_keys.push_back(m);
        }
    }
    std::sort(month_keys.begin(), month_keys.end());

    out.commodities     = cmdty_order;
    out.contract_months = month_keys;
    out.z.assign(cmdty_order.size(), std::vector<float>(month_keys.size(), 0.f));
    for (std::size_t ci2 = 0; ci2 < cmdty_order.size(); ++ci2) {
        const std::string& c = cmdty_order[ci2];
        for (std::size_t mi = 0; mi < month_keys.size(); ++mi) {
            auto it = grid[c].find(month_keys[mi]);
            if (it != grid[c].end()) out.z[ci2][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 19. Commodity Volatility
// ============================================================================
bool load_cmdty_vol(
    const std::vector<std::vector<std::string>>& rows,
    CommodityVolData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_dte    = col_idx(hdr, "dte");
    int ci_strike = col_idx(hdr, "strike_pct");
    int ci_vol    = col_idx(hdr, "vol_pct");
    if (ci_dte < 0 || ci_strike < 0 || ci_vol < 0) {
        err = "Missing required columns: dte, strike_pct, vol_pct"; return false;
    }
    int max_ci = std::max({ci_dte, ci_strike, ci_vol});

    std::map<int, std::map<float, float>> grid; // dte -> strike -> vol
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            int   dte    = to_i(row[ci_dte]);
            float strike = to_f(row[ci_strike]);
            float vol    = to_f(row[ci_vol]);
            grid[dte][strike] = vol;
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
    out.commodity   = "IMPORTED";
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
// 20. Crack/Crush Spread
// ============================================================================
bool load_crack_spread(
    const std::vector<std::vector<std::string>>& rows,
    CrackSpreadData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_type   = col_idx(hdr, "spread_type");
    int ci_months = col_idx(hdr, "contract_months");
    int ci_spread = col_idx(hdr, "spread_usd");
    if (ci_type < 0 || ci_months < 0 || ci_spread < 0) {
        err = "Missing required columns: spread_type, contract_months, spread_usd"; return false;
    }
    int max_ci = std::max({ci_type, ci_months, ci_spread});

    std::vector<std::string> type_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string type   = row[ci_type];
            int   months = to_i(row[ci_months]);
            float spread = to_f(row[ci_spread]);
            if (grid.find(type) == grid.end()) type_order.push_back(type);
            grid[type][months] = spread;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> month_keys;
    for (auto& [t, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(month_keys.begin(), month_keys.end(), m) == month_keys.end())
                month_keys.push_back(m);
        }
    }
    std::sort(month_keys.begin(), month_keys.end());

    out.spread_types    = type_order;
    out.contract_months = month_keys;
    out.z.assign(type_order.size(), std::vector<float>(month_keys.size(), 0.f));
    for (std::size_t ti = 0; ti < type_order.size(); ++ti) {
        const std::string& t = type_order[ti];
        for (std::size_t mi = 0; mi < month_keys.size(); ++mi) {
            auto it = grid[t].find(month_keys[mi]);
            if (it != grid[t].end()) out.z[ti][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 21. Contango/Backwardation
// ============================================================================
bool load_contango(
    const std::vector<std::vector<std::string>>& rows,
    ContangoData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_cmdty  = col_idx(hdr, "commodity");
    int ci_months = col_idx(hdr, "contract_months");
    int ci_pct    = col_idx(hdr, "pct_from_spot");
    if (ci_cmdty < 0 || ci_months < 0 || ci_pct < 0) {
        err = "Missing required columns: commodity, contract_months, pct_from_spot"; return false;
    }
    int max_ci = std::max({ci_cmdty, ci_months, ci_pct});

    std::vector<std::string> cmdty_order;
    std::map<std::string, std::map<int, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string cmdty = row[ci_cmdty];
            int   months = to_i(row[ci_months]);
            float pct    = to_f(row[ci_pct]);
            if (grid.find(cmdty) == grid.end()) cmdty_order.push_back(cmdty);
            grid[cmdty][months] = pct;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<int> month_keys;
    for (auto& [c, mm] : grid) {
        for (auto& [m, v] : mm) {
            if (std::find(month_keys.begin(), month_keys.end(), m) == month_keys.end())
                month_keys.push_back(m);
        }
    }
    std::sort(month_keys.begin(), month_keys.end());

    out.commodities     = cmdty_order;
    out.contract_months = month_keys;
    out.z.assign(cmdty_order.size(), std::vector<float>(month_keys.size(), 0.f));
    for (std::size_t ci2 = 0; ci2 < cmdty_order.size(); ++ci2) {
        const std::string& c = cmdty_order[ci2];
        for (std::size_t mi = 0; mi < month_keys.size(); ++mi) {
            auto it = grid[c].find(month_keys[mi]);
            if (it != grid[c].end()) out.z[ci2][mi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 22. Correlation Matrix
// ============================================================================
bool load_correlation(
    const std::vector<std::vector<std::string>>& rows,
    CorrelationMatrixData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_i    = col_idx(hdr, "asset_i");
    int ci_j    = col_idx(hdr, "asset_j");
    int ci_corr = col_idx(hdr, "correlation");
    if (ci_i < 0 || ci_j < 0 || ci_corr < 0) {
        err = "Missing required columns: asset_i, asset_j, correlation"; return false;
    }
    int max_ci = std::max({ci_i, ci_j, ci_corr});

    std::vector<std::string> asset_order;
    std::map<std::string, std::map<std::string, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string ai = row[ci_i];
            std::string aj = row[ci_j];
            float corr = to_f(row[ci_corr]);
            if (std::find(asset_order.begin(), asset_order.end(), ai) == asset_order.end())
                asset_order.push_back(ai);
            if (std::find(asset_order.begin(), asset_order.end(), aj) == asset_order.end())
                asset_order.push_back(aj);
            grid[ai][aj] = corr;
            grid[aj][ai] = corr;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    int n = static_cast<int>(asset_order.size());
    // Build flat n*n matrix in single time row
    std::vector<float> flat(static_cast<std::size_t>(n * n), 0.f);
    for (int ii = 0; ii < n; ++ii) {
        for (int jj = 0; jj < n; ++jj) {
            if (ii == jj) {
                flat[static_cast<std::size_t>(ii * n + jj)] = 1.f;
            } else {
                auto ait = grid.find(asset_order[static_cast<std::size_t>(ii)]);
                if (ait != grid.end()) {
                    auto ajt = ait->second.find(asset_order[static_cast<std::size_t>(jj)]);
                    if (ajt != ait->second.end())
                        flat[static_cast<std::size_t>(ii * n + jj)] = ajt->second;
                }
            }
        }
    }

    out.assets = asset_order;
    out.window = 0;
    out.z = {flat};
    return true;
}

// ============================================================================
// 23. PCA
// ============================================================================
bool load_pca(
    const std::vector<std::vector<std::string>>& rows,
    PCAData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_asset   = col_idx(hdr, "asset");
    int ci_factor  = col_idx(hdr, "factor");
    int ci_loading = col_idx(hdr, "loading");
    if (ci_asset < 0 || ci_factor < 0 || ci_loading < 0) {
        err = "Missing required columns: asset, factor, loading"; return false;
    }
    int max_ci = std::max({ci_asset, ci_factor, ci_loading});

    std::vector<std::string> asset_order;
    std::vector<std::string> factor_order;
    std::map<std::string, std::map<std::string, float>> grid; // asset -> factor -> loading
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string asset   = row[ci_asset];
            std::string factor  = row[ci_factor];
            float loading = to_f(row[ci_loading]);
            if (std::find(asset_order.begin(), asset_order.end(), asset) == asset_order.end())
                asset_order.push_back(asset);
            if (std::find(factor_order.begin(), factor_order.end(), factor) == factor_order.end())
                factor_order.push_back(factor);
            grid[asset][factor] = loading;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    int n_factors = static_cast<int>(factor_order.size());
    float uniform_var = n_factors > 0 ? 1.f / static_cast<float>(n_factors) : 0.f;

    out.assets   = asset_order;
    out.factors  = factor_order;
    out.variance_explained.assign(static_cast<std::size_t>(n_factors), uniform_var);
    out.z.assign(asset_order.size(), std::vector<float>(static_cast<std::size_t>(n_factors), 0.f));
    for (std::size_t ai = 0; ai < asset_order.size(); ++ai) {
        const std::string& asset = asset_order[ai];
        for (std::size_t fi = 0; fi < factor_order.size(); ++fi) {
            auto ait = grid.find(asset);
            if (ait != grid.end()) {
                auto fit = ait->second.find(factor_order[fi]);
                if (fit != ait->second.end()) out.z[ai][fi] = fit->second;
            }
        }
    }
    return true;
}

// ============================================================================
// 24. VaR
// ============================================================================
bool load_var(
    const std::vector<std::vector<std::string>>& rows,
    VaRData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_conf    = col_idx(hdr, "confidence_pct");
    int ci_horizon = col_idx(hdr, "horizon_days");
    int ci_var     = col_idx(hdr, "var_pct");
    if (ci_conf < 0 || ci_horizon < 0 || ci_var < 0) {
        err = "Missing required columns: confidence_pct, horizon_days, var_pct"; return false;
    }
    int max_ci = std::max({ci_conf, ci_horizon, ci_var});

    std::map<float, std::map<int, float>> grid; // confidence -> horizon -> var
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            float conf    = to_f(row[ci_conf]);
            int   horizon = to_i(row[ci_horizon]);
            float var_val = to_f(row[ci_var]);
            grid[conf][horizon] = var_val;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    std::vector<float> conf_keys;
    std::vector<int> horizon_keys;
    for (auto& [c, hm] : grid) {
        conf_keys.push_back(c);
        for (auto& [h, v] : hm) {
            if (std::find(horizon_keys.begin(), horizon_keys.end(), h) == horizon_keys.end())
                horizon_keys.push_back(h);
        }
    }
    std::sort(horizon_keys.begin(), horizon_keys.end());

    out.confidence_levels = conf_keys;
    out.horizons          = horizon_keys;
    out.z.assign(conf_keys.size(), std::vector<float>(horizon_keys.size(), 0.f));
    for (std::size_t ci2 = 0; ci2 < conf_keys.size(); ++ci2) {
        float c = conf_keys[ci2];
        for (std::size_t hi = 0; hi < horizon_keys.size(); ++hi) {
            auto it = grid[c].find(horizon_keys[hi]);
            if (it != grid[c].end()) out.z[ci2][hi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 25. Stress Test P&L
// ============================================================================
bool load_stress_test(
    const std::vector<std::vector<std::string>>& rows,
    StressTestData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_scenario  = col_idx(hdr, "scenario");
    int ci_portfolio = col_idx(hdr, "portfolio");
    int ci_pnl       = col_idx(hdr, "pnl_pct");
    if (ci_scenario < 0 || ci_portfolio < 0 || ci_pnl < 0) {
        err = "Missing required columns: scenario, portfolio, pnl_pct"; return false;
    }
    int max_ci = std::max({ci_scenario, ci_portfolio, ci_pnl});

    std::vector<std::string> scenario_order;
    std::vector<std::string> portfolio_order;
    std::map<std::string, std::map<std::string, float>> grid;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string scenario  = row[ci_scenario];
            std::string portfolio = row[ci_portfolio];
            float pnl = to_f(row[ci_pnl]);
            if (std::find(scenario_order.begin(), scenario_order.end(), scenario) == scenario_order.end())
                scenario_order.push_back(scenario);
            if (std::find(portfolio_order.begin(), portfolio_order.end(), portfolio) == portfolio_order.end())
                portfolio_order.push_back(portfolio);
            grid[scenario][portfolio] = pnl;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    out.scenarios  = scenario_order;
    out.portfolios = portfolio_order;
    out.z.assign(scenario_order.size(), std::vector<float>(portfolio_order.size(), 0.f));
    for (std::size_t si = 0; si < scenario_order.size(); ++si) {
        const std::string& sc = scenario_order[si];
        for (std::size_t pi = 0; pi < portfolio_order.size(); ++pi) {
            auto it = grid[sc].find(portfolio_order[pi]);
            if (it != grid[sc].end()) out.z[si][pi] = it->second;
        }
    }
    return true;
}

// ============================================================================
// 26. Factor Exposure
// ============================================================================
bool load_factor_exposure(
    const std::vector<std::vector<std::string>>& rows,
    FactorExposureData& out,
    std::string& err)
{
    if (rows.empty()) { err = "No data rows"; return false; }
    const auto& hdr = rows[0];
    int ci_asset    = col_idx(hdr, "asset");
    int ci_factor   = col_idx(hdr, "factor");
    int ci_exposure = col_idx(hdr, "exposure");
    if (ci_asset < 0 || ci_factor < 0 || ci_exposure < 0) {
        err = "Missing required columns: asset, factor, exposure"; return false;
    }
    int max_ci = std::max({ci_asset, ci_factor, ci_exposure});

    std::vector<std::string> asset_order;
    std::vector<std::string> factor_order;
    std::map<std::string, std::map<std::string, float>> grid; // asset -> factor -> exposure
    for (std::size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (static_cast<int>(row.size()) <= max_ci) continue;
        try {
            std::string asset   = row[ci_asset];
            std::string factor  = row[ci_factor];
            float exposure = to_f(row[ci_exposure]);
            if (std::find(asset_order.begin(), asset_order.end(), asset) == asset_order.end())
                asset_order.push_back(asset);
            if (std::find(factor_order.begin(), factor_order.end(), factor) == factor_order.end())
                factor_order.push_back(factor);
            grid[asset][factor] = exposure;
        } catch (...) { continue; }
    }
    if (grid.empty()) { err = "No valid data rows"; return false; }

    out.assets  = asset_order;
    out.factors = factor_order;
    out.z.assign(asset_order.size(), std::vector<float>(factor_order.size(), 0.f));
    for (std::size_t ai = 0; ai < asset_order.size(); ++ai) {
        const std::string& asset = asset_order[ai];
        for (std::size_t fi = 0; fi < factor_order.size(); ++fi) {
            auto ait = grid.find(asset);
            if (ait != grid.end()) {
                auto fit = ait->second.find(factor_order[fi]);
                if (fit != ait->second.end()) out.z[ai][fi] = fit->second;
            }
        }
    }
    return true;
}

// ============================================================================
// 27. Liquidity Heatmap
// ============================================================================

} // namespace fincept::surface
