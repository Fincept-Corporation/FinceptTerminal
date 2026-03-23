#include "services/workflow/nodes/AgentNodes.h"
#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_agent_nodes(NodeRegistry& registry)
{
    registry.register_type({
        .type_id = "agent.single", .display_name = "AI Agent",
        .category = "Agents", .description = "Execute a single AI agent with a prompt",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Context", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Response", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "agent_type", "Agent", "select", "general", {"general","geopolitics","economic","hedge_fund","trader","investor","deep_research"}, "", true },
            { "prompt", "Prompt", "code", "", {}, "Instructions for the agent", true },
            { "model", "Model", "select", "auto", {"auto","claude-sonnet","claude-haiku","gpt-4o","gpt-4o-mini"}, "" },
            { "max_tokens", "Max Tokens", "number", 4096, {}, "" },
        },
        .execute = nullptr, // Via AgentService
    });

    registry.register_type({
        .type_id = "agent.multi", .display_name = "Multi-Agent",
        .category = "Agents", .description = "Run multiple AI agents in parallel",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Context", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Combined", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "agents", "Agent Types", "string", "economic,geopolitics", {}, "Comma-separated agent types" },
            { "prompt", "Shared Prompt", "code", "", {}, "Prompt sent to all agents" },
        },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "agent.mediator", .display_name = "Agent Mediator",
        .category = "Agents", .description = "Coordinate multiple agents (sequential, parallel, consensus)",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {
            { "input_0", "Agent 1", PortDirection::Input, ConnectionType::Main },
            { "input_1", "Agent 2", PortDirection::Input, ConnectionType::Main },
        },
        .outputs = {{ "output_main", "Mediated", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "mode", "Mode", "select", "sequential", {"sequential","parallel","consensus","debate"}, "" },
            { "mediator_prompt", "Mediator Prompt", "code", "", {}, "Instructions for the mediator" },
        },
        .execute = nullptr,
    });

    // ── Tier 3: AI nodes ───────────────────────────────────────────

    registry.register_type({
        .type_id = "agent.summarizer", .display_name = "Summarizer",
        .category = "Agents", .description = "Summarize long text, data, or documents",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Text In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Summary", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "max_length", "Max Length", "number", 500, {}, "Max summary words" },
            { "style", "Style", "select", "concise", {"concise","detailed","bullet_points","executive"}, "" },
            { "model", "Model", "select", "auto", {"auto","claude-sonnet","claude-haiku","gpt-4o","gpt-4o-mini"}, "" },
        },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "agent.classifier", .display_name = "Classifier",
        .category = "Agents", .description = "Classify text (bullish/bearish/neutral, sector, topic)",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Text In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Classification", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "categories", "Categories", "string", "bullish,bearish,neutral", {}, "Comma-separated" },
            { "multi_label", "Multi-Label", "boolean", false, {}, "Allow multiple categories" },
            { "confidence_threshold", "Min Confidence", "number", 0.7, {}, "" },
        },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "agent.code_generator", .display_name = "Code Generator",
        .category = "Agents", .description = "Generate Python/SQL code from natural language",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Context", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Code", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "language", "Language", "select", "python", {"python","sql","javascript","r"}, "" },
            { "prompt", "Prompt", "code", "", {}, "Describe what code to generate", true },
            { "execute_code", "Auto-Execute", "boolean", false, {}, "Run generated code immediately" },
        },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "agent.rag_query", .display_name = "RAG Query",
        .category = "Agents", .description = "Query against document embeddings (RAG)",
        .icon_text = "AI", .accent_color = "#7c3aed", .version = 1,
        .inputs  = {{ "input_0", "Context", PortDirection::Input, ConnectionType::Main }},
        .outputs = {{ "output_main", "Answer", PortDirection::Output, ConnectionType::Main }},
        .parameters = {
            { "query", "Query", "string", "", {}, "Question to ask", true },
            { "collection", "Collection", "string", "default", {}, "Document collection name" },
            { "top_k", "Top K Results", "number", 5, {}, "" },
        },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
