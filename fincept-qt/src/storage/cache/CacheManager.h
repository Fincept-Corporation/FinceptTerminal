#pragma once
#include <QObject>
#include <QString>
#include <QVariant>

namespace fincept {

/// SQLite-backed cache. All reads and writes go directly to CacheDatabase (cache.db).
/// No in-memory layer — avoids double-storage and keeps a single source of truth.
class CacheManager : public QObject {
    Q_OBJECT
  public:
    static CacheManager& instance();

    void put(const QString& key, const QVariant& value, int ttl_seconds = 300, const QString& category = "general");
    QVariant get(const QString& key) const;
    bool has(const QString& key) const;
    void remove(const QString& key);
    void remove_prefix(const QString& prefix);
    void clear();
    void clear_category(const QString& category);

    int entry_count() const;

  private:
    explicit CacheManager(QObject* parent = nullptr);
};

} // namespace fincept
