// AgentsTools.cpp — Tools that drive the Agent Studio (Agent Config) screen.
//
// 8-panel screen → ~37 tools in category "agents":
//   • discovery / introspection (sync + signal-bridged)
//   • agent execution (run / structured / route / multi-query)
//   • financial workflows (stock analysis, portfolio rebalance/risk)
//   • teams + workflows execution
//   • planner CRUD + execute
//   • memory + knowledge
//   • config CRUD (AgentConfigRepository — sync)
//   • workflow CRUD (WorkflowRepository — sync)
//   • cache ops (sync)
//
// AgentService methods that return a request_id and emit a `*_result` signal
// later are wrapped with a per-request signal filter so concurrent callers
// don't cross results.
//
// The tool definitions are partitioned across three sibling TUs:
//   - AgentsTools_Discovery.cpp  — list / discover / introspection / cache
//   - AgentsTools_Execution.cpp  — run_agent + routing + workflows + streaming
//   - AgentsTools_Repos.cpp      — planner + memory + config/workflow CRUD +
//                                  sessions + paper trading + decisions
// Shared helpers (JSON converters, dispatch_agent_run) live in
// AgentsTools_internal.h.

#include "mcp/tools/AgentsTools.h"
#include "mcp/tools/AgentsTools_internal.h"

#include "core/logging/Logger.h"

#include <QString>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_agents_tools() {
    std::vector<ToolDef> tools;
    agents_internal::register_discovery_tools(tools);
    agents_internal::register_execution_tools(tools);
    agents_internal::register_repos_tools(tools);
    LOG_INFO(agents_internal::TAG, QString("Defined %1 agents tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
