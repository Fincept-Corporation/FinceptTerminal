#include "storage/cache/CacheManager.h"

#include "storage/sqlite/CacheDatabase.h"

#include <QSqlQuery>

namespace fincept {

CacheManager& CacheManager::instance() {
    static CacheManager s;
    return s;
}

CacheManager::CacheManager(QObject* parent) : QObject(parent) {}

void CacheManager::put(const QString& key, const QVariant& value, int ttl_seconds, const QString& category) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return;

    const QString data = value.toString();
    cdb.execute("INSERT OR REPLACE INTO unified_cache "
                "(key, value, category, ttl_seconds, expires_at, hit_count, size_bytes) "
                "VALUES (?, ?, ?, ?, datetime('now', '+' || ? || ' seconds'), 0, ?)",
                {key, data, category, ttl_seconds, ttl_seconds, data.size()});
}

QVariant CacheManager::get(const QString& key) const {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return {};

    auto r = cdb.execute("SELECT value FROM unified_cache WHERE key = ? AND expires_at > datetime('now')", {key});
    if (r.is_err())
        return {};

    QSqlQuery q = r.value();
    if (!q.next())
        return {};

    // Increment hit counter (fire-and-forget)
    cdb.execute("UPDATE unified_cache SET hit_count = hit_count + 1 WHERE key = ?", {key});

    return q.value(0).toString();
}

bool CacheManager::has(const QString& key) const {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return false;

    auto r = cdb.execute("SELECT 1 FROM unified_cache WHERE key = ? AND expires_at > datetime('now')", {key});
    if (r.is_err())
        return false;

    QSqlQuery q = r.value();
    return q.next();
}

void CacheManager::remove(const QString& key) {
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.execute("DELETE FROM unified_cache WHERE key = ?", {key});
}

void CacheManager::remove_prefix(const QString& prefix) {
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.execute("DELETE FROM unified_cache WHERE key LIKE ?", {prefix + "%"});
}

void CacheManager::clear() {
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.exec("DELETE FROM unified_cache");
}

void CacheManager::clear_category(const QString& category) {
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.execute("DELETE FROM unified_cache WHERE category = ?", {category});
}

int CacheManager::entry_count() const {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return 0;

    auto r = cdb.execute("SELECT COUNT(*) FROM unified_cache WHERE expires_at > datetime('now')", {});
    if (r.is_err())
        return 0;

    QSqlQuery q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

} // namespace fincept
