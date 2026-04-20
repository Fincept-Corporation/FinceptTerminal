#include "storage/cache/CacheManager.h"

#include "storage/sqlite/CacheDatabase.h"

#include <QSqlQuery>
#include <utility>

namespace fincept {

namespace {
// Escape LIKE wildcards (% and _) in user-supplied prefixes. Paired with "ESCAPE '\\'" in SQL.
QString escape_like(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (QChar ch : s) {
        if (ch == '\\' || ch == '%' || ch == '_')
            out.append('\\');
        out.append(ch);
    }
    return out;
}
} // namespace

CacheManager& CacheManager::instance() {
    static CacheManager s;
    return s;
}

CacheManager::CacheManager(QObject* parent) : QObject(parent) {}

void CacheManager::put(const QString& key, const QVariant& value, int ttl_seconds, const QString& category) {
    if (key.isEmpty())
        return;
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return;

    const QString data = value.toString();
    const int size_bytes = data.toUtf8().size();
    // ON CONFLICT DO UPDATE preserves created_at and hit_count across re-puts of the same key.
    // (INSERT OR REPLACE would delete+insert, resetting those fields.)
    cdb.execute("INSERT INTO unified_cache "
                "(key, value, category, ttl_seconds, expires_at, size_bytes) "
                "VALUES (?, ?, ?, ?, datetime('now', '+' || ? || ' seconds'), ?) "
                "ON CONFLICT(key) DO UPDATE SET "
                "  value=excluded.value, "
                "  category=excluded.category, "
                "  ttl_seconds=excluded.ttl_seconds, "
                "  expires_at=excluded.expires_at, "
                "  size_bytes=excluded.size_bytes",
                {key, data, category, ttl_seconds, ttl_seconds, size_bytes});
}

QVariant CacheManager::get(const QString& key) const {
    if (key.isEmpty())
        return {};
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return {};

    auto r = cdb.execute("SELECT value FROM unified_cache WHERE key = ? AND expires_at > datetime('now')", {key});
    if (r.is_err())
        return {};

    QSqlQuery q = std::move(r.value());
    if (!q.next())
        return {};

    return q.value(0).toString();
}

std::optional<QString> CacheManager::try_get(const QString& key) const {
    const QVariant v = get(key);
    if (v.isNull())
        return std::nullopt;
    return v.toString();
}

bool CacheManager::has(const QString& key) const {
    if (key.isEmpty())
        return false;
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return false;

    auto r = cdb.execute("SELECT 1 FROM unified_cache WHERE key = ? AND expires_at > datetime('now')", {key});
    if (r.is_err())
        return false;

    QSqlQuery q = std::move(r.value());
    return q.next();
}

void CacheManager::remove(const QString& key) {
    if (key.isEmpty())
        return;
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.execute("DELETE FROM unified_cache WHERE key = ?", {key});
}

void CacheManager::remove_prefix(const QString& prefix) {
    // Empty prefix would match everything — refuse to accidentally DELETE FROM unified_cache.
    if (prefix.isEmpty())
        return;
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.execute("DELETE FROM unified_cache WHERE key LIKE ? ESCAPE '\\'", {escape_like(prefix) + "%"});
}

void CacheManager::clear() {
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open())
        cdb.exec("DELETE FROM unified_cache");
}

void CacheManager::clear_category(const QString& category) {
    if (category.isEmpty())
        return;
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

    QSqlQuery q = std::move(r.value());
    return q.next() ? q.value(0).toInt() : 0;
}

} // namespace fincept
