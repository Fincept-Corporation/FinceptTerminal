#include "storage/repositories/NewsMonitorRepository.h"

namespace fincept {

NewsMonitorRepository& NewsMonitorRepository::instance() {
    static NewsMonitorRepository s;
    return s;
}

NewsMonitorRow NewsMonitorRepository::map_row(QSqlQuery& q) {
    return {
        q.value(0).toString(),
        q.value(1).toString(),
        q.value(2).toString().split(",", Qt::SkipEmptyParts),
        q.value(3).toString(),
        q.value(4).toBool(),
    };
}

Result<QVector<NewsMonitorRow>> NewsMonitorRepository::list_all() {
    return query_list(
        "SELECT id, label, keywords, color, enabled FROM news_monitors ORDER BY label",
        {}, map_row);
}

Result<void> NewsMonitorRepository::upsert(const NewsMonitorRow& m) {
    return exec_write(
        "INSERT OR REPLACE INTO news_monitors (id, label, keywords, color, enabled, updated_at) "
        "VALUES (?, ?, ?, ?, ?, datetime('now'))",
        {m.id, m.label, m.keywords.join(","), m.color, m.enabled ? 1 : 0});
}

Result<void> NewsMonitorRepository::remove(const QString& id) {
    return exec_write("DELETE FROM news_monitors WHERE id = ?", {id});
}

Result<void> NewsMonitorRepository::set_enabled(const QString& id, bool enabled) {
    return exec_write(
        "UPDATE news_monitors SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
        {enabled ? 1 : 0, id});
}

} // namespace fincept
