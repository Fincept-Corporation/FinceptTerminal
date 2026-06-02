#include "services/cloud/WorkflowCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/WorkflowRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QUuid>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString wf_entity() {
    return QStringLiteral("workflow");
}
QString wf_path(const QString& rid) {
    return QStringLiteral("/workflows/") + rid;
}
AdapterResult wf_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult wf_result(const CloudResponse& resp) {
    AdapterResult r;
    if (resp.ok) {
        r.ok = true;
        return r;
    }
    if (resp.is_insufficient_credits()) {
        r.credits_exhausted = true;
        r.error = QStringLiteral("insufficient_credits");
        return r;
    }
    r.retry = true;
    r.error = resp.error.isEmpty() ? QStringLiteral("push_failed") : resp.error;
    return r;
}

QString wf_status_to_str(workflow::WorkflowStatus s) {
    switch (s) {
    case workflow::WorkflowStatus::Idle:
        return QStringLiteral("idle");
    case workflow::WorkflowStatus::Running:
        return QStringLiteral("running");
    case workflow::WorkflowStatus::Completed:
        return QStringLiteral("completed");
    case workflow::WorkflowStatus::Error:
        return QStringLiteral("error");
    default:
        return QStringLiteral("draft");
    }
}
workflow::WorkflowStatus wf_status_from_str(const QString& s) {
    const QString u = s.toLower();
    if (u == QLatin1String("idle"))
        return workflow::WorkflowStatus::Idle;
    if (u == QLatin1String("running"))
        return workflow::WorkflowStatus::Running;
    if (u == QLatin1String("completed"))
        return workflow::WorkflowStatus::Completed;
    if (u == QLatin1String("error"))
        return workflow::WorkflowStatus::Error;
    return workflow::WorkflowStatus::Draft;
}

QJsonObject wf_payload(const workflow::WorkflowDef& wf) {
    QJsonObject o;
    o["name"] = wf.name;
    o["description"] = wf.description;
    o["status"] = wf_status_to_str(wf.status);
    o["static_data"] = wf.static_data;
    QJsonArray nodes;
    for (const workflow::NodeDef& n : wf.nodes) {
        QJsonObject jn;
        jn["id"] = n.id;
        jn["type"] = n.type;
        jn["name"] = n.name;
        jn["type_version"] = n.type_version;
        jn["pos_x"] = n.x;
        jn["pos_y"] = n.y;
        jn["parameters"] = n.parameters;
        jn["credentials"] = n.credentials;
        jn["disabled"] = n.disabled;
        jn["continue_on_fail"] = n.continue_on_fail;
        jn["retry_on_fail"] = n.retry_on_fail;
        jn["max_tries"] = n.max_tries;
        nodes.append(jn);
    }
    o["nodes"] = nodes;
    QJsonArray edges;
    for (const workflow::EdgeDef& e : wf.edges) {
        QJsonObject je;
        je["id"] = e.id;
        je["source_node"] = e.source_node;
        je["target_node"] = e.target_node;
        je["source_port"] = e.source_port;
        je["target_port"] = e.target_port;
        je["animated"] = e.animated;
        edges.append(je);
    }
    o["edges"] = edges;
    return o;
}

workflow::WorkflowDef wf_parse(const QJsonObject& o, const QString& local_id) {
    workflow::WorkflowDef wf;
    wf.id = local_id;
    wf.name = o.value("name").toString();
    wf.description = o.value("description").toString();
    wf.status = wf_status_from_str(o.value("status").toString());
    wf.static_data = o.value("static_data").toObject();
    for (const QJsonValue& v : o.value("nodes").toArray()) {
        const QJsonObject jn = v.toObject();
        workflow::NodeDef n;
        n.id = jn.value("id").toString();
        n.type = jn.value("type").toString();
        n.name = jn.value("name").toString();
        n.type_version = jn.value("type_version").toInt(1);
        n.x = jn.value("pos_x").toDouble();
        n.y = jn.value("pos_y").toDouble();
        n.parameters = jn.value("parameters").toObject();
        n.credentials = jn.value("credentials").toObject();
        n.disabled = jn.value("disabled").toBool();
        n.continue_on_fail = jn.value("continue_on_fail").toBool();
        n.retry_on_fail = jn.value("retry_on_fail").toBool();
        n.max_tries = jn.value("max_tries").toInt(1);
        wf.nodes.append(n);
    }
    for (const QJsonValue& v : o.value("edges").toArray()) {
        const QJsonObject je = v.toObject();
        workflow::EdgeDef e;
        e.id = je.value("id").toString();
        e.source_node = je.value("source_node").toString();
        e.target_node = je.value("target_node").toString();
        e.source_port = je.value("source_port").toString();
        e.target_port = je.value("target_port").toString();
        e.animated = je.value("animated").toBool();
        wf.edges.append(e);
    }
    return wf;
}
} // namespace

WorkflowCloudAdapter& WorkflowCloudAdapter::instance() {
    static WorkflowCloudAdapter s;
    return s;
}

void WorkflowCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else
        push_upsert(row, std::move(done));
}

void WorkflowCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    auto wf = WorkflowRepository::instance().load(row.local_id);
    if (wf.is_err()) {
        done(wf_ok());
        return;
    }
    const QJsonObject body = wf_payload(wf.value());
    const QString local_id = row.local_id;
    auto remote = SyncMap::remote_id(wf_entity(), local_id);
    if (remote) {
        CloudClient::instance().put(wf_path(*remote), body, [done](CloudResponse r) { done(wf_result(r)); }, this);
    } else {
        // /import creates a workflow WITH the full graph in a single call.
        CloudClient::instance().post(
            QStringLiteral("/workflows/import"), body,
            [local_id, done](CloudResponse r) {
                if (r.ok && r.data.isObject()) {
                    SyncMap::put(wf_entity(), local_id, r.data.toObject().value("id").toString(), {});
                    done(wf_ok());
                } else {
                    done(wf_result(r));
                }
            },
            this);
    }
}

void WorkflowCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(wf_entity(), row.local_id);
    if (!remote) {
        done(wf_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        wf_path(*remote),
        [local_id, done](CloudResponse r) {
            if (r.ok || r.status == 404) {
                SyncMap::remove_by_local(wf_entity(), local_id);
                done(wf_ok());
            } else {
                done(wf_result(r));
            }
        },
        this);
}

int WorkflowCloudAdapter::local_count() const {
    auto r = WorkflowRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void WorkflowCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/workflows"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void WorkflowCloudAdapter::enqueue_all_local() {
    auto ws = WorkflowRepository::instance().list_all();
    if (ws.is_err())
        return;
    for (const WorkflowRow& w : ws.value())
        SyncOutbox::record_unique(wf_entity(), w.id, QStringLiteral("upsert"));
}

void WorkflowCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/workflows"),
        [this, done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, resp.error);
                return;
            }
            const QJsonArray arr = resp.data.toArray();
            auto remote_ids = std::make_shared<QSet<QString>>();
            for (const QJsonValue& v : arr)
                remote_ids->insert(v.toObject().value("id").toString());
            if (arr.isEmpty()) {
                reconcile_deletes(*remote_ids);
                done(true, QString());
                return;
            }
            auto pending = std::make_shared<int>(static_cast<int>(arr.size()));
            auto had_error = std::make_shared<bool>(false);
            for (const QJsonValue& v : arr) {
                const QString rid = v.toObject().value("id").toString();
                CloudClient::instance().get(
                    wf_path(rid),
                    [this, pending, had_error, remote_ids, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_workflow(dr.data.toObject());
                        else
                            *had_error = true;
                        if (--*pending == 0) {
                            reconcile_deletes(*remote_ids);
                            done(!*had_error, *had_error ? QStringLiteral("pull_partial") : QString());
                        }
                    },
                    this);
            }
        },
        this);
}

void WorkflowCloudAdapter::apply_remote_workflow(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    const QString rid = o.value("id").toString();
    auto existing = SyncMap::local_id(wf_entity(), rid);
    const QString local_id = existing ? *existing : QUuid::createUuid().toString(QUuid::WithoutBraces);
    const workflow::WorkflowDef wf = wf_parse(o, local_id);
    WorkflowRepository::instance().save(wf);
    SyncMap::put(wf_entity(), local_id, rid, o.value("updated_at").toString());
}

void WorkflowCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(wf_entity());
    if (maps.is_err())
        return;
    auto& repo = WorkflowRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(wf_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
