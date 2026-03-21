#include "storage/repositories/ContextRecordingRepository.h"

namespace fincept {

ContextRecordingRepository& ContextRecordingRepository::instance() {
    static ContextRecordingRepository s;
    return s;
}

RecordedContext ContextRecordingRepository::map_row(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
            q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
            q.value(6).toInt(), q.value(7).toString(), q.value(8).toString()};
}

static const char* kCols =
    "id, tab_name, data_type, label, raw_data, metadata, data_size, tags, created_at";

Result<QVector<RecordedContext>> ContextRecordingRepository::list_all(int limit) {
    return query_list(
        QString("SELECT %1 FROM recorded_contexts ORDER BY created_at DESC LIMIT %2")
            .arg(kCols).arg(limit),
        {}, map_row);
}

Result<QVector<RecordedContext>> ContextRecordingRepository::list_by_tab(const QString& tab_name,
                                                                          int limit) {
    return query_list(
        QString("SELECT %1 FROM recorded_contexts WHERE tab_name = ? ORDER BY created_at DESC LIMIT %2")
            .arg(kCols).arg(limit),
        {tab_name}, map_row);
}

Result<RecordedContext> ContextRecordingRepository::get(const QString& id) {
    return query_one(QString("SELECT %1 FROM recorded_contexts WHERE id = ?").arg(kCols),
                     {id}, map_row);
}

Result<void> ContextRecordingRepository::save(const RecordedContext& ctx) {
    return exec_write(
        "INSERT OR REPLACE INTO recorded_contexts "
        "(id, tab_name, data_type, label, raw_data, metadata, data_size, tags) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {ctx.id, ctx.tab_name, ctx.data_type, ctx.label,
         ctx.raw_data, ctx.metadata, ctx.data_size, ctx.tags});
}

Result<void> ContextRecordingRepository::remove(const QString& id) {
    return exec_write("DELETE FROM recorded_contexts WHERE id = ?", {id});
}

Result<void> ContextRecordingRepository::clear_all() {
    return exec_write("DELETE FROM recorded_contexts", {});
}

} // namespace fincept
