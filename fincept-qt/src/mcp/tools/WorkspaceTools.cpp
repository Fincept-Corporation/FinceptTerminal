// WorkspaceTools.cpp — Tools that drive the multi-window / layout / panel /
// monitor / symbol-link / action subsystems.
//
// 58 tools in category "workspace", grouped:
//   • Monitors  (3)     — QGuiApplication::screens() + monitor topology
//   • Windows   (9)     — WindowRegistry + per-WindowFrame ops
//   • Panels    (14)    — PanelRegistry + DockScreenRouter (incl. dash-add-news)
//   • Layouts   (13)    — LayoutCatalog + WorkspaceShell + LayoutTemplates
//   • Snapshots (6-8)   — WorkspaceSnapshotRing + CrashRecovery
//   • SymbolGrp (9)     — SymbolGroupRegistry + SymbolContext (slot-aware)
//   • Actions   (3)     — ActionRegistry
//   • CmdBar    (1)     — try_parse_dock_command-equivalent
//
// All targets (WindowFrame, DockScreenRouter, LayoutCatalog, SymbolContext,
// ActionRegistry, ...) are UI-thread only. Every tool dispatches its body
// onto the qApp thread via AsyncDispatch::callback_to_promise so callers
// from any LLM worker thread are safe.
//
// Tool definitions are partitioned across sibling TUs:
//   - WorkspaceTools_MonitorsWindows.cpp   — monitors + windows
//   - WorkspaceTools_Panels.cpp            — panels
//   - WorkspaceTools_LayoutsSnapshots.cpp  — layouts + snapshots
//   - WorkspaceTools_SymbolsActions.cpp    — symbol groups + actions + cmdbar
// Shared helpers (JSON converters, run_on_ui) live in WorkspaceTools_internal.h.

#include "mcp/tools/WorkspaceTools.h"
#include "mcp/tools/WorkspaceTools_internal.h"

#include "core/logging/Logger.h"

#include <QString>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_workspace_tools() {
    std::vector<ToolDef> tools;
    workspace_internal::register_monitor_window_tools(tools);
    workspace_internal::register_panel_tools(tools);
    workspace_internal::register_layout_snapshot_tools(tools);
    workspace_internal::register_symbol_action_tools(tools);
    LOG_INFO(workspace_internal::TAG, QString("Defined %1 workspace tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
