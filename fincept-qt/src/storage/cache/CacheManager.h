#pragma once
#include <QHash>
#include <QString>
#include <QVariant>
#include <QDateTime>
#include <QMutex>
#include <QTimer>
#include <QObject>

namespace fincept {

/// In-memory L1 cache with write-through to cache.db.
/// Falls back to pure in-memory if CacheDatabase is not available.
class CacheManager : public QObject {
    Q_OBJECT
public:
    static CacheManager& instance();

    void put(const QString& key, const QVariant& value, int ttl_seconds = 300,
             const QString& category = "general");
    QVariant get(const QString& key) const;
    bool has(const QString& key) const;
    void remove(const QString& key);
    void clear();
    void clear_category(const QString& category);

    int entry_count() const;

private:
    explicit CacheManager(QObject* parent = nullptr);

    void evict_expired();
    void persist(const QString& key, const QVariant& value, int ttl, const QString& category);
    void remove_persisted(const QString& key);

    struct Entry {
        QVariant  value;
        QDateTime expires_at;
        QString   category;
    };

    mutable QMutex mutex_;
    QHash<QString, Entry> cache_;
    QTimer* evict_timer_ = nullptr;
};

} // namespace fincept
