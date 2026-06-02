#include "services/cloud/NewsFeedCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/RssFeedRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QUuid>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString nf_entity() {
    return QStringLiteral("news_feed");
}
QString nf_path(const QString& rid) {
    return QStringLiteral("/news-cloud/feeds/") + rid;
}
AdapterResult nf_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult nf_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult nf_result(const CloudResponse& resp) {
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

bool nf_find_local(const QString& id, RssFeedRow& out) {
    auto r = RssFeedRepository::instance().list_all();
    if (r.is_err())
        return false;
    for (const RssFeedRow& f : r.value())
        if (f.id == id) {
            out = f;
            return true;
        }
    return false;
}
} // namespace

NewsFeedCloudAdapter& NewsFeedCloudAdapter::instance() {
    static NewsFeedCloudAdapter s;
    return s;
}

void NewsFeedCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else if (row.op == QLatin1String("toggle"))
        push_toggle(row, std::move(done));
    else
        push_upsert(row, std::move(done));
}

void NewsFeedCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    RssFeedRow f;
    if (!nf_find_local(row.local_id, f)) {
        done(nf_ok());
        return;
    }
    const QString local_id = row.local_id;
    auto remote = SyncMap::remote_id(nf_entity(), local_id);
    if (remote) {
        QJsonObject body; // url is the immutable conflict key — not sent on update
        body["name"] = f.name;
        body["category"] = f.category;
        body["region"] = f.region;
        body["source"] = f.source;
        body["tier"] = f.tier;
        CloudClient::instance().put(nf_path(*remote), body, [done](CloudResponse r) { done(nf_result(r)); }, this);
    } else {
        QJsonObject body;
        body["name"] = f.name;
        body["url"] = f.url;
        body["category"] = f.category;
        body["region"] = f.region;
        body["source"] = f.source;
        body["tier"] = f.tier;
        CloudClient::instance().post(
            QStringLiteral("/news-cloud/feeds"), body,
            [local_id, done](CloudResponse r) {
                if (r.ok && r.data.isObject()) {
                    SyncMap::put(nf_entity(), local_id, r.data.toObject().value("id").toString(), {});
                    done(nf_ok());
                } else {
                    done(nf_result(r));
                }
            },
            this);
    }
}

void NewsFeedCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(nf_entity(), row.local_id);
    if (!remote) {
        done(nf_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        nf_path(*remote),
        [local_id, done](CloudResponse r) {
            if (r.ok || r.status == 404) {
                SyncMap::remove_by_local(nf_entity(), local_id);
                done(nf_ok());
            } else {
                done(nf_result(r));
            }
        },
        this);
}

void NewsFeedCloudAdapter::push_toggle(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(nf_entity(), row.local_id);
    if (!remote) {
        done(nf_retry(QStringLiteral("awaiting_create")));
        return;
    }
    CloudClient::instance().put(nf_path(*remote) + QStringLiteral("/toggle"), QJsonObject(),
                                [done](CloudResponse r) { done(nf_result(r)); }, this);
}

int NewsFeedCloudAdapter::local_count() const {
    auto r = RssFeedRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void NewsFeedCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/news-cloud/feeds"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void NewsFeedCloudAdapter::enqueue_all_local() {
    auto fs = RssFeedRepository::instance().list_all();
    if (fs.is_err())
        return;
    for (const RssFeedRow& f : fs.value()) {
        SyncOutbox::record_unique(nf_entity(), f.id, QStringLiteral("upsert"));
        if (!f.enabled)
            SyncOutbox::record_unique(nf_entity(), f.id, QStringLiteral("toggle"));
    }
}

void NewsFeedCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/news-cloud/feeds"),
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
                apply_remote_feed(o);
            }
            reconcile_deletes(remote_ids);
            done(true, QString());
        },
        this);
}

void NewsFeedCloudAdapter::apply_remote_feed(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    const QString rid = o.value("id").toString();
    auto existing = SyncMap::local_id(nf_entity(), rid);
    RssFeedRow f;
    f.id = existing ? *existing : QUuid::createUuid().toString(QUuid::WithoutBraces);
    f.name = o.value("name").toString();
    f.url = o.value("url").toString();
    f.category = o.value("category").toString();
    f.region = o.value("region").toString();
    f.source = o.value("source").toString();
    f.tier = o.value("tier").toInt(3);
    f.is_builtin = o.value("is_builtin").toBool();
    f.enabled = o.value("enabled").toBool(true);
    RssFeedRepository::instance().upsert(f);
    SyncMap::put(nf_entity(), f.id, rid, {});
}

void NewsFeedCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(nf_entity());
    if (maps.is_err())
        return;
    auto& repo = RssFeedRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(nf_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
