#include "services/cloud/AgentConfigCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/AgentConfigRepository.h"
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
QString ac_entity() {
    return QStringLiteral("agent_config");
}
QString ac_path(const QString& rid) {
    return QStringLiteral("/agent-configs/") + rid;
}
AdapterResult ac_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult ac_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult ac_result(const CloudResponse& resp) {
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

// config_json is a TEXT blob locally but a JSON value on the wire.
QJsonValue ac_parse_config(const QString& s) {
    QJsonParseError e;
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8(), &e);
    if (e.error != QJsonParseError::NoError)
        return QJsonObject();
    if (d.isObject())
        return d.object();
    if (d.isArray())
        return d.array();
    return QJsonObject();
}
QString ac_serialize_config(const QJsonValue& v) {
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return QStringLiteral("{}");
}
QJsonObject ac_body(const AgentConfig& c) {
    QJsonObject b;
    b["name"] = c.name;
    b["description"] = c.description;
    b["config_json"] = ac_parse_config(c.config_json);
    b["category"] = c.category.isEmpty() ? QStringLiteral("general") : c.category;
    b["is_default"] = c.is_default;
    return b;
}
} // namespace

AgentConfigCloudAdapter& AgentConfigCloudAdapter::instance() {
    static AgentConfigCloudAdapter s;
    return s;
}

void AgentConfigCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else if (row.op == QLatin1String("activate"))
        push_activate(row, std::move(done));
    else
        push_upsert(row, std::move(done)); // "upsert"
}

void AgentConfigCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    auto c = AgentConfigRepository::instance().get(row.local_id);
    if (c.is_err()) {
        done(ac_ok());
        return;
    }
    const QJsonObject body = ac_body(c.value());
    const QString local_id = row.local_id;
    auto remote = SyncMap::remote_id(ac_entity(), local_id);
    if (remote) {
        CloudClient::instance().put(ac_path(*remote), body, [done](CloudResponse r) { done(ac_result(r)); }, this);
    } else {
        CloudClient::instance().post(
            QStringLiteral("/agent-configs"), body,
            [local_id, done](CloudResponse r) {
                if (r.ok && r.data.isObject()) {
                    const QJsonObject o = r.data.toObject();
                    SyncMap::put(ac_entity(), local_id, o.value("id").toString(), o.value("created_at").toString());
                    done(ac_ok());
                } else {
                    done(ac_result(r));
                }
            },
            this);
    }
}

void AgentConfigCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(ac_entity(), row.local_id);
    if (!remote) {
        done(ac_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        ac_path(*remote),
        [local_id, done](CloudResponse r) {
            if (r.ok || r.status == 404) {
                SyncMap::remove_by_local(ac_entity(), local_id);
                done(ac_ok());
            } else {
                done(ac_result(r));
            }
        },
        this);
}

void AgentConfigCloudAdapter::push_activate(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(ac_entity(), row.local_id);
    if (!remote) {
        done(ac_retry(QStringLiteral("awaiting_create")));
        return;
    }
    CloudClient::instance().put(ac_path(*remote) + QStringLiteral("/activate"), QJsonObject(),
                                [done](CloudResponse r) { done(ac_result(r)); }, this);
}

int AgentConfigCloudAdapter::local_count() const {
    auto r = AgentConfigRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void AgentConfigCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/agent-configs"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void AgentConfigCloudAdapter::enqueue_all_local() {
    auto cs = AgentConfigRepository::instance().list_all();
    if (cs.is_err())
        return;
    for (const AgentConfig& c : cs.value()) {
        SyncOutbox::record_unique(ac_entity(), c.id, QStringLiteral("upsert"));
        if (c.is_active)
            SyncOutbox::record_unique(ac_entity(), c.id, QStringLiteral("activate"));
    }
}

void AgentConfigCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/agent-configs"),
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
                    ac_path(rid),
                    [this, pending, had_error, remote_ids, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_agent(dr.data.toObject());
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

void AgentConfigCloudAdapter::apply_remote_agent(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    auto& repo = AgentConfigRepository::instance();
    const QString rid = o.value("id").toString();
    auto existing = SyncMap::local_id(ac_entity(), rid);

    AgentConfig c;
    c.id = existing ? *existing : QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = o.value("name").toString();
    c.description = o.value("description").toString();
    c.config_json = ac_serialize_config(o.value("config_json"));
    c.category = o.value("category").toString();
    c.is_default = o.value("is_default").toBool();
    c.is_active = o.value("is_active").toBool();
    repo.save(c);
    SyncMap::put(ac_entity(), c.id, rid, o.value("updated_at").toString());
}

void AgentConfigCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(ac_entity());
    if (maps.is_err())
        return;
    auto& repo = AgentConfigRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(ac_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
