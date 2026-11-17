#include "storage/repositories/DataSourceRepository.h"

#include "services/connectors/ConnectorCredentialBridge.h"
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
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

/// Connections whose stored credentials could NOT be read when their config was
/// last loaded (SecureStorage error, not "nothing stored"). Their config reached
/// the caller with blank secrets; a save of that config must not be taken as
/// "the user cleared the secrets" and delete the stored copy it never read.
struct DsUnreadable {
    QMutex mutex;
    QHash<QString, QString> ids; ///< id -> plain reason
};
DsUnreadable& ds_unreadable() {
    static DsUnreadable s;
    return s;
}
void ds_mark_unreadable(const QString& id, bool unreadable, const QString& reason = QString()) {
    auto& u = ds_unreadable();
    QMutexLocker lock(&u.mutex);
    if (unreadable)
        u.ids.insert(id, reason);
    else
        u.ids.remove(id);
}
bool ds_is_unreadable(const QString& id, QString* reason = nullptr) {
    auto& u = ds_unreadable();
    QMutexLocker lock(&u.mutex);
    const auto it = u.ids.constFind(id);
    if (it == u.ids.constEnd())
        return false;
    if (reason)
        *reason = it.value();
    return true;
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
/// SecureStorage and blanked. `*column` = `config` unchanged when it has no
/// secrets, is not a JSON object, or the encrypted store failed to write.
/// Returns false (with `*error`) when the stored credentials could not be read
/// when this connection was loaded and still cannot be: writing now would replace
/// secrets that were never read, so nothing is changed.
bool ds_config_for_column(const QString& id, const QString& config, QString* column, QString* error) {
    *column = config;
    const QJsonDocument doc = QJsonDocument::fromJson(config.toUtf8());
    if (!doc.isObject() || id.isEmpty())
        return true;

    QJsonObject obj = doc.object();
    QJsonObject secrets;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (ds_is_secret_entry(it.key(), it.value()))
            secrets.insert(it.key(), it.value());
    }

    if (secrets.isEmpty()) {
        // Nothing sensitive left (secret cleared, or none ever set): drop any stale
        // copy — unless the stored copy could not be read when this config was
        // loaded: then the blanks are "unknown", not "cleared", and it is kept.
        if (ds_is_unreadable(id)) {
            LOG_WARN("DataSourceRepo", "Stored connection credentials kept: they could not be read when the "
                                       "connection was loaded, so blank fields are not a deletion id=" + id);
            return true;
        }
        SecureStorage::instance().remove(ds_secret_handle(id));
        return true;
    }

    if (ds_is_unreadable(id)) {
        // The blanks of this config were "could not read", not "empty". Read again: when the
        // store answers now, keep every stored value the user did not retype; when it still
        // fails, refuse — a write would replace credentials nobody has read.
        const SecretRead sr = SecureStorage::instance().read(ds_secret_handle(id));
        if (sr.failed()) {
            *error = QStringLiteral("Could not read secure storage: %1. The connection's saved credentials were "
                                    "not changed.")
                         .arg(sr.reason());
            return false;
        }
        ds_mark_unreadable(id, false);
        if (sr.found()) {
            const QJsonObject stored = QJsonDocument::fromJson(sr.value.toUtf8()).object();
            for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
                const QJsonValue cur = obj.value(it.key());
                if (!secrets.contains(it.key()) && cur.isString() && cur.toString().isEmpty())
                    secrets.insert(it.key(), it.value());
            }
        }
    }

    // The column is blanked only once the encrypted copy was written AND read back
    // equal; otherwise the plaintext stays in the column (never lost) and the move
    // is retried on the next read or save. One line per connection per session.
    const QString payload = QString::fromUtf8(QJsonDocument(secrets).toJson(QJsonDocument::Compact));
    const auto stored = SecureStorage::instance().store_verified(ds_secret_handle(id), payload);
    LOG_TRANSITION("DataSourceRepo", QStringLiteral("secret_move|") + id, stored.is_err(),
                   stored.is_err() ? LogLevel::Warn : LogLevel::Info,
                   stored.is_err() ? logf("Connection credentials kept in the config column, secure storage write "
                                          "not confirmed",
                                          {{"id", id}, {"error", QString::fromStdString(stored.error())}})
                                   : logf("Connection credentials stored in secure storage again", {{"id", id}}));
    if (stored.is_err())
        return true;
    for (const QString& key : secrets.keys())
        obj.insert(key, QString());
    *column = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return true;
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

    const SecretRead sr = SecureStorage::instance().read(ds_secret_handle(id));
    // Error ≠ "nothing stored": remember it so a later save does not delete or
    // replace the copy (the read failure itself is logged by SecureStorage).
    ds_mark_unreadable(id, sr.failed(), sr.failed() ? sr.reason() : QString());
    if (!sr.found() || sr.value.isEmpty())
        return column_config;
    const QJsonDocument sdoc = QJsonDocument::fromJson(sr.value.toUtf8());
    if (!sdoc.isObject()) {
        // Read on every list; one line per connection per hour, never the value.
        LOG_WARN_RL("DataSourceRepo", QStringLiteral("secret_not_json|") + id, 3600 * 1000,
                    logf("Stored data source secret is not valid JSON", {{"id", id}, {"bytes", sr.value.size()}}));
        return column_config;
    }

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
    // An automatic move never writes over a stored copy that cannot be read (it
    // may hold values the column does not): the column keeps the plaintext and
    // the move is retried on a later read.
    const SecretRead current = SecureStorage::instance().read(ds_secret_handle(ds.id));
    LOG_TRANSITION("DataSourceRepo", QStringLiteral("secret_move_read|") + ds.id, current.failed(),
                   current.failed() ? LogLevel::Warn : LogLevel::Info,
                   current.failed() ? logf("Connection credentials not moved, secure storage could not be read",
                                           {{"id", ds.id}, {"kind", current.error_name()}})
                                    : logf("Connection credentials readable again", {{"id", ds.id}}));
    if (current.failed())
        return;
    QString column;
    QString error;
    if (!ds_config_for_column(ds.id, ds.config, &column, &error) || column == ds.config)
        return; // write not confirmed (logged once) — or nothing to move
    auto r = Database::instance().execute("UPDATE data_sources SET config = ? WHERE id = ?", {column, ds.id});
    if (r.is_err())
        LOG_WARN_RL("DataSourceRepo", QStringLiteral("scrub|") + ds.id, 3600 * 1000,
                    logf("Connection credentials moved but the config column could not be blanked",
                         {{"id", ds.id}, {"error", QString::fromStdString(r.error())}}));
    else
        LOG_INFO("DataSourceRepo", logf("Connection credentials moved into secure storage", {{"id", ds.id}}));
}
} // namespace

DataSourceRepository& DataSourceRepository::instance() {
    static DataSourceRepository s;
    return s;
}

DataSource DataSourceRepository::map_row(QSqlQuery& q) {
    const QString id = q.value(0).toString();
    DataSource ds{id,
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
                  q.value(11).toString(),
                  q.value(12).toString(),
                  q.value(13).toString(),
                  q.value(14).toString(),
                  q.value(15).toString(),
                  q.value(16).isNull() ? -1 : q.value(16).toLongLong()};
    QString reason;
    ds.secrets_unreadable = ds_is_unreadable(id, &reason);
    ds.secrets_error = reason;
    return ds;
}

static const char* kDsCols = "id, alias, display_name, description, type, provider, category, config, "
                             "enabled, tags, created_at, updated_at, last_status, last_code, last_message, "
                             "last_tested_at, last_latency_ms";

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
    QString column_config;
    QString secret_error;
    if (!ds_config_for_column(ds.id, ds.config, &column_config, &secret_error))
        return Result<void>::err(secret_error.toStdString());
    auto result =
        exec_write("INSERT INTO data_sources "
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
    // A market-data key typed into a connection (Finnhub, Polygon, Alpha Vantage, ...) used to
    // stay in this store only, so the Python connectors that read FINNHUB_API_KEY etc. never
    // saw it. Copy it to the env-named SecureStorage slot PythonRunner injects — only when that
    // slot is still empty. Covers every save path: the dialog, import and the MCP ds_* tools.
    if (result.is_ok())
        services::connectors::bridge_connection_credentials(ds.provider, ds.config);
    return result;
}

Result<void> DataSourceRepository::remove(const QString& id) {
    SecureStorage::instance().remove(ds_secret_handle(id)); // drop the encrypted credentials too
    return exec_write("DELETE FROM data_sources WHERE id = ?", {id});
}

Result<void> DataSourceRepository::update_health(const QString& id, const QString& status, const QString& code,
                                                 const QString& message, qint64 latency_ms) {
    return exec_write("UPDATE data_sources SET last_status = ?, last_code = ?, last_message = ?, "
                      "last_tested_at = datetime('now'), last_latency_ms = ? WHERE id = ?",
                      {status, code, message.left(500), latency_ms, id});
}

Result<void> DataSourceRepository::set_enabled(const QString& id, bool enabled) {
    return exec_write("UPDATE data_sources SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
                      {enabled ? 1 : 0, id});
}

} // namespace fincept
