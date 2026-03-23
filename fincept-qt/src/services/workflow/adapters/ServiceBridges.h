#pragma once

namespace fincept::workflow {
class NodeRegistry;

/// Wire market data node execute functions to MarketDataService + PythonRunner.
void wire_market_data_bridges(NodeRegistry& registry);

/// Wire trading node execute functions to UnifiedTrading + PaperTrading.
void wire_trading_bridges(NodeRegistry& registry);

/// Wire agent node execute functions to AgentService.
void wire_agent_bridges(NodeRegistry& registry);

/// Wire all bridges at once.
void wire_all_bridges(NodeRegistry& registry);

} // namespace fincept::workflow
