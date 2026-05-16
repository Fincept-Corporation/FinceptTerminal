// src/services/workflow/nodes/UtilityNodes.cpp
//
// Workflow utility-node registry. Sub-registrations are partitioned across:
//   - UtilityNodes_Tier1Core.cpp   — DateTime, Filter, Map, Aggregate, Sort,
//                                    Join, GroupBy, Deduplicate, Limit
//   - UtilityNodes_Tier1Extra.cpp  — Item Lists, RSS, Crypto/Hash, DB,
//                                    Execute Workflow, Market Event, Reshape
//   - UtilityNodes_Tier2.cpp       — Tier-2 data + utility additions
#include "services/workflow/nodes/UtilityNodes.h"

namespace fincept::workflow {

void register_utility_tier1_core(NodeRegistry& registry);
void register_utility_tier1_extra(NodeRegistry& registry);
void register_utility_tier2(NodeRegistry& registry);

void register_utility_nodes(NodeRegistry& registry) {
    register_utility_tier1_core(registry);
    register_utility_tier1_extra(registry);
    register_utility_tier2(registry);
}

} // namespace fincept::workflow
