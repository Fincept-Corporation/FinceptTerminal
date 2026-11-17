#include "storage/repositories/ReportRepository.h"

#include "storage/sync/SyncOutbox.h"

namespace fincept {

ReportRepository& ReportRepository::instance() {
    static ReportRepository s;
    return s;
}

Report ReportRepository::map_report(QSqlQuery& q) {
    return {q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
            q.value(4).toString()};
}

ReportTemplate ReportRepository::map_template(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
            q.value(3).toString(), q.value(4).toString(), q.value(5).toString()};
}

Result<qint64> ReportRepository::create(const QString& title, const QString& content_json) {
    auto r = exec_insert("INSERT INTO reports (title, content_json) VALUES (?, ?)", {title, content_json});
    if (r.is_ok())
        SyncOutbox::record("report", QString::number(r.value()), "create");
    return r;
}

Result<Report> ReportRepository::get(int id) {
    return query_one("SELECT id, title, content_json, created_at, updated_at FROM reports WHERE id = ?", {id},
                     map_report);
}

Result<QVector<Report>> ReportRepository::list_all() {
    return query_list("SELECT id, title, content_json, created_at, updated_at FROM reports ORDER BY updated_at DESC",
                      {}, map_report);
}

Result<QVector<Report>> ReportRepository::search(const QString& query, int limit) {
    QString sql = QStringLiteral("SELECT id, title, content_json, created_at, updated_at FROM reports");
    QVariantList params;
    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        // LIKE is case-insensitive for ASCII in SQLite; '%' / '_' typed by the user are
        // escaped so they match literally.
        QString pattern = q;
        pattern.replace(QLatin1Char('\\'), QLatin1String("\\\\"))
            .replace(QLatin1Char('%'), QLatin1String("\\%"))
            .replace(QLatin1Char('_'), QLatin1String("\\_"));
        pattern = QLatin1Char('%') + pattern + QLatin1Char('%');
        sql += QStringLiteral(" WHERE title LIKE ? ESCAPE '\\' OR content_json LIKE ? ESCAPE '\\'");
        params << pattern << pattern;
    }
    sql += QStringLiteral(" ORDER BY updated_at DESC, id DESC");
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT ?");
        params << limit;
    }
    return query_list(sql, params, map_report);
}

Result<void> ReportRepository::update(int id, const QString& title, const QString& content_json) {
    auto r = exec_write("UPDATE reports SET title = ?, content_json = ?, updated_at = datetime('now') WHERE id = ?",
                        {title, content_json, id});
    // The Report Builder keeps its library row current while the user edits (a write a
    // few seconds after typing stops). The push reads the live row, so pending updates of
    // one report coalesce into a single outbox entry instead of one cloud call per save.
    if (r.is_ok())
        SyncOutbox::record_unique("report", QString::number(id), "update");
    return r;
}

Result<void> ReportRepository::remove(int id) {
    auto r = exec_write("DELETE FROM reports WHERE id = ?", {id});
    if (r.is_ok())
        SyncOutbox::record("report", QString::number(id), "delete");
    return r;
}

Result<QVector<ReportTemplate>> ReportRepository::list_templates() {
    auto r = db().execute("SELECT id, name, description, template_data, created_at, updated_at "
                          "FROM report_templates ORDER BY name");
    if (r.is_err())
        return Result<QVector<ReportTemplate>>::err(r.error());
    QVector<ReportTemplate> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_template(q));
    return Result<QVector<ReportTemplate>>::ok(std::move(result));
}

Result<ReportTemplate> ReportRepository::get_template(const QString& id) {
    auto r = db().execute("SELECT id, name, description, template_data, created_at, updated_at "
                          "FROM report_templates WHERE id = ?",
                          {id});
    if (r.is_err())
        return Result<ReportTemplate>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<ReportTemplate>::err("Template not found");
    return Result<ReportTemplate>::ok(map_template(q));
}

Result<void> ReportRepository::save_template(const ReportTemplate& t) {
    return exec_write("INSERT OR REPLACE INTO report_templates (id, name, description, template_data, updated_at) "
                      "VALUES (?, ?, ?, ?, datetime('now'))",
                      {t.id, t.name, t.description, t.template_data});
}

Result<void> ReportRepository::remove_template(const QString& id) {
    return exec_write("DELETE FROM report_templates WHERE id = ?", {id});
}

} // namespace fincept
