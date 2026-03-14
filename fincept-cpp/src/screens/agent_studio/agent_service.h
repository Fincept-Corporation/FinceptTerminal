#pragma once
// Agent Service — Unified agent operations dispatcher
// Mirrors Tauri's agentService.ts — heavy work goes to Python subprocess or direct HTTP

#include "agent_types.h"
#include <future>
#include <atomic>

namespace fincept::agent_studio {

class AgentService {
public:
    static AgentService& instance();

    // ── Single Agent Execution ──
    std::future<AgentResult> run_agent(const AgentConfig& config,
                                        const std::string& query,
                                        const json& context = {});

    // ── Query Routing (SuperAgent) ──
    std::future<RoutingResult> route_query(const std::string& query,
                                            const std::string& provider,
                                            const std::string& model,
                                            const std::string& api_key);

    // ── Routed Execution ──
    std::future<AgentResult> execute_routed(const std::string& query,
                                             const std::string& provider,
                                             const std::string& model,
                                             const std::string& api_key);

    // ── Team Execution ──
    std::future<AgentResult> run_team(const TeamConfig& team,
                                       const std::string& query,
                                       const std::string& api_key);

    // ── Plan Operations ──
    std::future<ExecutionPlan> generate_plan(const std::string& query,
                                              const AgentConfig& config);
    std::future<ExecutionPlan> execute_plan(ExecutionPlan plan,
                                             const AgentConfig& config);

    // ── Direct HTTP to LLM (no Python needed) ──
    std::future<AgentResult> call_llm_direct(const std::string& provider,
                                               const std::string& model,
                                               const std::string& api_key,
                                               const std::string& system_prompt,
                                               const std::string& user_message,
                                               double temperature = 0.7,
                                               int max_tokens = 4096);

    // ── Cancellation ──
    void cancel() { cancel_flag_ = true; }
    bool is_cancelled() const { return cancel_flag_; }
    void reset_cancel() { cancel_flag_ = false; }

private:
    AgentService() = default;
    std::atomic<bool> cancel_flag_{false};

    // Build the JSON payload for Python dispatch
    json build_payload(const std::string& action, const AgentConfig& config,
                       const std::string& query, const json& extra = {});

    // Call Python finagent_core/main.py with a JSON payload via stdin
    AgentResult call_python_agent(const json& payload);

    // Parse Python stdout into AgentResult
    AgentResult parse_python_result(const std::string& output, double elapsed_ms);

    // Get API key for a provider from DB
    std::string get_api_key(const std::string& provider);

    // HTTP-based LLM calls
    AgentResult call_openai_compatible(const std::string& base_url,
                                        const std::string& api_key,
                                        const std::string& model,
                                        const std::string& system_prompt,
                                        const std::string& user_message,
                                        double temperature, int max_tokens);

    AgentResult call_anthropic(const std::string& api_key,
                                const std::string& model,
                                const std::string& system_prompt,
                                const std::string& user_message,
                                double temperature, int max_tokens);
};

} // namespace fincept::agent_studio
