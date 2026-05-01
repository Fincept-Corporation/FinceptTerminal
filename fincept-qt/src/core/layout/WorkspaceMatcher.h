#pragma once
#include "core/layout/LayoutTypes.h"

namespace fincept::layout {

/// Phase 6: matches a current monitor topology to the best `WorkspaceVariant`
/// inside a Workspace. Implements decision 4.5 (per-topology variants).
///
/// Algorithm:
///   1. Exact topology key match → return that variant.
///   2. Otherwise, return the variant with the closest monitor count
///      (ties broken by recency = list order, latest-defined wins).
///   3. If the workspace has no variants at all, return nullptr — caller
///      falls back to hard-bind via SessionManager (Phase 1 behaviour).
///
/// Pure function — no Qt connections, no state. Easy to unit-test.
class WorkspaceMatcher {
  public:
    /// Walk `workspace.variants` and return the best match, or nullptr
    /// if no variant is suitable. The returned pointer aliases into the
    /// workspace argument; do not store across mutations.
    static const WorkspaceVariant* match(const Workspace& workspace,
                                         const MonitorTopologyKey& current_topology);

    /// How many monitors the topology key represents — counts comma-
    /// separated entries. Used by the matcher's tier-2 nearest-by-count
    /// step but exposed publicly for UI surfaces ("Saved variant for
    /// 3-monitor layout") and tests.
    static int monitor_count(const MonitorTopologyKey& key);
};

} // namespace fincept::layout
