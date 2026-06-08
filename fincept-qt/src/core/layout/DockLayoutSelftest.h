#pragma once

namespace fincept::layout {

// Headless reproduction harness for the dock-layout persistence bug.
//
// Reproduces the "tabbed panels come back split, plus phantom closed areas
// accumulate" corruption deterministically without a GUI, using raw ADS
// against the real save/restore code paths. Run with:
//
//   QT_QPA_PLATFORM=offscreen ./FinceptTerminal --selftest-dock-layout
//
// Exits 0 if every arrangement round-trips faithfully, 1 if any case fails.
int run_dock_layout_selftest();

} // namespace fincept::layout
