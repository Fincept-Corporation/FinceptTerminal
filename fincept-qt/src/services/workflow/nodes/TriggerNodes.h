#pragma once

namespace fincept::workflow {
class NodeRegistry;

/// Register trigger node types (ManualTrigger, ScheduleTrigger, PriceAlert, etc.)
void register_trigger_nodes(NodeRegistry& registry);

} // namespace fincept::workflow
