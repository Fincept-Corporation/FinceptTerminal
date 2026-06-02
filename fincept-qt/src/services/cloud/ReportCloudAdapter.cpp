#include "services/cloud/ReportCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/ReportRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString rp_entity() {
    return QStringLiteral("report");
}
QString rp_path(const QString& rid) {
    return QStringLiteral("/reports/") + rid;
}
AdapterResult rp_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult rp_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult rp_result(const CloudResponse& resp) {
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

QJsonValue rp_parse(const QString& s) {
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
QString rp_serialize(const QJsonValue& v) {
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return QStringLiteral("{}");
}
} // namespace

ReportCloudAdapter& ReportCloudAdapter::instance() {
    static ReportCloudAdapter s;
    return s;
}

void ReportCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("create"))
        push_create(row, std::move(done));
    else if (row.op == QLatin1String("update"))
        push_update(row, std::move(done));
    else if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else
        done(rp_ok());
}

void ReportCloudAdapter::push_create(const OutboxRow& row, PushDone done) {
    auto r = ReportRepository::instance().get(row.local_id.toInt());
    if (r.is_err()) {
        done(rp_ok());
        return;
    }
    QJsonObject body;
    body["title"] = r.value().title;
    body["content_json"] = rp_parse(r.value().content_json);
    const QString local_id = row.local_id;
    CloudClient::instance().post(
        QStringLiteral("/reports"), body,
        [local_id, done](CloudResponse resp) {
            if (resp.ok && resp.data.isObject()) {
                const QJsonObject o = resp.data.toObject();
                SyncMap::put(rp_entity(), local_id, o.value("id").toString(), o.value("created_at").toString());
                done(rp_ok());
            } else {
                done(rp_result(resp));
            }
        },
        this);
}

void ReportCloudAdapter::push_update(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(rp_entity(), row.local_id);
    if (!remote) {
        done(rp_retry(QStringLiteral("awaiting_create")));
        return;
    }
    auto r = ReportRepository::instance().get(row.local_id.toInt());
    if (r.is_err()) {
        done(rp_ok());
        return;
    }
    QJsonObject body;
    body["title"] = r.value().title;
    body["content_json"] = rp_parse(r.value().content_json);
    CloudClient::instance().put(rp_path(*remote), body, [done](CloudResponse resp) { done(rp_result(resp)); }, this);
}

void ReportCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(rp_entity(), row.local_id);
    if (!remote) {
        done(rp_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        rp_path(*remote),
        [local_id, done](CloudResponse resp) {
            if (resp.ok || resp.status == 404) {
                SyncMap::remove_by_local(rp_entity(), local_id);
                done(rp_ok());
            } else {
                done(rp_result(resp));
            }
        },
        this);
}

int ReportCloudAdapter::local_count() const {
    auto r = ReportRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void ReportCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/reports"),
        [done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, 0);
                return;
            }
            done(true, resp.data.isArray() ? static_cast<int>(resp.data.toArray().size()) : 0);
        },
        this);
}

void ReportCloudAdapter::enqueue_all_local() {
    auto rs = ReportRepository::instance().list_all();
    if (rs.is_err())
        return;
    for (const Report& r : rs.value())
        SyncOutbox::record(rp_entity(), QString::number(r.id), QStringLiteral("create"));
}

void ReportCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/reports"),
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
                    rp_path(rid),
                    [this, pending, had_error, remote_ids, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_report(dr.data.toObject());
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

void ReportCloudAdapter::apply_remote_report(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    auto& repo = ReportRepository::instance();
    const QString rid = o.value("id").toString();
    const QString title = o.value("title").toString();
    const QString content = rp_serialize(o.value("content_json"));

    auto existing = SyncMap::local_id(rp_entity(), rid);
    QString local_id;
    if (existing) {
        repo.update(existing->toInt(), title, content);
        local_id = *existing;
    } else {
        auto cr = repo.create(title, content);
        if (cr.is_err())
            return;
        local_id = QString::number(cr.value());
    }
    SyncMap::put(rp_entity(), local_id, rid, o.value("updated_at").toString());
}

void ReportCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(rp_entity());
    if (maps.is_err())
        return;
    auto& repo = ReportRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id.toInt());
            SyncMap::remove_by_local(rp_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
