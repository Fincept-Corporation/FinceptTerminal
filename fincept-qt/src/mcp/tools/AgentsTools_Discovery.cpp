// src/mcp/tools/AgentsTools_Discovery.cpp
//
// Discovery / introspection / cache tools: list_agents, discover_agents,
// list_agent_tools, list_agent_models, get_agent_system_info, agent cache ops.
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

void agents_internal::register_discovery_tools(std::vector<ToolDef>& tools) {
    // ── 1. list_agents ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_agents";
        t.description = "List cached discovered agents (id, name, category, capabilities, provider).";
        t.category = "agents";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& a : services::AgentService::instance().cached_agents())
                arr.append(agent_to_json(a));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── 2. discover_agents ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "discover_agents";
        t.description = "Force a refresh of the agent registry from the Python backend.";
        t.category = "agents";
        t.default_timeout_ms = 60000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::agents_discovered, holder,
                                      [resolve, holder](QVector<services::AgentInfo> agents,
                                                        QVector<services::AgentCategory> cats) {
                                          QJsonArray a_arr;
                                          for (const auto& a : agents)
                                              a_arr.append(agent_to_json(a));
                                          QJsonArray c_arr;
                                          for (const auto& c : cats)
                                              c_arr.append(QJsonObject{{"name", c.name}, {"count", c.count}});
                                          resolve(ToolResult::ok("Discovery complete",
                                                                  QJsonObject{{"agents", a_arr}, {"categories", c_arr}}));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->discover_agents();
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 3. list_agent_tools ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_agent_tools";
        t.description = "List agent-framework tools (web search, calculator, etc) grouped by category.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::tools_loaded, holder,
                                      [resolve, holder](services::AgentToolsInfo info) {
                                          QJsonArray cats;
                                          for (const auto& c : info.categories)
                                              cats.append(c);
                                          resolve(ToolResult::ok_data(
                                              QJsonObject{{"tools", info.tools},
                                                          {"categories", cats},
                                                          {"total_count", info.total_count}}));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->list_tools();
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 4. list_agent_models ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_agent_models";
        t.description = "List LLM providers / models known to the agent framework.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::models_loaded, holder,
                                      [resolve, holder](services::AgentModelsInfo info) {
                                          QJsonArray provs;
                                          for (const auto& p : info.providers)
                                              provs.append(p);
                                          resolve(ToolResult::ok_data(
                                              QJsonObject{{"providers", provs}, {"count", info.count}}));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->list_models();
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 5. get_agent_system_info ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_agent_system_info";
        t.description = "Get agent framework version, capabilities, features.";
        t.category = "agents";
        t.default_timeout_ms = 30000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::AgentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::AgentService::system_info_loaded, holder,
                                      [resolve, holder](services::AgentSystemInfo info) {
                                          QJsonArray feats;
                                          for (const auto& f : info.features)
                                              feats.append(f);
                                          resolve(ToolResult::ok_data(
                                              QJsonObject{{"version", info.version},
                                                          {"framework", info.framework},
                                                          {"capabilities", info.capabilities},
                                                          {"features", feats}}));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::AgentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->get_system_info();
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 6. get_agent_cache_stats ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_agent_cache_stats";
        t.description = "Get cached agent count and response-cache statistics.";
        t.category = "agents";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& svc = services::AgentService::instance();
            return ToolResult::ok_data(QJsonObject{
                {"cached_agents", svc.cached_agent_count()},
                {"cache_stats", svc.get_cache_stats()},
            });
        };
        tools.push_back(std::move(t));
    }


    // ── 35-37. Cache ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "clear_agent_cache";
        t.description = "Clear all agent caches (agents, tools, models, system_info).";
        t.category = "agents";
        t.is_destructive = true;
        t.handler = [](const QJsonObject&) -> ToolResult {
            services::AgentService::instance().clear_cache();
            return ToolResult::ok("Agent cache cleared");
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "clear_agent_response_cache";
        t.description = "Clear only the per-query agent response cache.";
        t.category = "agents";
        t.is_destructive = true;
        t.handler = [](const QJsonObject&) -> ToolResult {
            services::AgentService::instance().clear_response_cache();
            return ToolResult::ok("Agent response cache cleared");
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
