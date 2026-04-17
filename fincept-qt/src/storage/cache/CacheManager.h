#pragma once
#include <QObject>
#include <QString>
#include <QVariant>

#include <optional>

namespace fincept {

/// SQLite-backed cache. All reads and writes go directly to CacheDatabase (cache.db).
/// No in-memory layer — avoids double-storage and keeps a single source of truth.
class CacheManager : public QObject {
    Q_OBJECT
  public:
    static CacheManager& instance();

    void put(const QString& key, const QVariant& value, int ttl_seconds = 300, const QString& category = "general");
    /// Returns the cached value (as QString-convertible QVariant) or a null QVariant on miss/expiry.
    QVariant get(const QString& key) const;
    /// Single-query variant of get(): std::nullopt on miss, value on hit. Prefer this over has()+get()
    /// — those two-round-trips duplicate work since get() already checks expiry.
    std::optional<QString> try_get(const QString& key) const;
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
