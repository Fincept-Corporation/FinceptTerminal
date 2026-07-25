#pragma once
// Headless selftest for ui::LiveTableModel / ui::LiveTableDelegate.
// Invoked from main.cpp via --selftest-live-table. Returns 0 on success.

namespace fincept::ui {

int run_live_table_selftest();

} // namespace fincept::ui
