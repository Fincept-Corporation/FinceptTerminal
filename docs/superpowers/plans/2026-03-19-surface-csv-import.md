# Surface Analytics CSV Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-surface CSV import to all 35 Surface Analytics surfaces, with an in-UI column schema reference shown before import, so users can load real data and analyze it instead of demo data.

**Architecture:** A single shared CSV parser (`csv_importer.cpp`) reads rows into `vector<vector<string>>`, then per-surface parser functions convert that into the existing typed data structs (`VolatilitySurfaceData`, `BondSpreadData`, etc.). A reusable `render_csv_import_dialog()` modal shows the required column schema, a path input, parse errors, and a load button. The `SurfaceScreen` adds an IMPORT button to the control bar that opens this modal for the active surface.

**Tech Stack:** C++20, Dear ImGui (`BeginPopupModal`), standard `<fstream>` + `<sstream>` for CSV parsing, existing theme helpers (`theme::AccentButton`, `theme::SecondaryButton`), existing data structs in `surface_types.h`.

---

## CSV Column Schemas (locked format for all 35 surfaces)

These are the canonical column definitions. They must be shown in the UI at import time.

### Equity Derivatives
| Surface | Required Columns |
|---|---|
| VOL SURFACE | `strike, dte, iv_pct` |
| DELTA | `strike, dte, delta` |
| GAMMA | `strike, dte, gamma` |
| VEGA | `strike, dte, vega` |
| THETA | `strike, dte, theta` |
| SKEW | `dte, delta, skew_pct` |
| LOCAL VOL | `strike, dte, local_vol_pct` |

### Fixed Income
| Surface | Required Columns |
|---|---|
| YIELD CURVE | `maturity_months, date, yield_pct` |
| SWAPTION VOL | `option_expiry_months, swap_tenor_months, vol_bps` |
| CAP/FLOOR | `maturity_months, strike_pct, vol_bps` |
| BOND SPREAD | `rating, maturity_months, spread_bps` |
| OIS BASIS | `day, tenor_months, basis_bps` |
| REAL YIELD | `day, maturity_months, yield_pct` |
| FWD RATE | `start_months, fwd_period_months, rate_pct` |

### FX
| Surface | Required Columns |
|---|---|
| FX VOL | `tenor_days, delta, vol_pct` |
| FX FORWARD | `pair, tenor_days, pips` |
| XCCY BASIS | `pair, tenor_months, basis_bps` |

### Credit
| Surface | Required Columns |
|---|---|
| CDS SPREAD | `entity, tenor_months, spread_bps` |
| TRANSITION | `from_rating, to_rating, probability_pct` |
| RECOVERY | `sector, seniority, recovery_pct` |

### Commodities
| Surface | Required Columns |
|---|---|
| CMDTY FWD | `commodity, contract_months, price` |
| CMDTY VOL | `dte, strike_pct, vol_pct` |
| CRACK/CRUSH | `spread_type, contract_months, spread_usd` |
| CONTANGO | `commodity, contract_months, pct_from_spot` |

### Risk & Portfolio
| Surface | Required Columns |
|---|---|
| CORRELATION | `asset_i, asset_j, correlation` |
| PCA | `asset, factor, loading` |
| VAR | `confidence_pct, horizon_days, var_pct` |
| STRESS TEST | `scenario, portfolio, pnl_pct` |
| FACTOR EXP | `asset, factor, exposure` |
| LIQUIDITY | `dte, strike, spread` |
| DRAWDOWN | `asset, window_days, max_dd_pct` |
| BETA | `asset, horizon_days, beta` |
| IMPL DIV | `dte, strike, div_yield_pct` |

### Macro
| Surface | Required Columns |
|---|---|
| INFLATION | `day, horizon_years, bei_pct` |
| RATE PATH | `central_bank, meeting_num, rate_pct` |

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `src/screens/surface_analytics/csv_importer.h` | **CREATE** | Schema definitions, parser function declarations |
| `src/screens/surface_analytics/csv_importer.cpp` | **CREATE** | Generic CSV parser + all 35 per-surface parse functions |
| `src/screens/surface_analytics/surface_screen.h` | **MODIFY** | Add import state members + `render_csv_import_dialog()` declaration |
| `src/screens/surface_analytics/surface_screen.cpp` | **MODIFY** | Add IMPORT button to control bar; call `render_csv_import_dialog()` |
| `src/screens/surface_analytics/import_dialog.cpp` | **CREATE** | `render_csv_import_dialog()` — modal UI with schema display + load logic |
| `CMakeLists.txt` | **MODIFY** | Add `csv_importer.cpp` and `import_dialog.cpp` to SOURCES + SKIP_UNITY_BUILD |

---

## Task 1: CSV Schema Definitions + Generic Parser (`csv_importer.h` + `csv_importer.cpp`)

**Files:**
- Create: `src/screens/surface_analytics/csv_importer.h`
- Create: `src/screens/surface_analytics/csv_importer.cpp`

- [ ] **Step 1: Create `csv_importer.h`**

```cpp
#pragma once
#include "surface_types.h"
#include <string>
#include <vector>

namespace fincept::surface {

// ── Schema descriptor ────────────────────────────────────────────────────────
struct CsvColumn {
    const char* name;        // e.g. "strike"
    const char* description; // e.g. "Option strike price (absolute, not %)"
};

struct CsvSchema {
    ChartType   type;
    const char* surface_name;
    std::vector<CsvColumn> columns;
    const char* example_row;  // one realistic example row
};

// Returns the schema for the given chart type
const CsvSchema& get_csv_schema(ChartType type);

// ── Generic CSV parser ────────────────────────────────────────────────────────
// Returns rows as vector<vector<string>>; first row assumed header.
// On error, out_error is set and return is empty.
std::vector<std::vector<std::string>> parse_csv_file(
    const std::string& path,
    std::string& out_error);

// ── Per-surface loaders ───────────────────────────────────────────────────────
// Each function reads the parsed rows and fills the data struct.
// Returns true on success; sets out_error on failure.

bool load_vol_surface(    const std::vector<std::vector<std::string>>& rows, VolatilitySurfaceData& out, std::string& err);
bool load_greeks_surface( const std::vector<std::vector<std::string>>& rows, GreeksSurfaceData& out,     std::string& err, const std::string& greek_name);
bool load_skew_surface(   const std::vector<std::vector<std::string>>& rows, SkewSurfaceData& out,       std::string& err);
bool load_local_vol(      const std::vector<std::vector<std::string>>& rows, LocalVolData& out,          std::string& err);
bool load_yield_curve(    const std::vector<std::vector<std::string>>& rows, YieldCurveData& out,        std::string& err);
bool load_swaption_vol(   const std::vector<std::vector<std::string>>& rows, SwaptionVolData& out,       std::string& err);
bool load_capfloor_vol(   const std::vector<std::vector<std::string>>& rows, CapFloorVolData& out,       std::string& err);
bool load_bond_spread(    const std::vector<std::vector<std::string>>& rows, BondSpreadData& out,        std::string& err);
bool load_ois_basis(      const std::vector<std::vector<std::string>>& rows, OISBasisData& out,          std::string& err);
bool load_real_yield(     const std::vector<std::vector<std::string>>& rows, RealYieldData& out,         std::string& err);
bool load_forward_rate(   const std::vector<std::vector<std::string>>& rows, ForwardRateData& out,       std::string& err);
bool load_fx_vol(         const std::vector<std::vector<std::string>>& rows, FXVolData& out,             std::string& err);
bool load_fx_forward(     const std::vector<std::vector<std::string>>& rows, FXForwardPointsData& out,   std::string& err);
bool load_xccy_basis(     const std::vector<std::vector<std::string>>& rows, CrossCurrencyBasisData& out,std::string& err);
bool load_cds_spread(     const std::vector<std::vector<std::string>>& rows, CDSSpreadData& out,         std::string& err);
bool load_credit_trans(   const std::vector<std::vector<std::string>>& rows, CreditTransitionData& out,  std::string& err);
bool load_recovery_rate(  const std::vector<std::vector<std::string>>& rows, RecoveryRateData& out,      std::string& err);
bool load_cmdty_forward(  const std::vector<std::vector<std::string>>& rows, CommodityForwardData& out,  std::string& err);
bool load_cmdty_vol(      const std::vector<std::vector<std::string>>& rows, CommodityVolData& out,      std::string& err);
bool load_crack_spread(   const std::vector<std::vector<std::string>>& rows, CrackSpreadData& out,       std::string& err);
bool load_contango(       const std::vector<std::vector<std::string>>& rows, ContangoData& out,          std::string& err);
bool load_correlation(    const std::vector<std::vector<std::string>>& rows, CorrelationMatrixData& out, std::string& err);
bool load_pca(            const std::vector<std::vector<std::string>>& rows, PCAData& out,               std::string& err);
bool load_var(            const std::vector<std::vector<std::string>>& rows, VaRData& out,               std::string& err);
bool load_stress_test(    const std::vector<std::vector<std::string>>& rows, StressTestData& out,        std::string& err);
bool load_factor_exposure(const std::vector<std::vector<std::string>>& rows, FactorExposureData& out,    std::string& err);
bool load_liquidity(      const std::vector<std::vector<std::string>>& rows, LiquidityData& out,         std::string& err);
bool load_drawdown(       const std::vector<std::vector<std::string>>& rows, DrawdownData& out,          std::string& err);
bool load_beta(           const std::vector<std::vector<std::string>>& rows, BetaData& out,              std::string& err);
bool load_implied_div(    const std::vector<std::vector<std::string>>& rows, ImpliedDividendData& out,   std::string& err);
bool load_inflation(      const std::vector<std::vector<std::string>>& rows, InflationExpData& out,      std::string& err);
bool load_monetary(       const std::vector<std::vector<std::string>>& rows, MonetaryPolicyData& out,    std::string& err);

} // namespace fincept::surface
```

- [ ] **Step 2: Create `csv_importer.cpp` — schema table + generic parser**

```cpp
#include "csv_importer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <cstdio>

// Must be listed in SKIP_UNITY_BUILD_INCLUSION in CMakeLists.txt

namespace fincept::surface {

// ── Schema table ─────────────────────────────────────────────────────────────
static const CsvSchema SCHEMAS[] = {
    { ChartType::Volatility,            "VOL SURFACE",
      {{"strike","Option strike (absolute price)"},{"dte","Days to expiry (integer)"},{"iv_pct","Implied vol % e.g. 22.5"}},
      "450.0,30,22.5" },
    { ChartType::DeltaSurface,          "DELTA",
      {{"strike","Option strike"},{"dte","Days to expiry"},{"delta","Delta value 0..1"}},
      "450.0,30,0.52" },
    { ChartType::GammaSurface,          "GAMMA",
      {{"strike","Option strike"},{"dte","Days to expiry"},{"gamma","Gamma value"}},
      "450.0,30,0.0031" },
    { ChartType::VegaSurface,           "VEGA",
      {{"strike","Option strike"},{"dte","Days to expiry"},{"vega","Vega value"}},
      "450.0,30,0.85" },
    { ChartType::ThetaSurface,          "THETA",
      {{"strike","Option strike"},{"dte","Days to expiry"},{"theta","Theta (negative decay per day)"}},
      "450.0,30,-0.12" },
    { ChartType::SkewSurface,           "SKEW",
      {{"dte","Days to expiry"},{"delta","Delta e.g. 10,25,50,75,90"},{"skew_pct","Skew value %"}},
      "30,25,-1.2" },
    { ChartType::LocalVolSurface,       "LOCAL VOL",
      {{"strike","Option strike"},{"dte","Days to expiry"},{"local_vol_pct","Local vol % (Dupire)"}},
      "450.0,30,21.0" },
    { ChartType::YieldCurve,            "YIELD CURVE",
      {{"maturity_months","Maturity in months e.g. 3,6,12,24"},{"date","Row date label (integer day index or YYYY-MM-DD)"},{"yield_pct","Yield % e.g. 4.35"}},
      "24,2024-01-15,4.35" },
    { ChartType::SwaptionVol,           "SWAPTION VOL",
      {{"option_expiry_months","Option expiry in months"},{"swap_tenor_months","Swap tenor in months"},{"vol_bps","Normal vol in bps"}},
      "12,60,85" },
    { ChartType::CapFloorVol,           "CAP/FLOOR VOL",
      {{"maturity_months","Cap/floor maturity months"},{"strike_pct","Strike rate %"},{"vol_bps","Normal vol bps"}},
      "24,4.5,62" },
    { ChartType::BondSpread,            "BOND SPREAD",
      {{"rating","Credit rating e.g. AAA,AA,A,BBB,BB,B,CCC"},{"maturity_months","Bond maturity months"},{"spread_bps","OAS spread bps"}},
      "BBB,60,120" },
    { ChartType::OISBasis,              "OIS BASIS",
      {{"day","Time point index (integer, oldest=0)"},{"tenor_months","Tenor months"},{"basis_bps","OIS-SOFR basis bps"}},
      "0,12,2.5" },
    { ChartType::RealYield,             "REAL YIELD",
      {{"day","Time point index"},{"maturity_months","TIPS maturity months"},{"yield_pct","Real yield %"}},
      "0,60,1.82" },
    { ChartType::ForwardRate,           "FWD RATE",
      {{"start_months","Forward start in months"},{"fwd_period_months","Forward period months"},{"rate_pct","Forward rate %"}},
      "12,12,4.8" },
    { ChartType::FXVol,                 "FX VOL",
      {{"tenor_days","Tenor in days"},{"delta","Delta e.g. 10,25,50,75,90"},{"vol_pct","FX implied vol %"}},
      "30,25,7.2" },
    { ChartType::FXForwardPoints,       "FX FORWARD",
      {{"pair","Currency pair e.g. EUR/USD"},{"tenor_days","Tenor days"},{"pips","Forward points in pips"}},
      "EUR/USD,30,-12.5" },
    { ChartType::CrossCurrencyBasis,    "XCCY BASIS",
      {{"pair","Currency pair vs USD e.g. EUR"},{"tenor_months","Tenor months"},{"basis_bps","XCCY basis bps (negative=USD premium)"}},
      "EUR,12,-22" },
    { ChartType::CDSSpread,             "CDS SPREAD",
      {{"entity","Company or sovereign name"},{"tenor_months","CDS tenor months"},{"spread_bps","CDS spread bps"}},
      "Apple Inc,60,45" },
    { ChartType::CreditTransition,      "TRANSITION MATRIX",
      {{"from_rating","Initial rating e.g. AAA"},{"to_rating","Final rating e.g. AA"},{"probability_pct","Transition probability %"}},
      "AAA,AA,8.2" },
    { ChartType::RecoveryRate,          "RECOVERY RATE",
      {{"sector","Industry sector e.g. Technology"},{"seniority","Debt seniority e.g. Senior,Sub,Junior"},{"recovery_pct","Recovery rate %"}},
      "Technology,Senior,52.0" },
    { ChartType::CommodityForward,      "CMDTY FORWARD",
      {{"commodity","Commodity name e.g. WTI Crude"},{"contract_months","Months forward"},{"price","Forward price"}},
      "WTI Crude,3,78.50" },
    { ChartType::CommodityVol,          "CMDTY VOL",
      {{"dte","Days to expiry"},{"strike_pct","Strike as % of spot e.g. 90,100,110"},{"vol_pct","Implied vol %"}},
      "30,100,32.5" },
    { ChartType::CrackSpread,           "CRACK/CRUSH SPREAD",
      {{"spread_type","Spread name e.g. 3-2-1 Crack"},{"contract_months","Months forward"},{"spread_usd","Spread value USD/unit"}},
      "3-2-1 Crack,3,24.5" },
    { ChartType::ContangoBackwardation, "CONTANGO/BACKWARDATION",
      {{"commodity","Commodity name"},{"contract_months","Months forward"},{"pct_from_spot","% above(+) or below(-) spot"}},
      "WTI Crude,3,1.8" },
    { ChartType::Correlation,           "CORRELATION",
      {{"asset_i","First asset ticker"},{"asset_j","Second asset ticker"},{"correlation","Pearson correlation -1..1"}},
      "SPY,QQQ,0.92" },
    { ChartType::PCA,                   "PCA LOADINGS",
      {{"asset","Asset ticker"},{"factor","Factor name e.g. PC1,PC2,PC3"},{"loading","Factor loading value"}},
      "SPY,PC1,0.45" },
    { ChartType::VaR,                   "VAR",
      {{"confidence_pct","Confidence level % e.g. 95,99"},{"horizon_days","Holding period days"},{"var_pct","VaR as % of portfolio"}},
      "99,1,2.35" },
    { ChartType::StressTestPnL,         "STRESS TEST",
      {{"scenario","Scenario name e.g. 2008 Crisis"},{"portfolio","Portfolio name"},{"pnl_pct","P&L % (negative=loss)"}},
      "2008 Crisis,Core Portfolio,-18.5" },
    { ChartType::FactorExposure,        "FACTOR EXPOSURE",
      {{"asset","Asset ticker"},{"factor","Factor name e.g. Market,Size,Value"},{"exposure","Factor exposure (beta-like)"}},
      "AAPL,Market,1.12" },
    { ChartType::LiquidityHeatmap,      "LIQUIDITY",
      {{"dte","Days to expiry"},{"strike","Strike price"},{"spread","Bid-ask spread"}},
      "30,450,0.15" },
    { ChartType::Drawdown,              "DRAWDOWN",
      {{"asset","Asset ticker"},{"window_days","Rolling window days"},{"max_dd_pct","Max drawdown % (negative)"}},
      "SPY,252,-19.4" },
    { ChartType::BetaSurface,           "BETA",
      {{"asset","Asset ticker"},{"horizon_days","Rolling horizon days"},{"beta","Beta to SPX"}},
      "AAPL,60,1.18" },
    { ChartType::ImpliedDividend,       "IMPLIED DIVIDEND",
      {{"dte","Days to expiry"},{"strike","Strike price"},{"div_yield_pct","Implied dividend yield %"}},
      "60,450,0.58" },
    { ChartType::InflationExpectations, "INFLATION EXPECTATIONS",
      {{"day","Time point index"},{"horizon_years","Breakeven horizon years e.g. 1,2,5,10"},{"bei_pct","Breakeven inflation %"}},
      "0,5,2.38" },
    { ChartType::MonetaryPolicyPath,    "RATE PATH",
      {{"central_bank","Bank name e.g. Fed,ECB,BoJ"},{"meeting_num","Meeting number ahead (1-based)"},{"rate_pct","Implied policy rate %"}},
      "Fed,1,5.25" },
};

static constexpr int N_SCHEMAS = (int)(sizeof(SCHEMAS) / sizeof(SCHEMAS[0]));

const CsvSchema& get_csv_schema(ChartType type) {
    for (int i = 0; i < N_SCHEMAS; i++)
        if (SCHEMAS[i].type == type) return SCHEMAS[i];
    return SCHEMAS[0]; // fallback
}

// ── Generic CSV parser ────────────────────────────────────────────────────────
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    bool in_quotes = false;
    for (char c : line) {
        if (c == '"') { in_quotes = !in_quotes; }
        else if (c == ',' && !in_quotes) { cols.push_back(trim(cur)); cur.clear(); }
        else { cur += c; }
    }
    cols.push_back(trim(cur));
    return cols;
}

std::vector<std::vector<std::string>> parse_csv_file(
    const std::string& path, std::string& out_error)
{
    std::ifstream f(path);
    if (!f.is_open()) { out_error = "Cannot open file: " + path; return {}; }
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue; // skip blank/comment
        rows.push_back(split_csv_line(line));
    }
    if (rows.empty()) { out_error = "File is empty."; return {}; }
    return rows;
}

// ── Helper: find column index by header name ──────────────────────────────────
static int col_idx(const std::vector<std::string>& header, const std::string& name) {
    for (int i = 0; i < (int)header.size(); i++)
        if (trim(header[i]) == name) return i;
    return -1;
}

static float to_f(const std::string& s) { return std::stof(s); }
static int   to_i(const std::string& s) { return std::stoi(s); }

// ── Per-surface loaders ───────────────────────────────────────────────────────

// Pivot helper: rows with (row_key_col, col_key_col, value_col)
// builds a sorted 2D grid: unique row keys → unique col keys → float values
// Fills string vectors and float 2D grid into output parameters.
static bool pivot_string_string_float(
    const std::vector<std::vector<std::string>>& rows,
    int ri, int ci, int vi,
    std::vector<std::string>& out_rows,
    std::vector<std::string>& out_cols,
    std::vector<std::vector<float>>& out_z,
    std::string& err)
{
    // Collect unique keys preserving first-seen order
    std::vector<std::string> rkeys, ckeys;
    for (int i = 1; i < (int)rows.size(); i++) {
        if ((int)rows[i].size() <= vi) continue;
        const std::string& rk = rows[i][ri];
        const std::string& ck = rows[i][ci];
        if (std::find(rkeys.begin(), rkeys.end(), rk) == rkeys.end()) rkeys.push_back(rk);
        if (std::find(ckeys.begin(), ckeys.end(), ck) == ckeys.end()) ckeys.push_back(ck);
    }
    if (rkeys.empty() || ckeys.empty()) { err = "No data rows found."; return false; }
    out_rows = rkeys; out_cols = ckeys;
    out_z.assign(rkeys.size(), std::vector<float>(ckeys.size(), 0.0f));
    for (int i = 1; i < (int)rows.size(); i++) {
        if ((int)rows[i].size() <= vi) continue;
        int ri2 = (int)(std::find(rkeys.begin(), rkeys.end(), rows[i][ri]) - rkeys.begin());
        int ci2 = (int)(std::find(ckeys.begin(), ckeys.end(), rows[i][ci]) - ckeys.begin());
        try { out_z[ri2][ci2] = to_f(rows[i][vi]); } catch (...) {}
    }
    return true;
}

// VOL SURFACE: strike, dte, iv_pct  →  z[dte_idx][strike_idx]
bool load_vol_surface(const std::vector<std::vector<std::string>>& rows,
                      VolatilitySurfaceData& out, std::string& err)
{
    const auto& h = rows[0];
    int si = col_idx(h,"strike"), di = col_idx(h,"dte"), vi = col_idx(h,"iv_pct");
    if (si<0||di<0||vi<0) { err="Missing columns: strike, dte, iv_pct"; return false; }

    std::map<int,std::map<float,float>> grid; // dte -> strike -> iv
    for (int i=1;i<(int)rows.size();i++) {
        if ((int)rows[i].size()<=vi) continue;
        try {
            int dte = to_i(rows[i][di]);
            float sk = to_f(rows[i][si]);
            float iv = to_f(rows[i][vi]);
            grid[dte][sk] = iv;
        } catch (...) {}
    }
    if (grid.empty()) { err="No valid data rows."; return false; }

    out.expirations.clear(); out.strikes.clear(); out.z.clear();
    // Build sorted dte list
    for (auto& [d,_] : grid) out.expirations.push_back(d);
    // Build sorted strike list from first dte
    for (auto& [s,_] : grid.begin()->second) out.strikes.push_back(s);
    // Fill z
    for (int d : out.expirations) {
        std::vector<float> row2;
        for (float s : out.strikes)
            row2.push_back(grid[d].count(s) ? grid[d][s] : 0.0f);
        out.z.push_back(row2);
    }
    out.underlying = "IMPORTED";
    out.spot_price = out.strikes[out.strikes.size()/2];
    return true;
}

// GREEKS SURFACE: strike, dte, <greek_col>
bool load_greeks_surface(const std::vector<std::vector<std::string>>& rows,
                         GreeksSurfaceData& out, std::string& err,
                         const std::string& greek_name)
{
    // Column name is lowercase greek name: delta/gamma/vega/theta/local_vol_pct/skew_pct
    std::string vcol = greek_name;
    // normalize
    if (vcol=="Delta") vcol="delta";
    else if (vcol=="Gamma") vcol="gamma";
    else if (vcol=="Vega")  vcol="vega";
    else if (vcol=="Theta") vcol="theta";

    const auto& h = rows[0];
    int si=col_idx(h,"strike"), di=col_idx(h,"dte"), vi=col_idx(h,vcol);
    if (si<0||di<0||vi<0) { err="Missing columns: strike, dte, "+vcol; return false; }

    std::map<int,std::map<float,float>> grid;
    for (int i=1;i<(int)rows.size();i++) {
        if ((int)rows[i].size()<=vi) continue;
        try { grid[to_i(rows[i][di])][to_f(rows[i][si])] = to_f(rows[i][vi]); } catch (...) {}
    }
    if (grid.empty()) { err="No valid data rows."; return false; }

    out.expirations.clear(); out.strikes.clear(); out.z.clear();
    for (auto& [d,_]:grid) out.expirations.push_back(d);
    for (auto& [s,_]:grid.begin()->second) out.strikes.push_back(s);
    for (int d:out.expirations) {
        std::vector<float> row2;
        for (float s:out.strikes) row2.push_back(grid[d].count(s)?grid[d][s]:0.0f);
        out.z.push_back(row2);
    }
    out.greek_name = greek_name;
    out.underlying = "IMPORTED";
    out.spot_price = out.strikes[out.strikes.size()/2];
    return true;
}

// SKEW: dte, delta, skew_pct
bool load_skew_surface(const std::vector<std::vector<std::string>>& rows,
                       SkewSurfaceData& out, std::string& err)
{
    const auto& h = rows[0];
    int di=col_idx(h,"dte"), li=col_idx(h,"delta"), vi=col_idx(h,"skew_pct");
    if (di<0||li<0||vi<0){err="Missing columns: dte, delta, skew_pct";return false;}
    std::map<int,std::map<float,float>> grid;
    for (int i=1;i<(int)rows.size();i++){
        if ((int)rows[i].size()<=vi) continue;
        try{grid[to_i(rows[i][di])][to_f(rows[i][li])]=to_f(rows[i][vi]);}catch(...){}
    }
    if (grid.empty()){err="No valid data rows.";return false;}
    out.expirations.clear();out.deltas.clear();out.z.clear();
    for(auto&[d,_]:grid) out.expirations.push_back(d);
    for(auto&[d,_]:grid.begin()->second) out.deltas.push_back(d);
    for(int d:out.expirations){
        std::vector<float> r;
        for(float dl:out.deltas) r.push_back(grid[d].count(dl)?grid[d][dl]:0.0f);
        out.z.push_back(r);
    }
    out.underlying="IMPORTED";
    return true;
}

// LOCAL VOL: strike, dte, local_vol_pct
bool load_local_vol(const std::vector<std::vector<std::string>>& rows,
                    LocalVolData& out, std::string& err)
{
    const auto& h=rows[0];
    int si=col_idx(h,"strike"),di=col_idx(h,"dte"),vi=col_idx(h,"local_vol_pct");
    if(si<0||di<0||vi<0){err="Missing columns: strike, dte, local_vol_pct";return false;}
    std::map<int,std::map<float,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{grid[to_i(rows[i][di])][to_f(rows[i][si])]=to_f(rows[i][vi]);}catch(...){}
    }
    if(grid.empty()){err="No valid data rows.";return false;}
    out.expirations.clear();out.strikes.clear();out.z.clear();
    for(auto&[d,_]:grid) out.expirations.push_back(d);
    for(auto&[s,_]:grid.begin()->second) out.strikes.push_back(s);
    for(int d:out.expirations){
        std::vector<float> r;
        for(float s:out.strikes) r.push_back(grid[d].count(s)?grid[d][s]:0.0f);
        out.z.push_back(r);
    }
    out.underlying="IMPORTED"; out.spot_price=out.strikes[out.strikes.size()/2];
    return true;
}

// YIELD CURVE: maturity_months, date, yield_pct  →  z[time][maturity]
bool load_yield_curve(const std::vector<std::vector<std::string>>& rows,
                      YieldCurveData& out, std::string& err)
{
    const auto& h=rows[0];
    int mi=col_idx(h,"maturity_months"),di=col_idx(h,"date"),vi=col_idx(h,"yield_pct");
    if(mi<0||di<0||vi<0){err="Missing columns: maturity_months, date, yield_pct";return false;}
    // date strings as row keys, maturities as col keys
    std::vector<std::string> dates; std::vector<int> mats;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string dt=rows[i][di]; int m=to_i(rows[i][mi]); float y=to_f(rows[i][vi]);
            if(std::find(dates.begin(),dates.end(),dt)==dates.end()) dates.push_back(dt);
            if(std::find(mats.begin(),mats.end(),m)==mats.end()) mats.push_back(m);
            grid[dt][m]=y;
        }catch(...){}
    }
    if(grid.empty()){err="No valid data rows.";return false;}
    std::sort(mats.begin(),mats.end());
    out.maturities=mats; out.z.clear();
    for(auto& dt:dates){
        std::vector<float> r;
        for(int m:mats) r.push_back(grid[dt].count(m)?grid[dt][m]:0.0f);
        out.z.push_back(r);
    }
    return true;
}

// SWAPTION VOL: option_expiry_months, swap_tenor_months, vol_bps
bool load_swaption_vol(const std::vector<std::vector<std::string>>& rows,
                       SwaptionVolData& out, std::string& err)
{
    const auto& h=rows[0];
    int ei=col_idx(h,"option_expiry_months"),ti=col_idx(h,"swap_tenor_months"),vi=col_idx(h,"vol_bps");
    if(ei<0||ti<0||vi<0){err="Missing: option_expiry_months, swap_tenor_months, vol_bps";return false;}
    std::map<int,std::map<int,float>> grid;
    std::vector<int> exps,tenors;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int e=to_i(rows[i][ei]),t=to_i(rows[i][ti]); float v=to_f(rows[i][vi]);
            if(std::find(exps.begin(),exps.end(),e)==exps.end()) exps.push_back(e);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            grid[e][t]=v;
        }catch(...){}
    }
    std::sort(exps.begin(),exps.end()); std::sort(tenors.begin(),tenors.end());
    out.option_expiries=exps; out.swap_tenors=tenors; out.z.clear();
    for(int e:exps){std::vector<float> r; for(int t:tenors) r.push_back(grid[e].count(t)?grid[e][t]:0.0f); out.z.push_back(r);}
    return true;
}

// CAP/FLOOR VOL: maturity_months, strike_pct, vol_bps
bool load_capfloor_vol(const std::vector<std::vector<std::string>>& rows,
                       CapFloorVolData& out, std::string& err)
{
    const auto& h=rows[0];
    int mi=col_idx(h,"maturity_months"),si=col_idx(h,"strike_pct"),vi=col_idx(h,"vol_bps");
    if(mi<0||si<0||vi<0){err="Missing: maturity_months, strike_pct, vol_bps";return false;}
    std::map<int,std::map<float,float>> grid; std::vector<int> mats; std::vector<float> strikes;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int m=to_i(rows[i][mi]); float s=to_f(rows[i][si]),v=to_f(rows[i][vi]);
            if(std::find(mats.begin(),mats.end(),m)==mats.end()) mats.push_back(m);
            if(std::find(strikes.begin(),strikes.end(),s)==strikes.end()) strikes.push_back(s);
            grid[m][s]=v;
        }catch(...){}
    }
    std::sort(mats.begin(),mats.end()); std::sort(strikes.begin(),strikes.end());
    out.maturities=mats; out.strikes=strikes; out.z.clear();
    for(int m:mats){std::vector<float> r; for(float s:strikes) r.push_back(grid[m].count(s)?grid[m][s]:0.0f); out.z.push_back(r);}
    return true;
}

// BOND SPREAD: rating, maturity_months, spread_bps
bool load_bond_spread(const std::vector<std::vector<std::string>>& rows,
                      BondSpreadData& out, std::string& err)
{
    const auto& h=rows[0];
    int ri=col_idx(h,"rating"),mi=col_idx(h,"maturity_months"),vi=col_idx(h,"spread_bps");
    if(ri<0||mi<0||vi<0){err="Missing: rating, maturity_months, spread_bps";return false;}
    std::vector<std::string> ratings; std::vector<int> mats;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string rg=rows[i][ri]; int m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(ratings.begin(),ratings.end(),rg)==ratings.end()) ratings.push_back(rg);
            if(std::find(mats.begin(),mats.end(),m)==mats.end()) mats.push_back(m);
            grid[rg][m]=v;
        }catch(...){}
    }
    std::sort(mats.begin(),mats.end());
    out.ratings=ratings; out.maturities=mats; out.z.clear();
    for(auto& rg:ratings){std::vector<float> r; for(int m:mats) r.push_back(grid[rg].count(m)?grid[rg][m]:0.0f); out.z.push_back(r);}
    return true;
}

// OIS BASIS: day, tenor_months, basis_bps
bool load_ois_basis(const std::vector<std::vector<std::string>>& rows,
                    OISBasisData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"day"),ti=col_idx(h,"tenor_months"),vi=col_idx(h,"basis_bps");
    if(di<0||ti<0||vi<0){err="Missing: day, tenor_months, basis_bps";return false;}
    std::map<int,std::map<int,float>> grid; std::vector<int> days,tenors;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]),t=to_i(rows[i][ti]); float v=to_f(rows[i][vi]);
            if(std::find(days.begin(),days.end(),d)==days.end()) days.push_back(d);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            grid[d][t]=v;
        }catch(...){}
    }
    std::sort(days.begin(),days.end()); std::sort(tenors.begin(),tenors.end());
    out.time_points=days; out.tenors=tenors; out.z.clear();
    for(int d:days){std::vector<float> r; for(int t:tenors) r.push_back(grid[d].count(t)?grid[d][t]:0.0f); out.z.push_back(r);}
    return true;
}

// REAL YIELD: day, maturity_months, yield_pct
bool load_real_yield(const std::vector<std::vector<std::string>>& rows,
                     RealYieldData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"day"),mi=col_idx(h,"maturity_months"),vi=col_idx(h,"yield_pct");
    if(di<0||mi<0||vi<0){err="Missing: day, maturity_months, yield_pct";return false;}
    std::map<int,std::map<int,float>> grid; std::vector<int> days,mats;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]),m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(days.begin(),days.end(),d)==days.end()) days.push_back(d);
            if(std::find(mats.begin(),mats.end(),m)==mats.end()) mats.push_back(m);
            grid[d][m]=v;
        }catch(...){}
    }
    std::sort(days.begin(),days.end()); std::sort(mats.begin(),mats.end());
    out.time_points=days; out.maturities=mats; out.z.clear();
    for(int d:days){std::vector<float> r; for(int m:mats) r.push_back(grid[d].count(m)?grid[d][m]:0.0f); out.z.push_back(r);}
    return true;
}

// FORWARD RATE: start_months, fwd_period_months, rate_pct
bool load_forward_rate(const std::vector<std::vector<std::string>>& rows,
                       ForwardRateData& out, std::string& err)
{
    const auto& h=rows[0];
    int si=col_idx(h,"start_months"),fi=col_idx(h,"fwd_period_months"),vi=col_idx(h,"rate_pct");
    if(si<0||fi<0||vi<0){err="Missing: start_months, fwd_period_months, rate_pct";return false;}
    std::map<int,std::map<int,float>> grid; std::vector<int> starts,fwds;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int s=to_i(rows[i][si]),f=to_i(rows[i][fi]); float v=to_f(rows[i][vi]);
            if(std::find(starts.begin(),starts.end(),s)==starts.end()) starts.push_back(s);
            if(std::find(fwds.begin(),fwds.end(),f)==fwds.end()) fwds.push_back(f);
            grid[s][f]=v;
        }catch(...){}
    }
    std::sort(starts.begin(),starts.end()); std::sort(fwds.begin(),fwds.end());
    out.start_tenors=starts; out.forward_periods=fwds; out.z.clear();
    for(int s:starts){std::vector<float> r; for(int f:fwds) r.push_back(grid[s].count(f)?grid[s][f]:0.0f); out.z.push_back(r);}
    return true;
}

// FX VOL: tenor_days, delta, vol_pct
bool load_fx_vol(const std::vector<std::vector<std::string>>& rows,
                 FXVolData& out, std::string& err)
{
    const auto& h=rows[0];
    int ti=col_idx(h,"tenor_days"),di=col_idx(h,"delta"),vi=col_idx(h,"vol_pct");
    if(ti<0||di<0||vi<0){err="Missing: tenor_days, delta, vol_pct";return false;}
    std::map<int,std::map<float,float>> grid; std::vector<int> tenors; std::vector<float> deltas;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int t=to_i(rows[i][ti]); float d=to_f(rows[i][di]),v=to_f(rows[i][vi]);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            if(std::find(deltas.begin(),deltas.end(),d)==deltas.end()) deltas.push_back(d);
            grid[t][d]=v;
        }catch(...){}
    }
    std::sort(tenors.begin(),tenors.end()); std::sort(deltas.begin(),deltas.end());
    out.tenors=tenors; out.deltas=deltas; out.pair="IMPORTED"; out.z.clear();
    for(int t:tenors){std::vector<float> r; for(float d:deltas) r.push_back(grid[t].count(d)?grid[t][d]:0.0f); out.z.push_back(r);}
    return true;
}

// FX FORWARD: pair, tenor_days, pips
bool load_fx_forward(const std::vector<std::vector<std::string>>& rows,
                     FXForwardPointsData& out, std::string& err)
{
    const auto& h=rows[0];
    int pi=col_idx(h,"pair"),ti=col_idx(h,"tenor_days"),vi=col_idx(h,"pips");
    if(pi<0||ti<0||vi<0){err="Missing: pair, tenor_days, pips";return false;}
    std::vector<std::string> pairs; std::vector<int> tenors;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string p=rows[i][pi]; int t=to_i(rows[i][ti]); float v=to_f(rows[i][vi]);
            if(std::find(pairs.begin(),pairs.end(),p)==pairs.end()) pairs.push_back(p);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            grid[p][t]=v;
        }catch(...){}
    }
    std::sort(tenors.begin(),tenors.end());
    out.pairs=pairs; out.tenors=tenors; out.z.clear();
    for(auto& p:pairs){std::vector<float> r; for(int t:tenors) r.push_back(grid[p].count(t)?grid[p][t]:0.0f); out.z.push_back(r);}
    return true;
}

// XCCY BASIS: pair, tenor_months, basis_bps
bool load_xccy_basis(const std::vector<std::vector<std::string>>& rows,
                     CrossCurrencyBasisData& out, std::string& err)
{
    const auto& h=rows[0];
    int pi=col_idx(h,"pair"),ti=col_idx(h,"tenor_months"),vi=col_idx(h,"basis_bps");
    if(pi<0||ti<0||vi<0){err="Missing: pair, tenor_months, basis_bps";return false;}
    std::vector<std::string> pairs; std::vector<int> tenors;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string p=rows[i][pi]; int t=to_i(rows[i][ti]); float v=to_f(rows[i][vi]);
            if(std::find(pairs.begin(),pairs.end(),p)==pairs.end()) pairs.push_back(p);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            grid[p][t]=v;
        }catch(...){}
    }
    std::sort(tenors.begin(),tenors.end());
    out.pairs=pairs; out.tenors=tenors; out.z.clear();
    for(auto& p:pairs){std::vector<float> r; for(int t:tenors) r.push_back(grid[p].count(t)?grid[p][t]:0.0f); out.z.push_back(r);}
    return true;
}

// CDS SPREAD: entity, tenor_months, spread_bps
bool load_cds_spread(const std::vector<std::vector<std::string>>& rows,
                     CDSSpreadData& out, std::string& err)
{
    const auto& h=rows[0];
    int ei=col_idx(h,"entity"),ti=col_idx(h,"tenor_months"),vi=col_idx(h,"spread_bps");
    if(ei<0||ti<0||vi<0){err="Missing: entity, tenor_months, spread_bps";return false;}
    std::vector<std::string> entities; std::vector<int> tenors;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string e=rows[i][ei]; int t=to_i(rows[i][ti]); float v=to_f(rows[i][vi]);
            if(std::find(entities.begin(),entities.end(),e)==entities.end()) entities.push_back(e);
            if(std::find(tenors.begin(),tenors.end(),t)==tenors.end()) tenors.push_back(t);
            grid[e][t]=v;
        }catch(...){}
    }
    std::sort(tenors.begin(),tenors.end());
    out.entities=entities; out.tenors=tenors; out.z.clear();
    for(auto& e:entities){std::vector<float> r; for(int t:tenors) r.push_back(grid[e].count(t)?grid[e][t]:0.0f); out.z.push_back(r);}
    return true;
}

// CREDIT TRANSITION: from_rating, to_rating, probability_pct
bool load_credit_trans(const std::vector<std::vector<std::string>>& rows,
                       CreditTransitionData& out, std::string& err)
{
    const auto& h=rows[0];
    int fi=col_idx(h,"from_rating"),ti=col_idx(h,"to_rating"),vi=col_idx(h,"probability_pct");
    if(fi<0||ti<0||vi<0){err="Missing: from_rating, to_rating, probability_pct";return false;}
    std::vector<std::string> froms,tos;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string f=rows[i][fi],t=rows[i][ti]; float v=to_f(rows[i][vi]);
            if(std::find(froms.begin(),froms.end(),f)==froms.end()) froms.push_back(f);
            if(std::find(tos.begin(),tos.end(),t)==tos.end()) tos.push_back(t);
            grid[f][t]=v;
        }catch(...){}
    }
    out.ratings=froms; out.to_ratings=tos; out.z.clear();
    for(auto& f:froms){std::vector<float> r; for(auto& t:tos) r.push_back(grid[f].count(t)?grid[f][t]:0.0f); out.z.push_back(r);}
    return true;
}

// RECOVERY RATE: sector, seniority, recovery_pct
bool load_recovery_rate(const std::vector<std::vector<std::string>>& rows,
                        RecoveryRateData& out, std::string& err)
{
    const auto& h=rows[0];
    int si=col_idx(h,"sector"),ni=col_idx(h,"seniority"),vi=col_idx(h,"recovery_pct");
    if(si<0||ni<0||vi<0){err="Missing: sector, seniority, recovery_pct";return false;}
    std::vector<std::string> sects,sens;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string s=rows[i][si],n=rows[i][ni]; float v=to_f(rows[i][vi]);
            if(std::find(sects.begin(),sects.end(),s)==sects.end()) sects.push_back(s);
            if(std::find(sens.begin(),sens.end(),n)==sens.end()) sens.push_back(n);
            grid[s][n]=v;
        }catch(...){}
    }
    out.sectors=sects; out.seniorities=sens; out.z.clear();
    for(auto& s:sects){std::vector<float> r; for(auto& n:sens) r.push_back(grid[s].count(n)?grid[s][n]:0.0f); out.z.push_back(r);}
    return true;
}

// COMMODITY FORWARD: commodity, contract_months, price
bool load_cmdty_forward(const std::vector<std::vector<std::string>>& rows,
                        CommodityForwardData& out, std::string& err)
{
    const auto& h=rows[0];
    int ci=col_idx(h,"commodity"),mi=col_idx(h,"contract_months"),vi=col_idx(h,"price");
    if(ci<0||mi<0||vi<0){err="Missing: commodity, contract_months, price";return false;}
    std::vector<std::string> comms; std::vector<int> months;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string c=rows[i][ci]; int m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(comms.begin(),comms.end(),c)==comms.end()) comms.push_back(c);
            if(std::find(months.begin(),months.end(),m)==months.end()) months.push_back(m);
            grid[c][m]=v;
        }catch(...){}
    }
    std::sort(months.begin(),months.end());
    out.commodities=comms; out.contract_months=months; out.z.clear();
    for(auto& c:comms){std::vector<float> r; for(int m:months) r.push_back(grid[c].count(m)?grid[c][m]:0.0f); out.z.push_back(r);}
    return true;
}

// COMMODITY VOL: dte, strike_pct, vol_pct
bool load_cmdty_vol(const std::vector<std::vector<std::string>>& rows,
                    CommodityVolData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"dte"),si=col_idx(h,"strike_pct"),vi=col_idx(h,"vol_pct");
    if(di<0||si<0||vi<0){err="Missing: dte, strike_pct, vol_pct";return false;}
    std::map<int,std::map<float,float>> grid; std::vector<int> dtes; std::vector<float> strikes;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]); float s=to_f(rows[i][si]),v=to_f(rows[i][vi]);
            if(std::find(dtes.begin(),dtes.end(),d)==dtes.end()) dtes.push_back(d);
            if(std::find(strikes.begin(),strikes.end(),s)==strikes.end()) strikes.push_back(s);
            grid[d][s]=v;
        }catch(...){}
    }
    std::sort(dtes.begin(),dtes.end()); std::sort(strikes.begin(),strikes.end());
    out.expirations=dtes; out.strikes=strikes; out.commodity="IMPORTED"; out.z.clear();
    for(int d:dtes){std::vector<float> r; for(float s:strikes) r.push_back(grid[d].count(s)?grid[d][s]:0.0f); out.z.push_back(r);}
    return true;
}

// CRACK SPREAD: spread_type, contract_months, spread_usd
bool load_crack_spread(const std::vector<std::vector<std::string>>& rows,
                       CrackSpreadData& out, std::string& err)
{
    const auto& h=rows[0];
    int ti=col_idx(h,"spread_type"),mi=col_idx(h,"contract_months"),vi=col_idx(h,"spread_usd");
    if(ti<0||mi<0||vi<0){err="Missing: spread_type, contract_months, spread_usd";return false;}
    std::vector<std::string> types; std::vector<int> months;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string t=rows[i][ti]; int m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(types.begin(),types.end(),t)==types.end()) types.push_back(t);
            if(std::find(months.begin(),months.end(),m)==months.end()) months.push_back(m);
            grid[t][m]=v;
        }catch(...){}
    }
    std::sort(months.begin(),months.end());
    out.spread_types=types; out.contract_months=months; out.z.clear();
    for(auto& t:types){std::vector<float> r; for(int m:months) r.push_back(grid[t].count(m)?grid[t][m]:0.0f); out.z.push_back(r);}
    return true;
}

// CONTANGO: commodity, contract_months, pct_from_spot
bool load_contango(const std::vector<std::vector<std::string>>& rows,
                   ContangoData& out, std::string& err)
{
    const auto& h=rows[0];
    int ci=col_idx(h,"commodity"),mi=col_idx(h,"contract_months"),vi=col_idx(h,"pct_from_spot");
    if(ci<0||mi<0||vi<0){err="Missing: commodity, contract_months, pct_from_spot";return false;}
    std::vector<std::string> comms; std::vector<int> months;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string c=rows[i][ci]; int m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(comms.begin(),comms.end(),c)==comms.end()) comms.push_back(c);
            if(std::find(months.begin(),months.end(),m)==months.end()) months.push_back(m);
            grid[c][m]=v;
        }catch(...){}
    }
    std::sort(months.begin(),months.end());
    out.commodities=comms; out.contract_months=months; out.z.clear();
    for(auto& c:comms){std::vector<float> r; for(int m:months) r.push_back(grid[c].count(m)?grid[c][m]:0.0f); out.z.push_back(r);}
    return true;
}

// CORRELATION: asset_i, asset_j, correlation
bool load_correlation(const std::vector<std::vector<std::string>>& rows,
                      CorrelationMatrixData& out, std::string& err)
{
    const auto& h=rows[0];
    int ai=col_idx(h,"asset_i"),aj=col_idx(h,"asset_j"),vi=col_idx(h,"correlation");
    if(ai<0||aj<0||vi<0){err="Missing: asset_i, asset_j, correlation";return false;}
    std::vector<std::string> assets;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string a=rows[i][ai],b=rows[i][aj]; float v=to_f(rows[i][vi]);
            if(std::find(assets.begin(),assets.end(),a)==assets.end()) assets.push_back(a);
            if(std::find(assets.begin(),assets.end(),b)==assets.end()) assets.push_back(b);
            grid[a][b]=v; grid[b][a]=v;
        }catch(...){}
    }
    int n=(int)assets.size();
    out.assets=assets; out.z.clear();
    std::vector<float> flat(n*n,0.0f);
    for(int i=0;i<n;i++) for(int j=0;j<n;j++){
        if(i==j) flat[i*n+j]=1.0f;
        else if(grid.count(assets[i])&&grid[assets[i]].count(assets[j]))
            flat[i*n+j]=grid[assets[i]][assets[j]];
    }
    out.z.push_back(flat);
    out.window=0;
    return true;
}

// PCA: asset, factor, loading
bool load_pca(const std::vector<std::vector<std::string>>& rows,
              PCAData& out, std::string& err)
{
    const auto& h=rows[0];
    int ai=col_idx(h,"asset"),fi=col_idx(h,"factor"),vi=col_idx(h,"loading");
    if(ai<0||fi<0||vi<0){err="Missing: asset, factor, loading";return false;}
    std::vector<std::string> assets,factors;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string a=rows[i][ai],f=rows[i][fi]; float v=to_f(rows[i][vi]);
            if(std::find(assets.begin(),assets.end(),a)==assets.end()) assets.push_back(a);
            if(std::find(factors.begin(),factors.end(),f)==factors.end()) factors.push_back(f);
            grid[a][f]=v;
        }catch(...){}
    }
    out.assets=assets; out.factors=factors; out.z.clear();
    for(auto& a:assets){std::vector<float> r; for(auto& f:factors) r.push_back(grid[a].count(f)?grid[a][f]:0.0f); out.z.push_back(r);}
    out.variance_explained.assign(factors.size(),1.0f/(float)factors.size());
    return true;
}

// VAR: confidence_pct, horizon_days, var_pct
bool load_var(const std::vector<std::vector<std::string>>& rows,
              VaRData& out, std::string& err)
{
    const auto& h=rows[0];
    int ci=col_idx(h,"confidence_pct"),hi=col_idx(h,"horizon_days"),vi=col_idx(h,"var_pct");
    if(ci<0||hi<0||vi<0){err="Missing: confidence_pct, horizon_days, var_pct";return false;}
    std::vector<float> confs; std::vector<int> horizons;
    std::map<float,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            float c=to_f(rows[i][ci]); int h2=to_i(rows[i][hi]); float v=to_f(rows[i][vi]);
            if(std::find(confs.begin(),confs.end(),c)==confs.end()) confs.push_back(c);
            if(std::find(horizons.begin(),horizons.end(),h2)==horizons.end()) horizons.push_back(h2);
            grid[c][h2]=v;
        }catch(...){}
    }
    std::sort(confs.begin(),confs.end()); std::sort(horizons.begin(),horizons.end());
    out.confidence_levels=confs; out.horizons=horizons; out.z.clear();
    for(float c:confs){std::vector<float> r; for(int h2:horizons) r.push_back(grid[c].count(h2)?grid[c][h2]:0.0f); out.z.push_back(r);}
    return true;
}

// STRESS TEST: scenario, portfolio, pnl_pct
bool load_stress_test(const std::vector<std::vector<std::string>>& rows,
                      StressTestData& out, std::string& err)
{
    const auto& h=rows[0];
    int si=col_idx(h,"scenario"),pi=col_idx(h,"portfolio"),vi=col_idx(h,"pnl_pct");
    if(si<0||pi<0||vi<0){err="Missing: scenario, portfolio, pnl_pct";return false;}
    std::vector<std::string> scens,ports;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string s=rows[i][si],p=rows[i][pi]; float v=to_f(rows[i][vi]);
            if(std::find(scens.begin(),scens.end(),s)==scens.end()) scens.push_back(s);
            if(std::find(ports.begin(),ports.end(),p)==ports.end()) ports.push_back(p);
            grid[s][p]=v;
        }catch(...){}
    }
    out.scenarios=scens; out.portfolios=ports; out.z.clear();
    for(auto& s:scens){std::vector<float> r; for(auto& p:ports) r.push_back(grid[s].count(p)?grid[s][p]:0.0f); out.z.push_back(r);}
    return true;
}

// FACTOR EXPOSURE: asset, factor, exposure
bool load_factor_exposure(const std::vector<std::vector<std::string>>& rows,
                          FactorExposureData& out, std::string& err)
{
    const auto& h=rows[0];
    int ai=col_idx(h,"asset"),fi=col_idx(h,"factor"),vi=col_idx(h,"exposure");
    if(ai<0||fi<0||vi<0){err="Missing: asset, factor, exposure";return false;}
    std::vector<std::string> assets,factors;
    std::map<std::string,std::map<std::string,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string a=rows[i][ai],f=rows[i][fi]; float v=to_f(rows[i][vi]);
            if(std::find(assets.begin(),assets.end(),a)==assets.end()) assets.push_back(a);
            if(std::find(factors.begin(),factors.end(),f)==factors.end()) factors.push_back(f);
            grid[a][f]=v;
        }catch(...){}
    }
    out.assets=assets; out.factors=factors; out.z.clear();
    for(auto& a:assets){std::vector<float> r; for(auto& f:factors) r.push_back(grid[a].count(f)?grid[a][f]:0.0f); out.z.push_back(r);}
    return true;
}

// LIQUIDITY: dte, strike, spread
bool load_liquidity(const std::vector<std::vector<std::string>>& rows,
                    LiquidityData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"dte"),si=col_idx(h,"strike"),vi=col_idx(h,"spread");
    if(di<0||si<0||vi<0){err="Missing: dte, strike, spread";return false;}
    std::map<int,std::map<float,float>> grid; std::vector<int> dtes; std::vector<float> strikes;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]); float s=to_f(rows[i][si]),v=to_f(rows[i][vi]);
            if(std::find(dtes.begin(),dtes.end(),d)==dtes.end()) dtes.push_back(d);
            if(std::find(strikes.begin(),strikes.end(),s)==strikes.end()) strikes.push_back(s);
            grid[d][s]=v;
        }catch(...){}
    }
    std::sort(dtes.begin(),dtes.end()); std::sort(strikes.begin(),strikes.end());
    out.expirations=dtes; out.strikes=strikes; out.underlying="IMPORTED"; out.z.clear();
    for(int d:dtes){std::vector<float> r; for(float s:strikes) r.push_back(grid[d].count(s)?grid[d][s]:0.0f); out.z.push_back(r);}
    return true;
}

// DRAWDOWN: asset, window_days, max_dd_pct
bool load_drawdown(const std::vector<std::vector<std::string>>& rows,
                   DrawdownData& out, std::string& err)
{
    const auto& h=rows[0];
    int ai=col_idx(h,"asset"),wi=col_idx(h,"window_days"),vi=col_idx(h,"max_dd_pct");
    if(ai<0||wi<0||vi<0){err="Missing: asset, window_days, max_dd_pct";return false;}
    std::vector<std::string> assets; std::vector<int> windows;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string a=rows[i][ai]; int w=to_i(rows[i][wi]); float v=to_f(rows[i][vi]);
            if(std::find(assets.begin(),assets.end(),a)==assets.end()) assets.push_back(a);
            if(std::find(windows.begin(),windows.end(),w)==windows.end()) windows.push_back(w);
            grid[a][w]=v;
        }catch(...){}
    }
    std::sort(windows.begin(),windows.end());
    out.assets=assets; out.windows=windows; out.z.clear();
    for(auto& a:assets){std::vector<float> r; for(int w:windows) r.push_back(grid[a].count(w)?grid[a][w]:0.0f); out.z.push_back(r);}
    return true;
}

// BETA: asset, horizon_days, beta
bool load_beta(const std::vector<std::vector<std::string>>& rows,
               BetaData& out, std::string& err)
{
    const auto& h=rows[0];
    int ai=col_idx(h,"asset"),hi=col_idx(h,"horizon_days"),vi=col_idx(h,"beta");
    if(ai<0||hi<0||vi<0){err="Missing: asset, horizon_days, beta";return false;}
    std::vector<std::string> assets; std::vector<int> horizons;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string a=rows[i][ai]; int h2=to_i(rows[i][hi]); float v=to_f(rows[i][vi]);
            if(std::find(assets.begin(),assets.end(),a)==assets.end()) assets.push_back(a);
            if(std::find(horizons.begin(),horizons.end(),h2)==horizons.end()) horizons.push_back(h2);
            grid[a][h2]=v;
        }catch(...){}
    }
    std::sort(horizons.begin(),horizons.end());
    out.assets=assets; out.horizons=horizons; out.z.clear();
    for(auto& a:assets){std::vector<float> r; for(int h2:horizons) r.push_back(grid[a].count(h2)?grid[a][h2]:0.0f); out.z.push_back(r);}
    return true;
}

// IMPLIED DIVIDEND: dte, strike, div_yield_pct
bool load_implied_div(const std::vector<std::vector<std::string>>& rows,
                      ImpliedDividendData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"dte"),si=col_idx(h,"strike"),vi=col_idx(h,"div_yield_pct");
    if(di<0||si<0||vi<0){err="Missing: dte, strike, div_yield_pct";return false;}
    std::map<int,std::map<float,float>> grid; std::vector<int> dtes; std::vector<float> strikes;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]); float s=to_f(rows[i][si]),v=to_f(rows[i][vi]);
            if(std::find(dtes.begin(),dtes.end(),d)==dtes.end()) dtes.push_back(d);
            if(std::find(strikes.begin(),strikes.end(),s)==strikes.end()) strikes.push_back(s);
            grid[d][s]=v;
        }catch(...){}
    }
    std::sort(dtes.begin(),dtes.end()); std::sort(strikes.begin(),strikes.end());
    out.expirations=dtes; out.strikes=strikes; out.underlying="IMPORTED"; out.z.clear();
    for(int d:dtes){std::vector<float> r; for(float s:strikes) r.push_back(grid[d].count(s)?grid[d][s]:0.0f); out.z.push_back(r);}
    return true;
}

// INFLATION: day, horizon_years, bei_pct
bool load_inflation(const std::vector<std::vector<std::string>>& rows,
                    InflationExpData& out, std::string& err)
{
    const auto& h=rows[0];
    int di=col_idx(h,"day"),hi=col_idx(h,"horizon_years"),vi=col_idx(h,"bei_pct");
    if(di<0||hi<0||vi<0){err="Missing: day, horizon_years, bei_pct";return false;}
    std::map<int,std::map<int,float>> grid; std::vector<int> days,horizons;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            int d=to_i(rows[i][di]),h2=to_i(rows[i][hi]); float v=to_f(rows[i][vi]);
            if(std::find(days.begin(),days.end(),d)==days.end()) days.push_back(d);
            if(std::find(horizons.begin(),horizons.end(),h2)==horizons.end()) horizons.push_back(h2);
            grid[d][h2]=v;
        }catch(...){}
    }
    std::sort(days.begin(),days.end()); std::sort(horizons.begin(),horizons.end());
    out.time_points=days; out.horizons=horizons; out.z.clear();
    for(int d:days){std::vector<float> r; for(int h2:horizons) r.push_back(grid[d].count(h2)?grid[d][h2]:0.0f); out.z.push_back(r);}
    return true;
}

// MONETARY POLICY: central_bank, meeting_num, rate_pct
bool load_monetary(const std::vector<std::vector<std::string>>& rows,
                   MonetaryPolicyData& out, std::string& err)
{
    const auto& h=rows[0];
    int bi=col_idx(h,"central_bank"),mi=col_idx(h,"meeting_num"),vi=col_idx(h,"rate_pct");
    if(bi<0||mi<0||vi<0){err="Missing: central_bank, meeting_num, rate_pct";return false;}
    std::vector<std::string> banks; std::vector<int> meetings;
    std::map<std::string,std::map<int,float>> grid;
    for(int i=1;i<(int)rows.size();i++){
        if((int)rows[i].size()<=vi) continue;
        try{
            std::string b=rows[i][bi]; int m=to_i(rows[i][mi]); float v=to_f(rows[i][vi]);
            if(std::find(banks.begin(),banks.end(),b)==banks.end()) banks.push_back(b);
            if(std::find(meetings.begin(),meetings.end(),m)==meetings.end()) meetings.push_back(m);
            grid[b][m]=v;
        }catch(...){}
    }
    std::sort(meetings.begin(),meetings.end());
    out.central_banks=banks; out.meetings_ahead=meetings; out.z.clear();
    for(auto& b:banks){std::vector<float> r; for(int m:meetings) r.push_back(grid[b].count(m)?grid[b][m]:0.0f); out.z.push_back(r);}
    return true;
}

} // namespace fincept::surface
```

- [ ] **Step 3: Verify it compiles (added to CMake in Task 4 first)**

---

## Task 2: Import Dialog UI (`import_dialog.cpp`)

**Files:**
- Create: `src/screens/surface_analytics/import_dialog.cpp`
- Modify: `src/screens/surface_analytics/surface_screen.h` (add state + declaration)

- [ ] **Step 1: Add state members + declaration to `surface_screen.h`**

Add inside `class SurfaceScreen { private:` after `bool show_table_ = false;`:

```cpp
// CSV import state
bool   show_import_dialog_ = false;
char   import_path_buf_[512] = {};
std::string import_error_;
bool   import_success_ = false;

void render_csv_import_dialog();
bool dispatch_csv_load(const std::string& path, std::string& err);
```

- [ ] **Step 2: Create `import_dialog.cpp`**

```cpp
#include "surface_screen.h"
#include "csv_importer.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>

// Must be listed in SKIP_UNITY_BUILD_INCLUSION in CMakeLists.txt

namespace fincept::surface {

using namespace theme::colors;

// ── dispatch_csv_load ─────────────────────────────────────────────────────────
// Parses file then calls the correct per-surface loader based on active_chart_.
bool SurfaceScreen::dispatch_csv_load(const std::string& path, std::string& err) {
    auto rows = parse_csv_file(path, err);
    if (rows.empty()) return false;

    switch (active_chart_) {
    case ChartType::Volatility:
        return load_vol_surface(rows, vol_data_, err);
    case ChartType::DeltaSurface:
        return load_greeks_surface(rows, delta_data_, err, "Delta");
    case ChartType::GammaSurface:
        return load_greeks_surface(rows, gamma_data_, err, "Gamma");
    case ChartType::VegaSurface:
        return load_greeks_surface(rows, vega_data_, err, "Vega");
    case ChartType::ThetaSurface:
        return load_greeks_surface(rows, theta_data_, err, "Theta");
    case ChartType::SkewSurface:
        return load_skew_surface(rows, skew_data_, err);
    case ChartType::LocalVolSurface:
        return load_local_vol(rows, local_vol_data_, err);
    case ChartType::YieldCurve:
        return load_yield_curve(rows, yield_data_, err);
    case ChartType::SwaptionVol:
        return load_swaption_vol(rows, swaption_data_, err);
    case ChartType::CapFloorVol:
        return load_capfloor_vol(rows, capfloor_data_, err);
    case ChartType::BondSpread:
        return load_bond_spread(rows, bond_spread_data_, err);
    case ChartType::OISBasis:
        return load_ois_basis(rows, ois_data_, err);
    case ChartType::RealYield:
        return load_real_yield(rows, real_yield_data_, err);
    case ChartType::ForwardRate:
        return load_forward_rate(rows, fwd_rate_data_, err);
    case ChartType::FXVol:
        return load_fx_vol(rows, fx_vol_data_, err);
    case ChartType::FXForwardPoints:
        return load_fx_forward(rows, fx_fwd_data_, err);
    case ChartType::CrossCurrencyBasis:
        return load_xccy_basis(rows, xccy_data_, err);
    case ChartType::CDSSpread:
        return load_cds_spread(rows, cds_data_, err);
    case ChartType::CreditTransition:
        return load_credit_trans(rows, credit_trans_data_, err);
    case ChartType::RecoveryRate:
        return load_recovery_rate(rows, recovery_data_, err);
    case ChartType::CommodityForward:
        return load_cmdty_forward(rows, cmdty_fwd_data_, err);
    case ChartType::CommodityVol:
        return load_cmdty_vol(rows, cmdty_vol_data_, err);
    case ChartType::CrackSpread:
        return load_crack_spread(rows, crack_data_, err);
    case ChartType::ContangoBackwardation:
        return load_contango(rows, contango_data_, err);
    case ChartType::Correlation:
        return load_correlation(rows, corr_data_, err);
    case ChartType::PCA:
        return load_pca(rows, pca_data_, err);
    case ChartType::VaR:
        return load_var(rows, var_data_, err);
    case ChartType::StressTestPnL:
        return load_stress_test(rows, stress_data_, err);
    case ChartType::FactorExposure:
        return load_factor_exposure(rows, factor_data_, err);
    case ChartType::LiquidityHeatmap:
        return load_liquidity(rows, liquidity_data_, err);
    case ChartType::Drawdown:
        return load_drawdown(rows, drawdown_data_, err);
    case ChartType::BetaSurface:
        return load_beta(rows, beta_data_, err);
    case ChartType::ImpliedDividend:
        return load_implied_div(rows, impl_div_data_, err);
    case ChartType::InflationExpectations:
        return load_inflation(rows, inflation_data_, err);
    case ChartType::MonetaryPolicyPath:
        return load_monetary(rows, monetary_data_, err);
    }
    err = "Unknown surface type.";
    return false;
}

// ── render_csv_import_dialog ──────────────────────────────────────────────────
void SurfaceScreen::render_csv_import_dialog() {
    if (show_import_dialog_) {
        ImGui::OpenPopup("##sa_import_modal");
        show_import_dialog_ = false;
        import_error_.clear();
        import_success_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(580, 420), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg,    BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0,0,0,0.6f));

    if (!ImGui::BeginPopupModal("##sa_import_modal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::PopStyleColor(2);
        return;
    }

    const auto categories = get_surface_categories();
    const ImVec4& cat_col = CAT_COLORS[active_category_];
    const CsvSchema& schema = get_csv_schema(active_chart_);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Header bar ────────────────────────────────────────────────────────────
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p0, ImVec2(p0.x + 580, p0.y + 38),
            ImGui::GetColorU32(ImVec4(cat_col.x*0.15f, cat_col.y*0.15f, cat_col.z*0.15f, 1.0f)));
        dl->AddLine(ImVec2(p0.x, p0.y + 38), ImVec2(p0.x + 580, p0.y + 38),
            ImGui::GetColorU32(ImVec4(cat_col.x, cat_col.y, cat_col.z, 0.5f)));
        ImGui::SetCursorPosX(12); ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        ImGui::TextColored(cat_col, "IMPORT CSV");
        ImGui::SameLine(0, 10);
        ImGui::TextColored(TEXT_SECONDARY, "%s  /  %s", categories[active_category_].name, schema.surface_name);
        ImGui::Dummy(ImVec2(0, 8));
    }

    ImGui::SetCursorPosX(12);

    // ── Schema reference table ────────────────────────────────────────────────
    ImGui::TextColored(TEXT_DIM, "REQUIRED COLUMNS");
    ImGui::Spacing();

    if (ImGui::BeginTable("##schema_tbl", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(556, 0))) {
        ImGui::TableSetupColumn("COLUMN NAME",  ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("DESCRIPTION",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& col : schema.columns) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(cat_col, "%s", col.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(TEXT_SECONDARY, "%s", col.description);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();

    // ── Example row ──────────────────────────────────────────────────────────
    ImGui::TextColored(TEXT_DIM, "EXAMPLE ROW");
    ImGui::SameLine(0, 8);
    // Build header line from schema
    std::string header_line;
    for (int i = 0; i < (int)schema.columns.size(); i++) {
        if (i > 0) header_line += ", ";
        header_line += schema.columns[i].name;
    }
    ImGui::TextColored(TEXT_DISABLED, "%s", header_line.c_str());
    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "          DATA");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(MARKET_GREEN, "%s", schema.example_row);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Path input ───────────────────────────────────────────────────────────
    ImGui::TextColored(TEXT_DIM, "CSV FILE PATH");
    ImGui::SetNextItemWidth(556);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    bool enter = ImGui::InputText("##import_path", import_path_buf_, sizeof(import_path_buf_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Status message ────────────────────────────────────────────────────────
    if (import_success_) {
        ImGui::TextColored(MARKET_GREEN, "  Data loaded successfully.");
    } else if (!import_error_.empty()) {
        ImGui::TextColored(MARKET_RED, "  Error: %s", import_error_.c_str());
    } else {
        ImGui::TextColored(TEXT_DISABLED, "  First row must be the header. Blank lines and #comments are ignored.");
    }

    ImGui::Spacing();

    // ── Action buttons ────────────────────────────────────────────────────────
    bool do_load = enter;

    ImGui::SetCursorPosX(12);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(cat_col.x*0.3f, cat_col.y*0.3f, cat_col.z*0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(cat_col.x*0.5f, cat_col.y*0.5f, cat_col.z*0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, cat_col);
    if (ImGui::Button("LOAD", ImVec2(100, 28))) do_load = true;
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button,        BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text,          TEXT_SECONDARY);
    if (ImGui::Button("CANCEL", ImVec2(80, 28))) {
        import_error_.clear();
        import_success_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(3);

    // ── Load action ───────────────────────────────────────────────────────────
    if (do_load && std::strlen(import_path_buf_) > 0) {
        import_error_.clear();
        import_success_ = false;
        std::string path(import_path_buf_);
        if (dispatch_csv_load(path, import_error_)) {
            import_success_ = true;
            data_loaded_ = true; // mark loaded so chart renders imported data
        }
    }

    // Auto-close on success after showing the message for one frame
    if (import_success_) {
        ImGui::SameLine(0, 16);
        ImGui::TextColored(TEXT_DIM, "(close to view)");
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter))
            ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleColor(2);
}

} // namespace fincept::surface
```

- [ ] **Step 3: Add `render_csv_import_dialog()` call at end of `SurfaceScreen::render()` in `surface_screen.cpp`**

In `render()`, just before `frame.end()`:
```cpp
render_csv_import_dialog();
```

---

## Task 3: IMPORT Button in Control Bar

**Files:**
- Modify: `src/screens/surface_analytics/surface_screen.cpp` — `render_control_bar()`

- [ ] **Step 1: Add IMPORT button in the right side of Row 1 (category bar), alongside REFRESH**

In `render_control_bar()`, in the right-side button group after the REFRESH `InvisibleButton` block, add:

```cpp
ImGui::SameLine(0, 4);
{
    float iw = ImGui::CalcTextSize("IMPORT").x + 14.0f;
    ImVec2 ic = ImGui::GetCursorScreenPos();
    ImVec2 imn = ic, imx = ImVec2(ic.x + iw, ic.y + 20.0f);
    bool ihov = ImGui::IsMouseHoveringRect(imn, imx);
    dl->AddRectFilled(imn, imx,
        ImGui::GetColorU32(ihov
            ? ImVec4(cat_accent.x*0.35f, cat_accent.y*0.35f, cat_accent.z*0.35f, 1.0f)
            : BG_WIDGET));
    dl->AddRect(imn, imx,
        ImGui::GetColorU32(ImVec4(cat_accent.x, cat_accent.y, cat_accent.z, 0.4f)));
    dl->AddText(ImVec2(imn.x + 7, imn.y + (20.0f - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(cat_accent), "IMPORT");
    ImGui::InvisibleButton("##import", ImVec2(iw, 20.0f));
    if (ImGui::IsItemClicked()) show_import_dialog_ = true;
}
```

---

## Task 4: CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new source files to SOURCES list**

In the `set(SOURCES ...)` block, alongside the other surface analytics files, add:
```cmake
src/screens/surface_analytics/csv_importer.cpp
src/screens/surface_analytics/import_dialog.cpp
```

- [ ] **Step 2: Add to SKIP_UNITY_BUILD_INCLUSION**

In the `set_source_files_properties(... SKIP_UNITY_BUILD_INCLUSION TRUE)` block, add:
```cmake
${CMAKE_CURRENT_SOURCE_DIR}/src/screens/surface_analytics/csv_importer.cpp
${CMAKE_CURRENT_SOURCE_DIR}/src/screens/surface_analytics/import_dialog.cpp
```

- [ ] **Step 3: Build and verify 0 errors**

```bash
cd fincept-cpp
cmake --build build --config Release 2>&1 | tail -5
```
Expected: `FinceptTerminal.exe` with 0 errors.

---

## Task 5: Smoke Test

- [ ] Run the terminal, navigate to Surface Analytics
- [ ] Click IMPORT button — verify modal opens showing correct columns for active surface
- [ ] Switch surface, re-open IMPORT — verify columns update for new surface
- [ ] Enter a non-existent path — verify error message shown
- [ ] Create a minimal CSV for VOL SURFACE (`strike,dte,iv_pct` header + a few rows) and import — verify 3D surface updates
- [ ] Verify REFRESH reloads demo data (overwriting imported data)
- [ ] Verify TABLE view works after import

---

## Notes

- `csv_importer.cpp` and `import_dialog.cpp` **must** be in `SKIP_UNITY_BUILD_INCLUSION` — they use `<fstream>`, `<map>`, and `using namespace theme::colors` which conflict in unity builds
- Imported data replaces demo data in the same struct — REFRESH always restores demo data
- The modal is per-surface: switching active surface changes which loader fires and which schema is shown
- No new dependencies required — uses only `<fstream>`, `<sstream>`, `<map>`, `<algorithm>` from the standard library
