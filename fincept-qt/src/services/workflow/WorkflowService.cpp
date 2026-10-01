#include "services/workflow/WorkflowService.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/workflow/WorkflowExecutor.h"
#include "storage/repositories/WorkflowRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

namespace fincept::workflow {

WorkflowService& WorkflowService::instance() {
    static WorkflowService s;
    return s;
}

WorkflowService::WorkflowService() : QObject(nullptr) {}

void WorkflowService::save_workflow(const WorkflowDef& wf) {
    auto r = WorkflowRepository::instance().save(wf);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Save failed: %1").arg(QString::fromStdString(r.error())));
        emit save_failed(QString::fromStdString(r.error()));
        return;
    }
    emit workflow_saved(wf.id);
    LOG_INFO("WorkflowService", QString("Saved workflow: %1").arg(wf.name));
}

void WorkflowService::load_workflow(const QString& id) {
    auto r = WorkflowRepository::instance().load(id);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Load failed: %1").arg(QString::fromStdString(r.error())));
        emit workflow_load_failed(QString::fromStdString(r.error()));
        return;
    }
    emit workflow_loaded(r.value());
}

void WorkflowService::list_workflows() {
    auto r = WorkflowRepository::instance().list_all();
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("List failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }

    // Convert WorkflowRow summaries to lightweight WorkflowDefs (no nodes/edges)
    QVector<WorkflowDef> workflows;
    for (const auto& row : r.value()) {
        WorkflowDef wf;
        wf.id = row.id;
        wf.name = row.name;
        wf.description = row.description;
        wf.created_at = row.created_at;
        wf.updated_at = row.updated_at;

        if (row.status == "idle")
            wf.status = WorkflowStatus::Idle;
        else if (row.status == "running")
            wf.status = WorkflowStatus::Running;
        else if (row.status == "completed")
            wf.status = WorkflowStatus::Completed;
        else if (row.status == "error")
            wf.status = WorkflowStatus::Error;
        else
            wf.status = WorkflowStatus::Draft;

        workflows.append(wf);
    }
    emit workflows_listed(workflows);
}

void WorkflowService::delete_workflow(const QString& id) {
    auto r = WorkflowRepository::instance().remove(id);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Delete failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    emit workflow_deleted(id);
    LOG_INFO("WorkflowService", QString("Deleted workflow: %1").arg(id));
}

// ── Import/Export ──────────────────────────────────────────────────────

Result<WorkflowDef> WorkflowService::import_from_json(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Result<WorkflowDef>::err("Failed to open file: " + path.toStdString());

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return Result<WorkflowDef>::err("Invalid JSON workflow file");

    QJsonObject obj = doc.object();
    WorkflowDef wf;
    wf.id = obj.value("id").toString();
    wf.name = obj.value("name").toString("Imported Workflow");
    wf.description = obj.value("description").toString();

    // The canvas keys nodes and edges by id: a missing or repeated id would make
    // two nodes collapse into one, so repair them on the way in.
    QSet<QString> seen_node_ids;
    QSet<QString> seen_edge_ids;

    for (const auto& nv : obj.value("nodes").toArray()) {
        QJsonObject no = nv.toObject();
        NodeDef nd;
        nd.id = no.value("id").toString();
        if (nd.id.isEmpty() || seen_node_ids.contains(nd.id))
            nd.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        seen_node_ids.insert(nd.id);
        nd.type = no.value("type").toString();
        nd.name = no.value("name").toString();
        nd.type_version = no.value("typeVersion").toInt(1);
        nd.x = no.value("position").toObject().value("x").toDouble();
        nd.y = no.value("position").toObject().value("y").toDouble();
        nd.parameters = no.value("parameters").toObject();
        nd.disabled = no.value("disabled").toBool();
        nd.continue_on_fail = no.value("continueOnFail").toBool();
        nd.retry_on_fail = no.value("retryOnFail").toBool();
        nd.max_tries = no.value("maxTries").toInt(1);
        wf.nodes.append(nd);
    }

    for (const auto& ev : obj.value("edges").toArray()) {
        QJsonObject eo = ev.toObject();
        EdgeDef ed;
        ed.id = eo.value("id").toString();
        if (ed.id.isEmpty() || seen_edge_ids.contains(ed.id))
            ed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        seen_edge_ids.insert(ed.id);
        ed.source_node = eo.value("source").toString();
        ed.target_node = eo.value("target").toString();
        ed.source_port = eo.value("sourceHandle").toString();
        ed.target_port = eo.value("targetHandle").toString();
        wf.edges.append(ed);
    }

    return Result<WorkflowDef>::ok(std::move(wf));
}

Result<void> WorkflowService::export_to_json(const WorkflowDef& wf, const QString& path) {
    QJsonObject obj;
    obj["id"] = wf.id;
    obj["name"] = wf.name;
    obj["description"] = wf.description;

    QJsonArray nodes_arr;
    for (const auto& nd : wf.nodes) {
        QJsonObject no;
        no["id"] = nd.id;
        no["type"] = nd.type;
        no["name"] = nd.name;
        no["typeVersion"] = nd.type_version;
        no["position"] = QJsonObject{{"x", nd.x}, {"y", nd.y}};
        no["parameters"] = nd.parameters;
        no["disabled"] = nd.disabled;
        no["continueOnFail"] = nd.continue_on_fail;
        no["retryOnFail"] = nd.retry_on_fail;
        no["maxTries"] = nd.max_tries;
        nodes_arr.append(no);
    }
    obj["nodes"] = nodes_arr;

    QJsonArray edges_arr;
    for (const auto& ed : wf.edges) {
        QJsonObject eo;
        eo["id"] = ed.id;
        eo["source"] = ed.source_node;
        eo["target"] = ed.target_node;
        eo["sourceHandle"] = ed.source_port;
        eo["targetHandle"] = ed.target_port;
        edges_arr.append(eo);
    }
    obj["edges"] = edges_arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return Result<void>::err("Failed to write file: " + path.toStdString());

    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size())
        return Result<void>::err("Failed to write file (disk full?): " + path.toStdString());
    LOG_INFO("WorkflowService", QString("Exported workflow to: %1").arg(path));
    return Result<void>::ok();
}

// ── Execution ──────────────────────────────────────────────────────────

namespace {

// Node types that place, modify or cancel orders and carry a `mode` select
// (paper | live). The executors treat ANY mode other than the exact string
// "paper" as live — this list is deliberately matched the same way.
const QSet<QString>& wf_moded_order_types() {
    static const QSet<QString> k = {
        QStringLiteral("trading.place_order"),   QStringLiteral("trading.cancel_order"),
        QStringLiteral("trading.modify_order"),  QStringLiteral("trading.close_position"),
        QStringLiteral("trading.bracket_order"), QStringLiteral("trading.trailing_stop"),
        QStringLiteral("trading.scale_in"),
    };
    return k;
}

// Order nodes that have no paper mode at all: they always act on the broker.
const QSet<QString>& wf_live_only_order_types() {
    static const QSet<QString> k = {
        QStringLiteral("trading.smart_order"),
        QStringLiteral("trading.cancel_all"),
        QStringLiteral("trading.close_all"),
    };
    return k;
}

QString wf_variant_text(const QJsonValue& v) {
    return v.toVariant().toString();
}

// True when the MCP tool named by an mcp.tool_call node is a declared-destructive
// internal tool (anything that mutates state: orders, transfers, writes).
bool wf_mcp_tool_is_destructive(const QString& tool) {
    if (tool.isEmpty() || tool.contains(QLatin1String("__")))
        return false; // external tools carry no destructiveness metadata
    for (const auto& t : mcp::McpService::instance().get_all_tools()) {
        if (t.is_internal && t.name == tool)
            return t.is_destructive;
    }
    return false;
}

void wf_collect_live_rows(const WorkflowDef& wf, const QString& via, int depth, QSet<QString>& visited,
                          QStringList& rows) {
    for (const auto& nd : wf.nodes) {
        if (nd.disabled)
            continue;

        const QString label = nd.name.isEmpty() ? nd.type : nd.name;
        QString detail;

        if (wf_moded_order_types().contains(nd.type) || wf_live_only_order_types().contains(nd.type)) {
            // Same rule as the executors: only the exact string "paper" is paper
            // ("Paper", "" or any other value is routed to the real broker).
            const QString mode = nd.parameters.value(QStringLiteral("mode")).toString(QStringLiteral("paper"));
            if (wf_moded_order_types().contains(nd.type) && mode == QLatin1String("paper"))
                continue;

            const QString symbol = nd.parameters.value(QStringLiteral("symbol")).toString();
            const QString side = nd.parameters.value(QStringLiteral("side")).toString();
            QString qty = wf_variant_text(nd.parameters.value(QStringLiteral("quantity")));
            if (qty.isEmpty())
                qty = wf_variant_text(nd.parameters.value(QStringLiteral("total_quantity")));
            if (qty.isEmpty())
                qty = wf_variant_text(nd.parameters.value(QStringLiteral("position_size")));
            const QString broker = nd.parameters.value(QStringLiteral("broker")).toString();

            if (!symbol.isEmpty())
                detail += QStringLiteral("  %1").arg(symbol);
            if (!side.isEmpty())
                detail += QStringLiteral("  %1").arg(side.toUpper());
            if (!qty.isEmpty())
                detail += QStringLiteral(" x%1").arg(qty);
            if (!broker.isEmpty())
                detail += QStringLiteral("  → %1").arg(broker);
        } else if (nd.type == QLatin1String("mcp.tool_call")) {
            const QString tool = nd.parameters.value(QStringLiteral("tool")).toString().trimmed();
            if (!wf_mcp_tool_is_destructive(tool))
                continue;
            detail = QStringLiteral("  %1").arg(WorkflowService::tr("destructive MCP tool: %1").arg(tool));
        } else if (nd.type == QLatin1String("control.execute_workflow")) {
            const QString sub_id = nd.parameters.value(QStringLiteral("workflow_id")).toString().trimmed();
            if (sub_id.isEmpty() || depth >= 5 || visited.contains(sub_id))
                continue;
            visited.insert(sub_id);
            auto loaded = WorkflowRepository::instance().load(sub_id);
            if (loaded.is_ok()) {
                const WorkflowDef& sub = loaded.value();
                const QString sub_via = via.isEmpty() ? sub.name : via + QStringLiteral(" › ") + sub.name;
                wf_collect_live_rows(sub, sub_via, depth + 1, visited, rows);
            }
            continue;
        } else {
            continue;
        }

        QString row = QStringLiteral("  • %1  [%2]").arg(label, nd.type) + detail;
        if (!via.isEmpty())
            row += QStringLiteral("   ") + WorkflowService::tr("(in sub-workflow: %1)").arg(via);
        rows << row;
    }
}

} // namespace

QStringList WorkflowService::live_order_rows(const WorkflowDef& wf) {
    QStringList rows;
    QSet<QString> visited;
    if (!wf.id.isEmpty())
        visited.insert(wf.id); // a workflow calling itself is reported once, not recursed
    wf_collect_live_rows(wf, {}, 0, visited, rows);
    return rows;
}

WorkflowExecutor* WorkflowService::make_executor() {
    if (executor_) {
        if (executor_->is_running()) {
            // Replacing a live executor orphans its in-flight nodes (orders included)
            // and strands the editor in "running" — refuse; the caller can Stop first.
            LOG_WARN("WorkflowService", "Execution already in progress — ignoring new run request");
            return nullptr;
        }
        executor_->deleteLater();
        executor_ = nullptr;
    }

    auto* ex = new WorkflowExecutor(this);
    executor_ = ex;

    connect(ex, &WorkflowExecutor::execution_started, this, &WorkflowService::execution_started);
    connect(ex, &WorkflowExecutor::node_started, this, &WorkflowService::node_execution_started);
    connect(ex, &WorkflowExecutor::node_completed, this, &WorkflowService::node_execution_completed);
    connect(ex, &WorkflowExecutor::execution_finished, this, [this, ex](const WorkflowExecutionResult& result) {
        emit execution_finished(result);
        // Retire only THIS run's executor — a newer one may already be current.
        ex->deleteLater();
        if (executor_ == ex)
            executor_ = nullptr;
    });
    return ex;
}

void WorkflowService::execute_workflow(const WorkflowDef& wf) {
    if (auto* ex = make_executor())
        ex->execute(wf);
}

void WorkflowService::execute_from_node(const WorkflowDef& wf, const QString& start_node_id) {
    if (auto* ex = make_executor())
        ex->execute_from(wf, start_node_id);
}

void WorkflowService::stop_execution() {
    if (executor_)
        executor_->stop();
}

bool WorkflowService::is_executing() const {
    return executor_ && executor_->is_running();
}

} // namespace fincept::workflow
