#include "storage/cache/CacheManager.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/CacheDatabase.h"

#include <QJsonDocument>

namespace fincept {

CacheManager& CacheManager::instance() {
    static CacheManager s;
    return s;
}

CacheManager::CacheManager(QObject* parent) : QObject(parent) {
    // Periodic eviction every 60 seconds
    evict_timer_ = new QTimer(this);
    connect(evict_timer_, &QTimer::timeout, this, &CacheManager::evict_expired);
    evict_timer_->start(60000);
}

void CacheManager::put(const QString& key, const QVariant& value, int ttl_seconds, const QString& category) {
    QMutexLocker lock(&mutex_);
    cache_[key] = {value, QDateTime::currentDateTime().addSecs(ttl_seconds), category};
    persist(key, value, ttl_seconds, category);
}

QVariant CacheManager::get(const QString& key) const {
    QMutexLocker lock(&mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end())
        return {};
    if (QDateTime::currentDateTime() > it->expires_at) {
        return {};
    }
    return it->value;
}

bool CacheManager::has(const QString& key) const {
    QMutexLocker lock(&mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end())
        return false;
    return QDateTime::currentDateTime() <= it->expires_at;
}

void CacheManager::remove(const QString& key) {
    QMutexLocker lock(&mutex_);
    cache_.remove(key);
    remove_persisted(key);
}

void CacheManager::remove_prefix(const QString& prefix) {
    QMutexLocker lock(&mutex_);
    QStringList to_remove;
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it.key().startsWith(prefix))
            to_remove.append(it.key());
    }
    for (const auto& k : to_remove)
        cache_.remove(k);
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open()) {
        cdb.execute("DELETE FROM unified_cache WHERE key LIKE ?",
                    {prefix + "%"});
    }
}

void CacheManager::clear() {
    QMutexLocker lock(&mutex_);
    cache_.clear();
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open()) {
        cdb.exec("DELETE FROM unified_cache");
    }
}

void CacheManager::clear_category(const QString& category) {
    QMutexLocker lock(&mutex_);
    // Remove from in-memory cache
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->category == category)
            it = cache_.erase(it);
        else
            ++it;
    }
    // Remove from DB
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open()) {
        cdb.execute("DELETE FROM unified_cache WHERE category = ?", {category});
    }
}

int CacheManager::entry_count() const {
    QMutexLocker lock(&mutex_);
    return cache_.size();
}

void CacheManager::evict_expired() {
    QMutexLocker lock(&mutex_);
    auto now = QDateTime::currentDateTime();
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (now > it->expires_at)
            it = cache_.erase(it);
        else
            ++it;
    }
    // Also clean DB
    auto& cdb = CacheDatabase::instance();
    if (cdb.is_open()) {
        cdb.exec("DELETE FROM unified_cache WHERE expires_at <= datetime('now')");
    }
}

void CacheManager::persist(const QString& key, const QVariant& value, int ttl, const QString& category) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return;

    QString data = value.toString();
    if (data.isEmpty() && value.canConvert<QJsonDocument>()) {
        data = QString::fromUtf8(value.toJsonDocument().toJson(QJsonDocument::Compact));
    }
    int size = data.size();

    cdb.execute("INSERT OR REPLACE INTO unified_cache "
                "(key, value, category, ttl_seconds, expires_at, hit_count, size_bytes) "
                "VALUES (?, ?, ?, ?, datetime('now', '+' || ? || ' seconds'), 0, ?)",
                {key, data, category, ttl, ttl, size});
}

void CacheManager::remove_persisted(const QString& key) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return;
    cdb.execute("DELETE FROM unified_cache WHERE key = ?", {key});
}

} // namespace fincept
