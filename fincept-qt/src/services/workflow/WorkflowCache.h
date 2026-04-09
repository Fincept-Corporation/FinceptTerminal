#pragma once

#include <QJsonValue>
#include <QObject>
#include <QString>

namespace fincept::workflow {

/// Thin wrapper over CacheManager for workflow execution results and node outputs.
/// Preserves the original public API; all persistence/TTL handled by CacheManager.
class WorkflowCache : public QObject {
    Q_OBJECT
  public:
    static WorkflowCache& instance();

    /// Get a cached value. Returns null QJsonValue if miss.
    QJsonValue get(const QString& key) const;

    /// Store a value with optional TTL (milliseconds, converted to seconds).
    void put(const QString& key, const QJsonValue& value, int ttl_ms = 300000);

    /// Check if key exists and is not expired.
    bool has(const QString& key) const;

    /// Remove a specific key.
    void remove(const QString& key);

    /// Remove all entries matching a prefix.
    void invalidate_prefix(const QString& prefix);

    /// Clear all workflow cache entries.
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
    explicit WorkflowCache(QObject* parent = nullptr);

    static constexpr int kDefaultTtlSec = 300;
    bool enabled_ = true;
    mutable int hits_ = 0;
    mutable int misses_ = 0;
};

} // namespace fincept::workflow
