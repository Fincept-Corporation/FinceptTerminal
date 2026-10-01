#include "storage/repositories/DataSourceRepository.h"

#include "storage/secure/SecureStorage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace fincept {

namespace {
// `config` carries a connector's credentials (REST API keys, SQL/NoSQL passwords,
// tokens). Like LlmConfigRepository / McpServerRepository, keep those values
// encrypted at rest in SecureStorage instead of in the plaintext `data_sources`
// table: save() moves every secret-looking string into one SecureStorage entry per
// connection and leaves "" in the column; reads merge them back, so callers still
// see the complete config. A legacy row whose column still holds plaintext secrets
// keeps working (a non-empty column value wins) and is migrated lazily the first
// time list_all()/get() reads it. If the encrypted store is unavailable the
// plaintext stays in the column — a credential is never dropped.

QString ds_secret_handle(const QString& id) {
    return QStringLiteral("datasource:cfg:") + id;
}

bool ds_is_secret_key(const QString& key) {
    const QString k = key.toLower();
    for (const char* frag : {"password", "passwd", "secret", "token", "apikey", "api_key", "privatekey", "private_key",
                             "accesskey", "access_key", "accountkey", "account_key", "applicationkey", "appkey",
                             "app_key", "credential", "connectionstring"}) {
        if (k.contains(QLatin1String(frag)))
            return true;
    }
    // Cosmos "key" (Primary Key) and the REST/GraphQL custom-headers JSON, which
    // routinely carries an Authorization / X-API-Key value.
    return k == QLatin1String("key") || k == QLatin1String("headers");
}

bool ds_is_secret_entry(const QString& key, const QJsonValue& value) {
    return value.isString() && !value.toString().isEmpty() && ds_is_secret_key(key);
}

/// True when the stored column still holds at least one plaintext secret value.
bool ds_column_has_plaintext_secret(const QString& column_config) {
    const QJsonDocument doc = QJsonDocument::fromJson(column_config.toUtf8());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (ds_is_secret_entry(it.key(), it.value()))
            return true;
    }
    return false;
}

/// Text to store in the `config` column for `config`: secrets moved to
/// SecureStorage and blanked. Returns `config` unchanged when it has no secrets,
/// is not a JSON object, or the encrypted store failed.
QString ds_config_for_column(const QString& id, const QString& config) {
    const QJsonDocument doc = QJsonDocument::fromJson(config.toUtf8());
    if (!doc.isObject() || id.isEmpty())
        return config;

    QJsonObject obj = doc.object();
    QJsonObject secrets;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (ds_is_secret_entry(it.key(), it.value()))
            secrets.insert(it.key(), it.value());
    }

    if (secrets.isEmpty()) {
        // Nothing sensitive left (secret cleared, or none ever set): drop any stale copy.
        SecureStorage::instance().remove(ds_secret_handle(id));
        return config;
    }

    const QString payload = QString::fromUtf8(QJsonDocument(secrets).toJson(QJsonDocument::Compact));
    if (SecureStorage::instance().store(ds_secret_handle(id), payload).is_err()) {
        LOG_WARN("DataSourceRepo", "SecureStorage unavailable — keeping connection credentials in the config column");
        return config;
    }
    for (const QString& key : secrets.keys())
        obj.insert(key, QString());
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/// Merge SecureStorage-held secrets back into a config read from the column.
/// A non-empty column value (legacy plaintext, or a save made while the encrypted
/// store was down) wins over the stored copy.
QString ds_config_from_column(const QString& id, const QString& column_config) {
    const QJsonDocument doc = QJsonDocument::fromJson(column_config.toUtf8());
    if (!doc.isObject() || id.isEmpty())
        return column_config;

    QJsonObject obj = doc.object();
    bool has_blank_secret = false;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (ds_is_secret_key(it.key()) && it.value().isString() && it.value().toString().isEmpty()) {
            has_blank_secret = true;
            break;
        }
    }
    if (!has_blank_secret)
        return column_config;

    auto sr = SecureStorage::instance().retrieve(ds_secret_handle(id));
    if (sr.is_err() || sr.value().isEmpty())
        return column_config;
    const QJsonDocument sdoc = QJsonDocument::fromJson(sr.value().toUtf8());
    if (!sdoc.isObject())
        return column_config;

    const QJsonObject secrets = sdoc.object();
    bool changed = false;
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it) {
        const QJsonValue cur = obj.value(it.key());
        if (!cur.isString() || cur.toString().isEmpty()) {
            obj.insert(it.key(), it.value());
            changed = true;
        }
    }
    return changed ? QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)) : column_config;
}

/// Move a legacy row's plaintext secrets into SecureStorage (config text is
/// already the merged, complete one). Leaves `updated_at` alone — this is not a
/// user edit. A failure is harmless: the row simply stays as it was.
void ds_migrate_legacy_secrets(const DataSource& ds) {
    const QString column = ds_config_for_column(ds.id, ds.config);
    if (column == ds.config)
        return; // encrypted store unavailable (or nothing to move)
    auto r = Database::instance().execute("UPDATE data_sources SET config = ? WHERE id = ?", {column, ds.id});
    if (r.is_err())
        LOG_WARN("DataSourceRepo", "Could not scrub migrated credentials from the config column: " +
                                       QString::fromStdString(r.error()));
    else
        LOG_INFO("DataSourceRepo", "Moved connection credentials into SecureStorage: " + ds.id);
}
} // namespace

DataSourceRepository& DataSourceRepository::instance() {
    static DataSourceRepository s;
    return s;
}

DataSource DataSourceRepository::map_row(QSqlQuery& q) {
    const QString id = q.value(0).toString();
    return {id,
            q.value(1).toString(),
            q.value(2).toString(),
            q.value(3).toString(),
            q.value(4).toString(),
            q.value(5).toString(),
            q.value(6).toString(),
            ds_config_from_column(id, q.value(7).toString()),
            q.value(8).toBool(),
            q.value(9).toString(),
            q.value(10).toString(),
            q.value(11).toString()};
}

static const char* kDsCols = "id, alias, display_name, description, type, provider, category, config, "
                             "enabled, tags, created_at, updated_at";

Result<QVector<DataSource>> DataSourceRepository::list_all() {
    QStringList legacy_ids; // rows whose column still holds plaintext secrets
    auto r = query_list(QString("SELECT %1 FROM data_sources ORDER BY display_name").arg(kDsCols), {},
                        [&legacy_ids](QSqlQuery& q) {
                            DataSource ds = map_row(q);
                            if (ds_column_has_plaintext_secret(q.value(7).toString()))
                                legacy_ids << ds.id;
                            return ds;
                        });
    if (r.is_ok()) {
        for (const auto& ds : r.value()) {
            if (legacy_ids.contains(ds.id))
                ds_migrate_legacy_secrets(ds);
        }
    }
    return r;
}

Result<DataSource> DataSourceRepository::get(const QString& id) {
    bool legacy = false;
    auto r = query_one(QString("SELECT %1 FROM data_sources WHERE id = ?").arg(kDsCols), {id},
                       [&legacy](QSqlQuery& q) {
                           legacy = ds_column_has_plaintext_secret(q.value(7).toString());
                           return map_row(q);
                       });
    if (r.is_ok() && legacy)
        ds_migrate_legacy_secrets(r.value());
    return r;
}

Result<void> DataSourceRepository::save(const DataSource& ds) {
    // Credentials go to SecureStorage; the column keeps the non-secret fields.
    const QString column_config = ds_config_for_column(ds.id, ds.config);
    return exec_write("INSERT INTO data_sources "
                      "(id, alias, display_name, description, type, provider, category, config, enabled, tags, "
                      "created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                      "COALESCE((SELECT created_at FROM data_sources WHERE id = ?), datetime('now')), datetime('now')) "
                      "ON CONFLICT(id) DO UPDATE SET "
                      "alias = excluded.alias, "
                      "display_name = excluded.display_name, "
                      "description = excluded.description, "
                      "type = excluded.type, "
                      "provider = excluded.provider, "
                      "category = excluded.category, "
                      "config = excluded.config, "
                      "enabled = excluded.enabled, "
                      "tags = excluded.tags, "
                      "updated_at = datetime('now')",
                      {ds.id, ds.alias, ds.display_name, ds.description, ds.type, ds.provider, ds.category, column_config,
                       ds.enabled ? 1 : 0, ds.tags, ds.id});
}

Result<void> DataSourceRepository::remove(const QString& id) {
    SecureStorage::instance().remove(ds_secret_handle(id)); // drop the encrypted credentials too
    return exec_write("DELETE FROM data_sources WHERE id = ?", {id});
}

Result<void> DataSourceRepository::set_enabled(const QString& id, bool enabled) {
    return exec_write("UPDATE data_sources SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
                      {enabled ? 1 : 0, id});
}

} // namespace fincept
