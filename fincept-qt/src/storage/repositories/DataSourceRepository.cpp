#include "storage/repositories/DataSourceRepository.h"

namespace fincept {

DataSourceRepository& DataSourceRepository::instance() {
    static DataSourceRepository s;
    return s;
}

DataSource DataSourceRepository::map_row(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
            q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
            q.value(6).toString(), q.value(7).toString(), q.value(8).toBool(),
            q.value(9).toString(), q.value(10).toString(), q.value(11).toString()};
}

static const char* kDsCols =
    "id, alias, display_name, description, type, provider, category, config, "
    "enabled, tags, created_at, updated_at";

Result<QVector<DataSource>> DataSourceRepository::list_all() {
    return query_list(QString("SELECT %1 FROM data_sources ORDER BY display_name").arg(kDsCols),
                      {}, map_row);
}

Result<DataSource> DataSourceRepository::get(const QString& id) {
    return query_one(QString("SELECT %1 FROM data_sources WHERE id = ?").arg(kDsCols),
                     {id}, map_row);
}

Result<void> DataSourceRepository::save(const DataSource& ds) {
    return exec_write(
        "INSERT OR REPLACE INTO data_sources "
        "(id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
        {ds.id, ds.alias, ds.display_name, ds.description, ds.type,
         ds.provider, ds.category, ds.config, ds.enabled ? 1 : 0, ds.tags});
}

Result<void> DataSourceRepository::remove(const QString& id) {
    return exec_write("DELETE FROM data_sources WHERE id = ?", {id});
}

Result<void> DataSourceRepository::set_enabled(const QString& id, bool enabled) {
    return exec_write("UPDATE data_sources SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
                      {enabled ? 1 : 0, id});
}

} // namespace fincept
