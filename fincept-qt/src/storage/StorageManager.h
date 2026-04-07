#pragma once
#include "core/result/Result.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept {

/// Per-category storage statistics.
struct StorageCategoryInfo {
    QString id;          // e.g. "chat_history"
    QString label;       // e.g. "Chat History"
    QString group;       // e.g. "AI & LLM", "Trading", "Data"
    int     count = 0;   // number of rows / entries
    bool    clearable = true;
};

/// Centralized manager for querying and clearing all persistent data stores.
/// Wraps individual repositories and direct SQL for tables without bulk-delete methods.
class StorageManager : public QObject {
    Q_OBJECT
  public:
    static StorageManager& instance();

    /// Returns stats for every data category.
    QVector<StorageCategoryInfo> all_stats() const;

    /// Returns entry count for a single category.
    int count_for(const QString& category_id) const;

    /// Clears all data in a single category. Returns error string on failure.
    Result<void> clear_category(const QString& category_id);

    /// Returns the on-disk size in bytes of fincept.db.
    qint64 main_db_size() const;

    /// Returns the on-disk size in bytes of cache.db.
    qint64 cache_db_size() const;

    /// Returns the on-disk size in bytes of log files.
    qint64 log_files_size() const;

    /// Clears log file contents (truncates).
    Result<void> clear_log_files();

    /// Returns total workspace file size.
    qint64 workspace_files_size() const;

    /// Deletes all workspace files.
    Result<void> clear_workspace_files();

    /// Clears all QSettings (window geometry, perspectives, UI state).
    Result<void> clear_qsettings();

  signals:
    void category_cleared(const QString& category_id);

  private:
    explicit StorageManager(QObject* parent = nullptr);

    int count_table(const QString& table, const QString& where = {}) const;
    Result<void> delete_from(const QString& table, const QString& where = {});
};

} // namespace fincept
