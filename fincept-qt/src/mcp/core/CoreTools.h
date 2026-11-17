#pragma once
// CoreTools.h — the terminal's complete internal MCP tool surface.
//
// Ten tools replace the ~900 per-screen tools the terminal used to register.
// Instead of one dedicated tool per screen feature, an agent operates the
// terminal the way a person does — it looks at what is open, opens screens,
// reads what they show and works their controls — plus one tool for the
// bundled Python analytics scripts:
//
//   ui_overview    what is open: windows, panels (with refs), dialogs, sign-in/lock, busy work
//   ui_open        open any screen (optionally on a symbol); no screen = the screen list
//   ui_snapshot    a panel / dialog / window as one element per line, each control with a ref;
//                  since="sN" answers only what changed since snapshot sN
//   ui_act         click, type, select, check, press keys, scroll, table-cell edits … and batches
//                  that also wait and read (one call: set fields, run, wait, read the result)
//   ui_read        an element's data in full: table/list rows (text, or typed json), text, options,
//                  chart series, web text
//   ui_wait        wait for text, a state, a dialog, idle (loading done) or a pause
//   ui_screenshot  a picture of the window / panel / element, optionally with ref labels
//   ui_workspace   arrange panels and windows; save / load / delete layouts
//   ui_invoke      run the terminal's own named commands (command bar, menus)
//   python         search, describe and run the bundled analytics scripts (background jobs)
//   report         Report Builder reports: list / outline (read); append blocks, edit a
//                  block's text, refresh data, export (each asks the user first) — ReportTool.h
//
// Every caller — the in-app AI chat, the Python agent bridge and external
// agents over McpHttpServer (Claude Code, Codex, Gemini CLI, Cursor …) — sees
// the same eleven tools; the ui_* part is ~8.5 KB (budgeted by
// tests/tst_mcp_ui_catalog.cpp), so no tool search / discovery round-trip is
// needed. Schemas and descriptions of the ui_* tools: CoreToolCatalog.cpp.
// Every failure answer starts with an error code (UiErrors.h).
//
// Safety is per action, not per tool: ui_act / ui_invoke / python classify
// each step at run time and ask the user (ToolApprovalDialog) before anything
// that places or cancels orders, moves money, spends credits, deletes data or
// runs a sensitive script. Secret fields (password-echo inputs and widgets
// tagged `fincept_secret`) are never read or typed.
//
// Threading: every ui_* tool does its widget work on the GUI thread (posted
// there from the calling worker) and never blocks it: actions are dispatched
// asynchronously so a modal dialog opened by a click cannot hang the call, and
// waits are timer-driven.

#include "mcp/McpTypes.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace fincept::mcp::core {

// Tool categories. Policy code keys off these, never off tool names:
//   ui-read     observes only (overview, snapshot, read, wait, screenshot)
//   ui-control  changes what a screen holds (act, invoke)
//   navigation  changes which screens are open / how they are arranged (open, workspace)
//   analytics   runs bundled Python scripts (python)
inline constexpr const char* kCategoryUiRead = "ui-read";
inline constexpr const char* kCategoryUiControl = "ui-control";
inline constexpr const char* kCategoryNavigation = "navigation";
inline constexpr const char* kCategoryAnalytics = "analytics";

/// The core tools (the ten above plus `report`), ready for McpProvider::register_tools().
std::vector<ToolDef> get_core_tools();

/// Names of the core tools, in catalogue order.
const QStringList& core_tool_names();

} // namespace fincept::mcp::core
