#pragma once
// CoreToolsSelfTest.h — headless self-test of the UI engine behind the core
// tools (`--selftest-ui-tools`).
//
// Builds a real widget form (fields with labels, a dropdown, a checkbox,
// buttons, tabs, a table, a password field, a button that opens a modal
// message box, a "Delete everything" button) and drives it through the engine
// exactly as an agent would: snapshot, type, select, check, click, set_value,
// row clicks, waits, reads and a screenshot. It also proves the safety rules:
// secret fields are neither read nor typed, a risky click goes through the
// approval dialog (denied here) and does nothing, a modal dialog blocks clicks
// behind it, and a click that opens a modal never hangs the call.
//
// Needs a QApplication (offscreen platform is fine); no network, no Python.

namespace fincept::mcp::core {

/// Returns 0 when every check passed.
int run_core_tools_selftest();

} // namespace fincept::mcp::core
