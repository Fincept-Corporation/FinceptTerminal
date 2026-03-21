#include "storage/repositories/SettingsRepository.h"

namespace fincept {

SettingsRepository& SettingsRepository::instance() {
    static SettingsRepository s;
    return s;
}

Setting SettingsRepository::map_row(QSqlQuery& q) {
    return {
        q.value(0).toString(),
        q.value(1).toString(),
        q.value(2).toString(),
        q.value(3).toString(),
    };
}

Result<void> SettingsRepository::set(const QString& key, const QString& value,
                                      const QString& category) {
    return exec_write(
        "INSERT OR REPLACE INTO settings (key, value, category, updated_at) "
        "VALUES (?, ?, ?, datetime('now'))",
        {key, value, category});
}

Result<QString> SettingsRepository::get(const QString& key, const QString& default_val) {
    auto r = db().execute("SELECT value FROM settings WHERE key = ?", {key});
    if (r.is_err()) return Result<QString>::ok(default_val);
    auto& q = r.value();
    if (!q.next()) return Result<QString>::ok(default_val);
    QString val = q.value(0).toString();
    return Result<QString>::ok(val.isEmpty() ? default_val : val);
}

Result<void> SettingsRepository::remove(const QString& key) {
    return exec_write("DELETE FROM settings WHERE key = ?", {key});
}

Result<QVector<Setting>> SettingsRepository::get_by_category(const QString& category) {
    return query_list(
        "SELECT key, value, category, updated_at FROM settings WHERE category = ? ORDER BY key",
        {category}, map_row);
}

Result<void> SettingsRepository::clear_category(const QString& category) {
    return exec_write("DELETE FROM settings WHERE category = ?", {category});
}

} // namespace fincept
