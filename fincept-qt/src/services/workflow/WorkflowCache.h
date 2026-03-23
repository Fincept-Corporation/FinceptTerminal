#pragma once

#include <QJsonValue>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

namespace fincept::workflow {

/// LRU cache entry with TTL.
struct CacheEntry {
    QJsonValue data;
    qint64 timestamp_ms = 0;
    int ttl_ms = 300000; // 5 minutes default
};

/// In-memory LRU cache for workflow execution results and node outputs.
class WorkflowCache : public QObject {
    Q_OBJECT
  public:
    static WorkflowCache& instance();

    /// Get a cached value. Returns null QJsonValue if miss.
    QJsonValue get(const QString& key) const;

    /// Store a value with optional TTL.
    void put(const QString& key, const QJsonValue& value, int ttl_ms = 300000);

    /// Check if key exists and is not expired.
    bool has(const QString& key) const;

    /// Remove a specific key.
    void remove(const QString& key);

    /// Remove all entries matching a prefix (e.g., "workflow:abc:" invalidates all for that workflow).
    void invalidate_prefix(const QString& prefix);

    /// Clear all cache.
    void clear();

    /// Cache statistics.
    int size() const;
    int hits() const { return hits_; }
    int misses() const { return misses_; }
    double hit_rate() const;

    /// Enable/disable caching.
    bool enabled() const { return enabled_; }
    void set_enabled(bool on) { enabled_ = on; }

  private:
    WorkflowCache();
    void cleanup_expired();

    mutable QMutex mutex_;
    QMap<QString, CacheEntry> entries_;
    int max_entries_ = 500;
    bool enabled_ = true;

    mutable int hits_ = 0;
    mutable int misses_ = 0;

    QTimer* cleanup_timer_ = nullptr;
};

} // namespace fincept::workflow
