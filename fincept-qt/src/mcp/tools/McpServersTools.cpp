// McpServersTools.cpp — Tools that drive the External MCP Server panel.
// Read tools expose installed servers / marketplace / logs / external tools.
// Lifecycle and config tools manage server processes and entries.
// Lifecycle ops (start/stop/restart/call_external) are async because
// McpManager / McpClient explicitly forbid being called from the UI thread
// (they block on QProcess / RPC). All mutating tools are marked
// is_destructive so the auth checker prompts the user.

#include "mcp/tools/McpServersTools.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpMarketplace.h"
#include "mcp/McpService.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "McpServersTools";

QJsonObject server_to_json(const McpServerConfig& s) {
    QJsonArray args_arr;
    for (const auto& a : s.args)
        args_arr.append(a);
    QJsonObject env_obj;
    for (auto it = s.env.constBegin(); it != s.env.constEnd(); ++it)
        env_obj[it.key()] = it.value();
    return QJsonObject{
        {"id", s.id},
        {"name", s.name},
        {"description", s.description},
        {"command", s.command},
        {"args", args_arr},
        {"env", env_obj},
        {"category", s.category},
        {"enabled", s.enabled},
        {"auto_start", s.auto_start},
        {"status", server_status_str(s.status)},
    };
}

QString id_from_name(const QString& name) {
    return name.toLower().replace(' ', '_');
}

void resolve_async(std::shared_ptr<QPromise<ToolResult>> promise, ToolResult r) {
    if (promise->future().isFinished())
        return;
    promise->addResult(std::move(r));
    promise->finish();
}

} // namespace

std::vector<ToolDef> get_mcp_servers_tools() {
    std::vector<ToolDef> tools;

    // ── list_mcp_servers ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_mcp_servers";
        t.description = "List all installed external MCP servers with their status and config.";
        t.category = "mcp-servers";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& s : McpManager::instance().get_servers())
                arr.append(server_to_json(s));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── get_mcp_server ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_mcp_server";
        t.description = "Get full config and current status for one installed external MCP server by id.";
        t.category = "mcp-servers";
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id (lowercase, underscore-separated)").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            for (const auto& s : McpManager::instance().get_servers()) {
                if (s.id == id)
                    return ToolResult::ok_data(server_to_json(s));
            }
            return ToolResult::fail("Server not found: " + id);
        };
        tools.push_back(std::move(t));
    }

    // ── get_mcp_server_logs ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_mcp_server_logs";
        t.description = "Get captured stdout/stderr log lines for a running server (most recent first up to N).";
        t.category = "mcp-servers";
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .integer("limit", "Max lines to return").default_int(100).between(1, 500)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const int limit = args["limit"].toInt(100);
            const QStringList all = McpManager::instance().get_logs(id);
            const qsizetype start = std::max<qsizetype>(0, all.size() - limit);
            QJsonArray arr;
            for (qsizetype i = start; i < all.size(); ++i)
                arr.append(all[i]);
            return ToolResult::ok_data(QJsonObject{{"id", id}, {"count", arr.size()}, {"lines", arr}});
        };
        tools.push_back(std::move(t));
    }

    // ── list_external_mcp_tools ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_external_mcp_tools";
        t.description = "List all tools exposed by currently-running external MCP servers.";
        t.category = "mcp-servers";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& tool : McpManager::instance().get_all_external_tools()) {
                arr.append(QJsonObject{
                    {"server_id", tool.server_id},
                    {"server_name", tool.server_name},
                    {"name", tool.name},
                    {"description", tool.description},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── list_mcp_marketplace ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_mcp_marketplace";
        t.description = "List preset external MCP servers from the built-in marketplace catalog.";
        t.category = "mcp-servers";
        t.input_schema = ToolSchemaBuilder()
            .string("category", "Filter by category (utilities, developer, database). Empty = all.")
                .default_str("")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString filter = args["category"].toString().toLower();
            QJsonArray arr;
            for (const auto& e : marketplace_catalog()) {
                if (!filter.isEmpty() && e.category != filter)
                    continue;
                QJsonArray args_arr;
                for (const auto& a : e.args)
                    args_arr.append(a);
                QJsonArray env_keys;
                for (const auto& k : e.env_keys)
                    env_keys.append(k);
                arr.append(QJsonObject{
                    {"name", e.name},
                    {"description", e.description},
                    {"command", e.command},
                    {"args", args_arr},
                    {"env_keys", env_keys},
                    {"category", e.category},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── install_mcp_server_from_marketplace ─────────────────────────────
    {
        ToolDef t;
        t.name = "install_mcp_server_from_marketplace";
        t.description = "Install a preset external MCP server by its marketplace name. Required env vars must be supplied.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.input_schema = ToolSchemaBuilder()
            .string("name", "Marketplace entry name (e.g. 'Fetch', 'PostgreSQL')").required().length(1, 64)
            .object("env", "Environment variables {KEY: value} required by the entry")
            .boolean("auto_start", "Auto-start this server on app launch").default_bool(false)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString name = args["name"].toString().trimmed();
            const MarketplaceEntry* entry = nullptr;
            for (const auto& e : marketplace_catalog()) {
                if (e.name.compare(name, Qt::CaseInsensitive) == 0) {
                    entry = &e;
                    break;
                }
            }
            if (!entry)
                return ToolResult::fail("Marketplace entry not found: " + name);

            McpServerConfig cfg;
            cfg.id = id_from_name(entry->name);
            cfg.name = entry->name;
            cfg.description = entry->description;
            cfg.command = entry->command;
            cfg.args = entry->args;
            cfg.category = entry->category;
            cfg.enabled = true;
            cfg.auto_start = args["auto_start"].toBool(false);

            const QJsonObject env = args["env"].toObject();
            for (const auto& key : entry->env_keys) {
                const QString val = env[key].toString().trimmed();
                if (val.isEmpty())
                    return ToolResult::fail("Missing required env var: " + key);
                cfg.env[key] = val;
            }

            const auto r = McpManager::instance().save_server(cfg);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            LOG_INFO(TAG, "Installed from marketplace: " + cfg.id);
            return ToolResult::ok("Server installed", QJsonObject{{"id", cfg.id}, {"name", cfg.name}});
        };
        tools.push_back(std::move(t));
    }

    // ── add_mcp_server ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_mcp_server";
        t.description = "Add a custom external MCP server. Spawns a process via command + args.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.input_schema = ToolSchemaBuilder()
            .string("name", "Display name").required().length(1, 64)
            .string("command", "Executable command (e.g. 'uvx', 'npx', 'python')").required().length(1, 128)
            .array("args", "Command arguments",
                   QJsonObject{{"type", "string"}})
            .object("env", "Environment variables {KEY: value}")
            .string("description", "Optional description").default_str("").length(0, 512)
            .string("category", "Category tag").default_str("utilities").length(0, 32)
            .boolean("auto_start", "Auto-start on launch").default_bool(false)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString name = args["name"].toString().trimmed();
            const QString command = args["command"].toString().trimmed();
            if (name.isEmpty() || command.isEmpty())
                return ToolResult::fail("name and command are required");

            McpServerConfig cfg;
            cfg.id = id_from_name(name);
            cfg.name = name;
            cfg.description = args["description"].toString();
            cfg.command = command;
            for (const auto& v : args["args"].toArray())
                cfg.args.append(v.toString());
            const QJsonObject env = args["env"].toObject();
            for (auto it = env.constBegin(); it != env.constEnd(); ++it)
                cfg.env[it.key()] = it.value().toString();
            cfg.category = args["category"].toString("utilities");
            cfg.enabled = true;
            cfg.auto_start = args["auto_start"].toBool(false);

            const auto r = McpManager::instance().save_server(cfg);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            LOG_INFO(TAG, "Added custom server: " + cfg.id);
            return ToolResult::ok("Server added", QJsonObject{{"id", cfg.id}, {"name", cfg.name}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_mcp_server ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_mcp_server";
        t.description = "Remove an installed external MCP server. Stops it first if running.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const auto r = McpManager::instance().remove_server(id);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            LOG_INFO(TAG, "Removed server: " + id);
            return ToolResult::ok("Server removed", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── set_mcp_server_enabled ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_mcp_server_enabled";
        t.description = "Enable or disable an installed external MCP server (does not start/stop the process).";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .boolean("enabled", "true to enable, false to disable").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const bool enabled = args["enabled"].toBool();

            // McpManager has no set_enabled — round-trip via save_server.
            for (const auto& s : McpManager::instance().get_servers()) {
                if (s.id != id)
                    continue;
                McpServerConfig cfg = s;
                cfg.enabled = enabled;
                const auto r = McpManager::instance().save_server(cfg);
                if (r.is_err())
                    return ToolResult::fail(QString::fromStdString(r.error()));
                return ToolResult::ok(QString("Server %1").arg(enabled ? "enabled" : "disabled"),
                                       QJsonObject{{"id", id}, {"enabled", enabled}});
            }
            return ToolResult::fail("Server not found: " + id);
        };
        tools.push_back(std::move(t));
    }

    // ── set_mcp_server_autostart ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_mcp_server_autostart";
        t.description = "Toggle whether an installed external MCP server auto-starts on app launch.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .boolean("auto_start", "true to enable auto-start, false to disable").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const bool auto_start = args["auto_start"].toBool();
            for (const auto& s : McpManager::instance().get_servers()) {
                if (s.id != id)
                    continue;
                McpServerConfig cfg = s;
                cfg.auto_start = auto_start;
                const auto r = McpManager::instance().save_server(cfg);
                if (r.is_err())
                    return ToolResult::fail(QString::fromStdString(r.error()));
                return ToolResult::ok("Auto-start updated",
                                       QJsonObject{{"id", id}, {"auto_start", auto_start}});
            }
            return ToolResult::fail("Server not found: " + id);
        };
        tools.push_back(std::move(t));
    }

    // ── start_mcp_server ────────────────────────────────────────────────
    // Async: McpManager::start_server blocks on QProcess and is documented
    // as unsafe to call from the UI thread.
    {
        ToolDef t;
        t.name = "start_mcp_server";
        t.description = "Start an installed external MCP server process by id.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString id = args["id"].toString();
            (void)QtConcurrent::run([id, promise, ctx]() {
                if (ctx.cancelled())
                    return resolve_async(promise, ToolResult::fail("cancelled"));
                const auto r = McpManager::instance().start_server(id);
                resolve_async(promise,
                              r.is_ok() ? ToolResult::ok("Server started", QJsonObject{{"id", id}})
                                        : ToolResult::fail(QString::fromStdString(r.error())));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── stop_mcp_server ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "stop_mcp_server";
        t.description = "Stop a running external MCP server process by id.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString id = args["id"].toString();
            (void)QtConcurrent::run([id, promise, ctx]() {
                if (ctx.cancelled())
                    return resolve_async(promise, ToolResult::fail("cancelled"));
                const auto r = McpManager::instance().stop_server(id);
                resolve_async(promise,
                              r.is_ok() ? ToolResult::ok("Server stopped", QJsonObject{{"id", id}})
                                        : ToolResult::fail(QString::fromStdString(r.error())));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── restart_mcp_server ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "restart_mcp_server";
        t.description = "Restart a running external MCP server process by id.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Server id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString id = args["id"].toString();
            (void)QtConcurrent::run([id, promise, ctx]() {
                if (ctx.cancelled())
                    return resolve_async(promise, ToolResult::fail("cancelled"));
                const auto r = McpManager::instance().restart_server(id);
                resolve_async(promise,
                              r.is_ok() ? ToolResult::ok("Server restarted", QJsonObject{{"id", id}})
                                        : ToolResult::fail(QString::fromStdString(r.error())));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── call_external_mcp_tool ──────────────────────────────────────────
    // Forwards to McpService::execute_tool. Async because the external RPC
    // can block on stdio for arbitrary time and must run off the UI thread.
    {
        ToolDef t;
        t.name = "call_external_mcp_tool";
        t.description = "Invoke a tool exposed by a running external MCP server. Use list_external_mcp_tools to discover names.";
        t.category = "mcp-servers";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("server_id", "Server id from list_mcp_servers").required().length(1, 128)
            .string("tool_name", "Tool name from list_external_mcp_tools").required().length(1, 128)
            .object("args", "Tool arguments object (matches the external tool's input schema)")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString server_id = args["server_id"].toString();
            const QString tool_name = args["tool_name"].toString();
            const QJsonObject inner_args = args["args"].toObject();
            QtConcurrent::run([server_id, tool_name, inner_args, promise, ctx]() {
                if (ctx.cancelled())
                    return resolve_async(promise, ToolResult::fail("cancelled"));
                resolve_async(promise,
                              McpService::instance().execute_tool(server_id, tool_name, inner_args));
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 mcp-servers tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
