#include "services/workflow/WorkflowService.h"

#include "core/logging/Logger.h"
#include "services/workflow/WorkflowExecutor.h"
#include "storage/repositories/WorkflowRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

    for (const auto& nv : obj.value("nodes").toArray()) {
        QJsonObject no = nv.toObject();
        NodeDef nd;
        nd.id = no.value("id").toString();
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

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    LOG_INFO("WorkflowService", QString("Exported workflow to: %1").arg(path));
    return Result<void>::ok();
}

// ── Execution ──────────────────────────────────────────────────────────

void WorkflowService::execute_workflow(const WorkflowDef& wf) {
    if (executor_) {
        executor_->deleteLater();
        executor_ = nullptr;
    }

    executor_ = new WorkflowExecutor(this);

    connect(executor_, &WorkflowExecutor::execution_started, this, &WorkflowService::execution_started);
    connect(executor_, &WorkflowExecutor::node_started, this, &WorkflowService::node_execution_started);
    connect(executor_, &WorkflowExecutor::node_completed, this, &WorkflowService::node_execution_completed);
    connect(executor_, &WorkflowExecutor::execution_finished, this, [this](const WorkflowExecutionResult& result) {
        emit execution_finished(result);
        if (executor_) {
            executor_->deleteLater();
            executor_ = nullptr;
        }
    });

    executor_->execute(wf);
}

void WorkflowService::execute_from_node(const WorkflowDef& wf, const QString& start_node_id) {
    if (executor_) {
        executor_->deleteLater();
        executor_ = nullptr;
    }

    executor_ = new WorkflowExecutor(this);

    connect(executor_, &WorkflowExecutor::execution_started, this, &WorkflowService::execution_started);
    connect(executor_, &WorkflowExecutor::node_started, this, &WorkflowService::node_execution_started);
    connect(executor_, &WorkflowExecutor::node_completed, this, &WorkflowService::node_execution_completed);
    connect(executor_, &WorkflowExecutor::execution_finished, this, [this](const WorkflowExecutionResult& result) {
        emit execution_finished(result);
        if (executor_) {
            executor_->deleteLater();
            executor_ = nullptr;
        }
    });

    executor_->execute_from(wf, start_node_id);
}

void WorkflowService::stop_execution() {
    if (executor_)
        executor_->stop();
}

bool WorkflowService::is_executing() const {
    return executor_ && executor_->is_running();
}

} // namespace fincept::workflow
