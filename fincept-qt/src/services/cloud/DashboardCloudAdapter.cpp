#include "services/cloud/DashboardCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/DashboardLayoutRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString db_entity() {
    return QStringLiteral("dashboard");
}
QString db_layout_path(const QString& profile) {
    return QStringLiteral("/dashboard/layouts/") + profile;
}
AdapterResult db_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult db_result(const CloudResponse& resp) {
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

QJsonObject db_layout_body(const screens::GridLayout& l, bool is_active) {
    QJsonObject o;
    o["cols"] = l.cols;
    o["row_height"] = l.row_h;
    o["margin"] = l.margin;
    o["is_active"] = is_active;
    QJsonArray widgets;
    int i = 0;
    for (const screens::GridItem& it : l.items) {
        QJsonObject w;
        w["instance_id"] = it.instance_id;
        w["widget_type"] = it.id;
        w["grid_x"] = it.cell.x;
        w["grid_y"] = it.cell.y;
        w["grid_w"] = it.cell.w;
        w["grid_h"] = it.cell.h;
        w["min_w"] = it.cell.min_w;
        w["min_h"] = it.cell.min_h;
        w["sort_order"] = i++;
        w["config"] = it.config;
        widgets.append(w);
    }
    o["widgets"] = widgets;
    return o;
}

screens::GridLayout db_parse_layout(const QJsonObject& o) {
    screens::GridLayout l;
    l.cols = o.value("cols").toInt(12);
    l.row_h = o.value("row_height").toInt(60);
    l.margin = o.value("margin").toInt(4);
    for (const QJsonValue& v : o.value("widgets").toArray()) {
        const QJsonObject w = v.toObject();
        screens::GridItem it;
        it.id = w.value("widget_type").toString();
        it.instance_id = w.value("instance_id").toString();
        it.cell.x = w.value("grid_x").toInt();
        it.cell.y = w.value("grid_y").toInt();
        it.cell.w = w.value("grid_w").toInt(4);
        it.cell.h = w.value("grid_h").toInt(3);
        it.cell.min_w = w.value("min_w").toInt(2);
        it.cell.min_h = w.value("min_h").toInt(3);
        it.config = w.value("config").toObject();
        l.items.append(it);
    }
    return l;
}
} // namespace

DashboardCloudAdapter& DashboardCloudAdapter::instance() {
    static DashboardCloudAdapter s;
    return s;
}

void DashboardCloudAdapter::push(const OutboxRow& row, PushDone done) {
    push_upsert(row, std::move(done)); // dashboard only emits "upsert"
}

void DashboardCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    const QString profile = row.local_id;
    auto lr = DashboardLayoutRepository::instance().load_layout(profile);
    if (lr.is_err()) {
        done(db_ok());
        return;
    }
    const QJsonObject body = db_layout_body(lr.value(), /*is_active=*/profile == QLatin1String("default"));
    CloudClient::instance().put(
        db_layout_path(profile), body,
        [profile, done](CloudResponse resp) {
            if (resp.ok) {
                SyncMap::put(db_entity(), profile, profile, {});
                done(db_ok());
            } else {
                done(db_result(resp));
            }
        },
        this);
}

int DashboardCloudAdapter::local_count() const {
    return 1; // a layout (at least the default template) always exists locally
}

void DashboardCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/dashboard/layouts"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            done(true, r.data.isArray() ? static_cast<int>(r.data.toArray().size()) : 0);
        },
        this);
}

void DashboardCloudAdapter::enqueue_all_local() {
    // Upload the primary profile on first-enable; other profiles upload when next saved.
    SyncOutbox::record_unique(db_entity(), QStringLiteral("default"), QStringLiteral("upsert"));
}

void DashboardCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/dashboard/layouts"),
        [this, done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, resp.error);
                return;
            }
            const QJsonArray arr = resp.data.toArray();
            auto profiles = std::make_shared<QSet<QString>>();
            for (const QJsonValue& v : arr)
                profiles->insert(v.toObject().value("profile_name").toString());
            if (arr.isEmpty()) {
                reconcile_deletes(*profiles);
                done(true, QString());
                return;
            }
            auto pending = std::make_shared<int>(static_cast<int>(arr.size()));
            auto had_error = std::make_shared<bool>(false);
            for (const QJsonValue& v : arr) {
                const QString profile = v.toObject().value("profile_name").toString();
                CloudClient::instance().get(
                    db_layout_path(profile),
                    [this, profile, pending, had_error, profiles, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_layout(profile, dr.data.toObject());
                        else
                            *had_error = true;
                        if (--*pending == 0) {
                            reconcile_deletes(*profiles);
                            done(!*had_error, *had_error ? QStringLiteral("pull_partial") : QString());
                        }
                    },
                    this);
            }
        },
        this);
}

void DashboardCloudAdapter::apply_remote_layout(const QString& profile, const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard;
    const screens::GridLayout layout = db_parse_layout(o);
    DashboardLayoutRepository::instance().save_layout(layout, profile);
    SyncMap::put(db_entity(), profile, profile, o.value("updated_at").toString());
}

void DashboardCloudAdapter::reconcile_deletes(const QSet<QString>& remote_profiles) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(db_entity());
    if (maps.is_err())
        return;
    auto& repo = DashboardLayoutRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_profiles.contains(m.remote_id)) {
            repo.clear_layout(m.local_id); // reset locally; the cloud profile is gone
            SyncMap::remove_by_local(db_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
