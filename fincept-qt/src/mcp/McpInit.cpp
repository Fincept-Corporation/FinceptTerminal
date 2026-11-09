// McpInit.cpp — Register the core MCP tools and start the system

#include "mcp/McpInit.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/core/CoreTools.h"
#include "mcp/server/McpHttpServer.h"

#include <QCoreApplication>
#include <QJsonDocument>

#include <algorithm>
#include <atomic>

namespace fincept::mcp {

static constexpr const char* TAG = "McpInit";

// One-shot diagnostic: walk every registered ToolDef and warn on any whose
// serialized schema exceeds kSchemaSizeWarnBytes. Common culprits: huge
// enum lists (e.g. exhaustive country/currency codes), verbose multi-line
// descriptions, or deeply nested object schemas. Every byte of schema
// multiplies across every LLM turn that includes the tool.
//
// Logs at INFO if all schemas are within budget; logs WARN with offenders
// (sorted largest-first) otherwise.
//
// NOT cheap, despite the old comment: it re-serialises every registered tool
// schema with QJsonDocument::toJson purely to produce a log line, on the
// pre-window boot path — hundreds of KB of JSON the app then throws away.
// Opt-in only: pass --audit-tools, or turn the McpInit tag up to Debug.
static constexpr int kSchemaSizeWarnBytes = 2048;

static bool schema_audit_requested() {
    if (Logger::instance().is_enabled(LogLevel::Debug, QString::fromLatin1(TAG)))
        return true;
    return qApp && qApp->arguments().contains(QStringLiteral("--audit-tools"));
}

static void audit_tool_schema_sizes() {
    const auto tools = McpProvider::instance().list_all_tools();
    struct Offender {
        QString name;
        int bytes;
    };
    QVector<Offender> over_budget;
    int total_bytes = 0;
    for (const auto& t : tools) {
        const int bytes = QJsonDocument(t.input_schema).toJson(QJsonDocument::Compact).size();
        total_bytes += bytes;
        if (bytes > kSchemaSizeWarnBytes)
            over_budget.append({t.name, bytes});
    }
    std::sort(over_budget.begin(), over_budget.end(),
              [](const Offender& a, const Offender& b) { return a.bytes > b.bytes; });

    LOG_INFO(
        TAG,
        QString("Tool schema audit: %1 tools, %2 KB total schema bytes").arg(tools.size()).arg(total_bytes / 1024));
    if (over_budget.isEmpty())
        return;

    LOG_WARN(TAG, QString("Tool schema audit: %1 tools exceed %2 B budget — every "
                          "byte multiplies across every LLM turn. Consider trimming "
                          "enums / descriptions / nested objects.")
                      .arg(over_budget.size())
                      .arg(kSchemaSizeWarnBytes));
    for (const auto& o : over_budget)
        LOG_WARN(TAG, QString("  %1 — %2 B").arg(o.name).arg(o.bytes));
}

void initialize_all_tools() {
    auto& provider = McpProvider::instance();

    // The terminal's whole internal tool surface: ten core tools that operate
    // the UI the way a person does (ui_overview, ui_open, ui_snapshot, ui_act,
    // ui_read, ui_wait, ui_screenshot, ui_workspace, ui_invoke) plus `python`
    // for the bundled analytics scripts. Every screen is reachable through them,
    // so no screen needs a dedicated tool. See mcp/core/CoreTools.h.
    provider.register_tools(core::get_core_tools());

    LOG_INFO(TAG, QString("Registered %1 internal MCP tools").arg(provider.tool_count()));

    // Audit schema sizes after registration — surfaces bloated tools before
    // they bleed prompt tokens on every turn. Opt-in: it re-serialises every
    // schema and this is the pre-window boot path.
    if (schema_audit_requested())
        audit_tool_schema_sizes();

    // Initialize unified service (starts external servers in background)
    McpService::instance().initialize();

    // External-agent MCP server (Claude Code / Codex / Cursor). Only when the
    // user enabled "start with the terminal"; needs the catalogue above.
    McpHttpServer::instance().start_if_enabled();
}

void shutdown_mcp() {
    // Idempotent: teardown may be reached from an aboutToQuit handler, a test
    // harness, and a destructor in the same run. Running McpManager::shutdown()
    // twice would stop already-deleted clients.
    static std::atomic<bool> s_done{false};
    bool expected = false;
    if (!s_done.compare_exchange_strong(expected, true)) {
        LOG_DEBUG(TAG, "shutdown_mcp() already ran — ignoring");
        return;
    }

    // Close the local agent bridge and the external-agent MCP server first so no
    // new tool call can arrive while the provider registry is being cleared out
    // from under it.
    TerminalMcpBridge::instance().stop();
    McpHttpServer::instance().stop();

    // Stops the health-check timer, then every external server process.
    // Without this the child processes (npx/uvx/python MCP servers) outlive the
    // terminal — see the CROSS-FILE note about main.cpp's aboutToQuit handler,
    // which is what makes this function reachable at all.
    McpService::instance().shutdown();
    LOG_INFO(TAG, "MCP system shut down");
}

} // namespace fincept::mcp
