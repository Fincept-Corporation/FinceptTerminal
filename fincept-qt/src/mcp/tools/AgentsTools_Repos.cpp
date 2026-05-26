// src/mcp/tools/AgentsTools_Repos.cpp
//
// Repository / persistence tools: planner CRUD + execute, memory + knowledge,
// AgentConfigRepository CRUD, WorkflowRepository CRUD, memory repo variants,
// session management, paper trading, trade decisions.
//
// Part of the topic-based split of AgentsTools.cpp.

#include "mcp/tools/AgentsTools.h"
#include "mcp/tools/AgentsTools_internal.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "screens/node_editor/NodeEditorTypes.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/WorkflowRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUuid>

namespace fincept::mcp::tools {

using namespace fincept::mcp::tools::agents_internal;

void agents_internal::register_repos_tools(std::vector<ToolDef>& tools) {
    // ── 17-20. Planner ─────────────────────────────────────────────────
    auto bridge_plan = [](auto kick, ToolContext ctx,
                          std::shared_ptr<QPromise<ToolResult>> promise) {
        auto* svc = &services::AgentService::instance();
        AsyncDispatch::callback_to_promise(
            svc, std::move(ctx), promise,
            [svc, kick = std::move(kick)](auto resolve) mutable {
                const QString req_id = kick();
                if (req_id.isEmpty()) {
                    resolve(ToolResult::fail("Failed to start planner"));
                    return;
                }
                auto* holder = new QObject(svc);
                QObject::connect(svc, &services::AgentService::plan_created, holder,
                                  [req_id, resolve, holder](services::ExecutionPlan p) {
                                      if (p.request_id != req_id)
                                          return;
                                      resolve(ToolResult::ok_data(plan_to_json(p)));
                                      holder->deleteLater();
                                  });
                QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                  [resolve, holder](QString, QString msg) {
                                      resolve(ToolResult::fail(msg));
                                      holder->deleteLater();
                                  });
            });
    };

    {
        ToolDef t;
        t.name = "create_agent_plan";
        t.description = "Create an execution plan for a generic query.";
        t.category = "agents";
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Goal / query").required().length(1, 8192)
            .object("config", "Optional config override")
            .build();
        t.async_handler = [bridge_plan](const QJsonObject& args, ToolContext ctx,
                                         std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QJsonObject config = args["config"].toObject();
            bridge_plan(
                [query, config]() { return services::AgentService::instance().create_plan(query, config); },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "create_stock_analysis_plan";
        t.description = "Create an execution plan for analysing a specific ticker.";
        t.category = "agents";
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 16)
            .object("config", "Optional config override")
            .build();
        t.async_handler = [bridge_plan](const QJsonObject& args, ToolContext ctx,
                                         std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString symbol = args["symbol"].toString();
            const QJsonObject config = args["config"].toObject();
            bridge_plan(
                [symbol, config]() {
                    return services::AgentService::instance().create_stock_analysis_plan(symbol, config);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "create_portfolio_plan";
        t.description = "Create an execution plan for a portfolio (with goals + optional config).";
        t.category = "agents";
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .object("goals", "Portfolio goals (target return, max risk, horizon, etc.)")
            .object("config", "Optional config override")
            .build();
        t.async_handler = [bridge_plan](const QJsonObject& args, ToolContext ctx,
                                         std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject goals = args["goals"].toObject();
            const QJsonObject config = args["config"].toObject();
            bridge_plan(
                [goals, config]() {
                    return services::AgentService::instance().create_portfolio_plan(goals, config);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "execute_agent_plan";
        t.description = "Execute a previously-created plan and stream step results.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .object("plan", "Plan object from create_*_plan").required()
            .object("config", "Optional config override")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject plan = args["plan"].toObject();
            const QJsonObject config = args["config"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, plan, config](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::plan_executed, holder,
                                      [resolve, holder](services::ExecutionPlan p) {
                                          if (p.has_failed)
                                              resolve(ToolResult::fail("Plan failed"));
                                          else
                                              resolve(ToolResult::ok_data(plan_to_json(p)));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->execute_plan(plan, config);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 21-23. Memory & Knowledge ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "store_agent_memory";
        t.description = "Persist a piece of agent memory for later recall.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("content", "Memory content").required().length(1, 16384)
            .string("memory_type", "Memory bucket").default_str("general").length(0, 64)
            .object("metadata", "Optional metadata")
            .string("agent_id", "Optional scoping agent id").default_str("").length(0, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString content = args["content"].toString();
            const QString type = args["memory_type"].toString("general");
            const QJsonObject meta = args["metadata"].toObject();
            const QString agent_id = args["agent_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, content, type, meta, agent_id](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::memory_stored, holder,
                                      [resolve, holder](bool ok, QString msg) {
                                          resolve(ok ? ToolResult::ok(msg) : ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->store_memory(content, type, meta, agent_id);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "recall_agent_memories";
        t.description = "Recall stored memories matching a query.";
        t.category = "agents";
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 1024)
            .string("memory_type", "Filter by memory bucket").default_str("").length(0, 64)
            .integer("limit", "Max results").default_int(10).between(1, 100)
            .string("agent_id", "Optional scoping agent id").default_str("").length(0, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QString type = args["memory_type"].toString();
            const int limit = args["limit"].toInt(10);
            const QString agent_id = args["agent_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, type, limit, agent_id](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::memories_recalled, holder,
                                      [resolve, holder](QJsonArray arr) {
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->recall_memories(query, type, limit, agent_id);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "search_agent_knowledge";
        t.description = "Search the agent knowledge base (vector store).";
        t.category = "agents";
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 1024)
            .integer("limit", "Max results").default_int(10).between(1, 100)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const int limit = args["limit"].toInt(10);
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, limit](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::knowledge_results, holder,
                                      [resolve, holder](QJsonArray arr) {
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->search_knowledge(query, limit);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 24-30. AgentConfigRepository CRUD (sync) ────────────────────────
    {
        ToolDef t;
        t.name = "list_agent_configs";
        t.description = "List saved agent configs (agents/teams/etc) with metadata.";
        t.category = "agents";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto r = AgentConfigRepository::instance().list_all();
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            QJsonArray arr;
            for (const auto& c : r.value())
                arr.append(config_to_json(c));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_agent_configs_by_category";
        t.description = "List saved configs filtered by category (e.g. 'agent', 'team').";
        t.category = "agents";
        t.input_schema = ToolSchemaBuilder()
            .string("category", "Category filter").required().length(1, 64)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto r = AgentConfigRepository::instance().list_by_category(args["category"].toString());
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            QJsonArray arr;
            for (const auto& c : r.value())
                arr.append(config_to_json(c));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_agent_config";
        t.description = "Get a saved agent config by id.";
        t.category = "agents";
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Config id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto r = AgentConfigRepository::instance().get(args["id"].toString());
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok_data(config_to_json(r.value()));
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_active_agent_config";
        t.description = "Get the currently-active agent config.";
        t.category = "agents";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto r = AgentConfigRepository::instance().get_active();
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok_data(config_to_json(r.value()));
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "save_agent_config";
        t.description = "Upsert an agent config (id, name, description, category, config_json).";
        t.category = "agents";
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Config id (empty = generated)").default_str("").length(0, 128)
            .string("name", "Display name").required().length(1, 128)
            .string("description", "Description").default_str("").length(0, 512)
            .string("category", "Category: agent / team / workflow").default_str("agent").length(1, 64)
            .string("config_json", "Serialized config payload").required().length(1, 65536)
            .boolean("is_default", "Whether this is the default config").default_bool(false)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            AgentConfig c;
            c.id = args["id"].toString();
            if (c.id.isEmpty())
                c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            c.name = args["name"].toString();
            c.description = args["description"].toString();
            c.category = args["category"].toString("agent");
            c.config_json = args["config_json"].toString();
            c.is_default = args["is_default"].toBool(false);
            const auto r = AgentConfigRepository::instance().save(c);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok("Config saved", QJsonObject{{"id", c.id}});
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "delete_agent_config";
        t.description = "Delete an agent config by id.";
        t.category = "agents";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Config id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const auto r = AgentConfigRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok("Config deleted", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_active_agent_config";
        t.description = "Mark an agent config as the active one (exclusive).";
        t.category = "agents";
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Config id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const auto r = AgentConfigRepository::instance().set_active(id);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok("Active config set", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── 31-34. WorkflowRepository CRUD ─────────────────────────────────
    {
        ToolDef t;
        t.name = "list_workflows";
        t.description = "List saved workflow summaries (no node/edge detail).";
        t.category = "agents";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto r = WorkflowRepository::instance().list_all();
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            QJsonArray arr;
            for (const auto& w : r.value()) {
                arr.append(QJsonObject{
                    {"id", w.id},
                    {"name", w.name},
                    {"description", w.description},
                    {"status", w.status},
                    {"created_at", w.created_at},
                    {"updated_at", w.updated_at},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_workflow";
        t.description = "Load a workflow (full definition: nodes + edges + metadata) by id.";
        t.category = "agents";
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Workflow id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto r = WorkflowRepository::instance().load(args["id"].toString());
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok_data(workflow_to_json(r.value()));
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "save_workflow";
        t.description = "Upsert a workflow. Accepts the full WorkflowDef JSON shape returned by get_workflow.";
        t.category = "agents";
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .object("workflow", "Workflow definition: id, name, description, nodes[], edges[], status, static_data").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto wf = json_to_workflow(args["workflow"].toObject());
            if (wf.id.isEmpty())
                wf.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const auto r = WorkflowRepository::instance().save(wf);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok("Workflow saved",
                                   QJsonObject{{"id", wf.id},
                                               {"nodes", static_cast<int>(wf.nodes.size())},
                                               {"edges", static_cast<int>(wf.edges.size())}});
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "delete_workflow";
        t.description = "Delete a workflow and its nodes/edges by id.";
        t.category = "agents";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Workflow id").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString();
            const auto r = WorkflowRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail(QString::fromStdString(r.error()));
            return ToolResult::ok("Workflow deleted", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }


    // ── Memory repo variants (different backend from store_/recall_) ──
    {
        ToolDef t;
        t.name = "save_agent_memory_repo";
        t.description = "Save a memory to the agent repository (Python-side memory store).";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("content", "Memory content").required().length(1, 16384)
            .string("agent_id", "Scoping agent id").default_str("").length(0, 128)
            .object("options", "Implementation-specific options (e.g. priority, tags)")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString content = args["content"].toString();
            const QString agent_id = args["agent_id"].toString();
            const QJsonObject options = args["options"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, content, agent_id, options](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::memory_stored, holder,
                                      [resolve, holder](bool ok, QString msg) {
                                          resolve(ok ? ToolResult::ok(msg) : ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->save_memory_repo(content, agent_id, options);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "search_agent_memories_repo";
        t.description = "Search the agent memory repository (Python-side memory store).";
        t.category = "agents";
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 1024)
            .string("agent_id", "Scoping agent id").default_str("").length(0, 128)
            .integer("limit", "Max results").default_int(10).between(1, 100)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QString agent_id = args["agent_id"].toString();
            const int limit = args["limit"].toInt(10);
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, agent_id, limit](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::memories_recalled, holder,
                                      [resolve, holder](QJsonArray arr) {
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->search_memories_repo(query, agent_id, limit);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── Session management ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "save_agent_session";
        t.description = "Save (upsert) an agent session record.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .object("session_data", "Session payload (id, title, messages[], etc.)").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject data = args["session_data"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, data](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::session_saved, holder,
                                      [resolve, holder](bool ok) {
                                          resolve(ok ? ToolResult::ok("Session saved")
                                                     : ToolResult::fail("Session save failed"));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->save_session(data);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_agent_session";
        t.description = "Load an agent session by id.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("session_id", "Session id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString id = args["session_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, id](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::session_loaded, holder,
                                      [resolve, holder](QJsonObject s) {
                                          resolve(ToolResult::ok_data(s));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->get_session(id);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "add_agent_session_message";
        t.description = "Append a message (role + content) to an existing agent session.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("session_id", "Session id").required().length(1, 128)
            .string("role", "Message role: user / assistant / system").required().length(1, 32)
            .string("content", "Message content").required().length(1, 32768)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sid = args["session_id"].toString();
            const QString role = args["role"].toString();
            const QString content = args["content"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sid, role, content](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::session_saved, holder,
                                      [resolve, holder](bool ok) {
                                          resolve(ok ? ToolResult::ok("Message appended")
                                                     : ToolResult::fail("Message append failed"));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->add_session_message(sid, role, content);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── Paper trading (agent-side, distinct from PaperTradingTools) ────
    // All three emit the same `trade_executed` signal — concurrent calls
    // can cross results. Caller is responsible for serialising calls per
    // portfolio_id if precise ordering matters.
    {
        ToolDef t;
        t.name = "agent_paper_execute_trade";
        t.description = "Execute a paper trade via the agent backend (buy/sell qty @ price).";
        t.category = "agents";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("portfolio_id", "Portfolio id").required().length(1, 128)
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("action", "buy / sell").required().enums({"buy", "sell"})
            .number("quantity", "Quantity to trade").required().min(0)
            .number("price", "Execution price").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString pid = args["portfolio_id"].toString();
            const QString sym = args["symbol"].toString();
            const QString action = args["action"].toString();
            const double qty = args["quantity"].toDouble();
            const double px = args["price"].toDouble();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, pid, sym, action, qty, px](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::trade_executed, holder,
                                      [resolve, holder](QJsonObject r) {
                                          resolve(ToolResult::ok_data(r));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->paper_execute_trade(pid, sym, action, qty, px);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "agent_paper_get_portfolio";
        t.description = "Get an agent-side paper-trading portfolio snapshot by id.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("portfolio_id", "Portfolio id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString pid = args["portfolio_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, pid](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::trade_executed, holder,
                                      [resolve, holder](QJsonObject r) {
                                          resolve(ToolResult::ok_data(r));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->paper_get_portfolio(pid);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "agent_paper_get_positions";
        t.description = "Get current positions for an agent-side paper-trading portfolio.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("portfolio_id", "Portfolio id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString pid = args["portfolio_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, pid](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::trade_executed, holder,
                                      [resolve, holder](QJsonObject r) {
                                          resolve(ToolResult::ok_data(r));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->paper_get_positions(pid);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── Trade decisions ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "save_agent_trade_decision";
        t.description = "Persist a trade decision payload. Fire-and-forget — errors surface via the next list_agent_trade_decisions call.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .object("decision", "Trade decision payload (symbol, action, rationale, competition_id, etc.)").required()
            .build();
        // save_trade_decision only emits error_occurred on failure; nothing
        // on success. Bridge by listening briefly for error_occurred —
        // resolve ok after a short grace period if nothing fires.
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject decision = args["decision"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, decision](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString ctxName, QString msg) {
                                          if (ctxName != "save_trade_decision")
                                              return;
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    QTimer::singleShot(2000, holder, [resolve, holder]() {
                        resolve(ToolResult::ok("Trade decision saved (fire-and-forget)"));
                        holder->deleteLater();
                    });
                    svc->save_trade_decision(decision);
                });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_agent_trade_decisions";
        t.description = "List saved trade decisions, optionally filtered by competition id and/or model name.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.input_schema = ToolSchemaBuilder()
            .string("competition_id", "Filter by competition id").default_str("").length(0, 128)
            .string("model_name", "Filter by model name").default_str("").length(0, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString comp = args["competition_id"].toString();
            const QString model = args["model_name"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, comp, model](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::trade_decisions_loaded, holder,
                                      [resolve, holder](QJsonArray arr) {
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString ctxName, QString msg) {
                                          if (ctxName != "get_decisions")
                                              return;
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->get_trade_decisions(comp, model);
                });
        };
        tools.push_back(std::move(t));
    }
}
} // namespace fincept::mcp::tools
