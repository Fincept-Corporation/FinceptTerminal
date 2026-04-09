#include "storage/repositories/DataMappingRepository.h"

namespace fincept {

DataMappingRepository& DataMappingRepository::instance() {
    static DataMappingRepository s;
    return s;
}

DataMapping DataMappingRepository::map_row(QSqlQuery& q) {
    return {
        q.value(0).toString(),  // id
        q.value(1).toString(),  // name
        q.value(2).toString(),  // source_id
        q.value(3).toString(),  // schema_name
        q.value(4).toString(),  // base_url
        q.value(5).toString(),  // endpoint
        q.value(6).toString(),  // method
        q.value(7).toString(),  // auth_type
        q.value(8).toString(),  // auth_token
        q.value(9).toString(),  // headers
        q.value(10).toString(), // body
        q.value(11).toString(), // parser
        q.value(12).toBool(),   // cache_enabled
        q.value(13).toInt(),    // cache_ttl
        q.value(14).toString(), // field_mappings_json
        q.value(15).toString(), // created_at
        q.value(16).toString(), // updated_at
    };
}

static const char* kDmCols =
    "id, name, source_id, schema_name, base_url, endpoint, method, auth_type, auth_token, "
    "headers, body, parser, cache_enabled, cache_ttl, field_mappings_json, created_at, updated_at";

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
    return exec_write("INSERT INTO data_mappings "
                      "(id, name, source_id, schema_name, base_url, endpoint, method, auth_type, auth_token, "
                      " headers, body, parser, cache_enabled, cache_ttl, field_mappings_json, "
                      " created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
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
                      "  updated_at         = datetime('now')",
                      {dm.id, dm.name, dm.source_id, dm.schema_name, dm.base_url, dm.endpoint, dm.method, dm.auth_type,
                       dm.auth_token, dm.headers, dm.body, dm.parser, dm.cache_enabled ? 1 : 0, dm.cache_ttl,
                       dm.field_mappings_json, dm.id});
}

Result<void> DataMappingRepository::remove(const QString& id) {
    return exec_write("DELETE FROM data_mappings WHERE id = ?", {id});
}

} // namespace fincept
