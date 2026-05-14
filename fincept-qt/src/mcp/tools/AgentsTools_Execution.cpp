// src/mcp/tools/AgentsTools_Execution.cpp
//
// Execution path: run_agent / run_agent_structured / route_agent_query /
// execute_multi_agent_query, financial workflows (stock / portfolio / risk),
// run_agent_team / run_agent_workflow, streaming agent, execute_routed_agent_query.
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

void agents_internal::register_execution_tools(std::vector<ToolDef>& tools) {
    // ── 7. run_agent ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_agent";
        t.description = "Run an agent with a query and optional config. Non-streaming.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Natural-language query for the agent").required().length(1, 8192)
            .object("config", "Optional agent config override (model, tools, etc.)")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QJsonObject config = args["config"].toObject();
            dispatch_agent_run(
                [query, config]() {
                    return services::AgentService::instance().run_agent(query, config);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── 8. run_agent_structured ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_agent_structured";
        t.description = "Run an agent and constrain its output to a named Pydantic model on the Python side.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Natural-language query for the agent").required().length(1, 8192)
            .string("output_model", "Pydantic model name on the Python side").required().length(1, 128)
            .object("config", "Optional agent config override")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QString output_model = args["output_model"].toString();
            const QJsonObject config = args["config"].toObject();
            dispatch_agent_run(
                [query, config, output_model]() {
                    return services::AgentService::instance().run_agent_structured(query, config, output_model);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── 9. route_agent_query ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "route_agent_query";
        t.description = "Get the routing decision (agent_id, intent, confidence) for a query without executing it.";
        t.category = "agents";
        t.default_timeout_ms = 60000;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Query to route").required().length(1, 8192)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query](auto resolve) {
                    const QString req_id = svc->route_query(query);
                    if (req_id.isEmpty()) {
                        resolve(ToolResult::fail("Failed to start routing"));
                        return;
                    }
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::routing_result, holder,
                                      [req_id, resolve, holder](services::RoutingResult r) {
                                          if (r.request_id != req_id)
                                              return;
                                          resolve(r.success ? ToolResult::ok_data(routing_to_json(r))
                                                            : ToolResult::fail("Routing failed"));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 10. execute_multi_agent_query ───────────────────────────────────
    {
        ToolDef t;
        t.name = "execute_multi_agent_query";
        t.description = "Run a query across multiple agents in parallel; optionally aggregate the responses.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Query to fan out").required().length(1, 8192)
            .boolean("aggregate", "Combine responses into a single result").default_bool(true)
            .object("config", "Optional agent config override")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const bool aggregate = args["aggregate"].toBool(true);
            const QJsonObject config = args["config"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, aggregate, config](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::multi_query_result, holder,
                                      [resolve, holder](QJsonObject result) {
                                          resolve(ToolResult::ok_data(result));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->execute_multi_query(query, aggregate, config);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 11-14. Financial workflows ──────────────────────────────────────
    // Note: these emit agent_result via the AgentExecutionResult signal but
    // do NOT return a request_id (they kick off internally). We use a
    // single-shot connection that resolves on the first agent_result.
    auto bridge_workflow_no_reqid =
        [](auto kick, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, kick = std::move(kick)](auto resolve) mutable {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::agent_result, holder,
                                      [resolve, holder](services::AgentExecutionResult r) {
                                          if (r.success)
                                              resolve(ToolResult::ok(r.response.isEmpty() ? "OK" : r.response,
                                                                      result_to_json(r)));
                                          else
                                              resolve(ToolResult::fail(r.error.isEmpty() ? "Run failed" : r.error));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    kick();
                });
        };

    {
        ToolDef t;
        t.name = "run_stock_analysis_agent";
        t.description = "Run a full agent-driven stock analysis for a ticker symbol.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol (e.g. AAPL)").required().length(1, 16)
            .object("config", "Optional agent config override")
            .build();
        t.async_handler = [bridge_workflow_no_reqid](const QJsonObject& args, ToolContext ctx,
                                                      std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString symbol = args["symbol"].toString();
            const QJsonObject config = args["config"].toObject();
            bridge_workflow_no_reqid(
                [symbol, config]() { services::AgentService::instance().run_stock_analysis(symbol, config); },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "run_portfolio_rebalancing_agent";
        t.description = "Run agent-driven portfolio rebalancing.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .object("portfolio_data", "Portfolio snapshot (holdings, target weights, constraints)")
            .build();
        t.async_handler = [bridge_workflow_no_reqid](const QJsonObject& args, ToolContext ctx,
                                                      std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject data = args["portfolio_data"].toObject();
            bridge_workflow_no_reqid(
                [data]() { services::AgentService::instance().run_portfolio_rebalancing(data); },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "run_risk_assessment_agent";
        t.description = "Run agent-driven portfolio risk assessment.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .object("portfolio_data", "Portfolio snapshot")
            .build();
        t.async_handler = [bridge_workflow_no_reqid](const QJsonObject& args, ToolContext ctx,
                                                      std::shared_ptr<QPromise<ToolResult>> promise) {
            const QJsonObject data = args["portfolio_data"].toObject();
            bridge_workflow_no_reqid(
                [data]() { services::AgentService::instance().run_risk_assessment(data); },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "run_portfolio_analysis_agent";
        t.description = "Run a typed portfolio analysis (e.g. 'attribution', 'exposure', 'sector').";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("analysis_type", "Analysis type identifier").required().length(1, 64)
            .object("portfolio_summary", "Portfolio summary payload")
            .build();
        t.async_handler = [bridge_workflow_no_reqid](const QJsonObject& args, ToolContext ctx,
                                                      std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString type = args["analysis_type"].toString();
            const QJsonObject summary = args["portfolio_summary"].toObject();
            bridge_workflow_no_reqid(
                [type, summary]() {
                    services::AgentService::instance().run_portfolio_analysis(type, summary);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── 15. run_agent_team ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_agent_team";
        t.description = "Execute a team of agents with a coordinator/mode (coordinate/route/collaborate).";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Query for the team").required().length(1, 8192)
            .object("team_config", "Team config: name, mode, members[], coordinator_model").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QJsonObject team_config = args["team_config"].toObject();
            dispatch_agent_run(
                [query, team_config]() {
                    return services::AgentService::instance().run_team(query, team_config);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── 16. run_agent_workflow ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "run_agent_workflow";
        t.description = "Execute a saved workflow by type with optional params.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("workflow_type", "Workflow type identifier").required().length(1, 128)
            .object("params", "Optional workflow params")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString type = args["workflow_type"].toString();
            const QJsonObject params = args["params"].toObject();
            dispatch_agent_run(
                [type, params]() {
                    return services::AgentService::instance().run_workflow(type, params);
                },
                std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }


    // ── Streaming agent (resolves on stream_done) ──────────────────────
    {
        ToolDef t;
        t.name = "run_agent_streaming";
        t.description = "Run an agent in streaming mode; resolves with the final assembled response when streaming completes.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Natural-language query").required().length(1, 8192)
            .object("config", "Optional agent config override")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QJsonObject config = args["config"].toObject();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, config](auto resolve) {
                    const QString req_id = svc->run_agent_streaming(query, config);
                    if (req_id.isEmpty()) {
                        resolve(ToolResult::fail("Failed to start streaming run"));
                        return;
                    }
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::agent_stream_done, holder,
                                      [req_id, resolve, holder](services::AgentExecutionResult r) {
                                          if (r.request_id != req_id)
                                              return;
                                          if (r.success)
                                              resolve(ToolResult::ok(r.response, result_to_json(r)));
                                          else
                                              resolve(ToolResult::fail(
                                                  r.error.isEmpty() ? "Streaming run failed" : r.error));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                });
        };
        tools.push_back(std::move(t));
    }

    // ── execute_routed_agent_query ────────────────────────────────────
    {
        ToolDef t;
        t.name = "execute_routed_agent_query";
        t.description = "Route a query and execute it on the chosen agent in one call. Optional session_id for context.";
        t.category = "agents";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultAgentTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Natural-language query").required().length(1, 8192)
            .object("config", "Optional config override")
            .string("session_id", "Optional session id for memory context").default_str("").length(0, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString query = args["query"].toString();
            const QJsonObject config = args["config"].toObject();
            const QString session_id = args["session_id"].toString();
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, query, config, session_id](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::agent_result, holder,
                                      [resolve, holder](services::AgentExecutionResult r) {
                                          if (r.success)
                                              resolve(ToolResult::ok(r.response, result_to_json(r)));
                                          else
                                              resolve(ToolResult::fail(
                                                  r.error.isEmpty() ? "Routed query failed" : r.error));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->execute_routed_query(query, config, session_id);
                });
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
