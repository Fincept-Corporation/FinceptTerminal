#pragma once
// Surface Analytics — capability matrix.
// Source of truth mapping every ChartType to: Databento support tier,
// dataset/schema/symbology lineage, and the set of input controls the surface needs.
//
// The control panel reads this to decide which sections to render. The data
// inspector reads it to display lineage. The screen reads it to dispatch the
// right DatabentoService method. No hardcoded knowledge anywhere else.

#include "SurfaceTypes.h"

#include <array>

namespace fincept::surface {

enum class SurfaceTier {
    LIVE,     // Direct Databento fetch
    COMPUTED, // Python-derived from a Databento input
    EQUITIES, // Equities OHLCV-driven
    DEMO,     // No Databento source — synthetic data
};

inline const char* tier_name(SurfaceTier t) {
    switch (t) {
        case SurfaceTier::LIVE:
            return "LIVE";
        case SurfaceTier::COMPUTED:
            return "COMPUTED";
        case SurfaceTier::EQUITIES:
            return "EQUITIES";
        case SurfaceTier::DEMO:
            return "DEMO";
    }
    return "UNKNOWN";
}

// View modes a surface can be rendered in. The capability declares which apply.
enum class ViewMode { Surface3D, Table, Line };

struct SurfaceCapability {
    ChartType type;
    SurfaceTier tier;
    const char* dataset;     // e.g. "OPRA.PILLAR" — empty for DEMO
    const char* schema;      // e.g. "cbbo-1s" — empty for DEMO
    const char* symbology;   // e.g. "parent" — what stype_in resolves to
    const char* description; // human-readable lineage line
    bool needs_underlying;
    bool needs_date_range;
    bool needs_strike_window;
    bool needs_expiry_filter;
    bool needs_correlation_basket;
    bool supports_3d;
    bool supports_table;
    bool supports_line;
};

// 35 entries — must remain in 1:1 correspondence with ChartType.
inline constexpr std::array<SurfaceCapability, 35> SURFACE_CAPABILITIES = {{
    // ── Equity Derivatives ─────────────────────────────────────────────────
    {ChartType::Volatility, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "OPRA options definitions joined to CBBO snapshot, BS-implied vol",
     true, true, true, true, false, true, true, false},
    {ChartType::DeltaSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Black-Scholes delta from CBBO mid",
     true, true, true, true, false, true, true, false},
    {ChartType::GammaSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Black-Scholes gamma from CBBO mid",
     true, true, true, true, false, true, true, false},
    {ChartType::VegaSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Black-Scholes vega from CBBO mid",
     true, true, true, true, false, true, true, false},
    {ChartType::ThetaSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Black-Scholes theta from CBBO mid",
     true, true, true, true, false, true, true, false},
    {ChartType::SkewSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "IV skew across delta buckets and tenors",
     true, true, true, true, false, true, true, false},
    {ChartType::LocalVolSurface, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Dupire local vol from BS implied vol surface",
     true, true, true, true, false, true, true, false},
    // ── Fixed Income (DEMO) ─────────────────────────────────────────────────
    {ChartType::YieldCurve, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, true},
    {ChartType::SwaptionVol, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::CapFloorVol, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::BondSpread, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::OISBasis, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::RealYield, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::ForwardRate, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    // ── FX (DEMO) ───────────────────────────────────────────────────────────
    {ChartType::FXVol, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::FXForwardPoints, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, true},
    {ChartType::CrossCurrencyBasis, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    // ── Credit (DEMO) ───────────────────────────────────────────────────────
    {ChartType::CDSSpread, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::CreditTransition, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    {ChartType::RecoveryRate, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, false},
    // ── Commodities ─────────────────────────────────────────────────────────
    {ChartType::CommodityForward, SurfaceTier::LIVE, "GLBX.MDP3", "definition+ohlcv-1d", "parent",
     "CME futures definitions and daily settlements",
     true, true, false, false, false, true, true, true},
    {ChartType::CommodityVol, SurfaceTier::COMPUTED, "GLBX.MDP3", "definition+ohlcv-1d", "parent",
     "Daily realised vol from futures settlements",
     true, true, false, false, false, true, true, false},
    {ChartType::CrackSpread, SurfaceTier::COMPUTED, "GLBX.MDP3", "ohlcv-1d", "parent",
     "Crack/crush spreads computed from related futures",
     true, true, false, false, false, true, true, true},
    {ChartType::ContangoBackwardation, SurfaceTier::LIVE, "GLBX.MDP3", "definition+ohlcv-1d", "parent",
     "Term-structure shape from daily settlement curve",
     true, true, false, false, false, true, true, true},
    // ── Risk & Portfolio ────────────────────────────────────────────────────
    {ChartType::Correlation, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Pairwise correlation from equities daily returns",
     false, true, false, false, true, true, true, false},
    {ChartType::PCA, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Principal components of basket returns",
     false, true, false, false, true, true, true, false},
    {ChartType::VaR, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Value-at-Risk by confidence and horizon",
     false, true, false, false, true, true, true, false},
    {ChartType::StressTestPnL, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Scenario PnL from user-defined shock vectors",
     false, true, false, false, true, true, true, false},
    {ChartType::FactorExposure, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Factor loadings via OLS on basket returns",
     false, true, false, false, true, true, true, false},
    {ChartType::LiquidityHeatmap, SurfaceTier::COMPUTED, "OPRA.PILLAR", "cbbo-1s+statistics", "parent",
     "Open interest / volume across strike×expiry",
     true, true, true, true, false, true, true, false},
    {ChartType::Drawdown, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Rolling drawdown across windows",
     false, true, false, false, true, true, true, false},
    {ChartType::BetaSurface, SurfaceTier::EQUITIES, "EQUS.MINI", "ohlcv-1d", "raw_symbol",
     "Rolling beta vs SPY across horizons",
     false, true, false, false, true, true, true, false},
    {ChartType::ImpliedDividend, SurfaceTier::COMPUTED, "OPRA.PILLAR", "definition+cbbo-1s", "parent",
     "Implied dividend yield from put-call parity",
     true, true, true, true, false, true, true, false},
    // ── Macro (DEMO) ────────────────────────────────────────────────────────
    {ChartType::InflationExpectations, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, true},
    {ChartType::MonetaryPolicyPath, SurfaceTier::DEMO, "", "", "",
     "No Databento source — synthetic data",
     false, false, false, false, false, true, true, true},
}};

inline const SurfaceCapability& capability_for(ChartType t) {
    for (const auto& cap : SURFACE_CAPABILITIES) {
        if (cap.type == t)
            return cap;
    }
    // Cannot happen — every ChartType is in the table. Fall back to first entry.
    return SURFACE_CAPABILITIES[0];
}

inline bool is_live(ChartType t) {
    auto tier = capability_for(t).tier;
    return tier != SurfaceTier::DEMO;
}

} // namespace fincept::surface
