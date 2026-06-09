#include "services/cloud/NotebookCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/NotebookRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString nb_entity() {
    return QStringLiteral("notebook");
}
QString nb_path(const QString& rid) {
    return QStringLiteral("/notebooks/") + rid;
}
AdapterResult nb_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
[[maybe_unused]] AdapterResult nb_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult nb_result(const CloudResponse& resp) {
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

QJsonArray nb_parse_cells(const QString& s) {
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8());
    return d.isArray() ? d.array() : QJsonArray();
}
QString nb_serialize_cells(const QJsonValue& v) {
    return v.isArray() ? QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact))
                       : QStringLiteral("[]");
}
QJsonObject nb_parse_meta(const QString& s) {
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8());
    return d.isObject() ? d.object() : QJsonObject();
}
QString nb_serialize_meta(const QJsonValue& v) {
    return v.isObject() ? QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact))
                        : QStringLiteral("{}");
}
QJsonObject nb_body(const Notebook& n) {
    QJsonObject b;
    b["name"] = n.name;
    b["description"] = n.description;
    b["cells"] = nb_parse_cells(n.cells);
    b["metadata"] = nb_parse_meta(n.metadata);
    b["execution_counter"] = n.execution_counter;
    return b;
}
} // namespace

NotebookCloudAdapter& NotebookCloudAdapter::instance() {
    static NotebookCloudAdapter s;
    return s;
}

void NotebookCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else
        push_upsert(row, std::move(done));
}

void NotebookCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    auto nb = NotebookRepository::instance().get(row.local_id);
    if (nb.is_err()) {
        done(nb_ok());
        return;
    }
    const QJsonObject body = nb_body(nb.value());
    const QString local_id = row.local_id;
    auto remote = SyncMap::remote_id(nb_entity(), local_id);
    if (remote) {
        CloudClient::instance().put(nb_path(*remote), body, [done](CloudResponse r) { done(nb_result(r)); }, this);
    } else {
        CloudClient::instance().post(
            QStringLiteral("/notebooks"), body,
            [local_id, done](CloudResponse r) {
                if (r.ok && r.data.isObject()) {
                    const QJsonObject o = r.data.toObject();
                    SyncMap::put(nb_entity(), local_id, o.value("id").toString(), o.value("created_at").toString());
                    done(nb_ok());
                } else {
                    done(nb_result(r));
                }
            },
            this);
    }
}

void NotebookCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(nb_entity(), row.local_id);
    if (!remote) {
        done(nb_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        nb_path(*remote),
        [local_id, done](CloudResponse r) {
            if (r.ok || r.status == 404) {
                SyncMap::remove_by_local(nb_entity(), local_id);
                done(nb_ok());
            } else {
                done(nb_result(r));
            }
        },
        this);
}

int NotebookCloudAdapter::local_count() const {
    auto r = NotebookRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void NotebookCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/notebooks"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void NotebookCloudAdapter::enqueue_all_local() {
    auto ns = NotebookRepository::instance().list_all();
    if (ns.is_err())
        return;
    for (const Notebook& n : ns.value())
        SyncOutbox::record_unique(nb_entity(), n.id, QStringLiteral("upsert"));
}

void NotebookCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/notebooks"),
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
                    nb_path(rid),
                    [this, pending, had_error, remote_ids, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_notebook(dr.data.toObject());
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

void NotebookCloudAdapter::apply_remote_notebook(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    auto& repo = NotebookRepository::instance();
    const QString rid = o.value("id").toString();
    auto existing = SyncMap::local_id(nb_entity(), rid);

    Notebook n;
    n.id = existing ? *existing : QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.name = o.value("name").toString();
    n.description = o.value("description").toString();
    n.cells = nb_serialize_cells(o.value("cells"));
    n.metadata = nb_serialize_meta(o.value("metadata"));
    n.execution_counter = o.value("execution_counter").toInt();
    repo.save(n);
    SyncMap::put(nb_entity(), n.id, rid, o.value("updated_at").toString());
}

void NotebookCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(nb_entity());
    if (maps.is_err())
        return;
    auto& repo = NotebookRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(nb_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
