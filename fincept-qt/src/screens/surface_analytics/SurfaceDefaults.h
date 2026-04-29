#pragma once
// Surface Analytics — single home for legitimate constexpr defaults used
// to seed control-panel state on first run / reset. Anything here is a
// fall-back, never an authoritative value baked into business logic.

#include <array>

namespace fincept::surface::defaults {

// Starter equity options underlyings — used to seed the asset search field
// when the user first opens an OPRA-backed surface.
inline constexpr std::array<const char*, 7> EQUITY_UNDERLYINGS = {
    "SPY", "QQQ", "IWM", "AAPL", "TSLA", "NVDA", "AMZN"};

// Default correlation / PCA / risk basket — user can edit and persist their own
// via SurfaceControlPanel state.
inline constexpr std::array<const char*, 8> RISK_BASKET = {
    "SPY", "QQQ", "IWM", "DIA", "GLD", "TLT", "IEF", "HYG"};

// Default commodity roots used for futures-curve surfaces. The control panel
// shows these in the asset combo when the active surface is commodity-backed.
inline constexpr std::array<const char*, 5> COMMODITY_ROOTS = {
    "CL", "NG", "GC", "ZC", "ZS"};

// Default historical lookback (days) for equities OHLCV-driven surfaces.
inline constexpr int LOOKBACK_DAYS = 60;

// Default strike window for OPRA option-surface fetches, % of spot.
inline constexpr int STRIKE_WINDOW_PCT = 20;

// Default DTE filter for option surfaces (days-to-expiry).
inline constexpr int DTE_MIN = 7;
inline constexpr int DTE_MAX = 365;

} // namespace fincept::surface::defaults
