#pragma once
// Agent Studio — Data types mirroring Tauri agentServiceTypes.ts
// All agent-related structs and enums in one place

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace fincept::agent_studio {

using json = nlohmann::json;

// ============================================================================
// View modes — maps to Tauri ViewMode type
// ============================================================================
enum class ViewMode {
    Agents,
    Teams,
    Chat,
    Planner,
    Workflows,
    Tools,
    System,
    Create
};

inline const char* view_mode_label(ViewMode m) {
    switch (m) {
        case ViewMode::Agents:    return "AGENTS";
        case ViewMode::Teams:     return "TEAMS";
        case ViewMode::Chat:      return "CHAT";
        case ViewMode::Planner:   return "PLANNER";
        case ViewMode::Workflows: return "WORKFLOWS";
        case ViewMode::Tools:     return "TOOLS";
        case ViewMode::System:    return "SYSTEM";
        case ViewMode::Create:    return "CREATE";
    }
    return "AGENTS";
}

// ============================================================================
// Agent card — metadata for a discovered or custom agent
// ============================================================================
struct AgentCard {
    std::string id;
    std::string name;
    std::string description;
    std::string category;       // "trader", "hedge-fund", "economic", "geopolitics", "custom"
    std::string version = "1.0";
    std::string provider;       // LLM provider (openai, anthropic, etc.)
    std::vector<std::string> capabilities;
    json config;                // Full agent configuration JSON
    bool user_created = false;  // Custom agent vs discovered
};

// ============================================================================
// Agent categories (for grouping in the agent list)
// ============================================================================
struct AgentCategory {
    std::string id;
    std::string name;
    const char* icon;     // Text icon for category badge
    ImU32 color;          // IM_COL32 color
};

inline std::vector<AgentCategory> builtin_categories() {
    return {
        {"all",         "All Agents",     "*", IM_COL32(255, 255, 255, 255)},
        {"trader",      "Trader",         "T", IM_COL32(34, 197, 94, 255)},
        {"hedge-fund",  "Hedge Fund",     "H", IM_COL32(99, 102, 241, 255)},
        {"economic",    "Economic",       "E", IM_COL32(234, 179, 8, 255)},
        {"geopolitics", "Geopolitics",    "G", IM_COL32(239, 68, 68, 255)},
        {"analysis",    "Analysis",       "A", IM_COL32(59, 130, 246, 255)},
        {"custom",      "Custom",         "C", IM_COL32(168, 85, 247, 255)},
    };
}

// ============================================================================
// Agent config — editable configuration for running an agent
// ============================================================================
struct AgentConfig {
    // Model selection
    std::string provider = "fincept";
    std::string model_id = "fincept-llm";
    double temperature = 0.7;
    int max_tokens = 4096;

    // System prompt / instructions
    std::string instructions;

    // Tools
    std::vector<std::string> tools;

    // Feature toggles
    bool enable_memory = false;
    bool enable_reasoning = false;
    bool enable_knowledge = false;
    bool enable_guardrails = false;
    bool enable_agentic_memory = false;
    bool enable_storage = false;
    bool enable_tracing = false;
    bool enable_compression = false;

    // Guardrails config
    bool guardrails_pii = true;
    bool guardrails_injection = true;
    bool guardrails_compliance = false;

    // Serialize to JSON for Python dispatch
    json to_json() const {
        json j = {
            {"model", {{"provider", provider}, {"model_id", model_id}}},
            {"temperature", temperature},
            {"max_tokens", max_tokens},
            {"instructions", instructions},
            {"tools", tools},
            {"memory", enable_memory},
            {"reasoning", enable_reasoning},
            {"knowledge", enable_knowledge},
            {"agentic_memory", enable_agentic_memory},
            {"tracing", enable_tracing},
            {"compression", enable_compression},
        };
        if (enable_guardrails) {
            j["guardrails"] = {
                {"enabled", true},
                {"pii_detection", guardrails_pii},
                {"injection_check", guardrails_injection},
                {"financial_compliance", guardrails_compliance}
            };
        } else {
            j["guardrails"] = false;
        }
        if (enable_storage) {
            j["storage"] = {{"enabled", true}, {"type", "sqlite"}};
        }
        return j;
    }

    static AgentConfig from_json(const json& j) {
        AgentConfig c;
        if (j.contains("model") && j["model"].is_object()) {
            c.provider = j["model"].value("provider", "fincept");
            c.model_id = j["model"].value("model_id", "fincept-llm");
        }
        c.temperature = j.value("temperature", 0.7);
        c.max_tokens = j.value("max_tokens", 4096);
        c.instructions = j.value("instructions", "");
        if (j.contains("tools") && j["tools"].is_array()) {
            for (auto& t : j["tools"]) c.tools.push_back(t.get<std::string>());
        }
        c.enable_memory = j.value("memory", false);
        c.enable_reasoning = j.value("reasoning", false);
        c.enable_knowledge = j.value("knowledge", false);
        if (j.contains("guardrails")) {
            if (j["guardrails"].is_boolean()) {
                c.enable_guardrails = j["guardrails"].get<bool>();
            } else if (j["guardrails"].is_object()) {
                c.enable_guardrails = j["guardrails"].value("enabled", false);
                c.guardrails_pii = j["guardrails"].value("pii_detection", true);
                c.guardrails_injection = j["guardrails"].value("injection_check", true);
                c.guardrails_compliance = j["guardrails"].value("financial_compliance", false);
            }
        }
        c.enable_agentic_memory = j.value("agentic_memory", false);
        c.enable_tracing = j.value("tracing", false);
        c.enable_compression = j.value("compression", false);
        if (j.contains("storage") && j["storage"].is_object()) {
            c.enable_storage = j["storage"].value("enabled", false);
        }
        return c;
    }
};

// ============================================================================
// Routing result — from SuperAgent query classification
// ============================================================================
struct RoutingResult {
    bool success = false;
    std::string intent;          // TRADING, PORTFOLIO, ANALYSIS, etc.
    std::string agent_id;
    double confidence = 0.0;
    std::vector<std::string> matched_keywords;
    json config;
};

// ============================================================================
// Chat message
// ============================================================================
struct AgentChatMessage {
    enum class Role { User, Assistant, System };

    Role role = Role::User;
    std::string content;
    std::string timestamp;
    std::string agent_name;
    std::optional<RoutingResult> routing_info;
};

// ============================================================================
// Team member
// ============================================================================
struct TeamMember {
    std::string name;
    std::string role;
    std::string provider;
    std::string model_id;
    std::vector<std::string> tools;
    std::string instructions;
};

// ============================================================================
// Team configuration
// ============================================================================
enum class TeamMode { Coordinate, Route, Collaborate };

inline const char* team_mode_label(TeamMode m) {
    switch (m) {
        case TeamMode::Coordinate:  return "DELEGATE";
        case TeamMode::Route:       return "ROUTE";
        case TeamMode::Collaborate: return "COLLABORATE";
    }
    return "DELEGATE";
}

struct TeamConfig {
    std::string name = "Analysis Team";
    TeamMode mode = TeamMode::Coordinate;
    std::string coordinator_provider;
    std::string coordinator_model;
    std::vector<TeamMember> members;
    int leader_index = 0;
    bool show_member_responses = true;

    json to_json() const {
        json members_j = json::array();
        for (auto& m : members) {
            members_j.push_back({
                {"name", m.name}, {"role", m.role},
                {"model", {{"provider", m.provider}, {"model_id", m.model_id}}},
                {"tools", m.tools}, {"instructions", m.instructions}
            });
        }
        std::string mode_str = "coordinate";
        if (mode == TeamMode::Route) mode_str = "route";
        else if (mode == TeamMode::Collaborate) mode_str = "collaborate";

        return {
            {"name", name}, {"mode", mode_str},
            {"model", {{"provider", coordinator_provider}, {"model_id", coordinator_model}}},
            {"members", members_j},
            {"leader_index", leader_index},
            {"show_members_responses", show_member_responses}
        };
    }
};

// ============================================================================
// Execution plan (for Planner view)
// ============================================================================
struct PlanStep {
    std::string id;
    std::string name;
    std::string step_type;         // "agent", "data", "transform", etc.
    json config;
    std::vector<std::string> dependencies;
    std::string status = "pending"; // pending, running, completed, failed
    std::string result;
    std::string error;
};

struct ExecutionPlan {
    std::string id;
    std::string name;
    std::string description;
    std::vector<PlanStep> steps;
    json context;
    std::string status = "draft";   // draft, running, completed, failed
    bool is_complete = false;
    bool has_failed = false;
};

// ============================================================================
// Tool definition (for Tools view)
// ============================================================================
struct ToolDef {
    std::string name;
    std::string description;
    std::string category;        // finance, trading, portfolio, etc.
    bool enabled = true;
    bool blocked = false;        // Dangerous tools are blocked from agent access
};

// ============================================================================
// Agent execution result
// ============================================================================
struct AgentResult {
    bool success = false;
    std::string response;
    std::string raw_output;
    std::string error;
    double execution_time_ms = 0;
    std::string model_used;
    std::string provider_used;
    int tokens_used = 0;
};

// ============================================================================
// Plan templates
// ============================================================================
struct PlanTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    std::vector<PlanStep> steps;
};

inline std::vector<PlanTemplate> builtin_plan_templates() {
    return {
        {"stock_analysis",   "Stock Analysis",        "Research and analyze a stock",        "research",  {}},
        {"portfolio_rebal",  "Portfolio Rebalancing",  "Optimize portfolio allocation",       "portfolio", {}},
        {"risk_assessment",  "Risk Assessment",        "Comprehensive risk analysis",         "risk",      {}},
        {"multi_query",      "Multi-Agent Analysis",   "Route query to specialized agents",   "agent",     {}},
        {"paper_trade",      "Paper Trade",            "Simulated trading workflow",           "trading",   {}},
    };
}

// ============================================================================
// Output model definitions (structured output)
// ============================================================================
struct OutputModel {
    std::string id;
    std::string name;
    std::string description;
};

inline std::vector<OutputModel> builtin_output_models() {
    return {
        {"trade_signal",       "Trade Signal",       "Buy/Sell signals with confidence"},
        {"stock_analysis",     "Stock Analysis",     "Comprehensive stock report"},
        {"portfolio_analysis", "Portfolio Analysis",  "Portfolio metrics & allocation"},
        {"risk_assessment",    "Risk Assessment",    "Risk metrics & warnings"},
        {"market_analysis",    "Market Analysis",    "Market conditions & trends"},
    };
}

// ============================================================================
// Builtin tool definitions
// ============================================================================
inline std::vector<ToolDef> builtin_tools() {
    return {
        // Finance
        {"yfinance",       "Fetch stock data from Yahoo Finance",       "finance",  true, false},
        {"calculator",     "Mathematical calculations",                  "finance",  true, false},
        {"economic_data",  "Fetch economic indicators (FRED, World Bank)", "finance", true, false},

        // Research
        {"web_search",     "Search the web for information",             "research", true, false},
        {"edgar_tools",    "SEC EDGAR filings and company data",         "research", true, false},
        {"duckduckgo",     "Web search via DuckDuckGo",                  "research", true, false},

        // Trading — Read-only
        {"get_balance",    "Check account balance",                      "trading",  true, false},
        {"get_positions",  "List open positions",                        "trading",  true, false},
        {"get_orders",     "List pending orders",                        "trading",  true, false},

        // Trading — Blocked (dangerous)
        {"place_order",    "Place a new order",                          "trading",  false, true},
        {"cancel_order",   "Cancel an existing order",                   "trading",  false, true},
        {"modify_order",   "Modify an existing order",                   "trading",  false, true},
        {"execute_trade",  "Execute a trade immediately",                "trading",  false, true},
        {"place_crypto_order", "Place cryptocurrency order",             "trading",  false, true},
    };
}

} // namespace fincept::agent_studio
