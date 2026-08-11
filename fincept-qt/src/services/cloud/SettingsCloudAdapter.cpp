#include "services/cloud/SettingsCloudAdapter.h"

#include "core/logging/Logger.h"
#include "network/cloud/CloudClient.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sync/CloudSyncSettings.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>
#include <QStringList>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
constexpr const char* kSettingsCloudTag = "SettingsCloudAdapter";

QString st_entity() {
    return QStringLiteral("setting");
}
QString st_path(const QString& key) {
    return QStringLiteral("/settings/") + key;
}
// Derive the category from the key prefix ("appearance.font_size" -> "appearance").
QString st_category(const QString& key) {
    return key.section('.', 0, 0);
}
const QStringList& st_categories() {
    static const QStringList c = {QStringLiteral("appearance"), QStringLiteral("general"),
                                  QStringLiteral("notifications"), QStringLiteral("keybindings"),
                                  QStringLiteral("voice")};
    return c;
}
AdapterResult st_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult st_result(const CloudResponse& resp) {
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
} // namespace

SettingsCloudAdapter& SettingsCloudAdapter::instance() {
    static SettingsCloudAdapter s;
    return s;
}

void SettingsCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else
        push_upsert(row, std::move(done));
}

void SettingsCloudAdapter::push_upsert(const OutboxRow& row, PushDone done) {
    const QString key = row.local_id;
    auto v = SettingsRepository::instance().get(key);
    if (v.is_err()) {
        // Never sync a value we failed to read. Pushing the "" fallback would
        // make the cloud authoritative for an empty string and fan it back out
        // to every device on the account. Report a retryable failure so the
        // outbox keeps the row and pushes it once the DB is healthy again.
        LOG_ERROR(kSettingsCloudTag,
                  QString("settings read failed for '%1' — aborting cloud push, row stays queued: %2")
                      .arg(key, QString::fromStdString(v.error())));
        AdapterResult r;
        r.retry = true;
        r.error = QStringLiteral("settings_read_failed");
        done(r);
        return;
    }
    QJsonObject body;
    body["value"] = v.value();
    body["category"] = st_category(key);
    CloudClient::instance().put(st_path(key), body, [done](CloudResponse r) { done(st_result(r)); }, this);
}

void SettingsCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    CloudClient::instance().del(
        st_path(row.local_id),
        [done](CloudResponse r) {
            if (r.ok || r.status == 404)
                done(st_ok());
            else
                done(st_result(r));
        },
        this);
}

int SettingsCloudAdapter::local_count() const {
    int n = 0;
    for (const QString& cat : st_categories()) {
        auto r = SettingsRepository::instance().get_by_category(cat);
        if (r.is_ok())
            n += static_cast<int>(r.value().size());
    }
    return n;
}

void SettingsCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/settings"),
        [done](CloudResponse r) {
            if (!r.ok) {
                done(false, 0);
                return;
            }
            int n = 0;
            for (const QJsonValue& v : r.data.toArray())
                if (CloudSyncSettings::is_syncable_setting_key(v.toObject().value("key").toString()))
                    ++n;
            done(true, n);
        },
        this);
}

void SettingsCloudAdapter::enqueue_all_local() {
    for (const QString& cat : st_categories()) {
        auto r = SettingsRepository::instance().get_by_category(cat);
        if (r.is_err())
            continue;
        for (const Setting& s : r.value())
            if (CloudSyncSettings::is_syncable_setting_key(s.key))
                SyncOutbox::record(st_entity(), s.key, QStringLiteral("upsert"));
    }
}

void SettingsCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/settings"),
        [done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, resp.error);
                return;
            }
            SyncOutbox::ApplyGuard guard; // suppress outbox while applying remote
            auto& repo = SettingsRepository::instance();
            for (const QJsonValue& v : resp.data.toArray()) {
                const QJsonObject o = v.toObject();
                const QString key = o.value("key").toString();
                if (!CloudSyncSettings::is_syncable_setting_key(key))
                    continue;
                const QString category =
                    o.value("category").toString().isEmpty() ? st_category(key) : o.value("category").toString();
                repo.set(key, o.value("value").toString(), category);
            }
            done(true, QString());
        },
        this);
}

} // namespace fincept::services::cloud
