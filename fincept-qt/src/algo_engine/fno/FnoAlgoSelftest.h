#pragma once

namespace fincept::algo::fno {

// Headless self-test for the F&O algo foundation. Covers: leg JSON round-trip,
// v046 schema presence, strategy persistence round-trip, and FnoLegResolver
// strike resolution.
// Run with: QT_QPA_PLATFORM=offscreen ./FinceptTerminal --selftest-fno-algo
// Returns 0 if all checks pass, 1 if any check fails.
int run_fno_algo_selftest();

} // namespace fincept::algo::fno
