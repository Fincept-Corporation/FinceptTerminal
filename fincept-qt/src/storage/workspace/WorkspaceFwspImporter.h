#pragma once
// One-shot migration of legacy v3 workspace files (`*.fwsp`) into the new
// LayoutCatalog. Imports only the `metadata.name` field — the actual frame /
// panel / dock state cannot be ported because the legacy file format predates
// the multi-window FrameLayout/PanelState model. Users see their old layout
// names in the Launchpad list with a "(Imported — please re-save)" hint and
// can re-save under the same name once the desired layout is set up.
//
// Idempotent via `LayoutCatalog::meta("fwsp_import_done") == "1"` flag.
// Original .fwsp files are left untouched (read-only).

namespace fincept {

class WorkspaceFwspImporter {
  public:
    /// Scan `AppPaths::workspaces()` for `*.fwsp` files. For each, create a
    /// kind="user" Workspace with the original name + an explanatory
    /// description. No-op after the first successful run.
    /// Safe to call from TerminalShell::initialise after LayoutCatalog::open.
    static void run_once_if_needed();

  private:
    WorkspaceFwspImporter() = delete;
};

} // namespace fincept
