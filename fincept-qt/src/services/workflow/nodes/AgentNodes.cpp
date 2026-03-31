#include "services/workflow/nodes/AgentNodes.h"

#include "ai_chat/LlmService.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "services/agents/AgentTypes.h"
#include "services/workflow/NodeRegistry.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QJsonDocument>
#include <QObject>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

namespace fincept::workflow {

// Serialise an input QJsonValue to a readable string for injecting into prompts.
static QString json_to_context(const QJsonValue& v) {
    if (v.isString())  return v.toString();
    if (v.isDouble())  return QString::number(v.toDouble());
    if (v.isBool())    return v.toBool() ? "true" : "false";
    if (v.isObject())  return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())   return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return {};
}

void register_agent_nodes(NodeRegistry& registry) {

    // ── Single unified AI Agent node ───────────────────────────────
    // Parameters are populated dynamically at property-panel render time.
    // The node stores agent_id + llm_profile_id; at execute time it:
    //   1. Loads the AgentConfig from the repository by agent_id
    //   2. Resolves the LLM profile by llm_profile_id (or falls back to
    //      the agent's saved assignment → global default)
    //   3. Builds the same config JSON that AgentsViewPanel does
    //   4. Calls AgentService::run_agent() and bridges the async
    //      agent_result signal back to the workflow callback via a
    //      one-shot QObject connection guarded by request_id

    registry.register_type({
        .type_id      = "agent.run",
        .display_name = "AI Agent",
        .category     = "Agents",
        .description  = "Run a configured AI agent with optional context input",
        .icon_text    = "AI",
        .accent_color = "#7c3aed",
        .version      = 2,
        .inputs  = {{"input_0", "Context", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Response", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                // agent_id: populated as a "select" in the properties panel by querying
                // AgentConfigRepository at render time via the ServiceBridges post-register
                // hook. Stored as the agent UUID so the execute lambda can reload it.
                {"agent_id",       "Agent",       "agent_select",  "",      {}, "Select a configured agent"},
                {"llm_profile_id", "LLM Profile", "llm_select",    "",      {}, "Override LLM profile (leave blank to use agent default)"},
                {"extra_instructions", "Extra Instructions", "code", "", {}, "Append additional instructions to the agent's system prompt"},
                {"reasoning",  "Extended Thinking", "boolean", false, {}, ""},
                {"memory",     "Memory",            "boolean", false, {}, ""},
                {"knowledge",  "Knowledge Base",    "boolean", false, {}, ""},
                {"guardrails", "Guardrails",        "boolean", false, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {

                QString agent_id      = params.value("agent_id").toString();
                QString profile_id    = params.value("llm_profile_id").toString();
                QString extra_instr   = params.value("extra_instructions").toString();

                // Load agent config from DB
                QString instructions;
                QJsonArray tools;
                if (!agent_id.isEmpty()) {
                    auto res = fincept::AgentConfigRepository::instance().get(agent_id);
                    if (res.is_ok()) {
                        auto cfg_json = QJsonDocument::fromJson(
                            res.value().config_json.toUtf8()).object();
                        instructions = cfg_json.value("instructions").toString();
                        tools        = cfg_json.value("tools").toArray();
                    }
                }

                if (!extra_instr.isEmpty()) {
                    instructions = instructions.isEmpty()
                                   ? extra_instr
                                   : instructions + "\n\n" + extra_instr;
                }

                // Resolve LLM profile
                using fincept::ai_chat::LlmService;
                using fincept::LlmProfileRepository;
                using fincept::ResolvedLlmProfile;

                ResolvedLlmProfile resolved;
                if (!profile_id.isEmpty()) {
                    auto pr = LlmProfileRepository::instance().list_profiles();
                    if (pr.is_ok()) {
                        for (const auto& p : pr.value()) {
                            if (p.id == profile_id) {
                                resolved.profile_id    = p.id;
                                resolved.profile_name  = p.name;
                                resolved.provider      = p.provider;
                                resolved.model_id      = p.model_id;
                                resolved.api_key       = p.api_key;
                                resolved.base_url      = p.base_url;
                                resolved.temperature   = p.temperature;
                                resolved.max_tokens    = p.max_tokens;
                                resolved.system_prompt = p.system_prompt;
                                break;
                            }
                        }
                    }
                }
                if (resolved.provider.isEmpty())
                    resolved = LlmService::instance().resolve_profile("agent", agent_id);

                // Build config (mirrors AgentsViewPanel::build_config_from_editor)
                QJsonObject config;
                config["model"]        = LlmService::profile_to_json(resolved);
                config["instructions"] = instructions;
                config["tools"]        = tools;
                config["reasoning"]    = params.value("reasoning").toBool(false);
                config["memory"]       = params.value("memory").toBool(false);
                config["knowledge"]    = params.value("knowledge").toBool(false);
                config["guardrails"]   = params.value("guardrails").toBool(false);
                config["tracing"]      = false;
                config["agentic_memory"] = false;

                // Build query — prepend upstream context if any
                QString query;
                if (!inputs.isEmpty() && !inputs[0].isNull() && !inputs[0].isUndefined()) {
                    QString ctx = json_to_context(inputs[0]);
                    if (!ctx.isEmpty())
                        query = "Context:\n" + ctx + "\n\n";
                }
                if (!instructions.isEmpty() && query.isEmpty())
                    query = instructions;
                else if (!instructions.isEmpty())
                    query += instructions;

                if (query.trimmed().isEmpty()) {
                    cb(false, {}, "No prompt or instructions — set Extra Instructions or configure the agent");
                    return;
                }

                auto& svc = fincept::services::AgentService::instance();

                // Run agent — result comes back via agent_result signal.
                // Connect a one-shot lambda guarded by request_id to avoid
                // cross-workflow contamination.
                QString req_id = svc.run_agent(query, config);

                // Bridge the signal to the workflow callback.
                // QObject::connect with a lambda capture; the connection is
                // stored and disconnected after first matching result.
                struct Guard : QObject {
                    std::function<void(bool, QJsonValue, QString)> cb;
                    QString req_id;
                    QMetaObject::Connection conn;
                };
                auto* guard = new Guard;
                guard->cb     = cb;
                guard->req_id = req_id;
                guard->conn   = QObject::connect(
                    &svc, &fincept::services::AgentService::agent_result,
                    guard,
                    [guard](const fincept::services::AgentExecutionResult& result) {
                        if (result.request_id != guard->req_id) return;
                        QObject::disconnect(guard->conn);
                        guard->deleteLater();
                        if (!result.success) {
                            guard->cb(false, {}, result.error.isEmpty()
                                      ? "Agent execution failed" : result.error);
                            return;
                        }
                        QJsonObject out;
                        out["response"]  = result.response;
                        out["agent_id"]  = guard->req_id.left(8);
                        out["exec_ms"]   = result.execution_time_ms;
                        guard->cb(true, out, {});
                    });
            },
    });

    // ── Tool Picker ────────────────────────────────────────────────
    // Takes a natural-language query (or upstream JSON context), asks the
    // configured LLM which Fincept MCP tool best handles the request, and
    // outputs { "tool": "<name>", "args": {...} } ready for the MCP Tool node.
    registry.register_type({
        .type_id      = "agent.tool_picker",
        .display_name = "Tool Picker",
        .category     = "Agents",
        .description  = "Ask the LLM to pick the right MCP tool and arguments for a query",
        .icon_text    = "TP",
        .accent_color = "#6366f1",
        .version      = 1,
        .inputs  = {{"input_0", "Query / Context", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Tool Call", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"query", "Query", "string", "", {}, "What do you want to do? (overrides input)", false},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {

                // Build the user query — explicit param wins, else upstream input
                QString query = params.value("query").toString().trimmed();
                if (query.isEmpty() && !inputs.isEmpty() && !inputs[0].isNull()) {
                    query = json_to_context(inputs[0]);
                }
                if (query.isEmpty()) {
                    cb(false, {}, "Tool Picker: provide a query via the 'Query' param or an upstream input");
                    return;
                }

                // Build tool catalogue summary for the prompt
                auto tools = mcp::McpService::instance().get_all_tools();
                QString tool_list;
                for (const auto& t : tools) {
                    tool_list += QString("- %1: %2\n").arg(t.name, t.description);
                }

                QString system_prompt =
                    "You are a tool-selection assistant for the Fincept Terminal.\n"
                    "Given a user request, choose the single best tool from the list below "
                    "and return ONLY a JSON object with this exact shape:\n"
                    "{\"tool\": \"<tool_name>\", \"args\": {<required args as key/value>}}\n"
                    "Do not include any explanation or markdown. Return raw JSON only.\n\n"
                    "Available tools:\n" + tool_list;

                (void)QtConcurrent::run([system_prompt, query, cb]() {
                    using fincept::ai_chat::LlmService;
                    using fincept::ai_chat::ConversationMessage;

                    // Single non-streaming call using globally active LLM config
                    auto resp = LlmService::instance().chat(
                        query,
                        {ConversationMessage{"system", system_prompt}});

                    if (!resp.success) {
                        cb(false, {}, "Tool Picker LLM call failed: " + resp.error);
                        return;
                    }

                    // Strip markdown fences if model wraps output
                    QString raw = resp.content.trimmed();
                    if (raw.startsWith("```"))
                        raw = raw.section('\n', 1).section("```", 0, 0).trimmed();

                    auto doc = QJsonDocument::fromJson(raw.toUtf8());
                    if (doc.isNull() || !doc.isObject()) {
                        cb(false, {}, "Tool Picker: LLM did not return valid JSON: " + raw.left(200));
                        return;
                    }

                    QJsonObject out = doc.object();
                    if (!out.contains("tool")) {
                        cb(false, {}, "Tool Picker: JSON missing 'tool' key: " + raw.left(200));
                        return;
                    }

                    cb(true, out, {});
                });
            },
    });
}

} // namespace fincept::workflow
