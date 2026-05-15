// src/mcp/tools/AgentsTools_internal.h
//
// Private header — included only by AgentsTools*.cpp TUs. Provides the
// per-section registration entry points used by AgentsTools.cpp to assemble
// the full agents-tool catalog, plus the JSON converters and the shared
// dispatch_agent_run kicker. All helpers are `inline`/template so the header
// is safe to include from multiple TUs.

#pragma once

#include "mcp/AsyncDispatch.h"
#include "mcp/McpTypes.h"
#include "screens/node_editor/NodeEditorTypes.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/AgentConfigRepository.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPromise>
#include <QString>

#include <memory>
#include <vector>

namespace fincept::mcp::tools {

namespace agents_internal {

inline constexpr const char* TAG = "AgentsTools";

// Agent runs hit Python LangGraph chains; allow up to 5 minutes by default.
inline constexpr int kDefaultAgentTimeoutMs = 300000;

inline QJsonObject agent_to_json(const services::AgentInfo& a) {
    QJsonArray caps;
    for (const auto& c : a.capabilities)
        caps.append(c);
    return QJsonObject{
        {"id", a.id},
        {"name", a.name},
        {"description", a.description},
        {"category", a.category},
        {"capabilities", caps},
        {"provider", a.provider},
        {"version", a.version},
    };
}

inline QJsonObject result_to_json(const services::AgentExecutionResult& r) {
    return QJsonObject{
        {"success", r.success},
        {"response", r.response},
        {"error", r.error},
        {"execution_time_ms", r.execution_time_ms},
        {"request_id", r.request_id},
    };
}

inline QJsonObject routing_to_json(const services::RoutingResult& r) {
    QJsonArray kws;
    for (const auto& k : r.matched_keywords)
        kws.append(k);
    return QJsonObject{
        {"success", r.success},
        {"agent_id", r.agent_id},
        {"intent", r.intent},
        {"confidence", r.confidence},
        {"matched_keywords", kws},
        {"config", r.config},
        {"request_id", r.request_id},
    };
}

inline QJsonObject plan_to_json(const services::ExecutionPlan& p) {
    QJsonArray steps;
    for (const auto& s : p.steps) {
        QJsonArray deps;
        for (const auto& d : s.dependencies)
            deps.append(d);
        steps.append(QJsonObject{
            {"id", s.id},
            {"name", s.name},
            {"step_type", s.step_type},
            {"config", s.config},
            {"dependencies", deps},
            {"status", s.status},
            {"result", s.result},
            {"error", s.error},
        });
    }
    return QJsonObject{
        {"id", p.id},
        {"name", p.name},
        {"description", p.description},
        {"status", p.status},
        {"is_complete", p.is_complete},
        {"has_failed", p.has_failed},
        {"request_id", p.request_id},
        {"steps", steps},
    };
}

inline QJsonObject config_to_json(const AgentConfig& c) {
    return QJsonObject{
        {"id", c.id},
        {"name", c.name},
        {"description", c.description},
        {"category", c.category},
        {"config_json", c.config_json},
        {"is_default", c.is_default},
        {"is_active", c.is_active},
        {"created_at", c.created_at},
        {"updated_at", c.updated_at},
    };
}

// Generic kicker: invoke a member fn that returns request_id, bridge
// agent_result(req_id) signal back to the promise.
template <typename KickFn>
inline void dispatch_agent_run(KickFn&& kick, ToolContext ctx,
                               std::shared_ptr<QPromise<ToolResult>> promise) {
    auto* svc = &services::AgentService::instance();
    AsyncDispatch::callback_to_promise(
        svc, std::move(ctx), promise,
        [svc, kick = std::forward<KickFn>(kick)](auto resolve) mutable {
            const QString req_id = kick();
            if (req_id.isEmpty()) {
                resolve(ToolResult::fail("Failed to start agent run"));
                return;
            }
            auto* holder = new QObject(svc);
            QObject::connect(svc, &services::AgentService::agent_result, holder,
                              [req_id, resolve, holder](services::AgentExecutionResult r) {
                                  if (r.request_id != req_id)
                                      return;
                                  if (r.success)
                                      resolve(ToolResult::ok(r.response.isEmpty() ? "OK" : r.response, result_to_json(r)));
                                  else
                                      resolve(ToolResult::fail(r.error.isEmpty() ? "Agent run failed" : r.error));
                                  holder->deleteLater();
                              });
            QObject::connect(svc, &services::AgentService::error_occurred, holder,
                              [resolve, holder](QString, QString msg) {
                                  resolve(ToolResult::fail(msg));
                                  holder->deleteLater();
                              });
        });
}

// ── WorkflowDef <-> JSON (consumed by save_workflow / get_workflow) ───
inline const char* workflow_status_str(workflow::WorkflowStatus s) {
    switch (s) {
        case workflow::WorkflowStatus::Draft:     return "draft";
        case workflow::WorkflowStatus::Idle:      return "idle";
        case workflow::WorkflowStatus::Running:   return "running";
        case workflow::WorkflowStatus::Completed: return "completed";
        case workflow::WorkflowStatus::Error:     return "error";
    }
    return "draft";
}

inline workflow::WorkflowStatus parse_workflow_status(const QString& s) {
    if (s == "idle")      return workflow::WorkflowStatus::Idle;
    if (s == "running")   return workflow::WorkflowStatus::Running;
    if (s == "completed") return workflow::WorkflowStatus::Completed;
    if (s == "error")     return workflow::WorkflowStatus::Error;
    return workflow::WorkflowStatus::Draft;
}

inline QJsonObject workflow_to_json(const workflow::WorkflowDef& wf) {
    QJsonArray nodes;
    for (const auto& n : wf.nodes) {
        nodes.append(QJsonObject{
            {"id", n.id},
            {"type", n.type},
            {"name", n.name},
            {"type_version", n.type_version},
            {"x", n.x},
            {"y", n.y},
            {"parameters", n.parameters},
            {"credentials", n.credentials},
            {"disabled", n.disabled},
            {"continue_on_fail", n.continue_on_fail},
            {"retry_on_fail", n.retry_on_fail},
            {"max_tries", n.max_tries},
        });
    }
    QJsonArray edges;
    for (const auto& e : wf.edges) {
        edges.append(QJsonObject{
            {"id", e.id},
            {"source_node", e.source_node},
            {"target_node", e.target_node},
            {"source_port", e.source_port},
            {"target_port", e.target_port},
            {"animated", e.animated},
        });
    }
    return QJsonObject{
        {"id", wf.id},
        {"name", wf.name},
        {"description", wf.description},
        {"status", workflow_status_str(wf.status)},
        {"static_data", wf.static_data},
        {"nodes", nodes},
        {"edges", edges},
        {"created_at", wf.created_at},
        {"updated_at", wf.updated_at},
    };
}

inline workflow::WorkflowDef json_to_workflow(const QJsonObject& j) {
    workflow::WorkflowDef wf;
    wf.id = j["id"].toString();
    wf.name = j["name"].toString("Untitled Workflow");
    wf.description = j["description"].toString();
    wf.status = parse_workflow_status(j["status"].toString());
    wf.static_data = j["static_data"].toObject();
    wf.created_at = j["created_at"].toString();
    wf.updated_at = j["updated_at"].toString();
    for (const auto& nv : j["nodes"].toArray()) {
        const auto no = nv.toObject();
        workflow::NodeDef n;
        n.id = no["id"].toString();
        n.type = no["type"].toString();
        n.name = no["name"].toString();
        n.type_version = no["type_version"].toInt(1);
        n.x = no["x"].toDouble();
        n.y = no["y"].toDouble();
        n.parameters = no["parameters"].toObject();
        n.credentials = no["credentials"].toObject();
        n.disabled = no["disabled"].toBool(false);
        n.continue_on_fail = no["continue_on_fail"].toBool(false);
        n.retry_on_fail = no["retry_on_fail"].toBool(false);
        n.max_tries = no["max_tries"].toInt(1);
        wf.nodes.append(n);
    }
    for (const auto& ev : j["edges"].toArray()) {
        const auto eo = ev.toObject();
        workflow::EdgeDef e;
        e.id = eo["id"].toString();
        e.source_node = eo["source_node"].toString();
        e.target_node = eo["target_node"].toString();
        e.source_port = eo["source_port"].toString();
        e.target_port = eo["target_port"].toString();
        e.animated = eo["animated"].toBool(false);
        wf.edges.append(e);
    }
    return wf;
}

// Per-section registration entry points. Each appends tool definitions to
// the passed-in vector. Implementations live in the matching .cpp file.
void register_discovery_tools(std::vector<ToolDef>& tools);
void register_execution_tools(std::vector<ToolDef>& tools);
void register_repos_tools(std::vector<ToolDef>& tools);

} // namespace agents_internal

} // namespace fincept::mcp::tools
