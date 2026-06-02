#include "services/cloud/NewsMonitorCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/NewsMonitorRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QUuid>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString nm_entity() {
    return QStringLiteral("news_monitor");
}
QString nm_path(const QString& rid) {
    return QStringLiteral("/news-cloud/monitors/") + rid;
}
AdapterResult nm_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult nm_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult nm_result(const CloudResponse& resp) {
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

QJsonArray nm_keywords_to_json(const QStringList& kws) {
    QJsonArray a;
    for (const QString& k : kws)
        a.append(k);
    return a;
}
QStringList nm_keywords_from_json(const QJsonValue& v) {
    QStringList out;
    for (const QJsonValue& k : v.toArray())
        out.append(k.toString());
    return out;
}
// NewsMonitorRepository has no get(id) — find by id in the list.
bool nm_find_local(const QString& id, NewsMonitorRow& out) {
    auto r = NewsMonitorRepository::instance().list_all();
    if (r.is_err())
        return false;
    for (const NewsMonitorRow& m : r.value())
        if (m.id == id) {
            out = m;
            return true;
        }
    return false;
}
} // namespace

NewsMonitorCloudAdapter& NewsMonitorCloudAdapter::instance() {
    static NewsMonitorCloudAdapter s;
    return s;
}

void NewsMonitorCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else if (row.op == QLatin1String("toggle"))
        push_toggle(row, std::move(done));
    else
        push_upsert(row, std::move(done));
}

void NewsMonitorCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    NewsMonitorRow m;
    if (!nm_find_local(row.local_id, m)) {
        done(nm_ok());
        return;
    }
    QJsonObject body;
    body["label"] = m.label;
    body["keywords"] = nm_keywords_to_json(m.keywords);
    body["color"] = m.color;
    const QString local_id = row.local_id;
    auto remote = SyncMap::remote_id(nm_entity(), local_id);
    if (remote) {
        CloudClient::instance().put(nm_path(*remote), body, [done](CloudResponse r) { done(nm_result(r)); }, this);
    } else {
        CloudClient::instance().post(
            QStringLiteral("/news-cloud/monitors"), body,
            [local_id, done](CloudResponse r) {
                if (r.ok && r.data.isObject()) {
                    SyncMap::put(nm_entity(), local_id, r.data.toObject().value("id").toString(), {});
                    done(nm_ok());
                } else {
                    done(nm_result(r));
                }
            },
            this);
    }
}

void NewsMonitorCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(nm_entity(), row.local_id);
    if (!remote) {
        done(nm_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        nm_path(*remote),
        [local_id, done](CloudResponse r) {
            if (r.ok || r.status == 404) {
                SyncMap::remove_by_local(nm_entity(), local_id);
                done(nm_ok());
            } else {
                done(nm_result(r));
            }
        },
        this);
}

void NewsMonitorCloudAdapter::push_toggle(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(nm_entity(), row.local_id);
    if (!remote) {
        done(nm_retry(QStringLiteral("awaiting_create")));
        return;
    }
    CloudClient::instance().put(nm_path(*remote) + QStringLiteral("/toggle"), QJsonObject(),
                                [done](CloudResponse r) { done(nm_result(r)); }, this);
}

int NewsMonitorCloudAdapter::local_count() const {
    auto r = NewsMonitorRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void NewsMonitorCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/news-cloud/monitors"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void NewsMonitorCloudAdapter::enqueue_all_local() {
    auto ms = NewsMonitorRepository::instance().list_all();
    if (ms.is_err())
        return;
    for (const NewsMonitorRow& m : ms.value()) {
        SyncOutbox::record_unique(nm_entity(), m.id, QStringLiteral("upsert"));
        if (!m.enabled) // cloud create defaults enabled=true → toggle to match a disabled local
            SyncOutbox::record_unique(nm_entity(), m.id, QStringLiteral("toggle"));
    }
}

void NewsMonitorCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/news-cloud/monitors"),
        [this, done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, resp.error);
                return;
            }
            const QJsonArray arr = resp.data.toArray();
            QSet<QString> remote_ids;
            for (const QJsonValue& v : arr) {
                const QJsonObject o = v.toObject();
                remote_ids.insert(o.value("id").toString());
                apply_remote_monitor(o);
            }
            reconcile_deletes(remote_ids);
            done(true, QString());
        },
        this);
}

void NewsMonitorCloudAdapter::apply_remote_monitor(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    const QString rid = o.value("id").toString();
    auto existing = SyncMap::local_id(nm_entity(), rid);
    NewsMonitorRow m;
    m.id = existing ? *existing : QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.label = o.value("label").toString();
    m.keywords = nm_keywords_from_json(o.value("keywords"));
    m.color = o.value("color").toString();
    m.enabled = o.value("enabled").toBool(true);
    NewsMonitorRepository::instance().upsert(m);
    SyncMap::put(nm_entity(), m.id, rid, o.value("updated_at").toString());
}

void NewsMonitorCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(nm_entity());
    if (maps.is_err())
        return;
    auto& repo = NewsMonitorRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(nm_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
