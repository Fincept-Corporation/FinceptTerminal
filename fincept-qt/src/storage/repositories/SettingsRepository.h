#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct Setting {
    QString key;
    QString value;
    QString category;
    QString updated_at;
};

/// Repository for the settings key-value table.
class SettingsRepository : public BaseRepository<Setting> {
  public:
    static SettingsRepository& instance();

    Result<void> set(const QString& key, const QString& value, const QString& category = "general");

    /// Reads a setting.
    ///
    /// Three distinct outcomes — do not collapse them:
    ///  - `ok(stored value)`  — the key is set.
    ///  - `ok(default_val)`   — the key is genuinely absent (or stored empty).
    ///  - `err(message)`      — the **read failed** (locked DB, wrong-thread
    ///                          connection, schema error). The stored value is
    ///                          unknown; it is NOT the default.
    ///
    /// A caller that reads a setting, populates UI from it, and later writes it
    /// back MUST abort that write on `is_err()`. Treating an error as the
    /// default persists the default over the user's real value — set() is an
    /// `INSERT OR REPLACE`, so the original is gone. Read-only display paths may
    /// fall back to the default, but must not write it back.
    Result<QString> get(const QString& key, const QString& default_val = {});
    Result<void> remove(const QString& key);
    Result<QVector<Setting>> get_by_category(const QString& category);
    Result<void> clear_category(const QString& category);

  private:
    SettingsRepository() = default;
    static Setting map_row(QSqlQuery& q);
};

} // namespace fincept
