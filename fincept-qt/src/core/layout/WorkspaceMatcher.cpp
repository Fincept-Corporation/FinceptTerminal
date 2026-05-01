#include "core/layout/WorkspaceMatcher.h"

#include "core/logging/Logger.h"

#include <QString>
#include <cstdlib>

namespace fincept::layout {

int WorkspaceMatcher::monitor_count(const MonitorTopologyKey& key) {
    if (key.value.isEmpty())
        return 0;
    // Topology key format: comma-joined sorted monitor stable ids
    // (per MonitorTopology::current_key). Count = comma count + 1.
    return key.value.count(QChar(',')) + 1;
}

const WorkspaceVariant* WorkspaceMatcher::match(const Workspace& workspace,
                                                const MonitorTopologyKey& current_topology) {
    if (workspace.variants.isEmpty())
        return nullptr;

    // Tier 1: exact match.
    if (const auto* exact = workspace.find_variant_exact(current_topology))
        return exact;

    // Tier 2: nearest by monitor count.
    const int target_count = monitor_count(current_topology);
    const WorkspaceVariant* best = nullptr;
    int best_delta = INT32_MAX;
    for (const auto& v : workspace.variants) {
        const int v_count = monitor_count(v.topology);
        const int delta = std::abs(v_count - target_count);
        // Strictly less so ties resolve to the *first* variant the user
        // saved — list order in the JSON is preserved at load time.
        if (delta < best_delta) {
            best = &v;
            best_delta = delta;
        }
    }
    if (best) {
        LOG_INFO("WorkspaceMatcher",
                 QString("No exact topology match — falling back to '%1' (delta=%2)")
                     .arg(best->topology.value).arg(best_delta));
    }
    return best;
}

} // namespace fincept::layout
