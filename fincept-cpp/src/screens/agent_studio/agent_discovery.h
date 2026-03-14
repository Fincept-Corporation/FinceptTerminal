#pragma once
// Agent Discovery — Scans built-in agent definitions + custom DB
// Caches results with 10-minute TTL. Mirrors agentService.discoverAgents()

#include "agent_types.h"
#include <mutex>

namespace fincept::agent_studio {

class AgentDiscovery {
public:
    static AgentDiscovery& instance();

    // Get all agents (cached, refreshes if stale)
    std::vector<AgentCard> get_all_agents();

    // Force refresh
    void refresh();

    // Get agents filtered by category
    std::vector<AgentCard> get_by_category(const std::string& category);

    // Search agents by name/description
    std::vector<AgentCard> search(const std::string& query);

    // Get unique categories present in discovered agents
    std::vector<std::string> get_categories();

    // Total count
    int total_count() const;
    int custom_count() const;

private:
    AgentDiscovery() = default;

    // Built-in agent definitions (hardcoded, matching finagent_core/configs/)
    std::vector<AgentCard> get_builtin_agents();

    // Load custom agents from SQLite
    std::vector<AgentCard> load_custom_agents();

    std::mutex mutex_;
    std::vector<AgentCard> cache_;
    double last_refresh_ = 0;
    static constexpr double CACHE_TTL = 600.0; // 10 minutes
};

} // namespace fincept::agent_studio
