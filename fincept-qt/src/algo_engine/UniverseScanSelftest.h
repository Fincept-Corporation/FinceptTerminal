// src/algo_engine/UniverseScanSelftest.h
#pragma once

namespace fincept::algo {

/// Headless self-test for the universe-scan core logic. Validates required_bars()
/// math and that ConditionEvaluator fires/doesn't-fire on the exact synthetic
/// window the runner builds. No GUI, no threads, no broker, no DB. Returns 0 on
/// pass, 1 on any failure. Run with: --selftest-universe-scan (QT_QPA_PLATFORM=offscreen).
int run_universe_scan_selftest();

} // namespace fincept::algo
