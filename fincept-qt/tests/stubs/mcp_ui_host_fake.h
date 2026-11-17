#pragma once
// mcp_ui_host_fake.h — UiHost.h and CoreApproval.h for the UI engine's unit
// tests (tests/tst_mcp_ui_engine.cpp): ordinary widgets play the terminal.
//
//   * a "frame" (terminal window) is a top-level widget with the dynamic
//     property  fincept_test_frame = true;
//   * a "panel" is a widget with  fincept_test_panel = "<screen id>"  (its
//     window title is the panel's title; it is its own content and screen);
//   * the approval dialog is replaced by a scripted verdict, and every request
//     is recorded.

#include "mcp/server/ToolApprovalDialog.h"

#include <QList>

namespace fincept::mcp::test_host {

/// What ask_user_approval() answers next (default Denied), and whether it
/// answers at all (false: never — the call must then time out on its own).
void set_verdict(ToolApprovalVerdict verdict, bool answer = true);
/// Every request asked so far.
QList<ToolApprovalRequest>& approvals();
/// The terminal's PIN lock.
void set_locked(bool locked);

} // namespace fincept::mcp::test_host
