#include "storage/repositories/DataMappingRepository.h"

#include "storage/secure/SecretMigration.h"
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>

namespace fincept {

namespace {

// The AUTH VALUE of an inline-HTTP mapping is a credential: it lives in SecureStorage, the column
// keeps "". A row from before this change still holding plaintext is moved on first read (the
// value is never dropped: if the encrypted store fails, the column keeps it).
QString dm_secret_handle(const QString& id) {
    return QStringLiteral("datamapping:auth:") + id;
}

/// Mappings whose stored auth value could NOT be read when last loaded (SecureStorage error,
/// not "nothing stored"). Their DataMapping reached the caller with an empty auth_token; a save
/// of it must not be taken as "the user cleared the token" and delete the copy nobody read.
struct DmUnreadable {
    QMutex mutex;
    QHash<QString, QString> ids; ///< id -> plain reason
};
DmUnreadable& dm_unreadable() {
    static DmUnreadable s;
    return s;
}
void dm_mark_unreadable(const QString& id, bool unreadable, const QString& reason = QString()) {
    auto& u = dm_unreadable();
    QMutexLocker lock(&u.mutex);
    if (unreadable)
        u.ids.insert(id, reason);
    else
        u.ids.remove(id);
}
bool dm_is_unreadable(const QString& id) {
    auto& u = dm_unreadable();
    QMutexLocker lock(&u.mutex);
    return u.ids.contains(id);
}

/// One line when a mapping's auth value starts staying in its row, one when it moves
/// again — mappings are read often. Names and failure kinds only, never the value.
void dm_log_move(const QString& id, secret_migration::Failure f, const QString& detail) {
    const bool failing = f != secret_migration::Failure::None;
    LOG_TRANSITION("DataMappingRepo", QStringLiteral("secret_move|") + id, failing,
                   failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("Mapping auth value kept in its row, the move into secure storage is retried",
                                  {{"id", id}, {"reason", secret_migration::failure_name(f)}, {"error", detail.left(200)}})
                           : logf("Mapping auth value stored in secure storage again", {{"id", id}}));
}

QString dm_token_for_column(const QString& id, const QString& token) {
    if (id.isEmpty())
        return token;
    if (token.isEmpty()) {
        // Blank because it could not be read → unknown, not cleared: keep the stored copy.
        if (dm_is_unreadable(id)) {
            LOG_WARN("DataMappingRepo", "Stored mapping auth value kept: it could not be read when the mapping was "
                                        "loaded, so a blank value is not a deletion id=" + id);
            return QString();
        }
        SecureStorage::instance().remove(dm_secret_handle(id));
        return QString();
    }
    // A value typed by the user replaces the stored one (also one that could not be read:
    // the user saw "could not read" and entered a new value on purpose). The row keeps the
    // value unless the encrypted copy was written AND read back equal.
    const auto stored = SecureStorage::instance().store_verified(dm_secret_handle(id), token);
    dm_log_move(id, stored.is_err() ? secret_migration::Failure::WriteFailed : secret_migration::Failure::None,
                QString::fromStdString(stored.is_err() ? stored.error() : std::string()));
    if (stored.is_err())
        return token;
    dm_mark_unreadable(id, false);
    return QString();
}

void dm_merge_secret(DataMapping& dm) {
    if (!dm.auth_token.isEmpty()) {
        // Plaintext in the row: a legacy row, or a save made while the store failed (then
        // the newer copy — it wins). The value is used as it is; the move follows
        // write -> read back -> compare -> blank, and never writes over a stored copy
        // that cannot be read. A failed move keeps the row and is retried on a later read.
        if (dm.id.isEmpty())
            return;
        namespace sm = secret_migration;
        auto& secure = SecureStorage::instance();
        const QString handle = dm_secret_handle(dm.id);
        const QString id = dm.id;
        sm::Io io;
        io.read_stored = [&secure, handle]() { return secure.read(handle); };
        io.write_stored = [&secure, handle](const QString& v) { return secure.store(handle, v); };
        io.delete_old = [id]() {
            auto r = Database::instance().execute("UPDATE data_mappings SET auth_token = '' WHERE id = ?", {id});
            return r.is_ok() ? Result<void>::ok() : Result<void>::err(r.error());
        };
        const sm::Outcome o = sm::run(sm::OldCopy::present(dm.auth_token), sm::OnConflict::OldWins, io);
        if (o.stored_verified)
            dm_mark_unreadable(id, false);
        dm_log_move(id, o.failure, o.detail);
        if (o.old_deleted)
            LOG_INFO("DataMappingRepo", logf("Mapping auth value moved into secure storage", {{"id", id}}));
        return;
    }
    const SecretRead sr = SecureStorage::instance().read(dm_secret_handle(dm.id));
    dm_mark_unreadable(dm.id, sr.failed(), sr.failed() ? sr.reason() : QString());
    if (sr.found()) {
        dm.auth_token = sr.value;
    } else if (sr.failed()) {
        dm.auth_unreadable = true;
        dm.auth_read_error = sr.reason();
    }
}

} // namespace

DataMappingRepository& DataMappingRepository::instance() {
    static DataMappingRepository s;
    return s;
}

DataMapping DataMappingRepository::map_row(QSqlQuery& q) {
    DataMapping dm;
    dm.id = q.value(0).toString();
    dm.name = q.value(1).toString();
    dm.source_id = q.value(2).toString();
    dm.schema_name = q.value(3).toString();
    dm.base_url = q.value(4).toString();
    dm.endpoint = q.value(5).toString();
    dm.method = q.value(6).toString();
    dm.auth_type = q.value(7).toString();
    dm.auth_token = q.value(8).toString();
    dm.headers = q.value(9).toString();
    dm.body = q.value(10).toString();
    dm.parser = q.value(11).toString();
    dm.cache_enabled = q.value(12).toBool();
    dm.cache_ttl = q.value(13).toInt();
    dm.field_mappings_json = q.value(14).toString();
    dm.created_at = q.value(15).toString();
    dm.updated_at = q.value(16).toString();
    dm.spec_json = q.value(17).toString();
    dm.spec_version = q.value(18).toInt();
    dm.revision = q.value(19).toInt();
    dm.last_run_at = q.value(20).toString();
    dm.last_run_status = q.value(21).toString();
    dm.last_run_rows = q.value(22).toInt();
    dm.last_run_errors = q.value(23).toInt();
    dm_merge_secret(dm);
    return dm;
}

static const char* kDmCols =
    "id, name, source_id, schema_name, base_url, endpoint, method, auth_type, auth_token, "
    "headers, body, parser, cache_enabled, cache_ttl, field_mappings_json, created_at, updated_at, "
    "spec_json, spec_version, revision, last_run_at, last_run_status, last_run_rows, last_run_errors";

Result<QVector<DataMapping>> DataMappingRepository::list_all() {
    return query_list(QString("SELECT %1 FROM data_mappings ORDER BY name").arg(kDmCols), {}, map_row);
}

Result<QVector<DataMapping>> DataMappingRepository::list_by_source(const QString& source_id) {
    return query_list(QString("SELECT %1 FROM data_mappings WHERE source_id = ? ORDER BY name").arg(kDmCols),
                      {source_id}, map_row);
}

Result<QVector<DataMapping>> DataMappingRepository::list_by_schema(const QString& schema_name) {
    return query_list(QString("SELECT %1 FROM data_mappings WHERE schema_name = ? ORDER BY name").arg(kDmCols),
                      {schema_name}, map_row);
}

Result<DataMapping> DataMappingRepository::get(const QString& id) {
    return query_one(QString("SELECT %1 FROM data_mappings WHERE id = ?").arg(kDmCols), {id}, map_row);
}

Result<void> DataMappingRepository::save(const DataMapping& dm) {
    const QString token_column = dm_token_for_column(dm.id, dm.auth_token);
    return exec_write("INSERT INTO data_mappings "
                      "(id, name, source_id, schema_name, base_url, endpoint, method, auth_type, auth_token, "
                      " headers, body, parser, cache_enabled, cache_ttl, field_mappings_json, "
                      " spec_json, spec_version, revision, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                      " COALESCE((SELECT created_at FROM data_mappings WHERE id = ?), datetime('now')), "
                      " datetime('now')) "
                      "ON CONFLICT(id) DO UPDATE SET "
                      "  name               = excluded.name, "
                      "  source_id          = excluded.source_id, "
                      "  schema_name        = excluded.schema_name, "
                      "  base_url           = excluded.base_url, "
                      "  endpoint           = excluded.endpoint, "
                      "  method             = excluded.method, "
                      "  auth_type          = excluded.auth_type, "
                      "  auth_token         = excluded.auth_token, "
                      "  headers            = excluded.headers, "
                      "  body               = excluded.body, "
                      "  parser             = excluded.parser, "
                      "  cache_enabled      = excluded.cache_enabled, "
                      "  cache_ttl          = excluded.cache_ttl, "
                      "  field_mappings_json = excluded.field_mappings_json, "
                      "  spec_json          = excluded.spec_json, "
                      "  spec_version       = excluded.spec_version, "
                      "  revision           = excluded.revision, "
                      "  updated_at         = datetime('now')",
                      {dm.id, dm.name, dm.source_id.isEmpty() ? QVariant() : QVariant(dm.source_id), dm.schema_name,
                       dm.base_url, dm.endpoint, dm.method.isEmpty() ? QStringLiteral("GET") : dm.method,
                       dm.auth_type.isEmpty() ? QStringLiteral("None") : dm.auth_type, token_column, dm.headers, dm.body,
                       dm.parser.isEmpty() ? QStringLiteral("JSONPath") : dm.parser, dm.cache_enabled ? 1 : 0, dm.cache_ttl,
                       dm.field_mappings_json.isEmpty() ? QStringLiteral("[]") : dm.field_mappings_json, dm.spec_json,
                       dm.spec_version, dm.revision, dm.id});
}

Result<int> DataMappingRepository::save_revision(DataMapping dm) {
    int next = 1;
    {
        auto r = db().execute("SELECT COALESCE(MAX(revision), 0) FROM data_mapping_revisions WHERE mapping_id = ?", {dm.id});
        if (r.is_ok() && r.value().next())
            next = r.value().value(0).toInt() + 1;
    }
    next = std::max(next, dm.revision + (dm.spec_version >= 2 ? 1 : 0));
    dm.revision = next;
    dm.spec_version = 2;
    auto saved = save(dm);
    if (saved.is_err())
        return Result<int>::err(saved.error());
    auto rev = exec_write("INSERT OR REPLACE INTO data_mapping_revisions (mapping_id, revision, spec_json, saved_at) "
                          "VALUES (?, ?, ?, datetime('now'))",
                          {dm.id, next, dm.spec_json});
    if (rev.is_err())
        return Result<int>::err(rev.error());
    return Result<int>::ok(next);
}

Result<QVector<DataMappingRevision>> DataMappingRepository::list_revisions(const QString& mapping_id) {
    return query_list_as<DataMappingRevision>(
        "SELECT revision, spec_json, saved_at FROM data_mapping_revisions WHERE mapping_id = ? ORDER BY revision DESC",
        {mapping_id}, [](QSqlQuery& q) {
            DataMappingRevision r;
            r.revision = q.value(0).toInt();
            r.spec_json = q.value(1).toString();
            r.saved_at = q.value(2).toString();
            return r;
        });
}

Result<void> DataMappingRepository::remove(const QString& id) {
    SecureStorage::instance().remove(dm_secret_handle(id));
    exec_write("DELETE FROM data_mapping_revisions WHERE mapping_id = ?", {id});
    exec_write("DELETE FROM mapped_datasets WHERE mapping_id = ?", {id});
    return exec_write("DELETE FROM data_mappings WHERE id = ?", {id});
}

Result<void> DataMappingRepository::update_run_stats(const QString& id, const QString& status, int rows, int errors) {
    return exec_write("UPDATE data_mappings SET last_run_at = datetime('now'), last_run_status = ?, last_run_rows = ?, "
                      "last_run_errors = ? WHERE id = ?",
                      {status, rows, errors, id});
}

Result<void> DataMappingRepository::save_output(const MappedDataset& out) {
    return exec_write("INSERT OR REPLACE INTO mapped_datasets (mapping_id, revision, schema_name, columns_json, types_json, "
                      "rows_json, row_count, error_cells, source, produced_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
                      {out.mapping_id, out.revision, out.schema_name, out.columns_json, out.types_json, out.rows_json,
                       out.row_count, out.error_cells, out.source});
}

std::optional<MappedDataset> DataMappingRepository::get_output(const QString& mapping_id) {
    auto r = db().execute("SELECT mapping_id, revision, schema_name, columns_json, types_json, rows_json, row_count, "
                          "error_cells, source, produced_at FROM mapped_datasets WHERE mapping_id = ?",
                          {mapping_id});
    if (r.is_err() || !r.value().next())
        return std::nullopt;
    QSqlQuery& q = r.value();
    MappedDataset d;
    d.mapping_id = q.value(0).toString();
    d.revision = q.value(1).toInt();
    d.schema_name = q.value(2).toString();
    d.columns_json = q.value(3).toString();
    d.types_json = q.value(4).toString();
    d.rows_json = q.value(5).toString();
    d.row_count = q.value(6).toInt();
    d.error_cells = q.value(7).toInt();
    d.source = q.value(8).toString();
    d.produced_at = q.value(9).toString();
    return d;
}

} // namespace fincept
