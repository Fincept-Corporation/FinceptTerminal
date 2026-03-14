// Agent Service — Python subprocess dispatch + direct HTTP LLM fallback

#include "agent_service.h"
#include "http/http_client.h"
#include "python/python_runner.h"
#include "storage/database.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::agent_studio {

using json = nlohmann::json;

// Forward declaration
static RoutingResult keyword_fallback_route(const std::string& query);

AgentService& AgentService::instance() {
    static AgentService svc;
    return svc;
}

// ============================================================================
// Payload builder
// ============================================================================
json AgentService::build_payload(const std::string& action, const AgentConfig& config,
                                  const std::string& query, const json& extra) {
    json payload = {
        {"action", action},
        {"query", query},
        {"config", config.to_json()},
        {"api_keys", json::object()}
    };

    // Inject all configured API keys
    auto llm_configs = db::ops::get_llm_configs();
    for (auto& c : llm_configs) {
        if (c.api_key && !c.api_key->empty()) {
            payload["api_keys"][c.provider] = *c.api_key;
        }
    }

    // Merge extra fields
    if (!extra.is_null() && extra.is_object()) {
        for (auto& [k, v] : extra.items()) {
            payload[k] = v;
        }
    }

    return payload;
}

// ============================================================================
// Python agent call
// ============================================================================
AgentResult AgentService::call_python_agent(const json& payload) {
    auto start = std::chrono::steady_clock::now();

    auto py_result = python::execute_with_stdin(
        "agents/finagent_core/main.py", {}, payload.dump());

    auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    if (py_result.success) {
        return parse_python_result(py_result.output, elapsed);
    }

    AgentResult result;
    result.success = false;
    result.error = py_result.error.empty() ? "Python execution failed (exit code "
                   + std::to_string(py_result.exit_code) + ")" : py_result.error;
    result.execution_time_ms = elapsed;
    return result;
}

AgentResult AgentService::parse_python_result(const std::string& output, double elapsed_ms) {
    AgentResult result;
    result.execution_time_ms = elapsed_ms;
    result.raw_output = output;

    try {
        auto j = json::parse(output);

        // The Python agent returns various formats — try common patterns
        if (j.contains("data") && j["data"].is_object() && j["data"].contains("response")) {
            result.response = j["data"]["response"].get<std::string>();
        } else if (j.contains("response") && j["response"].is_string()) {
            result.response = j["response"].get<std::string>();
        } else if (j.contains("result") && j["result"].is_string()) {
            result.response = j["result"].get<std::string>();
        } else {
            // Return the whole JSON as the response
            result.response = j.dump(2);
        }

        result.success = j.value("success", true);
        result.model_used = j.value("model", "");
        result.provider_used = j.value("provider", "");
        result.tokens_used = j.value("tokens_used", 0);

        if (j.contains("error") && !j["error"].is_null()) {
            result.error = j["error"].get<std::string>();
            if (!result.error.empty()) result.success = false;
        }
    } catch (const std::exception& e) {
        // Not JSON — use raw output as response
        result.response = output;
        result.success = !output.empty();
        if (output.empty()) {
            result.error = std::string("Failed to parse agent output: ") + e.what();
        }
    }

    return result;
}

// ============================================================================
// API key lookup
// ============================================================================
std::string AgentService::get_api_key(const std::string& provider) {
    auto configs = db::ops::get_llm_configs();
    for (auto& c : configs) {
        if (c.provider == provider && c.api_key && !c.api_key->empty())
            return *c.api_key;
    }
    return "";
}

// ============================================================================
// Run single agent
// ============================================================================
std::future<AgentResult> AgentService::run_agent(const AgentConfig& config,
                                                   const std::string& query,
                                                   const json& context) {
    return std::async(std::launch::async, [this, config, query, context]() {
        LOG_INFO("AgentService", "Running agent: provider=%s model=%s",
                 config.provider.c_str(), config.model_id.c_str());

        // Try Python first
        auto payload = build_payload("run", config, query, {{"context", context}});
        auto result = call_python_agent(payload);

        // If Python failed, fall back to direct HTTP
        if (!result.success && result.error.find("Python") != std::string::npos) {
            LOG_INFO("AgentService", "Python unavailable, falling back to direct HTTP");
            std::string api_key = get_api_key(config.provider);
            if (api_key.empty()) {
                result.error = "No API key configured for provider: " + config.provider;
                return result;
            }

            if (config.provider == "anthropic") {
                result = call_anthropic(api_key, config.model_id,
                    config.instructions, query, config.temperature, config.max_tokens);
            } else {
                // OpenAI-compatible (openai, groq, ollama, etc.)
                std::string base_url;
                if (config.provider == "openai") base_url = "https://api.openai.com/v1";
                else if (config.provider == "groq") base_url = "https://api.groq.com/openai/v1";
                else if (config.provider == "ollama") base_url = "http://localhost:11434/v1";
                else if (config.provider == "fincept") base_url = "https://api.fincept.in/api/llm";
                else {
                    // Check DB for custom base_url
                    auto cfgs = db::ops::get_llm_configs();
                    for (auto& c : cfgs) {
                        if (c.provider == config.provider && c.base_url) {
                            base_url = *c.base_url;
                            break;
                        }
                    }
                    if (base_url.empty()) base_url = "https://api.openai.com/v1";
                }

                result = call_openai_compatible(base_url, api_key, config.model_id,
                    config.instructions, query, config.temperature, config.max_tokens);
            }
        }

        return result;
    });
}

// ============================================================================
// Route query (SuperAgent)
// ============================================================================
std::future<RoutingResult> AgentService::route_query(const std::string& query,
                                                       const std::string& provider,
                                                       const std::string& model,
                                                       const std::string& api_key) {
    return std::async(std::launch::async, [this, query, provider, model, api_key]() {
        LOG_INFO("AgentService", "Routing query via SuperAgent");

        AgentConfig config;
        config.provider = provider;
        config.model_id = model;

        json extra = {{"api_key", api_key}};
        auto payload = build_payload("route_query", config, query, extra);
        auto py_result = call_python_agent(payload);

        RoutingResult routing;
        if (py_result.success) {
            try {
                auto j = json::parse(py_result.raw_output);
                routing.success = j.value("success", false);
                routing.intent = j.value("intent", "GENERAL");
                routing.agent_id = j.value("agent_id", "");
                routing.confidence = j.value("confidence", 0.0);
                if (j.contains("matched_keywords") && j["matched_keywords"].is_array()) {
                    for (auto& kw : j["matched_keywords"])
                        routing.matched_keywords.push_back(kw.get<std::string>());
                }
                if (j.contains("config")) routing.config = j["config"];
            } catch (...) {
                routing.success = false;
                routing.intent = "GENERAL";
            }
        } else {
            // Keyword fallback routing
            routing = keyword_fallback_route(query);
        }

        return routing;
    });
}

// ============================================================================
// Execute routed query
// ============================================================================
std::future<AgentResult> AgentService::execute_routed(const std::string& query,
                                                        const std::string& provider,
                                                        const std::string& model,
                                                        const std::string& api_key) {
    return std::async(std::launch::async, [this, query, provider, model, api_key]() {
        // Route first
        auto route_future = route_query(query, provider, model, api_key);
        auto routing = route_future.get();

        // Build config from routing result
        AgentConfig config;
        config.provider = provider;
        config.model_id = model;
        if (routing.success && routing.config.contains("instructions")) {
            config.instructions = routing.config.value("instructions", "");
        }

        // Execute with routed config
        auto exec_future = run_agent(config, query);
        return exec_future.get();
    });
}

// ============================================================================
// Team execution
// ============================================================================
std::future<AgentResult> AgentService::run_team(const TeamConfig& team,
                                                  const std::string& query,
                                                  const std::string& api_key) {
    return std::async(std::launch::async, [this, team, query, api_key]() {
        LOG_INFO("AgentService", "Running team '%s' with %zu members",
                 team.name.c_str(), team.members.size());

        AgentConfig config;
        config.provider = team.coordinator_provider;
        config.model_id = team.coordinator_model;

        json extra = {
            {"team", team.to_json()},
            {"api_key", api_key}
        };

        auto payload = build_payload("run_team", config, query, extra);
        return call_python_agent(payload);
    });
}

// ============================================================================
// Plan generation
// ============================================================================
std::future<ExecutionPlan> AgentService::generate_plan(const std::string& query,
                                                         const AgentConfig& config) {
    return std::async(std::launch::async, [this, query, config]() {
        auto payload = build_payload("generate_dynamic_plan", config, query);
        auto result = call_python_agent(payload);

        ExecutionPlan plan;
        plan.id = "plan_" + std::to_string(time(nullptr));
        plan.name = "Plan: " + query.substr(0, 50);
        plan.description = query;

        if (result.success) {
            try {
                auto j = json::parse(result.raw_output);
                if (j.contains("steps") && j["steps"].is_array()) {
                    for (auto& s : j["steps"]) {
                        PlanStep step;
                        step.id = s.value("id", "step_" + std::to_string(plan.steps.size()));
                        step.name = s.value("name", "Step " + std::to_string(plan.steps.size() + 1));
                        step.step_type = s.value("step_type", "agent");
                        if (s.contains("config")) step.config = s["config"];
                        if (s.contains("dependencies") && s["dependencies"].is_array()) {
                            for (auto& d : s["dependencies"])
                                step.dependencies.push_back(d.get<std::string>());
                        }
                        plan.steps.push_back(std::move(step));
                    }
                }
            } catch (...) {
                // Add a single fallback step
                PlanStep step;
                step.id = "step_1";
                step.name = "Execute Query";
                step.step_type = "agent";
                plan.steps.push_back(step);
            }
        } else {
            plan.status = "failed";
            plan.has_failed = true;
        }

        return plan;
    });
}

// ============================================================================
// Plan execution
// ============================================================================
std::future<ExecutionPlan> AgentService::execute_plan(ExecutionPlan plan,
                                                        const AgentConfig& config) {
    return std::async(std::launch::async, [this, plan, config]() mutable {
        plan.status = "running";

        for (auto& step : plan.steps) {
            if (cancel_flag_) {
                step.status = "cancelled";
                continue;
            }

            // Check dependencies
            bool deps_met = true;
            for (auto& dep : step.dependencies) {
                for (auto& other : plan.steps) {
                    if (other.id == dep && other.status != "completed") {
                        deps_met = false;
                        break;
                    }
                }
                if (!deps_met) break;
            }

            if (!deps_met) {
                step.status = "skipped";
                continue;
            }

            step.status = "running";

            auto payload = build_payload("execute_plan_step", config, step.name,
                                          {{"step", {{"id", step.id}, {"config", step.config}}}});
            auto result = call_python_agent(payload);

            if (result.success) {
                step.status = "completed";
                step.result = result.response;
            } else {
                step.status = "failed";
                step.error = result.error;
                plan.has_failed = true;
            }
        }

        // Check overall status
        bool all_done = true;
        for (auto& s : plan.steps) {
            if (s.status != "completed" && s.status != "skipped") {
                all_done = false;
                break;
            }
        }
        plan.is_complete = all_done;
        plan.status = plan.has_failed ? "failed" : (all_done ? "completed" : "partial");

        return plan;
    });
}

// ============================================================================
// Direct LLM HTTP
// ============================================================================
std::future<AgentResult> AgentService::call_llm_direct(
    const std::string& provider, const std::string& model,
    const std::string& api_key, const std::string& system_prompt,
    const std::string& user_message, double temperature, int max_tokens)
{
    return std::async(std::launch::async, [=, this]() {
        if (provider == "anthropic") {
            return call_anthropic(api_key, model, system_prompt,
                                   user_message, temperature, max_tokens);
        }

        // Map provider to base URL
        std::string base_url;
        if (provider == "openai")       base_url = "https://api.openai.com/v1";
        else if (provider == "groq")    base_url = "https://api.groq.com/openai/v1";
        else if (provider == "ollama")  base_url = "http://localhost:11434/v1";
        else if (provider == "fincept") base_url = "https://api.fincept.in/api/llm";
        else if (provider == "google")  base_url = "https://generativelanguage.googleapis.com/v1beta";
        else {
            auto cfgs = db::ops::get_llm_configs();
            for (auto& c : cfgs) {
                if (c.provider == provider && c.base_url) {
                    base_url = *c.base_url;
                    break;
                }
            }
            if (base_url.empty()) base_url = "https://api.openai.com/v1";
        }

        return call_openai_compatible(base_url, api_key, model,
                                       system_prompt, user_message,
                                       temperature, max_tokens);
    });
}

// ============================================================================
// OpenAI-compatible HTTP call
// ============================================================================
AgentResult AgentService::call_openai_compatible(
    const std::string& base_url, const std::string& api_key,
    const std::string& model, const std::string& system_prompt,
    const std::string& user_message, double temperature, int max_tokens)
{
    auto start = std::chrono::steady_clock::now();

    json messages = json::array();
    if (!system_prompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", system_prompt}});
    }
    messages.push_back({{"role", "user"}, {"content", user_message}});

    json body = {
        {"model", model},
        {"messages", messages},
        {"temperature", temperature},
        {"max_tokens", max_tokens}
    };

    auto& client = http::HttpClient::instance();
    auto resp = client.post_json(
        base_url + "/chat/completions", body,
        {{"Authorization", "Bearer " + api_key},
         {"Content-Type", "application/json"}});

    auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    AgentResult result;
    result.execution_time_ms = elapsed;
    result.model_used = model;
    result.provider_used = base_url;

    if (resp.success) {
        auto j = resp.json_body();
        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            auto& choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                result.response = choice["message"]["content"].get<std::string>();
                result.success = true;
            }
        }
        if (j.contains("usage")) {
            result.tokens_used = j["usage"].value("total_tokens", 0);
        }
        result.raw_output = resp.body;
    } else {
        result.error = resp.error.empty()
            ? "HTTP " + std::to_string(resp.status_code) + ": " + resp.body
            : resp.error;
    }

    return result;
}

// ============================================================================
// Anthropic HTTP call
// ============================================================================
AgentResult AgentService::call_anthropic(
    const std::string& api_key, const std::string& model,
    const std::string& system_prompt, const std::string& user_message,
    double temperature, int max_tokens)
{
    auto start = std::chrono::steady_clock::now();

    json body = {
        {"model", model},
        {"max_tokens", max_tokens},
        {"messages", json::array({
            {{"role", "user"}, {"content", user_message}}
        })}
    };
    if (!system_prompt.empty()) {
        body["system"] = system_prompt;
    }

    auto& client = http::HttpClient::instance();
    auto resp = client.post_json(
        "https://api.anthropic.com/v1/messages", body,
        {{"x-api-key", api_key},
         {"anthropic-version", "2023-06-01"},
         {"Content-Type", "application/json"}});

    auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    AgentResult result;
    result.execution_time_ms = elapsed;
    result.model_used = model;
    result.provider_used = "anthropic";

    if (resp.success) {
        auto j = resp.json_body();
        if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
            result.response = j["content"][0].value("text", "");
            result.success = true;
        }
        if (j.contains("usage")) {
            result.tokens_used = j["usage"].value("input_tokens", 0)
                               + j["usage"].value("output_tokens", 0);
        }
        result.raw_output = resp.body;
    } else {
        result.error = resp.error.empty()
            ? "Anthropic HTTP " + std::to_string(resp.status_code) + ": " + resp.body
            : resp.error;
    }

    return result;
}

// ============================================================================
// Keyword fallback routing (when Python/LLM unavailable)
// ============================================================================
static RoutingResult keyword_fallback_route(const std::string& query) {
    RoutingResult r;
    r.success = true;
    r.confidence = 0.6;

    // Simple keyword matching
    auto contains = [&](const char* kw) {
        return query.find(kw) != std::string::npos;
    };

    if (contains("trade") || contains("order") || contains("buy") || contains("sell")) {
        r.intent = "TRADING";
        r.agent_id = "trading_agent";
    } else if (contains("portfolio") || contains("allocation") || contains("rebalance")) {
        r.intent = "PORTFOLIO";
        r.agent_id = "portfolio_management_agent";
    } else if (contains("risk") || contains("drawdown") || contains("volatility")) {
        r.intent = "RISK";
        r.agent_id = "risk_analysis_agent";
    } else if (contains("news") || contains("sentiment") || contains("headline")) {
        r.intent = "NEWS";
        r.agent_id = "sentiment_news_analyst";
    } else if (contains("geopolit") || contains("conflict") || contains("sanctions")) {
        r.intent = "GEOPOLITICS";
        r.agent_id = "geopolitical_risk_analyst";
    } else if (contains("economy") || contains("gdp") || contains("inflation") || contains("fed")) {
        r.intent = "ECONOMICS";
        r.agent_id = "macro_cycle_agent";
    } else if (contains("analyz") || contains("stock") || contains("equity") || contains("valuation")) {
        r.intent = "ANALYSIS";
        r.agent_id = "stock_analysis_agent";
    } else {
        r.intent = "GENERAL";
        r.agent_id = "analysis_agent";
    }

    return r;
}

} // namespace fincept::agent_studio
