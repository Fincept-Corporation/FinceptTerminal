#pragma once
// McpClientProfile.h — one table of what each MCP client (harness) needs from
// the terminal's server, instead of special cases scattered through the code.
// QtCore only; unit-tested by tests/tst_mcp_server_engine.cpp.
//
// A client is identified, most reliable first, by
//   1. the harness id bound to the run-scoped bearer token (in-app runs: the
//      terminal launched the CLI itself and knows exactly which one it is),
//   2. clientInfo.name from `initialize` (remembered per Mcp-Session-Id),
//   3. the HTTP User-Agent.
// Unknown clients get the Generic profile: plain, spec-conformant MCP.
//
// What a profile decides at run time:
//   * the JSON-Schema dialect tools/list is shaped into (Standard = as built;
//     Gemini = the OpenAPI subset of GeminiSchema.h; OpenAiStrict = every object
//     closed and every property required, optional ones nullable),
//   * whether tools/call results carry `structuredContent` next to the text
//     content (LLM harnesses read `content`; a copy in structuredContent is
//     either ignored or — for clients that prefer it — replaces the text and
//     loses the message, see kStructuredContentNote in the .cpp),
//   * the tool-name budget left after the client's own prefix.
// How each harness is configured to reach the server lives in the adapters
// (src/services/harness/adapters); the comments in the .cpp cite the documents.

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace fincept::mcp::profiles {

enum class ClientKind {
    Generic = 0,
    ClaudeCode,
    ClaudeDesktop,
    Codex,
    GeminiCli,
    QwenCode,
    Cursor,
    Copilot,
    Grok,
    VsCode,
    Count_ // not a client
};

enum class SchemaDialect {
    Standard,     ///< JSON Schema as the tools declare it
    Gemini,       ///< Gemini FunctionDeclaration subset (GeminiSchema.h)
    OpenAiStrict, ///< closed objects, all properties required, optional ones nullable
};

struct ClientProfile {
    ClientKind kind = ClientKind::Generic;
    const char* id = "generic";      ///< stable id (logs, self-check report)
    const char* display = "MCP client";
    const char* harness_id = "";     ///< matching src/services/harness adapter id ("claude", "codex", …)
    /// Stable caller id handed to tool handlers (ScopedCallClient, CallApproval.h)
    /// and stamped on what an agent causes ("agent:<actor_id>"). Unknown = "external".
    const char* actor_id = "external";
    SchemaDialect dialect = SchemaDialect::Standard;
    bool structured_content = true;  ///< send structuredContent next to the text content
    int tool_name_budget = 64;       ///< longest advertised tool name this client accepts
    int description_max_chars = 0;   ///< 0 = no cap
    /// Largest tool result sent inline to this client, whatever the user's
    /// budget says (0 = the user's budget only). Above a client's own threshold
    /// it spills the result to a file and the model needs an extra round trip.
    int result_budget_cap_bytes = 0;
};

const ClientProfile& profile(ClientKind kind);

/// Every profile, Generic first (for the self-check and tests).
QList<ClientKind> all_kinds();

/// From clientInfo.name (and optionally the HTTP User-Agent). Never empty-handed:
/// anything unrecognised is Generic.
ClientKind detect_client(const QString& client_name, const QString& user_agent = {});

/// From a harness adapter id ("claude", "codex", "gemini", "qwen", "cursor",
/// "copilot", "grok"). Unknown → Generic.
ClientKind kind_for_harness(const QString& harness_id);

/// tools/list entries for this client: the dialect applied to every
/// inputSchema, descriptions capped, names checked against the budget (an entry
/// whose name does not fit is kept as is and reported through `*warnings`).
QJsonArray shape_catalogue(const QJsonArray& entries, const ClientProfile& p, QStringList* warnings = nullptr);

/// One inputSchema in the profile's dialect (always an object schema).
QJsonObject shape_input_schema(const QJsonObject& schema, SchemaDialect dialect);

/// Undo what a dialect did to the arguments a client sends back, against the
/// ORIGINAL schema: OpenAiStrict drops nulls the model had to send for optional
/// properties; Gemini parses JSON strings that stood in for free-form objects.
QJsonObject restore_call_args(const QJsonObject& original_schema, const QJsonObject& args, SchemaDialect dialect);

/// True when `schema` satisfies OpenAI strict mode (every object closed with
/// additionalProperties:false and every property listed in required). For tests
/// and the self-check.
bool is_strict_schema(const QJsonObject& schema, QString* why = nullptr);

} // namespace fincept::mcp::profiles
